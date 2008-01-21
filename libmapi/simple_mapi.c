/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007.

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
#include <libmapi/proto_private.h>
#include <gen_ndr/ndr_exchange.h>


/**
   \file simple_mapi.c

   \brief Convenient functions.
*/


/**
   \details Retrieve the folder id for the specified default folder in
   a public folder store

   \param obj_store the store to search
   \param id the type of folder to search for
   \param folder the resulting folder reference

   The following types of folders are supported:
   - olFolderPublicRoot
   - olFolderPublicIPMSubtree

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized.
   - MAPI_E_INVALID_PARAMETER: obj_store is undefined
   - MAPI_E_NOT_FOUND: The specified folder could not be found or is
     not yet supported.

   \sa MAPIInitialize, OpenPublicFolder, GetLastError
 */
_PUBLIC_ enum MAPISTATUS GetDefaultPublicFolder(mapi_object_t *obj_store,
						uint64_t *folder,
						const uint32_t id)
{
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);

	switch (id) {
	case olFolderPublicRoot:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_pf_public_root;
		break;
	case olFolderPublicIPMSubtree:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_pf_ipm_subtree;
		break;
	default:
		return MAPI_E_NOT_FOUND;
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieves the folder id for the specified default folder
   in a mailbox store

   \param obj_store the store to search
   \param id the type of folder to search for
   \param folder the resulting folder reference

   The following types of folders are supported:
   - olFolderTopInformationStore
   - olFolderDeletedItems
   - olFolderOutbox
   - olFolderSentMail
   - olFolderInbox
   - olFolderCalendar
   - olFolderContacts
   - olFolderJournal
   - olFolderNotes
   - olFolderTasks
   - olFolderDrafts

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized.
   - MAPI_E_INVALID_PARAMETER: obj_store is undefined
   - MAPI_E_NOT_FOUND: The specified folder could not be found or is
     not yet supported.

   \sa MAPIInitialize, OpenMsgStore, GetLastError
*/
_PUBLIC_ enum MAPISTATUS GetDefaultFolder(mapi_object_t *obj_store, 
					  uint64_t *folder,
					  const uint32_t id)
{
	enum MAPISTATUS			retval;
	TALLOC_CTX			*mem_ctx;
	mapi_object_t			obj_inbox;
	mapi_id_t			id_inbox;
	struct mapi_SPropValue_array	properties_array;
	const struct SBinary_short		*entryid;
	uint32_t			low;
	uint32_t			high;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_init("GetDefaultFolder");

	mapi_object_init(&obj_inbox);
	retval = GetReceiveFolder(obj_store, &id_inbox);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	if (id > 6) {
		retval = OpenFolder(obj_store, id_inbox, &obj_inbox);
		MAPI_RETVAL_IF(retval, retval, mem_ctx);

		retval = GetPropsAll(&obj_inbox, &properties_array);
		MAPI_RETVAL_IF(retval, retval, mem_ctx);
	} 

	switch (id) {
	case olFolderTopInformationStore:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_top_information_store;
		return MAPI_E_SUCCESS;
	case olFolderDeletedItems:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_deleted_items;
		return MAPI_E_SUCCESS;
	case olFolderOutbox:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_outbox;
		return MAPI_E_SUCCESS;
	case olFolderSentMail:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_sent_items;
		return MAPI_E_SUCCESS;
	case olFolderInbox:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_inbox;
		return MAPI_E_SUCCESS;
	case olFolderCommonView:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_common_views;
		return MAPI_E_SUCCESS;
	case olFolderCalendar:
		entryid = (const struct SBinary_short *)find_mapi_SPropValue_data(&properties_array, PR_IPM_APPOINTMENT_ENTRYID);
		break;
	case olFolderContacts:
		entryid = (const struct SBinary_short *)find_mapi_SPropValue_data(&properties_array, PR_IPM_CONTACT_ENTRYID);
		break;
	case olFolderJournal:
		entryid = (const struct SBinary_short *)find_mapi_SPropValue_data(&properties_array, PR_IPM_JOURNAL_ENTRYID);
		break;
	case olFolderNotes:
		entryid = (const struct SBinary_short *)find_mapi_SPropValue_data(&properties_array, PR_IPM_NOTE_ENTRYID);
		break;		
	case olFolderTasks:
		entryid = (const struct SBinary_short *)find_mapi_SPropValue_data(&properties_array, PR_IPM_TASK_ENTRYID);
		break;
	case olFolderDrafts:
		entryid = (const struct SBinary_short *)find_mapi_SPropValue_data(&properties_array, PR_IPM_DRAFTS_ENTRYID);
		break;		
	case olFolderFinder:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_finder;
		return MAPI_E_SUCCESS;
	default:
		*folder = 0;
		talloc_free(mem_ctx);
		return MAPI_E_NOT_FOUND;
	}

	MAPI_RETVAL_IF(!entryid, MAPI_E_NOT_FOUND, mem_ctx);
	MAPI_RETVAL_IF(entryid->cb < 8, MAPI_E_INVALID_PARAMETER, mem_ctx);

	low = 0;
	low += entryid->lpb[entryid->cb - 1] << 24;
	low += entryid->lpb[entryid->cb - 2] << 16;
	low += entryid->lpb[entryid->cb - 3] << 8;
	low += entryid->lpb[entryid->cb - 4];
	
	high = 0;
	high += entryid->lpb[entryid->cb - 5] << 24;
	high += entryid->lpb[entryid->cb - 6] << 16;
	high += entryid->lpb[entryid->cb - 7] << 8;
	high += entryid->lpb[entryid->cb - 8];
	
	*folder = high;
	*folder = ((uint64_t)low) << 48;
	/* the lowest byte of folder id is set to 1 while entryid one is not */
	*folder += 1;

	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}


/**
   \details Retrieves the total and unread number of items for a
    specified folder.

    \param obj_folder the folder to get item counts for
    \param unread the number of items in the folder (result)
    \param total the number of items in the folder, including unread
    items (result)

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized.
   - MAPI_E_INVALID_PARAMETER: obj_folder is undefined
   - MAPI_E_NOT_FOUND: The specified folder could not be found or is
     not yet supported.

   \sa MAPIInitialize, OpenFolder, GetLastError
*/
_PUBLIC_ enum MAPISTATUS GetFolderItemsCount(mapi_object_t *obj_folder,
					     uint32_t *unread,
					     uint32_t *total)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		count;
	mapi_object_t		obj_table;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_init("GetFolderIteamsCount");

	/* Init mapi objects */
	mapi_object_init(&obj_table);

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2, 
					  PR_CONTENT_UNREAD,
					  PR_CONTENT_COUNT);

	retval = GetProps(obj_folder, SPropTagArray, &lpProps, &count);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	*unread = lpProps[0].value.l;
	*total = lpProps[1].value.l;

	mapi_object_release(&obj_table);

	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}


