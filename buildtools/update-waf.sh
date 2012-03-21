#!/bin/sh
# Update our copy of waf

TARGETDIR="`dirname $0`"
WORKDIR="`mktemp -d -t update-waf.XXXXXX`"

mkdir -p "$WORKDIR"

git clone https://code.google.com/p/waf "$WORKDIR"

rsync -C -avz --delete "$WORKDIR/waflib/" "$TARGETDIR/waflib/"

rm -rf "$WORKDIR"
