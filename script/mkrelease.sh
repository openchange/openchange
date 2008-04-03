#!/bin/sh

TMPDIR=`mktemp libmapi-XXXXX`
rm $TMPDIR || exit 1
svn export . $TMPDIR || exit 1

( cd $TMPDIR/
 ./autogen.sh || exit 1
 ./configure || exit 1
 make realdistclean  || exit 1
) || exit 1

VERSION=$1-$2
mv $TMPDIR libmapi-$VERSION || exit 1
tar -cf libmapi-$VERSION.tar libmapi-$VERSION || exit 1
echo "Now run: "
echo "gpg --detach-sign --armor libmapi-$VERSION.tar"
echo "gzip libmapi-$VERSION.tar" 
echo "And then upload "
echo "libmapi-$VERSION.tar.gz libmapi-$VERSION.tar.asc" 
