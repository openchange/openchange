/*
   Stand-alone MAPI testsuite

   OpenChange Project - Permissions operations

   Copyright (C) Brad Hards <bradh@openchange.org> 2010

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

/**
   \file module_oxcperm.c

   \brief Permissions Protocol test suite
*/

/*
 Utility function to dump out the permissions
 
 This depends on PR_ENTRYID being the first property returned
 */
static void mapitest_dump_permissions_SRowSet(struct mapitest *mt, struct SRowSet *SRowSet, const char *sep)
{
	uint32_t		i, j;

	/* Sanity checks */
	if (!SRowSet) return;
	if (!SRowSet->cRows) return;

	for (i = 0; i < SRowSet->cRows; i++) {
		struct SRow *thisRow = &(SRowSet->aRow[i]);
		if ((thisRow->lpProps[0].value.d) == 0x0) {
			/* this is the "default" user id - ignore this */
			continue;
		}
		for (j = 0; j < thisRow->cValues; j++) {
			mapitest_print_SPropValue(mt, thisRow->lpProps[j], sep);
		}
	}
}

/*
  \details Utility function to get a user different from ourselves.

  \return a username on success, NULL otherwise
*/
static const char * mapitest_get_other_user(struct mapitest *mt)
{
	bool			found = false;
	const char		*username = NULL;
	enum MAPISTATUS		retval;
	struct PropertyRowSet_r *rowset;
	struct SPropTagArray	*s_prop_tag_array;
	uint32_t		i;

	s_prop_tag_array = set_SPropTagArray(mt->mem_ctx, 0x2,
					     PR_INSTANCE_KEY,
					     PR_ACCOUNT);

	retval = GetGALTable(mt->session, s_prop_tag_array, &rowset, 2, TABLE_START);
	if (retval != MAPI_E_SUCCESS || !rowset || !(rowset->aRow)) {
		return NULL;
	}

	for (i = 0; i < rowset->cRows && !found; i++) {
		username = (const char *) find_PropertyValue_data(&rowset->aRow[i], PR_ACCOUNT);
		if (username) {
			found = (strncmp(mt->session->profile->username, username, strlen(username)) != 0);
			if (found) {
				talloc_steal(mt->mem_ctx, username);
			}
		}
	}

	if (!found) {
		username = NULL;
	}

	MAPIFreeBuffer(rowset);

	return username;
}


/**
   \details Test the GetPermissionsTable (0x3e) operation

   This function:
   -# Log on private message store
   -# Open the top store folder
   -# Gets the permissions table handle
   -# Fetches properties from the permissions table
 */
_PUBLIC_ bool mapitest_oxcperm_GetPermissionsTable(struct mapitest *mt)
{
	enum MAPISTATUS			retval;
	mapi_object_t			obj_store;
	mapi_object_t			obj_permtable;
	mapi_object_t			obj_folder;
	mapi_id_t			id_folder;
	struct SPropTagArray		*SPropTagArray;
	struct SRowSet			SRowSet;
	bool				ret = true;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_folder);
	mapi_object_init(&obj_permtable);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 2. Open Top Information Store folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
	mapitest_print_retval(mt, "GetDefaultFolder");
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3. Get the permissions table handle */
        retval = GetPermissionsTable(&obj_folder, 0x00, &obj_permtable);
        mapitest_print_retval(mt, "GetPermissionsTable");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	
	/* Step 4. Fetch some properties from the permissions and dump them out */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_MEMBER_ID,
					  PR_MEMBER_NAME_UNICODE,
					  PR_MEMBER_RIGHTS);
	retval = SetColumns(&obj_permtable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "SetColumns");
		ret = false;
		goto cleanup;
	}

	retval = QueryRows(&obj_permtable, 0x20, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "QueryRows");
		ret = false;
		goto cleanup;
	}
	mapitest_dump_permissions_SRowSet(mt, &SRowSet, "\t");
	
cleanup:
	mapi_object_release(&obj_permtable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);
	return ret;
}

/**
   \details Test the ModifyPermissions (0x40) operation

   This function:
   -# Log on private message store
   -# Open the top store folder
   -# Get another user to use for permissions
   -# Creates a temporary folder
   -# Adds permissions for other user, and checks them
   -# Modifies permissions for other user, and checks them
   -# Removes permissions for other user, and checks them
   -# Deletes the folder
 */
