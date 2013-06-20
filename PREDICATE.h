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

#ifndef NO_REDEFINE_PREDICATE_NAME_ARITY

// avoid 2 warnings ( -Wunused-parameter ) for each builtin
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

#endif

#ifndef NO_SHORTEN_INTERFACE

// shorten interface
typedef const char* CCP;

#include <QString>
inline CCP ccp(QString s) { return s.toUtf8().constData(); }

inline CCP S(const PlTerm &T) { return T; }
typedef PlTerm T;
typedef PlTermv V;
typedef PlAtom A;
typedef PlCompound C;

// get back an object passed by pointer to Prolog
template<typename Obj> Obj* pq_cast(T ptr) { return static_cast<Obj*>(static_cast<void*>(ptr)); }

#endif

#endif // PREDICATE_H
