/*
    pqConsole    : interfacing SWI-Prolog and Qt

    Author       : Carlo Capelli
    E-mail       : cc.carlo.cap@gmail.com
    Copyright (C): 2013, Carlo Capelli

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "Swipl_IO.h"
#include "PREDICATE.h"
#include "Completion.h"

#include <QKeyEvent>
#include <QRegExp>
#include <QtDebug>
#include <QTimer>
#include <QEventLoop>
#include <QAction>
#include <QMainWindow>
#include <QMenuBar>
#include <QApplication>
#include <QToolTip>
#include <QFileDialog>

/** some default color, seems sufficiently visible to me
 */
static QColor ANSI2col(int c, bool highlight = false) {
    static QColor
    v[] = { "black",
            "red",
            "green",
            "brown",
            "blue",
            "magenta",
            "cyan",
            "white" },
    h[] = { "gray",
            "magenta",
            "chartreuse",
            "gold",
            "dodgerblue",
            "magenta",
            "lightblue",
            "whitesmoke" };
    Q_ASSERT(c >= 0 && c < 8);
    return (highlight ? h : v)[c];
}

/** TBD configuration
QFont ConsoleEdit::curr_font("courier");
ConsoleEditBase::LineWrapMode ConsoleEdit::wrap_mode = ConsoleEditBase::WidgetWidth;
*/

/** build command line interface to SWI Prolog engine
 *  this start the *primary* console
 */
ConsoleEdit::ConsoleEdit(int argc, char **argv, QWidget *parent)
    : ConsoleEditBase(parent), io(0)
{
    qApp->setWindowIcon(QIcon(":/swipl.png"));

    //qDebug() << "ConsoleEdit" << CVP(this) << CVP(CT);

    //int t_pfunc =
    qRegisterMetaType<pfunc>("pfunc");
    //qDebug() << "t_pfunc" << t_pfunc;

    setup();
    eng = new SwiPrologEngine;

    // wire up console IO
    connect(eng, SIGNAL(user_output(QString)), this, SLOT(user_output(QString)));
    connect(eng, SIGNAL(user_prompt(int)), this, SLOT(user_prompt(int)));
    connect(this, SIGNAL(user_input(QString)), eng, SLOT(user_input(QString)));

    // issue worker thread start
    eng->start(argc, argv);
}

/** this start an *interactor* console
 */
ConsoleEdit::ConsoleEdit(Swipl_IO* io, QString title)
    : ConsoleEditBase(), eng(0), io(io)
{
    io->host = this;

    setup();

    // wire up console IO
    connect(io, SIGNAL(user_output(QString)), this, SLOT(user_output(QString)));
    connect(io, SIGNAL(user_prompt(int)), this, SLOT(user_prompt(int)));
    connect(this, SIGNAL(user_input(QString)), io, SLOT(user_input(QString)));

    auto w = new QMainWindow();
    w->setCentralWidget(this);
    w->setWindowTitle(title);
    w->show();

    QTimer::singleShot(100, this, SLOT(attached()));
}

/** common setup between =main= and =thread= console
 *  different setting required, due to difference in events handling
 */
void ConsoleEdit::setup() {

    qApp->installEventFilter(this);
    thid = -1;
    count_output = 0;
    update_refresh_rate = 100;
    preds = 0;
    helpidx_status = untried;

    // preset presentation attributes
    output_text_fmt.setForeground(ANSI2col(0));

    input_text_fmt.setBackground(ANSI2col(6, true));
    //input_text_fmt.setForeground(ANSI2col(3, true));

    //setLineWrapMode(wrap_mode);
    setFont(QFont("courier"));

    connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(onCursorPositionChanged()));

    connect(this, SIGNAL(sig_run_function(pfunc)), this, SLOT(run_function(pfunc)));
}

/** strict control on keyboard events required
 */
