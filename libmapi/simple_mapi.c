/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2008.

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


/**
   \file simple_mapi.c

   \brief Convenience functions.
*/


/**
   \details Retrieve the folder id for the specified default folder in
   a public folder store

   \param obj_store the store to search
   \param id the type of folder to search for
   \param folder the resulting folder reference

   The following types of folders are supported:
   - olFolderPublicRoot - the parent (directly or indirectly) for the folders below
   - olFolderPublicIPMSubtree - Interpersonal Messages (IPM) folders
   - olFolderPublicNonIPMSubtree - Non-interpersonal message folders
   - olFolderPublicEFormsRoot - EForms Registry Root Folder
   - olFolderPublicFreeBusyRoot - Free/busy root folder
   - olFolderPublicOfflineAB - Offline address book root folder
   - olFolderPublicEFormsRegistry - EForms Registry for the users locale
   - olFolderPublicLocalFreeBusy - Site local free/busy folders
   - olFolderPublicLocalOfflineAB - Site local Offline address book
   - olFolderPublicNNTPArticle - NNTP article index folder

   \return MAPI_E_SUCCESS on success, otherwise a failure code (MAPISTATUS)
   indicating the error.

   \note Developers may also call GetLastError() to retrieve the last
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
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);

	switch (id) {
	case olFolderPublicRoot:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_pf_public_root;
		break;
	case olFolderPublicIPMSubtree:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_pf_ipm_subtree;
		break;
	case olFolderPublicNonIPMSubtree:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_pf_non_ipm_subtree;
		break;
	case olFolderPublicEFormsRoot:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_pf_EFormsRegistryRoot;
		break;
	case olFolderPublicFreeBusyRoot:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_pf_FreeBusyRoot;
		break;
	case olFolderPublicOfflineAB:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_pf_OfflineAB;
		break;
	case olFolderPublicEFormsRegistry:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_pf_EFormsRegistry;
		break;
	case olFolderPublicLocalFreeBusy:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_pf_LocalSiteFreeBusy;
		break;
	case olFolderPublicLocalOfflineAB:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_pf_LocalSiteOfflineAB;
		break;
	case olFolderPublicNNTPArticle:
		*folder = ((mapi_object_store_t *)obj_store->private_data)->fid_pf_NNTPArticle;
		break;
	default:
		OPENCHANGE_RETVAL_ERR(MAPI_E_NOT_FOUND, NULL);
	}

	return MAPI_E_SUCCESS;
}


static enum MAPISTATUS CacheDefaultFolders(mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	mapi_object_store_t	*store;
	mapi_object_t		obj_inbox;
	mapi_id_t		id_inbox;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct SRow		aRow;
	struct SPropValue	*lpProps;
	uint32_t		count;
	const struct Binary_r	*entryid;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);

	store = (mapi_object_store_t *)obj_store->private_data;
	OPENCHANGE_RETVAL_IF(!store, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = talloc_named(NULL, 0, "GetDefaultFolder");

	mapi_object_init(&obj_inbox);
	retval = GetReceiveFolder(obj_store, &id_inbox, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
	
	retval = OpenFolder(obj_store, id_inbox, &obj_inbox);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
	
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x6,
					  PR_IPM_APPOINTMENT_ENTRYID,
					  PR_IPM_CONTACT_ENTRYID,
					  PR_IPM_JOURNAL_ENTRYID,
					  PR_IPM_NOTE_ENTRYID,
					  PR_IPM_TASK_ENTRYID,
					  PR_IPM_DRAFTS_ENTRYID);
	
	retval = GetProps(&obj_inbox, SPropTagArray, &lpProps, &count);
	MAPIFreeBuffer(SPropTagArray);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
	
	aRow.cValues = count;
	aRow.lpProps = lpProps;
	
	/* set cached calendar FID */
	entryid = (const struct Binary_r *)find_SPropValue_data(&aRow, PR_IPM_APPOINTMENT_ENTRYID);
	OPENCHANGE_RETVAL_IF(!entryid, MAPI_E_NOT_FOUND, mem_ctx);
	retval = GetFIDFromEntryID(entryid->cb, entryid->lpb, id_inbox, &store->fid_calendar);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
	
	/* set cached contact FID */
	entryid = (const struct Binary_r *)find_SPropValue_data(&aRow, PR_IPM_CONTACT_ENTRYID);
	OPENCHANGE_RETVAL_IF(!entryid, MAPI_E_NOT_FOUND, mem_ctx);
	retval = GetFIDFromEntryID(entryid->cb, entryid->lpb, id_inbox, &store->fid_contact);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
	
	/* set cached journal FID */
	entryid = (const struct Binary_r *)find_SPropValue_data(&aRow, PR_IPM_JOURNAL_ENTRYID);
	OPENCHANGE_RETVAL_IF(!entryid, MAPI_E_NOT_FOUND, mem_ctx);
	retval = GetFIDFromEntryID(entryid->cb, entryid->lpb, id_inbox, &store->fid_journal);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
	
	/* set cached note FID */
	entryid = (const struct Binary_r *)find_SPropValue_data(&aRow, PR_IPM_NOTE_ENTRYID);
	OPENCHANGE_RETVAL_IF(!entryid, MAPI_E_NOT_FOUND, mem_ctx);
	retval = GetFIDFromEntryID(entryid->cb, entryid->lpb, id_inbox, &store->fid_note);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
	
	/* set cached task FID */
	entryid = (const struct Binary_r *)find_SPropValue_data(&aRow, PR_IPM_TASK_ENTRYID);
	OPENCHANGE_RETVAL_IF(!entryid, MAPI_E_NOT_FOUND, mem_ctx);
	retval = GetFIDFromEntryID(entryid->cb, entryid->lpb, id_inbox, &store->fid_task);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
	
	/* set cached drafts FID */
	entryid = (const struct Binary_r *)find_SPropValue_data(&aRow, PR_IPM_DRAFTS_ENTRYID);
	OPENCHANGE_RETVAL_IF(!entryid, MAPI_E_NOT_FOUND, mem_ctx);
	retval = GetFIDFromEntryID(entryid->cb, entryid->lpb, id_inbox, &store->fid_drafts);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
	
	store->cached_mailbox_fid = true;
	
	mapi_object_release(&obj_inbox);
	talloc_free(mem_ctx);

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
   - olFolderCommonView
   - olFolderCalendar
   - olFolderContacts
   - olFolderJournal
   - olFolderNotes
   - olFolderTasks
   - olFolderDrafts
   - olFolderReminders
   - olFolderFinder

   Note that this function will cache FID values for common accessed
   folders such as calendar, contact, journal, note, task and drafts
   until the store object got released.

   \return MAPI_E_SUCCESS on success, otherwise a failure code (MAPISTATUS)
   indicating the error.

   \note Developers may also call GetLastError() to retrieve the last
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
	mapi_object_store_t		*store;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!folder, MAPI_E_INVALID_PARAMETER, NULL);

	store = (mapi_object_store_t *)obj_store->private_data;
	OPENCHANGE_RETVAL_IF(!store, MAPI_E_NOT_INITIALIZED, NULL);

	if ((id > 6) && (store->cached_mailbox_fid == false)) {
		retval = CacheDefaultFolders(obj_store);
		OPENCHANGE_RETVAL_IF(retval, retval, NULL);
	} 

	switch (id) {
	case olFolderTopInformationStore:
		*folder = store->fid_top_information_store;
		return MAPI_E_SUCCESS;
	case olFolderDeletedItems:
		*folder = store->fid_deleted_items;
		return MAPI_E_SUCCESS;
	case olFolderOutbox:
		*folder = store->fid_outbox;
		return MAPI_E_SUCCESS;
	case olFolderSentMail:
		*folder = store->fid_sent_items;
		return MAPI_E_SUCCESS;
	case olFolderInbox:
		*folder = store->fid_inbox;
		return MAPI_E_SUCCESS;
	case olFolderCommonView:
		*folder = store->fid_common_views;
		return MAPI_E_SUCCESS;
	case olFolderCalendar:
		*folder = store->fid_calendar;
		return MAPI_E_SUCCESS;
	case olFolderContacts:
		*folder = store->fid_contact;
		return MAPI_E_SUCCESS;
	case olFolderJournal:
		*folder = store->fid_journal;
		return MAPI_E_SUCCESS;
	case olFolderNotes:
		*folder = store->fid_note;
		return MAPI_E_SUCCESS;
	case olFolderTasks:
		*folder = store->fid_task;
		return MAPI_E_SUCCESS;
	case olFolderDrafts:
		*folder = store->fid_drafts;
		return MAPI_E_SUCCESS;
	case olFolderFinder:
		*folder = store->fid_search;
		return MAPI_E_SUCCESS;
	default:
		*folder = 0;
		OPENCHANGE_RETVAL_ERR(MAPI_E_NOT_FOUND, 0);
	}
}


