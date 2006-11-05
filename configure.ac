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
dnl Check for samba dependencies 
dnl *****************************

AC_CHECK_LIB(popt, poptFreeContext, , [ AC_MSG_ERROR([Popt is required]) ])

CPPFLAGS="-I$prefix/include"
AC_CHECK_HEADERS(torture/ui.h param/share.h param/share_proto.h, , [ AC_MSG_ERROR([Install this header file from Samba4 manually])])

PKG_PROG_PKG_CONFIG([0.20])
PKG_CHECK_MODULES(TALLOC, talloc)
PKG_CHECK_MODULES(SAMBA, dcerpc ndr tdr samba-util samba-config)
PKG_CHECK_MODULES(LDB, ldb)

TORTURE_MODULESDIR=`$PKG_CONFIG --variable=modulesdir torture`
AC_SUBST(TORTURE_MODULESDIR)

AC_CHECK_TYPE(uint_t, unsigned int)

dnl ****************************
dnl Check for evolution plugins 
dnl ****************************

AC_ARG_WITH(evolution, [  --evolution=[version] Use Evolution version.],
			evolution_version="$withval", evolution_version="2.6")	

AC_ARG_ENABLE(eplugins, [  --enable-eplugins=[yes,no] Build Evolution plugins.],
                       enable_eplugins="$enableval", enable_eplugins="no")

if test "x${enable_eplugins}" = "xyes"; then
	dnl Check for dependencies

	PKG_CHECK_MODULES(EPLUGINS, evolution-plugin-${evolution_version})

	dnl Add evolution plugin rules here
	EPLUGINS="eplugins-all"
	AC_SUBST(EPLUGINS)

	EPLUGINS_INSTALL="eplugins-install"
	AC_SUBST(EPLUGINS_INSTALL)

	EPLUGINS_UNINSTALL="eplugins-uninstall"
	AC_SUBST(EPLUGINS_UNINSTALL)

	plugindir=`$PKG_CONFIG --variable=plugindir evolution-plugin-${evolution_version}`
	AC_SUBST(plugindir)
fi


dnl ***********************
dnl Makefiles 
dnl ***********************

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

dnl ***********************
dnl Output 
dnl ***********************

