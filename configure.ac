# Simple configuration script for OpenChange
# Written by Jelmer Vernooij <jelmer@openchange.org>

AC_PREREQ(2.57)
AC_INIT(openchange, 2.2, [openchange@openchange.org])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE
AC_DEFINE(_GNU_SOURCE, 1, [Use GNU extensions])

PKG_PROG_PKG_CONFIG([0.20])

if test -z "$CFLAGS";
then
  CFLAGS="-g";
fi

CFLAGS="-I. $CFLAGS"

dnl #################################################################
dnl MySQL
dnl #################################################################
AC_PATH_PROG(MYSQL_CONFIG, mysql_config)
if test x"$MYSQL_CONFIG" = x""; then
	AC_MSG_WARN([No version of mysql_config was found!])
	AC_MSG_ERROR([Please install libmysqlclient-dev])
fi
AC_SUBST(MYSQL_CONFIG)

AC_MSG_CHECKING(mysql cflags)
MYSQL_CFLAGS=`$MYSQL_CONFIG --cflags`
AC_MSG_RESULT($MYSQL_CFLAGS)
AC_SUBST(MYSQL_CFLAGS)

AC_MSG_CHECKING(mysql libs)
MYSQL_LIBS=`$MYSQL_CONFIG --libs`
AC_MSG_RESULT($MYSQL_LIBS)
AC_SUBST(MYSQL_LIBS)

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
	PKG_CONFIG_PATH="$1/lib64/pkgconfig:$1/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
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
	if test -n "${sambaprefix:-}"; then
	break
	fi
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
if test "x${$1}set" != "xset"; then
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
	TOOLS)
		OC_$2="$OC_$2 $1"
		OC_$2_INSTALL="$OC_$2_INSTALL $1-install"
   		OC_$2_UNINSTALL="$OC_$2_UNINSTALL $1-uninstall"
	;;
	TESTSUITE)
		OC_$2="$OC_$2 $1"
		OC_$2_INSTALL="$OC_$2_INSTALL $1-install"
   		OC_$2_UNINSTALL="$OC_$2_UNINSTALL $1-uninstall"
		OC_$2_CHECK="$OC_$2_CHECK $1-check"

		AC_SUBST(OC_$2_CHECK)
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
AC_CHECK_FUNCS(strcasestr)



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
dnl Check for check (unittest library)
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(CHECK, check, [testsuite=1], [AC_MSG_WARN(Conditions unmet for unit testing support)])
AC_SUBST(CHECK_CFLAGS)
AC_SUBST(CHECK_LIBS)
OC_RULE_ADD(testsuite, TESTSUITE)

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
    COMPILER_OPTIONS_SHARED="-Wall -fstrict-aliasing -Wmissing-prototypes -Wstrict-prototypes -wd2259,188,593,869,981,181,1419,2218"
    COMPILER_OPTIONS_C="$COMPILER_OPTIONS_SHARED"
    COMPILER_OPTIONS_CXX="$COMPILER_OPTIONS_SHARED"
else
dnl GNU Compiler
    COMPILER_OPTIONS_SHARED="-Wall -fstrict-aliasing -Wp,-D_FORTIFY_SOURCE=2"
    if test "x$use_cov" = "xyes"; then
        COMPILER_OPTIONS_SHARED="-O0 $COMPILER_OPTIONS_SHARED"
    else
        COMPILER_OPTIONS_SHARED="$COMPILER_OPTIONS_SHARED"
    fi
    COMPILER_OPTIONS_C="$COMPILER_OPTIONS_SHARED -Wmissing-prototypes -Wstrict-prototypes"
    if test "x$use_cov" = "xyes"; then
        COMPILER_OPTIONS_C="$COMPILER_OPTIONS_C -fprofile-arcs -ftest-coverage"
        LDFLAGS="$LDFLAGS -lgcov --coverage"
    fi
    COMPILER_OPTIONS_CXX="$COMPILER_OPTIONS_SHARED"
    if test "x$use_cov" = "xyes"; then
        COMPILER_OPTIONS_CXX="$COMPILER_OPTIONS_CXX -fprofile-arcs -ftest-coverage"
        LDFLAGS="$LDFLAGS -lgcov --coverage"
    fi
fi
AC_SUBST(COMPILER_OPTIONS_C)
AC_SUBST(COMPILER_OPTIONS_CXX)

