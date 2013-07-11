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
ConsoleEdit *pqMainWindow::console() const {
    return qobject_cast<ConsoleEdit*>(centralWidget());
}
