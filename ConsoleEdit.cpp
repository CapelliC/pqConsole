/*
    pqConsole    : interfacing SWI-Prolog and Qt

    Author       : Carlo Capelli
    E-mail       : cc.carlo.cap@gmail.com
    Copyright (C): 2013,2014 Carlo Capelli

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

#include <SWI-Stream.h>

#include "Swipl_IO.h"
#include "do_events.h"
#include "PREDICATE.h"
#include "Completion.h"
#include "Preferences.h"
#include "pqMainWindow.h"
#include "pqConsole.h"

#include <signal.h>

#include <QUrl>
#include <QTime>
#include <QRegExp>
#include <QtDebug>
#include <QAction>
#include <QToolTip>
#include <QKeyEvent>
#include <QMimeData>
#include <QTextBlock>
#include <QMessageBox>
#include <QMainWindow>
#include <QApplication>
#include <QStringListModel>

/** peek color by index */
static QColor ANSI2col(int c, bool highlight = false) { return Preferences::ANSI2col(c, highlight); }

/** can be disabled from ~/.plrc */
bool ConsoleEdit::color_term = true;

/** build command line interface to SWI Prolog engine
 *  this start the *primary* console
 */
ConsoleEdit::ConsoleEdit(int argc, char **argv, QWidget *parent)
    : ConsoleEditBase(parent), io(0)
{
    qApp->setWindowIcon(QIcon(":/swipl.png"));

    qRegisterMetaType<pfunc>("pfunc");

    setup();
    eng = new SwiPrologEngine(this);

    // wire up console IO
    connect(eng, SIGNAL(user_output(QString)), this, SLOT(user_output(QString)));
    connect(eng, SIGNAL(user_prompt(int, bool)), this, SLOT(user_prompt(int, bool)));
    connect(this, SIGNAL(user_input(QString)), eng, SLOT(user_input(QString)));

    connect(eng, SIGNAL(finished()), this, SLOT(eng_completed()));

    // issue worker thread start
    eng->start(argc, argv);

    // reactive console
    connect(this, SIGNAL(anchorClicked(const QUrl &)), this, SLOT(anchorClicked(const QUrl &)));
}

void ConsoleEdit::add_thread(int id) {
    Q_ASSERT(id > 0);
    Q_ASSERT(thids.empty());
    thids.append(id);
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

/** handle consoles list
 */
ConsoleEdit::~ConsoleEdit() {
    pqConsole::removeConsole(this);
}

/** more factorization, after introducing the possibility
 *  of instancing in a tabbed interface
 */
void ConsoleEdit::setup(Swipl_IO* io) {

    setup();

    // wire up console IO
    connect(io, SIGNAL(user_output(QString)), this, SLOT(user_output(QString)));
    connect(io, SIGNAL(user_prompt(int, bool)), this, SLOT(user_prompt(int, bool)));
    connect(this, SIGNAL(user_input(QString)), io, SLOT(user_input(QString)));

    connect(io, SIGNAL(sig_eng_at_exit()), this, SLOT(eng_completed()));

    io->attached(this);
}

/** common setup between =main= and =thread= console
 *  different setting required, due to difference in events handling
 */
void ConsoleEdit::setup() {

    set_editable(true);
    pqConsole::addConsole(this);

    status = idle;
    promptPosition = -1;

    // added to handle reactive actions
    parsedStart = 0; //parsedLimit = -1;

    qApp->installEventFilter(this);
    count_output = 0;
    update_refresh_rate = 100;
    preds = 0;

    Preferences p;

    // preset presentation attributes
    output_text_fmt.setForeground(ANSI2col(p.console_out_fore));
    output_text_fmt.setBackground(ANSI2col(p.console_out_back));

    input_text_fmt.setForeground(ANSI2col(p.console_inp_fore));
    input_text_fmt.setBackground(ANSI2col(p.console_inp_back));

    setLineWrapMode(p.wrapMode);
    setFont(p.console_font);

    connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(onCursorPositionChanged()));

    connect(this, SIGNAL(sig_run_function(pfunc)), this, SLOT(run_function(pfunc)));
    connect(this, SIGNAL(selectionChanged()), this, SLOT(selectionChanged()));
}

