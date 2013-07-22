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
#include "pqMainWindow.h"
#include "do_events.h"

#include <QKeyEvent>
#include <QRegExp>
#include <QtDebug>
#include <QAction>
#include <QMainWindow>
#include <QApplication>
#include <QToolTip>
#include <QStringListModel>

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

/** build command line interface to SWI Prolog engine
 *  this start the *primary* console
 */
ConsoleEdit::ConsoleEdit(int argc, char **argv, QWidget *parent)
    : ConsoleEditBase(parent), io(0)
{
    qApp->setWindowIcon(QIcon(":/swipl.png"));

    qRegisterMetaType<pfunc>("pfunc");

    setup();
    eng = new SwiPrologEngine;

    // wire up console IO
    connect(eng, SIGNAL(user_output(QString)), this, SLOT(user_output(QString)));
    connect(eng, SIGNAL(user_prompt(int, bool)), this, SLOT(user_prompt(int, bool)));
    connect(this, SIGNAL(user_input(QString)), eng, SLOT(user_input(QString)));

    //connect(eng, SIGNAL(sig_run_function(pfunc)), eng, SLOT(run_function(pfunc)));

    // issue worker thread start
    eng->start(argc, argv);
}

/** this start an *interactor* console hosted in a QMainWindow
 */
ConsoleEdit::ConsoleEdit(Swipl_IO* io, QString title)
    : ConsoleEditBase(), eng(0), io(io)
{
    auto w = new QMainWindow();
    w->setCentralWidget(this);
    w->setWindowTitle(title);
    w->show();
    setup(io);
}

/** start an *interactor* console in tabbed interface
 *  no MainWindow to attach this
 */
ConsoleEdit::ConsoleEdit(Swipl_IO* io)
    : ConsoleEditBase(), eng(0), io(io)
{
    setup(io);
}

/** more factorization, after introducing the possibility
 *  of instancing in a tabbed interface
 */
void ConsoleEdit::setup(Swipl_IO* io) {
    io->host = this;

    setup();

    // wire up console IO
    connect(io, SIGNAL(user_output(QString)), this, SLOT(user_output(QString)));
    connect(io, SIGNAL(user_prompt(int, bool)), this, SLOT(user_prompt(int, bool)));
    connect(this, SIGNAL(user_input(QString)), io, SLOT(user_input(QString)));

    QTimer::singleShot(100, this, SLOT(attached()));
}

/** common setup between =main= and =thread= console
 *  different setting required, due to difference in events handling
 */
void ConsoleEdit::setup() {

    status = idle;
    promptPosition = -1;

    qApp->installEventFilter(this);
    thid = -1;
    count_output = 0;
    update_refresh_rate = 100;
    preds = 0;

    // preset presentation attributes
    output_text_fmt.setForeground(ANSI2col(0));
    input_text_fmt.setBackground(ANSI2col(6, true));

    //setLineWrapMode(wrap_mode);
    setFont(QFont("courier", 12));

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
        if (!on_completion && ctrl && cp >= fixedPosition) {
            compinit2(c);
            return;
        }
        break;
    case Key_Tab:
        if (!on_completion && !ctrl && cp >= fixedPosition) {
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

    // this ugly hack is necessary because I don't know
    // when the engine is expecting a single char...
        /*
    case ';':
        if (cp == fixedPosition && c.atEnd()) {
            setCurrentCharFormat(input_text_fmt);
            ConsoleEditBase::keyPressEvent(event);
            emit user_input(";");
            return;
        }
        else
            accept = cp > fixedPosition;
        break;
        */

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
            if (eng)
                eng->cancel_running();
            break;
        }
        // fall throu

    default:
        accept = cp >= fixedPosition || event->matches(QKeySequence::Copy);
    }

    QString cmd;

    if (accept) {

        setCurrentCharFormat(input_text_fmt);
        ConsoleEditBase::keyPressEvent(event);

        if (on_completion) {
            c.select(QTextCursor::WordUnderCursor);
            preds->setCompletionPrefix(c.selectedText());
            preds->popup()->setCurrentIndex(preds->completionModel()->index(0, 0));
        }
        else {

            if (is_tty && c.atEnd()) {
                cmd = QString(QChar(k));
                goto _cmd_;
            }

            // handle ^A+Del (clear buffer)
            c.movePosition(c.End);
            if (fixedPosition > c.position())
                fixedPosition = c.position();
        }
    }

    if (ret) {
        c.setPosition(fixedPosition);
        c.movePosition(c.End, c.KeepAnchor);
        cmd = c.selectedText();
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

    _cmd_:
        if (io)
            io->take_input(cmd);
        else
            emit user_input(cmd);

        status = running;
    }
}

/** place accepted Completer selection in editor
 *  very clean, after removing (useless?) customcompleter sample code
 */
void ConsoleEdit::insertCompletion(QString completion) {
    int sep = completion.indexOf(" | ");
    if (sep > 0)    // remove description
        completion = completion.left(sep);
    int extra = completion.length() - preds->completionPrefix().length();
    textCursor().insertText(completion.right(extra));
}

/** completion initialize
 *  this is the simpler setup I found so far
 */
