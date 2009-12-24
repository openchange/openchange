#!/bin/sh

#
# ./script/mkrelease.sh VERSION NICKNAME NICKNAME2 FIRST_REVISION
# ./script/mkrelease.sh 0.7 PHASER Phaser 308
#

TMPDIR=`mktemp openchange-XXXXX`
rm $TMPDIR || exit 1
svn export . $TMPDIR || exit 1
svn log -r$4:HEAD > $TMPDIR/CHANGELOG || exit 1

( cd $TMPDIR/
 ./autogen.sh || exit 1
 ./configure || exit 1
 make || exit 1
 sed -i "s/^OPENCHANGE_VERSION_IS_SVN_SNAPSHOT=yes/OPENCHANGE_VERSION_IS_SVN_SNAPSHOT=no/g" VERSION || exit 1
 sed -i "s/^OPENCHANGE_VERSION_RELEASE_NICKNAME=.*/OPENCHANGE_VERSION_RELEASE_NICKNAME=$3/g" VERSION || exit 1
 sed -i "s/^OPENCHANGE_VERSION_RELEASE_NUMBER=.*/OPENCHANGE_VERSION_RELEASE_NUMBER=$1/g" VERSION || exit 1

 ./autogen.sh || exit 1
 ./configure || exit 1
 make doxygen || exit 1
 make distclean  || exit 1
 rm .bzrignore
) || exit 1

VERSION=$1-$2
mv $TMPDIR openchange-$VERSION || exit 1
tar -cf openchange-$VERSION.tar openchange-$VERSION || exit 1
echo "Now run: "
echo "gpg --detach-sign --armor openchange-$VERSION.tar"
echo "gzip openchange-$VERSION.tar" 
echo "And then upload "
echo "openchange-$VERSION.tar.gz openchange-$VERSION.tar.asc" 
