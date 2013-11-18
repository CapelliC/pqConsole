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

// by now peek system namespace. Eventually, will move to pqConsole
#define PROLOG_MODULE "system"

#include <SWI-Stream.h>

#include "Swipl_IO.h"
#include "pqConsole.h"
#include "PREDICATE.h"
#include "do_events.h"
#include "ConsoleEdit.h"
#include "Preferences.h"
#include "pqMainWindow.h"
#include "plMiniSyntax.h"

#include <QTime>
#include <QStack>
#include <QDebug>
#include <QMenuBar>
#include <QClipboard>
#include <QFileDialog>
#include <QGridLayout>
#include <QColorDialog>
#include <QFontDialog>
#include <QMessageBox>
#include <QMainWindow>
#include <QApplication>
#include <QFontMetrics>

/** Run a default GUI to demo the ability to embed Prolog with minimal effort.
 *  It will evolve - eventually - from a demo
 *  to the *official* SWI-Prolog console in main distribution - Wow
 */
int pqConsole::runDemo(int argc, char *argv[]) {
    QApplication a(argc, argv);
    pqMainWindow w(argc, argv);
    w.show();
    return a.exec();
}

/** open Prolog script with mini syntax support
 */
int pqConsole::showMiniSyntax(int argc, char *argv[]) {
    QApplication a(argc, argv);
    pqMainWindow w(argc, argv);
    w.show();

    if (argc >= 2) {
        QFile f(argv[1]);
        if (f.open(f.ReadOnly)) {
            QTextEdit *ed = new QTextEdit();
            ed->setText(QTextStream(&f).readAll());
            new plMiniSyntax(ed);
            ed->show();
        }
    }

    return a.exec();
}

#if 0
/** depth first search of widgets hierarchy, from application topLevelWidgets
 */
QWidget *pqConsole::search_widget(std::function<bool(QWidget* w)> match) {
    foreach (auto widget, QApplication::topLevelWidgets()) {
        QStack<QObject*> s;
        s.push(widget);
        while (!s.isEmpty()) {
            auto p = qobject_cast<QWidget*>(s.pop());
            if (match(p))
                return p;
            foreach (auto c, p->children())
                if (c->isWidgetType())
                    s.push(c);
        }
    }
    return 0;
}

/** search widgets hierarchy looking for the first (the only)
 *  that owns the calling thread ID
 */
ConsoleEdit *pqConsole::by_thread() {
    int thid = PL_thread_self();
    return qobject_cast<ConsoleEdit*>(search_widget([=](QWidget* p) {
        if (auto ce = qobject_cast<ConsoleEdit*>(p))
            return ce->match_thread(thid);
        return false;
    }));
}

/** search widgets hierarchy looking for any ConsoleEdit
 */
ConsoleEdit *pqConsole::peek_first() {
    return qobject_cast<ConsoleEdit*>(search_widget([](QWidget* p) {
        return qobject_cast<ConsoleEdit*>(p) != 0;
    }));
}
#endif

QList<ConsoleEdit*> pqConsole::consoles;
QMutex pqConsole::consoles_sync;

void pqConsole::addConsole(ConsoleEdit* c) {
    QMutexLocker l(&consoles_sync);
    Q_ASSERT(!consoles.contains(c));
    consoles.append(c);
}

void pqConsole::removeConsole(ConsoleEdit* c) {
    QMutexLocker l(&consoles_sync);
    Q_ASSERT(consoles.contains(c));
    consoles.removeOne(c);
}

/** search widgets hierarchy looking for the first (the only)
 *  that owns the calling thread ID
 */
ConsoleEdit *pqConsole::by_thread() {
    QMutexLocker l(&consoles_sync);
    int thid = PL_thread_self();
    foreach (ConsoleEdit *ce, consoles)
        if (ce->match_thread(thid))
            return ce;
    return 0;
}

/** search widgets hierarchy looking for any ConsoleEdit
 */
ConsoleEdit *pqConsole::peek_first() {
    QMutexLocker l(&consoles_sync);
    Q_ASSERT(!consoles.isEmpty());
    return consoles[0];
}

/** rendez vous in GUI thread, syncronized
 */
void pqConsole::gui_run(pfunc f) {
    ConsoleEdit::exec_sync s;
    peek_first()->exec_func([&]() {
        f();
        s.go();
    });
    s.stop();
}

/** append new command to history list for current console
 */
PREDICATE(rl_add_history, 1) {
    ConsoleEdit* c = pqConsole::by_thread();
    if (c) {
        WCP line = PL_A1;
        if (*line)
            c->add_history_line(QString::fromWCharArray(line));
        return TRUE;
    }
    return FALSE;
}

/** this should only be used as flag to enable processing ?
 */
PREDICATE(rl_read_init_file, 1) {
    Q_UNUSED(PL_A1);
    return TRUE;
}

/** get history lines for this console
 */
QStringList pqConsole::last_history_lines;

NAMED_PREDICATE("$rl_history", rl_history, 1) {
    /*
    ConsoleEdit* c = pqConsole::by_thread();
    if (c) {
        PlTail lines(PL_A1);
        foreach(QString x, c->history_lines())
            lines.append(W(x));
        lines.close();
        return TRUE;
    }
    */
    PlTail lines(PL_A1);
    foreach(QString x, pqConsole::last_history_lines)
        lines.append(W(x));
    lines.close();
    return TRUE;
}

/** attempt to overcome default tty_size/2
 */
PREDICATE(tty_size, 2) {
    ConsoleEdit* c = pqConsole::by_thread();
    if (c) {
        QSize sz = c->fontMetrics().size(0, "Q");
        long Rows = c->height() / sz.height();
        long Cols = c->width() / sz.width();
        PL_A1 = Rows;
        PL_A2 = Cols;
        return TRUE;
    }
    return FALSE;
}

