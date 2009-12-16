# Simple configuration script for OpenChange
# Written by Jelmer Vernooij <jelmer@openchange.org>

AC_PREREQ(2.57)
AC_INIT(openchange, 0.9, [openchange@openchange.org])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE
AC_DEFINE(_GNU_SOURCE, 1, [Use GNU extensions])

PKG_PROG_PKG_CONFIG([0.20])

CFLAGS="-I. $CFLAGS"

dnl #################################################################
dnl Check for OS dependent options
dnl #################################################################
AC_CANONICAL_HOST

case "${host}" in
     *freebsd*) 
		BUILD_FOR_FREEBSD=yes
		AC_SUBST(BUILD_FOR_FREEBSD)
		MANUALLY_CREATE_SYMLINKS=yes
		;;
     *solaris*) 
		BUILD_FOR_SOLARIS=yes
		AC_SUBST(BUILD_FOR_SOLARIS)
		MANUALLY_CREATE_SYMLINKS=yes
		;;
esac
AC_SUBST(MANUALLY_CREATE_SYMLINKS)

#
# OC_CHECK_SAMBA_PATH([PATH],[action-if-found],[action-if-not-found])
# -------------------------------------------------------------------
AC_DEFUN([OC_CHECK_SAMBA_PATH],
[
	old_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"
	PKG_CONFIG_PATH="$1/lib/pkgconfig"
	export PKG_CONFIG_PATH
	PKG_CHECK_EXISTS([samba-hostconfig], [found=1], [found=0])
	PKG_CONFIG_PATH="$old_PKG_CONFIG_PATH"
	if test $found = 1; then
		ifelse([$2],[], [echo -n ], [$2])
		ifelse([$3],[],[],[else
	[$3]])
	fi
])

AC_MSG_CHECKING([for samba 4])

AC_ARG_WITH(samba, 
[AC_HELP_STRING([--with-samba], [Override location Samba is installed])],
[ 
 sambaprefix="$withval"
],[
 for p in "$prefix" /usr/local/samba /usr/local /usr
 do
	 OC_CHECK_SAMBA_PATH($p, [sambaprefix="$p"])
 done
])
AC_SUBST(sambaprefix)
OC_CHECK_SAMBA_PATH($sambaprefix,[], AC_MSG_ERROR(Samba 4 not found))
AC_MSG_RESULT($sambaprefix)
PKG_CONFIG_PATH="$sambaprefix/lib/pkgconfig:$PKG_CONFIG_PATH"
PATH="$sambaprefix/bin:$PATH"

#
# OC_SETVAL([NAME])
# -----------------
AC_DEFUN([OC_SETVAL],
[
AC_ARG_VAR([NAME], [var name])
if test x"$enable_$1" = x""; then
   enable_$1="no"
fi[]
])

