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

#include "pqMainWindow.h"
#include "ConsoleEdit.h"
#include <QDebug>
#include <QTimer>
#include "PREDICATE.h"

pqMainWindow::pqMainWindow(QWidget *parent) :
    QMainWindow(parent)
{
}

/** this is the mandatory constructor to get SWI-prolog embedding
 *  and proper XPCE termination
 */
pqMainWindow::pqMainWindow(int argc, char *argv[]) {
    setCentralWidget(new ConsoleEdit(argc, argv));
    resize(800, 600);
}

/** handle application closing, WRT XPCE termination
 */
void pqMainWindow::closeEvent(QCloseEvent *event) {
    if (!console()->can_close())
        event->ignore();
}

/** pass initialization script to actual interface
 */
void pqMainWindow::set_script(QString name, QString text) {
    console()->engine()->script_run(name, text);
}

/** get access to the widget
 */
ConsoleEdit *pqMainWindow::console(int thread) const {

    if (!consoles()) {
        // don't search
        auto c = qobject_cast<ConsoleEdit*>(centralWidget());
        return c->match_thread(thread) ? c : 0;
    }

    for (int i = 0; consoles()->count(); ++i) {
        auto c = qobject_cast<ConsoleEdit*>(consoles()->widget(i));
        if (c->match_thread(thread))
            return c;
    }

    return 0;
}

/** qualify widget type
 */
QTabWidget *pqMainWindow::consoles() const {
    return qobject_cast<QTabWidget*>(centralWidget());
}

/** switch the interface to tabbed on creation of second (and subsequent) console
 */
void pqMainWindow::addConsole(ConsoleEdit *console, QString title) {
    auto t = consoles();
    if (!t) {
        auto c = qobject_cast<ConsoleEdit*>(centralWidget());
        t = new QTabWidget;
        t->setTabsClosable(true);
        QString T = windowTitle();
        if (T.isEmpty())
            T = "swipl";
        t->addTab(c, T);
        setCentralWidget(t);
        connect(t, SIGNAL(tabCloseRequested(int)), this, SLOT(tabCloseRequested(int)));
    }
    t->addTab(console, title);
}

/** handle the close button, issuing console request and removing from tab
 */
void pqMainWindow::tabCloseRequested(int tabId) {
    auto c = qobject_cast<ConsoleEdit*>(consoles()->widget(tabId));
    if (c->can_close())
        consoles()->removeTab(tabId);
}
