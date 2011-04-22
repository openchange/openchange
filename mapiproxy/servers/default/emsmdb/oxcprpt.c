/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009

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
   \file oxcprpt.c

   \brief Property and Stream Object routines and Rops
 */

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"

/**
   \details EcDoRpc GetPropertiesSpecific (0x07) Rop. This operation
   retrieves from properties data from specified object.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetPropertiesSpecific
   EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the GetPropertiesSpecific
   EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetPropertiesSpecific(TALLOC_CTX *mem_ctx,
							  struct emsmdbp_context *emsmdbp_ctx,
							  struct EcDoRpc_MAPI_REQ *mapi_req,
							  struct EcDoRpc_MAPI_REPL *mapi_repl,
							  uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	struct GetProps_req	*request;
	struct GetProps_repl	*response;
	uint32_t		handle;
	struct mapi_handles	*rec = NULL;
	void			*private_data = NULL;
	struct emsmdbp_object	*object;
        struct SPropTagArray    *properties;
        void                    **data_pointers;
        enum MAPISTATUS         *retvals = NULL;
        bool                    *untyped_status;
        uint16_t                i, propType;
	uint32_t		stream_size;
	struct emsmdbp_stream_data *stream_data;

	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] GetPropertiesSpecific (0x07)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = &mapi_req->u.mapi_GetProps;
	response = &mapi_repl->u.mapi_GetProps;

	/* Initialize GetProps response blob */
	response->prop_data.length = 0;
	response->prop_data.data = NULL;

	/* Fill EcDoRpc_MAPI_REPL reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_NOT_FOUND;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);
        object = private_data;

        properties = talloc_zero(NULL, struct SPropTagArray);
        properties->cValues = request->prop_count;
        properties->aulPropTag = talloc_array(properties, enum MAPITAGS, request->prop_count);
        untyped_status = talloc_array(NULL, bool, request->prop_count);

        for (i = 0; i < request->prop_count; i++) {
                properties->aulPropTag[i] = request->properties[i];
                if ((request->properties[i] & 0xffff) == 0) {
                        properties->aulPropTag[i] |= get_property_type(request->properties[i] >> 16);
                        untyped_status[i] = true;
                }
                else {
                        untyped_status[i] = false;
                }
        }

        data_pointers = emsmdbp_object_get_properties(emsmdbp_ctx, object, properties, &retvals);
        if (data_pointers) {
		for (i = 0; i < request->prop_count; i++) {
			if (retvals[i] == MAPI_E_SUCCESS) {
				propType = properties->aulPropTag[i] & 0xffff;
				if (propType == PT_STRING8) {
					stream_size = strlen((const char *) stream_data) + 1;
				}
				else if (propType == PT_UNICODE) {
					stream_size = strlen_m_ext((char *) data_pointers[i], CH_UTF8, CH_UTF16LE) * 2 + 2;
				}
				else if (propType == PT_BINARY) {
					stream_size = ((struct Binary_r *) data_pointers[i])->cb;
				}
				else {
					stream_size = 0;
				}
				if (stream_size > 8192) {
					DEBUG(5, ("%s: attaching stream data for property %.8x\n", __FUNCTION__, properties->aulPropTag[i]));
					stream_data = emsmdbp_stream_data_from_value(object, properties->aulPropTag[i], data_pointers[i]);
					if (stream_data) {
						DLIST_ADD(object->stream_data, stream_data);
					}
					/* This will trigger the opening of a property stream from the client. */
					retvals[i] = MAPI_E_NOT_ENOUGH_MEMORY;
				}
			}
		}
                mapi_repl->error_code = MAPI_E_SUCCESS;
		emsmdbp_fill_row_blob(mem_ctx,
				      emsmdbp_ctx,
				      &response->layout,
				      &response->prop_data,
				      properties,
				      data_pointers,
				      retvals,
				      untyped_status);
                talloc_free(data_pointers);
        }
        talloc_free(properties);
        talloc_free(retvals);

 end:
	*size += libmapiserver_RopGetPropertiesSpecific_size(mapi_req, mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc SetProperties (0x0a) Rop. This operation sets
   property values for an object.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SetProperties EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the SetProperties EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSetProperties(TALLOC_CTX *mem_ctx,
						  struct emsmdbp_context *emsmdbp_ctx,
						  struct EcDoRpc_MAPI_REQ *mapi_req,
						  struct EcDoRpc_MAPI_REPL *mapi_repl,
						  uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	uint32_t		handle;
	struct mapi_handles	*rec = NULL;
	void			*private_data = NULL;
	bool			mapistore = false;
	struct emsmdbp_object	*object;
	uint64_t		fmid;
	uint32_t		contextID;
	uint16_t		i;
	struct SRow		aRow;

	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] SetProperties (0x0a)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	
	mapi_repl->u.mapi_SetProps.PropertyProblemCount = 0;
	mapi_repl->u.mapi_SetProps.PropertyProblem = NULL;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);
	object = (struct emsmdbp_object *)private_data;
	if (!object) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	mapistore = emsmdbp_is_mapistore(private_data);
	switch (mapistore) {
	case false:
		DEBUG(0, ("SetProps on openchangedb not implemented yet\n"));
		break;
	case true:
		aRow.cValues = mapi_req->u.mapi_SetProps.values.cValues;
		aRow.lpProps = talloc_array(mem_ctx, struct SPropValue, aRow.cValues + 2);
		for (i = 0; i < mapi_req->u.mapi_SetProps.values.cValues; i++) {
			cast_SPropValue(aRow.lpProps, &(mapi_req->u.mapi_SetProps.values.lpProps[i]),
					&(aRow.lpProps[i]));
		}

                if (object->type == EMSMDBP_OBJECT_MESSAGE) {
                        contextID = object->object.message->contextID;
                }
                else if (object->type == EMSMDBP_OBJECT_FOLDER) {
                        contextID = object->object.folder->contextID;
                }
                else if (object->type == EMSMDBP_OBJECT_ATTACHMENT) {
                        contextID = object->object.attachment->contextID;
                }
                else {
                        DEBUG(5, ("  object type %d not implemented\n", object->type));
                        mapi_repl->error_code = MAPI_E_NO_SUPPORT;
                        goto end;
                }

                if (object->poc_api) {
                        mapistore_pocop_set_properties(emsmdbp_ctx->mstore_ctx, contextID,
                                                       object->poc_backend_object,
                                                       &aRow);
                }
                else {
                        if (object->type == EMSMDBP_OBJECT_MESSAGE) {
                                fmid = object->object.message->messageID;
                                mapistore_setprops(emsmdbp_ctx->mstore_ctx, contextID, fmid, 
                                                   MAPISTORE_MESSAGE, &aRow);
                        }
                        else if (object->type == EMSMDBP_OBJECT_FOLDER) {
                                fmid = object->object.folder->folderID;
                                mapistore_setprops(emsmdbp_ctx->mstore_ctx, contextID, fmid, 
                                                   MAPISTORE_FOLDER, &aRow);
                        }
                        else {
                                DEBUG(5, ("  object type %d not implemented\n", object->type));
                        }
                }
		break;
	}
	

