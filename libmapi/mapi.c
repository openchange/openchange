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
		struct ndr_print *ndr_print;

		ndr_print = talloc_zero(mem_ctx, struct ndr_print);
		ndr_print->print = ndr_print_debug_helper;

		ndr_print_MAPISTATUS(ndr_print, "error code",
				     mapi_response->mapi_repl->error_code);

		return mapi_response->mapi_repl->error_code;
	}

	DEBUG(3, ("folder id = 0x%.16llx\n", mapi_response->mapi_repl->u.mapi_GetReceiveFolder.folder_id));
	*folder_id = mapi_response->mapi_repl->u.mapi_GetReceiveFolder.folder_id;

	DEBUG(3, ("GetReceiveFolder: handles[0] = 0x%.8x\n", mapi_response->handles[0]));

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
		struct ndr_print *ndr_print;

		ndr_print = talloc_zero(mem_ctx, struct ndr_print);
		ndr_print->print = ndr_print_debug_helper;

		ndr_print_MAPISTATUS(ndr_print, "error code",
				     mapi_response->mapi_repl->error_code);

		return mapi_response->mapi_repl->error_code;
	}

	*obj_id = mapi_response->handles[1];

	DEBUG(3, ("OpenFolder: handles[0] = 0x%.8x\n", mapi_response->handles[0]));
	DEBUG(3, ("OpenFolder: handles[1] = 0x%.8x\n", mapi_response->handles[1]));

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
		struct ndr_print *ndr_print;

		ndr_print = talloc_zero(mem_ctx, struct ndr_print);
		ndr_print->print = ndr_print_debug_helper;

		ndr_print_MAPISTATUS(ndr_print, "error code",
				     mapi_response->mapi_repl->error_code);

		return mapi_response->mapi_repl->error_code;
	}

	*hid_msg = mapi_response->handles[1];
	printf("-- message subject: %s\n", mapi_response->mapi_repl->u.mapi_OpenMessage.subject);

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
		struct ndr_print *ndr_print;

		ndr_print = talloc_zero(mem_ctx, struct ndr_print);
		ndr_print->print = ndr_print_debug_helper;

		ndr_print_MAPISTATUS(ndr_print, "error code",
				     mapi_response->mapi_repl->error_code);

		return mapi_response->mapi_repl->error_code;
	}

	*table_id = mapi_response->handles[1];

	DEBUG(3, ("GetContentsTable: handles[0] = 0x%.8x\n", mapi_response->handles[0]));
	DEBUG(3, ("GetContentsTable: handles[1] = 0x%.8x\n", mapi_response->handles[1]));

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
               struct ndr_print *ndr_print;

               ndr_print = talloc_zero(mem_ctx, struct ndr_print);
               ndr_print->print = ndr_print_debug_helper;

               ndr_print_MAPISTATUS(ndr_print, "error code",
                                    mapi_response->mapi_repl->error_code);

               return mapi_response->mapi_repl->error_code;
       }

       emsmdb->prop_count = properties->cValues;
       emsmdb->properties = properties->aulPropTag;
       aRow = emsmdb_set_SRow(emsmdb, 1, mapi_response->mapi_repl->u.mapi_GetProps.prop_data);

       {
	 struct ndr_print *ndr;
	 
	 ndr = talloc_zero(mem_ctx, struct ndr_print);
	 ndr->print = ndr_print_debug_helper;

	 ndr_print_SRow(ndr, "GetProps", aRow);
       }

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
		struct ndr_print *ndr_print;

		ndr_print = talloc_zero(mem_ctx, struct ndr_print);
		ndr_print->print = ndr_print_debug_helper;

		ndr_print_MAPISTATUS(ndr_print, "error code",
				     mapi_response->mapi_repl->error_code);

		return mapi_response->mapi_repl->error_code;
	}

	/* Fill in the SRowSet array */
	reply = &(mapi_response->mapi_repl[1].u.mapi_QueryRows);

	if (emsmdb->prop_count) {
		rowSet[0]->cRows = reply->results_count;		
		rowSet[0]->aRow = emsmdb_set_SRow(emsmdb, rowSet[0]->cRows, reply->inbox);
	}
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

MAPISTATUS	OpenMsgStore(struct emsmdb_context *emsmdb, uint32_t ulFlags, 
			     uint32_t *handle_id, const char *mailbox)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct OpenMsgStore_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;


	mem_ctx = talloc_init("OpenMsgStore");

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
		struct ndr_print *ndr_print;

		ndr_print = talloc_zero(mem_ctx, struct ndr_print);
		ndr_print->print = ndr_print_debug_helper;

		ndr_print_MAPISTATUS(ndr_print, "error code",
				     mapi_response->mapi_repl->error_code);

		return mapi_response->mapi_repl->error_code;
	}

	*handle_id = mapi_response->handles[0];

	DEBUG(3, ("OpenMsgStore: handles[0] = 0x%.8x\n", mapi_response->handles[0]));
	
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}