/**
   \details Adds permissions for a user on a given folder

   \param obj_folder the folder we add permission for
   \param username the Exchange username we add permissions for
   \param role the permission mask value

   The following permissions and rights are supported:
   - RightsNone
   - RightsReadItems
   - RightsCreateItems
   - RightsEditOwn
   - RightsDeleteOwn
   - RightsEditAll
   - RightsDeleteAll
   - RightsCreateSubfolders
   - RightsFolderOwner
   - RightsFolderContact
   - RoleNone
   - RoleReviewer
   - RoleContributor
   - RoleNoneditingAuthor
   - RoleAuthor
   - RoleEditor
   - RolePublishAuthor
   - RolePublishEditor
   - RightsAll
   - RoleOwner

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized.
   - MAPI_E_INVALID_PARAMETER: username is NULL

   \sa ResolveNames, ModifyTable
 */
_PUBLIC_ enum MAPISTATUS AddUserPermission(mapi_object_t *obj_folder, const char *username, enum ACLRIGHTS role)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	struct SPropTagArray	*SPropTagArray;
	const char		*names[1];
	struct SRowSet		*rows = NULL;
	struct FlagList		*flaglist = NULL;
	struct mapi_SRowList	rowList;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_init("AddUserPermission");

	/* query Address book */

	SPropTagArray = set_SPropTagArray(mem_ctx, 2, PR_ENTRYID, PR_DISPLAY_NAME);
	names[0] = username;
	names[1] = NULL;
	retval = ResolveNames((const char **)names, SPropTagArray, &rows, &flaglist, 0);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx);

	/* Check if the username was found */
	MAPI_RETVAL_IF((*flaglist[0].ulFlags != MAPI_RESOLVED), MAPI_E_NOT_FOUND, mem_ctx);

	rowList.cEntries = 1;
	rowList.aEntries = talloc_array(mem_ctx, struct mapi_SRow, 1);
	rowList.aEntries[0].ulRowFlags = ROW_ADD;
	rowList.aEntries[0].lpProps.cValues = 2;
	rowList.aEntries[0].lpProps.lpProps = talloc_array(mem_ctx, struct mapi_SPropValue, 2);
	cast_mapi_SPropValue(&rowList.aEntries[0].lpProps.lpProps[0], &rows->aRow[0].lpProps[0]);
	rowList.aEntries[0].lpProps.lpProps[1].ulPropTag = PR_MEMBER_RIGHTS;
	rowList.aEntries[0].lpProps.lpProps[1].value.l = role;

	retval = ModifyTable(obj_folder, &rowList);
	MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Modify permissions for a user on a given folder
   
   \param obj_folder the folder we add permission for
   \param username the Exchange username we modify permissions for
   \param role the permission mask value

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized.
   - MAPI_E_INVALID_PARAMETER: username is NULL
   - MAPI_E_NOT_FOUND: couldn't find or change permissions for the
     given user

   \sa AddUserPermission, ResolveNames, GetTable, ModifyTable
 */
