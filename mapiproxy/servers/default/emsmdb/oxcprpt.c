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
	enum MAPISTATUS	retval;
	void		*data;
	int		i;

	response->layout = 0x1;
	for (i = 0; i < request.prop_count; i++) {
		request.properties[i] = (request.properties[i] & 0xFFFF0000) + PT_ERROR;
		retval = MAPI_E_NOT_FOUND;
		data = (void *)&retval;
		libmapiserver_push_property(mem_ctx, lp_iconv_convenience(emsmdbp_ctx->lp_ctx),
					    request.properties[i], (const void *)data,
					    &response->prop_data, response->layout, 0);
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
		default:
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
			libmapiserver_push_property(mem_ctx, lp_iconv_convenience(emsmdbp_ctx->lp_ctx),
						    request.properties[i], (const void *)&error, 
						    &response->prop_data, response->layout, 0);
			break;
		case PR_USER_ENTRYID:
			retval = entryid_set_AB_EntryID(mem_ctx, object->object.mailbox->szUserDN, &bin);
			libmapiserver_push_property(mem_ctx, lp_iconv_convenience(emsmdbp_ctx->lp_ctx),
						    request.properties[i], (const void *)&bin,
						    &response->prop_data, response->layout, 0);
			talloc_free(bin.lpb);
			break;
		case PR_MAILBOX_OWNER_ENTRYID:
			retval = entryid_set_AB_EntryID(mem_ctx, object->object.mailbox->owner_EssDN, &bin);
			libmapiserver_push_property(mem_ctx, lp_iconv_convenience(emsmdbp_ctx->lp_ctx),
						    request.properties[i], (const void *)&bin,
						    &response->prop_data, response->layout, 0);
			talloc_free(bin.lpb);
			break;
		case PR_MAILBOX_OWNER_NAME:
		case PR_MAILBOX_OWNER_NAME_UNICODE:
			libmapiserver_push_property(mem_ctx, lp_iconv_convenience(emsmdbp_ctx->lp_ctx),
						    request.properties[i], (const void *)object->object.mailbox->owner_Name,
						    &response->prop_data, response->layout, 0);
			break;
		default:
			retval = openchangedb_get_folder_property(mem_ctx, emsmdbp_ctx->oc_ctx,
								  emsmdbp_ctx->szDisplayName, request.properties[i],
								  object->object.mailbox->folderID, (void **)&data);
			if (retval) {
				request.properties[i] = (request.properties[i] & 0xFFFF0000) + PT_ERROR;
				data = (void *)&retval;
			}
			libmapiserver_push_property(mem_ctx, lp_iconv_convenience(emsmdbp_ctx->lp_ctx),
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
   \param response pointer to the private data stored for this
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
		libmapiserver_push_property(mem_ctx, lp_iconv_convenience(emsmdbp_ctx->lp_ctx),
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
	int			systemfolder = -1;
	void			*private_data = NULL;

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

	retval = mapi_handles_get_systemfolder(rec, &systemfolder);
	retval = mapi_handles_get_private_data(rec, &private_data);

	DEBUG(0, ("GetProps: SystemFolder = %d\n", systemfolder));

	switch (systemfolder) {
	case -1:
		/* folder and messages handled by mapistore */
		retval = RopGetPropertiesSpecific_mapistore(mem_ctx, emsmdbp_ctx, request, &response, private_data);
		break;
	case 0x0:
		/* private mailbox */
		retval = RopGetPropertiesSpecific_Mailbox(mem_ctx, emsmdbp_ctx, request, &response, private_data);
		break;
	case 0x1:
		/* system or special folder */
		retval = RopGetPropertiesSpecific_SystemSpecialFolder(mem_ctx, emsmdbp_ctx, request, &response, private_data);
		break;
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

	*size += libmapiserver_RopSetProperties_size(mapi_repl);

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

	mapi_repl->u.mapi_GetIDsFromNames.count = 0;
	mapi_repl->u.mapi_GetIDsFromNames.propID = NULL;

	*size += libmapiserver_RopGetPropertyIdsFromNames_size(mapi_repl);

	return MAPI_E_SUCCESS;
}