end:
	*size += libmapiserver_RopSetProperties_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc DeleteProperties (0x0b) Rop. This operation
   deletes property values for an object.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the DeleteProperties EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the DeleteProperties EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopDeleteProperties(TALLOC_CTX *mem_ctx,
						     struct emsmdbp_context *emsmdbp_ctx,
						     struct EcDoRpc_MAPI_REQ *mapi_req,
						     struct EcDoRpc_MAPI_REPL *mapi_repl,
						     uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] DeleteProperties (0x0b) -- stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	
	mapi_repl->u.mapi_DeleteProps.PropertyProblemCount = 0;
	mapi_repl->u.mapi_DeleteProps.PropertyProblem = NULL;

	*size += libmapiserver_RopDeleteProperties_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc OpenStream (0x2b) Rop. This operation opens a
   property for streaming access.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenStream EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenStream EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopOpenStream(TALLOC_CTX *mem_ctx,
					       struct emsmdbp_context *emsmdbp_ctx,
					       struct EcDoRpc_MAPI_REQ *mapi_req,
					       struct EcDoRpc_MAPI_REPL *mapi_repl,
					       uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	struct mapi_handles		*parent = NULL;
	struct mapi_handles		*rec = NULL;
	struct emsmdbp_object		*object = NULL;
	struct emsmdbp_object		*parent_object = NULL;
	struct OpenStream_req		*request;
	struct OpenStream_repl		*response;
	uint32_t			handle, contextID;
	uint64_t			objectID;
	uint8_t				objectType;
	void				*data;
        struct SPropTagArray		properties;
	void				**data_pointers;
	enum MAPISTATUS			*retvals;
	struct emsmdbp_stream_data	*stream_data;

	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] OpenStream (0x2b)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
        mapi_repl->handle_idx = mapi_req->u.mapi_OpenStream.handle_idx;
	mapi_repl->u.mapi_OpenStream.StreamSize = 0;

	/* Step 1. Retrieve parent handle in the hierarchy */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	mapi_handles_get_private_data(parent, &data);
	parent_object = (struct emsmdbp_object *) data;
	if (parent_object->type == EMSMDBP_OBJECT_FOLDER) {
                contextID = parent_object->object.folder->contextID;
		objectID = parent_object->object.folder->folderID;
		objectType = MAPISTORE_FOLDER;
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

	request = &mapi_req->u.mapi_OpenStream;
	response = &mapi_repl->u.mapi_OpenStream;

	/* TODO: implementation status:
	   - OpenStream_ReadOnly (supported)
	   - OpenStream_ReadWrite
	   - OpenStream_Create (supported)
	   - OpenStream_BestAccess
	*/

	object = emsmdbp_object_stream_init((TALLOC_CTX *)rec, emsmdbp_ctx, parent_object);
	object->object.stream->objectID = objectID;
	object->object.stream->objectType = objectType;
	object->object.stream->flags = request->OpenModeFlags;
	object->object.stream->property = request->PropertyTag;

	if (object->object.stream->flags == OpenStream_ReadOnly || object->object.stream->flags == OpenStream_ReadWrite) {
		object->object.stream->buffer_pos = 0;

		stream_data = emsmdbp_object_get_stream_data(parent_object, object->object.stream->property);
		if (stream_data) {
			talloc_steal(object, stream_data);
			DLIST_REMOVE(parent_object->stream_data, stream_data);
		}
		else {
			properties.cValues = 1;
			properties.aulPropTag = &request->PropertyTag;

			data_pointers = emsmdbp_object_get_properties(emsmdbp_ctx, parent_object, &properties, &retvals);
			if (data_pointers == NULL) {
				mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
				talloc_free(object);
				goto end;
			}
			if (retvals[0] != MAPI_E_SUCCESS) {
				mapi_repl->error_code = retvals[0];
				talloc_free(object);
				talloc_free(data_pointers);
				talloc_free(retvals);
				goto end;
			}
			stream_data = emsmdbp_stream_data_from_value(object, request->PropertyTag, data_pointers[0]);
		}

		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
		handles[mapi_repl->handle_idx] = rec->handle;
		mapi_handles_set_private_data(rec, object);

		object->object.stream->buffer = stream_data->data;
		mapi_repl->u.mapi_OpenStream.StreamSize = object->object.stream->buffer.length;
	}
	else { /* OpenStream_Create */
		object->object.stream->buffer.data = talloc_zero(object->object.stream, uint8_t);
		object->object.stream->buffer.length = 0;
	}

end:
	*size += libmapiserver_RopOpenStream_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc ReadStream (0x2c) Rop. This operation reads bytes
   from a stream.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the ReadStream EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the ReadStream EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopReadStream(TALLOC_CTX *mem_ctx,
					       struct emsmdbp_context *emsmdbp_ctx,
					       struct EcDoRpc_MAPI_REQ *mapi_req,
					       struct EcDoRpc_MAPI_REPL *mapi_repl,
					       uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	struct mapi_handles		*rec = NULL;
	void				*private_data;
	struct emsmdbp_object		*object;
	struct emsmdbp_object_stream	*stream;
	uint32_t			handle, buffer_size;

	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] ReadStream (0x2c)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->u.mapi_ReadStream.data.length = 0;
	mapi_repl->u.mapi_ReadStream.data.data = NULL;

	/* Step 1. Retrieve parent handle in the hierarchy */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	/* Step 2. Retrieve the stream object */
	retval = mapi_handles_get_private_data(rec, &private_data);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_STREAM) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  invalid object\n"));
		goto end;
	}

	/* Step 3. Return the data and update the position in the stream */
	buffer_size = mapi_req->u.mapi_ReadStream.ByteCount;
	/* careful here, let's switch to idiot mode */
	if (buffer_size == 0xBABE) {
		buffer_size = mapi_req->u.mapi_ReadStream.MaximumByteCount.value;
	}
	stream = object->object.stream;
	if (buffer_size + stream->buffer_pos > stream->buffer.length) {
		buffer_size = stream->buffer.length - stream->buffer_pos;
	}
	mapi_repl->u.mapi_ReadStream.data.data = stream->buffer.data + stream->buffer_pos;
	mapi_repl->u.mapi_ReadStream.data.length = buffer_size;
	stream->buffer_pos += buffer_size;

