#!/usr/bin/env bash

#
# VARS
#
. `dirname $0`/samba4_ver.sh

if which gmake 2>/dev/null; then
	MAKE=gmake
else
	MAKE=make
fi

# If you have a samba checkout (even not up-to-date), you can make this a lot faster using --reference, e.g.
# SAMBA_GIT_REFERENCE="--reference $HOME/samba-master"
if test x"$SAMBA_GIT_REPO" = x""; then
    SAMBA_GIT_REPO=git://git.samba.org/samba.git
fi

# Set SAMBA_PREFIX to wherever you want to install to. Defaults to /usr/local/samba, as if you did the build manually
if test x"$SAMBA_PREFIX" = x""; then
    SAMBA_PREFIX="/usr/local/samba"
fi

export CPPFLAGS="-I${SAMBA_PREFIX}/include"

# use ccache for faster rebuild, where available
if which ccache 2>/dev/null; then
	export CC="ccache gcc"
else
	export CC="gcc"
fi

export PKG_CONFIG_PATH=$SAMBA_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH
pythondir_nodep=`python -c "from distutils import sysconfig; print sysconfig.get_python_lib(0,0,'/')"`
pythondir_dep=`python -c "from distutils import sysconfig; print sysconfig.get_python_lib(1,0,'/')"`
export PYTHONPATH=$SAMBA_PREFIX$pythondir_nodep:$SAMBA_PREFIX$python_dep:$PYTHONPATH

RUNDIR=$(readlink -f $(dirname $0))
HOST_OS=`$RUNDIR/../config.guess`

BUILDTOOLS=$RUNDIR/../samba4/buildtools/

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

cleanup_lib() {
    lib="$1"
    if test -f samba4/$lib/Makefile; then
	echo "cleaning up $lib directory"
	pushd samba4/$lib
	$MAKE distclean
	popd
    fi
}

cleanup_talloc() {
    cleanup_lib "lib/talloc"
}

cleanup_tdb() {
    cleanup_lib "lib/tdb"
}

cleanup_ldb() {
    cleanup_lib "lib/ldb"
}

delete_install() {

    # cleanup existing existing samba4 installation
    if test -d $SAMBA_PREFIX; then
	echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	echo "A previous samba4 installation has been detected in $SAMBA_PREFIX"
	echo "It is highly recommended to delete it prior compiling Samba4"
	echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	echo ""
	echo -n "Proceed removing all of $SAMBA_PREFIX? [Yn]: "
	read answer
	case "$answer" in
	    Y|y|yes)
		echo "Step0: Removing previous samba4 installation"
		if test -w $SAMBA_PREFIX; then
		    rm -rf $SAMBA_PREFIX
		else
		    sudo rm -rf $SAMBA_PREFIX
		fi
		;;
	    N|n|no)
		echo "Step0: Keep previous samba4 installation"
		;;
	esac
    fi

    cleanup_talloc
    cleanup_tdb
    cleanup_ldb
}

#
# Checkout Samba4
#
checkout() {
    GITPATH=`whereis -b git`

    if test x"$GITPATH" = x"git:"; then
	echo "git was not found in your path!"
	echo "Please install git"
	exit 1
    fi

    echo "Step1: Fetching Samba4 latest GIT revision"
    git clone $SAMBA_GIT_REPO $SAMBA_GIT_REFERENCE samba4
    error_check $? "Step1"

    echo "Step2: Creating openchange local copy"
    pushd samba4
    git checkout -b openchange origin/master
    error_check $? "Step2"

    if test x"$SAMBA4_GIT_REV" != x""; then
	echo "Step3: Revert to commit $SAMBA4_GIT_REV"
	git reset --hard $SAMBA4_GIT_REV
	error_check $? "Step3"
    fi
    popd
    return $?
}

#
# Update Samba4
#
update() {
    GITPATH=`whereis -b git`

    if test x"$GITPATH" = x"git:"; then
	echo "git was not found in your path!"
	echo "Please install git"
	exit 1
    fi

    echo "Step1: Update Samba4 to latest GIT revision"
    pushd samba4
    git pull 
    error_check $? "Step1"

    if test x"$SAMBA4_GIT_REV" != x""; then
	echo "Step3: Revert to commit $SAMBA4_GIT_REV"
	git reset --hard $SAMBA4_GIT_REV
	error_check $? "Step3"
    fi
    popd
    return $?
}

