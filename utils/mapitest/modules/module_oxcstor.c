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
	retval = OpenMsgStore(&obj_store);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "OpenMsgStore", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	mapi_object_release(&obj_store);

	/* Step 2. Logon Public Folder store */
	mapi_object_init(&obj_store);
	retval = OpenPublicFolder(&obj_store);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "OpenPublicFolder", GetLastError());
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
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	
	/* Step 2. Call the GetReceiveFolder operation */
	retval = GetReceiveFolder(&obj_store, &id_inbox, "IPF.Post");
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetReceiveFolder", GetLastError());
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
	retval = OpenMsgStore(&obj_store);
	mapitest_print(mt, "* Step 1. %-35s: 0x%.8x\n", "Logon", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Get the original ReceiveFolder */
	retval = GetReceiveFolder(&obj_store, &id_inbox, NULL);
	mapitest_print(mt, "* Step 2. %-35s: 0x%.8x\n", "GetReceiveFolder", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Open the ReceiveFolder */
	mapi_object_init(&obj_inbox);
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapitest_print(mt, "* Step 3. %-35s: 0x%.8x\n", "OpenFolder", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Open the Top Information Store folder */
	retval = GetDefaultFolder(&obj_store, &id_tis, olFolderTopInformationStore);
	mapitest_print(mt, "* Step 4. %-35s: 0x%.8x\n", "GetDefaultFolder", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&obj_tis);
	retval = OpenFolder(&obj_store, id_tis, &obj_tis);
	mapitest_print(mt, "* Step 4.1. %-33s: 0x%.8x\n", "OpenFolder", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Create the New Inbox folder under Top Information Store */
	mapi_object_init(&obj_folder);
	retval = CreateFolder(&obj_tis, FOLDER_GENERIC, "New Inbox", NULL,
			      OPEN_IF_EXISTS, &obj_folder);
	mapitest_print(mt, "* Step 5. %-35s: 0x%.8x\n", "CreateFolder", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Set IPM.Note receive folder to New Inbox */
	retval = SetReceiveFolder(&obj_store, &obj_folder, "IPM.Note");
	mapitest_print(mt, "* Step 6. %-35s: 0x%.8x (New Inbox)\n", "SetReceiveFolder", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Reset receive folder to Inbox */
	retval = SetReceiveFolder(&obj_store, &obj_inbox, "IPM.Note");
	mapitest_print(mt, "* Step 6.1 %-34s: 0x%.8x\n", "SetReceiveFolder", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	/* Delete New Inbox folder */
	retval = EmptyFolder(&obj_folder);
	mapitest_print(mt, "* Step 7. %-35s: 0x%.8x\n", "EmptyFolder", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	retval = DeleteFolder(&obj_tis, mapi_object_get_id(&obj_folder));
	mapitest_print(mt, "* Step 8. %-35s: 0x%.8x\n", "DeleteFolder", GetLastError());
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
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Call the GetReceiveFolderTable operation */
	retval = GetReceiveFolderTable(&obj_store, &SRowSet);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetReceiveFolderTable", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapidump_SRowSet(&SRowSet, "\t\t[*]");
	MAPIFreeBuffer(SRowSet.aRow);

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}
