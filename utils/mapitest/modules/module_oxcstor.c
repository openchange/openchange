/*
   Stand-alone MAPI testsuite

   OpenChange Project - STORE OBJECT PROTOCOL operations

   Copyright (C) Julien Kerihuel 2008

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "utils/mapitest/mapitest.h"
#include "utils/mapitest/proto.h"
#include "libmapi/libmapi_private.h"

/**
   \file module_oxcstor.c

   \brief Store Object Protocol test suite
*/


/**
   \details Test the Logon (0xFE) operation

   This function:
   -# Log on the user private mailbox
   -# Log on the public folder store

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcstor_Logon(struct mapitest *mt)
{
	mapi_object_t		obj_store;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	mapi_object_release(&obj_store);

	/* Step 2. Logon Public Folder store */
	mapi_object_init(&obj_store);
	OpenPublicFolder(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenPublicFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the GetReceiveFolder (0x27) operation

   This function:
   -# Log on the user private mailbox
   -# Call the GetReceiveFolder operation
   -# Call the GetReceiveFolder with different explicit message class values

   \param mt the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcstor_GetReceiveFolder(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_id_t		receivefolder = 0;
	bool			ret = true;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval_clean(mt, "OpenMsgStore", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}
	
	/* Step 2. Call the GetReceiveFolder operation */
	retval = GetReceiveFolder(&obj_store, &receivefolder, NULL);
	mapitest_print_retval_clean(mt, "GetReceiveFolder for All target", retval);
	mapitest_print(mt, "FID: 0x%016"PRIx64"\n", receivefolder);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}

	/* Step 3. Call GetReceiveFolder again, with an explicit message class */
	retval = GetReceiveFolder(&obj_store, &receivefolder, "IPC");
	mapitest_print_retval_clean(mt, "GetReceiveFolder for IPC", retval);
	mapitest_print(mt, "FID: 0x%016"PRIx64"\n", receivefolder);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}

	/* Step 4. Call GetReceiveFolder again, with an explicit message class */
	retval = GetReceiveFolder(&obj_store, &receivefolder, "IPM.FooBarBaz");
	mapitest_print_retval_clean(mt, "GetReceiveFolder for IPM.FooBarBaz", retval);
	mapitest_print(mt, "FID: 0x%016"PRIx64"\n", receivefolder);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}

	/* Step 5. Call GetReceiveFolder again, with an explicit message class */
	retval = GetReceiveFolder(&obj_store, &receivefolder, "MT.Mapitest.tc");
	mapitest_print_retval_clean(mt, "GetReceiveFolder for MT.Mapitest.tc", retval);
	mapitest_print(mt, "FID: 0x%016"PRIx64"\n", receivefolder);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}

release:
	mapi_object_release(&obj_store);

	return ret;
}


