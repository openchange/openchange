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

#include <libmapi/libmapi.h>
#include "utils/mapitest/mapitest.h"
#include "utils/mapitest/proto.h"

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
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	mapi_object_release(&obj_store);

	/* Step 2. Logon Public Folder store */
	mapi_object_init(&obj_store);
	retval = OpenPublicFolder(mt->session, &obj_store);
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
   
   \param mt the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcstor_GetReceiveFolder(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_id_t		id_inbox;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	
	/* Step 2. Call the GetReceiveFolder operation */
	retval = GetReceiveFolder(&obj_store, &id_inbox, "IPF.Post");
	mapitest_print_retval(mt, "GetReceiveFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the SetReceiveFolder (0x26) operation

   This function:
   -# Log on the user private mailbox
   -# Call the SetReceiveFolder operation

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

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval_step(mt, "1.", "Logon");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Get the original ReceiveFolder */
	retval = GetReceiveFolder(&obj_store, &id_inbox, NULL);
	mapitest_print_retval_step(mt, "2.", "GetReceiveFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Open the ReceiveFolder */
	mapi_object_init(&obj_inbox);
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapitest_print_retval_step(mt, "3.", "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Open the Top Information Store folder */
	retval = GetDefaultFolder(&obj_store, &id_tis, olFolderTopInformationStore);
	mapitest_print_retval_step(mt, "4.", "GetDefaultFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&obj_tis);
	retval = OpenFolder(&obj_store, id_tis, &obj_tis);
	mapitest_print_retval_step(mt, "4.1.", "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Create the New Inbox folder under Top Information Store */
	mapi_object_init(&obj_folder);
	retval = CreateFolder(&obj_tis, FOLDER_GENERIC, "New Inbox", NULL,
			      OPEN_IF_EXISTS, &obj_folder);
	mapitest_print_retval_step(mt, "5.", "CreateFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Set IPM.Note receive folder to New Inbox */
	retval = SetReceiveFolder(&obj_store, &obj_folder, "IPM.Note");
	mapitest_print_retval_step_fmt(mt, "6.", "SetReceiveFolder", "%s", "(New Inbox)");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Reset receive folder to Inbox */
	retval = SetReceiveFolder(&obj_store, &obj_inbox, "IPM.Note");
	mapitest_print_retval_step(mt, "6.1.", "SetReceiveFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	/* Delete New Inbox folder */
	retval = EmptyFolder(&obj_folder);
	mapitest_print_retval_step(mt, "7.", "EmptyFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	retval = DeleteFolder(&obj_tis, mapi_object_get_id(&obj_folder),
			      DEL_FOLDERS | DEL_MESSAGES | DELETE_HARD_DELETE, NULL);
	mapitest_print_retval_step(mt, "8.", "DeleteFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_tis);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);

	return true;
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
	enum MAPISTATUS		retval;
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	uint64_t		folderId;
	uint16_t		OwningServersCount;
	uint16_t		CheapServersCount;
	char			*OwningServers;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenPublicFolder(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenPublicFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	
	/* Step 2. Open IPM Subtree folder */
	retval = GetDefaultPublicFolder(&obj_store, &folderId, olFolderPublicIPMSubtree);
	mapitest_print_retval(mt, "GetDefaultPublicFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, folderId, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3. Call GetOwningServers */
	retval = GetOwningServers(&obj_store, &obj_folder, &OwningServersCount, &CheapServersCount, &OwningServers);
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
	enum MAPISTATUS		retval;
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	uint64_t		folderId;
	bool			IsGhosted;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenPublicFolder(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenPublicFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	
	/* Step 2. Open IPM Subtree folder */
	retval = GetDefaultPublicFolder(&obj_store, &folderId, olFolderPublicIPMSubtree);
	mapitest_print_retval(mt, "GetDefaultPublicFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, folderId, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3. Call PublicFolderIsGhosted */
	retval = PublicFolderIsGhosted(&obj_store, &obj_folder, &IsGhosted);
	mapitest_print_retval(mt, "PublicFolderIsGhosted");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
	}
	mapitest_print(mt, "* %-35s: IsGhosted is set to %s\n", "PublicFolderIsGhosted", ((IsGhosted) ? "true" : "false"));

	/* cleanup objects */
	mapi_object_release(&obj_folder);

cleanup:
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
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	struct SRowSet		SRowSet;
	
 	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Call the GetReceiveFolderTable operation */
	retval = GetReceiveFolderTable(&obj_store, &SRowSet);
	mapitest_print_retval(mt, "GetReceiveFolderTable");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapidump_SRowSet(&SRowSet, "\t\t[*]");
	MAPIFreeBuffer(SRowSet.aRow);

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}

/**
   \details Test the LongTermIdFromId (0x43) and IdFromLongTermId (0x44)
   operations

   This function:
   -# Logs into the user private mailbox
   -# Open the Receive Folder
   -# Looks up the long term id for the receive folder FID
   -# Looks up the short term id for the long term id
   -# Checks the id matches the original FID

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcstor_LongTermId(struct mapitest *mt)
{
	mapi_object_t		obj_store;
	mapi_id_t		id_inbox;
	struct LongTermId	long_term_id;
	mapi_id_t		id_check;
	bool			ret = true;

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

	/* Step 4. Call GetIdFromLongTermId on LongTermId from previous step*/
	GetIdFromLongTermId(&obj_store, long_term_id, &id_check);
	mapitest_print_retval(mt, "GetIdFromLongTermId");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 5. Check whether ids are the same */
	if ( id_check == id_inbox ) {
		mapitest_print(mt, "* Check: IDs match - [SUCCESS]\n" );
	} else {
		mapitest_print(mt, "* Check: IDs do not match - [SUCCESS] (0x%x, expected 0x%x)\n",
			       id_check, id_inbox);
		ret=false;
		goto cleanup;
	}

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
