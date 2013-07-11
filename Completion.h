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

#ifndef COMPLETION_H
#define COMPLETION_H

#include <QCompleter>
#include <QTextCursor>
#include <QAbstractItemView>

/** service class, holds a sorted list of predicates for word completion
 */
class Completion
{
public:

    /** load predicates into strings */
    static void initialize(QStringList &strings);
};

#if 0

/** service class, holds a sorted list of predicates for word completion
 */
class Completion : public QCompleter
{
    Q_OBJECT

public:

    /** default setup */
    Completion(QWidget* owner);

    /** release locals */
    ~Completion();

    /** load predicates to this model */
    void initialize();

    /** load predicates into strings */
    static void initialize(QStringList &strings);

    /** simpler usage pattern I found */
    void capture(QTextCursor c);

    /** for now, stick to hardcoded */
    void display(QRect cr);

signals:
    
public slots:
    
public:

    QStringList *lpreds;
};
#endif

#endif // COMPLETION_H
