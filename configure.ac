# Simple configuration script for OpenChange
# Written by Jelmer Vernooij <jelmer@samba.org>

AC_PREREQ(2.57)
AC_INIT(openchange, 0.1, [openchange@openchange.org])
AC_CONFIG_HEADER([config.h])
AC_DEFINE(_GNU_SOURCE, 1, [Use GNU extensions])

AC_PROG_CC

dnl ***********************
dnl Check for PERL
dnl ***********************

AC_PATH_PROG(PERL, perl)

if test -z "$PERL"
then
	AC_MSG_ERROR(Please install perl)
fi

dnl ***********************
dnl Check for Pidl
dnl ***********************

AC_PATH_PROG(PIDL, pidl)

if test -z "$PIDL"
then
	AC_MSG_ERROR(Please install pidl)
fi

dnl *****************************
dnl Check for libmagic
dnl *****************************
AC_CHECK_LIB([magic], [magic_open],
	             [AC_DEFINE(HAVE_LIBMAGIC, 1,
 	                       [Define if you want to use libmagic])
 	              LIB_MAGIC="-lmagic -lz"],
 	             [AC_MSG_ERROR([libmagic is missing - install it please])
 	             ])

dnl *****************************
dnl Check for samba dependencies 
dnl *****************************

AC_CHECK_LIB(popt, poptFreeContext, , [ AC_MSG_ERROR([Popt is required]) ])

CPPFLAGS="-I$prefix/include"

PKG_PROG_PKG_CONFIG([0.20])
PKG_CHECK_MODULES(TALLOC, talloc)
PKG_CHECK_MODULES(SAMBA, dcerpc ndr samba-config)
PKG_CHECK_MODULES(LDB, ldb tdb)

TORTURE_MODULESDIR=`$PKG_CONFIG --variable=modulesdir torture`
AC_SUBST(TORTURE_MODULESDIR)

AC_CHECK_TYPE(uint_t, unsigned int)

dnl ***********************
dnl Makefiles 
dnl ***********************

AC_CONFIG_FILES([Makefile libmapi.pc])
AC_OUTPUT

dnl ***********************
dnl Output 
dnl ***********************

