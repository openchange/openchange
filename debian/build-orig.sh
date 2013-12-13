#!/bin/sh
# Build a source tarball for openchange

openchange_repos=https://github.com/Zentyal/openchange.git
version=$( dpkg-parsechangelog -l`dirname $0`/changelog | sed -n 's/^Version: \(.*:\|\)//p' | sed 's/-[0-9.]\+$//' )

if test -z "$BRANCH"; then
    BRANCH="master"
fi

if echo $1 | grep git > /dev/null; then
    git clone "$openchange_repos" openchange_checkout || exit 1
    cd openchange_checkout
    timestamp=`date +%Y%m%d%H%M%S`
    revision=`git describe $BRANCH --tags | sed 's/^openchange-//g'`
    ./autogen.sh
    cd ..
    mv openchange_checkout openchange-$revision
    tar cvz openchange-$revision > openchange_$revision.orig.tar.gz
    rm -rf openchange-$revision
    dch -v "1:$revision-0zentyal0" "Built GIT snapshot."
    dch -r "Built GIT snapshot."
else
    uscan --upstream-version $version
fi