/** strict control on keyboard events required
 */
void ConsoleEdit::keyPressEvent(QKeyEvent *event) {

    using namespace Qt;
    //qDebug() << "keyPressEvent" << event;

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
    int cp = c.position(), k = event->key();

    bool accept = true, ret = false, down = true, editable = (cp >= fixedPosition);

    QString cmd;

    switch (k) {

    case Key_Space:
        if (!on_completion && ctrl && editable) {
            compinit2(c);
            return;
        }
        accept = editable;
        break;
    case Key_Tab:
        if (ctrl) {
            event->ignore(); // otherwise tab control get lost !
            return;
        }
        if (!on_completion && !ctrl && editable) {
            compinit(c);
            return;
        }
        break;

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
        ret = editable;
        if (ret) {
            c.movePosition(c.End);
            setTextCursor(c);
        }
        break;

    case Key_Backspace:
        accept = (cp > fixedPosition);
        break;

    case Key_Up:
        down = false;
        // fall throu
    case Key_Down:
        if (ctrl) { //if (!ctrl) {
            // naive history handler
            if (editable) {
                if (!history.empty()) {

                    c.setPosition(fixedPosition);
                    c.movePosition(c.End, c.KeepAnchor);

                    auto repc = [&](QString t) {
                        c.removeSelectedText();
                        c.insertText(t, input_text_fmt);
                        c.movePosition(c.End);
                        ensureCursorVisible();
                    };

                    if (down) {
                        if (history_next < history.count() - 1)
                            repc(history[++history_next]);
                        else if (history_next == history.count() - 1) {
                            ++history_next;
                            repc(history_spare);
                        }
                    } else {
                        if (history_next == history.count()) {
                            history_spare = c.selectedText();
                            repc(history[--history_next]);
                        } else if (history_next > 0)
                            repc(history[--history_next]);
                    }
                    return;
                }
            }
            event->ignore();
            return;
        }
        c.movePosition(k == Key_Up ? c.Up : c.Down);
        setTextCursor(c);
        return;

    case Key_D:
        #ifdef Q_OS_DARWIN
        if ((accept = editable) && event->modifiers() == META)
        #else
        if ((accept = editable) && ctrl)
        #endif
        {   qDebug() << "^D" << thids;
            c.movePosition(c.End);
            setTextCursor(c);
            ret = true;
            status = eof;
        }
        break;

    case Key_C:
    // case Key_Pause: I thought this one also work. It's not true.
        if (ctrl && status == running) {
            qDebug() << "^C" << thids << status;
            PL_thread_raise(thids[0], SIGINT);
            return;
        }
        // fall throu

    default:

        accept = editable || event->matches(QKeySequence::Copy);

        // when keypressed in output area place at cursor and accept
        if (!accept && !editable && !ret && !event->modifiers()) {
                //!event->modifiers() == CTRL && !event->modifiers() == ALT && !event->modifiers() == SHIFT && !event->modifiers() == META) {
            c.movePosition(c.End);
            setTextCursor(c);
            ensureCursorVisible();

            if (fixedPosition > c.position())
                fixedPosition = c.position();

            setCurrentCharFormat(input_text_fmt);
            ConsoleEditBase::keyPressEvent(event);

            if (is_tty) {
                cmd = event->text();
                if (!cmd.isEmpty())
                    goto _cmd_;
            }
            return;
        }
    }

    if (accept) {

        setCurrentCharFormat(input_text_fmt);
        ConsoleEditBase::keyPressEvent(event);

        if (is_tty && c.atEnd()) {
            cmd = event->text();
            if (!cmd.isEmpty())
                goto _cmd_;
        }

        if (on_completion) {
            c.select(QTextCursor::WordUnderCursor);
            preds->setCompletionPrefix(c.selectedText());
            preds->popup()->setCurrentIndex(preds->completionModel()->index(0, 0));
        }
        else {
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
            add_history_line(cmd.left(cmd.length() - 1));
        }

    _cmd_:
        if (io)
            io->take_input(cmd);
        else
            emit user_input(cmd);

    if ( status != eof || !cmd.isEmpty() )
        status = running;
    }
}