/**
   \details Check if a given folder identifier matches with a
   system/default one and optionally returns the olFolder type

   \param obj_store pointer to the store object
   \param fid reference to the folder identifier to check
   \param olFolder pointer to the returned olFolder

   \return true on success, otherwise false
 */
_PUBLIC_ bool IsMailboxFolder(mapi_object_t *obj_store, 
			      uint64_t fid, 
			      uint32_t *olFolder)
{
	enum MAPISTATUS		retval;
	mapi_object_store_t	*store;
	uint32_t		olFolderNum;
	bool			ret = true;

	if (!obj_store) return false;
	store = (mapi_object_store_t *) obj_store->private_data;
	if (!store) return false;

	if (store->cached_mailbox_fid == false) {
		retval = CacheDefaultFolders(obj_store);
		if (retval) return false;
	}

	if(fid == store->fid_top_information_store) {
		olFolderNum = olFolderTopInformationStore;
	} else if (fid == store->fid_deleted_items) {
		olFolderNum = olFolderDeletedItems;
	} else if (fid == store->fid_outbox) {
		olFolderNum = olFolderOutbox;
	} else if (fid == store->fid_sent_items) {
		olFolderNum = olFolderSentMail;
	} else if (fid == store->fid_inbox) {
		olFolderNum = olFolderInbox;
	} else if (fid == store->fid_common_views) {
		olFolderNum = olFolderCommonView;
	} else if (fid == store->fid_calendar) {
		olFolderNum = olFolderCalendar;
	} else if (fid == store->fid_contact) {
		olFolderNum = olFolderContacts;
	} else if (fid == store->fid_journal) {
		olFolderNum = olFolderJournal;
	} else if (fid == store->fid_note) {
		olFolderNum = olFolderNotes;
	} else if (fid == store->fid_task) {
		olFolderNum = olFolderTasks;
	} else if (fid == store->fid_drafts) {
		olFolderNum = olFolderDrafts;
	} else if (fid == store->fid_search) {
		olFolderNum = olFolderFinder;
	} else {
		olFolderNum = -1;
		ret = false;
	}

	if (olFolder) *olFolder = olFolderNum;
	return ret;
}

