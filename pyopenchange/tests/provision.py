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
title_nb = 1

os.mkdir("/tmp/mapistoredb");

def newTitle(title, separator):
    global title_nb
    ftitle = "[Step %d]. %s" % (title_nb, title)
    print ""
    print ftitle
    print separator * len(ftitle)
    title_nb += 1

def TreeRoot(name):
    print ""
    str = "[+] %s" % name
    print str
    print "=" * len(str)

def TreeLeafStatus(name, retval, errstr):
    tree = " " * (indent * 8)
    if (retval):
        print tree + "[+] %s: %s (%s)" % (string.ljust(name, 60 - indent * 8), "[KO]", errstr)
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
    newTitle("Initializing mapistore database", '=')
    MAPIStoreDB = mapistoredb.mapistoredb("/tmp/mapistoredb")

    newTitle("Provisioning mapistore database", '=')
    retval = MAPIStoreDB.provision(netbiosname = "server",
                                   firstorg = "OpenChange Project",
                                   firstou = "OpenChange Development Unit")
    if (retval == 0):
        print "\t* Provisioning: SUCCESS"
    else:
        print "\t* Provisioning: FAILURE (ret = %d)" % MAPIStoreDB.errstr(retval)

    newTitle("Create mapistore named properties database", '=')
    retval = MAPIStoreDB.provision_named_properties()
    if (retval == 0):
        print "\t* Provisioning: SUCCESS"
        (retval,npid) = MAPIStoreDB.namedprops_get_default_id(mapistoredb.MAPISTORE_NAMEDPROPS_EXTERNAL)
        print "\t\t* Last External Index: 0x%.4x" % (npid)
        (retval,npid) = MAPIStoreDB.namedprops_get_default_id(mapistoredb.MAPISTORE_NAMEDPROPS_INTERNAL)
        print "\t\t* Last Internal Index: 0x%.4x" % (npid)
    else:
        print "\t* Provisioning: FAILURE (ret = %s)" % MAPIStoreDB.errstr(retval)

    newTitle("Provision named properties namespace for backends", "=")
    retval = MAPIStoreDB.namedprops_provision_backends()

    newTitle("Provision named properties namespace for user", "=")
    retval = MAPIStoreDB.namedprops_provision_user("jkerihuel")
    "\t* Return status: %s" % MAPIStoreDB.errstr(retval)

    newTitle("Create a new mailbox", '=')
    mailbox_root = MAPIStoreDB.get_mapistore_uri(folder=mapistoredb.MDB_ROOT_FOLDER,
                                                 username=username, 
                                                 namespace="mstoredb://")
    retval = MAPIStoreDB.new_mailbox(username, mailbox_root)
    print "\t* Mailbox Root: %s" % mailbox_root
    
    newTitle("Get and Set a new allocation range to the mailbox", '=')
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
            
    newTitle("Uninitializing mapistore database context", '=')
    MAPIStoreDB.release()
                
    newTitle("Initializing mapistore context", '=')
    mapistore.set_mapping_path("/tmp/mapistoredb/mapistore")
    MAPIStore = mapistore.mapistore()
    
    newTitle("Adding a context over Mailbox Root Folder", '=')
    context_id = MAPIStore.add_context(username, mailbox_root)
    print "\t* context_id = %s" % context_id

    newTitle("Create the mailbox hierarchy", '=')
    
    TreeRoot("Mailbox Root Folder")
    
    # MDB_DEFERRED_ACTIONS
    retval = MAPIStore.root_mkdir(context_id=context_id,
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_DEFERRED_ACTIONS, 
                                  folder_name="");
    TreeLeafStatus("Deferred Actions", retval, MAPIStore.errstr(retval))
    
    # MDB_SPOOLER_QUEUE
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_SPOOLER_QUEUE, 
                                  folder_name="");
    TreeLeafStatus("Spooler Queue", retval, MAPIStore.errstr(retval))
    
    # MDB_IPM_SUBTREE
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_IPM_SUBTREE, 
                                  folder_name="")
    TreeLeafStatus("IPM Subtree", retval, MAPIStore.errstr(retval))
    
    # MDB_IPM_SUBTREE > MDB_INBOX
    TreeIndent()
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_INBOX, 
                                  folder_name="")
    TreeLeafStatus("Inbox", retval, MAPIStore.errstr(retval))
    
    # MDB_IPM_SUBTREE > MDB_OUTBOX
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_OUTBOX, 
                                  folder_name="")
    TreeLeafStatus("Outbox", retval, MAPIStore.errstr(retval))
    
    # MDB_IPM_SUBTREE > MDB_SENT_ITEMS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_SENT_ITEMS, 
                                  folder_name="")
    TreeLeafStatus("Sent Items", retval, MAPIStore.errstr(retval))
    
    # MDB_IPM_SUBTREE > MDB_DELETED_ITEMS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_DELETED_ITEMS, 
                                  folder_name="")
    TreeLeafStatus("Deleted Items", retval, MAPIStore.errstr(retval))
    
