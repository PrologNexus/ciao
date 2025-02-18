:- module(datafacts_rt, [
    asserta_fact/1, asserta_fact/2, assertz_fact/1, assertz_fact/2,
    current_fact/1, current_fact/2, retract_fact/1, retractall_fact/1,
    current_fact_nb/1, retract_fact_nb/1,
    close_predicate/1, open_predicate/1,
    set_fact/1, erase/1
], [noprelude, assertions, nortchecks, isomodes]).

:- doc(title,"Fast/concurrent update of facts (runtime)").
:- doc(author,"The Ciao Development Team").
:- doc(author,"Daniel Cabeza").
:- doc(author,"Manuel Carro").

:- doc(usage, "Do not use this module directly (use the @lib{datafacts}
   package instead).").

:- doc(module, "This module implements the assert/retract family of
   predicates to manipulate data predicates (facts).").

:- use_module(engine(basiccontrol)).
:- use_module(engine(term_basic)).
:- use_module(engine(term_typing)).
%
:- if(defined(optim_comp)).
:- use_module(engine(internals), [
    '$asserta'/2,
    '$asserta_ref'/3,
    '$assertz'/2,
    '$assertz_ref'/3,
    '$erase_ref'/1,
    '$erase'/2,
    '$erase_nb'/2,
    '$current'/2,
    '$current_nb'/2,
    '$current_nb_ref'/3,
    '$current_clauses'/2,
    '$close_pred'/1,
    '$open_pred'/1]).
:- else.
:- use_module(engine(internals), [
    term_to_meta/2, % TODO: use primitive meta term in set_fact/1?
    '$compile_term'/2,
    '$current_clauses'/2,
    '$inserta'/2,
    '$insertz'/2,
    '$ptr_ref'/2,
    '$current_instance'/5,
    '$instance'/3,
    '$erase'/1,
    '$close_predicate'/1,
    '$open_predicate'/1,
    '$unlock_predicate'/1]).
:- endif.

:- if(defined(optim_comp)).
% TODO: optim_comp cannot document predicates not defined in this module
:- else.
:- doc(doinclude,data/1).
:- decl data(Predicates) : sequence_or_list(predname)
    # "Defines each predicate in @var{Predicates} as a @concept{data
      predicate}.  If a predicate is defined data in a file, it must
      be defined data in every file containing clauses for that
      predicate. The directive should precede all clauses of the
      affected predicates.  This directive is defined as a prefix
      operator in the compiler.".
:- impl_defined(data/1).

:- doc(doinclude,concurrent/1).
:- decl concurrent(Predicates) : sequence_or_list(predname)
    # "Defines each predicate in @var{Predicates} as a
      @concept{concurrent predicate}.  If a predicate is defined
      concurrent in a file, it must be defined concurrent in every
      file containing clauses for that predicate. The directive
      should precede all clauses of the affected predicates.  This
      directive is defined as a prefix operator in the compiler.".
:- impl_defined(concurrent/1).
:- endif.