/**
   \details Retrieves the total and unread number of items for a
    specified folder.

    \param obj_folder the folder to get item counts for
    \param unread the number of items in the folder (result)
    \param total the number of items in the folder, including unread
    items (result)

   \return MAPI_E_SUCCESS on success, otherwise a failure code (MAPISTATUS)
   indicating the error.

   \note Developers may also call GetLastError() to retrieve the last
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

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!unread, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!total, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "GetFolderItemsCount");

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2, 
					  PR_CONTENT_UNREAD,
					  PR_CONTENT_COUNT);

	retval = GetProps(obj_folder, SPropTagArray, &lpProps, &count);
	MAPIFreeBuffer(SPropTagArray);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	*unread = lpProps[0].value.l;
	*total = lpProps[1].value.l;

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

   \return MAPI_E_SUCCESS on success, otherwise a failure code (MAPISTATUS)
   indicating the error.

   \note Developers may also call GetLastError() to retrieve the last
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
	const char		*names[2];
	struct SRowSet		*rows = NULL;
	struct SPropTagArray   	*flaglist = NULL;
	struct mapi_SRowList	rowList;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);

	rowList.padding = 0;

	mem_ctx = talloc_named(NULL, 0, "AddUserPermission");

	/* query Address book */

	SPropTagArray = set_SPropTagArray(mem_ctx, 2, PR_ENTRYID, PR_DISPLAY_NAME);
	names[0] = username;
	names[1] = NULL;
	retval = ResolveNames(mapi_object_get_session(obj_folder), (const char **)names, 
			      SPropTagArray, &rows, &flaglist, 0);
	MAPIFreeBuffer(SPropTagArray);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Check if the username was found */
	OPENCHANGE_RETVAL_IF((flaglist->aulPropTag[0] != MAPI_RESOLVED), MAPI_E_NOT_FOUND, mem_ctx);

	rowList.cEntries = 1;
	rowList.aEntries = talloc_array(mem_ctx, struct mapi_SRow, 1);
	rowList.aEntries[0].ulRowFlags = ROW_ADD;
	rowList.aEntries[0].lpProps.cValues = 2;
	rowList.aEntries[0].lpProps.lpProps = talloc_array(mem_ctx, struct mapi_SPropValue, 2);
	cast_mapi_SPropValue(&rowList.aEntries[0].lpProps.lpProps[0], &rows->aRow[0].lpProps[0]);
	rowList.aEntries[0].lpProps.lpProps[1].ulPropTag = PR_MEMBER_RIGHTS;
	rowList.aEntries[0].lpProps.lpProps[1].value.l = role;

	retval = ModifyTable(obj_folder, &rowList);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Modify permissions for a user on a given folder
   
   \param obj_folder the folder we add permission for
   \param username the Exchange username we modify permissions for
   \param role the permission mask value (see AddUserPermission)

   \return MAPI_E_SUCCESS on success, otherwise a failure code (MAPISTATUS)
   indicating the error.

   \note Developers may also call GetLastError() to retrieve the last
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
	const char		*names[2];
	const char		*user = NULL;
	struct SRowSet		*rows = NULL;
	struct SRowSet		rowset;
	struct SPropTagArray   	*flaglist = NULL;
	struct mapi_SRowList	rowList;
	struct SPropValue	*lpProp;
	mapi_object_t		obj_table;
	uint32_t		Numerator;
	uint32_t		Denominator;
	bool			found = false;
	uint32_t		i = 0;

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);

	rowList.padding = 0;

	mem_ctx = talloc_named(NULL, 0, "ModifyUserPermission");

	SPropTagArray = set_SPropTagArray(mem_ctx, 2, PR_ENTRYID, PR_DISPLAY_NAME);
	names[0] = username;
	names[1] = NULL;
	retval = ResolveNames(mapi_object_get_session(obj_folder), (const char **)names, 
			      SPropTagArray, &rows, &flaglist, 0);
	MAPIFreeBuffer(SPropTagArray);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	if (flaglist->aulPropTag[0] == MAPI_RESOLVED) {
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
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	SPropTagArray = set_SPropTagArray(mem_ctx, 4,
					  PR_ENTRYID,
					  PR_MEMBER_RIGHTS,
					  PR_MEMBER_ID,
					  PR_MEMBER_NAME);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	retval = QueryPosition(&obj_table, &Numerator, &Denominator);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	retval = QueryRows(&obj_table, Denominator, TBL_ADVANCE, &rowset);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

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
				OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
				found = true;
				break;
			}
		}
	}

	mapi_object_release(&obj_table);
	talloc_free(mem_ctx);
	
	OPENCHANGE_RETVAL_IF((found == true), MAPI_E_NOT_FOUND, 0);

	return MAPI_E_SUCCESS;
}


