#!/bin/sh

#
# ./script/mkrelease.sh VERSION NICKNAME NICKNAME2
# ./script/mkrelease.sh 2.1 QUADRANT Quadrant
#

if [ $# -ne 3 ]; then
    echo "Usage: `basename $0` <NUMBER> <NAME> <NICKNAME>"
    exit 1
fi

VERSION="$1-$2"

# Check if we already have a release
if [ -f openchange-$VERSION.tar.gz ]; then
    echo "The release already exists in current directory"
    exit 1
fi

if [ -f openchange-$VERSION.tar.gz.asc ]; then
    echo "The release signature already exists in current directory"
    exit 1
fi

# Make the release
TMPDIR=`mktemp openchange-XXXXX`
TMPTAR="$TMPDIR.tar"
rm -f $TMPDIR || exit 1
rm -f $TMPTAR || exit 1
(git archive master --format tar --output $TMPTAR) || exit 1

mkdir openchange-$VERSION || exit 1
tar xvf $TMPTAR -C openchange-$VERSION || exit 1
( cd openchange-$VERSION
 ./autogen.sh || exit 1
 ./configure || exit 1
 make || exit 1
 sed -i "s/^OPENCHANGE_VERSION_IS_GIT_SNAPSHOT=yes/OPENCHANGE_VERSION_IS_GIT_SNAPSHOT=no/g" VERSION || exit 1
 sed -i "s/^OPENCHANGE_VERSION_RELEASE_NICKNAME=.*/OPENCHANGE_VERSION_RELEASE_NICKNAME=$3/g" VERSION || exit 1
 sed -i "s/^OPENCHANGE_VERSION_RELEASE_NUMBER=.*/OPENCHANGE_VERSION_RELEASE_NUMBER=$1/g" VERSION || exit 1

 ./autogen.sh || exit 1
 ./configure || exit 1
 make doxygen || exit 1
 make distclean  || exit 1
) || exit 1

tar -cf openchange-$VERSION.tar openchange-$VERSION || exit 1
gzip openchange-$VERSION.tar

# cleanup temporary files
rm -f $TMPTAR
rm -rf openchange-$VERSION

echo "Now run: "
echo "gpg -u <key_id> --detach-sign --armor openchange-$VERSION.tar.gz"
echo "And then upload "
echo "openchange-$VERSION.tar.gz openchange-$VERSION.tar.gz.asc" 
