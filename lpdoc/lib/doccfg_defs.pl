% \title Base Configuration Definitions for LPdoc
% 
% :- doc(module, "This file provides the base configuration definitions
%    and documentation for a @apl{lpdoc} settings file.").
% 
% :- doc(author, "Manuel Hermenegildo").
% :- doc(author, "Jose F. Morales").

% ----------------------------------------------------------------------------
% Paths 

:- pred filepath(Path) => dirpath 
# "Defines the directories where the @tt{.pl} files to be documented
   can be found.  You also need to specify all the paths containing
   files which are used by the files being documented. For example,
   the paths to files included by an @tt{@@include} command or to
   figures.".

:- pred output_name(Base) => sourcename
# "Defines the base file name used to be part of the output name of
   the generated files. By default it is equal to the root file of the
   document structure @pred{doc_structure/1}.

   If the @tt{no_versioned_output} option is not specified in
   @pred{doc_mainopts/1}, the bundle version number is appended to the
   output name".

% ----------------------------------------------------------------------------
% The document structure

:- pred doc_structure(Term)
# "Defines the document structure as a tree. The tree is defined as
   a root node with optional childs. Nodes can be atoms or pairs
   (@tt{N-Cs}), where @tt{Cs} is a list of nodes. The root of the
   tree is the main file of the manual, i.e., the file which
   determines the manual's cover page, and first chapter. The child
   files are used as components, i.e., which will constitute the
   subsequent chapters of the manual.".

% ----------------------------------------------------------------------------
% Processing options for the different files

:- pred doc_mainopts(Option) :: supported_option
# "@var{Option} is a processing option which should be activated when
   processing the main file.".

:- pred doc_compopts(Option) :: supported_option
# "@var{Option} is a processing option which should be activated when
   processing the secondary files (all except the main file).".

% ----------------------------------------------------------------------------
% Default document formats

:- pred docformat(Format) => supported_format
# "Defines the documentation formats to be generated.".

% ----------------------------------------------------------------------------
% Indices to be generated

:- pred index(Format) => supported_index
# "Defines the indices to be generated by default when running
   @apl{lpdoc}, among the following: 

   @noindent @tt{concept lib apl pred prop regtype decl op modedef file global}

   Selecting @tt{all} generates all the supported indices. However,
   note that this (as well as selecting many indices explicitely)
   exceeds the capacity of most texinfo installations.".

% ----------------------------------------------------------------------------
% References

:- pred bibfile(Format) => filename
# "If you are using bibliographic references, define in this way the
   @tt{.bib} files to be used to find them.".

% ----------------------------------------------------------------------------
% Other settings

:- pred startpage(PageNumber) => int
# "Setting this to a different value allows changing the page number of
   the first page of the manual. This can be useful if the manual is to
   be included in a larger document or set of manuals.
   Typically, this should be an odd number.".

:- pred papertype(PageNumber) => supported_papertype
# "Selects the type of paper/format for printed documents.  See also
   the @tt{-onesided} and @tt{-twosided} options for the main file.".

:- pred libtexinfo/1 => yesno
# "If set to yes the @file{texinfo.tex} file that comes with the
   lpdoc distribution will be used when generating manuals in
   formats such as @tt{dvi} and @tt{ps}. Otherwise, the texinfo file
   that comes with your @apl{tex} installation will be used. It is
   recommended that you leave this set to @tt{'yes'}.".

:- pred comment_version/1 => yesno
# "The source files contain version information. If not
   specified lpdoc will assume the opposite".

:- pred allow_markdown/1 => yesno
# "Allow LPdoc-flavored markdown in docstrings".

:- pred syntax_highlight/1 => yesno
# "Syntax highlight code blocks (only for HTML backend)".

% ---------------------------------------------------------------------------
% Installation options
% (You only need to change these if you will be installing the docs somewhere)

% Where manuals will be installed

:- pred htmldir/1 => dirpath 
# "Directory where the @tt{html} manual will be generated.".

:- pred docdir/1 => dirpath
# "Directory in which you want the document(s) installed.".

:- pred infodir/1 => dirpath
# "Directory in which you want the @tt{info} file installed.".

:- pred mandir/1 => dirpath 
# "Directory in which the @tt{man} file will be installed.".

% Permissions

:- pred datamode(DataPermissions) => permission_term
# "Define this to be the mode for automatically generated data
   files.".

:- pred execmode(ExecPermissions) => permission_term
# "Define this to be the mode for automatically generated
   directories and executable files.".
