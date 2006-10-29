#!/bin/sh
# install OpenChange miscellaneous files

SRCDIR="$1"
JSDIR="$2"
SETUPDIR="$3"

cd $SRCDIR || exit 1

echo "Installing OpenChange js libs.."
cp libmapi/setup/scripting/libjs/*.js $JSDIR || exit 1
echo "Done.."

echo "Installing OpenChange setup templates.."
cp libmapi/setup/*.ldif $SETUPDIR || exit 1
echo "All Ok"

exit 0