/** jump to source location on warning/error messages
 */
void ConsoleEdit::mousePressEvent(QMouseEvent *e) {
    QTextCursor c = cursorForPosition(e->pos());
    clickable_message_line(c, false);
    ConsoleEditBase::mousePressEvent(e);

    // still need to experiment if actually was QTextBrowser to inhibit the feature...
#ifdef PQCONSOLE_BROWSER
    if (e->button() == Qt::MiddleButton) {
        if (QMimeData *d = createMimeDataFromSelection()) {
            insertFromMimeData(d);
            d->deleteLater();
        }
    }
#endif
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

    QStringList lpreds;
    QString prefix = Completion::initialize(fixedPosition, c, lpreds);

    if (!preds) {
        preds = new t_Completion(new QStringListModel(lpreds));
        preds->setWidget(this);
        connect(preds, SIGNAL(activated(QString)), this, SLOT(insertCompletion(QString)));
    }
    else {
        auto model = qobject_cast<QStringListModel*>(preds->model());
        model->setStringList(lpreds);
    }

    preds->setCompletionPrefix(prefix);
    preds->popup()->setCurrentIndex(preds->completionModel()->index(0, 0));

    QRect cr = cursorRect();
    cr.setWidth(300);
    preds->complete(cr);
}

void ConsoleEdit::compinit2(QTextCursor c) {

    QStringList atoms;
    QString prefix = Completion::initialize(fixedPosition, c, atoms);

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

    preds->setCompletionPrefix(prefix);
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

/** filter out insertion when cursor is not in editable position
 */
void ConsoleEdit::insertFromMimeData(const QMimeData *source) {
    qDebug() << "insertFromMimeData" << source;
    auto c = textCursor();
    if (c.position() >= fixedPosition)
        ConsoleEditBase::insertFromMimeData(source);
    else {
        c.movePosition(c.End);
        c.insertText(source->text());
    }
}

/** \brief send text to output
 *
 *  Decode ANSI terminal sequences, to output coloured text.
 *  Colours encoding are (approx) derived from swipl console.
 */
void ConsoleEdit::user_output(QString text) {

#if defined(Q_OS_WIN)
    text.replace("\r\n", "\n");
#endif

    QTextCursor c = textCursor();
    if (status == wait_input)
        c.setPosition(promptPosition);
    else {
        promptPosition = c.position();  // save for later
        c.movePosition(QTextCursor::End);
    }

    auto instext = [&](QString text) {
        c.insertText(text, output_text_fmt);
        // Jan requested extension: put messages *above* the prompt location
        if (status == wait_input) {
            int ltext = text.length();
            promptPosition += ltext;
            fixedPosition += ltext;
            ensureCursorVisible();
        }
    };

    // filter and apply (some) ANSI sequence
    int pos = text.indexOf('\x1B');
    if (pos >= 0) {
        int left = 0;

        static QRegExp eseq("\x1B\\[(?:(3([0-7]);([01])m)|(0m)|(1m;)|1;3([0-7])m|(1m)|(?:3([0-7])m))");

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

    linkto_message_source();
}

bool ConsoleEdit::match_thread(int thread_id) const {
    return thread_id == -1 || thids.contains(thread_id);
}

/** issue an input request
 */
void ConsoleEdit::user_prompt(int threadId, bool tty) {

    // attach thread IO to this console
    if (!thids.contains(threadId))
        thids.append(threadId);

    is_tty = tty;

    //Completion::helpidx();

    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::End);
    fixedPosition = c.position();
    setTextCursor(c);
    ensureCursorVisible();

    if (status == idle)
        emit engine_ready();

    status = wait_input;

    if (commands.count() > 0)
        QTimer::singleShot(1, this, SLOT(command_do()));

    linkto_message_source();
}

/** resolve error messages positions
 *  but delay replacement after document' block scan
 */
void ConsoleEdit::linkto_message_source() {

    auto c = textCursor();
    struct to_replace_pos { int pos, len; QString html; };
    QList<to_replace_pos> to_replace;

    // scan blocks looking for error messages
    for (QTextBlock block = c.document()->findBlockByNumber(parsedStart);
         block.blockNumber() < c.document()->blockCount() - 1; block = block.next()) {
         //block != c.document()->end(); block = block.next()) {

        QString text = block.text();
        ++parsedStart;

        static QRegExp jmsg("(ERROR|Warning):[ \t]*(([a-zA-Z]:)?[^:]+):([0-9]+)(:([0-9]+))?.*", Qt::CaseSensitive, QRegExp::RegExp2);
        if (jmsg.exactMatch(text)) {
            QStringList parts = jmsg.capturedTexts();
            QString path = parts[2].trimmed();
            int opb = path.indexOf('['), clb;
            if (opb >= 0 && (clb = path.indexOf(']', opb+1)) > opb)
                path = path.mid(clb + 1).trimmed();

            auto edit = QString("'%1':%2").arg(path).arg(parts[4].trimmed());
            if (!parts[6].isEmpty())
                edit += ":" + parts[6];

            int pos = text.indexOf(path);
            Q_ASSERT(pos > 0);

            // make the source reference clickable
            to_replace << to_replace_pos {
                block.position() + pos, path.length(),
                QString("<a href=\"system:edit(%1)\">%2</a>").arg(edit, path)};
        }
    }

    // apply delayed replacement
    foreach(auto b, to_replace) {
        c.setPosition(b.pos);
        c.setPosition(b.pos + b.len, c.KeepAnchor);
        c.removeSelectedText();
        c.insertHtml(b.html);
    }
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

    c.movePosition(QTextCursor::End);
    promptPosition = fixedPosition = c.position();

    emit user_input(cmd);
}

/** handle tooltip from helpidx to display current cursor word synopsis
 */
bool ConsoleEdit::event(QEvent *event) {

#if defined(PQCONSOLE_BROWSER)
    // there is a bug when QTextBrowser == ConsoleEditBase: trying to avoid discarding message
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *k = static_cast<QKeyEvent*>(event);
        if (k->key() == Qt::Key_Tab) {
            qDebug() << "ignore" << event->type();
            event->ignore();
            return true;
        }
    }
#endif

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
        QTextCursor c = cursorForPosition(static_cast<QMouseEvent*>(event)->pos());
        set_cursor_tip(c);
        clickable_message_line(c, true);
    }
    return false;
}

