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

Preferences::Preferences(QObject *parent) :
    QSettings("SWI-Prolog", "pqConsole", parent)
{
    //user_output_conntype = static_cast<Qt::ConnectionType>(value("user_output_conntype", Qt::BlockingQueuedConnection).toInt());

    console_font = value("console_font", QFont("courier", 12)).value<QFont>();
    wrapMode = static_cast<ConsoleEditBase::LineWrapMode>(value("wrapMode", ConsoleEditBase::WidgetWidth).toInt());

    console_output_fmt = value("console_output_fmt", "black").value<QColor>();
    console_input_fmt = value("console_input_fmt", "beige").value<QColor>();
}
Preferences::~Preferences() {
    //setValue("user_output_conntype", user_output_conntype);

    setValue("console_font", console_font);
    setValue("wrapMode", wrapMode);

    setValue("console_output_fmt", console_output_fmt);
    setValue("console_input_fmt", console_input_fmt);
}