#
# Download Samba4 release
#
download() {
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

    echo "Step1: Fetching samba-$SAMBA4_RELEASE tarball"
    if ! test -e samba-$SAMBA4_RELEASE.tar.gz; then
	rm -rf samba-$SAMBA4_RELEASE.tar.gz
	wget https://download.samba.org/pub/samba/stable/samba-$SAMBA4_RELEASE.tar.gz
	error_check $? "Step1"
    fi     

    echo "Step2: Extracting $SAMBA4_RELEASE"
    tar xzvf samba-$SAMBA4_RELEASE.tar.gz
    error_check $? "Step2"
    mv samba-$SAMBA4_RELEASE samba4

    return $?
}

#
# Apply patches to samba4
#
patch() {

    case "$HOST_OS" in
	*freebsd*)

	    echo "[+] Patching heimdal for FreeBSD"
	    pushd samba4/source4/heimdal/lib/roken
	    sed "s/#if defined(HAVE_OPENPTY) || defined(__linux) || defined(__osf__)/#if defined(HAVE_OPENPTY) || defined(__linux) || defined(__osf__) || defined(__FreeBSD__)/g" rkpty.c > rkpty2.c
	    mv rkpty2.c rkpty.c
	    sed -e "54i\\
#if defined(__FreeBSD__)\\
#include <sys/ioctl.h>\\
#include <termios.h>\\
#include <libutil.h>\\
#endif" rkpty.c > rkpty2.c
	    mv rkpty2.c rkpty.c
	    popd
	    ;;
    esac
    return $?
}

#
# Compile and Install samba4 packages:
# talloc, tdb, tevent, ldb
#
packages() {
    delete_install

    for lib in lib/talloc lib/tdb lib/tevent lib/ldb; do
	echo "Building and installing $lib library"
	export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$SAMBA_PREFIX/lib/pkgconfig
	pushd samba4/$lib
	error_check $? "$lib setup"

	echo ./configure -C --prefix=$SAMBA_PREFIX --enable-developer --bundled-libraries=NONE $extra
	./configure -C --prefix=$SAMBA_PREFIX --enable-developer --bundled-libraries=NONE $extra
	error_check $? "$lib configure"

	$MAKE -j
	error_check $? "$lib make"

	if test -w `dirname $SAMBA_PREFIX`; then
	    $MAKE install
	    error_check $? "$lib make install"
	else
	    sudo $MAKE install
	    error_check $? "$lib sudo make install"
	fi


	$MAKE distclean
	error_check $? "$lib make distclean"

	popd
	export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$SAMBA_PREFIX/lib/pkgconfig
    done
}

#
# Compile Samba4
#
compile() {
    echo "Step1: Preparing Samba4 system"
    pushd samba4/source4
    error_check $? "samba4 setup"

    cd $RUNDIR/../samba4
    export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$SAMBA_PREFIX/lib/pkgconfig
    ./configure.developer -C --prefix=$SAMBA_PREFIX --builtin-libraries=replace
    error_check $? "samba4 configure"

    echo "Step2: Compile Samba4 (Source)"
    $MAKE -j
    error_check $? "samba4 make"

    popd
}

#
# Post install operations
#
post_install() {
    case "$HOST_OS" in
	*freebsd*)
	    pushd samba4/pidl
	    error_check $? "post_install setup"
	    if test -w `dirname $SAMBA_PREFIX`; then
		$MAKE install
		error_check $? "post make install"
	    else
		sudo $MAKE install
		error_check $? "post sudo make install"
	    fi
	    popd
	    echo "[+] Add comparison_fn_t support to ndr.h header file"
	    pushd $SAMBA_PREFIX/include
	    if test -w $SAMBA_PREFIX/include; then
		sed -e "34i\\
#if defined(__FreeBSD__)\\
# ifndef HAVE_COMPARISON_FN_T\\
typedef int (*comparison_fn_t)(const void *, const void *);\\
# endif\\
#endif" ndr.h > /tmp/ndr.h
		mv /tmp/ndr.h ndr.h
	    else
		sudo sed -e "34i\\
#if defined(__FreeBSD__)\\
# ifndef HAVE_COMPARISON_FN_T\\
typedef int (*comparison_fn_t)(const void *, const void *);\\
# endif\\
#endif" ndr.h > /tmp/ndr.h
		sudo mv /tmp/ndr.h ndr.h
	    fi
	    popd
	    ;;
    esac
}

#
# Install Samba4
#
install() {
    echo "Step1: Installing Samba"
    echo "===> we are in $PWD"
    pushd samba4/source4
    error_check $? "samba4 setup"

    cd $RUNDIR/../samba4
    if test -w `dirname $SAMBA_PREFIX`; then
	$MAKE install
	error_check $? "samba4 install"
    else
	sudo $MAKE install
	error_check $? "samba4 install"
    fi

    popd
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
    git-update)
	update
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