void ConsoleEdit::keyPressEvent(QKeyEvent *event) {

    using namespace Qt;

    QTextCursor c = textCursor();

    bool on_completion = preds && preds->popup()->isVisible();
    if (on_completion) {
        // following keys are forwarded by the completer to the widget
        switch (event->key()) {
        case Key_Enter:
        case Key_Return:
        case Key_Escape:
        case Key_Tab:
        case Key_Backtab:
            event->ignore();
            return; // let the completer do default behavior
        default:
            compinit(c);
            break;
        }
    }

    bool ctrl = event->modifiers() == CTRL;
    bool accept = true, ret = false, down = true;
    int cp = c.position(), k = event->key();

    switch (k) {

    case Key_Space:
    case Key_Tab:
        if (!on_completion && ((k == Key_Space && ctrl) || k == Key_Tab) && cp >= fixedPosition) {
            compinit(c);
            return;
        }
        break;

    //case Key_Tab:
    case Key_Backtab:
        // otherwise tab control get lost !
        event->ignore();
        return;

    case Key_Home:
        if (!ctrl && cp > fixedPosition) {
            c.setPosition(fixedPosition, (event->modifiers() & SHIFT) ? c.KeepAnchor : c.MoveAnchor);
            setTextCursor(c);
            return;
        }
    case Key_End:
    case Key_Left:
    case Key_Right:
    case Key_PageUp:
    case Key_PageDown:
        break;

    case Key_Return:
        ret = cp >= fixedPosition;
        if (ret) {
            c.movePosition(c.End);
            setTextCursor(c);
        }
        break;

    case ';':
        if (cp == fixedPosition && c.atEnd())
            ret = true;
        else
            accept = cp > fixedPosition;
        break;

    case Key_Backspace:
        accept = cp > fixedPosition;
        break;

    case Key_Up:
        down = false;
        // fall throu
    case Key_Down:
        if (!ctrl) {
            // naive history handler
            if (cp >= fixedPosition) {
                if (!history.empty()) {
                    accept = false;
                    c.setPosition(fixedPosition);
                    c.movePosition(c.End, c.KeepAnchor);
                    c.removeSelectedText();
                    if (history_next < history.count())
                        c.insertText(history[history_next]);
                    if (down) {
                        if (history_next < history.count())
                            ++history_next;
                    } else
                        if (history_next > 0)
                            --history_next;
                    return;
                }
            }
            event->ignore();
            return;
        }
        c.movePosition(k == Key_Up ? c.Up : c.Down);
        setTextCursor(c);
        return;

    case Key_C:
    // case Key_Pause: I thought this one also work. It's not true.
        if (ctrl) {
            eng->cancel_running();
            break;
        }
        // fall throu

    default:
        accept = cp >= fixedPosition || event->matches(QKeySequence::Copy);
    }

    if (accept) {

        setCurrentCharFormat(input_text_fmt);
        ConsoleEditBase::keyPressEvent(event);

        if (on_completion) {
            c.select(QTextCursor::WordUnderCursor);
            preds->setCompletionPrefix(c.selectedText());
            preds->popup()->setCurrentIndex(preds->completionModel()->index(0, 0));
        }
    }

    if (ret) {
        c.setPosition(fixedPosition);
        c.movePosition(c.End, c.KeepAnchor);
        QString cmd = c.selectedText();
        if (!cmd.isEmpty()) {
            cmd.replace(cmd.length() - 1, 1, '\n');

            QString cmdc = cmd.left(cmd.length() - 1);
            if (!history.contains(cmdc)) {
                history_next = history.count();
                history.append(cmd.left(cmd.length() - 1));
            }
            else
                history_next = history.count() - 1;
        }

        // I don't understand why doesn't work (in thread), but the connected SLOT isn't called
        if (io)
            io->take_input(cmd);
        else
            emit user_input(cmd);
    }
}

/** completion initialize
 *  this is the simpler setup I found so far
 */
void ConsoleEdit::compinit(QTextCursor c) {

    /** issue setof(M,current_module(M),L) */
    {   SwiPrologEngine::in_thread _it;
        QStringList lmods;
        PlTerm M, Ms;
        if (PlCall("setof", PlTermv(M, PlCompound("current_module", M), Ms)))
            for (PlTail x(Ms); x.next(M); )
                lmods.append(CCP(M));
        if (lmods != lmodules) {
            lmodules = lmods;
            delete preds;
            preds = 0;
        }
    }

    if (!preds)
        initCompletion();

    c.movePosition(c.StartOfWord, c.KeepAnchor);
    preds->setCompletionPrefix(c.selectedText());
    preds->popup()->setCurrentIndex(preds->completionModel()->index(0, 0));

    QRect cr = cursorRect();
    cr.setWidth(300);
    preds->complete(cr);
}

/** handle focus event to keep QCompleter happy
 */
void ConsoleEdit::focusInEvent(QFocusEvent *e) {
    if (preds)
        preds->setWidget(this);
    ConsoleEditBase::focusInEvent(e);
}

/** \brief send text to output
 *
 *  Decode ANSI terminal sequences, to output coloured text.
 *  Colours encoding are (approx) derived from swipl console.
 */
