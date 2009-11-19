#!/bin/sh

ADMIN_USERNAME=Administrator
ADMIN_PASSWORD=YeahWhatever

TEST1_USERNAME=testuser1
TEST1_USERPASS=testuser1
TEST1_PROFILENAME=testuser1

TEST2_USERNAME=
TEST2_USERPASS=
TEST2_PROFILENAME=

SERVER1_NAME=
SERVER2_NAME=
SERVER1_IP=192.168.244.20

SERVER1_DOMAIN=OPENCHANGETEST

########################################################################
# Support functions
#
# You shouldn't need to modify anything after this
########################################################################

declare -i TESTCOUNT=0
declare -i TESTPOINTCOUNT=0
TEST() {
    TESTPOINTCOUNT=0
    TESTCOUNT=$TESTCOUNT+1
    echo "##############################################################"
    echo "Test $TESTCOUNT: $1"
    echo "##############################################################"
}

TESTPOINT() {
    TESTPOINTCOUNT=$TESTPOINTCOUNT+1
    echo "# Testpoint $TESTCOUNT.$TESTPOINTCOUNT: $1"
    res=eval $1
    echo $res
    echo ""
}

TESTPOINT_VERSION() {
    TOOL=$1
    TESTPOINT "$TOOL --version"
    TESTPOINT "$TOOL -V"
}

TESTPOINT_USAGE() {
    TOOL=$1
    TESTPOINT "$TOOL --help"
    TESTPOINT "$TOOL -?"
    TESTPOINT "$TOOL --usage"
}

########################################################################
TEST "Verify mapiprofile"
########################################################################

TESTPOINT_VERSION "./bin/mapiprofile"

TESTPOINT_USAGE "./bin/mapiprofile"

## Create a test profile database
PROFILEDB=`mktemp -u profiles.XXXXXXXXXXXXXXXXXXXXX`
TESTPOINT "./bin/mapiprofile --database=$PROFILEDB --newdb"

## Check we are OK if there is no entries in the database
TESTPOINT "./bin/mapiprofile --database=$PROFILEDB --list"

## Check that the languages list stuff works
TESTPOINT "./bin/mapiprofile --listlangs"

## Test point - Create an admin account profile

## Test point - Set the admin account as the default

## List the accounts
TESTPOINT "./bin/mapiprofile --database=$PROFILEDB --list"

## Get the default account
TESTPOINT "./bin/mapiprofile --database=$PROFILEDB --getdefault"

## Create a (test) user account using NTLM
TESTPOINT "./bin/mapiprofile --database=$PROFILEDB --username=$TEST1_USERNAME --password=$TEST1_USERPASS --profile=$TEST1_PROFILENAME --address=$SERVER1_IP --domain=$SERVER1_DOMAIN --create"

## Test point - Create another (test) user account using Kerberos

## Set the user account as the default
TESTPOINT "./bin/mapiprofile --database=$PROFILEDB --profile=$TEST1_PROFILENAME --default"

## Dump a profile's contents
TESTPOINT "./bin/mapiprofile --database=$PROFILEDB --profile=$TEST1_PROFILENAME --dump"

## Test point - Delete a profile

########################################################################
TEST "Verify openchangeclient basic functions"
########################################################################

TESTPOINT_VERSION "./bin/openchangeclient --database=$PROFILEDB --profile=$TEST1_PROFILENAME"

TESTPOINT_USAGE "./bin/openchangeclient --database=$PROFILEDB --profile=$TEST1_PROFILENAME"

########################################################################
TEST "Verify openchangeclient admin functions"
########################################################################

########################################################################
TEST "Verify openchangeclient user functions"
########################################################################

## Verify that we can get a basic mailbox list
TESTPOINT "./bin/openchangeclient --database=$PROFILEDB --profile=$TEST1_PROFILENAME --mailbox"

## Verify that we can list the users
TESTPOINT "./bin/openchangeclient --database=$PROFILEDB --profile=$TEST1_PROFILENAME --userlist"

## Verify that we can download mail
TESTPOINT "./bin/openchangeclient --database=$PROFILEDB --profile=$TEST1_PROFILENAME --fetchmail"

## Test point - verify we can send plain text mail

## Test point - verify we can send HTML mail

## Test point - verify we can add an attachment to a mail

########################################################################
TEST "Verify openchangeclient OCPF functions"
########################################################################

########################################################################
TEST "Run the mapitest suite"
########################################################################

TESTPOINT_VERSION "./bin/mapitest"

TESTPOINT_USAGE "./bin/mapitest"

TESTPOINT ./bin/mapitest --database=$PROFILEDB --no-server

## Test point - Run just one step from mapitest


## Run mapitest as a specified user
TESTPOINT "./bin/mapitest --database=$PROFILEDB --profile=$TEST1_PROFILENAME"

########################################################################
TEST "Verify exchange2mbox"
########################################################################

TESTPOINT_VERSION "./bin/exchange2mbox"

TESTPOINT_USAGE "./bin/exchange2mbox"

########################################################################
TEST "Verify exchange2ical"
########################################################################

TESTPOINT_VERSION "./bin/exchange2ical"

TESTPOINT_USAGE "./bin/exchange2ical"

## Verify basic dump of calendar
TESTPOINT "./bin/exchange2ical --database=$PROFILEDB --profile=$TEST1_PROFILENAME"

########################################################################
TEST "Verify locale_codepage"
########################################################################

TESTPOINT_VERSION "./bin/locale_codepage"

TESTPOINT_USAGE "./bin/locale_codepage"

## Check a locale ID
TESTPOINT "./bin/locale_codepage --locale_id=0x0c09"

## Verify the group listing works
TESTPOINT "./bin/locale_codepage --list-groups"

## Check a codepage
TESTPOINT "./bin/locale_codepage --codepage=0x4B0"

########################################################################
TEST "Verify openchangepfadmin"
########################################################################

TESTPOINT_VERSION "./bin/openchangepfadmin"

TESTPOINT_USAGE "./bin/openchangepfadmin"

## Get the list of public folders
TESTPOINT "./bin/openchangepfadmin --database=$PROFILEDB --profile=$TEST1_PROFILENAME --list"

#############
# Cleanup stuff
#############
rm $PROFILEDB
