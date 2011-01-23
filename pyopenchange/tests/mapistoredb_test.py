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

print "[Step 1]. Initializing mapistore database"
print "========================================="

MAPIStoreDB = mapistoredb.mapistoredb("/tmp/mapistoredb")
print ""

print "[Step 2]. Provisioning mapistore database"
print "========================================="
ret = MAPIStoreDB.provision()
if (ret == 0):
    print "Provisioning: SUCCESS"
else:
    print "Provisioning: FAILURE"
print ""

print "[Step 3]. Modify and dump configuration"
print "======================================="

MAPIStoreDB.netbiosname = "server"
MAPIStoreDB.firstorg = "OpenChange Project"
MAPIStoreDB.firstou = "OpenChange Development Unit"

MAPIStoreDB.dump_configuration()
print ""

print "[Step 4]. Testing API parts"
print "==========================="

print "A. Testing NetBIOS name"
print "-----------------------"
print "* NetBIOS name: %s" %MAPIStoreDB.netbiosname
print ""

print "B. Testing First OU"
print "-------------------"
print "* FirstOU: %s" %MAPIStoreDB.firstou
print ""

print "C. Testing First Organisation"
print "-----------------------------"
print "* First Organisation: %s" %MAPIStoreDB.firstorg
print ""


print "[Step 5]. Retrieve mapistore URI for fsocpf"
print "============================================"
print "*fsocpf:"
print "========"
print "\t* Inbox: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_INBOX, "jkerihuel", "fsocpf://")
print "\t* Calendar: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_CALENDAR, "jkerihuel", "fsocpf://")
print "\t* Outbox: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_OUTBOX, "jkerihuel", "fsocpf://")
print "\t* Contacts: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_CONTACTS, "jkerihuel", "fsocpf://")

print ""
print "mstoredb:"
print "========="
print "\t* Mailbox Root: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_ROOT_FOLDER, "jkerihuel", "mstoredb://")
print "\t* IPM SUbtree: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_IPM_SUBTREE, "jkerihuel", "mstoredb://")
print "\t* Inbox: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_INBOX, "jkerihuel", "mstoredb://")

MAPIStoreDB.netbiosname = "new_server"
MAPIStoreDB.firstorg = "FirstOrg"
MAPIStoreDB.firstou = "FirstOu"

print ""
print "mstoredb (new DN):"
print "=================="
print "\t* Mailbox Root: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_ROOT_FOLDER, "jkerihuel", "mstoredb://")
print "\t* IPM SUbtree: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_IPM_SUBTREE, "jkerihuel", "mstoredb://")
print "\t* Inbox: %s" % MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_INBOX, "jkerihuel", "mstoredb://")