/** interrupt/0
 *  Ctrl+C
 */
PREDICATE0(interrupt) {
    ConsoleEdit* c = pqConsole::by_thread();
    qDebug() << "interrupt" << CVP(c);
    if (c) {
        c->int_request();
        return TRUE;
    }
    return FALSE;
}

#undef PROLOG_MODULE
#define PROLOG_MODULE "pqConsole"

/** set/get settings of thread associated console
 *  some selected property
 *
 *  pq - updateRefreshRate(N) default 100
 *     - allow to alter default refresh rate (simply count outputs before setting cursor at end)
 *
 *  Qt - maximumBlockCount(N) default 0
 *     - remove (from top) text lines when exceeding the limit
 *
 *  Qt - lineWrapMode(Mode) Mode --> 'NoWrap' | 'WidgetWidth'
 *     - when NoWrap, an horizontal scroll bar could display
 *
 *  Qt - tabStopWidth(W)
 *     - change spacing (character should be monospace, fixed pitch)
 */
PREDICATE(console_settings, 1) {
    ConsoleEdit* c = pqConsole::by_thread();
    if (c) {
        PlFrame fr;
        PlTerm opt;
        for (PlTail opts(PL_A1); opts.next(opt); ) {
            if (opt.arity() == 1)
                pqConsole::unify(opt.name(), c, opt[1]);
            else
                throw PlException(A(c->tr("%1: properties have arity 1").arg(t2w(opt))));
        }
        return TRUE;
    }
    return FALSE;
}

/** getOpenFileName(+Title, ?StartPath, +Pattern, -Choice)
 *  run a modal dialog on request from foreign thread
 *  this must run a modal loop in GUI thread
 */
PREDICATE(getOpenFileName, 4) {
    ConsoleEdit* c = pqConsole::by_thread();
    if (c) {
        QString Caption = t2w(PL_A1), StartPath, Pattern = t2w(PL_A3), Choice;
        if (PL_A2.type() == PL_ATOM)
            StartPath = t2w(PL_A2);

        ConsoleEdit::exec_sync s;

        c->exec_func([&]() {
            Choice = QFileDialog::getOpenFileName(c, Caption, StartPath, Pattern);
            s.go();
        });
        s.stop();

        if (!Choice.isEmpty()) {
            PL_A4 = A(Choice);
            return TRUE;
        }
    }
    return FALSE;
}

/** getSaveFileName(+Title, ?StartPath, +Pattern, -Choice)
 *  run a modal dialog on request from foreign thread
 *  this must run a modal loop in GUI thread
 */
PREDICATE(getSaveFileName, 4) {
    ConsoleEdit* c = pqConsole::by_thread();
    if (c) {
        QString Caption = t2w(PL_A1), StartPath, Pattern = t2w(PL_A3), Choice;
        if (PL_A2.type() == PL_ATOM)
            StartPath = t2w(PL_A2);

        ConsoleEdit::exec_sync s;
        c->exec_func([&]() {
            Choice = QFileDialog::getSaveFileName(c, Caption, StartPath, Pattern);
            s.go();
        });
        s.stop();

        if (!Choice.isEmpty()) {
            PL_A4 = A(Choice);
            return TRUE;
        }
    }
    return FALSE;
}

/** select_font
 *  run Qt font selection
 */
PREDICATE0(select_font) {
    ConsoleEdit* c = pqConsole::by_thread();
    bool ok = false;
    if (c) {
        ConsoleEdit::exec_sync s;
        c->exec_func([&]() {
            Preferences p;
            QFont font = QFontDialog::getFont(&ok, p.console_font, c);
            if (ok) {
                c->setFont(p.console_font = font);
                p.save();
            }
            s.go();
        });
        s.stop();
    }
    return ok;
}

/** select_ANSI_term_colors
 *  run a dialog to let user configure console colors (associate user defined color to indexes 1-16)
 */
PREDICATE0(select_ANSI_term_colors) {
    ConsoleEdit* c = pqConsole::by_thread();
    bool ok = false;
    if (c) {
        ConsoleEdit::exec_sync s;
        c->exec_func([&]() {
            Preferences p;
            QColorDialog d(c);
            Q_ASSERT(d.customCount() >= p.ANSI_sequences.size());
            for (int i = 0; i < p.ANSI_sequences.size(); ++i)
                d.setCustomColor(i, p.ANSI_sequences[i].rgb());
            if (d.exec()) {
                for (int i = 0; i < p.ANSI_sequences.size(); ++i)
                    p.ANSI_sequences[i] = d.customColor(i);
                c->repaint();
                ok = true;
                p.save();
            }
            s.go();
        });
        s.stop();
        return ok;
    }
    return FALSE;
}

/** quit_console
 *  just issue termination to Qt application object
 */
PREDICATE0(quit_console) {
    ConsoleEdit* c = pqConsole::by_thread();
    if (c) {
        // run on foreground
        c->exec_func([]() { QApplication::postEvent(qApp, new QCloseEvent); });
        return TRUE;
    }
    return FALSE;
}

/** issue a copy to clipboard of current selection
 */
PREDICATE0(copy) {
    ConsoleEdit* c = pqConsole::by_thread();
    if (c) {
        c->exec_func([=](){
            QApplication::clipboard()->setText(c->textCursor().selectedText());
            do_events();
        });
        return TRUE;
    }
    return FALSE;
}

/** issue a paste to clipboard of current selection
 */
PREDICATE0(paste) {
    ConsoleEdit* c = pqConsole::by_thread();
    if (c) {
        c->exec_func([=](){
            c->textCursor().insertText(QApplication::clipboard()->text());
            do_events();
        });
        return TRUE;
    }
    return FALSE;
}