/** the user identifying label is attached somewhere to parents chain
 */
QString ConsoleEdit::titleLabel() {
    QString title;
    for (QWidget *p = parentWidget(); title.isEmpty() && p; p = p->parentWidget())
        if (auto t = qobject_cast<QTabWidget*>(p))
            title = t->tabText(t->indexOf(this));
        else
            title = p->windowTitle();
    return title;
}

/** attempt to gracefully stop XPCE thread
 */
bool ConsoleEdit::can_close() {
    if (eng && is_running()) {
        QMessageBox b(this);
        b.setText(tr("[%1] is running a query.\nQuit anyway ?").arg(titleLabel())); //thread_id()
        b.setStandardButtons(b.Yes|b.No);
        b.setIcon(b.Question);
        return b.exec() == b.Yes;
    }
    return true;
}

/** display different cursor where editing available
 */
void ConsoleEdit::onCursorPositionChanged() {
    QTextCursor c = textCursor();
    set_cursor_tip(c);
    if (fixedPosition > c.position()) {
        viewport()->setCursor(Qt::OpenHandCursor);
        set_editable(false);
        clickable_message_line(c, true);
    } else {
        set_editable(true);
        viewport()->setCursor(Qt::IBeamCursor);
    }

    if (pmatched.size()) {
        pmatched.format_both(c);
        pmatched = ParenMatching::range();
    }

    ParenMatching pm(c);
    if (pm)
        (pmatched = pm.positions).format_both(c, pmatched.bold());
}

