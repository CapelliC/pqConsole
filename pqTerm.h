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

#ifndef PQTERM_H
#define PQTERM_H

#include "pqConsole_global.h"
#include <QObject>
#include <SWI-cpp.h>

#define X PQCONSOLESHARED_EXPORT

class X pqTerm : public QObject {
    Q_OBJECT
public:
    explicit pqTerm(QObject *parent = 0);
    
signals:
    
public slots:
    
};

class X pqFunctor : public QObject {};
class X pqAtom : public QObject {};
class X pqTermv : public QObject {};

class X pqCompound : public pqTerm {};

class X pqString : public pqTerm {};
class X pqCodeList : public pqTerm {};
class X pqCharList : public pqTerm {};

class X pqException : public pqTerm {};
class X pqTypeError : public pqException {};
class X pqDomainError : public pqException {};
class X pqResourceError : public pqException {};
class X pqTermvDomainError : public pqException {};

#endif // PQTERM_H
