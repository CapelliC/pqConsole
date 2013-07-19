/*  Part of SWI-Prolog

    Author:        Jan Wielemaker
    E-mail:        J.Wielemaker@cs.vu.nl
    WWW:           http://www.swi-prolog.org
    Copyright (C): 2013, VU University Amsterdam

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, if you link this library with other files,
    compiled with a Free Software compiler, to produce an executable, this
    library does not by itself cause the resulting executable to be covered
    by the GNU General Public License. This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

:- module(prolog_console_input,
	  [
	  ]).
:- use_module(library(lists)).
:- use_module(library(apply)).

:- multifile
	prolog:complete_input/3.

%%	prolog:complete_input(+BeforeCursor, +AfterCursor, -Completions) is det.
%
%	True when Completions is a list   of completions. The input line
%	consists of BeforeCursor+AfterCursor, with   the  cursor located
%	between the two strings.

prolog:complete_input(Before, _After, Completions) :-
	string_to_list(Before, Chars),
	reverse(Chars, BeforeRev),
	complete(BeforeRev, Completions).

complete(BeforeRev, Completions) :-	% complete files
	phrase(file_prefix(Prefix), BeforeRev), !,
	atom_concat(Prefix, '*', Pattern),
	expand_file_name(Pattern, Files),
	maplist(atom_concat(Prefix), Completions, Files).
complete(BeforeRev, Completions) :-	% complete atoms
	phrase(atom_prefix(Prefix), BeforeRev), !,
	'$atom_completions'(Prefix, Atoms),
	maplist(atom_concat(Prefix), Completions, Atoms).

%%	atom_prefix(-Prefix) is det.

atom_prefix(Prefix) -->
	atom_chars(RevString),
	{ reverse(RevString, String),
	  string_to_list(Prefix, String) % do not create an atom
	}.

atom_chars([H|T]) --> atom_char(H), !, atom_chars(T).
atom_chars([]) --> [].

atom_char(C) --> [C], { atom_char(C) }.

atom_char(C) :- code_type(C, csym).

%%	file_prefix(-Prefix)// is semidet.
%
%	True when the part before the cursor looks like a file name.

file_prefix(Prefix) -->
	file_chars(RevString), "'",
	{ reverse(RevString, String),
	  atom_codes(Prefix, String)
	}.

file_chars([H|T]) --> file_char(H), !, file_chars(T).
file_chars([]) --> [].

file_char(C) --> [C], { file_char(C) }.

file_char(C) :- code_type(C, csym).
file_char(0'/).
file_char(0'.).
file_char(0'-).
file_char(0'~).
:- if(current_prolog_flag(windows,true)).
file_char(0':).
file_char(0'\s).
:- endif.