void ConsoleEdit::user_output(QString text) {
    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::End);

    // filter and apply (some) ANSI sequence
    int pos = text.indexOf('\e');
    if (pos >= 0) {
        int left = 0;
        static QRegExp eseq("\e\\[(?:(3([0-7]);([01])m)|(0m)|(1m;)|1;3([0-7])m|(1m)|(?:3([0-7])m))");
        forever {
            int pos1 = eseq.indexIn(text, pos);
            if (pos1 == -1)
                break;

            QStringList lcap = eseq.capturedTexts();
            Q_ASSERT(lcap.length() == 9); // match captures in eseq, 0 seems unrelated to paren

            // put 'out-of-band' text with current attribute, before changing it
            //c.insertText(text.mid(pos, pos1 - pos), output_text_fmt);
            c.insertText(text.mid(left, pos1 - left), output_text_fmt);

            // map sequence to text attributes
            QFont::Weight w;
            QBrush c;
            int skip = lcap[1].length();
            if (skip) {
                QString A = lcap[2], B = lcap[3];
                w = QFont::Normal;
                c = ANSI2col(B.toInt(), A == "1");
            }
            else if (!lcap[6].isNull()) {
                skip  = 5;
                w = QFont::Bold;
                c = ANSI2col(lcap[6].toInt());
            }
            else if ((skip = lcap[7].length()) > 0) {
                w = QFont::Bold;
                c = ANSI2col(0);
            }
            else if (!lcap[8].isNull()) {
                skip = 3;
                w = QFont::Normal;
                c = ANSI2col(lcap[8].toInt());
            }
            else {
                skip = lcap[4].length() + lcap[5].length();
                w = QFont::Normal;
                c = ANSI2col(0);
            }
            output_text_fmt.setFontWeight(w);
            output_text_fmt.setForeground(c);

            left = pos = pos1 + skip + 2; // add the SCI
        }

        c.insertText(text.mid(pos), output_text_fmt);
    }
    else
        c.insertText(text, output_text_fmt);

    if (update_refresh_rate > 0 && ++count_output == update_refresh_rate) {
        count_output = 0;
        moveCursor(QTextCursor::End);
        ensureCursorVisible();
        repaint();

        QEventLoop lp;
        QTimer::singleShot(1, &lp, SLOT(quit()));
        lp.exec();
    }
}

/** issue an input request
 */
void ConsoleEdit::user_prompt(int threadId) {
    //qDebug() << "user_prompt" << CVP(this) << threadId;

    Q_ASSERT(thid == -1 || thid == threadId);
    if (thid == -1)
        thid = threadId;

    if (helpidx_status == untried) {
        helpidx_status = missing;
        SwiPrologEngine::in_thread _e;
        try {
            if (PlCall("load_files(library(helpidx), [silent(true)])") &&
                PlCall("current_module(help_index)"))
                helpidx_status = available;
        }
        catch(PlException e) {
            qDebug() << CCP(e);
        }
        //qDebug() << "help_index" << helpidx_status;
    }

    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::End);
    fixedPosition = c.position();
    ensureCursorVisible();

    if (commands.count() > 0)
        QTimer::singleShot(1, this, SLOT(command_do()));
}

/** push command on queue
 */
bool ConsoleEdit::command(QString cmd) {
    commands.append(cmd);
    if (commands.count() == 1)
        QTimer::singleShot(1, this, SLOT(command_do()));
    return true;
}

/** push command from queue to Prolog processor
 */
void ConsoleEdit::command_do() {
    QString cmd = commands.takeFirst();
    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::End);
    c.insertText(cmd);
    emit user_input(cmd);
}

/** handle tooltip from helpidx to display current cursor word synopsis
 */
bool ConsoleEdit::event(QEvent *event) {
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent *helpEvent = static_cast<QHelpEvent*>(event);
        if (!last_tip.isEmpty())
            QToolTip::showText(helpEvent->globalPos(), last_tip);
        else {
            QToolTip::hideText();
            event->ignore();
        }
        return true;
    }

    return ConsoleEditBase::event(event);
}

/** sense word under cursor for tooltip display
 */
bool ConsoleEdit::eventFilter(QObject *, QEvent *event) {
    if (event->type() == QEvent::MouseMove) {
        set_cursor_tip(cursorForPosition(static_cast<QMouseEvent*>(event)->pos()));
    }
    return false;
}

/** attempt - miserably failed - to gracefully stop XPCE thread
 */
bool ConsoleEdit::can_close() {
    SwiPrologEngine::in_thread _e;
    bool pce_running = false;
    try {
        QString pce("pce"), status("status"), running("running");
        PlTerm Id, Prop;
        PlQuery q("thread_property", PlTermv(Id, Prop));
        while (q.next_solution()) {
            if (    S(Id) == pce &&
                    Prop.name() == status &&
                    Prop[1].name() == running)
                pce_running = true;
        }
        if (pce_running) {
            if (!PlCall("in_pce_thread(send(@pce, die, 0))"))
                qDebug() << "XPCE fail to quit";
            else
                pce_running = false;
        }
    }
    catch(PlException e) {
        qDebug() << CCP(e);
    }
    return !pce_running;
}

