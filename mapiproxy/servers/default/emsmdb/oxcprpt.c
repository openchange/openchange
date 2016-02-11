/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009-2014

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

#include "libmapi/libmapi.h"
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
	TALLOC_CTX		*local_mem_ctx;
	enum MAPISTATUS		retval;
	struct GetProps_req	*request;
	struct GetProps_repl	*response;
	uint32_t		handle;
	uint16_t		property_id, property_type;
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

	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] GetPropertiesSpecific (0x07)\n");

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
                mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);
        object = private_data;
	if (!object) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  object (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	if (!(object->type == EMSMDBP_OBJECT_MAILBOX
	      || object->type == EMSMDBP_OBJECT_FOLDER
	      || object->type == EMSMDBP_OBJECT_MESSAGE
	      || object->type == EMSMDBP_OBJECT_ATTACHMENT)) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  GetProperties cannot occur on an object of type '%s' (%d)\n", emsmdbp_getstr_type(object), object->type);
		goto end;
	}

        local_mem_ctx = talloc_new(NULL);
        OPENCHANGE_RETVAL_IF(!local_mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

        properties = talloc_zero(local_mem_ctx, struct SPropTagArray);
        properties->cValues = request->prop_count;
        properties->aulPropTag = talloc_array(properties, enum MAPITAGS, request->prop_count);
        untyped_status = talloc_array(local_mem_ctx, bool, request->prop_count);

        for (i = 0; i < request->prop_count; i++) {
                properties->aulPropTag[i] = request->properties[i];
                if ((request->properties[i] & 0xffff) == 0) {
			property_id = request->properties[i] >> 16;
			if (property_id < 0x8000) {
				property_type = get_property_type(property_id);
			}
			else {
				property_type = 0;
				mapistore_namedprops_get_nameid_type(emsmdbp_ctx->mstore_ctx->nprops_ctx, property_id, &property_type);
			}
			if (property_type) {
				properties->aulPropTag[i] |= property_type;
				untyped_status[i] = true;
			}
			else {
				properties->aulPropTag[i] |= PT_ERROR; /* fail with a MAPI_E_NOT_FOUND */
				untyped_status[i] = false;
			}
                }
                else {
                        untyped_status[i] = false;
                }
        }

        data_pointers = emsmdbp_object_get_properties(local_mem_ctx, emsmdbp_ctx, object, properties, &retvals);
        if (data_pointers) {
		for (i = 0; i < request->prop_count; i++) {
			if (retvals[i] == MAPI_E_SUCCESS) {
				propType = properties->aulPropTag[i] & 0xffff;
				if (propType == PT_STRING8) {
					stream_size = strlen((const char *) data_pointers[i]) + 1;
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
					OC_DEBUG(5, "attaching stream data for property %.8x\n", properties->aulPropTag[i]);
					stream_data = emsmdbp_stream_data_from_value(object, properties->aulPropTag[i], data_pointers[i], false);
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
	}
	talloc_free(local_mem_ctx);

 end:
	*size += libmapiserver_RopGetPropertiesSpecific_size(mapi_req, mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc GetPropertiesAll (0x08) Rop. This operation gets
   all the property values for an object.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetPropertiesAll EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the GetPropertiesAll EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetPropertiesAll(TALLOC_CTX *mem_ctx,
						     struct emsmdbp_context *emsmdbp_ctx,
						     struct EcDoRpc_MAPI_REQ *mapi_req,
						     struct EcDoRpc_MAPI_REPL *mapi_repl,
						     uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	enum MAPISTATUS			*retvals = NULL;
	struct GetPropsAll_repl		*response;
	uint32_t			handle;
	struct mapi_handles		*rec = NULL;
	void				*private_data = NULL;
	struct emsmdbp_object		*object;
	struct SPropTagArray		*SPropTagArray;
	struct SPropValue		tmp_value;
	void				**data_pointers;
	int				i;

	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] GetPropertiesAll (0x08)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	response = &mapi_repl->u.mapi_GetPropsAll;

	/* Initialize GetPropsAll response */
	response->properties.cValues = 0;
	response->properties.lpProps = NULL;

	/* Fill EcDoRpc_MAPI_REPL reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	/* mapi_repl->error_code = MAPI_E_NOT_FOUND; */
	mapi_repl->error_code = MAPI_E_SUCCESS;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);
	object = private_data;
	if (!object) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  object (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = emsmdbp_object_get_available_properties(mem_ctx, emsmdbp_ctx, object, &SPropTagArray);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  object (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	data_pointers = emsmdbp_object_get_properties(mem_ctx, emsmdbp_ctx, object, SPropTagArray, &retvals);
	if (data_pointers) {		
		response->properties.lpProps = talloc_array(mem_ctx, struct mapi_SPropValue, SPropTagArray->cValues);
		response->properties.cValues = 0;
		for (i = 0; i < SPropTagArray->cValues; i++) {
			if (retvals[i] == MAPI_E_SUCCESS) {
				tmp_value.ulPropTag = SPropTagArray->aulPropTag[i];
				if (set_SPropValue(&tmp_value, data_pointers[i])) {
					cast_mapi_SPropValue(mem_ctx, response->properties.lpProps + response->properties.cValues, &tmp_value);
					response->properties.cValues++;
				}
			}
		}
	}

end:
	*size += libmapiserver_RopGetPropertiesAll_size(mapi_repl);
	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc GetPropertiesList (0x9) Rop. This operation
   retrieves the list of MAPI tags for an object.

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
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetPropertiesList(TALLOC_CTX *mem_ctx,
						      struct emsmdbp_context *emsmdbp_ctx,
						      struct EcDoRpc_MAPI_REQ *mapi_req,
						      struct EcDoRpc_MAPI_REPL *mapi_repl,
						      uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	struct GetPropList_repl	*response;
	uint32_t		handle;
	struct mapi_handles	*rec = NULL;
	void			*private_data = NULL;
	struct emsmdbp_object	*object;
	struct SPropTagArray	*SPropTagArray;
	
	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] GetPropertiesList (0x9)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	response = &mapi_repl->u.mapi_GetPropList;

	/* Initialize GetPropList response */
	response->count = 0;
	response->tags = NULL;

	/* Fill EcDoRpc_MAPI_REPL reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
                mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);
	object = private_data;
	if (!object) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  object (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = emsmdbp_object_get_available_properties(mem_ctx, emsmdbp_ctx, object, &SPropTagArray);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  object (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	response->count = SPropTagArray->cValues;
	response->tags = SPropTagArray->aulPropTag;

end:
	*size += libmapiserver_RopGetPropertiesList_size(mapi_repl);
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
	struct emsmdbp_object	*object;
	uint16_t		i;
	struct SRow		aRow;

	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] SetProperties (0x0a)\n");

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
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);
	object = (struct emsmdbp_object *)private_data;
	if (!object) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	if (object->type == EMSMDBP_OBJECT_MESSAGE && !object->object.message->read_write) {
		mapi_repl->error_code = MAPI_E_NO_ACCESS;
		goto end;
	}

	aRow.cValues = mapi_req->u.mapi_SetProps.values.cValues;
	aRow.lpProps = talloc_array(mem_ctx, struct SPropValue, aRow.cValues + 2);
	for (i = 0; i < mapi_req->u.mapi_SetProps.values.cValues; i++) {
		cast_SPropValue(aRow.lpProps, &(mapi_req->u.mapi_SetProps.values.lpProps[i]),
				&(aRow.lpProps[i]));
	}

	retval = emsmdbp_object_set_properties(emsmdbp_ctx, object, &aRow);
	if (retval) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
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
	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] DeleteProperties (0x0b) -- stub\n");

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
	uint32_t			handle;
	void				*data;
        struct SPropTagArray		properties;
	void				**data_pointers;
	enum MAPISTATUS			*retvals;
	struct emsmdbp_stream_data	*stream_data;
	enum OpenStream_OpenModeFlags	mode;

	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] OpenStream (0x2b)\n");

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
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	mapi_handles_get_private_data(parent, &data);
	parent_object = (struct emsmdbp_object *) data;
	if (!(parent_object->type == EMSMDBP_OBJECT_FOLDER
	      || parent_object->type == EMSMDBP_OBJECT_MESSAGE
	      || parent_object->type == EMSMDBP_OBJECT_ATTACHMENT)) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
                goto end;
        }

	request = &mapi_req->u.mapi_OpenStream;

	mode = request->OpenModeFlags;
	if (mode == OpenStream_BestAccess) {
		if (parent_object->type == EMSMDBP_OBJECT_MESSAGE) {
			if (parent_object->object.message->read_write) {
				mode = OpenStream_ReadWrite;
			}
			else {
				mode = OpenStream_ReadOnly;
			}
		}
		else {
			mode = OpenStream_ReadOnly;
		}
	}

	if (parent_object->type == EMSMDBP_OBJECT_MESSAGE && !parent_object->object.message->read_write && (mode == OpenStream_ReadWrite || mode == OpenStream_Create)) {
		mapi_repl->error_code = MAPI_E_NO_ACCESS;
                goto end;
	}

	if (request->PropertyTag == PidTagSecurityDescriptorAsXml) {
		/* exception; see oxcperm - 3.1.4.1 Retrieving Folder Permissions */
		mapi_repl->error_code = MAPI_E_NO_SUPPORT; /* ecNotImplemented = MAPI_E_NO_SUPPORT */
                goto end;
	}

	/* TODO: implementation status:
	   - OpenStream_ReadOnly (supported)
	   - OpenStream_ReadWrite (supported)
	   - OpenStream_Create (supported)
	   - OpenStream_BestAccess
	*/

	object = emsmdbp_object_stream_init(NULL, emsmdbp_ctx, parent_object);
	if (!object) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
                goto end;
	}
	object->object.stream->property = request->PropertyTag;
	object->object.stream->stream.position = 0;
	object->object.stream->stream.buffer.length = 0;

	if (mode == OpenStream_ReadOnly || mode == OpenStream_ReadWrite) {
		object->object.stream->read_write = (mode == OpenStream_ReadWrite);
		stream_data = emsmdbp_object_get_stream_data(parent_object, object->object.stream->property);
		if (stream_data) {
			object->object.stream->stream.buffer.length = stream_data->data.length;
			object->object.stream->stream.buffer.data = talloc_memdup(object->object.stream, stream_data->data.data, stream_data->data.length);
			DLIST_REMOVE(parent_object->stream_data, stream_data);
			talloc_free(stream_data);
		}
		else {
			properties.cValues = 1;
			properties.aulPropTag = &request->PropertyTag;

			data_pointers = emsmdbp_object_get_properties(mem_ctx, emsmdbp_ctx, parent_object, &properties, &retvals);
			if (data_pointers == NULL) {
				mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
				talloc_free(object);
				goto end;
			}
			if (retvals[0] == MAPI_E_SUCCESS) {
				stream_data = emsmdbp_stream_data_from_value(data_pointers, request->PropertyTag, data_pointers[0], object->object.stream->read_write);
				object->object.stream->stream.buffer = stream_data->data;
				(void) talloc_reference(object, stream_data);
				talloc_free(data_pointers);
				talloc_free(retvals);
			}
			else {
				mapi_repl->error_code = retvals[0];
				talloc_free(data_pointers);
				talloc_free(retvals);
				talloc_free(object);
				goto end;
			}
		}
	}
	else { /* OpenStream_Create */
		object->object.stream->read_write = true;
		object->object.stream->stream.buffer.data = talloc_zero(object->object.stream, uint8_t);
		object->object.stream->stream.buffer.length = 0;
	}

	mapi_repl->u.mapi_OpenStream.StreamSize = object->object.stream->stream.buffer.length;

	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
	(void) talloc_reference(rec, object);
	handles[mapi_repl->handle_idx] = rec->handle;
	mapi_handles_set_private_data(rec, object);
	talloc_free(object);

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
	uint32_t			handle, buffer_size;

	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] ReadStream (0x2c)\n");

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
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	/* Step 2. Retrieve the stream object */
	retval = mapi_handles_get_private_data(rec, &private_data);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_STREAM) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  invalid object\n");
		goto end;
	}

	/* Step 3. Return the data and update the position in the stream */
	buffer_size = mapi_req->u.mapi_ReadStream.ByteCount;
	/* careful here, let's switch to idiot mode */
	if (buffer_size == 0xBABE) {
		/* If MaximumByteCount (uint32_t) overflows sizeof (uint16_t) */
		if (mapi_req->u.mapi_ReadStream.MaximumByteCount.value > 0xFFF0) {
			buffer_size = 0xFFF0;
		} else {
			buffer_size = mapi_req->u.mapi_ReadStream.MaximumByteCount.value;
		}
	}

        mapi_repl->u.mapi_ReadStream.data = emsmdbp_stream_read_buffer(&object->object.stream->stream, buffer_size);

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
	struct emsmdbp_object		*object;
	uint32_t			handle;
	struct WriteStream_req		*request;

	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] WriteStream (0x2d)\n");

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
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &private_data);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_STREAM) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  invalid object\n");
		goto end;
	}

	if (!object->object.stream->read_write) {
		mapi_repl->error_code = MAPI_E_NO_ACCESS;
		goto end;
	}

	request = &mapi_req->u.mapi_WriteStream;
	if (request->data.length > 0) {
                emsmdbp_stream_write_buffer(object->object.stream, &object->object.stream->stream, request->data);
		mapi_repl->u.mapi_WriteStream.WrittenSize = request->data.length;
	}

	object->object.stream->needs_commit = true;