#
# OC_CHECK_SAMBA_VERSION([RELEASE],[VERSION], [action-if-found],[action-if-not-found],
#			       	 	      [action-if-cross-compiling])
# ------------------------------------------------------------------------------------
AC_DEFUN([OC_CHECK_SAMBA_VERSION], [
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <samba/version.h>
int main() { if (!strcmp(SAMBA_VERSION_STRING, "$1") || !strcmp(SAMBA_VERSION_STRING, "$2")) {return 0; } else { return -1;} }
]])],[$3],[
	ifelse([$4],[],[AC_MSG_WARN([The Samba4 version installed on your system doesn't meet OpenChange requirements ($1 or $2).])],[$4])],[$5])
])

#
# OC_RULE_ADD([NAME], [TYPE])
# ---------------------------
AC_DEFUN([OC_RULE_ADD], 
[ 
AC_ARG_VAR([NAME], [rule name])
AC_ARG_VAR([TYPE], [rule type])
if test "x$1_set" != "xset"; then
   case "$2" in
   	LIBS)
		OC_$2="$OC_$2 $1"
   		OC_$2_INSTALL="$OC_$2_INSTALL $1-install"
   		OC_$2_UNINSTALL="$OC_$2_UNINSTALL $1-uninstall"
   		OC_$2_INSTALLPC="$OC_$2_INSTALLPC $1-installpc"
   		OC_$2_INSTALLHEADER="$OC_$2_INSTALLHEADER $1-installheader"
   		OC_$2_INSTALLLIB="$OC_$2_INSTALLLIB $1-installlib"

		AC_SUBST(OC_$2_INSTALLPC)
		AC_SUBST(OC_$2_INSTALLHEADER)
		AC_SUBST(OC_$2_INSTALLLIB)
	;;
	TOOLS|TORTURE)
		OC_$2="$OC_$2 $1"
		OC_$2_INSTALL="$OC_$2_INSTALL $1-install"
   		OC_$2_UNINSTALL="$OC_$2_UNINSTALL $1-uninstall"
	;;
	SERVER|MAPISTORE)
		OC_$2="$OC_$2 $1"
		OC_$2_CLEAN="$OC_$2_CLEAN $1-clean"
		OC_$2_INSTALL="$OC_$2_INSTALL $1-install"
   		OC_$2_UNINSTALL="$OC_$2_UNINSTALL $1-uninstall"
	;;
   esac

   AC_SUBST(OC_$2)
   AC_SUBST(OC_$2_CLEAN)
   AC_SUBST(OC_$2_INSTALL)
   AC_SUBST(OC_$2_UNINSTALL)

   enable_$1="yes"

fi[]
])

dnl ##################################################################
dnl Some general portability stuff
dnl ##################################################################
AC_CHECK_HEADERS( sys/cdefs.h string.h sys/sockio.h)
AC_CHECK_FUNCS( strcasestr )



dnl ###########################################################################
dnl _AC_LANG_COMPILER_ICC
dnl Check whether the compiler for the current language is really ICC.
dnl ###########################################################################
m4_define([AC_LANG_COMPILER_ICC],
[AC_CACHE_CHECK([whether we are really using the Intel _AC_LANG compiler],
		[ac_cv_[]_AC_LANG_ABBREV[]_compiler_icc],
[_AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [[#ifndef __INTEL_COMPILER
       choke me
#endif
]])],
		   [ac_compiler_icc=yes],
		   [ac_compiler_icc=no])
ac_cv_[]_AC_LANG_ABBREV[]_compiler_icc=$ac_compiler_icc
])])

dnl ###########################################################################
dnl _AC_LANG_COMPILER_SUNCC
dnl Check whether the compiler for the current language is really Sun compiler.
dnl ###########################################################################
m4_define([AC_LANG_COMPILER_SUNCC],
[AC_CACHE_CHECK([whether we are really using the Sun _AC_LANG compiler],
		[ac_cv_[]_AC_LANG_ABBREV[]_compiler_suncc],
[_AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [[#ifndef __SUNPRO_C
       choke me
#endif
]])],
		   [ac_compiler_suncc=yes],
		   [ac_compiler_suncc=no])
ac_cv_[]_AC_LANG_ABBREV[]_compiler_suncc=$ac_compiler_suncc
])])

dnl ###########################################################################
dnl FreeBSD installs some libraries such as libpopt in the non default
dnl search path /usr/local/{include,lib}. This nasty hack ensures
dnl configure.ac will find the library if available and additional
dnl flags be correctly added while compiling.
dnl ###########################################################################
if test x"$BUILD_FOR_FREEBSD" = x"yes"; then
   CFLAGS="$CFLAGS -I/usr/local/include"
   LDFLAGS="$LDFLAGS  -L/usr/local/lib"
   CXXFLAGS="$CXXFLAGS -I/usr/local/include"
fi

