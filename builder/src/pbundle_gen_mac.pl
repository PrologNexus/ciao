:- module(pbundle_gen_mac, [], [fsyntax, assertions]).

% TODO: This code needs major fixes and simplifications (use Prolog
%   when possible, use skel/template files, reuse binary installer
%   scripts)

:- doc(title, "Mac OS X distributions").

:- doc(author, "R@'{e}my Haemmerl@'{e}").
:- doc(author, "The Ciao Development Team").

:- doc(ack, "This builds the Ciao Macport Portfile and Mac pkg installers.
             Thanks to Edison Mera for his support.").

:- doc(copyright, "
Copyright @copyright{} 2008--2012 R@'{e}my Heammerl@'{e}/The Ciao Development Team.
").

% ===========================================================================

:- use_module(library(aggregates)).
:- use_module(library(lists)).
:- use_module(library(llists), [append/2]).
:- use_module(library(terms), [atom_concat/2]).
:- use_module(library(format)).
:- use_module(library(md5sum)).
:- use_module(library(process), [process_call/3, process_pipe/2]).
:- use_module(library(system)).
:- use_module(library(system_extra), [mkpath/2]).
:- use_module(library(streams)).
:- use_module(library(file_utils)).
:- use_module(library(pathnames), [path_concat/3]).

:- use_module(library(bundle/bundle_flags), [restore_all_bundle_flags/0]).
:- use_module(library(bundle/bundle_paths), [bundle_path/3]).
:- use_module(ciaobld(bundle_hash), [bundle_commit_info/3]).

:- use_module(ciaobld(eng_defs), [inst_eng_path/3]).
:- use_module(ciaobld(config_common), [default_eng_def/1]).
:- use_module(ciaobld(install_aux), [
    inst_builddir_path/2,
    inst_ciao_root/1,
    inst_bundle_path/3,
    active_docdir/2,
    perms/1
]).
:- use_module(ciaobld(pbundle_generator)).
:- use_module(ciaobld(builder_aux), [wr_template/4]).
:- use_module(ciaobld(builder_cmds), [builder_cmd/2]).
:- use_module(ciaobld(pbundle_gen_src)).

:- use_module(ciaobld(messages_aux), [normal_message/2, verbose_message/2]).

:- use_module(library(compiler/exemaker), [make_exec/2]).

% (hooks for gen_pbundle)
:- include(ciaobld(pbundle_gen_hookdefs)).

:- doc(summary, "This module provides predicates to build
@href{http://www.macports.com}{macports} @tt{Portfile} and Mac @tt{pkg} packages for
@apl{Ciao}. The system is designed to work on the MacOS (>= 10.4)
platform and requires Apple\'s Xcode 3.0 Developer Tools to be
installed in the machine where the packages are generated.").

:- doc(module, "
@section{Building Mac packages}

@subsection{Requirements (mac)}

These are the main prerequisites for the build process to succeed:

@begin{itemize}
@item A user account with enough free space (at least twice that 
      necessary for a compiled local repository for package generation).
@item A local @apl{Ciao} repository with binaries and documentation 
      already generated.
@item A working installation of @apl{Ciao}.
@item Apple\'s Xcode 3.0 Developer Tools (for package generation only).
@end{itemize}

The usual @apl{Ciao} software requirements also apply, the build
process should complain if anything is missing.

@subsection{Instructions (mac)}

@begin{itemize}
@item @tt{./ciao-boot.sh gen_pbundle --kind=macport}
@item @tt{./ciao-boot.sh gen_pbundle --kind=pkg}
@end{itemize}

@apl{./ciao-boot.sh gen_pbundle --kind=macport} produces a MacPorts
@tt{Portfile} which depends on a source @tt{tgz} distribution. If the
tarball is available in the package directory the command will
automatically produce it.  Notice that because the portfile includes a
checksum of the tarball source distribution it is dependent on this
archive. Since macports are used online the tarball used to produce
the portfile should be the same as the one available online on the
Ciao website.


@apl{./ciao-boot.sh gen_pbundle --kind=pkg} produces a @tt{pkg} (i.e.,
a standard \"Managed Install\" MacOs Package) wrapped into a dmg image
disk. This command assumes that binaries and documentation have been
locally built using correct configure options.

 
@section{Packager's maintenance guide (mac)}

The system comprises a skeleton for the MacPorts @tt{Portfile}
(@file{Portfile.skel}) that should be updated in case of changes in
@apl{ciao_builder}'s commands and options but also in case of changes
in the Ciao website.

Since the MacPorts @tt{Portfile} includes the electronic address where
the tarball source distribution resides (on the Ciao website) then any
change in the architecture of the website should be reflected here.
").

% ===========================================================================
% The URL and directory for our main distribution site

% TODO: Use bundle information
%:- export(home_url_str/1).
home_url_str := "https://ciao-lang.org/".
% TODO: Wrong
%:- export(packages_dir_str/1).
packages_dir_str := "packages/master/".

% ===========================================================================
:- doc(section, "pbundle Generation as a 'MacOS Binary Package' (.pkg)").

packageMaker := '/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker'.

% (hook)
gen_pbundle_hook(pkg, Target, _Options) :- !,
	gen_pbundle_descfile(Target),
	restore_all_bundle_flags, % TODO: Why is that necessary?
	gen_pbundle__pkg(Target).

:- pred gen_pbundle__pkg/1
   # "Create a temporary directory @apl{__tmp_for_packagemaker} in the
      current directory, install Ciao into this directory as root
      build and generate the package using @apl{package_pkg}, called
      with default arguments".

gen_pbundle__pkg(Target) :-
	normal_message("creating Mac OS X package", []),
	OutDir = ~pbundle_output_dir(Target),
	TmpDir = ~make_temp_dir,
	DestDir = ~path_concat(TmpDir, 'root'),
	mkpath(DestDir, ~perms), % TODO: owner?
	install_to_destdir(DestDir),
	generate_uninstaller(DestDir, UninstallerPath),
	package_pkg(Target, DestDir, TmpDir, OutDir, 'Ciao', % TODO: get from Target
	    ~dist_version(Target), PName),
	generate_uninstaller_wrapper(TmpDir, UninstallerPath,
	    UninstallWrapperPath),
	package_dmg(Target, OutDir, [PName, UninstallWrapperPath]),
	% TODO: Dangerous!
	process_call(path(rm), ['-rf', TmpDir], []).

% (also used in Portfile.skel)
% TODO: WRONG! Do not use a process
install_to_destdir(DestDir) :-
	process_call('./ciao-boot.sh',
	       ['install', ~atom_concat('--destdir=', DestDir)], []).

:- pred package_pkg(Target, DestPath, TmpDir, PPath, Name, Version, PName)
   # "Create a MacOS pkg package assuming that @var{DestPath} is the
      root build where Ciao has been installed, @var{Tmpdir} is a
      temporary directory where the predicate can store temporary
      files, @var{PPath} is the directory to store the package once it
      has been constructed. @var{Name} is the name of the distribution
      and @var{Version} the Ciao version".

package_pkg(Target, DestPath, TmpDir, PPath, Name, Version, PName) :-
        WorkPath = ~dist_workspace(Target),
	InfoFile = ~path_concat(WorkPath, 'Info.plist'),
%	DescriptionFile = ~path_concat(WorkPath, 'Description.plist'),
	ResourcesPath = ~path_concat(TmpDir, 'pkg_resources/English.lproj'),
	ScriptsDir = ~path_concat(TmpDir, 'Scripts'),
 	PName = ~atom_concat(~path_concat(PPath, ~dist_versioned_pkgname(Target)), '.pkg'),
	%
	mkpath(ResourcesPath, ~perms), % TODO: owner?
	write_welcome_html(ResourcesPath, Name, Version),
	write_conclusion_html(ResourcesPath),
%	copy_file('GPL', ~path_concat(ResourcesPath, 'Licence'), [overwrite]),
%	copy_file('Manifest/clip.png', ~path_concat(ResourcesPath, 'background'), [overwrite]),   
	generate_installation_scripts(DestPath, ScriptsDir),
	%
	write_info_plist(InfoFile, Name, Version),
	%
	verbose_message("Packaging binary distribution for Mac OS X", []),
	process_call(~packageMaker,
	       ['--root-volume-only', '--verbose',
		'--root', DestPath,
		'--out', PName,
		'--resources', ResourcesPath,
                '--scripts', ScriptsDir,
		'--title', ~atom_concat([Name, '-', Version]),
		'--info', InfoFile,
		'--target', ~target_os_version,
		'--domain', system,
		'--id', ~atom_concat('org.ciao-lang.', Name) % TODO: get from Target
	       ], [env(['PMResourceLocale'='English'])]).

package_dmg(Target, OutDir, List) :-
	verbose_message("Generating the dmg image", []),
	Name = ~dist_versioned_pkgname(Target),
 	DmgName = ~atom_concat(~path_concat(OutDir, Name), '.dmg'),
	process_list_for_src(List, Tail),
	process_call(path(hdiutil), 
	       ['create', DmgName,
		'-volname', Name
	       |Tail],
	       []).

process_list_for_src([],     []).
process_list_for_src([H|T1], ['-srcfolder', H|T2]) :-
	process_list_for_src(T1, T2).

:- use_module(library(glob), [glob/3]).

% TODO: Do not remove ~inst_ciao_root (only if it is empty)
generate_uninstaller(DestDir, Path) :-
	verbose_message("Generating bash script for uninstallation", []),
	BundleDirCore = ~inst_bundle_path(core, '.'),
	InstCiaoRoot = ~inst_ciao_root,
	%
 	Path = ~path_concat(InstCiaoRoot, 'uninstall_ciao'),
	%
	% Note: */* expands to bin/... lib/... but does not include
	%   those directories (we do not want to uninstall them)
	glob(DestDir, '*/*', DestFiles),
	% TODO: implement all in Prolog?
	process_pipe([
          process_call(path(find), DestFiles, [status(0)]),
	  process_call(path(sort), ['-r'])
        ], [cwd(DestDir), stdout(string(StrFiles))]),
	%
	info_files(DestDir, StrInfo),
	% TODO: Use shlang (quoting missing); use a skel?
	add_prefix_suffix(StrFiles, "rm -fd ", " || true", StrFiles_),
	add_prefix_suffix(StrInfo, "my_uninstall_info ", "", StrInfo_),
	generate_bash_script(DestDir, Path, "
my_uninstall_info () {
	 install-info --delete --info-dir=`dirname $1` $1 || true		   
}

if [ x\"$1\" != xNO_ASK_FOR_CONFIRMATION ]; then
    echo \"Are you sure you want to uninstall Ciao?(yes/NO)\"
    read answer 
    if [ x\"$answer\" != xyes ]; then 
	echo \"Uninstallation canceled!\"
	exit 1
    fi
fi

~s

~s

rm -fd ~a ~a ~a || true\n
", [StrInfo_, StrFiles_, Path, BundleDirCore, InstCiaoRoot]).


generate_uninstaller_wrapper(TmpDir, Path, AppPath) :-
	verbose_message("Generating uninstaller wrapper", []),
 	TxtPath = ~path_concat(TmpDir, 'uninstaller.applescript'),
 	AppPath = ~path_concat(TmpDir, 'Uninstall Ciao.app'),
	%
	open_output(TxtPath, Stream), Stream = o(_, Stream_),
	format(Stream_, 
"set PosixPath to \"~a\"

try
	(PosixPath as POSIX file) as alias
	beep
	display dialog \"Are you sure you want to uninstall Ciao?\"
	do shell script PosixPath & \" NO_ASK_FOR_CONFIRMATION\" with administrator privileges
	display dialog \"Ciao uninstalled successfully.\" buttons \"OK\" default button \"OK\"
on error
	display dialog \"Ciao does not seem to be installed in your system.\" buttons \"OK\" default button \"OK\"
end try
", [Path]),
	close_output(Stream),
	process_call(path(osacompile), ['-ai386', '-o', AppPath, TxtPath], []).

% TODO: wrong! use glob, etc.
% Enumerate info files under ~infodir_local (recursively), relative to BaseDir
%:- export(info_files/2).
info_files(BaseDir, Str) :-
	InfoDir = ~infodir_local, 
	( % Note: do not evaluate wildcard in -name pattern!
	  process_call(path(find), [InfoDir, '-name', '*.info'],
		 [cwd(BaseDir), stdout(string(Str))]) ->
	    true
	; Str = ""
	).

infodir_local(LocalInfoDir) :-
	( atom_codes(~active_docdir(info), "/"||LocalPathStr) ->
	    atom_codes(LocalInfoDir, LocalPathStr)
	; throw(error('infodir should be absolute (i.e. starting with /)', generate_uninstaller/2))
	).

% TODO: ugly hack, split in lines instead
add_prefix_suffix(L1, Prefix, Suffix, L) :-
	append(Prefix, "/"||L2, L),
	add_prefix_suffix_(L1, Prefix, Suffix, L2).
add_prefix_suffix([], _, _, []).
add_prefix_suffix_("\n"||L1, Prefix, Suffix, L) :- !,
	append(Suffix, "\n"||L2, L),
	add_prefix_suffix(L1, Prefix, Suffix, L2).
add_prefix_suffix_(" "||L1, Prefix, Suffix, "\\ "||L2) :- !,
	add_prefix_suffix_(L1, Prefix, Suffix, L2).
add_prefix_suffix_([H|L1], Prefix, Suffix, [H|L2]) :-
	add_prefix_suffix_(L1, Prefix, Suffix, L2).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% MacPorts Portfile                                        %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

:- use_module(library(system), [file_exists/1]).

target_os_version('10.4').
os_version(A) :-
	process_call(path(sw_vers), ['-productVersion'],
	             [stdout(line(Str))]),
	!,
	A = Str.

applescript_editor(X) :-
	(
	    ~os_version @>= "10.6" ->
	    X = "AppleScript Editor"
	;
	    X = "Script Editor"
	).

% (hook)
% Generate MacPorts @tt{Portfile}. The source distribution is generated if missing.
gen_pbundle_hook(macport, Target, _Options) :- !,
	TGZ = ~dist_versioned_pkgname(Target),
	AbsTGZ = ~atom_concat(~path_concat(~pbundle_output_dir(Target), ~dist_versioned_pkgname(Target)), '.tar.gz'),
	% TODO: check necessary?
	( file_exists(AbsTGZ) ->
	    true
	; gen_pbundle_hook(tgz, Target, []) % (in pbundle_gen_src)
	),
	%
	MD5Sum = ~md5sum(AbsTGZ),
	wr_template(at(~pbundle_output_dir(Target)), ~bundle_path(builder, 'src/mac'), 'Portfile', [
            'Version' = ~dist_version(Target),
            'VersionedPkgName' = ~atom_codes(TGZ),
            'HomeURL' = ~home_url_str, % TODO: from Target
            'MasterURL' = ~master_url_str(Target), % TODO: from Target
	    'MD5CheckSum' = MD5Sum
        ]).

% ---------------------------------------------------------------------------

% TODO: See macports documentation for explanation (check that this is correct)
% TODO: Define a predicate pbundle_url for this, connect with ciaobot code
master_url_str(Target) := MasterURL :-
	Name = ~atom_codes(~dist_versioned_pkgname(Target)),
	dist_main_bundle(Target, MainBundle),
	Rev = ~atom_codes(~bundle_commit_info(MainBundle, id)),
 	% TODO: Define a predicate pbundle_url for this, connect with ciaobot code
	MasterURL = ~append([~home_url_str, ~packages_dir_str, Rev, "/", Name]).

write_xml_header(Str) :-
	format(Str, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\">
\n", []).


write_info_plist(File, Name, Version) :-
	open_output(File, OStr), OStr = o(_, Str),
	write_xml_header(Str),
	format(Str, "<dict>
	<key>CFBundleGetInfoString</key>
	<string>~w ~w</string>
	<key>CFBundleIdentifier</key>
	<string>es.upm.fi.dia.clip.~w</string>
	<key>CFBundleName</key>
	<string>~w-~w</string>
	<key>CFBundleShortVersionString</key>
	<string>~w</string>
	<key>IFMajorVersion</key>
	<integer>0</integer>
	<key>IFPkgFlagAllowBackRev</key>
	<true/>
	<key>IFPkgFlagAuthorizationAction</key>
	<string>RootAuthorization</string>
	<key>IFPkgFlagDefaultLocation</key>
	<string>/</string>
	<key>IFPkgFlagInstallFat</key>
	<false/>
	<key>IFPkgFlagIsRequired</key>
	<false/>
	<key>IFPkgFlagRelocatable</key>
	<false/>
	<key>IFPkgFlagRestartAction</key>
	<string>NoRestart</string>
	<key>IFPkgFlagRootVolumeOnly</key>
	<false/>
	<key>IFPkgFlagUpdateInstalledLanguages</key>
	<false/>
	<key>IFPkgFormatVersion</key>
	<real>0.10000000</real>
</dict>
</plist>\n", [Name, Version, Name, Version, Name, Version]),
	close_output(OStr).


write_description_plist(File, Name, Version, Description) :-
	open_output(File, OStr), OStr = o(_, Str),
	write_xml_header(Str),
	format(Str, "<dict>
	<key>IFPkgDescriptionDeleteWarning</key>
	<string></string>
	<key>IFPkgDescriptionDescription</key>
	<string>~w</string>
	<key>IFPkgDescriptionTitle</key>
	<string>~w</string>
	<key>IFPkgDescriptionVersion</key>
	<string>~w</string>
</dict>
</plist>n", [Description, Name, Version]),
	close_output(OStr).


write_package_info(File) :-
	open_output(File, OStr), OStr = o(_, Str),
	format(Str,
"<pkg-info install-location=\"/\" relocatable=\"false\" auth=\"root\"></pkg-info>\n",
	    []),
	close_output(OStr).

write_welcome_html(ResourcesPath, Name, Version) :-
 	open_output(~path_concat(ResourcesPath, 'Welcome.html'), OStr), OStr = o(_, Str),
	% TODO: use skel
	format(Str, "<html lang=\"en\">
<head>
	<meta http-equiv=\"content-type\" content=\"text/html; charset=iso-8859-1\">
	<title>Install ~w</title>
</head>
<body>
<font face=\"Helvetica\">
<b>Welcome to the ~w for Mac OS X Installer</b>
<p>
Ciao is a public domain, next generation multi-paradigm 
programming environment with a unique set of features:
</p>
<p><a href=\"~s\">~s</a></p>
<p>
This installer will install the system in ~w.
</p>
<p>
This installer guides you through the steps necessary to 
install ~w ~w for Mac OS X. To get started, click Continue.
</p>
</font>
</body>
</html>\n", [Name, Name, ~home_url_str, ~home_url_str, ~inst_ciao_root, Name, Version]),
	close_output(OStr).

write_conclusion_html(ResourcesPath) :-
	InitFile = ~path_concat(~inst_ciao_root, 'ciao-mode-init.el'),
	open_output(~path_concat(ResourcesPath, 'Conclusion.html'), OStr), OStr = o(_, Str),
	format(Str, 
"<html lang=\"en\">
<head>
	<meta http-equiv=\"content-type\" content=\"text/html; charset=iso-8859-1\">
	<title>Important Information</title>
</head>
<body>
<font face=\"Helvetica\">
<p>
In order to enable the Ciao development environment for Emacs
(necessary for the source-level debugger and many other
functionalities), you need to <b>add manually at the end of your .emacs</b>
the following lines:
<pre>
(if (file-exists-p \"~w\")
 (load-file \"~w\")
)
</pre><br/>
It is strongly encouraged that you <b>install a recent graphical version
of emacs</b> (rather than the text-only version that comes with Mac OS), such as
<a href=\"http://emacsformacosx.com/\">Cocoa Emacs</a>
or <a href=\"http://aquamacs.org/\">Aquamacs</a>.
</p>
<p>
For uninstallation please use the \"Uninstall Ciao\" script that comes
with the installer.
</p></font>
</body>
</html>\n", [InitFile, InitFile]),
	close_output(OStr).


generate_installation_scripts(DestDir, Dir) :-
	process_call(path(mkdir), ['-p', Dir], []), 
	generate_bash_script(Dir, 'preupgrade', "\n$RECEIPT_PATH/preinstall\n", []),
	generate_bash_script(Dir, 'postupgrade', "\n$RECEIPT_PATH/postinstall\n", []), 
	%
	generate_preinstall_script(Dir, 'preinstall'),
	generate_postinstall_script(Dir, 'postinstall', DestDir).


% preinstall script 
%   - uninstallation of previously installed version of ciao

% NOTE: the uninstaller for version 1.1[45] (revision < 14672) is not
% in the same place as newer version (see script code).

% TODO: Deprecate old uninstallers
generate_preinstall_script(Dir, Name) :-
	generate_bash_script(Dir, Name, "
# Call old implementation of uninstallers
for file in /usr/local/lib/ciao/ciao-1.1[45]/delete_ciao; do
    if [ -f $file ]; then 
	$file NO_ASK_FOR_CONFIRMATION || true
    fi
done

# Call new uninstaller
for file in ~w/uninstall_ciao; do
    if [ -f $file ]; then 
	$file NO_ASK_FOR_CONFIRMATION || true
    fi
done
", [~inst_ciao_root]).

% postinstall script performs:
%   - installation of info file (should be done by calling ciao-boot.sh)
generate_postinstall_script(Dir, Name, DestDir) :-
	info_files(DestDir, Str),
	add_prefix_suffix(Str, "my_install_info ", "", Str_),
	% TODO: Use shlang (quoting missing)
	generate_bash_script(Dir, Name, "
my_install_info () {
	 install-info --info-dir=`dirname $1` $1 || true		   
}\n
~s", [Str_]).

generate_bash_script(Dir, Name, Str, Args) :-
	open_output(~path_concat(Dir, Name), OStream), OStream = o(_, Stream),
	format(Stream, "#!/bin/bash\n", []), 
	format(Stream, Str, Args), 
	close_output(OStream).

% ===========================================================================
:- doc(section, "pbundle Generation as a 'MacOS bundle' (.app)").

:- use_module(library(emacs/emacs_batch)).

% (hook)
gen_pbundle_hook(app, Target, _Options) :- !,
	gen_pbundle_descfile(Target),
	restore_all_bundle_flags, % TODO: Why is that necessary?
	gen_pbundle__app(Target).

gen_pbundle__app(Target) :-
	% Note: Mac OS X bundle IS NOT a Ciao bundle
	normal_message("creating Mac OS X bundle (as .app)", []),
	%
	Domain = "org.ciao-lang.ciao", % TODO: get from target
	BundleDirCore = ~inst_bundle_path(core, '.'),
	BinDir = ~inst_builddir_path('bin'),
	Eng = ~default_eng_def,
	CiaoEngine = ~inst_eng_path(exec, Eng),
	OutDir = ~pbundle_output_dir(Target),
	TmpDir = ~make_temp_dir,
	%
	AppBundlePath = ~path_concat(TmpDir, 'Ciao.app'),
	ResourcesDir = ~path_concat(AppBundlePath, 'Contents/Resources'),
	%
	% TODO: (see environment_and_windows_bats for similar code)
	set_emacs_type('MacOSBundle'), % TODO: strange; do it dynamically instead
 	% TODO: the emacs mode could be a bundle on its own in the future
	builder_cmd(build_bin, 'ciao_emacs.emacs_mode'), % TODO: make sure that this is rebuild
	unset_emacs_type,
	%
	wr_template(origin, ~bundle_path(builder, 'src/mac'), 'Ciao.applescript', [
	    'VERSION' = ~dist_versioned_pkgname(Target),
	    'BUNDLEDIR_CORE' = BundleDirCore,
	    'CIAOENGINE' = CiaoEngine,
	    'BINDIR' = BinDir,
	    'DOMAIN' = Domain
        ]),
	%
	process_call(path(osacompile),
	       ['-ai386', '-o', AppBundlePath,
		~bundle_path(builder, 'src/mac/Ciao.applescript')], []),
	%
	wr_template(at(TmpDir/'Ciao.app/Contents'), ~bundle_path(builder, 'src/mac'), 'Info.plist', [
	    'VERSION' = ~dist_versioned_pkgname(Target),
	    'DOMAIN' = Domain
	]),
	%
	process_call(path(rm), ['-f', ~path_concat(ResourcesDir, 'applet.icns')], []),
	%
	install_to_destdir(ResourcesDir),
	process_call(path(cp), ['-f',
	            ~bundle_path(builder, 'src/mac/ciao-icon.icns'),
		    ResourcesDir], []),
	%
        % TODO: This is wrong! We should call 'ciao-env' with a new '--elisp' option
	% TODO: try not to write the output here
	wr_template(origin, ~bundle_path(builder, 'src/mac'), 'configure_dotemacs.pl', [
	    'CIAOENGINE' = CiaoEngine,
	    'BINDIR' = BinDir,
	    'BUNDLEDIR_CORE' = BundleDirCore,
	    'DOMAIN' = Domain
        ]),
 	RBinDir = ~atom_concat(ResourcesDir, BinDir),
	process_call(path(mkdir), ['-p', RBinDir], []),
	make_exec([~bundle_path(builder, 'src/mac/configure_dotemacs.pl')],
	           ~path_concat(RBinDir, 'configure_dotemacs')),
	%
 	RBundleDirCore = ~atom_concat(ResourcesDir, BundleDirCore),
 	open_output(~path_concat(RBundleDirCore, 'sample.pl'), OStr), OStr = o(_, Str),
	sample_program_text(Str),
	close_output(OStr),
	%
	package_dmg(Target, OutDir, [AppBundlePath]),
	process_call(path(mv), [AppBundlePath, OutDir], []),
%	process_call(path(rm), ['-rf', TmpDir], []), 
	true.

% TODO: Move to a file and copy
sample_program_text(Str) :-
	format(Str,
"% You can type code in this buffer.  
% Save with \"File->Save Buffer As...\" or \"C-x C-s\".
% Load into toplevel with \"C-c l\"
% Explore menus and buttons above.
% See also Section \"Using Ciao inside GNU emacs\" of the Ciao manual
% (\"CiaoHelp->Ciao system manual\") 

:- module(_,_).

main(Arg) :- 
	write(Arg).

", []).

% ---------------------------------------------------------------------------

make_temp_dir := Dir :-
	% TODO: change name of tmp dir, use (fix if needed) system:mktemp_in_tmp/2
	process_call(path(mktemp),
	       ['-d', '/tmp/Ciao_package_XXXXX'],
	       [stdout(line(Codes))]),
	atom_codes(Dir, Codes).

