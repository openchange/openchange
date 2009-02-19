#!/bin/sh

#
# VARS
#
. `dirname $0`/samba4_ver.sh

if which gmake 2>/dev/null; then
	MAKE=gmake
else
	MAKE=make
fi

/usr/bin/env -i PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/samba/lib/pkgconfig

RUNDIR=`dirname $0`
HOST_OS=`$RUNDIR/../config.guess`

#
# Error check
#
error_check() {
    error=$1
    step=$2

    if [ $error -ne 0 ]; then
	echo "Error in $2 (error code $1)"
	exit 1
    fi
}

cleanup_talloc() {
    # cleanup existing talloc installation
    if test -f samba4/lib/talloc/Makefile; then
	echo "Step0: cleaning up talloc directory"
	OLD_PWD=$PWD
	cd samba4/lib/talloc
	make realdistclean
	cd $OLD_PWD
    fi
}

cleanup_tdb() {
    # cleanup existing tdb installation
    if test -f samba/lib/tdb/Makefile; then
	echo "Step0: cleaning up tdb directory"
	OLD_PWD=$PWD
	cd samba4/lib/tdb
	make realdistclean
	cd $OLD_PWD
    fi
}

delete_install() {

    # cleanup existing existing samba4 installation
    if test -d /usr/local/samba; then
	echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	echo "A previous samba4 installation has been detected"
	echo "It is highly recommended to delete it prior compiling Samba4"
	echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	echo ""
	echo -n "Proceed? [Yn]: "
	read answer
	case "$answer" in
	    Y|y|yes)
		echo "Step0: Removing previous samba4 installation"
		sudo rm -rf /usr/local/samba
		;;
	    N|n|no)
		echo "Step0: Keep previous samba4 installation"
		;;
	esac
    fi

	cleanup_talloc
	cleanup_tdb
}

#
# Checkout Samba4
#
checkout() {
    OLD_PWD=$PWD

    GITPATH=`whereis -b git`

    if test x"$GITPATH" = x"git:"; then
	echo "git was not found in your path!"
	echo "Please install git"
	exit 1
    fi

    echo "Step1: Fetching Samba4 latest GIT revision"
    git clone git://git.samba.org/samba.git samba4
    error_check $? "Step1"

    echo "Step2: Creating openchange local copy"
    cd samba4
    git checkout -b openchange origin/master
    error_check $? "Step2"

    echo "Step3: Revert to commit $SAMBA4_GIT_REV"
    git reset --hard $SAMBA4_GIT_REV
    error_check $? "Step3"

    cd $OLD_PWD
    return $?
}

#
# Download Samba4 release
#
download() {
    OLD_PWD=$PWD

    WGETPATH=`whereis -b wget`
    TARPATH=`whereis -b tar`

    if test x"$WGETPATH" = x"wget:"; then
	echo "wget was not found in your path!"
	echo "Please install wget"
	exit 1
    fi

    if test x"$TARPATH" = x"tar:"; then
	echo "tar was not found in your path!"
	echo "Please install tar"
	exit 1
    fi

    echo "Step0: Checking for previous samba4 directory"
    if test -d samba4; then
	echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	echo "A previous samba4 directory has been detected in current folder."
	echo "Should we delete the existing samba4 directory?"
	echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	echo ""
	echo -n "Proceed? [Yn]: "
	read answer
	case "$answer" in
	    Y|y|yes)
		echo "Step0: removing previous samba4 directory"
		sudo rm -rf samba4
		;;
	    N|n|no)
		echo "Step0: Keep existing directory"
		return
		;;
	esac
    fi

    echo "Step2: Fetching samba-$SAMBA4_RELEASE tarball"
    if ! test -e samba-$SAMBA4_RELEASE.tar.gz; then
	rm -rf samba-$SAMBA4_RELEASE.tar.gz
	wget http://us1.samba.org/samba/ftp/samba4/samba-$SAMBA4_RELEASE.tar.gz
	error_check $? "Step1"
    fi     

    echo "Step3: Extracting $SAMBA4_RELEASE"
    tar xzvf samba-$SAMBA4_RELEASE.tar.gz
    error_check $? "Step2"
    mv samba-$SAMBA4_RELEASE samba4

    cd $OLD_PWD
    return $?
}

