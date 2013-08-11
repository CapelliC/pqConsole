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

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QFont>
#include <QColor>
#include <QSettings>
#include <QTextCharFormat>
#include "ConsoleEdit.h"

/** some configurable user preference
 */
class Preferences : public QSettings
{
    Q_OBJECT
public:

    explicit Preferences(QObject *parent = 0);
    ~Preferences();

    QFont console_font;
    QColor console_output_fmt;
    QColor console_input_fmt;

    ConsoleEditBase::LineWrapMode wrapMode;

    /** helper to save/restore windows placements */
    void loadGeometry(QString key, QWidget *w);
    void saveGeometry(QString key, QWidget *w);

#if 0
    // TBD this is performance critical
    Qt::ConnectionType user_output_conntype;

    QColor console_background;
    QSize w_size;
    QPoint w_position;
    bool top_level_consoles;
#endif

signals:
    
public slots:
    
};

#endif // PREFERENCES_H
