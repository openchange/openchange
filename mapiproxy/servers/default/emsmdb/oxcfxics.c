/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Wolfgang Sourdeau 2010

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

/**
   \file oxcfxics.c

   \brief FastTransfer and ICS object routines and Rops
 */

#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"

/**
   \details EcDoRpc EcDoRpc_RopFastTransferSourceCopyTo (0x4d) Rop. This operation initializes a FastTransfer operation to download content from a given messaging object and its descendant subobjects.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopFastTransferSourceCopyTo(TALLOC_CTX *mem_ctx,
							     struct emsmdbp_context *emsmdbp_ctx,
							     struct EcDoRpc_MAPI_REQ *mapi_req,
							     struct EcDoRpc_MAPI_REPL *mapi_repl,
							     uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS				retval;
	enum MAPISTATUS				*retvals;
	struct mapi_handles			*parent_object_handle = NULL, *object_handle;
	struct emsmdbp_object			*parent_object = NULL, *object;
	struct FastTransferSourceCopyTo_req	 *request;
	struct FastTransferSourceCopyTo_repl	 *response;
	uint32_t				parent_handle_id, contextID, i;
	void					*data;
	uint64_t				objectID;
	uint8_t					objectType;
	struct SPropTagArray			needed_properties;
	void					*data_pointers;
	DATA_BLOB				*ftbuffer;

	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] FastTransferSourceCopyTo (0x4d)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* Step 1. Retrieve object handle */
	parent_handle_id = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, parent_handle_id, &parent_object_handle);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", parent_handle_id, mapi_req->handle_idx));
		goto end;
	}

	/* Step 2. Check whether the parent object supports fetching properties */
	mapi_handles_get_private_data(parent_object_handle, &data);
	parent_object = (struct emsmdbp_object *) data;
	if (parent_object->type == EMSMDBP_OBJECT_FOLDER) {
                contextID = parent_object->object.folder->contextID;
		objectID = parent_object->object.folder->folderID;
		objectType = MAPISTORE_FOLDER;
                goto end;
	}
	else if (parent_object->type == EMSMDBP_OBJECT_MESSAGE) {
                contextID = parent_object->object.message->contextID;
                objectID = parent_object->object.message->messageID;
		objectType = MAPISTORE_MESSAGE;
	}
	else if (parent_object->type == EMSMDBP_OBJECT_ATTACHMENT) {
                contextID = parent_object->object.attachment->contextID;
                objectID = parent_object->object.attachment->attachmentID;
		objectType = MAPISTORE_ATTACHMENT; // useless with poc
        }
        else {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
                goto end;
	}

	request = &mapi_req->u.mapi_FastTransferSourceCopyTo;
	response = &mapi_repl->u.mapi_FastTransferSourceCopyTo;

	if (request->Level > 0) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		DEBUG(5, ("  no support for levels > 0\n"));
                goto end;
	}

	emsmdbp_object_get_available_properties(emsmdbp_ctx, parent_object, &needed_properties);
	if (needed_properties.cValues > 0) {
		for (i = 0; i < request->PropertyTags.cValues; i++) {
			SPropTagArray_delete(mem_ctx, &needed_properties, request->PropertyTags.aulPropTag[i]);
		}

		data_pointers = emsmdbp_object_get_properties(emsmdbp_ctx, parent_object, &needed_properties, &retvals);
		if (data_pointers == NULL) {
			mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
			DEBUG(5, ("  unexpected error\n"));
			goto end;
		}

		object = emsmdbp_object_ftcontext_init(mem_ctx, emsmdbp_ctx, parent_object);
		if (object == NULL) {
			mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
			DEBUG(5, ("  context object not created\n"));
			goto end;
		}

		ftbuffer = &object->object.ftcontext->stream.buffer;
		emsmdbp_fill_ftbuffer_blob(object->object.ftcontext, emsmdbp_ctx, ftbuffer, &needed_properties, data_pointers, retvals);

		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, parent_handle_id, &object_handle);
		mapi_handles_set_private_data(object_handle, object);
                mapi_repl->handle_idx = request->handle_idx;
		handles[mapi_repl->handle_idx] = object_handle->handle;
        }

