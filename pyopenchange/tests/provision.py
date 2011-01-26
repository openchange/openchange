#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys

import os
import openchange
import openchange.mapistoredb as mapistoredb
import openchange.mapistore as mapistore
from openchange import mapi

username = "jkerihuel"

os.mkdir("/tmp/mapistoredb");

def newTitle(title, separator):
    print ""
    print "%s" % title
    print separator * len(title)

newTitle("[Step 1]. Initializing mapistore database", '=')
MAPIStoreDB = mapistoredb.mapistoredb("/tmp/mapistoredb")

newTitle("[Step 2]. Provisioning mapistore database", '=')
ret = MAPIStoreDB.provision(netbiosname = "server",
                            firstorg = "OpenChange Project",
                            firstou = "OpenChange Development Unit")
if (ret == 0):
    print "\t* Provisioning: SUCCESS"
else:
    print "\t* Provisioning: FAILURE"



newTitle("[Step 3]. Create a new mailbox", '=')
mailbox_root = MAPIStoreDB.get_mapistore_uri(mapistoredb.MDB_ROOT_FOLDER,
                                             username, "mstoredb://")
retval = MAPIStoreDB.new_mailbox(username, mailbox_root)
print "\t* Mailbox Root: %s" % mailbox_root

newTitle("[Step 4]. Get and Set a new allocation range to the mailbox", '=')
(retval, rstart,rend) = MAPIStoreDB.get_new_allocation_range(username,
                                                             0x1000);
if retval == 0:
    print "\t* Allocation range received:"
    print "\t\t* [start]: 0x%.16x" % rstart
    print "\t\t* [end]:   0x%.16x\n\n" % rend
    retval = MAPIStoreDB.set_mailbox_allocation_range(username, 
                                                      rstart, rend)
    if retval == 0:
        print "\t * Allocation range successfully set to mailbox"
    else:
        print "\t * Error while setting allocation range to the mailbox"

newTitle("[Step 5]. Uninitializing mapistore database context", '=')
MAPIStoreDB.release()

newTitle("[Step 6]. Initializing mapistore context", '=')
mapistore.set_mapping_path("/tmp/mapistoredb/mapistore")
MAPIStore = mapistore.mapistore()

newTitle("[Step 7]. Adding a context over Mailbox Root Folder", '=')
context_id = MAPIStore.add_context(username, mailbox_root)
print "\t* context_id = 0x%x" % context_id
