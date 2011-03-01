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
#SPropParent.add(mapi.PidTagFolderId, mailbox_fid)
SPropParent.add(mapi.PidTagDisplayName, "Parent")
SPropParent.add(mapi.PidTagComment, "Test comment")
SPropParent.add(mapi.PidTagFolderType, 1)
MAPIStore.setprops(ctx_id, mailbox_fid, mapistore.MAPISTORE_FOLDER, SPropParent)

#SPropValue = mapi.SPropValue()
#SPropValue.add(mapi.PR_PARENT_FID, 0x0000000000010001)
#SPropValue.add(mapi.PR_DISPLAY_NAME, "test")
#SPropValue.add(mapi.PR_COMMENT, "test folder")
#SPropValue.add(mapi.PR_FOLDER_TYPE, 1)

#MAPIStore.mkdir(ctx_id, 0x0000000000010001, 0x0000000000020001, SPropValue)
#MAPIStore.rmdir(ctx_id, 0x0000000000010001, 0x0000000000020001, mapistore.DEL_FOLDERS)

MAPIStore.del_context(ctx_id)