end:
	*size += libmapiserver_RopWriteStream_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc CommitStream (0x5d) Rop.

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
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopCommitStream(TALLOC_CTX *mem_ctx,
						 struct emsmdbp_context *emsmdbp_ctx,
						 struct EcDoRpc_MAPI_REQ *mapi_req,
						 struct EcDoRpc_MAPI_REPL *mapi_repl,
						 uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	struct mapi_handles		*rec = NULL;
	struct emsmdbp_object		*object = NULL;
	uint32_t			handle;
	void				*private_data;

	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] CommitStream (0x5d)\n");

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
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_STREAM) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  invalid object\n");
		goto end;
	}

	if (!object->object.stream->read_write) {
		mapi_repl->error_code = MAPI_E_NO_ACCESS;
		goto end;
	}

	emsmdbp_object_stream_commit(object);

end:
	*size += libmapiserver_RopCommitStream_size(mapi_repl);

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

	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] GetStreamSize (0x5e)\n");

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
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &private_data);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_STREAM) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  invalid object\n");
		goto end;
	}

	mapi_repl->u.mapi_GetStreamSize.StreamSize = object->object.stream->stream.buffer.length;

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

	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] SeekStream (0x2e)\n");

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
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &private_data);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_STREAM) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  invalid object\n");
		goto end;
	}

	switch (mapi_req->u.mapi_SeekStream.Origin) {
	case 0: /* beginning */
		new_position = 0;
		break;
	case 1: /* current */
		new_position = object->object.stream->stream.position;
		break;
	case 2: /* end */
		new_position = object->object.stream->stream.buffer.length;
		break;
	default:
		mapi_repl->error_code = MAPI_E_INVALID_PARAMETER;
		goto end;
	}

	new_position += mapi_req->u.mapi_SeekStream.Offset;
	if (new_position < object->object.stream->stream.buffer.length + 1) {
		object->object.stream->stream.position = new_position;
		mapi_repl->u.mapi_SeekStream.NewPosition = new_position;
	}
	else {
		mapi_repl->error_code = MAPI_E_DISK_ERROR;
	}

