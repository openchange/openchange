/*
 *  OpenChange MAPI implementation.
 *
 *  Copyright (C) Julien Kerihuel 2007.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <gen_ndr/ndr_exchange.h>

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