:- doc(asserta_fact(Fact), "@var{Fact} is added to the corresponding
   @concept{data predicate}.  The fact becomes the first clause of the
   predicate concerned.").
:- pred asserta_fact(+cgoal).

:- meta_predicate asserta_fact(primitive(fact)).
asserta_fact(Fact) :-
    meta_asserta_fact(Fact).

:- if(defined(optim_comp)).
meta_asserta_fact(Fact) :-
    '$asserta'(Fact, 'basiccontrol:true').
:- else.
meta_asserta_fact(Fact) :-
    '$compile_term'([Fact|'basiccontrol:true'], Ptr),
    '$current_clauses'(Fact, Root),
    '$inserta'(Root, Ptr).
:- endif.

:- doc(asserta_fact(Fact,Ref), "Same as @pred{asserta_fact/1},
   instantiating @var{Ref} to a unique identifier of the asserted
   fact.").
:- pred asserta_fact(+cgoal,-reference).

:- meta_predicate asserta_fact(primitive(fact),-).
:- if(defined(optim_comp)).
asserta_fact(Fact, Ref) :-
    '$asserta_ref'(Fact, 'basiccontrol:true', Ref).
:- else.
asserta_fact(Fact, Ref) :-
    '$compile_term'([Fact|'basiccontrol:true'], Ptr),
    '$current_clauses'(Fact, Root),
    '$inserta'(Root, Ptr),
    '$ptr_ref'(Ptr, Ref).
:- endif.

:- doc(assertz_fact(Fact), "@var{Fact} is added to the corresponding
   @concept{data predicate}.  The fact becomes the last clause of the
   predicate concerned.").
:- pred assertz_fact(+cgoal) => cgoal.

:- meta_predicate assertz_fact(primitive(fact)).
:- if(defined(optim_comp)).
assertz_fact(Fact) :-
    '$assertz'(Fact, 'basiccontrol:true').
:- else.
assertz_fact(Fact) :-
    '$compile_term'([Fact|'basiccontrol:true'], Ptr),
    '$current_clauses'(Fact, Root),
    '$insertz'(Root, Ptr).
:- endif.

:- doc(assertz_fact(Fact,Ref), "Same as @pred{assertz_fact/1},
   instantiating @var{Ref} to a unique identifier of the asserted
   fact.").
:- pred assertz_fact(+cgoal,-reference).

:- meta_predicate assertz_fact(primitive(fact),-).
:- if(defined(optim_comp)).
assertz_fact(Fact, Ref) :-
    '$assertz_ref'(Fact, 'basiccontrol:true', Ref).
:- else.
assertz_fact(Fact, Ref) :-
    '$compile_term'([Fact|'basiccontrol:true'], Ptr),
    '$current_clauses'(Fact, Root),
    '$insertz'(Root, Ptr),
    '$ptr_ref'(Ptr, Ref).
:- endif.

:- doc(current_fact(Fact), "Gives on backtracking all the facts
   defined as data or concurrent which unify with @var{Fact}.  It is
   faster than calling the predicate explicitly, which do invoke the
   meta-interpreter.  If the @var{Fact} has been defined as concurrent
   and has not been @concept{closed}, @pred{current_fact/1} will wait
   (instead of failing) for more clauses to appear after the last clause
   of @var{Fact} is returned.").
%% Current fact: if no clause is available, or if it is possible to 
%% determine that no matching exists, , $current_instance leaves
%% the predicate unlocked.  If the predicate is called, then it is left 
%% locked while the clause is being executed.
:- pred current_fact(+cgoal) => cgoal.

:- meta_predicate current_fact(primitive(fact)).
:- if(defined(optim_comp)).
current_fact(Fact) :-
    '$current'(Fact, 'basiccontrol:true').
:- else.
current_fact(Fact) :-
    '$current_clauses'(Fact, Root),
    '$current_instance'(Fact, ThisIsTrue, Root, _, block), this_is_true(ThisIsTrue),
    '$unlock_predicate'(Root).
:- endif.

:- doc(current_fact_nb(Fact), "Behaves as @pred{current_fact/1} but
   a fact is never waited on even if it is @concept{concurrent} and
   non-closed.").
:- pred current_fact_nb(+cgoal) => cgoal.

:- meta_predicate current_fact_nb(primitive(fact)).
:- if(defined(optim_comp)).
current_fact_nb(Fact) :-
    '$current_nb'(Fact, 'basiccontrol:true').
:- else.
current_fact_nb(Fact) :-
    '$current_clauses'(Fact, Root),
    '$current_instance'(Fact, ThisIsTrue, Root, _, no_block), this_is_true(ThisIsTrue),
    '$unlock_predicate'(Root).
:- endif.

:- doc(current_fact(Fact,Ref), "@var{Fact} is a fact of a
   @concept{data predicate} and @var{Ref} is its reference identifying
   it uniquely.").
:- pred current_fact(+cgoal,-reference) # "Gives on backtracking all
   the facts defined as data which unify with @var{Fact}, instantiating
   @var{Ref} to a unique identifier for each fact.".
:- pred current_fact(?cgoal,+reference) # "Given @var{Ref}, unifies
   @var{Fact} with the fact identified by it.".

:- meta_predicate current_fact(primitive(fact),-).
:- if(defined(optim_comp)).
current_fact(Fact, Ref) :-
    '$current_nb_ref'(Fact, 'basiccontrol:true', Ref).
:- else.
current_fact(Fact, Ref) :-
    '$ptr_ref'(Ptr, Ref), !,
    '$instance'(Fact, ThisIsTrue, Ptr), this_is_true(ThisIsTrue).
current_fact(Fact, Ref) :-
    '$current_clauses'(Fact, Root),
    '$current_instance'(Fact, ThisIsTrue, Root, Ptr, no_block), this_is_true(ThisIsTrue),
    '$ptr_ref'(Ptr, Ref),
    '$unlock_predicate'(Root).
:- endif.

:- if(defined(optim_comp)).
:- else.
% TODO: remove? (JF)
this_is_true(ThisIsTrue) :- ( ThisIsTrue = true -> true ; ThisIsTrue = 'basiccontrol:true' -> true ; fail ).
:- endif.

:- doc(retract_fact(Fact), "Unifies @var{Fact} with the first
   matching fact of a @concept{data predicate}, and then erases it.  On
   backtracking successively unifies with and erases new matching facts.
   If @var{Fact} is declared as @concept{concurrent} and is
   non-@concept{closed}, @pred{retract_fact/1} will wait for more
   clauses or for the closing of the predicate after the last matching
   clause has been removed.").
:- pred retract_fact(+cgoal) => cgoal. 

:- meta_predicate retract_fact(primitive(fact)).
:- if(defined(optim_comp)).
retract_fact(Fact) :-
    '$erase'(Fact, 'basiccontrol:true').
:- else.
retract_fact(Fact) :-
    '$current_clauses'(Fact, Root),
    '$current_instance'(Fact, ThisIsTrue, Root, Ptr, block), this_is_true(ThisIsTrue),
    '$erase'(Ptr),
    '$unlock_predicate'(Root).
:- endif.

:- doc(retract_fact_nb(Fact), "Behaves as @pred{retract_fact/1}, but
   never waits on a fact, even if it has been declared as
   @concept{concurrent} and is non-@concept{closed}.").
:- pred retract_fact_nb(+cgoal) => cgoal.

:- meta_predicate retract_fact_nb(primitive(fact)).
:- if(defined(optim_comp)).
retract_fact_nb(Fact) :-
    '$erase_nb'(Fact, 'basiccontrol:true').
:- else.
retract_fact_nb(Fact) :-
    '$current_clauses'(Fact, Root),
    '$current_instance'(Fact, ThisIsTrue, Root, Ptr, no_block), this_is_true(ThisIsTrue),
    '$erase'(Ptr),
    '$unlock_predicate'(Root).
:- endif.

:- doc(retractall_fact(Fact), "Erase all the facts of a
   @concept{data predicate} unifying with @var{Fact}.  Even if all facts
   are removed, the predicate continues to exist.").
:- pred retractall_fact(+cgoal) => cgoal. 

:- meta_predicate retractall_fact(primitive(fact)).
retractall_fact(Fact) :-
    meta_retractall_fact(Fact).

:- if(defined(optim_comp)).
meta_retractall_fact(Fact) :-
    '$erase_nb'(Fact, 'basiccontrol:true'),
    fail.
meta_retractall_fact(_).
:- else.
meta_retractall_fact(Fact) :-
    '$current_clauses'(Fact, Root),
    '$current_instance'(Fact, ThisIsTrue, Root, Ptr, no_block), this_is_true(ThisIsTrue),
    '$erase'(Ptr),
    '$unlock_predicate'(Root),
    fail.
meta_retractall_fact(_).
:- endif.

:- doc(close_predicate(Pred), "@cindex{closed} Changes the behavior
   of the predicate @var{Pred} if it has been declared as a
   @concept{concurrent predicate}: calls to this predicate will fail
   (instead of wait) if no more clauses of @var{Pred} are available.").
:- pred close_predicate(+cgoal) => cgoal.

:- meta_predicate close_predicate(primitive(fact)).
:- if(defined(optim_comp)).
close_predicate(Fact):-
    '$close_pred'(Fact).
:- else.
close_predicate(Fact):-
    '$current_clauses'(Fact, Root),
    '$close_predicate'(Root).
:- endif.

:- doc(open_predicate(Pred), "Reverts the behavior of
   @concept{concurrent predicate} @var{Pred} to waiting instead of
   failing if no more clauses of @var{Pred} are available.").
:- pred open_predicate(+cgoal) => cgoal.

:- meta_predicate open_predicate(primitive(fact)).
:- if(defined(optim_comp)).
open_predicate(Fact):-
    '$open_pred'(Fact).
:- else.
open_predicate(Fact):-
    '$current_clauses'(Fact, Root),
    '$open_predicate'(Root).
:- endif.

:- doc(set_fact(Fact), "Sets @var{Fact} as the unique fact of the
   corresponding @concept{data predicate}.").
:- pred set_fact(+cgoal) => cgoal.

:- if(defined(optim_comp)).
:- meta_predicate set_fact(primitive(fact)).
:- else.
:- meta_predicate set_fact(fact).
:- endif.
:- if(defined(optim_comp)).
set_fact(Fact) :-
    functor(Fact, F, A),
    functor(Template, F, A),
    meta_retractall_fact(Template),
    meta_asserta_fact(Fact).
:- else.
set_fact(Fact) :-
    term_to_meta(Fact_t, Fact),
    functor(Fact_t, F, A),
    functor(Template, F, A),
    meta_retractall_fact(Template),
    meta_asserta_fact(Fact_t).
:- endif.

:- doc(erase(Ref), "Deletes the clause referenced by @var{Ref}.").
:- pred erase(+reference) => reference + native.

:- if(defined(optim_comp)).
erase(Ref) :-
    '$erase_ref'(Ref).
:- else.
erase(Ref) :-
    '$ptr_ref'(Ptr, Ref),
    '$erase'(Ptr).
:- endif.

% :- doc(doinclude, fact/1).
% 
% :- prop fact(F) + regtype
%    # "@var{F} is a fact (an atom or a structure).".
% 
% fact(F) :- cgoal(F).

:- doc(doinclude, reference/1).
:- export(reference/1).
:- prop reference(R) + regtype
   # "@var{R} is a reference of a dynamic or data clause.".

reference('$ref'(_,_)).
