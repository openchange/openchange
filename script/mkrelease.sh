#!/bin/sh

#
# ./script/mkrelease.sh VERSION NICKNAME FIRST_REVISION
# ./script/mkrelease.sh 0.7 PHASER 308
#

TMPDIR=`mktemp libmapi-XXXXX`
rm $TMPDIR || exit 1
svn export . $TMPDIR || exit 1
svn log -r$3:HEAD > $TMPDIR/CHANGELOG || exit 1

( cd $TMPDIR/
 ./autogen.sh || exit 1
 ./configure || exit 1
 make || exit 1
 sed -i "s/^OPENCHANGE_VERSION_IS_SVN_SNAPSHOT=yes/OPENCHANGE_VERSION_IS_SVN_SNAPSHOT=no/g" VERSION || exit 1
 ./autogen.sh || exit 1
 ./configure || exit 1
 make doxygen || exit 1
 make distclean  || exit 1
 rm .bzrignore
) || exit 1

VERSION=$1-$2
mv $TMPDIR libmapi-$VERSION || exit 1
tar -cf libmapi-$VERSION.tar libmapi-$VERSION || exit 1
echo "Now run: "
echo "gpg --detach-sign --armor libmapi-$VERSION.tar"
echo "gzip libmapi-$VERSION.tar" 
echo "And then upload "
echo "libmapi-$VERSION.tar.gz libmapi-$VERSION.tar.asc" 
