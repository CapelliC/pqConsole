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

// SWIPL-WIN.EXE interface implementation

/** window_title(-Old, +New)
 *  get/set console title
 */
PREDICATE(window_title, 2) {
    if (ConsoleEdit* c = pqConsole::by_thread()) {
        QWidget *w = c->parentWidget();
        if (qobject_cast<QMainWindow*>(w)) {
            PL_A1 = A(w->windowTitle());
            w->setWindowTitle(t2w(PL_A2));
            return TRUE;
        }
    }
    return FALSE;
}

/** win_window_pos(Options)
 *  Option:
 *     size(W, H)
 *     position(X, Y)
 *     zorder(ZOrder)
 *     show(Bool)
 *     activate
 */
PREDICATE(win_window_pos, 1) {
    ConsoleEdit* c = pqConsole::by_thread();
    if (!c)
        return FALSE;

    QWidget *w = c->parentWidget();
    if (!w)
        return FALSE;

    T opt;
    L options(PL_A1);
    typedef QPair<int, QString> O;
    while (options.next(opt)) {
        O o = O(opt.arity(), opt.name());
        if (o == O(2, "size")) {
            long W = opt[1], H = opt[2];
            QSize sz = c->fontMetrics().size(0, "Q");
            w->resize(sz.width() * W, sz.height() * H);
            continue;
        }
        if (o == O(2, "position")) {
            long X = opt[1], Y = opt[2];
            w->move(X, Y);
            continue;
        }
        if (o == O(1, "zorder")) {
            // TBD ...
            // long ZOrder = opt[1];
            continue;
        }
        if (o == O(1, "show")) {
            bool y = QString(opt[1].name()) == "true";
            if (y)
                w->show();
            else
                w->hide();
            continue;
        }
        if (o == O(0, "activate")) {
            w->activateWindow();
            continue;
        }

        // print_error
        return FALSE;
    }

    return TRUE;
}

/** win_has_menu
 *  true =only= when ConsoleEdit is directly framed inside a QMainWindow
 */
PREDICATE0(win_has_menu) {
    auto ce = pqConsole::by_thread();
    return ce && qobject_cast<QMainWindow*>(ce->parentWidget()) ? TRUE : FALSE;
}

/** MENU interface
 *  helper to lookup position and issue action creation
 */
/** win_insert_menu(+Label, +Before)
 *  do action construction
 */
PREDICATE(win_insert_menu, 2) {
    if (ConsoleEdit *ce = pqConsole::by_thread()) {
        QString Label = t2w(PL_A1), Before = t2w(PL_A2);
        ce->exec_func([=]() {
            if (auto mw = qobject_cast<QMainWindow*>(ce->parentWidget())) {
                auto mbar = mw->menuBar();
                foreach (QAction *ac, mbar->actions())
                    if (ac->text() == Label)
                        return;
                foreach (QAction *ac, mbar->actions())
                    if (ac->text() == Before) {
                        mbar->insertMenu(ac, new QMenu(Label));
                        return;
                    }
                if (Before == "-") {
                    mbar->addMenu(Label);
                    return;
                }
            }
            qDebug() << "failed win_insert_menu" << Label << Before;
        });
        return TRUE;
    }
    return FALSE;
}

/** win_insert_menu_item(+Pulldown, +Label, +Before, :Goal)
 *  does search insertion position and create menu item
 */