end:
	*size += libmapiserver_RopReadStream_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc WriteStream (0x2d) Rop. This operation writes bytes
   to a stream.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the WriteStream EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the WriteStream EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopWriteStream(TALLOC_CTX *mem_ctx,
						struct emsmdbp_context *emsmdbp_ctx,
						struct EcDoRpc_MAPI_REQ *mapi_req,
						struct EcDoRpc_MAPI_REPL *mapi_repl,
						uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	struct mapi_handles		*parent = NULL;
	void				*private_data;
	struct emsmdbp_object		*object = NULL;
	struct emsmdbp_object_stream	*stream;
	uint32_t			handle, buffer_size;
	struct WriteStream_req		*request;

	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] WriteStream (0x2d)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->u.mapi_WriteStream.WrittenSize = 0;

	/* Step 1. Retrieve parent handle in the hierarchy */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &private_data);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_STREAM) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  invalid object\n"));
		goto end;
	}

	request = &mapi_req->u.mapi_WriteStream;
	if (request->data.length > 0) {
		stream = object->object.stream;
		buffer_size = stream->buffer_pos + request->data.length;
		if (buffer_size > stream->buffer.length) {
			stream->buffer.data = talloc_realloc(stream, stream->buffer.data, uint8_t, buffer_size);
		}
		memcpy(request->data.data, stream->buffer.data, request->data.length);
		stream->buffer_pos += request->data.length;
		mapi_repl->u.mapi_WriteStream.WrittenSize = request->data.length;
	}

