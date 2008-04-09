# Simple configuration script for OpenChange
# Written by Jelmer Vernooij <jelmer@samba.org>

AC_PREREQ(2.57)
AC_INIT(openchange, 0.7, [openchange@openchange.org])
AC_CONFIG_HEADER([config.h])
AC_DEFINE(_GNU_SOURCE, 1, [Use GNU extensions])

AC_ARG_WITH(samba, 
[AC_HELP_STRING([--with-samba], [Override location Samba is installed])],
[ 
 PKG_CONFIG_PATH="$withval/lib/pkgconfig"
 PATH="$withval/bin:$PATH"
 sambaprefix="$withval"
],[
 sambaprefix=$prefix
 if test x"$prefix" = x"NONE"; then
   sambaprefix=/usr/local/samba
fi
])
AC_SUBST(sambaprefix)

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
# OC_RULE_ADD([NAME], [TYPE])
# ---------------------------
AC_DEFUN([OC_RULE_ADD], 
[ 
AC_ARG_VAR([NAME], [rule name])
AC_ARG_VAR([TYPE], [rule type])
if test "x$1_set" != "xset"; then
   case "$2" in
   	LIBS)
		OC_$2+=" $1"
   		OC_$2_INSTALL+=" $1-install"
   		OC_$2_UNINSTALL+=" $1-uninstall"
   		OC_$2_INSTALLPC+=" $1-installpc"
   		OC_$2_INSTALLHEADER+=" $1-installheader"
   		OC_$2_INSTALLLIB+=" $1-installlib"

		AC_SUBST(OC_$2_INSTALLPC)
		AC_SUBST(OC_$2_INSTALLHEADER)
		AC_SUBST(OC_$2_INSTALLLIB)
	;;
	TOOLS|TORTURE)
		OC_$2+=" $1"
		OC_$2_INSTALL+=" $1-install"
   		OC_$2_UNINSTALL+=" $1-uninstall"
	;;
	SERVER)
		OC_$2+=" $1"
		OC_$2_INSTALL+=" $1-install"
   		OC_$2_UNINSTALL+=" $1-uninstall"
	;;
   esac

   AC_SUBST(OC_$2)
   AC_SUBST(OC_$2_INSTALL)
   AC_SUBST(OC_$2_UNINSTALL)

   enable_$1="yes"

fi[]
])



dnl ###########################################################################
dnl libmapi and required dependencies
dnl ###########################################################################

AC_CHECK_TYPE(uint_t, unsigned int)

dnl ---------------------------------------------------------------------------
dnl Check for CC
dnl ---------------------------------------------------------------------------
AC_PROG_CC

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

dnl ----------------------------------------------------------------------------
dnl Check for Flex
dnl ----------------------------------------------------------------------------
AC_ARG_VAR([FLEX], [Location of the flex program.])
AC_CHECK_PROG([FLEX], [flex], [flex])

if test x"$FLEX" = x""; then
   	AC_MSG_WARN([No version of flex was found!])
	AC_MSG_ERROR([Please install flex])
fi
AC_SUBST(FLEX)

dnl ---------------------------------------------------------------------------
dnl Check a particular Samba4 git revision
dnl ---------------------------------------------------------------------------

INCDIR=$sambaprefix/include

AC_CHECK_FILE($INCDIR/samba/version.h,,AC_MSG_ERROR([Could not find Samba4 headers. Re-run ./configure with --prefix=XXX where
 XXX is the same prefix that Samba4 was configured with.]))

SAMBA_VERSION=`cat $INCDIR/samba/version.h | grep SAMBA_VERSION_STRING | grep -v define | cut -d"=" -f2`

if test "$SAMBA_VERSION" != "4.0.0alpha4-GIT-7fccd85" ; then
   AC_MSG_WARN([The Samba4 version installed on your system doesn't meet OpenChange requirements (7fccd85).])
fi


dnl ---------------------------------------------------------------------------
dnl Samba4 modules
dnl ---------------------------------------------------------------------------
PKG_PROG_PKG_CONFIG([0.20])
PKG_CHECK_MODULES(TALLOC, talloc)
PKG_CHECK_MODULES(SAMBA, dcerpc ndr samba-hostconfig)
PKG_CHECK_MODULES(LDB, ldb)

dnl ---------------------------------------------------------------------------
dnl Finally add libmapi to the library list
dnl ---------------------------------------------------------------------------
OC_RULE_ADD(libmapi, LIBS)

dnl nasty hack: only compile IDL if we have a library
dnl libraries require libmapi and libmapi require IDL
OC_IDL="idl"
AC_SUBST(OC_IDL)



dnl ###########################################################################
dnl libocpf dependencies
dnl ###########################################################################

dnl ---------------------------------------------------------------------------
dnl Check for Bison
dnl ---------------------------------------------------------------------------
AC_ARG_VAR([BISON], [Location of the bison program.])
AC_PATH_PROG([BISON], [bison], [bison])

if test x"$BISON" != x""; then
   OC_RULE_ADD(libocpf, LIBS)
   AC_SUBST(BISON)
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
   	if test x"$enable_libpopt" = x"yes"; then
	   OC_RULE_ADD(exchange2mbox, TOOLS)
	fi
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
dnl Server compilation
dnl ##########################################################################
OC_RULE_ADD(server, SERVER)


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

   SWIGDIRSALL+="swigperl-all"
   SWIGDIRSINSTALL+="swigperl-install"
   SWIGDIRSUNINSTALL+="swigperl-uninstall"
fi

PERL5DIR=`$PERL -e 'use Config; my $dir = $Config{sitelib}; print $dir'`
AC_SUBST(PERL5DIR)

AC_SUBST(SWIGDIRSALL)
AC_SUBST(SWIGDIRSINSTALL)
AC_SUBST(SWIGDIRSUNINSTALL)



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
AC_CONFIG_FILES([config.mk swig/perl/Makefile libmapi.pc libmapiadmin.pc 
		 libocpf.pc Doxyfile libmapi/Doxyfile libocpf/Doxyfile])
AC_OUTPUT


dnl ##########################################################################
dnl Print configuration info
dnl ##########################################################################

OC_SETVAL(libmapi)
OC_SETVAL(libmapiadmin)
OC_SETVAL(libocpf)

OC_SETVAL(openchangeclient)
OC_SETVAL(mapiprofile)
OC_SETVAL(openchangepfadmin)
OC_SETVAL(exchange2mbox)
OC_SETVAL(mapitest)
OC_SETVAL(openchangemapidump)
OC_SETVAL(schemaIDGUID)
OC_SETVAL(locale_codepage)

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
	     - libmapiadmin:		$enable_libmapiadmin
	     - libocpf:			$enable_libocpf

	   * OpenChange Tools:
	     - openchangeclient:	$enable_openchangeclient
	     - mapiprofile:		$enable_mapiprofile
	     - openchangepfadmin:	$enable_openchangepfadmin
	     - exchange2mbox:		$enable_exchange2mbox
	     - mapitest:		$enable_mapitest
	     - openchangemapidump:	$enable_openchangemapidump
	     - schemaIDGUID:		$enable_schemaIDGUID
	     - locale_codepage:		$enable_locale_codepage

	   * OpenChange Torture Suite:	$enable_torture

	   * OpenChange Documentation:	$enable_doxygen

	   * OpenChange SWIG Bindings:
	     - Perl:			$enable_perlswig

===============================================================

])

