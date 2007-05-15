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


/**
 * OpenFolder is not part of MSDN and is certainly OpenEntry in
 * fact. Nevertheless, we keep this name until we are not absolutely
 * sure.
 *
 */

_PUBLIC_ enum MAPISTATUS OpenFolder(mapi_object_t *obj_store, mapi_id_t id_folder,
				    mapi_object_t *obj_folder)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct OpenFolder_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("OpenFolder");

	/* Fill the OpenFolder operation */
	request.handle_idx = 0x1;
	request.folder_id = id_folder;
	request.unknown = 0x0;
	size += sizeof (uint8_t) + sizeof(uint64_t) + sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenFolder;
	mapi_req->mapi_flags = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_OpenFolder = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_store);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);	

	mapi_object_set_id(obj_folder, id_folder);
	mapi_object_set_handle(obj_folder, mapi_response->handles[1]);

	talloc_free(mapi_response);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}



/**
 * Obtains the folder that was established as the destination for
 * incoming messages of a specified message class or the default
 * receive folder for the message store.
 */

_PUBLIC_ enum MAPISTATUS GetReceiveFolder(mapi_object_t *obj_store, 
					  mapi_id_t *id_folder)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct GetReceiveFolder_req request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("GetReceiveFolder");

	*id_folder = 0;

	/* Fill the GetReceiveFolder operation */
	request.unknown = 0x0;
	size += sizeof (uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetReceiveFolder;
	mapi_req->mapi_flags = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetReceiveFolder = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_store);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	*id_folder = mapi_response->mapi_repl->u.mapi_GetReceiveFolder.folder_id;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
 * Obtain the deleted items folder id.
 * No action is performed on the network
 * since we have the id from OpenMsgStore.
 */

_PUBLIC_ enum MAPISTATUS GetDeletedItemsFolder(mapi_object_t *obj_store, 
					       mapi_id_t *deleted_items_id)
{
	mapi_object_store_t	*store;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;

	store = (mapi_object_store_t *)obj_store->private_data;
	*deleted_items_id = store->deleted_items_id;

	return MAPI_E_SUCCESS;
}

/**
 * Obtain the outbox folder id.
 * No action is performed on the network
 * since we have the id from OpenMsgStore.
 */

_PUBLIC_ enum MAPISTATUS GetOutboxFolder(mapi_object_t *obj_store, 
					 mapi_id_t *outbox_id)
{
	mapi_object_store_t	*store;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;

	store = (mapi_object_store_t *)obj_store->private_data;
	*outbox_id = store->outbox_id;

	return MAPI_E_SUCCESS;
}

/**
 * Obtain the sent items folder id.
 * No action is performed on the network
 * since we have the id from OpenMsgStore.
 */

_PUBLIC_ enum MAPISTATUS GetSentItemsFolder(mapi_object_t *obj_store, 
					    mapi_id_t *sentitems_id)
{
	mapi_object_store_t	*store;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;

	store = (mapi_object_store_t *)obj_store->private_data;
	*sentitems_id = store->sent_items_id;

	return MAPI_E_SUCCESS;
}
