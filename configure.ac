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

PKG_PROG_PKG_CONFIG([0.20])
PKG_CHECK_MODULES(TALLOC, talloc)
PKG_CHECK_MODULES(SAMBA, dcerpc ndr tdr samba-config cli_smb)
PKG_CHECK_MODULES(LDB, ldb)

TORTURE_MODULESDIR=`$PKG_CONFIG --variable=modulesdir torture`
AC_SUBST(TORTURE_MODULESDIR)

AC_CHECK_TYPE(uint_t, unsigned int)

dnl ****************************
dnl Check for evolution plugins 
dnl ****************************

AC_ARG_WITH(evolution, [  --evolution=[version] Use Evolution version.],
			evolution_version="$withval", evolution_version="2.8")	

AC_ARG_WITH(evolution-camelprovider, [  --evolution-camel=[version] Use Evolution Camel Provider version.],
				   evolution_camel_version="$withval", evolution_camel_version="1.2" )

AC_ARG_ENABLE(client_evolution, [  --enable-evolution-client=[yes,no] Build Evolution plugins.],
                       enable_evolution_client="$enableval", enable_evolution_client="no")

if test "x${enable_evolution_client}" = "xyes"; then
	dnl Check for dependencies

	PKG_CHECK_MODULES(EVOLUTION_PLUGIN, evolution-plugin-${evolution_version} evolution-data-server-${evolution_camel_version} camel-provider-${evolution_camel_version})

	GETTEXT_PACKAGE=evolution
	AC_SUBST(GETTEXT_PACKAGE)
	AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Package Name for Gettext])

	dnl Add evolution plugin rules here
	EVOLUTION_PLUGIN="evolution-plugin-all"
	AC_SUBST(EVOLUTION_PLUGIN)

	EVOLUTION_PLUGIN_INSTALL="evolution-plugin-install"
	AC_SUBST(EVOLUTION_PLUGIN_INSTALL)

	EVOLUTION_PLUGIN_UNINSTALL="evolution-plugin-uninstall"
	AC_SUBST(EVOLUTION_PLUGIN_UNINSTALL)

	plugindir=`$PKG_CONFIG --variable=plugindir evolution-plugin-${evolution_version}`
	AC_SUBST(plugindir)

	camel_providerdir=`$PKG_CONFIG --variable=camel_providerdir camel-provider-${evolution_camel_version}`
	AC_SUBST(camel_providerdir)
fi


dnl ***********************
dnl Makefiles 
dnl ***********************

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

dnl ***********************
dnl Output 
dnl ***********************

