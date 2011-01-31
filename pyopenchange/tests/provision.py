#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys

import os
import openchange
import openchange.mapistoredb as mapistoredb
import openchange.mapistore as mapistore
from openchange import mapi
import string

username = "jkerihuel"
indent = 1

os.mkdir("/tmp/mapistoredb");

def newTitle(title, separator):
    print ""
    print "%s" % title
    print separator * len(title)

def TreeRoot(name):
    print ""
    str = "[+] %s" % name
    print str
    print "=" * len(str)

def TreeLeafStatus(name, retval):
    tree = " " * (indent * 8)
    if (retval):
        print tree + "[+] %s: %s" % (string.ljust(name, 60 - indent * 8), "[KO]")
        exit
    else:
        print tree + "[+] %s: %s" % (string.ljust(name, 60 - indent * 8), "[OK]")

def TreeIndent():
    global indent
    indent += 1

def TreeDeIndent():
    global indent
    indent -= 1

def start():
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
    mailbox_root = MAPIStoreDB.get_mapistore_uri(folder=mapistoredb.MDB_ROOT_FOLDER,
                                                 username=username, 
                                                 namespace="mstoredb://")
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
    print "\t* context_id = %s" % context_id

    newTitle("[Step 8]. Create the mailbox hierarchy", '=')
    
    TreeRoot("Mailbox Root Folder")
    
    # MDB_DEFERRED_ACTIONS
    retval = MAPIStore.root_mkdir(context_id=context_id,
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_DEFERRED_ACTIONS, 
                                  folder_name="");
    TreeLeafStatus("Deferred Actions", retval)
    
    # MDB_SPOOLER_QUEUE
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_SPOOLER_QUEUE, 
                                  folder_name="");
    TreeLeafStatus("Spooler Queue", retval)
    
    # MDB_IPM_SUBTREE
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_IPM_SUBTREE, 
                                  folder_name="")
    TreeLeafStatus("IPM Subtree", retval)
    
    # MDB_IPM_SUBTREE > MDB_INBOX
    TreeIndent()
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_INBOX, 
                                  folder_name="")
    TreeLeafStatus("Inbox", retval)
    
    # MDB_IPM_SUBTREE > MDB_OUTBOX
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_OUTBOX, 
                                  folder_name="")
    TreeLeafStatus("Outbox", retval)
    
    # MDB_IPM_SUBTREE > MDB_SENT_ITEMS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_SENT_ITEMS, 
                                  folder_name="")
    TreeLeafStatus("Sent Items", retval)
    
    # MDB_IPM_SUBTREE > MDB_DELETED_ITEMS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_DELETED_ITEMS, 
                                  folder_name="")
    TreeLeafStatus("Deleted Items", retval)
    
#### SPECIAL FOLDERS #####
        
    # MDB_IPM_SUBTREE > MDB_CALENDAR
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_CALENDAR, 
                                  folder_name="")
    TreeLeafStatus("Calendar", retval)
        
    # MDB_IPM_SUBTREE > MDB_CONTACTS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_CONTACTS, 
                                  folder_name="")
    TreeLeafStatus("Contacts", retval)
        
    # MDB_IPM_SUBTREE > MDB_JOURNAL
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_JOURNAL, 
                                  folder_name="")
    TreeLeafStatus("Journal", retval)
        
    # MDB_IPM_SUBTREE > MDB_NOTES
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_NOTES, 
                                  folder_name="")
    TreeLeafStatus("Notes", retval)

    # MDB_IPM_SUBTREE > MDB_TASKS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_TASKS, 
                                  folder_name="")
    TreeLeafStatus("Tasks", retval)

    # MDB_IPM_SUBTREE > MDB_DRAFTS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_DRAFTS, 
                                  folder_name="")
    TreeLeafStatus("Drafts", retval)
    
    TreeDeIndent()
    
    # MDB_COMMON_VIEWS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_COMMON_VIEWS, 
                                  folder_name="")
    TreeLeafStatus("Common Views", retval)

    # MDB_SCHEDULE
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_SCHEDULE, 
                                  folder_name="")
    TreeLeafStatus("Schedule", retval)

    # MDB_SEARCH
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_SEARCH, 
                                  folder_name="")
    TreeLeafStatus("Search", retval)

    # MDB_VIEWS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_VIEWS, 
                                  folder_name="")
    TreeLeafStatus("Views", retval)

    # MDB_SHORTCUTS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_SHORTCUTS, 
                                  folder_name="")
    TreeLeafStatus("Shortcuts", retval)

    # MDB_REMINDERS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_REMINDERS, 
                                  folder_name="")
    TreeLeafStatus("Reminders", retval)

start()
