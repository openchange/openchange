#!/usr/bin/python

import sys

sys.path.append("python")

import os
import openchange
import openchange.mapistore as mapistore
import openchange.mapistoredb as mapistoredb
from openchange import mapi

os.mkdir("/tmp/mapistore")

username = "testuser"

MAPIStoreDB = mapistoredb.mapistoredb()
MAPIStoreDB.mapping_path = "/tmp/mapistore"
MAPIStoreDB.database_path = "/tmp/mapistore/db.ldb"
MAPIStoreDB.namedprops_database_path = "/tmp/mapistore/named_props.ldb"

retval = MAPIStoreDB.initialize(None)
if (retval):
    print "%s" % MAPIStoreDB.errstr(retval)
    exit

retval = MAPIStoreDB.provision(netbiosname = "server", 
                               firstorg = "First Organization",
                               firstou = "First Organization Unit")
retval = MAPIStoreDB.provision_named_properties()
retval = MAPIStoreDB.namedprops_provision_user(username)

mailbox_root = MAPIStoreDB.get_mapistore_uri(folder=mapistoredb.MDB_ROOT_FOLDER,
                                             username=username, 
                                             namespace="mstoredb://")
retval = MAPIStoreDB.new_mailbox(username, mailbox_root)
MAPIStoreDB.release()


MAPIStore = mapistore.mapistore()
mapistore.debuglevel = 9

(context_id,mailbox_fid) = MAPIStore.add_context(username, mailbox_root)

root_uri = MAPIStore.get_mapistore_uri(mapistore.MDB_ROOT_FOLDER, username, "fsocpf://")
retval = MAPIStore.set_mapistore_uri(context_id, mapistore.MDB_ROOT_FOLDER, root_uri)

(ctx_id,mailbox_fid) = MAPIStore.add_context(username, root_uri)

SPropParent = mapi.SPropValue()
SPropParent.add(mapi.PidTagMessageClass, "IPM.Note")
SPropParent.add(mapi.PidTagDisplayName, "Parent")
SPropParent.add(mapi.PidTagComment, "Test comment")
SPropParent.add(mapi.PidTagFolderType, 1)
MAPIStore.setprops(ctx_id, mailbox_fid, mapistore.MAPISTORE_FOLDER, SPropParent)

subfolder_fid = MAPIStore.mkdir(ctx_id, mailbox_fid, "Child", "Child Folder", mapistore.FOLDER_GENERIC)
SPropValue = mapi.SPropValue()
MAPIStore.setprops(ctx_id, subfolder_fid, mapistore.MAPISTORE_FOLDER, SPropValue)

delfid = MAPIStore.mkdir(ctx_id, mailbox_fid, "Delete Me", "Folder to delete", mapistore.FOLDER_GENERIC)
MAPIStore.rmdir(ctx_id, mailbox_fid, delfid, mapistore.DEL_FOLDERS)

MAPIStore.del_context(ctx_id)

