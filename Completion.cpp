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

#include "Completion.h"
#include "PREDICATE.h"
#include "SwiPrologEngine.h"
#include <QDebug>

struct Bin : C { Bin(CCP op, T Left, T Right) : C(op, V(Left, Right)) {} };
struct Uni : C { Uni(CCP op, T arg) : C(op, arg) {} };

struct quv : Bin { quv(T Var, T Expr) : Bin("^", Var, Expr) {} };
struct mod : Bin { mod(T Mod, T Pred) : Bin(":", Mod, Pred) {} };
struct neg : Uni { neg(T pred) : Uni("\\+", pred) {} };
struct join : Bin { join(T Left, T Right) : Bin(",", Left, Right) {} };
struct arith : Bin { arith(T Pred, T Num) : Bin("/", Pred, Num) {} };

#define zero long(0)
#define one long(1)
#define _V T()

#if 0

/** capture predicates
 *  ?- setof(P,M^A^(current_predicate(M:P/A), \+sub_atom(P,0,1,_,$)), L).
 */
void Completion::initialize() {
    initialize(*lpreds);
    qDebug() << "initialize" << lpreds->count();
    QAbstractItemModel *c = model();
    qDebug() << CVP(c);
    setModel(c);
    //model.setStringList(strings);
}

Completion::Completion(QWidget* owner) :
    QCompleter(*(lpreds = new QStringList))
{
    setWidget(owner);
}
Completion::~Completion() { delete lpreds; }

/** simpler usage pattern I found */
void Completion::capture(QTextCursor c) {
    c.select(QTextCursor::WordUnderCursor);
    setCompletionPrefix(c.selectedText());
    popup()->setCurrentIndex(completionModel()->index(0, 0));
}

/** for now, stick to hardcoded */
void Completion::display(QRect cr) {
    cr.setWidth(300);
    complete(cr);
}

#endif

/** issue a query filling the model storage
 *  this will change when I will learn how to call SWI-Prolog completion interface
 */
void Completion::initialize(QStringList &strings) {

    SwiPrologEngine::in_thread _int;
    try {
        T p,m,a,l,v;
        PlQuery q("setof",
            V(p, quv(m,
                     quv(a,
                         join(C("current_predicate", mod(m, arith(p, a))),
                              neg(C("sub_atom", V(p, zero, one, _V, A("$"))))
                              ))),
              l));
        if (q.next_solution())
            for (L x(l); x.next(v); )
                strings.append(CCP(v));
    }
    catch(PlException e) {
        qDebug() << CCP(e);
    }
}