dnl ---------------------------------------------------------------------------
dnl Define an assert macro if this is a development release
dnl ---------------------------------------------------------------------------
. `dirname $0`/VERSION
AC_SUBST(OPENCHANGE_VERSION_IS_GIT_SNAPSHOT)

if test x"$OPENCHANGE_VERSION_IS_GIT_SNAPSHOT" = x"yes"; then
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

AC_PATH_PROG(PYTHON, python)
if test x"$PYTHON" = x""; then
	AC_MSG_WARN([No version of python was found!])
	AC_MSG_ERROR([Please install python])
fi
AC_SUBST(PYTHON)

AC_PATH_PROG(PYTHON_CONFIG, python-config)
if test x"$PYTHON_CONFIG" = x""; then
	AC_MSG_WARN([No version of python-config was found!])
	AC_MSG_ERROR([Please install python-dev 2.7])
fi
AC_SUBST(PYTHON_CONFIG)

AC_MSG_CHECKING(python cflags)
PYTHON_CFLAGS=`$PYTHON_CONFIG --cflags`
AC_MSG_RESULT($PYTHON_CFLAGS)
AC_SUBST(PYTHON_CFLAGS)

AC_MSG_CHECKING(python libs)
PYTHON_LIBS=`$PYTHON_CONFIG --libs`
AC_MSG_RESULT($PYTHON_LIBS)
AC_SUBST(PYTHON_LIBS)

AC_MSG_CHECKING(python library directory)
pythondir=`$PYTHON -c "from distutils import sysconfig; print sysconfig.get_python_lib(1, 0, '\\${prefix}')"`
AC_MSG_RESULT($pythondir)
AC_SUBST(pythondir)

dnl ----------------------------------------------------------------------------
dnl Check for MYSQL client libraries
dnl ----------------------------------------------------------------------------
AC_PATH_PROG(MYSQL_CONFIG, mysql_config)
if test x"$MYSQL_CONFIG" = x""; then
   AC_MSG_WARN([No version of mysql_config was found!])
   AC_MSG_ERROR([Please install libmysqlclient-dev])
fi
AC_SUBST(MYSQL_CONFIG)

AC_MSG_CHECKING(mysql cflags)
MYSQL_CFLAGS=`$MYSQL_CONFIG --cflags`
AC_MSG_RESULT($MYSQL_CFLAGS)
AC_SUBST(MYSQL_CFLAGS)

AC_MSG_CHECKING(mysql libs)
MYSQL_LIBS=`$MYSQL_CONFIG --libs`
AC_MSG_RESULT($MYSQL_LIBS)
AC_SUBST(MYSQL_LIBS)

dnl ----------------------------------------------------------------------------
dnl Check for Flex
dnl ----------------------------------------------------------------------------
if test x"$OPENCHANGE_VERSION_IS_GIT_SNAPSHOT" = x"yes"; then
   AC_ARG_VAR([FLEX], [Location of the flex program.])
   AC_CHECK_PROG([FLEX], [flex], [flex])

   if test x"$FLEX" = x""; then
      AC_MSG_WARN([No version of flex was found!])
      AC_MSG_ERROR([Please install flex])
   fi

   AC_SUBST(FLEX)
fi

dnl ---------------------------------------------------------------------------
dnl Samba4 modules
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(TALLOC, talloc)
PKG_CHECK_MODULES(SAMBA, dcerpc ndr samba-hostconfig samba-util tevent samba-credentials)
PKG_CHECK_MODULES(SAMBASERVER, dcerpc_server samdb, [], [AC_MSG_WARN(No mapiproxy support)])
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

AC_MSG_CHECKING([for DCE/RPC server modules directory])
DCERPC_SERVER_MODULESDIR=`$PKG_CONFIG --variable=modulesdir dcerpc_server`
AC_SUBST(DCERPC_SERVER_MODULESDIR)
AC_MSG_RESULT($DCERPC_SERVER_MODULESDIR)

dnl ---------------------------------------------------------------------------
dnl Finally add libmapi to the library list
dnl ---------------------------------------------------------------------------
if test "x$SAMBA_LIBS" != "x" ; then
    libmapi=1
fi
OC_RULE_ADD(libmapi, LIBS)

dnl nasty hack: only compile IDL if we have a library
dnl libraries require libmapi and libmapi require IDL
OC_IDL="idl"
AC_SUBST(OC_IDL)