/**
   \details Test the SetReceiveFolder (0x26) operation

   This function:
   -# Log on the user private mailbox
   -# Call the SetReceiveFolder operations
   -# Clean up

   \param mt the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcstor_SetReceiveFolder(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_id_t		id_inbox;
	mapi_id_t		id_tis;
	mapi_object_t		obj_tis;
	mapi_object_t		obj_inbox;
	mapi_object_t		obj_folder;
	bool			ret = true;


	mapi_object_init(&obj_store);
	mapi_object_init(&obj_inbox);
	mapi_object_init(&obj_tis);
	mapi_object_init(&obj_folder);

	/* Step 1. Logon */

	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval_step(mt, "1.", "Logon", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 2. Get the original ReceiveFolder */
	retval = GetReceiveFolder(&obj_store, &id_inbox, NULL);
	mapitest_print_retval_step(mt, "2.", "GetReceiveFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3. Open the ReceiveFolder */
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapitest_print_retval_step(mt, "3.", "OpenFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 4. Open the Top Information Store folder */
	retval = GetDefaultFolder(&obj_store, &id_tis, olFolderTopInformationStore);
	mapitest_print_retval_step(mt, "4.", "GetDefaultFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = OpenFolder(&obj_store, id_tis, &obj_tis);
	mapitest_print_retval_step(mt, "4.1.", "OpenFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Create the New Inbox folder under Top Information Store */
	retval = CreateFolder(&obj_tis, FOLDER_GENERIC, "New Inbox", NULL,
			      OPEN_IF_EXISTS, &obj_folder);
	mapitest_print_retval_step(mt, "5.", "CreateFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Set IPM.Note receive folder to New Inbox */
	retval = SetReceiveFolder(&obj_store, &obj_folder, "IPM.Note");
	mapitest_print_retval_step_fmt(mt, "6.", "SetReceiveFolder", "%s", "(New Inbox)", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Reset receive folder to Inbox */
	retval = SetReceiveFolder(&obj_store, &obj_inbox, "IPM.Note");
	mapitest_print_retval_step_fmt(mt, "6.1.", "SetReceiveFolder", "%s", "(original folder)", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Set a test message class */
	retval = SetReceiveFolder(&obj_store, &obj_folder, "MT.Mapitest.ta");
	mapitest_print_retval_step_fmt(mt, "7.", "SetReceiveFolder", "%s", "(MT.Mapitest.ta)", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Delete New Inbox folder */
	retval = EmptyFolder(&obj_folder);
	mapitest_print_retval_step(mt, "8.", "EmptyFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = DeleteFolder(&obj_tis, mapi_object_get_id(&obj_folder),
			      DEL_FOLDERS | DEL_MESSAGES | DELETE_HARD_DELETE, NULL);
	mapitest_print_retval_step(mt, "9.", "DeleteFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

cleanup:
	/* Release */
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_tis);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);

	return ret;
}


/**
   \details Test the GetOwningServers (0x42) operation

   This function:
   -# Log on the public folders store
   -# Open a public folder
   -# Call the GetOwningServers operation

   \param mt the top-level mapitest structure

   \return true on success, otherwise false
*/
_PUBLIC_ bool mapitest_oxcstor_GetOwningServers(struct mapitest *mt)
{
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	uint64_t		folderId;
	uint16_t		OwningServersCount;
	uint16_t		CheapServersCount;
	char			*OwningServers;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	OpenPublicFolder(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenPublicFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	
	/* Step 2. Open IPM Subtree folder */
	GetDefaultPublicFolder(&obj_store, &folderId, olFolderPublicIPMSubtree);
	mapitest_print_retval(mt, "GetDefaultPublicFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	mapi_object_init(&obj_folder);
	OpenFolder(&obj_store, folderId, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3. Call GetOwningServers */
	GetOwningServers(&obj_store, &obj_folder, &OwningServersCount, &CheapServersCount, &OwningServers);
	mapitest_print_retval(mt, "GetOwningServers");
	if (GetLastError() != MAPI_E_SUCCESS && GetLastError() != ecNoReplicaAvailable) {
		ret = false;
	} else if (GetLastError() == ecNoReplicaAvailable) {
		mapitest_print(mt, "* %-35s: No active replica for the folder\n", "GetOwningServers");
	} else {
		mapitest_print(mt, "* %-35s: OwningServersCount: %d\n", "PublicFolderIsGhosted", OwningServersCount);
		if (OwningServersCount) {
			uint16_t	i;
			
			for (i = 0; i < OwningServersCount; i++) {
				mapitest_print(mt, "* %-35s: OwningServers: %s\n", "GetOwningServers", &OwningServers[i]);
			}
			talloc_free(&OwningServers);
		}
	}

	/* cleanup objects */
	mapi_object_release(&obj_folder);

cleanup:
	mapi_object_release(&obj_store);

	return ret;
}

/**
   \details Test the PublicFolderIsGhosted (0x45) operation

   This function:
   -# Log on the public folders store
   -# Open a public folder
   -# Call the PublicFolderIsGhosted operation

   \param mt the top-level mapitest structure

   \return true on success, otherwise false
*/
_PUBLIC_ bool mapitest_oxcstor_PublicFolderIsGhosted(struct mapitest *mt)
{
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	uint64_t		folderId;
	bool			IsGhosted;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_folder);

	OpenPublicFolder(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenPublicFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	
	/* Step 2. Open IPM Subtree folder */
	GetDefaultPublicFolder(&obj_store, &folderId, olFolderPublicIPMSubtree);
	mapitest_print_retval(mt, "GetDefaultPublicFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	OpenFolder(&obj_store, folderId, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3. Call PublicFolderIsGhosted */
	PublicFolderIsGhosted(&obj_store, &obj_folder, &IsGhosted);
	mapitest_print_retval(mt, "PublicFolderIsGhosted");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	mapitest_print(mt, "* %-35s: IsGhosted is set to %s\n", "PublicFolderIsGhosted", ((IsGhosted) ? "true" : "false"));

cleanup:
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return ret;
}


/**
   \details Test the GetReceiveFolderTable (0x68) operation

   This function:
   -# Log on the user private mailbox
   -# Call the GetReceiveFolderTable operation
   
   \param mt the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcstor_GetReceiveFolderTable(struct mapitest *mt)
{
	mapi_object_t		obj_store;
	struct SRowSet		SRowSet;
	
 	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Call the GetReceiveFolderTable operation */
	GetReceiveFolderTable(&obj_store, &SRowSet);
	mapitest_print_retval(mt, "GetReceiveFolderTable");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapitest_print_SRowSet(mt, &SRowSet, "\t\t[*]");
	MAPIFreeBuffer(SRowSet.aRow);

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}

/**
   \details Test the LongTermIdFromId (0x43) and IdFromLongTermId (0x44)
   operations

   This function:
   -# Log into the user private mailbox
   -# Open the Receive Folder
   -# Look up the long term id for the receive folder FID
   -# Look up the short term id for the long term id
   -# Check the id matches the original FID
   -# Create a new replid from long-term
   -# Get the long-term from the given replid
   -# Check both long-term identifiers are equal

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcstor_LongTermId(struct mapitest *mt)
{
	bool			ret = true;
	struct LongTermId	long_term_id, long_term_id_check;
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_id_t		id_inbox;
	mapi_id_t		id_check;
	static struct GUID	fixed_GUID = {
		.time_low = 0xabcdef,
		.time_mid = 0xcafe,
		.time_hi_and_version = 0xbabe,
		.clock_seq = { 0x12, 0x34 },
		.node = { 0xde, 0xad, 0xfa, 0xce, 0xca, 0xfe } };

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 2. Call the GetReceiveFolder operation */
	GetReceiveFolder(&obj_store, &id_inbox, "IPF.Post");
	mapitest_print_retval(mt, "GetReceiveFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3. Call GetLongTermIdFromId on Folder ID */
	GetLongTermIdFromId(&obj_store, id_inbox, &long_term_id);
	mapitest_print_retval(mt, "GetLongTermIdFromId");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 4. Call GetIdFromLongTermId on LongTermId from previous step */
	GetIdFromLongTermId(&obj_store, long_term_id, &id_check);
	mapitest_print_retval(mt, "GetIdFromLongTermId");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 5. Check whether ids are the same */
	ret = (id_check == id_inbox);
	mapitest_print_assert(mt, "Check: IDs match", ret);
	if (ret == false) {
		goto cleanup;
	}

	/* Step 6. Create a new replid from the given long-term id */
	ZERO_STRUCT(long_term_id);
	long_term_id.DatabaseGuid = fixed_GUID;
	long_term_id.GlobalCounter[5] = 0x1;
	retval = GetIdFromLongTermId(&obj_store, long_term_id, &id_check);
	mapitest_print_retval_clean(mt, "GetIdFromLongTermId - Create ReplId", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 7: Get the long term id from the given replid */
	retval = GetLongTermIdFromId(&obj_store, id_check, &long_term_id_check);
	mapitest_print_retval_clean(mt, "GetLongTermIdFromId", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 8: Check both LongTermIds are equal */
	ret = GUID_equal(&(long_term_id.DatabaseGuid), &(long_term_id_check.DatabaseGuid));
	if (ret) {
		ret = (memcmp(long_term_id.GlobalCounter, long_term_id_check.GlobalCounter, 6) == 0);
	}
	mapitest_print_assert(mt, "Check: Long Term IDs match", ret);

 cleanup:
	/* Release */
	mapi_object_release(&obj_store);

	return ret;
}


/**
   \details Test the GetStoreState (0x7b) operation 

   This function:
   -# Logs into the user private mailbox
   -# Retrieve the store state

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcstor_GetStoreState(struct mapitest *mt)
{
	mapi_object_t  	obj_store;
	uint32_t       	StoreState = 0;
	bool	       	ret = true;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Get the store state */
	GetStoreState(&obj_store, &StoreState);
	mapitest_print_retval(mt, "GetStoreState");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
	}

	mapi_object_release(&obj_store);
	return ret;
}

/**
   \details Test the IsMailboxFolder convenience function

   This function:
   -# Logs into the user private mailbox

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcstor_IsMailboxFolder(struct mapitest *mt)
{
	mapi_object_t		obj_store;
	mapi_object_t		obj_pf_store;
	bool			ret = true;
	mapi_object_store_t *	store;
	mapi_object_store_t *	pf_store;
	uint32_t 		olFolderNumber;
	bool			callResult;
	enum MAPISTATUS		retval;

	mapi_object_init(&obj_store);
	mapi_object_init(&obj_pf_store);
	
	/* Step 1. Logon Private Mailbox */
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	store = (mapi_object_store_t *) obj_store.private_data;
	if (! store) {
		mapitest_print(mt, "* FAILED to get store private_data\n" );
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_store, store->fid_top_information_store, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for top_information_store\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderTopInformationStore) {
		mapitest_print(mt, "* FAILED - wrong folder number for top_information_store\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_store, store->fid_deleted_items, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for deleted_items\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderDeletedItems) {
		mapitest_print(mt, "* FAILED - wrong folder number for deleted_items\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_store, store->fid_outbox, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for outbox\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderOutbox) {
		mapitest_print(mt, "* FAILED - wrong folder number for outbox\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_store, store->fid_sent_items, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for sent items\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderSentMail) {
		mapitest_print(mt, "* FAILED - wrong folder number for sent items\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_store, store->fid_inbox, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for inbox\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderInbox) {
		mapitest_print(mt, "* FAILED - wrong folder number for inbox\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_store, store->fid_common_views, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for views\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderCommonView) {
		mapitest_print(mt, "* FAILED - wrong folder number for views\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_store, store->fid_calendar, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for calendar\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderCalendar) {
		mapitest_print(mt, "* FAILED - wrong folder number for calendar\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_store, store->fid_contact, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for contacts\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderContacts) {
		mapitest_print(mt, "* FAILED - wrong folder number for contacts\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_store, store->fid_journal, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for journal\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderJournal) {
		mapitest_print(mt, "* FAILED - wrong folder number for journal\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_store, store->fid_note, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for notes\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderNotes) {
		mapitest_print(mt, "* FAILED - wrong folder number for note\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_store, store->fid_task, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for tasks\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderTasks) {
		mapitest_print(mt, "* FAILED - wrong folder number for tasks\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_store, store->fid_drafts, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for drafts\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderDrafts) {
		mapitest_print(mt, "* FAILED - wrong folder number for drafts\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_store, store->fid_search, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for search\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderFinder) {
		mapitest_print(mt, "* FAILED - wrong folder number for search\n");
		ret = false;
		goto cleanup;
	}

	retval = OpenPublicFolder(mt->session, &obj_pf_store);
	mapitest_print_retval(mt, "OpenPublicFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	pf_store = (mapi_object_store_t *) obj_pf_store.private_data;

	callResult = IsMailboxFolder(&obj_pf_store, pf_store->fid_pf_OfflineAB, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for offline address book\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderPublicOfflineAB) {
		mapitest_print(mt, "* FAILED - wrong folder number for offline address book\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_pf_store, pf_store->fid_pf_FreeBusyRoot, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for free-busy root\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderPublicFreeBusyRoot) {
		mapitest_print(mt, "* FAILED - wrong folder number for free-busy root\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_pf_store, pf_store->fid_pf_EFormsRegistryRoot, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for EForms root\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderPublicEFormsRoot) {
		mapitest_print(mt, "* FAILED - wrong folder number for EForms root\n");
		ret = false;
		goto cleanup;
	}

	/* this one is a bit sensitive. sometimes the EFormsRegistry is null */
	callResult = IsMailboxFolder(&obj_pf_store, pf_store->fid_pf_EFormsRegistry, &olFolderNumber);
	if (pf_store->fid_pf_EFormsRegistry != 0) {
		if (! callResult) {
			mapitest_print(mt, "* FAILED to get folder number for EForms registry\n");
			ret = false;
			goto cleanup;
		}
		if (olFolderNumber != olFolderPublicEFormsRegistry) {
			mapitest_print(mt, "* FAILED - wrong folder number for EForms registry\n");
			ret = false;
			goto cleanup;
		}
	}

	callResult = IsMailboxFolder(&obj_pf_store, pf_store->fid_pf_public_root, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for Public root\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderPublicRoot) {
		mapitest_print(mt, "* FAILED - wrong folder number for Public root\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_pf_store, pf_store->fid_pf_ipm_subtree, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for IPM subtree\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderPublicIPMSubtree) {
		mapitest_print(mt, "* FAILED - wrong folder number for IPM subtree\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_pf_store, pf_store->fid_pf_non_ipm_subtree, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for non-IPM subtree\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderPublicNonIPMSubtree) {
		mapitest_print(mt, "* FAILED - wrong folder number for non-IPM subtree\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_pf_store, pf_store->fid_pf_LocalSiteFreeBusy, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for local free busy folder\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderPublicLocalFreeBusy) {
		mapitest_print(mt, "* FAILED - wrong folder number for local free busy folder\n");
		ret = false;
		goto cleanup;
	}

	callResult = IsMailboxFolder(&obj_pf_store, pf_store->fid_pf_LocalSiteOfflineAB, &olFolderNumber);
	if (! callResult) {
		mapitest_print(mt, "* FAILED to get folder number for local offline address book\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != olFolderPublicLocalOfflineAB) {
		mapitest_print(mt, "* FAILED - wrong folder number for local offline address folder\n");
		ret = false;
		goto cleanup;
	}

	/* this one is a bit sensitive. sometimes the NNTP Articles Folder ID is null */
	callResult = IsMailboxFolder(&obj_pf_store, pf_store->fid_pf_NNTPArticle, &olFolderNumber);
	if (pf_store->fid_pf_NNTPArticle != 0) {
		if (! callResult) {
			mapitest_print(mt, "* FAILED to get folder number for NNTP Articles\n");
			ret = false;
			goto cleanup;
		}
		if (olFolderNumber != olFolderPublicNNTPArticle) {
			mapitest_print(mt, "* FAILED - wrong folder number for NNTP Articles\n");
			ret = false;
			goto cleanup;
		}
	}

	/* this is meant to break */
	callResult = IsMailboxFolder(&obj_store, 0xFFEEDDCC, &olFolderNumber);
	if (callResult) {
		mapitest_print(mt, "* FAILED - expected no folder number\n");
		ret = false;
		goto cleanup;
	}
	if (olFolderNumber != 0xFFFFFFFF) {
		mapitest_print(mt, "* FAILED - wrong folder number for bad folder id\n");
		ret = false;
		goto cleanup;
	}

	mapitest_print(mt, "* All PASSED\n");

cleanup:
	mapi_object_release(&obj_store);
	mapi_object_release(&obj_pf_store);

	return ret;
}
