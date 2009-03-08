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
   \file oxcfold.c

   \brief Folder object routines and Rops
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"


/**
   \details Open a System or Special folder object.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param request OpenFolder request
   \param response pointer to the OpenFolder response

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
static enum MAPISTATUS RopOpenFolder_SystemSpecialFolder(TALLOC_CTX *mem_ctx, 
							 struct emsmdbp_context *emsmdbp_ctx,
							 struct OpenFolder_req request,
							 struct OpenFolder_repl *response)
{
	/* Find parent record */
	/* Set parent record as basedn */
	/* Look for systemfolder given its FolderID */

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc OpenFolder (0x02) Rop. This operation opens an
   existing folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenFolder EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenFolder EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopOpenFolder(TALLOC_CTX *mem_ctx,
					       struct emsmdbp_context *emsmdbp_ctx,
					       struct EcDoRpc_MAPI_REQ *mapi_req,
					       struct EcDoRpc_MAPI_REPL *mapi_repl,
					       uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	struct OpenFolder_req		request;
	struct OpenFolder_repl		response;
	struct mapi_handles		*parent = NULL;
	struct mapi_handles		*rec = NULL;
	struct emsmdbp_object		*object;
	uint32_t			handle;
	int				parentfolder = -1;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] OpenFolder (0x02)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = mapi_req->u.mapi_OpenFolder;
	response = mapi_repl->u.mapi_OpenFolder;

	mapi_repl->u.mapi_OpenFolder.HasRules = 0;
	mapi_repl->u.mapi_OpenFolder.IsGhosted = 0;

	/* Step 1. Retrieve parent handle in the hierarchy */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	retval = mapi_handles_get_systemfolder(parent, &parentfolder);

	switch (parentfolder) {
	case 0x0:
		/* system/special folder */
		retval = RopOpenFolder_SystemSpecialFolder(mem_ctx, emsmdbp_ctx, request, &response);
		mapi_repl->error_code = retval;
		break;
	default:
		/* handled by mapistore */
		break;
	}

	*size += libmapiserver_RopOpenFolder_size(mapi_repl);

	/* Fill EcDoRpc_MAPI_REPL reply */
	if (!mapi_repl->error_code) {
		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);

		object = emsmdbp_object_folder_init((TALLOC_CTX *)rec, emsmdbp_ctx, mapi_req, parent);
		if (object) {
			retval = mapi_handles_set_systemfolder(rec, object->object.folder->systemfolder);
			retval = mapi_handles_set_private_data(rec, object);
		}
		mapi_repl->opnum = mapi_req->opnum;
		mapi_repl->handle_idx = mapi_req->u.mapi_OpenFolder.handle_idx;

		handles[mapi_repl->handle_idx] = rec->handle;
	}

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc GetHierarchyTable (0x04) Rop. This operation gets
   the subfolder hierarchy table for a folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetHierarchyTable EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the GetHierarchyTable EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetHierarchyTable(TALLOC_CTX *mem_ctx,
						      struct emsmdbp_context *emsmdbp_ctx,
						      struct EcDoRpc_MAPI_REQ *mapi_req,
						      struct EcDoRpc_MAPI_REPL *mapi_repl,
						      uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	struct mapi_handles	*parent;
	struct mapi_handles	*rec = NULL;
	struct emsmdbp_object	*object = NULL;
	void			*data;
	uint32_t		folderID;
	uint32_t		handle;
	int			parentfolder = -1;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] GetHierarchyTable (0x04)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	retval = mapi_handles_get_systemfolder(parent, &parentfolder);

	/* Initialize default empty GetHierarchyTable reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->u.mapi_GetHierarchyTable.handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* GetHierarchyTable can only be called for mailbox/folder objects */
	mapi_handles_get_private_data(parent, &data);
	object = (struct emsmdbp_object *)data;
	if (!object) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		return MAPI_E_SUCCESS;
	}

	switch (object->type) {
	case EMSMDBP_OBJECT_MAILBOX:
		folderID = object->object.mailbox->folderID;
		break;
	case EMSMDBP_OBJECT_FOLDER:
		folderID = object->object.folder->folderID;
		break;
	default:
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		return MAPI_E_SUCCESS;
	}

	switch (parentfolder) {
	case 0x0:
	case 0x1:
		/* system/special folder */
		retval = openchangedb_get_folder_count(emsmdbp_ctx->oc_ctx, folderID, 
						       &mapi_repl->u.mapi_GetHierarchyTable.RowCount);
		break;
	default:
		/* handled by mapistore */
		mapi_repl->u.mapi_GetHierarchyTable.RowCount = 0;
		break;
	}

	/* Initialize Table object */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
	handles[mapi_repl->handle_idx] = rec->handle;

	object = emsmdbp_object_table_init((TALLOC_CTX *)rec, emsmdbp_ctx, parent);
	if (object) {
		retval = mapi_handles_set_private_data(rec, object);
		object->object.table->denominator = mapi_repl->u.mapi_GetHierarchyTable.RowCount;
	}

	*size += libmapiserver_RopGetHierarchyTable_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc GetContentsTable (0x05) Rop. This operation get
   the content table of a container.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetContentsTable EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the GetContentsTable EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetContentsTable(TALLOC_CTX *mem_ctx,
						     struct emsmdbp_context *emsmdbp_ctx,
						     struct EcDoRpc_MAPI_REQ *mapi_req,
						     struct EcDoRpc_MAPI_REPL *mapi_repl,
						     uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	struct mapi_handles	*rec = NULL;
	uint32_t		handle;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] GetContentsTable (0x05)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->u.mapi_GetContentsTable.handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_GetContentsTable.RowCount = 0;

	*size += libmapiserver_RopGetContentsTable_size(mapi_repl);

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
	handles[mapi_repl->handle_idx] = rec->handle;

	return MAPI_E_SUCCESS;
}
