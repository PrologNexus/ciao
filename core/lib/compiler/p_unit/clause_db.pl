:- module(clause_db,
    [ clause_read/7, 
      prop_clause_read/7,
      add_prop_clause_read/7,
      clause_locator/2, 
      maybe_clause_locator/2, 
      add_clause_locator/2,
      literal_locator/2, 
      source_clause/3, 
      cleanup_clause_db/0,
      cleanup_clause_db_code/0,
      load_lib_props/1,
      gen_lib_props/1,
      cleanup_lib_props/0
    ], [assertions, basicmodes, regtypes, datafacts]).

:- use_module(library(compiler/p_unit/program_keys), [clkey/1, clid_of_atomid/2]).
:- use_module(library(messages)).

%% ---------------------------------------------------------------------------
:- doc(bug,"1. There are invalid clause-keys in calls to clause_locator.
    E.g., from entries or exports during analysis.").
:- doc(bug,"2. We should get rid of dummy clause locators.").
%% ---------------------------------------------------------------------------

:- doc(cleanup_clause_db,"Cleans up the database.").
cleanup_clause_db:-
    retractall_fact(pgm_prop_clause_read(_,_,_,_,_,_,_)),
    cleanup_clause_db_code.

cleanup_clause_db_code:-
    retractall_fact(clause_read(_,_,_,_,_,_,_)),
%       retractall_fact(pgm_prop_clause_read(_,_,_,_,_,_,_)),
    retractall_fact(source_clause(_,_,_)),
    retractall_fact(locator(_,_)).

%% ---------------------------------------------------------------------------
:- doc(clause_read(M, Head, Body, VarNames, Source, LB, LE),
   "Each fact is a clause of module @var{M}.
    The clause is @var{Head:-Body} (if a directive, @var{Head} is a number,
    see @pred{c_itf:clause_of/7}). @var{VarNames} contains the names of the 
    variables of the clause. @var{Source} is the file in which the
    clause appears (treats included files correctly). @var{LB} and
    @var{LE} are the first and last line numbers in this source file in
    which the clause appears (if the source is not available or has
    not been read @var{LB}=@var{LE}=0). @var{VarNames} is not a variable, 
    and @var{Head:-Body} is fully expanded, including module names.").

:- data clause_read/7.

:- doc(prop_clause_read/7,"Same as @tt{clause_read/7} but for the
   properties not in the current module.").
prop_clause_read(M, Head, Body, VarNames, Source, LB, LE):-
    pgm_prop_clause_read(M, Head, Body, VarNames, Source, LB, LE).
prop_clause_read(M, Head, Body, VarNames, Source, LB, LE):-
    lib_prop_clause_read(M, Head, Body, VarNames, Source, LB, LE).

:- data pgm_prop_clause_read/7.
:- data lib_prop_clause_read/7.

:- pred add_prop_clause_read(M, Head, Body, VarNames, Source, LB, LE)
    # "Adds an entry for a property located in a user module (but not the
    current module).".
add_prop_clause_read(M, Head, Body, VarNames, Source, LB, LE):-
    assertz_fact(pgm_prop_clause_read(M, Head, Body, VarNames, Source, LB, LE)).

:- doc(source_clause(Key,Clause,Dict),"The current module has @var{Clause}
   identified by @var{Key} with variable names @var{Dict}.").

:- data source_clause/3.

%% ---------------------------------------------------------------------------

:- data locator/2.

:- pred clause_locator(ClKey,L) :: atm * location_t
    # "The (current module) clause identified by @var{ClKey} is located in the
      source file around @var{L}.".
clause_locator(ClKey,L) :- locator(ClKey,L).

:- pred maybe_clause_locator(ClKey,L) : atm(ClKey) => location_t(L)
# "The (current module) clause with identifier @var{ClKey} either appears
   in the source file and has locator @var{L}, or was generated by
   ciaopp (e.g., during transform(vers)) from some other clause. In
   the latter case it is assigned the locator of that clause (as it
   has no own locator in the source). This functionality is useful on
   file output after source transformations, assertion checking,
   etc.".
maybe_clause_locator(ClKey,L) :-
    find_lines_in_orig_prog(ClKey,L).

%% ---------------------------------------------------------------------------
% TODO: COPIED FROM fixpo_ops.pl
%   move program_keys:orig_clause_id/2 to this module to have all locator
%   processting code together?

:- use_module(library(compiler/p_unit/program_keys), [orig_clause_id/2]).

%   try to extract locator from original

find_lines_in_orig_prog(ClId,Loc):-
    clause_locator(ClId,Loc),!.
find_lines_in_orig_prog(ClId,Loc):-
    orig_clause_id(ClId,Orig_ClId),
    clause_locator(Orig_ClId,Loc).

:- pred add_clause_locator/2 : clkey * location_t.
add_clause_locator(ClKey, L) :-
    ( locator(ClKey,L) -> true
    ; asserta_fact(locator(ClKey, L))
    ).

:- pred literal_locator/2 : clkey * var => clkey * location_t.
literal_locator(LitKey,L):-
    clid_of_atomid(LitKey,ClKey),
    maybe_clause_locator(ClKey, L). % TODO: are we doing backtracking here?

%--------------------------------------------------------------------------

:- use_module(engine(io_basic)).
:- use_module(library(write), [writeq/2]).
:- use_module(library(read), [read/2]).

:- pred cleanup_lib_props
    # "Cleans up all facts of lib_prop_clause_read/7 predicate.".
cleanup_lib_props:-
    retractall_fact(lib_prop_clause_read(_,_,_,_,_,_,_)).

% TODO: [IG] generic reader
:- pred load_lib_props(Stream)
    # "Loads the facts for lib_prop_clause_read/7 from stream @var{Stream}.".
load_lib_props(Stream):-
    repeat,
    read(Stream,Fact),
    ( Fact = end_of_file ->
        true
    ;
        assertz_fact(Fact),
        fail
    ).

dump_lib_props_data(Data) :-
    prop_clause_read(M, Head, Body, VarNames, Source, LB, LE),
    Data = lib_prop_clause_read(M, Head, Body, VarNames, Source, LB, LE).

:- pred gen_lib_props(Stream)
    # "Saves the facts for lib_prop_clause_read/7 to stream @var{Stream}
    from pgm_prop_clause_read/7.".
gen_lib_props(Stream):-
    dump_lib_props_data(Data),
    writeq(Stream,Data),display(Stream,'.'),nl(Stream),
    fail.
gen_lib_props(_).