#### SPECIAL FOLDERS #####
        
    # MDB_IPM_SUBTREE > MDB_CALENDAR
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_CALENDAR, 
                                  folder_name="")
    TreeLeafStatus("Calendar", retval, MAPIStore.errstr(retval))
        
    # MDB_IPM_SUBTREE > MDB_CONTACTS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_CONTACTS, 
                                  folder_name="")
    TreeLeafStatus("Contacts", retval, MAPIStore.errstr(retval))
        
    # MDB_IPM_SUBTREE > MDB_JOURNAL
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_JOURNAL, 
                                  folder_name="")
    TreeLeafStatus("Journal", retval, MAPIStore.errstr(retval))
        
    # MDB_IPM_SUBTREE > MDB_NOTES
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_NOTES, 
                                  folder_name="")
    TreeLeafStatus("Notes", retval, MAPIStore.errstr(retval))

    # MDB_IPM_SUBTREE > MDB_TASKS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_TASKS, 
                                  folder_name="")
    TreeLeafStatus("Tasks", retval, MAPIStore.errstr(retval))

    # MDB_IPM_SUBTREE > MDB_DRAFTS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_IPM_SUBTREE, 
                                  index=mapistore.MDB_DRAFTS, 
                                  folder_name="")
    TreeLeafStatus("Drafts", retval, MAPIStore.errstr(retval))
    
    TreeDeIndent()
    
    # MDB_COMMON_VIEWS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_COMMON_VIEWS, 
                                  folder_name="")
    TreeLeafStatus("Common Views", retval, MAPIStore.errstr(retval))

    # MDB_SCHEDULE
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_SCHEDULE, 
                                  folder_name="")
    TreeLeafStatus("Schedule", retval, MAPIStore.errstr(retval))

    # MDB_SEARCH
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_SEARCH, 
                                  folder_name="")
    TreeLeafStatus("Search", retval, MAPIStore.errstr(retval))

    # MDB_VIEWS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_VIEWS, 
                                  folder_name="")
    TreeLeafStatus("Views", retval, MAPIStore.errstr(retval))

    # MDB_SHORTCUTS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_SHORTCUTS, 
                                  folder_name="")
    TreeLeafStatus("Shortcuts", retval, MAPIStore.errstr(retval))

    # MDB_REMINDERS
    retval = MAPIStore.root_mkdir(context_id=context_id, 
                                  parent_index=mapistore.MDB_ROOT_FOLDER, 
                                  index=mapistore.MDB_REMINDERS, 
                                  folder_name="")
    TreeLeafStatus("Reminders", retval, MAPIStore.errstr(retval))

    newTitle("Setting mailbox folders to fsocpf", '=')
    new_uri = MAPIStore.get_mapistore_uri(mapistoredb.MDB_INBOX,
                                          username, 
                                          "fsocpf://")
    retval = MAPIStore.set_mapistore_uri(context_id, mapistore.MDB_INBOX, new_uri)
    if retval == 0:
        print "\t* Inbox [fsocpf]: [OK]"
    else:
        print "\t* Inbox [fsocpf]: [KO] (%s)" % MAPIStore.errstr(retval)

start()