end:
	*size += libmapiserver_RopFastTransferSourceCopyTo_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopFastTransferSourceGetBuffer (0x4e) Rop. This operation downloads the next portion of a FastTransfer stream that is produced by a previously configured download operation.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopFastTransferSourceGetBuffer(TALLOC_CTX *mem_ctx,
								struct emsmdbp_context *emsmdbp_ctx,
								struct EcDoRpc_MAPI_REQ *mapi_req,
								struct EcDoRpc_MAPI_REPL *mapi_repl,
								uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS				retval;
	uint32_t				handle_id;
	struct mapi_handles			*object_handle = NULL;
	struct emsmdbp_object			*object = NULL;
	struct FastTransferSourceGetBuffer_req	 *request;
	struct FastTransferSourceGetBuffer_repl	 *response;
	uint16_t				buffer_size;
	void					*data;

	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] FastTransferSourceGetBuffer (0x4e)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* Step 1. Retrieve object handle */
	handle_id = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle_id, &object_handle);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle_id, mapi_req->handle_idx));
		goto end;
	}

	/* Step 2. Check whether the parent object supports fetching properties */
	mapi_handles_get_private_data(object_handle, &data);
	object = (struct emsmdbp_object *) data;
	if (!object) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		DEBUG(5, ("  object not found\n"));
                goto end;
	}

	if (object->type != EMSMDBP_OBJECT_FTCONTEXT) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		DEBUG(5, ("  object type %d not supported\n", object->type));
                goto end;
	}

	/* Step 3. Perform the read operation */
	request = &mapi_req->u.mapi_FastTransferSourceGetBuffer;
	response = &mapi_repl->u.mapi_FastTransferSourceGetBuffer;

	buffer_size = request->BufferSize;
	if (buffer_size == 0xBABE) {
		buffer_size = request->MaximumBufferSize.MaximumBufferSize;
	}

	if (object->object.ftcontext->stream.position == 0) {
		object->object.ftcontext->steps = 0;
		object->object.ftcontext->total_steps = (object->object.ftcontext->stream.buffer.length / buffer_size) + 1;
	}
	object->object.ftcontext->steps += 1;
	response->TransferBuffer = emsmdbp_stream_read_buffer(&object->object.ftcontext->stream, buffer_size);

	/* Step 4. Finalize the response variables */
	response->TotalStepCount = object->object.ftcontext->total_steps;
	response->TransferBufferSize = response->TransferBuffer.length;
	if (response->TransferBuffer.length < buffer_size) {
		response->TransferStatus = TransferStatus_Done;
		response->InProgressCount = response->TotalStepCount;
	}
	else {
		response->TransferStatus = TransferStatus_Partial;
		response->InProgressCount = object->object.ftcontext->steps;
	}
	response->Reserved = 0;