dnl ---------------------------------------------------------------------------
dnl Check for dl libs
dnl ---------------------------------------------------------------------------

AC_CHECK_LIB([dl], [dlopen],
	    [
		 	DL_LIBS="-ldl"
			AC_SUBST(DL_LIBS)
		],
		[
			AC_CHECK_FUNCS(dlopen, [
				DL_LIBS=""
				AC_SUBST(DL_LIBS)
			], [
				AC_MSG_ERROR([unable to find dlopen])
		 	])
		])

dnl ---------------------------------------------------------------------------
dnl Check for pthread libs
dnl ---------------------------------------------------------------------------
enable_pthread=no
enable_thread_lib=""
AC_CHECK_LIB([pthread], [pthread_mutex_init],
	     [
		AC_DEFINE(HAVE_PTHREADS, 1, [Define if you want to use pthread for thread safety])
		THREAD_LIBS=-lpthread
	     	enable_pthread="yes"
		enable_thread_lib="(pthread)"
	     ],
	     [
	     AC_MSG_WARN([pthread is missing - library is not thread safe])
	     enable_pthread="no"
	     ])

if test x"$enable_pthread" = x"no"; then
	dnl Check for gthread-2.0 / glib based implementation
   	PKG_CHECK_MODULES(GTHREAD, gthread-2.0, 
		[
			AC_DEFINE(HAVE_GTHREAD, 1, [Define if you want to use gthread for thread safety])
			THREAD_LIBS=$GTHREAD_LIBS
			THREAD_CFLAGS=$GTHREAD_CFLAGS
			enable_pthread=yes
			enable_thread_lib="(gthread)"
		], 
		[
			enable_pthread=no
		])
fi

AC_SUBST(THREAD_LIBS)
AC_SUBST(THREAD_CFLAGS)

dnl Additional thread library detection is required here:
dnl FreeBSD (pthreads), glib etc.

dnl ###########################################################################
dnl libmapi++ dependencies
dnl ###########################################################################

dnl ---------------------------------------------------------------------------
dnl Check for g++
dnl ---------------------------------------------------------------------------
AC_CACHE_CHECK([C++ compiler availability], [ac_cv_libmapixx_gxx_works],
	       [
		AC_LANG_PUSH([C++])
		AC_COMPILE_IFELSE([AC_LANG_SOURCE([[int main() { return 0; }]])], 
				  [ac_cv_libmapixx_gxx_works=yes],
				  [ac_cv_libmapixx_gxx_works=no])
		AC_LANG_POP([C++])
		])

if test x"$ac_cv_libmapixx_gxx_works" = "xyes"; then
   AC_PROG_CXX 
   libmapixx=1
   OC_RULE_ADD(libmapixx, LIBS)
fi


dnl ###########################################################################
dnl libocpf dependencies
dnl ###########################################################################

dnl ---------------------------------------------------------------------------
dnl Check for Bison
dnl ---------------------------------------------------------------------------
if test x"$OPENCHANGE_VERSION_IS_GIT_SNAPSHOT" = x"yes"; then
   AC_ARG_VAR([BISON], [Location of the bison program.])
   AC_PATH_PROG([BISON], [bison], [bison])

   if test x"$BISON" != x""; then
      libocpf=1
      OC_RULE_ADD(libocpf, LIBS)
      AC_SUBST(BISON)
   fi
else
   libocpf=1
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
   	PKG_CHECK_MODULES(SAMR, dcerpc_samr ndr_standard)
	libmapiadmin=1
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
PKG_CHECK_MODULES([ICAL_0_46], [libical >= 0.46], [have_libical_0_46="1"], [have_libical_0_46="0"])
AC_SUBST(have_libical_0_46)

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
	   openchangepfadmin=1
	   mapitest=1
	fi

	if test x"$enable_libocpf" = x"yes"; then
	   openchangeclient=1
	fi

	if test x"$have_libical" = x"yes"; then
	    exchange2ical=1
	fi

	MAPISTORE_TEST=mapistore_test
	mapiprofile=1
	openchangemapidump=1
	schemaIDGUID=1
	check_fasttransfer=1
	test_asyncnotif=1