PREDICATE(win_insert_menu_item, 4) {
    if (ConsoleEdit *ce = pqConsole::by_thread()) {
        QString Pulldown = t2w(PL_A1), Label, Before = t2w(PL_A3), Goal;
        QList<QPair<QString, QString>> lab_act;

        if (PL_A2.arity() == 2 /* &&
            strcmp(PL_A2.name(), "/") == 0 &&
            PL_A2[2].type() == PL_LIST &&
            PL_A4.type() == PL_LIST */ )
        {
            Label = t2w(PL_A2[1]);
            PlTail labels(PL_A2[2]), actions(PL_A4);
            PlTerm label, action;
            while (labels.next(label) && actions.next(action))
                lab_act.append(qMakePair(t2w(label), t2w(action)));
        }
        else {
            Label = t2w(PL_A2);
            Goal = t2w(PL_A4);
        }

        QString ctxtmod = t2w(PlAtom(PL_module_name(PL_context())));
        // if (PlCall("context_module", cx)) ctxtmod = t2w(cx); -- same as above: system
        ctxtmod = "win_menu";

        ce->exec_func([=]() {
            if (auto mw = qobject_cast<pqMainWindow*>(ce->parentWidget())) {
                foreach (QAction *ac, mw->menuBar()->actions())
                    if (ac->text() == Pulldown) {
                        QMenu *mn = ac->menu();
                        if (!lab_act.isEmpty()) {
                            foreach (QAction *cm, mn->actions())
                                if (cm->text() == Label) {
                                    cm->setMenu(new QMenu(Label));
                                    foreach (auto p, lab_act)
                                        mw->addActionPq(ce, cm->menu(), p.first, p.second);
                                    return;
                                }
                            return;
                        }
                        else {
                            if (Label != "--")
                                foreach (QAction *bc, mn->actions())
                                    if (bc->text() == Label) {
                                        bc->setToolTip(Goal);
                                        return;
                                    }
                            if (Before == "-") {
                                if (Label == "--")
                                    mn->addSeparator();
                                else
                                    mw->add_action(ce, mn, Label, ctxtmod, Goal);
                                return;
                            }
                            foreach (QAction *bc, mn->actions())
                                if (bc->text() == Before) {
                                    if (Label == "--")
                                        mn->insertSeparator(bc);
                                    else
                                        mw->add_action(ce, mn, Label, ctxtmod, Goal, bc);
                                    return;
                                }

                            QAction *bc = mw->add_action(ce, mn, Before, ctxtmod, "");
                            mw->add_action(ce, mn, Label, ctxtmod, Goal, bc);
                        }
                    }
            }
        });
        return TRUE;
    }
    return FALSE;
}

/** tty_clear
 *  as requested by Annie. Should as well be implemented capturing ANSI terminal sequence
 */
PREDICATE0(tty_clear) {
    if (ConsoleEdit* c = pqConsole::by_thread()) {
        pqConsole::gui_run([&]() { c->tty_clear(); });
        return TRUE;
    }
    return FALSE;
}

/** win_open_console(Title, In, Out, Err, [ registry_key(Key) ])
 *  code stolen - verbatim - from pl-ntmain.c
 *  registry_key(Key) unused by now
 */
PREDICATE(win_open_console, 5) {
    ConsoleEdit *ce = pqConsole::peek_first();
    if (!ce)
        throw PlException(A("no ConsoleEdit available"));

    static IOFUNCTIONS rlc_functions = {
        Swipl_IO::_read_f,
        Swipl_IO::_write_f,
        Swipl_IO::_seek_f,
        Swipl_IO::_close_f,
        Swipl_IO::_control_f,
        Swipl_IO::_seek64_f
    };

    #define STREAM_COMMON (\
        SIO_TEXT|       /* text-stream */           \
        SIO_NOCLOSE|    /* do no close on abort */	\
        SIO_ISATTY|     /* terminal */              \
        SIO_NOFEOF)     /* reset on end-of-file */

    auto c = new Swipl_IO;
    IOSTREAM
        *in  = Snew(c,  SIO_INPUT|SIO_LBUF|STREAM_COMMON, &rlc_functions),
        *out = Snew(c, SIO_OUTPUT|SIO_LBUF|STREAM_COMMON, &rlc_functions),
        *err = Snew(c, SIO_OUTPUT|SIO_NBUF|STREAM_COMMON, &rlc_functions);

    in->position  = &in->posbuf;		/* record position on same stream */
    out->position = &in->posbuf;
    err->position = &in->posbuf;

    in->encoding  = ENC_UTF8;
    out->encoding = ENC_UTF8;
    err->encoding = ENC_UTF8;

    ce->new_console(c, t2w(PL_A1));

    if (!PL_unify_stream(PL_A2, in) ||
        !PL_unify_stream(PL_A3, out) ||
        !PL_unify_stream(PL_A4, err)) {
            Sclose(in);
            Sclose(out);
            Sclose(err);
        return FALSE;
    }

    return TRUE;
}

