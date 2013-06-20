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

#define PROLOG_MODULE "pqConsole"

#include "ConsoleEdit.h"
#include "PREDICATE.h"

#include <QKeyEvent>
#include <QRegExp>
#include <QtDebug>
#include <QTimer>
#include <QEventLoop>

// some default color, seems suffciently visible to me
//
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

// SINGLETON pattern - only a console allowed in a process
//
static ConsoleEdit *con_;
ConsoleEdit* ConsoleEdit::console() { return con_; }

// allow to alter default refresh rate (simply the count of output before setting cursor at end)
//
static int update_refresh_rate = 100;
PREDICATE(set_update_refresh_rate, 1) {
    update_refresh_rate = A1;
    return TRUE;
}

// build command line interface to SWI Prolog engine
//
ConsoleEdit::ConsoleEdit(int argc, char **argv, QWidget *parent)
    : ConsoleEditBase(parent), count_output(0) {

    Q_ASSERT(con_ == 0);
    con_ = this;

    // wire up console IO
    connect(&eng, SIGNAL(user_output(QString)), this, SLOT(out(QString)));
    connect(&eng, SIGNAL(user_prompt()), this, SLOT(prompt()));
    connect(this, SIGNAL(inp(QString)), &eng, SLOT(user_input(QString)));

    connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(onCursorPositionChanged()));

    // preset presentation attributes
    tcf.setForeground(ANSI2col(0));
    setLineWrapMode(ConsoleEditBase::NoWrap);
    setFont(QFont("courier"));

    // issue worker thread start
    eng.start(argc, argv);
}

// strict control on keyboard events required
//
void ConsoleEdit::keyPressEvent(QKeyEvent *event) {

    bool accept = true, ret = false, down = true;
    QTextCursor c = textCursor();
    int cp = c.position();

    using namespace Qt;

    switch (event->key()) {

    case Key_Tab:
    case Key_Backtab:
        // otherwise tab control get lost !
        event->ignore();
        return;

    case Key_Home:
    case Key_End:
    case Key_Left:
    case Key_Right:
    case Key_PageUp:
    case Key_PageDown:
        break;

    case Key_Return:
        ret = accept = (cp > fixedPosition && c.atEnd());
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
        if (event->modifiers() == CTRL) {
            // naive 'lineedit' history handler
            if (cp >= fixedPosition) {
                if (!history.empty()) {
                    accept = false;
                    c.setPosition(fixedPosition);
                    c.movePosition(c.End, c.KeepAnchor);
                    c.removeSelectedText();
                    c.insertText(history[history_next]);
                    if (down) {
                        if (history_next < history.count() - 1)
                            ++history_next;
                    } else
                        if (history_next > 0)
                            --history_next;
                }
            }
        }
        break;

    case Key_C:
    // case Key_Pause: I thought this one also work. It's not the case.
        if (event->modifiers() == CTRL) {
            qDebug() << "SIGINT";
            eng.cancel_running();
            break;
        }
        // fall throu

    default:
        accept = cp >= fixedPosition || event->matches(QKeySequence::Copy);
    }

    if (accept)
        ConsoleEditBase::keyPressEvent(event);

    if (ret) {
        QString t = toPlainText();
        int count = t.count() - fixedPosition;
        QString cmd = t.right(count);
        if (count > 1) {
            QString cmdc = cmd.left(cmd.length() - 1);
            if (!history.contains(cmdc)) {
                history_next = history.count();
                history.append(cmd.left(cmd.length() - 1));
            }
            else
                history_next = history.count() - 1;
        }
        emit inp(cmd);
    }
}

// send text to output
//
void ConsoleEdit::out(QString text) {
    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::End);

    // filter and apply (some) ANSI sequence
    int pos = text.indexOf('\e');
    if (pos >= 0) {
        static QRegExp eseq("\e\\[(?:(3([0-7]);([01])m)|(0m)|(1m;)|1;3([0-7])m|(1m)|(?:3([0-7])m))");
        forever {
            int pos1 = eseq.indexIn(text, pos);
            if (pos1 == -1)
                break;

            QStringList lcap = eseq.capturedTexts();
            Q_ASSERT(lcap.length() == 9); // match captures in eseq, 0 seems unrelated to paren

            // put 'out-of-band' text with current attribute, before changing it
            c.insertText(text.mid(pos, pos1 - pos), tcf);

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
            tcf.setFontWeight(w);
            tcf.setForeground(c);

            pos = pos1 + skip + 2; // add the SCI
        }
        c.insertText(text.mid(pos), tcf);
    }
    else
        c.insertText(text, tcf);

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

// issue an input request
//
void ConsoleEdit::prompt() {
    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::End);
    fixedPosition = c.position();
    ensureCursorVisible();

    if (commands.count() > 0)
        QTimer::singleShot(1, this, SLOT(command_do()));
}

// push command on queue
//
bool ConsoleEdit::command(QString cmd) {
    commands.append(cmd);
    if (commands.count() == 1)
        QTimer::singleShot(1, this, SLOT(command_do()));
    return true;
}

// push command from queue to Prolog processor
//
void ConsoleEdit::command_do() {
    QString cmd = commands.takeFirst();
    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::End);
    c.insertText(cmd);
    emit inp(cmd);
}

// visual hint on available editing, based on cursor position
//
void ConsoleEdit::onCursorPositionChanged()
{
    if (fixedPosition > textCursor().position())
        viewport()->setCursor(Qt::OpenHandCursor);
    else
        viewport()->setCursor(Qt::IBeamCursor);
}