#
# Apply patches to samba4
#
patch() {
    case "$HOST_OS" in
	*freebsd*)

	    echo "[+] Patching tevent for FreeBSD"
	    OLD_PWD=$PWD
	    cd samba4/lib/tevent
	    sed 's/\$(PYTHON_CONFIG) --libs/\$(PYTHON_CONFIG) --ldflags/g' tevent.mk > tevent.mk2
	    mv tevent.mk2 tevent.mk
	    cd $OLD_PWD

	    echo "[+] Patching heimdal for FreeBSD"
	    OLD_PWD=$PWD
	    cd samba4/source4/heimdal/lib/roken
	    sed "s/#if defined(HAVE_OPENPTY) || defined(__linux) || defined(__osf__)/#if defined(HAVE_OPENPTY) || defined(__linux) || defined(__osf__) || defined(__FreeBSD__)/g" rkpty.c > rkpty2.c
	    mv rkpty2.c rkpty.c
	    sed -e "54i\\
#if defined(__FreeBSD__)\\
#include <sys/ioctl.h>\\
#include <termios.h>\\
#include <libutil.h>\\
#endif" rkpty.c > rkpty2.c
	    mv rkpty2.c rkpty.c
	    cd $OLD_PWD
	    ;;
    esac

    return $?
}

#
# Compile and Install samba4 packages:
# talloc, tdb
#
packages() {
    OLD_PWD=$PWD

    delete_install

    echo "Step1: Installing talloc library"
    cd samba4/lib/talloc
    error_check $? "Step1"

    ./autogen.sh
    error_check $? "Step1"

    ./configure --prefix=/usr/local/samba
    error_check $? "Step1"

    make
    error_check $? "Step1"

    sudo make install
    error_check $? "Step1"

    make realdistclean
    error_check $? "Step1"

    cd $OLD_PWD

    echo "Step2: Installing tdb library"

    cd samba4/lib/tdb
    error_check $? "Step2"

    ./autogen.sh
    error_check $? "Step2"

    ./configure --prefix=/usr/local/samba
    error_check $? "Step2"

    make
    error_check $? "Step2"

    sudo make install
    error_check $? "Step2"

    make realdistclean
    error_check $? "Step2"

    cd $OLD_PWD

    echo "Step3: Installing tevent library"

    cd samba4/lib/tevent
    error_check $? "Step3"

    ./autogen.sh
    error_check $? "Step3"

    ./configure --prefix=/usr/local/samba
    error_check $? "Step3"

    make
    error_check $? "Step3"

    sudo make install
    error_check $? "Step3"

    make realdistclean
    error_check $? "Step3"

    cd $OLD_PWD
}

#
# Compile Samba4
#
compile() {

    OLD_PWD=$PWD

    # Cleanup tdb and talloc directories
    cleanup_talloc
    cleanup_tdb

    echo "Step1: Preparing Samba4 system"
    cd samba4/source4
    error_check $? "Step1"

    ./autogen.sh
    error_check $? "Step1"

    ./configure.developer --enable-debug
    error_check $? "Step1"

    echo "Step2: Compile Samba4 (IDL)"
    $MAKE idl_full

    echo "Step3: Compile Samba4 (Source)"
   	$MAKE 
    error_check $? "Step3"

    cd $OLD_PWD
}

#
# Post install operations
#
post_install() {
    case "$HOST_OS" in
	*freebsd*)
	    OLD_PWD=$PWD
	    cd samba4/pidl
	    sudo $MAKE install
	    error_check $? "Step 1"
	    cd $OLD_PWD
            echo "[+] Add comparison_fn_t support to ndr.h header file"
            OLD_PWD=$PWD
            cd /usr/local/samba/include
            sudo sed -e "34i\\
#if defined(__FreeBSD__)\\
# ifndef HAVE_COMPARISON_FN_T\\
typedef int (*comparison_fn_t)(const void *, const void *);\\
# endif\\
#endif" ndr.h > /tmp/ndr.h
            sudo mv /tmp/ndr.h ndr.h
            cd $OLD_PWD
	    ;;
    esac
}

#
# Install Samba4
#
install() {

    OLD_PWD=$PWD

    echo "Step1: Installing Samba"
    echo "===> we are in $PWD"
    cd samba4/source4
    error_check $? "Step1"

    sudo $MAKE install
    error_check $? "Step1"

    cd $OLD_PWD
}


#
# main program
#
case $1 in
    checkout)
	checkout
	;;
    download)
	download
	;;
    patch)
	patch
	;;
    packages)
	packages
	;;
    compile)
	compile
	;;
    install)
	install
	;;
    post_install)
	post_install
	;;
    git-all)
	checkout
	patch
	packages
	compile
	install
	post_install
	;;
    all)
	download
	patch
	packages
	compile
	install
	post_install
	;;
    *)
	echo $"Usage: $0 {checkout|patch|packages|compile|install|post_install|git-all}"
	echo $"Usage: $0 {download|patch|packages|compile|install|post_install|all}"
	;;
esac

exit 0
