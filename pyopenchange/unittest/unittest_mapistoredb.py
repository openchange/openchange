#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import openchange
import openchange.mapistoredb as mapistoredb
import openchange.mapistore as mapistore
import openchange.mapi as mapi
import unittest
import shutil
import tempfile

folder_list = [ (mapistore.MDB_ROOT_FOLDER, mapistore.MDB_DEFERRED_ACTIONS),
		(mapistore.MDB_ROOT_FOLDER, mapistore.MDB_SPOOLER_QUEUE),
		(mapistore.MDB_ROOT_FOLDER, mapistore.MDB_IPM_SUBTREE),
		(mapistore.MDB_IPM_SUBTREE, mapistore.MDB_INBOX),
		(mapistore.MDB_IPM_SUBTREE, mapistore.MDB_OUTBOX),
		(mapistore.MDB_IPM_SUBTREE, mapistore.MDB_SENT_ITEMS),
		(mapistore.MDB_IPM_SUBTREE, mapistore.MDB_DELETED_ITEMS),
		(mapistore.MDB_IPM_SUBTREE, mapistore.MDB_CALENDAR),
		(mapistore.MDB_IPM_SUBTREE, mapistore.MDB_CONTACTS),
		(mapistore.MDB_IPM_SUBTREE, mapistore.MDB_JOURNAL),
		(mapistore.MDB_IPM_SUBTREE, mapistore.MDB_NOTES),
		(mapistore.MDB_IPM_SUBTREE, mapistore.MDB_TASKS),
		(mapistore.MDB_IPM_SUBTREE, mapistore.MDB_DRAFTS),
		(mapistore.MDB_IPM_SUBTREE, mapistore.MDB_SYNC_ISSUES),
		(mapistore.MDB_ROOT_FOLDER, mapistore.MDB_COMMON_VIEWS),
		(mapistore.MDB_ROOT_FOLDER, mapistore.MDB_SCHEDULE),
		(mapistore.MDB_ROOT_FOLDER, mapistore.MDB_SEARCH),
		(mapistore.MDB_ROOT_FOLDER, mapistore.MDB_VIEWS),
		(mapistore.MDB_ROOT_FOLDER, mapistore.MDB_SHORTCUTS),
		(mapistore.MDB_ROOT_FOLDER, mapistore.MDB_REMINDERS) ]
    

