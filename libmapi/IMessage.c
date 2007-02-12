/*
 *  OpenChange MAPI implementation.
 *
 *  Copyright (C) Julien Kerihuel 2007.
 *  Copyright (C) Fabien Le Mentec 2007.
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


/**
 * Creates a new attachment
 */

enum MAPISTATUS	CreateAttach(struct emsmdb_context *emsmdb, uint32_t ulFlags, 
			     uint32_t hdl_message, uint32_t *hdl_attach)
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

/**
 * Open the attachment table
 */
enum MAPISTATUS GetAttachmentTable(struct emsmdb_context *emsmdb, uint32_t hdl_message, uint32_t *hdl_attach_table)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetAttachmentTable_req	request;
	NTSTATUS			status;
	TALLOC_CTX			*mem_ctx;
	uint32_t			size = 0;

	mem_ctx = talloc_init("OpenAttachmentTable");

	*hdl_attach_table = 0;

	/* Fill the GetAttachmentTable operation */
	request.handle_idx = 0;
	request.unknown = 0x2;
	size += sizeof(request.handle_idx) + sizeof(request.unknown);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetAttachmentTable;
	mapi_req->mapi_flags = 0;
	mapi_req->u.mapi_GetAttachmentTable = request;
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

	*hdl_attach_table = mapi_response->handles[mapi_response->mapi_repl->mapi_flags];

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
 * Open an attachment given its PR_ATTACH_NUM
 */

enum MAPISTATUS	OpenAttach(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t hdl_message, uint32_t num_attach, uint32_t *hdl_attach)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct OpenAttach_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
 
	mem_ctx = talloc_init("OpenAttach");

	*hdl_attach = 0;
	size = 0;

	/* Fill the OpenAttach operation */
	request.unknown_0 = 0x0;
	size += sizeof(uint8_t);
	request.unknown_1 = 0x1;
	size += sizeof(uint16_t);
	request.num_attach = num_attach;
	size += sizeof(uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenAttach;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_OpenAttach = request;
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


/**
 * Adds, deletes, or modifies message recipients.
 */

enum MAPISTATUS	ModifyRecipients(struct emsmdb_context *emsmdb, uint32_t ulFlags, 
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



/**
 * Saves all changes to the message and marks it as ready for sending.
 */

enum MAPISTATUS	SubmitMessage(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t hdl_message)
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