void ConsoleEdit::compinit(QTextCursor c) {

    /*/ issue setof(M,current_module(M),L)
    QStringList lmods;
    {   SwiPrologEngine::in_thread _it;
        PlTerm M, Ms;
        if (PlCall("setof", PlTermv(M, PlCompound("current_module", M), Ms)))
            for (PlTail x(Ms); x.next(M); )
                lmods.append(CCP(M));
    }
    */

    //if (!preds || lmods != lmodules)
    {
        //lmodules = lmods;

        QStringList lpreds;
        Completion::initialize(fixedPosition, textCursor(), lpreds);

        if (!preds) {
            preds = new t_Completion(new QStringListModel(lpreds));
            preds->setWidget(this);
            connect(preds, SIGNAL(activated(QString)), this, SLOT(insertCompletion(QString)));
        }
        else {
            auto model = qobject_cast<QStringListModel*>(preds->model());
            model->setStringList(lpreds);
        }
    }

    c.movePosition(c.StartOfWord, c.KeepAnchor);
    preds->setCompletionPrefix(c.selectedText());
    preds->popup()->setCurrentIndex(preds->completionModel()->index(0, 0));

    QRect cr = cursorRect();
    cr.setWidth(300);
    preds->complete(cr);
}

void ConsoleEdit::compinit2(QTextCursor c) {

    QStringList atoms;
    Completion::initialize(fixedPosition, textCursor(), atoms);
    qDebug() << "compinit2" << "atoms" << atoms.count();

    if (!preds) {
        preds = new t_Completion(new QStringListModel());
        preds->setWidget(this);
        connect(preds, SIGNAL(activated(QString)), this, SLOT(insertCompletion(QString)));
    }

    QStringList lpreds;
    foreach (auto a, atoms) {
        auto p = Completion::pred_docs.constFind(a);
        if (p != Completion::pred_docs.end())
            foreach (auto d, p.value()) {
                QStringList la;
                for (int n = 0; n < d.first; ++n)
                    la.append(QString('A' + n));
                if (!la.isEmpty())
                    lpreds.append(QString("%1(%2) | %3").arg(a).arg(la.join(", ")).arg(d.second));
                else
                    lpreds.append(QString("%1 | %2").arg(a).arg(d.second));
            }
        else
            lpreds.append(a);
    }

    auto model = qobject_cast<QStringListModel*>(preds->model());
    model->setStringList(lpreds);

    c.movePosition(c.StartOfWord, c.KeepAnchor);
    preds->setCompletionPrefix(c.selectedText());
    preds->popup()->setCurrentIndex(preds->completionModel()->index(0, 0));

    QRect cr = cursorRect();
    cr.setWidth(400);
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
    if (status == wait_input)
        c.setPosition(promptPosition);
    else {
        promptPosition = c.position();  // save for later
        c.movePosition(QTextCursor::End);
    }

    auto instext = [&](QString text) {
        c.insertText(text, output_text_fmt);
        if (status == wait_input) {
            promptPosition += text.length();
            fixedPosition += text.length();
            ensureCursorVisible();
        }
    };

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
            instext(text.mid(left, pos1 - left));

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

        instext(text.mid(pos));
    }
    else
        instext(text);

    if (update_refresh_rate > 0 && ++count_output == update_refresh_rate) {
        count_output = 0;
        c.movePosition(c.End);
        setTextCursor(c);
        ensureCursorVisible();
        repaint();
        do_events();
    }
}

/** issue an input request
 */
void ConsoleEdit::user_prompt(int threadId, bool tty) {

    Q_ASSERT(thid == -1 || thid == threadId);
    if (thid == -1) {
        Q_ASSERT(status == idle);
        thid = threadId;
    }

    is_tty = tty;

    Completion::helpidx();

    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::End);
    fixedPosition = c.position();
    setTextCursor(c);
    ensureCursorVisible();

    status = wait_input;

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

/** attempt to gracefully stop XPCE thread
 */
bool ConsoleEdit::can_close() {
    bool quit = true;
    if (eng) {
        eng->query_run("halt");
        for (int i = 0; i < 20; ++i) {
            if ((quit = eng->isFinished()))
                break;
            SwiPrologEngine::msleep(500);
        }
    }
    return quit;
}

/** display different cursor where editing available
 */
void ConsoleEdit::onCursorPositionChanged() {
    QTextCursor c = textCursor();

    //qDebug() << "onCursorPositionChanged" << c.position();

    set_cursor_tip(c);
    if (fixedPosition > c.position()) {
        viewport()->setCursor(Qt::OpenHandCursor);

        // attempt to jump on message location
        c.movePosition(c.StartOfLine);
        c.movePosition(c.EndOfLine, c.KeepAnchor);

        QString line = c.selectedText();
        QStringList parts = line.split(':');

        /* using regex would be more elegant, but I found difficult to get it working properly...
        static QRegExp msg("(ERROR|Warning):([^:]+):*$");
        if (msg.exactMatch(line)) {
            parts = msg.capturedTexts();
        }
        */
        if (parts.count() > 3 && (parts[0] == QString("Warning") || parts[0] == QString("ERROR"))) {
            int p = 1;
            if (parts[p].length() == 2) { // Windoze paths: C:/ etc
                parts[p+1] = parts[p] + ':' + parts[p+1];
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
    last_tip = Completion::pred_tip(c);
    if (!last_tip.isEmpty())
        setToolTip(last_tip);
}

/** serve the user menu issuing the command
 */
void ConsoleEdit::onConsoleMenuAction() {
    auto a = qobject_cast<QAction *>(sender());
    if (a) {
        QString t = a->toolTip(),
                module = t.left(t.indexOf(':')),
                query = t.mid(t.indexOf(':') + 1);
        eng->query_run(module, query);
    }
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

    // multi tabbed interface:
    pqMainWindow *mw = 0;
    for (QWidget *w = parentWidget(); w && !mw; w = w->parentWidget())
        mw = qobject_cast<pqMainWindow*>(w);

    if (mw)
        mw->addConsole(new ConsoleEdit(e->iop), e->title);
    else    /* fire and forget :) auto ce = */
        new ConsoleEdit(e->iop, e->title);
}

/** store lines from swipl-win console protocol
 */
void ConsoleEdit::add_history_line(QString line)
{
    if (!history.contains(line)) {
        history_next = history.count();
        history.append(line);
    }
}
