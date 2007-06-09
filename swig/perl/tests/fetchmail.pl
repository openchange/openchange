#!/usr/bin/perl -w

##############################################
# Perl bindings test suite:
#
# o Fetch emails from Exchange Server
#
# Copyright Julien Kerihuel 2007.
# <j.kerihuel@openchange.org>
#
# released under GNU GPL


use strict;
use mapi;

package mapi;

use constant MAPI_E_NOT_FOUND => 0x8004010F;
use constant PR_FID => 0x67480014;
use constant PR_MID => 0x674a0014;
use constant olFolderInbox => 6;

my $retval;

sub mapi_init()
{
    my $profdb;
    my $profname;
    my $password = ();
    my $flag = 0;
    my $session = new_mapi_session_t();

    print "Initializing MAPI\n";
    $profdb = sprintf("%s/.openchange/profiles.ldb", $ENV{HOME});

    # Initialize MAPI library
    $retval = MAPIInitialize($profdb);
    mapi_errstr("MAPIInitialize", GetLastError());

    # Activate MAPI decoding
    # lw_dumpdata();

    # Retrieve default profile name
    ($retval, $profname) = GetDefaultProfile($flag);
    mapi_errstr("GetDefaultProfile", GetLastError());

    $retval = MapiLogonEx($session, $profname, $password);
    mapi_errstr("MapiLogonEx", $retval);
}

sub mapi_finalize()
{
    MAPIUninitialize();
}

sub mapi_fetchmail()
{
    my $obj_store = new_mapi_object();
    my $obj_inbox = new_mapi_object();
    my $obj_table = new_mapi_object();
    my $inbox_id = new_int64();
    my $cRows;
    my $count = new_int32();
    my $unread = new_int32();
    my $total = new_int32();
    my $SPropTagArray = new_SPropTagArray();
    my $SRowSet = new_SRowSet();

    ## Open Message Store
    $retval = mapi_object_init($obj_store);
    $retval = OpenMsgStore($obj_store);
    mapi_errstr("OpenMsgStore", GetLastError());

    ## Open Inbox
    $retval = mapi_object_init($obj_inbox);
    $retval = GetDefaultFolder($obj_store, $inbox_id, olFolderInbox);
    mapi_errstr("GetDefaultFolder", GetLastError());

    $retval = OpenFolder($obj_store, $inbox_id, $obj_inbox);
    mapi_errstr("OpenFolder", GetLastError());

    ## Count messages
    $retval = GetFolderItemsCount($obj_inbox, $unread, $total);
    mapi_errstr("GetFolderItemsCount", GetLastError());

    print "Mailbox:\n";
    print "\t => Unread(" . int32_value($unread) . ")\n";
    print "\t => Total(" . int32_value($total) . ")\n";

    ## GetContentsTable
    $retval = mapi_object_init($obj_table);
    $retval = GetContentsTable($obj_inbox, $obj_table);
    mapi_errstr("GetContentsTable", GetLastError());

    $retval = lw_SetCommonColumns($obj_table);
    mapi_errstr("SetColumns", GetLastError());

    $retval = GetRowCount($obj_table, $count);
    mapi_errstr("GetRowCount", GetLastError());

    while (($retval = QueryRows($obj_table, int32_value($count), 0, $SRowSet)) != MAPI_E_NOT_FOUND && lw_SRowSetEnd($SRowSet))
    {
	$cRows = lw_SRowSetEnd($SRowSet);
    
	for (my $i = 0; $i != $cRows; $i++) {
	    my $obj_message = new_mapi_object();	
	
	    my $fid = int64_value(lw_getID($SRowSet, PR_FID, $i));
	    my $mid = int64_value(lw_getID($SRowSet, PR_MID, $i));

	    $retval = mapi_object_init($obj_message);
	    $retval = OpenMessage($obj_store, $fid, $mid, $obj_message);

	    if (GetLastError() != MAPI_E_NOT_FOUND) {
		my $props = new_mapi_SPropValue_array();

		$retval = GetPropsAll($obj_message, $props);
		mapidump_message($props);
		mapi_object_release($obj_message);
	    }
	}
	$retval = GetRowCount($obj_table, $count);
    }

    mapi_object_release($obj_table);
    mapi_object_release($obj_inbox);
    mapi_object_release($obj_store);
}

&mapi_init();
&mapi_fetchmail();
&mapi_finalize();
