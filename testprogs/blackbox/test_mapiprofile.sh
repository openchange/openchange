#!/bin/sh
# Black tests for mapiprofile
# Copyright (C) 2009 Julien Kerihuel <j.kerihuel@openchange.org>

if [ $# -lt 4 ]; then
cat <<EOF
Usage: mapiprofile.sh SERVER USERNAME PASSWORD DOMAIN
EOF
exit 1;
fi

SERVER=$1
USERNAME=$2
PASSWORD=$3
DOMAIN=$4
shift 4
failed=0

ocbindir=`dirname $0`/../../bin
mapiprofile=$ocbindir/mapiprofile
profdb=`dirname $0`/profiles.ldb

. `dirname $0`/subunit.sh

testit "create new database" $VALGRIND $mapiprofile --newdb --database=$profdb || failed=`expr $failed + 1`

testit "create new profile" $VALGRIND $mapiprofile --database=$profdb -P mapiprofile --create --username=$USERNAME --password=$PASSWORD -I $SERVER -D $DOMAIN || failed=`expr $failed + 1`

testit "list profiles" $VALGRIND $mapiprofile --database=$profdb --list || failed=`expr $failed + 1`

testit "set it as default" $VALGRIND $mapiprofile --database=$profdb -P mapiprofile -S || failed=`expr $failed + 1`

# Check if the default profile has the proper name
list_test=`mapiprofile --list --database=$profdb | grep default | cut -d= -f2 | sed 's/\[default\]//' | sed 's/\s*//' | sed 's/ //'`
if [ "$list_test" != "mapiprofile" ]; then
    subunit_fail_test "\"$list_test\" is not the default profile"
    failed=`expr $failed + 1`
fi

testit "rename profile" $VALGRIND $mapiprofile --database=$profdb -P mapiprofile --rename=mapiprofile_rename || failed=`expr $failed + 1`

# Check if the new profile has the proper name
rename=`mapiprofile --list --database=$profdb | grep default | cut -d= -f2 | sed 's/\[default\]//' | sed 's/\s*//' | sed 's/ //'`
if [ "$rename" != "mapiprofile_rename" ]; then
    subunit_fail_test "mapiprofile was not renamed to mapiprofile_rename"
    failed=`expr $failed + 1`
fi

testit "get FQDN" $VALGRIND $mapiprofile --database=$profdb -P mapiprofile_rename --getfqdn || failed=`expr $failed + 1`

testit "delete profile" $VALGRIND $mapiprofile --database=$profdb -P mapiprofile_rename --delete || failed=`expr $failed + 1`

rm -f profiles.ldb