dnl ----------------------------------------------------------------------------
dnl Check for comparison_fn_t
dnl ----------------------------------------------------------------------------
AC_CHECK_TYPE(comparison_fn_t, 
[AC_DEFINE(HAVE_COMPARISON_FN_T, 1,[Whether or not we have comparison_fn_t])],
[], [[#include <stdlib.h>]])


dnl ###########################################################################
dnl libmapi and required dependencies
dnl ###########################################################################

dnl ---------------------------------------------------------------------------
dnl Check for CC
dnl ---------------------------------------------------------------------------
AC_PROG_CC

dnl ---------------------------------------------------------------------------
dnl coverage testing
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE([coverage], AS_HELP_STRING([--enable-coverage],
                                        [Compile the library with code coverage support (default is NO)]),
                                        [use_cov=$enableval], [use_cov=no])


if test "x$use_cov" = x"yes"; then
   COVERAGE_INIT="coverage-init"
fi
AC_SUBST(COVERAGE_INIT)

dnl ---------------------------------------------------------------------------
dnl Set up the right compiler options
dnl ---------------------------------------------------------------------------
AC_LANG_COMPILER_SUNCC 
AC_LANG_COMPILER_ICC

if test x"$ac_cv_c_compiler_suncc" = x"yes"; then
dnl Sun Studio Compiler
    COMPILER_OPTIONS_SHARED="-D__FUNCTION__=__func__"
    COMPILER_OPTIONS_C="$COMPILER_OPTIONS_SHARED"
    COMPILER_OPTIONS_CXX="$COMPILER_OPTIONS_SHARED"
    LDFLAGS="$LDFLAGS -z ignore -R '\$\$ORIGIN/../lib'"
elif test x"$ac_cv_c_compiler_icc" = x"yes"; then
dnl Intel Compiler
    COMPILER_OPTIONS_SHARED="-O3 -Wall -g3 -fstrict-aliasing -Wmissing-prototypes -Wstrict-prototypes -wd2259,188,593,869,981,181,1419,2218"
    COMPILER_OPTIONS_C="$COMPILER_OPTIONS_SHARED"
    COMPILER_OPTIONS_CXX="$COMPILER_OPTIONS_SHARED"
else
dnl GNU Compiler
    COMPILER_OPTIONS_SHARED="-Wall -g3 -fstrict-aliasing -Wp,-D_FORTIFY_SOURCE=2"
    if test "x$use_cov" = "xyes"; then
        COMPILER_OPTIONS_SHARED="-O0 $COMPILER_OPTIONS_SHARED"
    else
        COMPILER_OPTIONS_SHARED="-O3 $COMPILER_OPTIONS_SHARED"
    fi
    COMPILER_OPTIONS_C="$COMPILER_OPTIONS_SHARED -Wmissing-prototypes -Wstrict-prototypes"
    if test "x$use_cov" = "xyes"; then
        COMPILER_OPTIONS_C="$COMPILER_OPTIONS_C -fprofile-arcs -ftest-coverage"
        LDFLAGS="$LDFLAGS -lgcov --coverage"
    fi
    COMPILER_OPTIONS_CXX="$COMPILER_OPTIONS_SHARED"
fi
AC_SUBST(COMPILER_OPTIONS_C)
AC_SUBST(COMPILER_OPTIONS_CXX)

dnl ---------------------------------------------------------------------------
dnl Define an assert macro if this is a development release
dnl ---------------------------------------------------------------------------
. `dirname $0`/VERSION
AC_SUBST(OPENCHANGE_VERSION_IS_SVN_SNAPSHOT)

if test x"$OPENCHANGE_VERSION_IS_SVN_SNAPSHOT" = x"yes"; then
   ASSERT_DEFINITION="-DENABLE_ASSERTS"
fi
AC_SUBST(ASSERT_DEFINITION)

dnl ---------------------------------------------------------------------------
dnl Check for install
dnl ---------------------------------------------------------------------------
AC_PROG_INSTALL

dnl ---------------------------------------------------------------------------
dnl Check for Perl
dnl ---------------------------------------------------------------------------
AC_PATH_PROG(PERL, perl)

if test x"$PERL" = x""; then
   AC_MSG_WARN([No version of perl was found!])
   AC_MSG_ERROR([Please install perl http://www.perl.com])
fi
AC_SUBST(PERL)

dnl ---------------------------------------------------------------------------
dnl Check for Pidl
dnl ---------------------------------------------------------------------------
AC_PATH_PROG(PIDL, pidl)

if test x"$PIDL" = x""; then
   	AC_MSG_WARN([No version of pidl was found!])
	AC_MSG_ERROR([Please install pidl])
fi
AC_SUBST(PIDL)

dnl ---------------------------------------------------------------------------
dnl Check for Python
dnl ---------------------------------------------------------------------------

AC_PATH_PROG(PYTHON,python)
AC_PATH_PROG(PYTHON_CONFIG,python-config)

AC_MSG_CHECKING(python library directory)
pythondir=`$PYTHON -c "from distutils import sysconfig; print sysconfig.get_python_lib(1, 0, '\\${prefix}')"`
AC_MSG_RESULT($pythondir)

AC_SUBST(pythondir)

dnl ----------------------------------------------------------------------------
dnl Check for Flex
dnl Flex version < 2.5.35 is fine but 2.5.4 beta is not. This is the
dnl default version provided under FreeBSD or RHL5
dnl ----------------------------------------------------------------------------
if test x"$OPENCHANGE_VERSION_IS_SVN_SNAPSHOT" = x"yes"; then
   AC_ARG_VAR([FLEX], [Location of the flex program.])
   AC_CHECK_PROG([FLEX], [flex], [flex])

   if test x"$FLEX" = x""; then
      	AC_MSG_WARN([No version of flex was found!])
	AC_MSG_ERROR([Please install flex])
   else
	V=`$FLEX --version | awk '{print $NF}'`
   	W=`echo $V | awk -F. '{if (((($1*100 + $2)*100 + $3) > 20535) || $3 == 4) print "no"}'`
   	if test "x$W" != x ; then
	   AC_MSG_WARN([Adjust your FLEX environment variable])
      	   AC_MSG_ERROR([Flex version 2.5.35 or below is needed. You have $V])
      	fi
   fi

   AC_SUBST(FLEX)
fi

dnl ---------------------------------------------------------------------------
dnl Samba4 modules
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(TALLOC, talloc)
PKG_CHECK_MODULES(SAMBA, dcerpc ndr samba-hostconfig tevent)
PKG_CHECK_MODULES(TDB, tdb)
PKG_CHECK_MODULES(LDB, ldb)

dnl ---------------------------------------------------------------------------
dnl Check a particular Samba4 git revision
dnl ---------------------------------------------------------------------------

oc_save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS `$PKG_CONFIG --cflags-only-I samba-hostconfig`"
AC_CHECK_HEADER([samba/version.h],, AC_MSG_ERROR([Could not find Samba4 headers. Re-run ./configure with --with-samba=XXX where
 XXX is the prefix that Samba4 was installed to.]))

. `dirname $0`/script/samba4_ver.sh

OC_CHECK_SAMBA_VERSION([$SAMBA4_RELEASE],[$SAMBA4_GIT_VER-GIT-$SAMBA4_GIT_REV])
CPPFLAGS="$oc_save_CPPFLAGS"

dnl ---------------------------------------------------------------------------
dnl Finally add libmapi to the library list
dnl ---------------------------------------------------------------------------
OC_RULE_ADD(libmapi, LIBS)

dnl nasty hack: only compile IDL if we have a library
dnl libraries require libmapi and libmapi require IDL
OC_IDL="idl"
AC_SUBST(OC_IDL)

dnl ###########################################################################
dnl libmapi++ dependencies
dnl ###########################################################################

dnl ---------------------------------------------------------------------------
dnl Check for g++
dnl ---------------------------------------------------------------------------
AC_CACHE_CHECK([C++ compiler availability], [ac_cv_libmapixx_gxx_works],
	       [
		AC_LANG_PUSH([C++])
		AC_COMPILE_IFELSE([int main() { return 0; }], 
				  [ac_cv_libmapixx_gxx_works=yes],
				  [ac_cv_libmapixx_gxx_works=no])
		AC_LANG_POP([C++])
		])

dnl ---------------------------------------------------------------------------
dnl Check for boost-thread
dnl ---------------------------------------------------------------------------

AC_ARG_VAR([BOOST_LIB_SUFFIX], [Boost library name suffix])

AC_CACHE_CHECK([for boost_thread$BOOST_LIB_SUFFIX library], [ov_cv_boost_thread],
	       [
	        ov_cv_boost_thread=no
		ov_save_LIBS=$LIBS
		LIBS="-lboost_thread$BOOST_LIB_SUFFIX $LIBS"
		AC_LANG_PUSH([C++])
		AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <boost/thread.hpp>]],
						[[boost::thread t]])],
				[ov_cv_boost_thread=yes])
		AC_LANG_POP([C++])
		LIBS=$ov_save_LIBS
	       ])


