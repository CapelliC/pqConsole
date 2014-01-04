/*  File         : trace_interception.pl
    Purpose      : predicate to help locating sources while debugging

    pqConsole    : interfacing SWI-Prolog and Qt

    Author       : Carlo Capelli
    E-mail       : cc.carlo.cap@gmail.com
    Copyright (C): 2013,2014 Carlo Capelli

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

:- module(trace_interception, [goal_source_position/5]).

%%  prolog_trace_interception(+Port, +Frame, +Choice, -Action)
%
%   see http://www.swi-prolog.org/pldoc/doc_for?object=prolog_trace_interception/4
%
prolog_trace_interception(Port, Frame, Choice, Action) :-
        current_prolog_flag(pq_tracer, true), !,
        pq_trace_interception(Port, Frame, Choice, Action).

%%  goal_source_position(+Port, +Frame, -Clause, -File, -Position) is det
%
%   access internal frame/clause information to get the
%   source characters position
%
goal_source_position(_Port, Frame, Clause, File, A-Z) :-
        prolog_frame_attribute(Frame, hidden, false),
        prolog_frame_attribute(Frame, parent, Parent),
        prolog_frame_attribute(Frame, pc, Pc),
        prolog_frame_attribute(Parent, clause, Clause),
        clause_info(Clause, File, TermPos, _VarOffsets),
        locate_vm(Clause, 0, Pc, Pc1, VM),
        '$clause_term_position'(Clause, Pc1, TermPos1),
        ( VM = i_depart(_) -> append(TermPos2, [_], TermPos1) ; TermPos2 = TermPos1 ),
        range(TermPos2, TermPos, A, Z).

locate_vm(Clause, X, Pc, Pc1, VM) :-
	'$fetch_vm'(Clause, X, Y, T),
	(   X < Pc
	->  locate_vm(Clause, Y, Pc, Pc1, VM)
        ;   Pc1 = X, VM = T
	).

range([], Pos, A, Z) :-
	arg(1, Pos, A),
	arg(2, Pos, Z).
range([H|T], term_position(_, _, _, _, PosL), A, Z) :-
	nth1(H, PosL, Pos),
	range(T, Pos, A, Z).
