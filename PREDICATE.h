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

#ifndef PREDICATE_H
#define PREDICATE_H

#include <SWI-cpp.h>

/** avoid 2 warnings ( -Wunused-parameter ) for each builtin */
#ifndef NO_REDEFINE_PREDICATE_NAME_ARITY

#undef PREDICATE
#define PREDICATE(name, arity) \
    static foreign_t \
    pl_ ## name ## __ ## arity(PlTermv _av); \
    static foreign_t \
    _pl_ ## name ## __ ## arity(term_t t0, int, control_t) \
    { try \
      { \
        return pl_ ## name ## __ ## arity(PlTermv(arity, t0)); \
      } catch ( PlException &ex ) \
      { return ex.plThrow(); \
      } \
    } \
    static PlRegister _x ## name ## __ ## arity(PROLOG_MODULE, #name, arity, \
                        _pl_ ## name ## __ ## arity); \
    static foreign_t pl_ ## name ## __ ## arity(PlTermv _av)

#define NAMED_PREDICATE(plname, name, arity) \
    static foreign_t \
    pl_ ## name ## __ ## arity(PlTermv _av); \
    static foreign_t \
    _pl_ ## name ## __ ## arity(term_t t0, int, control_t) \
    { try \
      { \
        return pl_ ## name ## __ ## arity(PlTermv(arity, t0)); \
      } catch ( PlException &ex ) \
      { return ex.plThrow(); \
      } \
    } \
    static PlRegister _x ## name ## __ ## arity(PROLOG_MODULE, #plname, arity, \
                        _pl_ ## name ## __ ## arity); \
    static foreign_t pl_ ## name ## __ ## arity(PlTermv _av)

#endif

#ifndef NO_SHORTEN_INTERFACE

/** shorten interface */

typedef const char* CCP;
typedef const wchar_t* WCP;
typedef const void* CVP;
#define CT QThread::currentThread()

#include <QString>

inline CCP S(const PlTerm &T) { return T; }

inline PlAtom A(QString s) { QByteArray a = s.toUtf8(); return PlAtom(a.constData()); }
inline PlAtom W(QString s) {
    wchar_t *w = new wchar_t[s.length() + 1];
    w[s.toWCharArray(w)] = 0;
    PlAtom a(w);
    delete w;
    return a;
}

typedef PlTerm T;
typedef PlTermv V;
typedef PlCompound C;
typedef PlTail L;

/** get back an object passed by pointer to Prolog */
template<typename Obj> Obj* pq_cast(T ptr) { return static_cast<Obj*>(static_cast<void*>(ptr)); }

#endif

#endif // PREDICATE_H