end:
	*size += libmapiserver_RopWriteStream_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc GetStreamSize (0x5e) Rop. This operation returns the
   number of bytes in a stream.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the WriteStream EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the WriteStream EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetStreamSize(TALLOC_CTX *mem_ctx,
						  struct emsmdbp_context *emsmdbp_ctx,
						  struct EcDoRpc_MAPI_REQ *mapi_req,
						  struct EcDoRpc_MAPI_REPL *mapi_repl,
						  uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	struct mapi_handles		*parent = NULL;
	void				*private_data;
	struct emsmdbp_object		*object = NULL;
	uint32_t			handle;

	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] GetStreamSize (0x5e)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* Step 1. Retrieve parent handle in the hierarchy */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &private_data);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_STREAM) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  invalid object\n"));
		goto end;
	}

	mapi_repl->u.mapi_GetStreamSize.StreamSize = object->object.stream->buffer.length;

end:
	*size += libmapiserver_RopGetStreamSize_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc SeekStream (0x2e) Rop. This operation positions the cursor
   in the stream.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the WriteStream EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the WriteStream EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSeekStream(TALLOC_CTX *mem_ctx,
                                               struct emsmdbp_context *emsmdbp_ctx,
                                               struct EcDoRpc_MAPI_REQ *mapi_req,
                                               struct EcDoRpc_MAPI_REPL *mapi_repl,
                                               uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	struct mapi_handles		*parent = NULL;
	void				*private_data;
	struct emsmdbp_object		*object = NULL;
	uint32_t			handle, new_position;

	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] SeekStream (0x2e)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* Step 1. Retrieve parent handle in the hierarchy */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &private_data);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_STREAM) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  invalid object\n"));
		goto end;
	}

	switch (mapi_req->u.mapi_SeekStream.Origin) {
	case 0: /* beginning */
		new_position = 0;
		break;
	case 1: /* current */
		new_position = object->object.stream->buffer_pos;
		break;
	case 2: /* end */
		new_position = object->object.stream->buffer.length;
		break;
	default:
		mapi_repl->error_code = MAPI_E_INVALID_PARAMETER;
		goto end;
	}

	new_position += mapi_req->u.mapi_SeekStream.Offset;
	if (new_position < object->object.stream->buffer.length + 1) {
		object->object.stream->buffer_pos = new_position;
		mapi_repl->u.mapi_SeekStream.NewPosition = new_position;
	}
	else {
		mapi_repl->error_code = MAPI_E_DISK_ERROR;
	}

end:
	*size += libmapiserver_RopGetStreamSize_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc SetStreamSize (0x2f) Rop. This operation
   copy messages from one folder to another.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the DeleteProperties EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the DeleteProperties EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSetStreamSize(TALLOC_CTX *mem_ctx,
                                           struct emsmdbp_context *emsmdbp_ctx,
                                           struct EcDoRpc_MAPI_REQ *mapi_req,
                                           struct EcDoRpc_MAPI_REPL *mapi_repl,
                                           uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] SetStreamSize (0x2f) -- stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	
	*size += libmapiserver_RopSetStreamSize_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc GetPropertyIdsFromNames (0x56) Rop. This operation
   gets property IDs for specified property names.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetPropertyIdsFromNames
   EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the GetPropertyIdsFromNames
   EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