end:
	*size += libmapiserver_RopSeekStream_size(mapi_repl);

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
	enum MAPISTATUS			retval;
	struct mapi_handles		*parent = NULL;
	void				*private_data;
	struct emsmdbp_object		*object;
	uint32_t			handle;

	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] SetStreamSize (0x2f)\n");

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
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &private_data);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_STREAM) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  invalid object\n");
		goto end;
	}

end:
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
	enum mapistore_error	retval;
	int			i;
	int			ret;
	struct GUID		*lpguid;
	bool			has_transaction = false;
	uint16_t		mapped_id = 0;

	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] GetPropertyIdsFromNames (0x56)\n");

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
		ret = mapistore_namedprops_get_mapped_id(emsmdbp_ctx->mstore_ctx->nprops_ctx, 
							 mapi_req->u.mapi_GetIDsFromNames.nameid[i],
							 &mapi_repl->u.mapi_GetIDsFromNames.propID[i]);
		if (ret == MAPISTORE_SUCCESS)
			continue;
		// It doesn't exist, let's create it!
		if (mapi_req->u.mapi_GetIDsFromNames.ulFlags == GetIDsFromNames_GetOrCreate) {
			if (!has_transaction) {
				has_transaction = true;
				retval = mapistore_namedprops_transaction_start(emsmdbp_ctx->mstore_ctx->nprops_ctx);
				if (retval != MAPISTORE_SUCCESS) {
					return MAPI_E_UNABLE_TO_COMPLETE;
				}

				retval = mapistore_namedprops_next_unused_id(emsmdbp_ctx->mstore_ctx->nprops_ctx, &mapped_id);
				if (retval != MAPISTORE_SUCCESS) {
					OC_DEBUG(0, "ERROR: No remaining namedprops ID available\n");
					abort();
				}
			} else {
				mapped_id++;
			}
			mapistore_namedprops_create_id(emsmdbp_ctx->mstore_ctx->nprops_ctx,
						       mapi_req->u.mapi_GetIDsFromNames.nameid[i],
						       mapped_id);
			mapi_repl->u.mapi_GetIDsFromNames.propID[i] = mapped_id;
		} else {
			mapi_repl->u.mapi_GetIDsFromNames.propID[i] = 0x0000;
			lpguid = &mapi_req->u.mapi_GetIDsFromNames.nameid[i].lpguid;
			OC_DEBUG(5, "  no mapping for property %.8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x:",
				  lpguid->time_low, lpguid->time_mid, lpguid->time_hi_and_version,
				  lpguid->clock_seq[0], lpguid->clock_seq[1],
				  lpguid->node[0], lpguid->node[1],
				  lpguid->node[2], lpguid->node[3],
				  lpguid->node[4], lpguid->node[5]);

			if (mapi_req->u.mapi_GetIDsFromNames.nameid[i].ulKind == MNID_ID) {
				OC_DEBUG(5, "%.4x\n", mapi_req->u.mapi_GetIDsFromNames.nameid[i].kind.lid);
			} else if (mapi_req->u.mapi_GetIDsFromNames.nameid[i].ulKind == MNID_STRING) {
				OC_DEBUG(5, "%s\n", mapi_req->u.mapi_GetIDsFromNames.nameid[i].kind.lpwstr.Name);
			} else {
				OC_DEBUG(5, "[invalid ulKind]");
			}

			mapi_repl->error_code = MAPI_W_ERRORS_RETURNED;
		}
	}

	if (has_transaction) {
		enum mapistore_error err = mapistore_namedprops_transaction_commit(emsmdbp_ctx->mstore_ctx->nprops_ctx);
		if (err != MAPISTORE_SUCCESS) {
			return MAPI_E_UNABLE_TO_COMPLETE;
		}
	}

	*size += libmapiserver_RopGetPropertyIdsFromNames_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc GetNamesFromIDs (0x56) Rop. This operation
   gets property IDs for specified property names.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetNamesFromIDs
   EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the GetNamesFromIDs
   EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
