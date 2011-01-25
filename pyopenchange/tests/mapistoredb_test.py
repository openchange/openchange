#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys

sys.path.append("python")

import os
import openchange
import openchange.mapistoredb as mapistoredb
import openchange.mapistore as mapistore
from openchange import mapi

os.mkdir("/tmp/mapistoredb");

def newTitle(title, separator):
    print ""
    print "%s" % title
    print separator * len(title)

newTitle("[Step 1]. Initializing mapistore database", '=')
MAPIStoreDB = mapistoredb.mapistoredb("/tmp/mapistoredb")

newTitle("[Step 2]. Provisioning mapistore database", '=')
ret = MAPIStoreDB.provision()
if (ret == 0):
    print "\t* Provisioning: SUCCESS"
else:
    print "\t* Provisioning: FAILURE"


newTitle("[Step 3]. Modify and dump configuration", '=')
MAPIStoreDB.netbiosname = "server"
MAPIStoreDB.firstorg = "OpenChange Project"
MAPIStoreDB.firstou = "OpenChange Development Unit"

MAPIStoreDB.dump_configuration()

newTitle("[Step 4]. Testing API parts", '=')

newTitle("A. Testing NetBIOS name", '-')
print "* NetBIOS name: %s" %MAPIStoreDB.netbiosname

newTitle("B. Testing First OU", '-')
print "* FirstOU: %s" %MAPIStoreDB.firstou

newTitle("C. Testing First Organisation", '-')
print "* First Organisation: %s" %MAPIStoreDB.firstorg


newTitle("[Step 5]. Retrieve mapistore URI for fsocpf", '=')
newTitle("*fsocpf:", '=')
print "\t* Inbox: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_INBOX, "jkerihuel", "fsocpf://")
print "\t* Calendar: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_CALENDAR, "jkerihuel", "fsocpf://")
print "\t* Outbox: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_OUTBOX, "jkerihuel", "fsocpf://")
print "\t* Contacts: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_CONTACTS, "jkerihuel", "fsocpf://")

newTitle("mstoredb:", '=')
print "\t* Mailbox Root: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_ROOT_FOLDER, "jkerihuel", "mstoredb://")
print "\t* IPM SUbtree: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_IPM_SUBTREE, "jkerihuel", "mstoredb://")
print "\t* Inbox: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_INBOX, "jkerihuel", "mstoredb://")

MAPIStoreDB.netbiosname = "new_server"
MAPIStoreDB.firstorg = "FirstOrg"
MAPIStoreDB.firstou = "FirstOu"

newTitle("mstoredb (new DN):", '=')
print "\t* Mailbox Root: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_ROOT_FOLDER, "jkerihuel", "mstoredb://")
print "\t* IPM SUbtree: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_IPM_SUBTREE, "jkerihuel", "mstoredb://")
print "\t* Inbox: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_INBOX, "jkerihuel", "mstoredb://")

newTitle("[Step 6]. Create a new mailbox", '=')
uri = MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_ROOT_FOLDER, "jkerihuel", "mstoredb://")
fid = MAPIStoreDB.get_new_fid()
ret = MAPIStoreDB.new_mailbox("jkerihuel", fid, uri)
print "\t* new_mailbox: ret = %d" % ret

newTitle("[Step 7]. Get and Set a new allocation range to the mailbox ", '=')
(rstart,rend) = MAPIStoreDB.get_new_allocation_range(0x1000)
ret = MAPIStoreDB.new_mailbox_allocation_range("jkerihuel", fid, rstart, rend)
print "\t* allocation range on mailbox: %d" % ret


newTitle("[Step 7]. Retrieve a new FID", '=')
fid = MAPIStoreDB.get_new_fid()
print "\t* FID = 0x%.16x" % fid

newTitle("[Step 8]. Retrieve a new allocation range", '=')
(rstart,rend) = MAPIStoreDB.get_new_allocation_range(0x1000)
print "\t* range_start = 0x%.16x" % rstart
print "\t* range_end   = 0x%.16x" % rend

newTitle("[Step 9]. Retrieve a new FID", '=')
new_fid = MAPIStoreDB.get_new_fid()
print "\t* FID = 0x%.16x" % new_fid