/**
   \details Remove permissions for a user on a given folder

   \param obj_folder the folder we add permission for
   \param username the Exchange username we remove permissions for

   \return MAPI_E_SUCCESS on success, otherwise a failure code (MAPISTATUS)
   indicating the error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized.
   - MAPI_E_INVALID_PARAMETER: username or obj_folder are NULL
   - MAPI_E_NOT_FOUND: couldn't find or remove permissions for the
     given user

   \sa ResolveNames, GetTable, ModifyTable
 */
_PUBLIC_ enum MAPISTATUS RemoveUserPermission(mapi_object_t *obj_folder, const char *username)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	struct SPropTagArray	*SPropTagArray;
	const char		*names[2];
	const char		*user = NULL;
	struct SRowSet		*rows = NULL;
	struct SRowSet		rowset;
	struct SPropTagArray   	*flaglist = NULL;
	struct mapi_SRowList	rowList;
	struct SPropValue	*lpProp;
	mapi_object_t		obj_table;
	uint32_t		Numerator;
	uint32_t		Denominator;
	bool			found = false;
	uint32_t		i = 0;

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "RemoveUserPermission");

	SPropTagArray = set_SPropTagArray(mem_ctx, 2, PR_ENTRYID, PR_DISPLAY_NAME);
	names[0] = username;
	names[1] = NULL;
	retval = ResolveNames(mapi_object_get_session(obj_folder), (const char **)names, 
			      SPropTagArray, &rows, &flaglist, 0);
	MAPIFreeBuffer(SPropTagArray);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Check if the username was found */
	OPENCHANGE_RETVAL_IF((flaglist->aulPropTag[0] != MAPI_RESOLVED), MAPI_E_NOT_FOUND, mem_ctx);

	user = find_SPropValue_data(&(rows->aRow[0]), PR_DISPLAY_NAME);

	mapi_object_init(&obj_table);
	retval = GetTable(obj_folder, &obj_table);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	SPropTagArray = set_SPropTagArray(mem_ctx, 4,
					  PR_ENTRYID,
					  PR_MEMBER_RIGHTS,
					  PR_MEMBER_ID,
					  PR_MEMBER_NAME);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	retval = QueryPosition(&obj_table, &Numerator, &Denominator);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	retval = QueryRows(&obj_table, Denominator, TBL_ADVANCE, &rowset);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

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
				OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
				found = true;
				break;
			}
		}
	}

	mapi_object_release(&obj_table);
	talloc_free(mem_ctx);

	OPENCHANGE_RETVAL_IF((found == true), MAPI_E_NOT_FOUND, 0);

	return MAPI_E_SUCCESS;
}