fi
AC_SUBST(MAPISTORE_TEST)
OC_RULE_ADD(openchangeclient, TOOLS)
#OC_RULE_ADD(mapistore_fsocpf, MAPISTORE)
OC_RULE_ADD(exchange2ical, TOOLS)
OC_RULE_ADD(openchangepfadmin, TOOLS)
OC_RULE_ADD(mapitest, TOOLS)
OC_RULE_ADD(mapiprofile, TOOLS)
OC_RULE_ADD(openchangemapidump, TOOLS)
OC_RULE_ADD(schemaIDGUID, TOOLS)

OC_RULE_ADD(check_fasttransfer, TOOLS)
OC_RULE_ADD(test_asyncnotif, TOOLS)

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
		exchange2mbox=1
	fi
fi
OC_RULE_ADD(exchange2mbox, TOOLS)

dnl ##########################################################################
dnl libmapistore dependencies
dnl ##########################################################################

AC_CHECK_LIB([rt], [mq_open],
	    [
		 	RT_LIBS="-lrt"
			AC_SUBST(RT_LIBS)
		],
		[
			AC_CHECK_FUNCS(mq_open, [
				RT_LIBS=""
				AC_SUBST(RT_LIBS)
			], [
				AC_MSG_ERROR([unable to find mq_open])
		 	])
		])

dnl ##########################################################################
dnl libmapistore backends dependencies
dnl ##########################################################################

OC_MAPISTORE=
OC_MAPISTORE_CLEAN=
OC_MAPISTORE_INSTALL=
OC_MAPISTORE_UNINSTALL=
AC_SUBST(OC_MAPISTORE)
AC_SUBST(OC_MAPISTORE_CLEAN)
AC_SUBST(OC_MAPISTORE_INSTALL)
AC_SUBST(OC_MAPISTORE_UNINSTALL)

OC_SERVER=
OC_SERVER_CLEAN=
OC_SERVER_INSTALL=
OC_SERVER_UNINSTALL=
AC_SUBST(OC_SERVER)
AC_SUBST(OC_SERVER_CLEAN)
AC_SUBST(OC_SERVER_INSTALL)
AC_SUBST(OC_SERVER_UNINSTALL)

dnl ##########################################################################
dnl mapiproxy server
dnl ##########################################################################
if test x$PYTHON != x; then
	if test "x$SAMBASERVER_LIBS" != x ; then
		mapiproxy=1
	fi
fi
OC_RULE_ADD(mapiproxy, SERVER)

AC_ARG_WITH(modulesdir, 
[AS_HELP_STRING([--with-modulesdir], [Modules path to use])],
[modulesdir="$withval"; ],
[modulesdir="\${prefix}/modules"; ])

AC_SUBST(modulesdir)

dnl ##########################################################################
dnl Notifications service
dnl ##########################################################################
PKG_CHECK_MODULES(SSL, libssl)
PKG_CHECK_MODULES(BSD, libbsd)
PKG_CHECK_MODULES(RABBITMQ, librabbitmq >= 0.4.1)
PKG_CHECK_MODULES(JSONC, json-c >= 0.11)
PKG_CHECK_MODULES(CONFIG, libconfig >= 1.4.8)

OC_NOTIF=ocnotification
OC_NOTIF_CLEAN=ocnotification-clean
OC_NOTIF_INSTALL=ocnotification-install
OC_NOTIF_UNINSTALL=ocnotification-uninstall
AC_SUBST(OC_NOTIF)
AC_SUBST(OC_NOTIF_CLEAN)
AC_SUBST(OC_NOTIF_INSTALL)
AC_SUBST(OC_NOTIF_UNINSTALL)

dnl ##########################################################################
dnl Python bindings dependencies
dnl ##########################################################################
AC_ARG_ENABLE(pyopenchange, AC_HELP_STRING([--enable-pyopenchange],
			   [build Python bindings for libmapi]),
			   enable_pyopenchange="$enableval",
			   enable_pyopenchange=no)
PYCDIR=`$PYTHON -c "import distutils.sysconfig; print distutils.sysconfig.get_python_lib(1, prefix='\\$(prefix)')"`
AC_SUBST(PYCDIR)

PYOPENCHANGEALL=
PYOPENCHANGEINSTALL=
PYOPENCHANGEUNINSTALL=

if test x$enable_pyopenchange = xyes ; then
	dnl Enable pyopenchange by default
	PYOPENCHANGEALL="$PYOPENCHANGEALL pyopenchange"
	PYOPENCHANGEINSTALL="$PYOPENCHANGEALLINSTALL pyopenchange-install"
	PYOPENCHANGEUNINSTALL="$PYOPENCHANGEUNINSTALL pyopenchange-uninstall"