_PUBLIC_ enum MAPISTATUS ModifyUserPermission(mapi_object_t *obj_folder, const char *username, enum ACLRIGHTS role)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	struct SPropTagArray	*SPropTagArray;
	const char		*names[1];
	const char		*user = NULL;
	struct SRowSet		*rows = NULL;
	struct SRowSet		rowset;
	struct FlagList		*flaglist = NULL;
	struct mapi_SRowList	rowList;
	struct SPropValue	*lpProp;
	mapi_object_t		obj_table;
	uint32_t		row_count;
	bool			found = false;
	uint32_t		i = 0;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_init("ModifyUserPermission");

	SPropTagArray = set_SPropTagArray(mem_ctx, 2, PR_ENTRYID, PR_DISPLAY_NAME);
	names[0] = username;
	names[1] = NULL;
	retval = ResolveNames((const char **)names, SPropTagArray, &rows, &flaglist, 0);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	if (*flaglist[0].ulFlags == MAPI_RESOLVED) {
		user = find_SPropValue_data(&(rows->aRow[0]), PR_DISPLAY_NAME);
	} else {
		/* Special case: Not a AD user account but Default or
		 * Anonymous. Since names are language specific, we
		 * can't use strcmp 
		 */
		user = username;
	}

	mapi_object_init(&obj_table);
	retval = GetTable(obj_folder, &obj_table);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	SPropTagArray = set_SPropTagArray(mem_ctx, 4,
					  PR_ENTRYID,
					  PR_MEMBER_RIGHTS,
					  PR_MEMBER_ID,
					  PR_MEMBER_NAME);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	retval = GetRowCount(&obj_table, &row_count);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	retval = QueryRows(&obj_table, row_count, TBL_ADVANCE, &rowset);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	for (i = 0; i < rowset.cRows; i++) {
		lpProp = get_SPropValue_SRow(&rowset.aRow[i], PR_MEMBER_NAME);
		if (lpProp && lpProp->value.lpszA) {
			if (!strcmp(lpProp->value.lpszA, user)) {
				rowList.cEntries = 1;
				rowList.aEntries = talloc_array(mem_ctx, struct mapi_SRow, 1);
				rowList.aEntries[0].ulRowFlags = ROW_MODIFY;
				rowList.aEntries[0].lpProps.cValues = 2;
				rowList.aEntries[0].lpProps.lpProps = talloc_array(mem_ctx, struct mapi_SPropValue, 2);
				lpProp = get_SPropValue_SRow(&(rowset.aRow[i]), PR_MEMBER_ID);
				rowList.aEntries[0].lpProps.lpProps[0].ulPropTag = PR_MEMBER_ID;
				rowList.aEntries[0].lpProps.lpProps[0].value.d = lpProp->value.d;
				rowList.aEntries[0].lpProps.lpProps[1].ulPropTag = PR_MEMBER_RIGHTS;
				rowList.aEntries[0].lpProps.lpProps[1].value.l = role;
				
				retval = ModifyTable(obj_folder, &rowList);
				MAPI_RETVAL_IF(retval, retval, mem_ctx);
				found = true;
				break;
			}
		}
	}

	mapi_object_release(&obj_table);
	talloc_free(mem_ctx);
	
	errno = (found == true) ? MAPI_E_SUCCESS : MAPI_E_NOT_FOUND;
	return ((found == true) ? MAPI_E_SUCCESS : MAPI_E_NOT_FOUND);	
}


