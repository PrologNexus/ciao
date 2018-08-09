:- use_package([assertions,iso]).
:- doc(nodoc,assertions).

:- doc(filetype,package).

:- doc(title,"ISO-Prolog package").

:- doc(author, "The Ciao Development Team").

:- doc(module,"This package makes the standard @concept{ISO-Prolog}
   predicates available by default in the modules that include it. In
   particular, it includes the modules and packages listed in the
   @em{Usage and interface} box below. Note that this package is also 
   included when loading the @ref{Classic Prolog} package.

   Also note that library predicates that correspond to those in the
   ISO-Prolog standard are marked accordingly throughout the manuals,
   and differences between the Ciao and the prescribed ISO-Prolog
   behaviours, if any, commented appropriately.

   Compliance with ISO is still not complete: currently there are some
   minor deviations in, e.g., the treatment of characters, the syntax,
   some of the arithmetic functions, and part of the error system.
   Also, Ciao does not offer a strictly conforming mode which rejects
   uses of non-ISO features. However, as mentioned in @ref{ISO-Prolog
   compliance versus extensibility} in the introduction, the intention
   of the Ciao developers is to complete progressively the ISO
   standard support, as well as adapt to the corrigenda and reasonable
   future extensions of the standard.

").

% @comment{Given that the final version of the ISO standard has only
% been recently published,}

% @comment{On the other hand, Ciao has been reported by independent
% sources (members of the standarization body) to be one of the most
% conforming Prologs at the moment of this writing, and the first one
% to be able to compile all the standard-conforming test cases.}
   
