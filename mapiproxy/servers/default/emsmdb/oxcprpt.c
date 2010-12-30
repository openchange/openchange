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

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"

/**
   \details Retrieve properties on a mapistore object
   
   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param request GetProps request
   \param response pointer to the GetProps reply
   \param private_data pointer tot eh private data stored for this
   object

   \note We do not handle anything yet. This is just a skeleton.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
static enum MAPISTATUS RopGetPropertiesSpecific_mapistore(TALLOC_CTX *mem_ctx,
							  struct emsmdbp_context *emsmdbp_ctx,
							  struct GetProps_req request,
							  struct GetProps_repl *response,
							  void *private_data)
{
	enum MAPISTATUS		retval;
	struct emsmdbp_object	*object;
	uint32_t		contextID = -1;
	uint64_t		fmid = 0;
	void			*data;
	struct SPropTagArray	SPropTagArray;
	struct SRow		*aRow;
	int			i;
	int			j;
	uint8_t			type;
	bool			found = false;

	object = (struct emsmdbp_object *) private_data;
	if (object) {
		switch (object->type) {
		case EMSMDBP_OBJECT_FOLDER:
			/* contextID = object->object.folder->contextID; */
			/* fmid = object->object.folder->folderID; */
			/* type = MAPISTORE_FOLDER; */
			break;
		case EMSMDBP_OBJECT_MESSAGE:
			contextID = object->object.message->contextID;
			fmid = object->object.message->messageID;
			type = MAPISTORE_MESSAGE;
			break;
		default:
			break;
		}
	}

	SPropTagArray.cValues = request.prop_count;
	SPropTagArray.aulPropTag = request.properties;

	if (contextID != -1) {
		aRow = talloc_zero(mem_ctx, struct SRow);
		aRow->cValues = 0;
		mapistore_getprops(emsmdbp_ctx->mstore_ctx, contextID, fmid, type, &SPropTagArray, aRow);
		/* Check if we need the layout */
		for (i = 0; i < request.prop_count; i++) {
			for (j = 0; j < aRow->cValues; j++) {
				if (request.properties[i] == aRow->lpProps[j].ulPropTag) {
					found = true;
					response->layout = 0x0;
				}
			}
			if (found == false) {
				response->layout = 0x1;
				break;
			}
		}
		
		for (i = 0; i < request.prop_count; i++) {
			response->layout = 0x1;
			data = (void *) find_SPropValue_data(aRow, request.properties[i]);
			if (data == NULL) {
				request.properties[i] = (request.properties[i] & 0xFFFF0000) + PT_ERROR;
				data = (void *) find_SPropValue_data(aRow, request.properties[i]);
				if (data == NULL) {
					retval = MAPI_E_NOT_FOUND;
					data = (void *)&retval;
				}
			} 
			libmapiserver_push_property(mem_ctx, lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
						    request.properties[i], (const void *)data,
						    &response->prop_data, response->layout, 0);
		}

	} else {
		response->layout = 0x1;
		for (i = 0; i < request.prop_count; i++) {
			request.properties[i] = (request.properties[i] & 0xFFFF0000) + PT_ERROR;
			retval = MAPI_E_NOT_FOUND;
			response->layout = 0x1;
			data = (void *)&retval;
			libmapiserver_push_property(mem_ctx, lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
						    request.properties[i], (const void *)data,
						    &response->prop_data, response->layout, 0);
		}
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve properties on a mailbox object.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param request GetProps request
   \param response pointer to the GetProps reply
   \param private_data pointer to the private data stored for this
   object

   \note Mailbox objects have a limited set of supported properties.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
static enum MAPISTATUS RopGetPropertiesSpecific_Mailbox(TALLOC_CTX *mem_ctx,
							struct emsmdbp_context *emsmdbp_ctx,
							struct GetProps_req request,
							struct GetProps_repl *response,
							void *private_data)
{
	enum MAPISTATUS			retval;
	struct emsmdbp_object		*object;
	void				*data;
	struct SBinary_short		bin;
	uint32_t			i;
	uint32_t			error = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!private_data, MAPI_E_INVALID_PARAMETER, NULL);

	object = (struct emsmdbp_object *) private_data;

	/* Step 1. Check if we need a layout */
	response->layout = 0;
	for (i = 0; i < request.prop_count; i++) {
		switch (request.properties[i]) {
		case PR_MAPPING_SIGNATURE:
		case PR_IPM_PUBLIC_FOLDERS_ENTRYID:
			response->layout = 0x1;
			break;
		case PR_USER_ENTRYID:
			break;
		case PR_MAILBOX_OWNER_ENTRYID:
		case PR_MAILBOX_OWNER_NAME:
		case PR_MAILBOX_OWNER_NAME_UNICODE:
			if (object->object.mailbox->mailboxstore == false) {
				response->layout = 0x1;
			}
			break;
		default:
			retval = openchangedb_get_folder_property(mem_ctx, emsmdbp_ctx->oc_ctx,
								  emsmdbp_ctx->szDisplayName, 
								  request.properties[i],
								  object->object.mailbox->folderID, 
								  (void **)&data);
			if (retval) {
				response->layout = 0x1;
			}
			break;
		}
		if (response->layout == 1) {
			break;
		}
	}

	/* Step 2. Fill the GetProps blob */
	for (i = 0; i < request.prop_count; i++) {
		switch (request.properties[i]) {
		case PR_MAPPING_SIGNATURE:
		case PR_IPM_PUBLIC_FOLDERS_ENTRYID:
			error = MAPI_E_NO_ACCESS;
			request.properties[i] = (request.properties[i] & 0xFFFF0000) + PT_ERROR;
			libmapiserver_push_property(mem_ctx, lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
						    request.properties[i], (const void *)&error, 
						    &response->prop_data, response->layout, 0);
			break;
		case PR_USER_ENTRYID:
			retval = entryid_set_AB_EntryID(mem_ctx, object->object.mailbox->szUserDN, &bin);
			libmapiserver_push_property(mem_ctx, lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
						    request.properties[i], (const void *)&bin,
						    &response->prop_data, response->layout, 0);
			talloc_free(bin.lpb);
			break;
		case PR_MAILBOX_OWNER_ENTRYID:
			if (object->object.mailbox->mailboxstore == false) {
				error = MAPI_E_NO_ACCESS;
				request.properties[i] = (request.properties[i] & 0xFFFF0000) + PT_ERROR;
				libmapiserver_push_property(mem_ctx, lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
							    request.properties[i], (const void *)&error,
							    &response->prop_data, response->layout, 0);
			} else {
				retval = entryid_set_AB_EntryID(mem_ctx, object->object.mailbox->owner_EssDN,
								&bin);
				libmapiserver_push_property(mem_ctx, lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
							    request.properties[i], (const void *)&bin,
							    &response->prop_data, response->layout, 0);
				talloc_free(bin.lpb);
			}
			break;
		case PR_MAILBOX_OWNER_NAME:
		case PR_MAILBOX_OWNER_NAME_UNICODE:
			if (object->object.mailbox->mailboxstore == false) {
				error = MAPI_E_NO_ACCESS;
				request.properties[i] = (request.properties[i] & 0xFFFF0000) + PT_ERROR;
 				libmapiserver_push_property(mem_ctx, lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
							    request.properties[i], (const void *)&error,
							    &response->prop_data, response->layout, 0);
			} else {
				libmapiserver_push_property(mem_ctx, lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
							    request.properties[i], 
							    (const void *)object->object.mailbox->owner_Name,
							    &response->prop_data, response->layout, 0);
			}
			break;
		default:
			retval = openchangedb_get_folder_property(mem_ctx, emsmdbp_ctx->oc_ctx,
								  emsmdbp_ctx->szDisplayName, request.properties[i],
								  object->object.mailbox->folderID, (void **)&data);
			if (retval) {
				request.properties[i] = (request.properties[i] & 0xFFFF0000) + PT_ERROR;
				data = (void *)&retval;
			}
			libmapiserver_push_property(mem_ctx, lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
						    request.properties[i], (const void *)data, 
						    &response->prop_data, response->layout, 0);
			break;
		}
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve properties on a systemfolder object.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param request GetProps request
   \param response pointer to the GetProps reply
   \param private_data pointer to the private data stored for this
   object
   \param private_data pointer to the private data stored for this
   object

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
static enum MAPISTATUS RopGetPropertiesSpecific_SystemSpecialFolder(TALLOC_CTX *mem_ctx,
								    struct emsmdbp_context *emsmdbp_ctx,
								    struct GetProps_req request,
								    struct GetProps_repl *response,
								    void *private_data)
{
	enum MAPISTATUS			retval;
	struct emsmdbp_object		*object;
	struct emsmdbp_object_folder	*folder;
	void				*data;
	int				i;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!private_data, MAPI_E_INVALID_PARAMETER, NULL);

	object = (struct emsmdbp_object *) private_data;
	folder = (struct emsmdbp_object_folder *) object->object.folder;

	/* Step 1. Lookup properties and set layout */
	response->layout = 0x0;
	for (i = 0; i < request.prop_count; i++) {
		if (openchangedb_lookup_folder_property(emsmdbp_ctx->oc_ctx, request.properties[i], 
							folder->folderID)) {
			response->layout = 0x1;
			break;
		}
	}

	/* Step 2. Fetch properties values */
	for (i = 0; i < request.prop_count; i++) {
		retval = openchangedb_get_folder_property(mem_ctx, emsmdbp_ctx->oc_ctx, 
							  emsmdbp_ctx->szDisplayName, request.properties[i],
							  folder->folderID, (void **)&data);
		if (retval) {
			request.properties[i] = (request.properties[i] & 0xFFFF0000) + PT_ERROR;
			data = (void *)&retval;
		}
		libmapiserver_push_property(mem_ctx, lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
					    request.properties[i], (const void *)data,
					    &response->prop_data, response->layout, 0);
	}

	return MAPI_E_SUCCESS;
}


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
	struct GetProps_req	request;
	struct GetProps_repl	response;
	uint32_t		handle;
	struct mapi_handles	*rec = NULL;
	void			*private_data = NULL;
	bool			mapistore = false;
	struct emsmdbp_object	*object;

	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] GetPropertiesSpecific (0x07)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = mapi_req->u.mapi_GetProps;
	response = mapi_repl->u.mapi_GetProps;

	/* Initialize GetProps response blob */
	response.prop_data.length = 0;
	response.prop_data.data = NULL;

	/* Fill EcDoRpc_MAPI_REPL reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_NOT_FOUND;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) goto end;

	retval = mapi_handles_get_private_data(rec, &private_data);

	mapistore = emsmdbp_is_mapistore(rec);
	/* Nasty hack */
	if (!private_data) {
		mapistore = true;
	}

	/* Temporary hack: If this is a mapistore root container
	 * (e.g. Inbox, Calendar etc.), directly stored under
	 * IPM.Subtree, then fetch properties from openchange
	 * dispatcher db, not mapistore */
	object = (struct emsmdbp_object *) private_data;
	if (object && object->type == EMSMDBP_OBJECT_FOLDER &&
	    object->object.folder->mapistore_root == true) {
		retval = RopGetPropertiesSpecific_SystemSpecialFolder(mem_ctx, emsmdbp_ctx, 
								      request, &response, private_data);
	} else {
		switch (mapistore) {
		case false:
			switch (object->type) {
			case EMSMDBP_OBJECT_MAILBOX:
				retval = RopGetPropertiesSpecific_Mailbox(mem_ctx, emsmdbp_ctx, request, &response, private_data);
				break;
			case EMSMDBP_OBJECT_FOLDER:
				retval = RopGetPropertiesSpecific_SystemSpecialFolder(mem_ctx, emsmdbp_ctx, request, &response, private_data);
				break;
			default:
				break;
			}
			break;
		case true:
			/* folder or messages handled by mapistore */
			retval = RopGetPropertiesSpecific_mapistore(mem_ctx, emsmdbp_ctx, request, &response, private_data);
			break;
		}
	}

	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_GetProps = response;

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
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);
	object = (struct emsmdbp_object *)private_data;
	if (!object) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	mapistore = emsmdbp_is_mapistore(rec);
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
			fmid = object->object.message->messageID;
			contextID = object->object.message->contextID;
			mapistore_setprops(emsmdbp_ctx->mstore_ctx, contextID, fmid, 
					   MAPISTORE_MESSAGE, &aRow);
		}
		else if (object->type == EMSMDBP_OBJECT_FOLDER) {
			fmid = object->object.folder->folderID;
			contextID = object->object.folder->contextID;
			mapistore_setprops(emsmdbp_ctx->mstore_ctx, contextID, fmid, 
					   MAPISTORE_FOLDER, &aRow);
		}
		else {
			DEBUG(5, ("  object type %d not implemented\n", object->type));
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
	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] DeleteProperties (0x0b)\n"));

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
	uint32_t			handle, contextID;
	uint64_t			objectID;
	uint8_t				objectType;
	int				fd;
	char				*filename;
	void				*data;

	DEBUG(4, ("exchange_emsmdb: [OXCPRPT] OpenStream (0x2b)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->u.mapi_OpenStream.StreamSize = 0;

	/* Step 1. Retrieve parent handle in the hierarchy */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	mapi_handles_get_private_data(parent, &data);
	parent_object = (struct emsmdbp_object *) data;
	if (parent_object->type == EMSMDBP_OBJECT_FOLDER) {
		objectID = parent_object->object.folder->folderID;
		objectType = MAPISTORE_FOLDER;
	}
	else if (parent_object->type == EMSMDBP_OBJECT_MESSAGE) {
		objectID = parent_object->object.message->messageID;
		objectType = MAPISTORE_MESSAGE;
	}
	else {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
	}

	/* TODO: implementation status:
	   - message:
	     - OpenStream_ReadOnly (supported)
	     - OpenStream_ReadWrite
	     - OpenStream_Create (supported)
	     - OpenStream_BestAccess
	   - folder:
	     - OpenStream_ReadOnly
	     - OpenStream_ReadWrite
	     - OpenStream_Create
	     - OpenStream_BestAccess
	*/

	if (!mapi_repl->error_code) {
		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
		object = emsmdbp_object_stream_init((TALLOC_CTX *)rec, emsmdbp_ctx,
						    mapi_req->u.mapi_OpenStream.PropertyTag,
						    mapi_req->u.mapi_OpenStream.OpenModeFlags,
						    parent);
		
		if (object) {
			retval = mapi_handles_set_private_data(rec, object);
			filename = talloc_asprintf(mem_ctx, "/tmp/openchange-stream-XXXXXX");
			fd = mkstemp(filename);
			if (fd > -1) {
				object->object.stream->fd = fd;
				object->object.stream->objectID = objectID;
				object->object.stream->objectType = objectType;

				/* We unlink the file immediately as we will
				   only use its fd from now on... */
				unlink(filename);

				if (object->object.stream->flags == OpenStream_ReadOnly
				    || object->object.stream->flags == OpenStream_ReadWrite) {
					if (parent_object->type == EMSMDBP_OBJECT_FOLDER) {
						contextID = parent_object->object.folder->contextID;
					}
					else {
						contextID = parent_object->object.message->contextID;
					}
					mapi_repl->error_code = mapistore_get_property_into_fd(parent_object->mstore_ctx,
											       contextID,
											       objectID,
											       objectType,
											       mapi_req->u.mapi_OpenStream.PropertyTag,
											       fd);
					
					lseek(object->object.stream->fd, SEEK_SET, 0);
				}
			}
			else {
				mapi_repl->error_code = MAPI_E_DISK_ERROR;
			}

			talloc_free(filename);
		}

		mapi_repl->handle_idx = mapi_req->u.mapi_OpenStream.handle_idx;
		handles[mapi_repl->handle_idx] = rec->handle;
	}

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
	struct emsmdbp_object		*object = NULL;
	uint32_t			handle, bufferSize;

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
	if (retval) goto end;

	retval = mapi_handles_get_private_data(rec, &private_data);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_STREAM
	    || object->object.stream->fd == -1) {
		mapi_repl->error_code = MAPI_E_INVALID_PARAMETER;
	}
	else {
		bufferSize = mapi_req->u.mapi_ReadStream.ByteCount;
		/* careful here, let's switch to idiot mode */
		if (bufferSize == 0xBABE) {
			bufferSize = mapi_req->u.mapi_ReadStream.MaximumByteCount.value;
		}
		mapi_repl->u.mapi_ReadStream.data.data = talloc_array(object, uint8_t, bufferSize);
		mapi_repl->u.mapi_ReadStream.data.length = read(object->object.stream->fd,
								mapi_repl->u.mapi_ReadStream.data.data,
								bufferSize);
	}
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
	uint32_t			handle;
	ssize_t				written;

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
	if (retval) goto end;

	retval = mapi_handles_get_private_data(parent, &private_data);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_STREAM
	    || object->object.stream->fd == -1) {
		mapi_repl->error_code = MAPI_E_INVALID_PARAMETER;
	}
	else {
		if (mapi_req->u.mapi_WriteStream.data.length > 0) {
			written = write(object->object.stream->fd,
					mapi_req->u.mapi_WriteStream.data.data,
					mapi_req->u.mapi_WriteStream.data.length);
			if (written < 0) {
				DEBUG (5, ("%s: write failed (%d): %s\n",
					   __PRETTY_FUNCTION__, errno,
					   strerror(errno)));
				mapi_repl->error_code = MAPI_E_INVALID_PARAMETER;
			}
			else {
				mapi_repl->u.mapi_WriteStream.WrittenSize = written;
			}
		}
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
	struct stat			stream_file_stat;

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
	if (retval) goto end;

	retval = mapi_handles_get_private_data(parent, &private_data);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_STREAM
	    || object->object.stream->fd == -1) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
	}
	else {
		if (fstat(object->object.stream->fd, &stream_file_stat) == 0) {
			mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		}
		else {
			mapi_repl->u.mapi_GetStreamSize.StreamSize = stream_file_stat.st_size;
		}
	}

end:
	*size += libmapiserver_RopGetStreamSize_size(mapi_repl);

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