/**
   \details Remove permissions for a user on a given folder

   \param obj_folder the folder we add permission for
   \param username the Exchange username we remove permissions for

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized.
   - MAPI_E_INVALID_PARAMETER: username is NULL
   - MAPI_E_NOT_FOUND: couldn't find or remove permissions for the
     given user

   \sa ResolveNames, GetTable, ModifyTable
 */
_PUBLIC_ enum MAPISTATUS RemoveUserPermission(mapi_object_t *obj_folder, const char *username)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	struct SPropTagArray	*SPropTagArray;
	const char		*names[1];
	const char		*user = NULL;
	struct SRowSet		*rows = NULL;
	struct SRowSet		rowset;
	struct FlagList		*flaglist = NULL;
	struct mapi_SRowList	rowList;
	struct SPropValue	*lpProp;
	mapi_object_t		obj_table;
	uint32_t		row_count;
	bool			found = false;
	uint32_t		i = 0;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_init("RemoveUserPermission");

	SPropTagArray = set_SPropTagArray(mem_ctx, 2, PR_ENTRYID, PR_DISPLAY_NAME);
	names[0] = username;
	names[1] = NULL;
	retval = ResolveNames((const char **)names, SPropTagArray, &rows, &flaglist, 0);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* Check if the username was found */
	MAPI_RETVAL_IF((*flaglist[0].ulFlags != MAPI_RESOLVED), MAPI_E_NOT_FOUND, mem_ctx);

	user = find_SPropValue_data(&(rows->aRow[0]), PR_DISPLAY_NAME);

	mapi_object_init(&obj_table);
	retval = GetTable(obj_folder, &obj_table);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	SPropTagArray = set_SPropTagArray(mem_ctx, 4,
					  PR_ENTRYID,
					  PR_MEMBER_RIGHTS,
					  PR_MEMBER_ID,
					  PR_MEMBER_NAME);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	retval = GetRowCount(&obj_table, &row_count);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	retval = QueryRows(&obj_table, row_count, TBL_ADVANCE, &rowset);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	for (i = 0; i < rowset.cRows; i++) {
		lpProp = get_SPropValue_SRow(&rowset.aRow[i], PR_MEMBER_NAME);
		if (lpProp && lpProp->value.lpszA) {
			if (!strcmp(lpProp->value.lpszA, user)) {
				rowList.cEntries = 1;
				rowList.aEntries = talloc_array(mem_ctx, struct mapi_SRow, 1);
				rowList.aEntries[0].ulRowFlags = ROW_REMOVE;
				rowList.aEntries[0].lpProps.cValues = 1;
				rowList.aEntries[0].lpProps.lpProps = talloc_array(mem_ctx, struct mapi_SPropValue, 1);
				lpProp = get_SPropValue_SRow(&(rowset.aRow[i]), PR_MEMBER_ID);
				rowList.aEntries[0].lpProps.lpProps[0].ulPropTag = PR_MEMBER_ID;
				rowList.aEntries[0].lpProps.lpProps[0].value.d = lpProp->value.d;
				
				retval = ModifyTable(obj_folder, &rowList);
				MAPI_RETVAL_IF(retval, retval, mem_ctx);
				found = true;
				break;
			}
		}
	}

	mapi_object_release(&obj_table);
	talloc_free(mem_ctx);

	return ((found == true) ? MAPI_E_SUCCESS : MAPI_E_NOT_FOUND);
}
