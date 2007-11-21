# Simple configuration script for OpenChange
# Written by Jelmer Vernooij <jelmer@samba.org>

AC_PREREQ(2.57)
AC_INIT(openchange, 0.7, [openchange@openchange.org])
AC_CONFIG_HEADER([config.h])
AC_DEFINE(_GNU_SOURCE, 1, [Use GNU extensions])

AC_PROG_CC
dnl AC_PROG_YACC

dnl ***********************
dnl Check for PERL
dnl ***********************
AC_PROG_INSTALL

AC_PATH_PROG(PERL, perl)

if test -z "$PERL"
then
	AC_MSG_ERROR(Please install perl)
fi

dnl ***********************
dnl Check for Flex
dnl ***********************
AC_ARG_VAR([FLEX], [Location of the flex program.])
AC_CHECK_PROG([FLEX], [flex], [flex])
if test x"$FLEX" = x""; then
    AC_MSG_ERROR([You must have the flex lexer generator installed.])
fi

dnl ***********************
dnl Check for Pidl
dnl ***********************

AC_PATH_PROG(PIDL, pidl)

if test -z "$PIDL"
then
	AC_MSG_ERROR(Please install pidl)
fi

dnl ********************
dnl Check for smbtorture
dnl ********************

AC_PATH_PROG(SMBTORTURE, smbtorture, no, "$prefix/bin")


dnl *****************************
dnl Check for libmagic
dnl *****************************
AC_CHECK_LIB([magic], [magic_open],
	             [AC_DEFINE(HAVE_LIBMAGIC, 1,
 	                       [Define if you want to use libmagic])
 	              MAGIC_LIBS="-lmagic -lz"],
				  AC_SUBST(MAGIC_LIBS)
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
PKG_CHECK_MODULES(SAMR, dcerpc_samr)
PKG_CHECK_MODULES(LDB, ldb)

TORTURE_MODULESDIR=`$PKG_CONFIG --variable=modulesdir torture`
AC_SUBST(TORTURE_MODULESDIR)

AC_CHECK_TYPE(uint_t, unsigned int)

dnl ************************
dnl Nail down samba4 version
dnl ************************

dnl Samba4 revision boundaries
FIRST_REV=26100
LAST_REV=$FIRST_REV

INCDIR=$prefix/include
if test -f $INCDIR/samba/version.h; then
	SAMBA_VERSION=`cat $INCDIR/samba/version.h | grep SAMBA_VERSION_MAJOR | cut -d" " -f 3`
	SAMBA_VERSION_ALPHA_RELEASE=`cat $INCDIR/samba/version.h | grep SAMBA_VERSION_ALPHA_RELEASE | cut -d" " -f 3`
	SAMBA_VERSION_SVN_REVISION=`cat $INCDIR/samba/version.h | grep SAMBA_VERSION_SVN_REVISION | cut -d" " -f 3`

	dnl OpenChange only compiles with Samba4
	if test !"$SAMBA_VERSION" = 4 ; then
	   AC_MSG_ERROR(Samba4 installation is mandatory)
	fi

	dnl OpenChange only compiles with Samba4 > alpha 2
	if test "$SAMBA_VERSION_ALPHA_RELEASE" -lt 2; then
	   AC_MSG_ERROR(*******************************************************		\
		  OpenChange only compiles with Samba4 alpha 2 or greater		\
		  *******************************************************)
	fi

	dnl OpenChange won't compile with rev prior $FIRST_REV
	if test "$SAMBA_VERSION_SVN_REVISION" -lt $FIRST_REV; then
	   AC_MSG_ERROR(**********************************************************
		  * Samba4 version working with libmapi $PACKAGE_VERSION:		
		       First working revision:........$FIRST_REV			
		       Last known working revision:...$LAST_REV				
		       	    	  	  						
		  * Your Samba4 installation:						
		       Installed revision:............$SAMBA_VERSION_SVN_REVISION	
											
		  Please UPDATE your Samba4 installation to $LAST_REV or higher.	
		  **********************************************************)
	fi
	
	dnl OpenChange is known to compile with rev $LAST_REV
	if test "$SAMBA_VERSION_SVN_REVISION" -gt $LAST_REV; then
	   AC_MSG_WARN(**********************************************************	
		    * Samba4 version working with libmapi $PACKAGE_VERSION:		
		       First working revision:........$FIRST_REV			
		       Last known working revision:...$LAST_REV				
											
		    * Your Samba4 installation:						
		       Installed revision:............$SAMBA_VERSION_SVN_REVISION	
											
		    OpenChange has not been tested with this revision but
		    there is a high probability it compiles and works properly     	       	   			
											
		    If you encounter any issues with your Samba4 installation		
	   	    Please consider downgrading to samba4 rev $LAST_REV		
		    **********************************************************)
	fi

else
	AC_MSG_WARN([Unable to nail down Samba4 version])
fi

dnl ***********************
dnl Swig bindings
dnl ***********************
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
   SWIGDIRSCLEAN+="swigperl-clean"
   SWIGDIRSDISTCLEAN+="swigperl-distclean"
   SWIGDIRSREALDISTCLEAN+="swigperl-realdistclean"
fi

PERL5DIR=`$PERL -e 'use Config; my $dir = $Config{sitelib}; print $dir'`
AC_SUBST(PERL5DIR)

AC_SUBST(SWIGDIRSALL)
AC_SUBST(SWIGDIRSINSTALL)
AC_SUBST(SWIGDIRSUNINSTALL)
AC_SUBST(SWIGDIRSCLEAN)
AC_SUBST(SWIGDIRSDISTCLEAN)
AC_SUBST(SWIGDIRSREALDISTCLEAN)

dnl **************************
dnl Documentation dependencies
dnl **************************

AC_PATH_PROG(DOXYGEN, doxygen)
if test -z "$DOXYGEN"
   then
	AC_MSG_ERROR(Please install doxygen)
fi

dnl ***********************
dnl Makefiles 
dnl ***********************

AC_CONFIG_FILES([Makefile swig/perl/Makefile libmapi.pc libmapiadmin.pc
		 Doxyfile])
AC_OUTPUT