*/
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetPropertyIdsFromNames(TALLOC_CTX *mem_ctx,
							    struct emsmdbp_context *emsmdbp_ctx,
							    struct EcDoRpc_MAPI_REQ *mapi_req,
							    struct EcDoRpc_MAPI_REPL *mapi_repl,
							    uint32_t *handles, uint16_t *size)
{
	int		i;
	struct GUID *lpguid;

	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] GetPropertyIdsFromNames (0x56)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->u.mapi_GetIDsFromNames.count = mapi_req->u.mapi_GetIDsFromNames.count;
	mapi_repl->u.mapi_GetIDsFromNames.propID = talloc_array(mem_ctx, uint16_t, 
								mapi_req->u.mapi_GetIDsFromNames.count);

	for (i = 0; i < mapi_req->u.mapi_GetIDsFromNames.count; i++) {
		if (mapistore_namedprops_get_mapped_id(emsmdbp_ctx->mstore_ctx->nprops_ctx, 
						       mapi_req->u.mapi_GetIDsFromNames.nameid[i],
						       &mapi_repl->u.mapi_GetIDsFromNames.propID[i])
		    != MAPISTORE_SUCCESS) {
			lpguid = &mapi_req->u.mapi_GetIDsFromNames.nameid[i].lpguid;
			DEBUG(5, ("  no mapping for property %.8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x:",
				  lpguid->time_low, lpguid->time_mid, lpguid->time_hi_and_version,
				  lpguid->clock_seq[0], lpguid->clock_seq[1],
				  lpguid->node[0], lpguid->node[1],
				  lpguid->node[2], lpguid->node[3],
				  lpguid->node[4], lpguid->node[5]));
				  
			if (mapi_req->u.mapi_GetIDsFromNames.nameid[i].ulKind == MNID_ID)
				DEBUG(5, ("%.4x\n", mapi_req->u.mapi_GetIDsFromNames.nameid[i].kind.lid));
			else if (mapi_req->u.mapi_GetIDsFromNames.nameid[i].ulKind == MNID_STRING)
				DEBUG(5, ("%s\n", mapi_req->u.mapi_GetIDsFromNames.nameid[i].kind.lpwstr.Name));
			else
				DEBUG(5, ("[invalid ulKind]"));

			mapi_repl->error_code = MAPI_W_ERRORS_RETURNED;
		}
	}

	if (mapi_repl->error_code == MAPI_W_ERRORS_RETURNED
	    && mapi_req->u.mapi_GetIDsFromNames.ulFlags == 0x02) {
		DEBUG(5, ("%s: property creation is not implemented\n", __FUNCTION__));
	}

	*size += libmapiserver_RopGetPropertyIdsFromNames_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc DeletePropertiesNoReplicate (0x7a) Rop. deletes property
   values from an object without invoking replication.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetPropertyIdsFromNames
   EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the GetPropertyIdsFromNames
   EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
*/
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopDeletePropertiesNoReplicate(TALLOC_CTX *mem_ctx,
								struct emsmdbp_context *emsmdbp_ctx,
								struct EcDoRpc_MAPI_REQ *mapi_req,
								struct EcDoRpc_MAPI_REPL *mapi_repl,
								uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] DeletePropertiesNoReplicate (0x7a) -- stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	mapi_repl->u.mapi_DeletePropertiesNoReplicate.PropertyProblemCount = 0;
	mapi_repl->u.mapi_DeletePropertiesNoReplicate.PropertyProblem = NULL;

	*size += libmapiserver_RopDeletePropertiesNoReplicate_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc CopyTo (0x39) Rop. This operation
   copy messages from one folder to another.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the DeleteProperties EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the DeleteProperties EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopCopyTo(TALLOC_CTX *mem_ctx,
                                           struct emsmdbp_context *emsmdbp_ctx,
                                           struct EcDoRpc_MAPI_REQ *mapi_req,
                                           struct EcDoRpc_MAPI_REPL *mapi_repl,
                                           uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] CopyTo (0x39) -- stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	
	mapi_repl->u.mapi_CopyTo.PropertyProblemCount = 0;
	mapi_repl->u.mapi_CopyTo.PropertyProblem = NULL;

	*size += libmapiserver_RopCopyTo_size(mapi_repl);

	return MAPI_E_SUCCESS;
}