/** display modal message box
 *  win_message_box(+Text, +Options)
 *
 *  Options is list of name(Value). Currently only
 *   image - an image file name (can be resource based)
 *   title - the message box title
 *   icon  - identifier among predefined Qt message box icons
 *   image - pixmap file (ok resource)
 *   image_scale - multiplier to scale image
 */
PREDICATE(win_message_box, 2) {
    if (ConsoleEdit* c = pqConsole::by_thread()) {
        QString Text = t2w(PL_A1);

        QString Title = "swipl-win", Image;
        PlTerm Icon; //QMessageBox::Icon Icon = QMessageBox::NoIcon;

        // scan options
        float scale = 0;
        PlTerm Option;
        int min_width = 0;

        for (PlTail t(PL_A2); t.next(Option); )
            if (Option.arity() == 1) {
                QString name = Option.name();
                if (name == "title")
                    Title = t2w(Option[1]);
                if (name == "icon")
                    Icon = Option[1];
                if (name == "image")
                    Image = t2w(Option[1]);
                if (name == "image_scale")
                    scale = double(Option[1]);
                if (name == "min_width")
                    min_width = int(Option[1]);
            }
            else
                throw PlException(A(c->tr("option %1 : invalid arity").arg(t2w(Option))));

        int rc;
        QString err;
        ConsoleEdit::exec_sync s;

        c->exec_func([&]() {

            QMessageBox mbox(c);

            // get icon file, if required
            QPixmap imfile;
            if (!Image.isEmpty()) {
                if (!imfile.load(Image)) {
                    err = c->tr("icon file %1 not found").arg(Image);
                    return;
                }
                if (scale)
                    imfile = imfile.scaled(imfile.size() * scale,
                                           Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }

            mbox.setText(Text);
            mbox.setWindowTitle(Title);
            if (!imfile.isNull())
                mbox.setIconPixmap(imfile);

            if (min_width) {
                auto horizontalSpacer = new QSpacerItem(min_width, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
                auto layout = qobject_cast<QGridLayout*>(mbox.layout());
                layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
            }

            rc = mbox.exec() == mbox.Ok;
            s.go();
        });
        s.stop();

        if (!err.isEmpty())
            throw PlException(A(err));

        return rc;
    }

    return FALSE;
}

/** win_preference_groups(-Groups:list)
 */
PREDICATE(win_preference_groups, 1) {
    Preferences p;
    PlTail l(PL_A1);
    foreach (auto g, p.childGroups())
        l.append(A(g));
    l.close();
    return TRUE;
}

/** win_preference_keys(+Group, -Keys:list)
 */
PREDICATE(win_preference_keys, 2) {
    Preferences p;
    PlTail l(PL_A1);
    foreach (auto k, p.childKeys())
        l.append(A(k));
    l.close();
    return TRUE;
}

/** win_current_preference(+Group, +Key, -Value)
 */
PREDICATE(win_current_preference, 3) {
    Preferences p;

    auto g = t2w(PL_A1),
         k = t2w(PL_A2);

    p.beginGroup(g);
    if (p.contains(k)) {
        auto x = p.value(k).toString();
        return PL_A3 = PlCompound(x.toStdWString().data());
    }

    return FALSE;
}

/** win_set_preference(+Group, +Key, +Value)
 */
PREDICATE(win_set_preference, 3) {
    Preferences p;

    auto g = t2w(PL_A1),
         k = t2w(PL_A2);

    p.beginGroup(g);
    p.setValue(k, serialize(PL_A3));
    p.save();

    return TRUE;
}

/** output html at prompt
 */
PREDICATE(win_html_write, 1) {
    if (ConsoleEdit* c = pqConsole::by_thread()) {
        // run on foreground
        if (PL_A1.type() == PL_ATOM) {
            QString html = t2w(PL_A1);
            ConsoleEdit::exec_sync s;
            c->exec_func([&]() {
                c->html_write(html);
                s.go();
            });
            s.stop();
        }
        return TRUE;
    }
    return FALSE;
}