/** check if line content is appropriate, then highlight or open editor on it */
#ifndef PQCONSOLE_HANDLE_HOOVERING

void ConsoleEdit::clickable_message_line(QTextCursor c, bool highlight) {
    Q_UNUSED(c)
    Q_UNUSED(highlight)
}

#else

void ConsoleEdit::clickable_message_line(QTextCursor c, bool highlight) {

    c.movePosition(c.StartOfLine);

    int cposition_ = c.position();
    QTextCharFormat fposition_ = c.charFormat();

    c.movePosition(c.EndOfLine, c.KeepAnchor);

    QString line = c.selectedText();
    static QRegExp msg("(ERROR|Warning):[ \t]*(([a-zA-Z]:)?[^:]+):([0-9]+)(:([0-9]+))?.*",
               Qt::CaseSensitive, QRegExp::RegExp2);
    if ( msg.exactMatch(line) ) {
        QStringList parts = msg.capturedTexts();
     // qDebug() << "file" << parts[2] << "line" << parts[4] << "char" << parts[6];

    if ( highlight ) {
        if (cposition != cposition_) {
            cposition = cposition_;
        fposition = fposition_;
        QTextCharFormat f = fposition_;
        f.setFontUnderline(true);
        c.setCharFormat(f);
        }
        return;
    } else {
        auto cmd = QString("edit('"+parts[2]+"':"+parts[4]);
        if ( !parts[6].isEmpty() )
            cmd += ":"+parts[6];
        cmd += ")";
        qDebug() << cmd;
        query_run(cmd);
    }
    }

    if (fposition != QTextCharFormat()) {
        c.setPosition(cposition);
        c.movePosition(c.EndOfLine, c.KeepAnchor);
        c.setCharFormat(fposition);
        fposition = QTextCharFormat();
        cposition = -1;
    }
}
#endif

/** setup tooltip info
 */
void ConsoleEdit::set_cursor_tip(QTextCursor c) {
    last_tip = Completion::pred_tip(c);
    if (!last_tip.isEmpty())
        setToolTip(last_tip);
}

/** pass interrupt request to PlEngine
 */
void ConsoleEdit::int_request() {
    qDebug() << "int_request" << thids;
    if (!thids.empty())
        PL_thread_raise(thids[0], SIGINT);
}

/** serve the user menu issuing the command
 */
void ConsoleEdit::onConsoleMenuAction() {
    auto a = qobject_cast<QAction *>(sender());
    if (a) {
        QString action = a->toolTip();
        onConsoleMenuActionMap(action);
    }
}
void ConsoleEdit::onConsoleMenuActionMap(const QString& action) {
    if (auto w = find_parent<pqMainWindow>(this)) {
        if (ConsoleEdit *target = w->consoleActive()) {
            qDebug() << action << target->status << QTime::currentTime();
            if (target->status == running) {
                {   SwiPrologEngine::in_thread e;
                    int t = PL_thread_self();
                    Q_ASSERT(!target->thids.contains(t));
                    target->thids.append(t);
                    try {
                        PL_set_prolog_flag("console_thread", PL_INTEGER, t);
                        PlCall(action.toStdWString().data());
                        for (int c = 0; c < 100; c++)
                            do_events(10);
                    } catch(PlException e) {
                        qDebug() << CCP(e);
                    }
                    target->thids.removeLast();
                }
                return;
            }
            target->query_run("notrace("+action+")");
        }
    }
}

/** remove all text
 */
void ConsoleEdit::tty_clear() {
    clear();
    fixedPosition = promptPosition = parsedStart = 0;
}

/** issue instancing in GUI thread (cant moveToThread a Widget)
 */
void ConsoleEdit::new_console(Swipl_IO *io, QString title) {
    Q_ASSERT(io->target == 0);

    auto r = new req_new_console(io, title);
    QApplication::instance()->postEvent(this, r);
}

/** added to serve creation in thread
 *  signal from foreign thread to Qt was not fired
 */
