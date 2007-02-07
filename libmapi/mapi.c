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

#include "openchange.h"
#include "exchange.h"
#include "ndr_exchange.h"
#include "libmapi/include/mapidefs.h"
#include "libmapi/include/nspi.h"
#include "libmapi/include/emsmdb.h"
#include "libmapi/mapicode.h"
#include "libmapi/include/proto.h"
#include "libmapi/include/mapi_proto.h"

MAPISTATUS	Release(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t handle_id)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ *mapi_req;
	struct Release_req	request;
	NTSTATUS		status;
	TALLOC_CTX		*mem_ctx;
	uint32_t		size = 0;

	mem_ctx = talloc_init("Release");

	/* Fill the Release operation */
	request.unknown = 0;
	size += 1;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_Release;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_Release = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = handle_id;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

MAPISTATUS	GetReceiveFolder(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t handle_id, uint64_t *folder_id)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct GetReceiveFolder_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;

	mem_ctx = talloc_init("GetReceiveFolder");

	/* Fill the GetReceiveFolder operation */
	request.handle_id = 0x0;
	size += sizeof (uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetReceiveFolder;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_GetReceiveFolder = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = handle_id;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	*folder_id = mapi_response->mapi_repl->u.mapi_GetReceiveFolder.folder_id;
	
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

MAPISTATUS	OpenFolder(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t handle_id, uint64_t folder_id, uint32_t *obj_id)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct OpenFolder_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;

	mem_ctx = talloc_init("GetReceiveFolder");

	/* Fill the OpenFolder operation */
	request.handle = 0x100;
	request.folder_id = folder_id;
	request.unknown = 0x0;
	size += sizeof (uint16_t) + sizeof(uint64_t) + sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenFolder;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_OpenFolder = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = handle_id;
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	*obj_id = mapi_response->handles[1];

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

MAPISTATUS	OpenMessage(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t store_id, uint64_t folder_id, uint64_t message_id, uint32_t *hid_msg)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct OpenMessage_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
 
	mem_ctx = talloc_init("OpenMessage");

	/* Fill the OpenFolder operation */
	request.unknown = 0x100;
	request.max_data = 0xfff;
	request.folder_id = folder_id;
	request.padding = 0;
	request.message_id = message_id;
	size = sizeof (uint16_t) + sizeof(uint16_t) + sizeof(uint64_t) + sizeof(uint8_t) + sizeof(uint64_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenMessage;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_OpenMessage = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = store_id;
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	*hid_msg = mapi_response->handles[1];

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


MAPISTATUS	GetContentsTable(struct emsmdb_context *emsmdb, uint32_t ulFlags, 
				 uint32_t folder_id, uint32_t *table_id)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct GetContentsTable_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;

	mem_ctx = talloc_init("GetContentsTable");

	/* Fill the GetContentsTable operation */
	request.unknown = 0;
	request.unknown2 = 0x1;
	size += 3;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetContentsTable;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_GetContentsTable = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = folder_id;
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	*table_id = mapi_response->handles[1];

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

MAPISTATUS     GetProps(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t hdl_message, struct SPropTagArray* properties)
{
       struct mapi_request	*mapi_request;
       struct mapi_response	*mapi_response;
       struct EcDoRpc_MAPI_REQ	*mapi_req;
       struct GetProps_req	request;
       NTSTATUS			status;
       uint32_t			size = 0;
       TALLOC_CTX		*mem_ctx;
       struct SRow		*aRow;
 
       mem_ctx = talloc_init("GetProps");

       /* Fill the GetProps operation */
       properties->cValues -= 1;
       request.unknown = 0x0;
       request.unknown2 = 0x0;
       request.prop_count = (uint16_t)properties->cValues;
       request.properties = properties->aulPropTag;
       size = sizeof (uint8_t) + sizeof(uint32_t) + sizeof(uint16_t) + properties->cValues * sizeof(uint32_t);

       /* Fill the MAPI_REQ request */
       mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
       mapi_req->opnum = op_MAPI_GetProps;
       mapi_req->mapi_flags = ulFlags;
       mapi_req->u.mapi_GetProps = request;
       size += 4;

       /* Fill the mapi_request structure */
       mapi_request = talloc_zero(mem_ctx, struct mapi_request);
       mapi_request->mapi_len = size + sizeof (uint32_t);
       mapi_request->length = size;
       mapi_request->mapi_req = mapi_req;
       mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
       mapi_request->handles[0] = hdl_message;

       status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

       if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
               return mapi_response->mapi_repl->error_code;
       }

       emsmdb->prop_count = properties->cValues;
       emsmdb->properties = properties->aulPropTag;
       aRow = emsmdb_get_SRow(emsmdb, 1, mapi_response->mapi_repl->u.mapi_GetProps.prop_data, 0);

       talloc_free(mem_ctx);

       return MAPI_E_SUCCESS;
}


/*
  SetColumns set a *cache* in emsmdb_context. The call will be
  processed with another request such as QueryRows
*/

MAPISTATUS	SetColumns(struct emsmdb_context *emsmdb, uint32_t ulFlags, struct SPropTagArray *properties)
{
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SetColumns_req	request;

	/* Fill the SetColumns operation */
	request.handle = 0;
	request.unknown = 0;
	request.prop_count = properties->cValues - 1;
	request.properties = properties->aulPropTag;
	emsmdb->cache_size = 4 + request.prop_count * sizeof (uint32_t);

	/* Store the property tag array internally for further
	   decoding purpose 
	*/
	emsmdb->prop_count = properties->cValues - 1;
	emsmdb->properties = properties->aulPropTag;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(emsmdb->mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetColumns;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_SetColumns = request;
	emsmdb->cache_size += 2;
	
	emsmdb->cache_requests[0] = mapi_req;
	emsmdb->cache_count += 1;

	return MAPI_E_SUCCESS;
	
}

/*
  QueryRows returns a RowSet with the properties returned by Exchange Server
*/

MAPISTATUS	QueryRows(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t folder_id, uint16_t row_count, struct SRowSet **rowSet)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct QueryRows_req	request;
	struct QueryRows_repl	*reply;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;

	mem_ctx = talloc_init("QueryRows");

	/* Fill the QueryRows operation */
	request.unknown = 0;
	request.flag_noadvance = 0;
	request.unknown1 = 1;
	request.row_count = row_count;
	size += 5;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_QueryRows;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_QueryRows = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = folder_id;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	/* Fill in the SRowSet array */
	reply = &(mapi_response->mapi_repl[1].u.mapi_QueryRows);

	if (emsmdb->prop_count) {
		rowSet[0]->cRows = reply->results_count;		
		rowSet[0]->aRow = emsmdb_get_SRow(emsmdb, rowSet[0]->cRows, reply->inbox, 1);
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}




MAPISTATUS	OpenMsgStore(struct emsmdb_context *emsmdb, uint32_t ulFlags, 
			     uint32_t *handle_id, uint64_t *id_outbox, const char *mailbox)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct OpenMsgStore_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;


	mem_ctx = talloc_init("OpenMsgStore");

	*id_outbox = 0;

	/* Fill the OpenMsgStore operation */
	request.col = 0x0;
 	request.codepage = 0xc01; /*ok values: 0xc01 0xc09 */
	request.padding = 0;
	request.row = 0x0;
	request.mailbox_path = talloc_strdup(mem_ctx, mailbox);
	size = 10 + strlen(mailbox) + 1;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenMsgStore;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_OpenMsgStore = request;
	size += 6;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = 0xffffffff;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	*handle_id = mapi_response->handles[0];

	/* get outbox folder id */
	*id_outbox = mapi_response->mapi_repl->u.mapi_OpenMsgStore.folder_id[3];

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

MAPISTATUS	CreateMessage(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t hdl_folder, uint64_t id_folder, uint32_t *hdl_message)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct CreateMessage_req request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
 
	mem_ctx = talloc_init("CreateMessage");

	*hdl_message = 0;

	/* Fill the OpenFolder operation */
	request.unknown = 0x100;
	request.max_data = 0xfff;
	request.folder_id = id_folder;
	request.padding = 0;
	size = sizeof (uint16_t) + sizeof(uint16_t) + sizeof(uint64_t) + sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_CreateMessage;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_CreateMessage = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = hdl_folder;
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	*hdl_message = mapi_response->handles[1];

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

MAPISTATUS	ModifyRecipients(struct emsmdb_context *emsmdb, uint32_t ulFlags, 
				 struct SRowSet *SRowSet, uint32_t hdl_message)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct ModifyRecipients_req request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	unsigned long		i_prop, j;
	unsigned long		i_recip;
	const char *fixme = lp_parm_string(-1, "FIXME", "modifyrecipients");
	int unknown;
 
	mem_ctx = talloc_init("ModifyRecipients");

	if (!fixme) {
	  return 0;
	}
	unknown = strtol(fixme, NULL, 16);

	size = 0;

	/* Fill the ModifyRecipients operation */
	request.flags = MODRECIP_NULL;
	request.prop_count = SRowSet->aRow[0].cValues;
	size += sizeof(uint8_t) + sizeof(uint16_t);

	/* 
	 * append here property tags that can be fetched with
	 * ResolveNames bu shouldn't be included in ModifyRecipients rows
	 */
	request.properties = get_MAPITAGS_SRow(mem_ctx, &SRowSet->aRow[0]);
 	request.prop_count = MAPITAGS_delete_entries(request.properties, 0x2, 
						     PR_DISPLAY_NAME,
						     PR_GIVEN_NAME);
	size += request.prop_count * sizeof(uint32_t);

	request.cValues = SRowSet->cRows;
	size += sizeof(uint16_t);
	request.recipient = talloc_array(mem_ctx, struct recipients, request.cValues);

	for (i_recip = 0; i_recip < request.cValues; i_recip++) {
		struct SRow *aRow;
		struct recipients_headers *headers;
		struct ndr_push *ndr;
		struct mapi_SPropValue mapi_sprop;

		ndr = talloc_zero(mem_ctx, struct ndr_push);
		aRow = &(SRowSet->aRow[i_recip]);
		headers = &(request.recipient[i_recip].headers);
		
		request.recipient[i_recip].idx = i_recip;
		size += sizeof(uint8_t);
		request.recipient[i_recip].reserved = 0x01000000;
		size += sizeof(uint32_t);

		/* 
		   FIXME: We have not yet identified this field. It
		   can be either 0x004a0451 or 0x00410451 but these
		   values are unique for each server 
		*/
		headers->unknown = unknown;
		size += sizeof(uint32_t);
		
		/* display name */
		headers->display_name = (char *) find_SPropValue_data(aRow, PR_7BIT_DISPLAY_NAME);
		if (!headers->display_name) {
			headers->display_name = "";
			size += 1;
		} else {
			size += strlen(headers->display_name) + 1;
		}
		
		/* full name */
		headers->full_name = (char *) find_SPropValue_data(aRow, PR_GIVEN_NAME);
		if (!headers->full_name) {
			headers->full_name = "";
		} 
		size += strlen(headers->full_name) + 1;
		
		/* user name */
		headers->username = (char *) find_SPropValue_data(aRow, PR_DISPLAY_NAME);
		if (!headers->username) {
			return -1;
		} 
		size += strlen(headers->username) + 1;
		  
		headers->prop_count = request.prop_count;
		size += sizeof(uint16_t);
		headers->padding = 0;
		size += sizeof(uint8_t);
		
		/* for each property, we set the switch values and ndr_flags then call mapi_SPropValue_CTR */
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		for (i_prop = 0; i_prop < request.prop_count; i_prop++) {
			for (j = 0; j < aRow->cValues; j++) {
				if (aRow->lpProps[j].ulPropTag == request.properties[i_prop]) {
					ndr_push_set_switch_value(ndr, &mapi_sprop.value, 
								  (aRow->lpProps[j].ulPropTag & 0xFFFF));
					cast_mapi_SPropValue(&mapi_sprop, &aRow->lpProps[j]);
					ndr_push_mapi_SPropValue_CTR(ndr, NDR_SCALARS, &mapi_sprop.value);
				}
			}
		}
		
		headers->prop_values.length = ndr->offset;
		headers->prop_values.data = ndr->data;
		/* add the blob size field */
		size += sizeof(uint16_t);
		size += headers->prop_values.length;
	}
	
	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_ModifyRecipients;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_ModifyRecipients = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof(uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = hdl_message;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

MAPISTATUS	SetProps(struct emsmdb_context *emsmdb, uint32_t ulFlags, struct SPropValue *sprops, unsigned long cn_props, uint32_t hdl_object)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SetProps_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	unsigned long		i_prop;
	struct mapi_SPropValue	*mapi_props;
 
	mem_ctx = talloc_init("SetProps");

	/* Fill the SetProps operation */
	request.unknown = 0x00;
	size += sizeof(uint8_t);

	/* build the array */
	request.values.sprop_array = talloc_array(mem_ctx, struct mapi_SPropValue, cn_props);
	mapi_props = request.values.sprop_array;
	for (i_prop = 0; i_prop < cn_props; ++i_prop) {
		size += cast_mapi_SPropValue(&mapi_props[i_prop], &sprops[i_prop]);
		size += sizeof(uint32_t);
	  }

	request.values.sprop_count = cn_props;
	size += sizeof(uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetProps;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_SetProps = request;
	size += 6;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = hdl_object;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


MAPISTATUS	SetProps2(struct emsmdb_context *emsmdb, uint32_t ulFlags, struct SPropValue *sprops, unsigned long cn_props, uint32_t hdl_related, uint32_t hdl_object)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SetProps_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	unsigned long		i_prop;
	struct mapi_SPropValue	*mapi_props;
 
	mem_ctx = talloc_init("SetProps2");

	/* Fill the SetProps operation */
	request.unknown = 0x00;
	size += sizeof(uint8_t);

	/* build the array */
	request.values.sprop_array = talloc_array(mem_ctx, struct mapi_SPropValue, cn_props);
	mapi_props = request.values.sprop_array;
	for (i_prop = 0; i_prop < cn_props; ++i_prop) {
		size += cast_mapi_SPropValue(&mapi_props[i_prop], &sprops[i_prop]);
		size += sizeof(uint32_t);
	  }

	request.values.sprop_count = cn_props;
	size += sizeof(uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetProps;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_SetProps = request;
	size += 6;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = hdl_object;
	mapi_request->handles[1] = hdl_related;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


MAPISTATUS	SubmitMessage(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t hdl_message)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SubmitMessage_req request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
 
	mem_ctx = talloc_init("SubmitMessage");

	size = 0;

	/* Fill the SubmitMessage operation */
	request.unknown = 0x00;
	size += sizeof(uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SubmitMessage;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_SubmitMessage = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = hdl_message;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


MAPISTATUS	DeleteMessages(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t hdl_folder, uint64_t* id_messages, uint32_t cn_messages)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct DeleteMessages_req request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
 
	mem_ctx = talloc_init("DeleteMessages");

	size = 0;

	/* Fill the DeleteMessages operation */
	request.unknown = 0x00;
	size += sizeof(uint8_t);
	request.flags = 0x100;
	size += sizeof(uint16_t);
	request.cn_ids = (uint16_t)cn_messages;
	size += sizeof(uint16_t);
	request.message_ids = id_messages;
	size += request.cn_ids * sizeof(uint64_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_DeleteMessages;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_DeleteMessages = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = hdl_folder;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


MAPISTATUS	CreateAttach(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t hdl_message, uint32_t* hdl_attach)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct CreateAttach_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;

	*hdl_attach = 0;
 
	mem_ctx = talloc_init("CreateAttach");

	/* Fill the CreateAttach operation */
	request.flags = 0x100;
	size = sizeof (uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_CreateAttach;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_CreateAttach = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = hdl_message;
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	*hdl_attach = mapi_response->handles[1];

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


MAPISTATUS	SaveChanges(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t hdl_related, uint32_t hdl_object)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SaveChanges_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
 
	mem_ctx = talloc_init("SaveChanges");

	size = 0;

	/* Fill the SaveChanges operation */
	request.flags = 0x001;
	request.unknown = 0x0;
	size += sizeof(uint16_t) + sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SaveChanges;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_SaveChanges = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = hdl_object;
	mapi_request->handles[1] = hdl_related;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}



/**
 * Wrapper on nspi_ResolveNames
 * Fill an adrlist and flaglist structure
 *
 */

struct SRowSet *ResolveNames(struct emsmdb_context *emsmdb, const char **usernames, struct SPropTagArray *props)			
{
	struct SRowSet		SRowSet;
	struct SRowSet		*SRowSet2;

	nspi_ResolveNames(emsmdb->nspi, usernames, props, &SRowSet);

	SRowSet2 = talloc_zero(emsmdb->mem_ctx, struct SRowSet);
	*SRowSet2 = SRowSet;
	return SRowSet2;
}

/**
 * Initiates a connection on the exchange_nsp pipe
 *
 */

MAPISTATUS MAPIInitialize(struct emsmdb_context *emsmdb, struct cli_credentials *cred)
{
	struct dcerpc_pipe      *p;
        NTSTATUS status;
	const char *binding = lp_parm_string(-1, "exchange", "nspi_binding");

	if (!binding) {
		printf("You must specify a binding string\n");
		return MAPI_E_LOGON_FAILED;
	}

	status = dcerpc_pipe_connect(emsmdb->mem_ctx, 
				     &p, binding, &dcerpc_table_exchange_nsp,
				     cred, NULL);

	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to connect to remote server: %s %s\n", binding, nt_errstr(status));
	}

	emsmdb->nspi = talloc_zero(emsmdb->mem_ctx, struct nspi_context);
	emsmdb->nspi->rpc_connection = p;
	emsmdb->nspi = nspi_bind(emsmdb->mem_ctx, p, cred);

	if (!emsmdb->nspi) {
		return MAPI_E_LOGON_FAILED;
	}

	return MAPI_E_SUCCESS;
}

MAPISTATUS MAPIUninitialize(struct emsmdb_context *emsmdb)
{
	BOOL ret;

	if ((ret = nspi_unbind(emsmdb->nspi)) != True) {
		/* FIXME with a valid error value */
		return MAPI_E_LOGON_FAILED;
	}
	return MAPI_E_SUCCESS;
}