/** display different cursor where editing available
 */
void ConsoleEdit::onCursorPositionChanged() {
    QTextCursor c = textCursor();
    set_cursor_tip(c);
    if (fixedPosition > c.position()) {
        viewport()->setCursor(Qt::OpenHandCursor);

        // attempt to jump on message location
        c.movePosition(c.StartOfLine);
        c.movePosition(c.EndOfLine, c.KeepAnchor);

        QString line = c.selectedText();
        QStringList parts = line.split(':');

        /* using regex would be more elegant, but it's difficult to get it working properly...
        static QRegExp msg("(ERROR|Warning):([^:]+):*$");
        if (msg.exactMatch(line)) {
            parts = msg.capturedTexts();
        }
        */
        if (parts.count() > 3 && (parts[0] == QString("Warning") || parts[0] == QString("ERROR"))) {
            int p = 1;
            if (parts[p].length() == 2) { // Windoze paths: C:/ etc
                parts[p+1] = parts[p] + parts[p+1];
                ++p;
            }
            if (parts[p][0] == ' ') {
                QString path = parts[p].mid(1);
                bool is_numl;
                int numline = parts[p+1].toInt(&is_numl);
                if (is_numl)
                    eng->query_run(QString("edit('%1':%2)").arg(path).arg(numline));
            }
        }
    } else
        viewport()->setCursor(Qt::IBeamCursor);
}

/** setup tooltip info
 */
void ConsoleEdit::set_cursor_tip(QTextCursor c) {
    if (helpidx_status == available) {
        c.select(c.WordUnderCursor);
        QString w = c.selectedText();
        if (!w.isEmpty() && w != last_word) {
            last_word = w;
            SwiPrologEngine::in_thread tr;
            try {
                T Name = A(last_word), Arity, Descr, Start, Stop;
                PlQuery q("help_index", "predicate", V(Name, Arity, Descr, Start, Stop));
                QStringList tips;
                while (q.next_solution()) {
                    long arity = Arity.type() == PL_INTEGER ? long(Arity) : -1;
                    tips.append(QString("%1/%2:%3").arg(w).arg(arity).arg(CCP(Descr)));
                }
                last_tip = tips.join("\n");
            }
            catch(PlException e) {
                qDebug() << CCP(e);
            }

            if (!last_tip.isEmpty())
                setToolTip(last_tip);
        }
    }
}

/** serve the user menu issuing the command
 */
void ConsoleEdit::onConsoleMenuAction() {
    QAction *a = qobject_cast<QAction *>(sender());
    if (a) {
        QString t = a->toolTip(),
                module = t.left(t.indexOf(':')),
                query = t.mid(t.indexOf(':') + 1);
        eng->query_run(module, query);
    }
}

/** place accepted Completer selection in editor
 *  very clean, after removing (useless?) customcompleter sample code
 */
void ConsoleEdit::insertCompletion(QString completion) {
    int extra = completion.length() - preds->completionPrefix().length();
    textCursor().insertText(completion.right(extra));
}

/** hack around a critical race - apparently
 */
void ConsoleEdit::initCompletion() {
    Q_ASSERT(!preds);
    lpreds.clear();
    Completion::initialize(lpreds);
    preds = new t_Completion(lpreds);
    preds->setWidget(this);
    connect(preds, SIGNAL(activated(QString)), this, SLOT(insertCompletion(QString)));
    qDebug() << "initCompletion" << lpreds.count();
}

/** remove all text
 */
void ConsoleEdit::tty_clear() {
    clear();
    fixedPosition = 0;
}

/** issue instancing in GUI thread (cant moveToThread a Widget)
 */
void ConsoleEdit::new_console(Swipl_IO *io, QString title) {
    Q_ASSERT(io->host == 0);

    auto r = new req_new_console(io, title);
    QApplication::instance()->postEvent(this, r);

    io->wait_console();
}

/** let background thread resume execution
 */
void ConsoleEdit::attached() {
    io->attached(this);
}

/** added to serve creation in thread
 *  signal from foreign thread to Qt was not fired
 */
void ConsoleEdit::customEvent(QEvent *event) {

    qDebug() << "new_console_req" << CVP(this) << CVP(CT);

    Q_ASSERT(event->type() == QEvent::User);
    auto e = static_cast<req_new_console *>(event);

    /* fire and forget :) auto ce = */
    new ConsoleEdit(e->iop, e->title);
}