class TestMAPIStoreDB(unittest.TestCase):

	def setUp(self):
		self.username = "jkerihuel"
		self.working_directory = tempfile.mkdtemp(prefix="TestMAPIStoreDB")
		self.MAPIStoreDB = mapistoredb.mapistoredb()
		self.assert_(self.MAPIStoreDB)
		retval = self.MAPIStoreDB.initialize(self.working_directory)
		self.assertEqual(retval, 0)
		retval = self.MAPIStoreDB.provision(netbiosname = "server", firstorg = "OpenChange Project", firstou = "OpenChange Development Unit")
		self.assertEqual(retval, 0)
		retval = self.MAPIStoreDB.provision_named_properties()
		self.assertEqual(retval, 0) # success
		(retval,npid) = self.MAPIStoreDB.namedprops_get_default_id(mapistoredb.MAPISTORE_NAMEDPROPS_EXTERNAL)
		self.assertEqual(retval, 0) # success
		self.assertNotEqual(npid, 0) # we have at least one external property id
		(retval,npid) = self.MAPIStoreDB.namedprops_get_default_id(mapistoredb.MAPISTORE_NAMEDPROPS_INTERNAL)
		self.assertEqual(retval, 0) # success
		self.assertNotEqual(npid, 0) # we have at least one internal property id
		retval = self.MAPIStoreDB.namedprops_provision_backends()
		self.assertEqual(retval, 0) # success
		retval = self.MAPIStoreDB.namedprops_provision_user(self.username)
		self.assertEqual(retval, 0) # success
		self.mailbox_root = self.MAPIStoreDB.get_mapistore_uri(folder=mapistoredb.MDB_ROOT_FOLDER, username=self.username, namespace="mstoredb://")

	def test_get_parameters(self):
		netbiosname = self.MAPIStoreDB.netbiosname
		self.assertEqual(netbiosname, "server")
		firstorg = self.MAPIStoreDB.firstorg
		self.assertEqual(firstorg, "OpenChange Project")
		firstou = self.MAPIStoreDB.firstou
		self.assertEqual(firstou, "OpenChange Development Unit")

	def test_allocation_range(self):
		retval = self.MAPIStoreDB.new_mailbox(self.username, self.mailbox_root)
		self.assertEqual(retval, 0) # success
		(retval, rstart, rend) = self.MAPIStoreDB.get_new_allocation_range(self.username, 0x1000)
		self.assertEqual(retval, 0) # success
		self.assertNotEqual(rstart, 0) # should be greater than zero
		self.assertEqual(rend-rstart, (0x1000 - 1) << 16) # because ranges are inclusive
		retval = self.MAPIStoreDB.set_mailbox_allocation_range(self.username, rstart, rend)
		self.assertEqual(retval, 0) # success

	def test_get_new_fid(self):
		retval = self.MAPIStoreDB.new_mailbox(self.username, self.mailbox_root)
		self.assertEqual(retval, 0) # success
		fid = self.MAPIStoreDB.get_new_fid(self.username)
		self.assertNotEqual(fid, 0)
		
	def test_dump_config(self):
		retval = self.MAPIStoreDB.dump_configuration()
		self.assertEqual(retval, 0) # success

	def test_folder_context(self):
		mapistore.set_mapping_path(os.path.join(self.working_directory, "mapistore"))
		self.MAPIStore = mapistore.mapistore()
		retval = self.MAPIStoreDB.new_mailbox(self.username, self.mailbox_root)
		self.assertEqual(retval, 0) # success
		(context_id, mailbox_fid) = self.MAPIStore.add_context(self.username, self.mailbox_root)
		self.assertNotEqual(context_id, 0)
		self.assertNotEqual(mailbox_fid, 0)
		for (parent, index) in folder_list:
			retval = self.MAPIStore.root_mkdir(context_id=context_id, parent_index=parent, index=index, folder_name="")
			self.assertEqual(retval, 0, mapistore.errstr(retval))
		ipm_subtree_fid = self.MAPIStore.get_folder_identifier(context_id=context_id, index=mapistore.MDB_IPM_SUBTREE, uri=None)
		self.assertNotEqual(ipm_subtree_fid, 0)
		sync_fid = self.MAPIStore.get_folder_identifier(context_id=context_id, index=mapistore.MDB_SYNC_ISSUES, uri=None)
		self.assertNotEqual(sync_fid, 0)
		retval = self.MAPIStore.opendir(context_id=context_id, parent_fid=ipm_subtree_fid, fid=sync_fid)
		self.assertEqual(retval, 0)
		conflicts_fid = self.MAPIStore.mkdir(context_id, sync_fid, "Conflicts", None, mapistore.FOLDER_GENERIC)
		self.assertNotEqual(conflicts_fid, 0)
		inbox_uri = self.MAPIStore.get_mapistore_uri(mapistore.MDB_INBOX, self.username, "fsocpf://")
		retval = self.MAPIStore.set_mapistore_uri(context_id, mapistore.MDB_INBOX, inbox_uri)
		self.assertEqual(retval, 0)
		(inbox_context_id, inbox_fid) = self.MAPIStore.add_context(self.username, inbox_uri)
		self.assertNotEqual(inbox_context_id, 0)
		self.assertNotEqual(inbox_fid, 0)
		num_folders = self.MAPIStore.get_folder_count(inbox_context_id, inbox_fid)
		self.assertEqual(num_folders, 0);
		self.MAPIStore.debuglevel = 99
		test_subfolder_fid = self.MAPIStore.mkdir(inbox_context_id, inbox_fid, "Test Folder", "This is a test folder", mapistore.FOLDER_GENERIC)
		self.assertNotEqual(test_subfolder_fid, 0)
		num_folders = self.MAPIStore.get_folder_count(inbox_context_id, inbox_fid)
		self.assertEqual(num_folders, 1);
		num_folders = self.MAPIStore.get_folder_count(inbox_context_id, test_subfolder_fid)
		self.assertEqual(num_folders, 0);
		retval = self.MAPIStore.opendir(context_id = inbox_context_id, parent_fid = inbox_fid, fid = test_subfolder_fid)
		self.assertEqual(retval, 0)
		# TODO: add getprops support, and check return values.
		SPropValue = mapi.SPropValue()
		SPropValue.add(mapi.PidTagComment, "different comment")
		SPropValue.add(mapi.PidTagGeneration, "doesn't already exist")
		retval = self.MAPIStore.setprops(inbox_context_id, test_subfolder_fid, mapistore.MAPISTORE_FOLDER, SPropValue)
		self.assertEqual(retval, 0, self.MAPIStoreDB.errstr(retval))
		SPropValue = mapi.SPropValue()
		SPropValue.add(mapi.PidTagGeneration, "the first")
		retval = self.MAPIStore.setprops(inbox_context_id, inbox_fid, mapistore.MAPISTORE_FOLDER, SPropValue)
		self.assertEqual(retval, 0, self.MAPIStoreDB.errstr(retval))
		SPropValue = mapi.SPropValue()
		SPropValue.add(mapi.PidTagComment, "Inbox comment")
		SPropValue.add(mapi.PidTagGeneration, "the second")
		retval = self.MAPIStore.setprops(inbox_context_id, inbox_fid, mapistore.MAPISTORE_FOLDER, SPropValue)
		self.assertEqual(retval, 0, self.MAPIStoreDB.errstr(retval))
		retval = self.MAPIStore.closedir(inbox_context_id, test_subfolder_fid)
		self.assertEqual(retval, 0, self.MAPIStoreDB.errstr(retval))
		retval = self.MAPIStore.rmdir(inbox_context_id, inbox_fid, test_subfolder_fid, mapistore.DEL_FOLDERS)
		self.assertEqual(retval, 0, self.MAPIStoreDB.errstr(retval))

	def test_errstr(self):
		self.assertEqual(self.MAPIStoreDB.errstr(0), "Success")
		self.assertEqual(self.MAPIStoreDB.errstr(1), "Non-specific error")
		self.assertEqual(self.MAPIStoreDB.errstr(2), "No memory available")
		self.assertEqual(self.MAPIStoreDB.errstr(16), "Already exists")

	def tearDown(self):
		self.MAPIStoreDB.release()
		# shutil.rmtree(self.working_directory)

if __name__ == '__main__':
	unittest.main()