end:
	*size += libmapiserver_RopFastTransferSourceGetBuffer_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncConfigure (0x70) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncConfigure(TALLOC_CTX *mem_ctx,
						  struct emsmdbp_context *emsmdbp_ctx,
						  struct EcDoRpc_MAPI_REQ *mapi_req,
						  struct EcDoRpc_MAPI_REPL *mapi_repl,
						  uint32_t *handles, uint16_t *size)
{
	uint32_t		folder_handle;
	struct mapi_handles	*folder_rec;
	struct mapi_handles	*synccontext_rec;
	struct emsmdbp_object	*folder_object;
	struct emsmdbp_object	*synccontext_object;
	enum MAPISTATUS		retval;
	void			*data = NULL;

	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] RopSyncConfigure (0x70) - stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopSyncConfigure_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncImportHierarchyChange (0x73) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncImportHierarchyChange(TALLOC_CTX *mem_ctx,
							      struct emsmdbp_context *emsmdbp_ctx,
							      struct EcDoRpc_MAPI_REQ *mapi_req,
							      struct EcDoRpc_MAPI_REPL *mapi_repl,
							      uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] RopSyncImportHierarchyChange (0x73) - stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopSyncImportHierarchyChange_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncUploadStateStreamBegin (0x75) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncUploadStateStreamBegin(TALLOC_CTX *mem_ctx,
							       struct emsmdbp_context *emsmdbp_ctx,
							       struct EcDoRpc_MAPI_REQ *mapi_req,
							       struct EcDoRpc_MAPI_REPL *mapi_repl,
							       uint32_t *handles, uint16_t *size)
{
 	uint32_t		synccontext_handle;
	struct mapi_handles	*synccontext_rec;
	struct emsmdbp_object	*synccontext_object;
	enum MAPISTATUS		retval;
	void			*data = NULL;

	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] RopSyncUploadStateStreamBegin (0x75) - stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopSyncUploadStateStreamBegin_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncUploadStateStreamEnd (0x77) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncUploadStateStreamEnd(TALLOC_CTX *mem_ctx,
							     struct emsmdbp_context *emsmdbp_ctx,
							     struct EcDoRpc_MAPI_REQ *mapi_req,
							     struct EcDoRpc_MAPI_REPL *mapi_repl,
							     uint32_t *handles, uint16_t *size)
{
 	uint32_t		synccontext_handle;
	struct mapi_handles	*synccontext_rec;
	struct emsmdbp_object	*synccontext_object;
	enum MAPISTATUS		retval;
	void			*data = NULL;

	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] RopSyncUploadStateStreamEnd (0x77) - stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopSyncUploadStateStreamEnd_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncOpenCollector (0x7e) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncOpenCollector(TALLOC_CTX *mem_ctx,
						      struct emsmdbp_context *emsmdbp_ctx,
						      struct EcDoRpc_MAPI_REQ *mapi_req,
						      struct EcDoRpc_MAPI_REPL *mapi_repl,
						      uint32_t *handles, uint16_t *size)
{
	uint32_t		folder_handle;
	struct mapi_handles	*folder_rec;
	struct mapi_handles	*synccontext_rec;
	struct emsmdbp_object	*folder_object;
	struct emsmdbp_object	*synccontext_object;
	enum MAPISTATUS		retval;
	void			*data = NULL;

	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] RopSyncOpenCollector (0x7e)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopSyncOpenCollector_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopGetLocalReplicaIds (0x7f) Rop. This operation reserves a range of IDs to be used by a local replica.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetLocalReplicaIds(TALLOC_CTX *mem_ctx,
                                                       struct emsmdbp_context *emsmdbp_ctx,
                                                       struct EcDoRpc_MAPI_REQ *mapi_req,
                                                       struct EcDoRpc_MAPI_REPL *mapi_repl,
                                                       uint32_t *handles, uint16_t *size)
{
	char			*recipient;
        
	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] RopGetLocalReplicaIds (0x7f) - stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopGetLocalReplicaIds_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncGetTransferState (0x82) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncGetTransferState(TALLOC_CTX *mem_ctx,
							 struct emsmdbp_context *emsmdbp_ctx,
							 struct EcDoRpc_MAPI_REQ *mapi_req,
							 struct EcDoRpc_MAPI_REPL *mapi_repl,
							 uint32_t *handles, uint16_t *size)
{
	uint32_t		synccontext_handle;
	struct mapi_handles	*synccontext_rec;
	struct mapi_handles	*ftcontext_rec;
	struct emsmdbp_object	*synccontext_object;
	struct emsmdbp_object	*ftcontext_object;
	enum MAPISTATUS		retval;
	void			*data = NULL;

	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] RopSyncGetTransferState (0x82)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopSyncGetTransferState_size(mapi_repl);

	return MAPI_E_SUCCESS;
}
