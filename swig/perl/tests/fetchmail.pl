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
    #lw_dumpdata();

    # Retrieve default profile name
    ($retval, $profname) = GetDefaultProfile();
    mapi_errstr("GetDefaultProfile", GetLastError());

    $retval = MapiLogonEx($session, $profname, $password);
    mapi_errstr("MapiLogonEx", $retval);

    return $session;
}

sub mapi_finalize()
{
    MAPIUninitialize();
}

sub mapi_fetchmail($)
{
    my $session = shift;
    my $inbox_id = new_int64();
    my $cRows;
    my $count = new_int32();
    my $unread = new_int32();
    my $total = new_int32();
    my $SRowSet = new mapi::SRowSet();

    ## Open Message Store
    my $obj_store = new_mapi_object();
    $retval = OpenMsgStore(mapi_session_t_value($session), $obj_store);
    mapi_errstr("OpenMsgStore", GetLastError());

    ## Open Inbox
    my $obj_inbox = new_mapi_object();
    $retval = GetDefaultFolder($obj_store, $inbox_id, $mapi::olFolderInbox);
    mapi_errstr("GetDefaultFolder", GetLastError());

    $retval = OpenFolder($obj_store, int64_value($inbox_id), $obj_inbox);
    mapi_errstr("OpenFolder", GetLastError());

    ## Count messages
    $retval = GetFolderItemsCount($obj_inbox, $unread, $total);
    mapi_errstr("GetFolderItemsCount", GetLastError());

    print "Mailbox:\n";
    print "\t => Unread(" . int32_value($unread) . ")\n";
    print "\t => Total(" . int32_value($total) . ")\n";

    ## GetContentsTable
    my $obj_table = new_mapi_object();
    $retval = GetContentsTable($obj_inbox, $obj_table, 0, $count);
    mapi_errstr("GetContentsTable", GetLastError());

    ## Prepare MAPI table creation
    my $SPropTagArray = new_SPropTagArray(5);
    aulPropTag_setitem($SPropTagArray->{aulPropTag}, 0, $mapi::PR_FID);
    aulPropTag_setitem($SPropTagArray->{aulPropTag}, 1, $mapi::PR_MID);
    aulPropTag_setitem($SPropTagArray->{aulPropTag}, 2, $mapi::PR_INST_ID);
    aulPropTag_setitem($SPropTagArray->{aulPropTag}, 3, $mapi::PR_INSTANCE_NUM);
    aulPropTag_setitem($SPropTagArray->{aulPropTag}, 4, $mapi::PR_SUBJECT);

    $retval = SetColumns($obj_table, $SPropTagArray);
    delete_SPropTagArray($SPropTagArray);
    mapi_errstr("SetColumns", GetLastError());

    ## Get table rows
    while (($retval = QueryRows($obj_table, int32_value($count), 0, $SRowSet)) != $mapi::MAPI_E_NOT_FOUND 
	   && $SRowSet->{cRows})
    {

	for (my $i = 0; $i != $SRowSet->{cRows}; $i++) {

	    my $fid = int64_value(lw_getID($SRowSet, $mapi::PR_FID, $i));
	    my $mid = int64_value(lw_getID($SRowSet, $mapi::PR_MID, $i));
	    
	    my $obj_message = new_mapi_object();
	    $retval = OpenMessage($obj_store, $fid, $mid, $obj_message, $mapi::MAPI_CREATE|$mapi::MAPI_MODIFY);
	    
	    if (GetLastError() != $mapi::MAPI_E_NOT_FOUND) {
		my $props = new_mapi_SPropValue_array();
		
		$retval = GetPropsAll($obj_message, $props);
		mapidump_message($props, "");
		delete_mapi_object($obj_message);
	    }
	}
    }

    delete_mapi_object($obj_table);
    delete_mapi_object($obj_inbox);
    delete_mapi_object($obj_store);
}

my $session = &mapi_init();
&mapi_fetchmail($session);
&mapi_finalize();