/**
   \details Implement the BestBody algorithm and return the best body
   content type for a given message.

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND. If
   MAPI_E_NOT_FOUND is returned then format is set to 0x0
   (undefined). If MAPI_E_SUCCESS is returned, then format can have
   one of the following values:
   - olEditorText: format is plain text
   - olEditorHTML: format is HTML
   - olEditorRTF: format is RTF

   \param obj_message the message we find the best body for
   \param format the format - see above.
 */
_PUBLIC_ enum MAPISTATUS GetBestBody(mapi_object_t *obj_message, uint8_t *format)
{
	enum MAPISTATUS		retval;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct SPropValue	*lpProps;
	struct SRow		aRow;
	uint32_t		count;
	uint8_t			RtfInSync;
	uint32_t		PlainStatus;
	uint32_t		RtfStatus;
	uint32_t		HtmlStatus;
	const uint32_t		*err_code;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!format, MAPI_E_INVALID_PARAMETER, NULL);

	/* Step 1. Retrieve properties needed by the BestBody algorithm */
	SPropTagArray = set_SPropTagArray(global_mapi_ctx->mem_ctx, 0x4,
					  PR_BODY,
					  PR_RTF_COMPRESSED,
					  PR_HTML,
					  PR_RTF_IN_SYNC);
	retval = GetProps(obj_message, SPropTagArray, &lpProps, &count);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		*format = 0;
		OPENCHANGE_RETVAL_ERR(MAPI_E_NOT_FOUND, 0);
	}

	aRow.ulAdrEntryPad = 0;
	aRow.cValues = count;
	aRow.lpProps = lpProps;

	/* Step 2. Retrieve properties values and map errors */
	RtfInSync = *(const uint8_t *)find_SPropValue_data(&aRow, PR_RTF_IN_SYNC);

	err_code = (const uint32_t *)find_SPropValue_data(&aRow, PR_BODY_ERROR);
	PlainStatus = (err_code) ? *err_code : 0;

	err_code = (const uint32_t *)find_SPropValue_data(&aRow, PR_RTF_COMPRESSED_ERROR);
	RtfStatus = (err_code) ? *err_code : 0;

	err_code = (const uint32_t *)find_SPropValue_data(&aRow, PR_BODY_HTML_ERROR);
	HtmlStatus = (err_code) ? *err_code : 0;

	/* Step 3. Determine the body format (9 possible cases) */

	/* case 1 */
	if ((PlainStatus == MAPI_E_NOT_FOUND) && (RtfStatus == MAPI_E_NOT_FOUND) && 
	    (HtmlStatus == MAPI_E_NOT_FOUND)) {
		*format = 0;
		OPENCHANGE_RETVAL_ERR(MAPI_E_NOT_FOUND, 0);
	}
	
	/* case 2 */
	if (((PlainStatus == MAPI_E_NOT_ENOUGH_MEMORY) || (PlainStatus == 0)) && 
	    (RtfStatus == MAPI_E_NOT_FOUND) && (HtmlStatus == MAPI_E_NOT_FOUND)) {
		*format = olEditorText;
		return MAPI_E_SUCCESS;
	}

	/* case 3 */
	if ((PlainStatus == MAPI_E_NOT_ENOUGH_MEMORY) &&
	    (RtfStatus == MAPI_E_NOT_ENOUGH_MEMORY) && 
	    (HtmlStatus == MAPI_E_NOT_FOUND)) {
		*format = olEditorRTF;
		return MAPI_E_SUCCESS;
	}

	/* case 4 */
	if ((PlainStatus == MAPI_E_NOT_ENOUGH_MEMORY) &&
	    (RtfStatus == MAPI_E_NOT_ENOUGH_MEMORY) &&
	    (HtmlStatus == MAPI_E_NOT_ENOUGH_MEMORY) &&
	    (RtfInSync == 1)) {
		*format = olEditorRTF;
		return MAPI_E_SUCCESS;
	}

	/* case 5 */
	if ((PlainStatus == MAPI_E_NOT_ENOUGH_MEMORY) &&
	    (RtfStatus == MAPI_E_NOT_ENOUGH_MEMORY) &&
	    (HtmlStatus == MAPI_E_NOT_ENOUGH_MEMORY) &&
	    (RtfInSync == 0)) {
		*format = olEditorHTML;
		return MAPI_E_SUCCESS;
	}

	/* case 6 */
	if (((RtfStatus == 0) || (RtfStatus == MAPI_E_NOT_ENOUGH_MEMORY)) &&
	    ((HtmlStatus == 0) || (HtmlStatus == MAPI_E_NOT_ENOUGH_MEMORY)) &&
	    (RtfInSync == 1)) {
		*format = olEditorRTF;
		return MAPI_E_SUCCESS;
	}

	/* case 7 */
	if (((RtfStatus == 0) || (RtfStatus == MAPI_E_NOT_ENOUGH_MEMORY)) &&
	    ((HtmlStatus == 0) || (HtmlStatus == MAPI_E_NOT_ENOUGH_MEMORY)) &&
	    (RtfInSync == 0)) {
		*format = olEditorHTML;
		return MAPI_E_SUCCESS;
	}
	
	/* case 8 */
	if (((PlainStatus == 0) || (PlainStatus == MAPI_E_NOT_ENOUGH_MEMORY)) &&
	    ((RtfStatus == 0) || (RtfStatus == MAPI_E_NOT_ENOUGH_MEMORY)) &&
	    (RtfInSync == 1)) {
		*format = olEditorRTF;
		return MAPI_E_SUCCESS;
	}

	/* case 9 */
	if (((PlainStatus == 0) || (PlainStatus == MAPI_E_NOT_ENOUGH_MEMORY)) &&
	    ((RtfStatus == 0) || (RtfStatus == MAPI_E_NOT_ENOUGH_MEMORY)) &&
	    (RtfInSync == 0)) {
		*format = olEditorText;
		return MAPI_E_SUCCESS;
	}

	OPENCHANGE_RETVAL_ERR(MAPI_E_NOT_FOUND, 0);
}