*/
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetNamesFromIDs(TALLOC_CTX *mem_ctx,
						    struct emsmdbp_context *emsmdbp_ctx,
						    struct EcDoRpc_MAPI_REQ *mapi_req,
						    struct EcDoRpc_MAPI_REPL *mapi_repl,
						    uint32_t *handles, uint16_t *size)
{
	uint16_t			i;
	struct GetNamesFromIDs_req	*request;
	struct GetNamesFromIDs_repl	*response;
	struct MAPINAMEID		*nameid;

	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] GetNamesFromIDs (0x55)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	request = &mapi_req->u.mapi_GetNamesFromIDs;
	response = &mapi_repl->u.mapi_GetNamesFromIDs;

	response->nameid = talloc_array(mem_ctx, struct MAPINAMEID, request->PropertyIdCount);
	response->count = request->PropertyIdCount;
	for (i = 0; i < request->PropertyIdCount; i++) {
		if (request->PropertyIds[i] < 0x8000) {
			response->nameid[i].ulKind = MNID_ID;
			GUID_from_string(PS_MAPI, &response->nameid[i].lpguid);
			response->nameid[i].kind.lid = (uint32_t) request->PropertyIds[i] << 16 | get_property_type(request->PropertyIds[i]);
		}
		else if (mapistore_namedprops_get_nameid(emsmdbp_ctx->mstore_ctx->nprops_ctx, request->PropertyIds[i], mem_ctx, &nameid) == MAPISTORE_SUCCESS) {
			response->nameid[i] = *nameid;
		}
		else {
			response->nameid[i].ulKind = 0xff;
		}
	}

	*size += libmapiserver_RopGetNamesFromIDs_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc DeletePropertiesNoReplicate (0x7a) Rop. deletes property
   values from an object without invoking replication.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the DeletePropertiesNoReplicate
   EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the DeletePropertiesNoReplicate
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
	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] DeletePropertiesNoReplicate (0x7a) -- stub\n");

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
	struct CopyTo_req	*request;
	struct CopyTo_repl	*response;
	enum MAPISTATUS		retval;
	uint32_t		handle;
	struct mapi_handles	*rec = NULL;
	void			*private_data = NULL;
	struct emsmdbp_object	*source_object;
	struct emsmdbp_object	*dest_object;
	struct SPropTagArray	excluded_tags;

	OC_DEBUG(4, "exchange_emsmdb: [OXCPRPT] CopyTo (0x39)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = &mapi_req->u.mapi_CopyTo;
	response = &mapi_repl->u.mapi_CopyTo;

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	
	response->PropertyProblemCount = 0;
	response->PropertyProblem = NULL;

	if (request->WantAsynchronous) {
		OC_DEBUG(0, "  warning: asynchronous operations are not supported\n");
	}
	if ((request->CopyFlags & CopyFlagsMove)) {
		OC_DEBUG(0, "  moving properties is not supported\n");
        }
	if ((request->CopyFlags & CopyFlagsNoOverwrite)) {
		OC_DEBUG(0, "  properties WILL BE overwriten despite the operation flags\n");
	}

	/* Get the source object */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(0, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);
        source_object = private_data;
	if (!source_object) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(0, "  object (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	/* Get the destination object */
	handle = handles[request->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(0, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);
        dest_object = private_data;
	if (!dest_object) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(0, "  object (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	excluded_tags.cValues = request->ExcludedTags.cValues;
	excluded_tags.aulPropTag = request->ExcludedTags.aulPropTag;

	mapi_repl->error_code = emsmdbp_object_copy_properties(emsmdbp_ctx, source_object, dest_object, &excluded_tags, request->WantSubObjects);

end:
	*size += libmapiserver_RopCopyTo_size(mapi_repl);

	return MAPI_E_SUCCESS;
}