fi

AC_SUBST(PYOPENCHANGEALL)
AC_SUBST(PYOPENCHANGEINSTALL)
AC_SUBST(PYOPENCHANGEUNINSTALL)


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

dnl ##########################################################################
dnl (Optional) subunit support in mapitest
dnl ##########################################################################
PKG_CHECK_MODULES(SUBUNIT, libsubunit, found_subunit=yes, found_subunit=no)
AC_SUBST(SUBUNIT_CFLAGS)
AC_SUBST(SUBUNIT_LIBS)

if test x"$found_subunit" = x"yes"; then
	AC_DEFINE(HAVE_SUBUNIT, 1, [Define if you want to use subunit])
	have_subunit="yes"
else
	have_subunit="no"
fi

dnl ##########################################################################
dnl Qt4 support
dnl ##########################################################################
AC_ARG_ENABLE(openchange-qt4,
              AC_HELP_STRING([--enable-openchange-qt4],
                             [Compile OpenChange Qt4 wrapper.]),
              enable_openchange_qt4=$enableval,
              enable_openchange_qt4="no")
if test x$enable_openchange_qt4 = xyes; then
  PKG_CHECK_MODULES(Qt4,
                    QtCore >= 4.3.0 QtGui >= 4.3.0)
  MOC=`$PKG_CONFIG --variable=moc_location QtCore`
elif test x$enable_openchange_qt4 = xtry; then
  PKG_CHECK_MODULES(Qt4,
                    QtCore >= 4.3.0 QtGui >= 4.3.0,
                    [enable_openchange_qt4="yes"
                     MOC=`$PKG_CONFIG --variable=moc_location QtCore`],
                    [enable_openchange_qt4="no"])
fi

AC_SUBST(Qt4_CFLAGS)
AC_SUBST(Qt4_LIBS)
AC_SUBST(MOC)

if test "x$enable_openchange_qt4" = "xyes"; then
   OPENCHANGE_QT4="openchange_qt4"
fi
AC_SUBST(OPENCHANGE_QT4)

dnl ***********************
dnl Makefiles 
dnl ***********************
AC_CONFIG_FILES([config.mk libmapi.pc libmapiadmin.pc libocpf.pc mapiproxy/libmapiproxy.pc
		 mapiproxy/libmapiserver.pc mapiproxy/libmapistore.pc libmapi++.pc
		 Doxyfile libmapi++/Doxyfile libocpf/Doxyfile libmapiadmin/Doxyfile
		 libmapi/Doxyfile mapiproxy/Doxyfile utils/mapitest/Doxyfile
		 mapiproxy/libmapistore/Doxyfile])
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
OC_SETVAL(mapiproxy)

OC_SETVAL(testsuite)

OC_SETVAL(doxygen)

AC_MSG_RESULT([

===============================================================
OpenChange Configuration (Please review)

	   * OpenChange Libraries:
	     - libmapi (C library):	$enable_libmapi
		      Thread support:	$enable_pthread $enable_thread_lib
	     - libmapi++ (C++ library):	$enable_libmapixx
	     - libmapiadmin:		$enable_libmapiadmin
	     - libocpf:			$enable_libocpf

	   * OpenChange Server:
	     - mapiproxy:		$enable_mapiproxy

	   * OpenChange mapistore backends:
	     - backends dependencies goes here

	   * OpenChange Tools:
	     - openchangeclient:	$enable_openchangeclient
	     - mapiprofile:		$enable_mapiprofile
	     - openchangepfadmin:	$enable_openchangepfadmin
	     - exchange2mbox:		$enable_exchange2mbox
	     - exchange2ical:		$enable_exchange2ical
	     - openchangemapidump:	$enable_openchangemapidump
	     - schemaIDGUID:		$enable_schemaIDGUID

	   * Unit and functional testing
	     - mapitest:		$enable_mapitest
	     - mapitest (subunit):      $have_subunit
	     - openchange-testsuite:    $enable_testsuite
	     - coverage tests:		$use_cov

	   * OpenChange Documentation:	$enable_doxygen

	   * OpenChange Bindings:
	     - Qt4:			$enable_openchange_qt4

	   * Installation prefix:	$prefix

===============================================================

])

