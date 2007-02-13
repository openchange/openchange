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
#include <gen_ndr/ndr_exchange.h>

/**
 * Creates a new message
 */

enum MAPISTATUS	CreateMessage(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t hdl_folder, uint64_t id_folder, uint32_t *hdl_message)
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

/**
 * Deletes one or more messages.
 */

enum MAPISTATUS	DeleteMessages(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t hdl_folder, uint64_t* id_messages, uint32_t cn_messages)
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