if test x"$ac_cv_libmapixx_gxx_works" = "xyes"; then
   if test x"$ov_cv_boost_thread" = "xyes"; then
      AC_PROG_CXX 
      OC_RULE_ADD(libmapixx, LIBS)
   fi
fi


dnl ###########################################################################
dnl libocpf dependencies
dnl ###########################################################################

dnl ---------------------------------------------------------------------------
dnl Check for Bison
dnl ---------------------------------------------------------------------------
if test x"$OPENCHANGE_VERSION_IS_SVN_SNAPSHOT" = x"yes"; then
   AC_ARG_VAR([BISON], [Location of the bison program.])
   AC_PATH_PROG([BISON], [bison], [bison])

   if test x"$BISON" != x""; then
      OC_RULE_ADD(libocpf, LIBS)
      AC_SUBST(BISON)
   fi
else
   OC_RULE_ADD(libocpf, LIBS)
fi



dnl ###########################################################################
dnl libmapiadmin dependencies
dnl ###########################################################################
PKG_CHECK_EXISTS([ dcerpc_samr ],
		[
			enable_libmapiadmin="yes"
		], [
			enable_libmapiadmin="no"
		])

if test x"$enable_libmapiadmin" = x"yes"; then
   	PKG_CHECK_MODULES(SAMR, dcerpc_samr)
	OC_RULE_ADD(libmapiadmin, LIBS)
