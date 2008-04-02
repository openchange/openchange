#!/bin/sh

#
# Error check
#
function error_check() {
    error=$1
    step=$2

    if [ $error -ne 0 ]; then
	echo "Error in $2 (error code $1)"
	exit 1
    fi
}

#
# Checkout Samba4
#
function checkout() {
    OLDPWD=$PWD

    GITPATH=`whereis -b git`

    if test x"$GITPATH" = x"git:"; then
	echo "git was not found in your path!"
	echo "Please install git"
	exit 1
    fi

    echo "Step1: Fetching Samba4 latest GIT revision"
    git-clone git://git.samba.org/samba.git samba4
    error_check $? "Step1"

    echo "Step2: Creating openchange local copy"
    cd samba4
    git checkout -b openchange origin/v4-0-test
    error_check $? "Step2"

    echo "Step3: Revert to commit 41309dc"
    git reset --hard 41309dc
    error_check $? "Step3"

    cd $OLDPWD
    return $?
}

#
# Compile and Install samba4 packages:
# talloc, tdb
#
function packages() {
    OLDPWD=$PWD

    echo "Step1: Installing talloc library"
    cd samba4/source/lib/talloc
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

    rm -rf ../replace/*.{o,ho}
    error_check $? "Step1"

    cd $OLDPWD

    echo "Step2: Installing tdb library"

    cd samba4/source/lib/tdb
    error_check $? "Step2"

    sed -i 's/install-python/installpython/g' configure.ac
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

    rm -rf ../replace/*.{o,ho}
    error_check $? "Step2"

    cd $OLDPWD
}

#
# Compile Samba4
#
function compile() {

    OLDPWD=$PWD

    PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/samba/lib/pkgconfig

    echo "Step1: Preparing Samba4 system"
    cd samba4/source
    error_check $? "Step1"

    ./autogen.sh
    error_check $? "Step1"

    ./configure.developer
    error_check $? "Step1"

    echo "Step2: Compile Samba4"
    make
    error_check $? "Step2"



    echo "Step3: Preparing PIDL installation"
    cd pidl

    perl Makefile.PL
    error_check $? "Step3"

    make
    error_check $? "Step3"

    cd $OLDPWD
}


#
# Install Samba4
#
function install() {

    OLDPWD=$PWD

    echo "Step1: Installing Samba"
    cd samba4/source
    error_check $? "Step1"

    sudo make install
    error_check $? "Step1"

    echo "Step2: Installing PIDL"
    cd pidl
    sudo make install
    error_check $? "Step2"

    cd $OLDPWD
}


#
# main program
#
case $1 in
    checkout)
	checkout
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
    all)
	checkout
	packages
	compile
	install
	;;
    *)
	echo $"Usage: $0 {checkout|packages|compile|install|all}"
	;;
esac

exit 0