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

#include "Preferences.h"

/** get configured values, with reasonable defaults
 */
Preferences::Preferences(QObject *parent) :
    QSettings("SWI-Prolog", "pqConsole", parent)
{
    console_font = value("console_font", QFont("courier", 12)).value<QFont>();
    wrapMode = static_cast<ConsoleEditBase::LineWrapMode>(value("wrapMode", ConsoleEditBase::WidgetWidth).toInt());

    console_output_fmt = value("console_output_fmt", "black").value<QColor>();
    console_input_fmt = value("console_input_fmt", "beige").value<QColor>();
}

/** save configured values
 */
Preferences::~Preferences() {

    #define SV(s) setValue(#s, s)

    SV(console_font);
    SV(wrapMode);

    SV(console_output_fmt);
    SV(console_input_fmt);

    #undef SV
}

void Preferences::loadGeometry(QString key, QWidget *w) {
    beginGroup(key);
    QPoint pos = value("pos", QPoint(200, 200)).toPoint();
    QSize size = value("size", QSize(800, 600)).toSize();
    int state = value("state", static_cast<int>(Qt::WindowNoState)).toInt();
    w->move(pos);
    w->resize(size);
    w->setWindowState(static_cast<Qt::WindowStates>(state));
    endGroup();
}

void Preferences::saveGeometry(QString key, QWidget *w) {
    beginGroup(key);
    setValue("pos", w->pos());
    setValue("size", w->size());
    setValue("state", static_cast<int>(w->windowState()));
    endGroup();
}