fi



dnl ##########################################################################
dnl tools dependencies
dnl ##########################################################################

dnl --------------------------------------------------------------------------
dnl Check for libical                                                        
dnl --------------------------------------------------------------------------
PKG_CHECK_MODULES([ICAL], [libical], [have_libical="yes"], [have_libical="no"])
AC_SUBST(ICAL_CFLAGS)
AC_SUBST(ICAL_LIBS)

dnl --------------------------------------------------------------------------
dnl Check for popt
dnl --------------------------------------------------------------------------

AC_CHECK_LIB([popt], [poptFreeContext], 
             [
	       AC_DEFINE(HAVE_LIBPOPT, 1, [Define if you want to use libpopt])
	       enable_libpopt="yes"
             ], 
             [ 
               AC_MSG_WARN([libpopt is missing - can't build openchange tools]) 
	       enable_libpopt="no"
             ])

if test x"$enable_libpopt" = x"yes"; then
   	if test x"$enable_libmapiadmin" = x"yes"; then
	   OC_RULE_ADD(openchangepfadmin, TOOLS)
	   OC_RULE_ADD(mapitest, TOOLS)
	fi

	if test x"$enable_libocpf" = x"yes"; then
	   OC_RULE_ADD(openchangeclient, TOOLS)
	fi

	if test x"$have_libical" = x"yes"; then
	    OC_RULE_ADD(exchange2ical, TOOLS)
	fi

	OC_RULE_ADD(mapiprofile, TOOLS)
	OC_RULE_ADD(openchangemapidump, TOOLS)
	OC_RULE_ADD(schemaIDGUID, TOOLS)
	OC_RULE_ADD(locale_codepage, TOOLS)
fi

dnl --------------------------------------------------------------------------
dnl Check for libmagic
dnl --------------------------------------------------------------------------
AC_CHECK_LIB([magic], [magic_open],
	     [
               AC_DEFINE(HAVE_LIBMAGIC, 1, [Define if you want to use libmagic])
 	       MAGIC_LIBS="-lmagic -lz"
	       enable_libmagic="yes"
             ],
	       AC_SUBST(MAGIC_LIBS)
 	     [
               AC_MSG_WARN([libmagic is missing - can't build exchange2mbox])
	       enable_libmagic="no"
 	     ])

if test x"$enable_libmagic" = x"yes"; then
   	AC_CHECK_LIB([z], [gzopen], [], 
		     [
		       AC_MSG_ERROR([Z library not found, please install zlib-devel.], [1])
		     ])
   	if test x"$enable_libpopt" = x"yes"; then
		OC_RULE_ADD(exchange2mbox, TOOLS)
	fi
fi


dnl ##########################################################################
dnl libmapistore backends dependencies
dnl ##########################################################################

dnl --------------------------------------------------------------------------
dnl Check for sqlite3
dnl --------------------------------------------------------------------------
PKG_CHECK_MODULES(SQLITE, sqlite3, SQLITEFOUND=yes, [SQLITEFOUND=no])
AC_SUBST(SQLITE_CFLAGS)
AC_SUBST(SQLITE_LIBS)

if test x"$SQLITEFOUND" = x"yes"; then
   OC_RULE_ADD(mapistore_sqlite3, MAPISTORE)
else
   enable_mapistore_sqlite3="no"
fi


dnl ##########################################################################
dnl torture dependencies
dnl ##########################################################################
AC_PATH_PROG([SMBTORTURE], [smbtorture], no)

if test x"$SMBTORTURE" != x""; then
   	TORTURE_MODULESDIR=`$PKG_CONFIG --variable=modulesdir torture`
   	AC_SUBST(TORTURE_MODULESDIR)
   	OC_RULE_ADD(torture, TORTURE)
fi


dnl ##########################################################################
dnl mapiproxy server
dnl ##########################################################################
if test x$PYTHON != x; then
	OC_RULE_ADD(mapiproxy, SERVER)
fi

AC_ARG_WITH(modulesdir, 
[AS_HELP_STRING([--with-modulesdir], [Modules path to use])],
[modulesdir="$withval"; ],
[modulesdir="\${prefix}/modules"; ])

AC_SUBST(modulesdir)

dnl ##########################################################################
dnl Swig bindings dependencies
dnl ##########################################################################
AC_ARG_ENABLE(swig-perl, AC_HELP_STRING([--enable-swig-perl],
			   [build SWIG interfaces for Perl]),
			   enable_perlswig="$enableval")
if test "x${enable_perlswig}" = xyes; then
   AC_PATH_PROG(SWIG, swig)

   if test -z "$SWIG"
      then
	AC_MSG_ERROR(Please install swig)
   fi

   SWIGDIRSALL="swigperl-all"
   SWIGDIRSINSTALL="swigperl-install"
   SWIGDIRSUNINSTALL="swigperl-uninstall"
fi

PERL5DIR=`$PERL -e 'use Config; my $dir = $Config{sitelib}; print $dir'`
AC_SUBST(PERL5DIR)

AC_SUBST(SWIGDIRSALL)
AC_SUBST(SWIGDIRSINSTALL)
AC_SUBST(SWIGDIRSUNINSTALL)

dnl ##########################################################################
dnl Python bindings dependencies
dnl ##########################################################################
AC_ARG_ENABLE(pymapi, AC_HELP_STRING([--enable-pymapi],
			   [build Python bindings for libmapi]),
			   enable_pymapi="$enableval",
			   enable_pymapi=no)
if test "x${enable_pymapi}" = xyes; then
   PYMAPIALL="pymapi"
   PYMAPIINSTALL="pymapi-install"
   PYMAPIUNINSTALL="pymapi-uninstall"
fi

PYCDIR=`$PYTHON -c "import distutils.sysconfig; print distutils.sysconfig.get_python_lib(1, prefix='\\$(prefix)')"`
AC_SUBST(PYCDIR)

AC_SUBST(PYMAPIALL)
AC_SUBST(PYMAPIINSTALL)
AC_SUBST(PYMAPIUNINSTALL)

dnl ##########################################################################
dnl Documentation dependencies
dnl ##########################################################################
AC_PATH_PROG(DOXYGEN, doxygen)
if test x"$DOXYGEN" = x""; then
	AC_MSG_WARN(doxygen)
	enable_doxygen="no"
else
	enable_doxygen="yes"
	AC_SUBST(DOXYGEN)
fi

dnl ***********************
dnl Makefiles 
dnl ***********************
AC_CONFIG_FILES([config.mk libmapi.pc libmapiadmin.pc libocpf.pc mapiproxy/libmapiproxy.pc
		 mapiproxy/libmapiserver.pc mapiproxy/libmapistore.pc Doxyfile libmapi++/Doxyfile 
		 libocpf/Doxyfile libmapiadmin/Doxyfile libmapi/Doxyfile mapiproxy/Doxyfile 
		 utils/mapitest/Doxyfile])
AC_OUTPUT


dnl ##########################################################################
dnl Print configuration info
dnl ##########################################################################

OC_SETVAL(libmapi)
OC_SETVAL(libmapiadmin)
OC_SETVAL(libocpf)
OC_SETVAL(libmapixx)

OC_SETVAL(openchangeclient)
OC_SETVAL(mapiprofile)
OC_SETVAL(openchangepfadmin)
OC_SETVAL(exchange2mbox)
OC_SETVAL(exchange2ical)
OC_SETVAL(mapitest)
OC_SETVAL(openchangemapidump)
OC_SETVAL(schemaIDGUID)
OC_SETVAL(locale_codepage)
OC_SETVAL(mapiproxy)

OC_SETVAL(torture)
OC_SETVAL(doxygen)
OC_SETVAL(perlswig)

AC_MSG_RESULT([

===============================================================
OpenChange Configuration (Please review)

	   * Install:
	     - prefix:			$prefix

	   * OpenChange MAPI library:	$enable_libmapi

	   * OpenChange Libraries:
	     - libmapi++:		$enable_libmapixx
	     - libmapiadmin:		$enable_libmapiadmin
	     - libocpf:			$enable_libocpf

	   * OpenChange Server:
	     - mapiproxy:		$enable_mapiproxy

	   * OpenChange mapistore backends:
	     - sqlite3:			$enable_mapistore_sqlite3

	   * OpenChange Tools:
	     - openchangeclient:	$enable_openchangeclient
	     - mapiprofile:		$enable_mapiprofile
	     - openchangepfadmin:	$enable_openchangepfadmin
	     - exchange2mbox:		$enable_exchange2mbox
	     - exchange2ical:		$enable_exchange2ical
	     - mapitest:		$enable_mapitest
	     - openchangemapidump:	$enable_openchangemapidump
	     - schemaIDGUID:		$enable_schemaIDGUID
	     - locale_codepage:		$enable_locale_codepage

	   * OpenChange Torture Suite:	$enable_torture

	   * OpenChange Documentation:	$enable_doxygen

	   * Coverage Tests:		$use_cov

	   * OpenChange Bindings:
	     - Perl:			$enable_perlswig
	     - Python:			$enable_pymapi


===============================================================

])