_PUBLIC_ bool mapitest_oxcperm_ModifyPermissions(struct mapitest *mt)
{
	const char			*other_username;
	enum MAPISTATUS			retval;
	mapi_object_t			obj_store;
	mapi_object_t			obj_permtable;
	mapi_object_t			obj_top_folder;
	mapi_object_t			obj_temp_folder;
	mapi_id_t			id_top_folder;
	struct SPropTagArray		*SPropTagArray;
	struct SRowSet			SRowSet;
	bool				ret = true;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_top_folder);
	mapi_object_init(&obj_temp_folder);
	mapi_object_init(&obj_permtable);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 2. Open Top Information Store folder */
	retval = GetDefaultFolder(&obj_store, &id_top_folder, olFolderTopInformationStore);
	mapitest_print_retval(mt, "GetDefaultFolder");
	retval = OpenFolder(&obj_store, id_top_folder, &obj_top_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	
	/* Step 3. Create a temporary folder */
	retval = CreateFolder(&obj_top_folder, FOLDER_GENERIC, "MAPITEST_TEST_FOLDER", NULL,
			      OPEN_IF_EXISTS, &obj_temp_folder);
	mapitest_print_retval(mt, "CreateFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 4. Get other user */
	other_username = mapitest_get_other_user(mt);
	if (!other_username) {
		ret = false;
		goto cleanup;
	}

	/* Step 4. Add user permissions on the folder, and check it */
	retval = AddUserPermission(&obj_temp_folder, other_username, RightsReadItems);
	mapitest_print_retval(mt, "AddUserPermission");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = GetPermissionsTable(&obj_temp_folder, 0x00, &obj_permtable);
	mapitest_print_retval(mt, "GetPermissionsTable");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_MEMBER_ID,
					  PR_MEMBER_NAME_UNICODE,
					  PR_MEMBER_RIGHTS);
	retval = SetColumns(&obj_permtable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "SetColumns");
		ret = false;
		goto cleanup;
	}

	retval = QueryRows(&obj_permtable, 0x20, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "QueryRows");
		ret = false;
		goto cleanup;
	}
	mapitest_dump_permissions_SRowSet(mt, &SRowSet, "\t");

	/* Step 5. Modify user permissions on the folder, and check it */
	retval = ModifyUserPermission(&obj_temp_folder, other_username, RightsAll);
	mapitest_print_retval(mt, "ModifyUserPermission");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = GetPermissionsTable(&obj_temp_folder, 0x00, &obj_permtable);
	mapitest_print_retval(mt, "GetPermissionsTable");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_MEMBER_ID,
					  PR_MEMBER_NAME_UNICODE,
					  PR_MEMBER_RIGHTS);
	retval = SetColumns(&obj_permtable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "SetColumns");
		ret = false;
		goto cleanup;
	}

	retval = QueryRows(&obj_permtable, 0x20, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "QueryRows");
		ret = false;
		goto cleanup;
	}
	mapitest_dump_permissions_SRowSet(mt, &SRowSet, "\t");

	/* Step 6. Remove user permissions on the folder, and check it */
	retval = RemoveUserPermission(&obj_temp_folder, other_username);
	mapitest_print_retval(mt, "RemoveUserPermission");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = GetPermissionsTable(&obj_temp_folder, 0x00, &obj_permtable);
	mapitest_print_retval(mt, "GetPermissionsTable");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_MEMBER_ID,
					  PR_MEMBER_NAME_UNICODE,
					  PR_MEMBER_RIGHTS);
	retval = SetColumns(&obj_permtable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "SetColumns");
		ret = false;
		goto cleanup;
	}

	retval = QueryRows(&obj_permtable, 0x20, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "QueryRows");
		ret = false;
		goto cleanup;
	}
	mapitest_dump_permissions_SRowSet(mt, &SRowSet, "\t");

	/* Step 7. Delete the folder */
	retval = EmptyFolder(&obj_temp_folder);
	mapitest_print_retval(mt, "EmptyFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	
	retval = DeleteFolder(&obj_top_folder, mapi_object_get_id(&obj_temp_folder),
			      DEL_MESSAGES|DEL_FOLDERS|DELETE_HARD_DELETE, NULL);
	mapitest_print_retval(mt, "DeleteFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

cleanup:
	mapi_object_release(&obj_permtable);
	mapi_object_release(&obj_temp_folder);
	mapi_object_release(&obj_top_folder);
	mapi_object_release(&obj_store);
	return ret;
}