void ConsoleEdit::customEvent(QEvent *event) {

    Q_ASSERT(event->type() == QEvent::User);
    auto e = static_cast<req_new_console *>(event);

    // multi tabbed interface:
    pqMainWindow *mw = 0;
    for (QWidget *w = parentWidget(); w && !mw; w = w->parentWidget())
        mw = qobject_cast<pqMainWindow*>(w);

    ConsoleEdit *nc;
    if (mw)
        mw->addConsole(nc = new ConsoleEdit(e->iop), e->title);
    else    /* fire and forget :) auto ce = */
        nc = new ConsoleEdit(e->iop, e->title);
}

/** store lines from swipl-win console protocol
 */
void ConsoleEdit::add_history_line(QString line)
{
    if (history.isEmpty() || history.back() != line)
        history.append(line);
    history_next = history.count();
    history_spare.clear();
}

/** when engine gracefully complete-...
 */
void ConsoleEdit::eng_completed() {
    if (eng) {
        eng = 0;
        // qApp->quit();
        QApplication::postEvent(qApp, new QCloseEvent);
    }
    else if (io) {
        if (auto mw = find_parent<pqMainWindow>(this))
            mw->remConsole(this);
    }
}

/** dispatch execution to appropriate object
 */
void ConsoleEdit::query_run(QString call) {
    if (eng)
        eng->query_run(call);
    else if (io)
        io->query_run(call);
}

/** dispatch qualified execution to appropriate object
 */
void ConsoleEdit::query_run(QString module, QString call) {
    if (eng)
        eng->query_run(module, call);
    else if (io)
        query_run(module + ":" + call);
}

ConsoleEdit::exec_sync::exec_sync(int timeout_ms) : timeout_ms(timeout_ms) {
    stop_ = CT;
    go_ = 0;
}
void ConsoleEdit::exec_sync::stop() {
    Q_ASSERT(CT == stop_);
    for ( ; ; ) {
        {   QMutexLocker lk(&sync);
            if (go_)
                break;
        }
        SwiPrologEngine::msleep(10);
    }
    //Q_ASSERT(go_ && go_ != stop_);
}
void ConsoleEdit::exec_sync::go() {
    Q_ASSERT(go_ == 0);
    Q_ASSERT(stop_ != 0);
    auto t = CT;
    if (stop_ != t) {
        QMutexLocker lk(&sync);
        go_ = t;
    }
    else
        go_ = t;
}

void ConsoleEdit::setSource(const QUrl &name) {
    qDebug() << "setSource" << name;
}
void ConsoleEdit::anchorClicked(const QUrl &url) {
    query_run(url.toString());
}

void ConsoleEdit::html_write(QString html) {
    auto c = textCursor();
    c.movePosition(c.End);
    c.insertHtml(html);
}

void ConsoleEdit::set_editable(bool allow) {
    //qDebug() << "set_editable" << allow << "before" << textInteractionFlags();
    if (allow)
        setTextInteractionFlags(Qt::TextEditorInteraction | Qt::TextBrowserInteraction);
    else
        setTextInteractionFlags((Qt::TextEditorInteraction | Qt::TextBrowserInteraction) & ~Qt::TextEditable);
    //qDebug() << "after" << textInteractionFlags();
}

void ConsoleEdit::selectionChanged()
{
    foreach (ExtraSelection s, extraSelections())
        s.cursor.setCharFormat(s.format);
    extraSelections().clear();

    QTextCursor c = textCursor();
    if (c.hasSelection()) {
        QString csel = c.selectedText();
        QList<ExtraSelection> lsel;
        QTextCharFormat bold = ParenMatching::range::bold();

        QTextCursor cfirst = cursorForPosition(QPoint(0, 0));
        if (!cfirst.isNull()) {
            while (c.block() != cfirst.block())
                c.movePosition(c.Up);
            c.movePosition(c.Up);
            for ( ; ; ) {
                c = document()->find(csel, c, QTextDocument::FindCaseSensitively);
                if (c.isNull() || !c.block().isVisible())
                    break;
                lsel.append(ExtraSelection {c, c.blockCharFormat()});
                c.setCharFormat(bold);
            }
        }

        setExtraSelections(lsel);
    }
}
