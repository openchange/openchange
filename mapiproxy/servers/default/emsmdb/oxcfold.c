/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009-2010

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
	enum mapistore_error		ret;
	struct mapi_handles		*parent = NULL;
	struct mapi_handles		*rec = NULL;
        void                            *private_data;
	struct emsmdbp_object		*object, *parent_object;
	uint32_t			handle;
	struct OpenFolder_req		*request;
	struct OpenFolder_repl		*response;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] OpenFolder (0x02)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = &mapi_req->u.mapi_OpenFolder;
	response = &mapi_repl->u.mapi_OpenFolder;

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = request->handle_idx;

	/* Step 1. Retrieve parent handle in the hierarchy */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	/* With OpenFolder, the parent object may NOT BE the direct parent folder of the folder */
	mapi_handles_get_private_data(parent, &private_data);
        parent_object = private_data;
	if (!parent_object || (parent_object->type != EMSMDBP_OBJECT_FOLDER && parent_object->type != EMSMDBP_OBJECT_MAILBOX)) {
		DEBUG(5, ("  invalid handle (%x): %x\n", handle, mapi_req->handle_idx));
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	/* Fill EcDoRpc_MAPI_REPL reply */
	response->HasRules = 0;
	response->IsGhosted = 0;

	mapi_handles_add(emsmdbp_ctx->handles_ctx, 0, &rec);
	ret = emsmdbp_object_open_folder_by_fid(rec, emsmdbp_ctx, parent_object, request->folder_id, &object);
	if (ret != MAPISTORE_SUCCESS) {
		if (ret == MAPISTORE_ERR_DENIED) {
			mapi_repl->error_code = MAPI_E_NO_ACCESS;
		}
		else {
			mapi_repl->error_code = MAPI_E_NOT_FOUND;
		}
		goto end;
	}
	retval = mapi_handles_set_private_data(rec, object);
	handles[mapi_repl->handle_idx] = rec->handle;

end:
	*size += libmapiserver_RopOpenFolder_size(mapi_repl);

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
	struct emsmdbp_object	*object = NULL, *parent_object = NULL;
	struct mapistore_subscription_list *subscription_list;
	struct mapistore_subscription *subscription;
	struct mapistore_table_subscription_parameters subscription_parameters;
	void			*data;
	uint64_t		folderID;
	uint32_t		handle;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] GetHierarchyTable (0x04)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	/* Initialize default empty GetHierarchyTable reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->u.mapi_GetHierarchyTable.handle_idx;

	/* GetHierarchyTable can only be called for mailbox/folder objects */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	mapi_handles_get_private_data(parent, &data);
	parent_object = (struct emsmdbp_object *)data;
	if (!parent_object) {
		DEBUG(5, ("  no object found\n"));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	switch (parent_object->type) {
	case EMSMDBP_OBJECT_MAILBOX:
		folderID = parent_object->object.mailbox->folderID;
		break;
	case EMSMDBP_OBJECT_FOLDER:
		folderID = parent_object->object.folder->folderID;
		break;
	default:
		DEBUG(5, ("  unsupported object type\n"));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	/* Initialize Table object */
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
	handles[mapi_repl->handle_idx] = rec->handle;

	object = emsmdbp_folder_open_table(rec, parent_object, MAPISTORE_FOLDER_TABLE, rec->handle);
	if (!object) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}
	mapi_handles_set_private_data(rec, object);
	mapi_repl->u.mapi_GetHierarchyTable.RowCount = object->object.table->denominator;

	/* notifications */
	if ((mapi_req->u.mapi_GetHierarchyTable.TableFlags & TableFlags_NoNotifications)) {
		DEBUG(5, ("  notifications skipped\n"));
	}
	else {
		/* we attach the subscription to the session object */
		subscription_list = talloc_zero(emsmdbp_ctx->mstore_ctx, struct mapistore_subscription_list);
		DLIST_ADD(emsmdbp_ctx->mstore_ctx->subscriptions, subscription_list);

		subscription_parameters.table_type = MAPISTORE_FOLDER_TABLE;
		subscription_parameters.folder_id = folderID;

		/* note that a mapistore_subscription can exist without a corresponding emsmdbp_object (tables) */
		subscription = mapistore_new_subscription(subscription_list, emsmdbp_ctx->mstore_ctx,
							  emsmdbp_ctx->username,
							  rec->handle, fnevTableModified, &subscription_parameters);
		subscription_list->subscription = subscription;
		object->object.table->subscription_list = subscription_list;
	}

end:
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
	struct mapi_handles	*parent;
	struct mapi_handles	*rec = NULL;
	struct emsmdbp_object	*object = NULL, *parent_object;
        struct mapistore_subscription_list *subscription_list;
        struct mapistore_subscription *subscription;
        struct mapistore_table_subscription_parameters subscription_parameters;
	void			*data;
	uint64_t		folderID;
	uint32_t		handle;
	uint8_t			table_type;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] GetContentsTable (0x05)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	/* Initialize default empty GetContentsTable reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->u.mapi_GetContentsTable.handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_GetContentsTable.RowCount = 0;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	/* GetContentsTable can only be called for folder objects */
	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		DEBUG(5, ("  handle data not found, idx = %x\n", mapi_req->handle_idx));
		goto end;
	}

	parent_object = (struct emsmdbp_object *)data;
	if (!parent_object) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		DEBUG(5, ("  handle data not found, idx = %x\n", mapi_req->handle_idx));
		goto end;
	}

	if (parent_object->type != EMSMDBP_OBJECT_FOLDER) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	folderID = parent_object->object.folder->folderID;
	if ((mapi_req->u.mapi_GetContentsTable.TableFlags & TableFlags_Associated)) {
		DEBUG(5, ("  table is FAI table\n"));
		table_type = MAPISTORE_FAI_TABLE;
	}
	else {
		DEBUG(5, ("  table is contents table\n"));
		table_type = MAPISTORE_MESSAGE_TABLE;
	}

	/* Initialize Table object */
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
	handles[mapi_repl->handle_idx] = rec->handle;

	object = emsmdbp_folder_open_table(rec, parent_object, table_type, rec->handle);
	if (!object) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}
	mapi_handles_set_private_data(rec, object);
	mapi_repl->u.mapi_GetContentsTable.RowCount = object->object.table->denominator;

	/* notifications */
	if ((mapi_req->u.mapi_GetContentsTable.TableFlags & TableFlags_NoNotifications)) {
		DEBUG(5, ("  notifications skipped\n"));
	}
	else {
		/* we attach the subscription to the session object */
		subscription_list = talloc_zero(emsmdbp_ctx->mstore_ctx, struct mapistore_subscription_list);
		DLIST_ADD(emsmdbp_ctx->mstore_ctx->subscriptions, subscription_list);
		
		if ((mapi_req->u.mapi_GetContentsTable.TableFlags & TableFlags_Associated)) {
			subscription_parameters.table_type = MAPISTORE_FAI_TABLE;
		}
		else {
			subscription_parameters.table_type = MAPISTORE_MESSAGE_TABLE;
		}
		subscription_parameters.folder_id = folderID; 
                
		/* note that a mapistore_subscription can exist without a corresponding emsmdbp_object (tables) */
		subscription = mapistore_new_subscription(subscription_list, emsmdbp_ctx->mstore_ctx,
							  emsmdbp_ctx->username,
							  rec->handle, fnevTableModified, &subscription_parameters);
		subscription_list->subscription = subscription;
		object->object.table->subscription_list = subscription_list;
        }

end:
	
	*size += libmapiserver_RopGetContentsTable_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc CreateFolder (0x1c) Rop. This operation creates a
   folder on the remote server.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the CreateFolder EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the CreateFolder EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error

   \note We do not provide support for GhostInfo
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopCreateFolder(TALLOC_CTX *mem_ctx,
						 struct emsmdbp_context *emsmdbp_ctx,
						 struct EcDoRpc_MAPI_REQ *mapi_req,
						 struct EcDoRpc_MAPI_REPL *mapi_repl,
						 uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	enum mapistore_error		ret;
	struct mapi_handles		*parent = NULL;
	uint32_t			handle;
	uint64_t			parent_fid, fid, cn;
	struct SPropValue		cnValue;
	struct emsmdbp_object		*parent_object = NULL;
	struct emsmdbp_object		*object = NULL;
	struct CreateFolder_req		*request;
	struct CreateFolder_repl	*response;
	struct SRow			*aRow = NULL;
	void				*data;
	struct mapi_handles		*rec = NULL;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder (0x1c)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	/* Set up sensible values for the reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->u.mapi_CreateFolder.handle_idx;

	/* Step 1. Retrieve parent handle in the hierarchy */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* With CreateFolder, the parent object really IS the parent object */
	mapi_handles_get_private_data(parent, &data);
	parent_object = (struct emsmdbp_object *)data;
	if (!parent_object) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder null object\n"));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	if (parent_object->type != EMSMDBP_OBJECT_FOLDER && parent_object->type != EMSMDBP_OBJECT_MAILBOX) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder wrong object type: 0x%x\n", parent_object->type));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	request = &mapi_req->u.mapi_CreateFolder;
	response = &mapi_repl->u.mapi_CreateFolder;

	/* DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder parent: 0x%.16"PRIx64"\n", parent_fid)); */
	/* DEBUG(4, ("exchange_emsmdb: [OXCFOLD] Creating %s\n", request->FolderName.lpszW)); */

	/* if (request->ulFolderType != FOLDER_GENERIC) { */
	/* 	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] Unexpected folder type 0x%x\n", request->ulType)); */
	/* 	mapi_repl->error_code = MAPI_E_NO_SUPPORT; */
	/* 	goto end; */
	/* } */

	response->IsExistingFolder = false;

	ret = emsmdbp_object_get_fid_by_name(emsmdbp_ctx, parent_object, request->FolderName.lpszW, &fid);
	if (ret == MAPISTORE_SUCCESS) {
		if (request->ulFlags != OPEN_IF_EXISTS) {
			mapi_repl->error_code = MAPI_E_COLLISION;
			goto end;
		}
		response->IsExistingFolder = true;
	}

	mapi_handles_add(emsmdbp_ctx->handles_ctx, 0, &rec);
	if (response->IsExistingFolder) {
		ret = emsmdbp_object_open_folder_by_fid(rec, emsmdbp_ctx, parent_object, fid, &object);
		if (ret != MAPISTORE_SUCCESS) {
			DEBUG(5, (__location__": failure opening existing folder\n"));
			mapi_handles_delete(emsmdbp_ctx->handles_ctx, rec->handle);
			mapi_repl->error_code = retval;
			if (ret == MAPISTORE_ERR_DENIED) {
				mapi_repl->error_code = MAPI_E_NO_ACCESS;
			}
			else {
				mapi_repl->error_code = MAPI_E_CALL_FAILED;
			}
			goto end;
		}
	}
	else {
		/* Step 3. Turn CreateFolder parameters into MAPI property array */
		retval = openchangedb_get_new_folderID(emsmdbp_ctx->oc_ctx, &fid);
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(4, ("exchange_emsmdb: [OXCFOLD] Could not obtain a new folder id\n"));
			mapi_repl->error_code = MAPI_E_NO_SUPPORT;
			goto end;
		}

		retval = openchangedb_get_new_changeNumber(emsmdbp_ctx->oc_ctx, &cn);
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(4, ("exchange_emsmdb: [OXCFOLD] Could not obtain a new folder cn\n"));
			mapi_repl->error_code = MAPI_E_NO_SUPPORT;
			goto end;
		}

		parent_fid = parent_object->object.folder->folderID;
		
		aRow = libmapiserver_ROP_request_to_properties(mem_ctx, (void *)&mapi_req->u.mapi_CreateFolder, op_MAPI_CreateFolder);
		aRow->lpProps = add_SPropValue(mem_ctx, aRow->lpProps, &(aRow->cValues), PR_PARENT_FID, (void *)(&parent_fid));
		cnValue.ulPropTag = PidTagChangeNumber;
		cnValue.value.d = cn;
		SRow_addprop(aRow, cnValue);

		retval = emsmdbp_object_create_folder(emsmdbp_ctx, parent_object, rec, fid, aRow, &object);
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(5, (__location__": folder creation failed\n"));
			mapi_handles_delete(emsmdbp_ctx->handles_ctx, rec->handle);
			mapi_repl->error_code = retval;
			goto end;
		}
	}

	handles[mapi_repl->handle_idx] = rec->handle;
	mapi_handles_set_private_data(rec, object);

	response->folder_id = fid;

	if (response->IsExistingFolder == true) {
		response->GhostUnion.GhostInfo.HasRules = false;
		response->GhostUnion.GhostInfo.IsGhosted = false;
	}

end:
	*size += libmapiserver_RopCreateFolder_size(mapi_repl);

	if (aRow) {
		talloc_free(aRow);
	}

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc DeleteFolder (0x1d) Rop. This operation deletes a
   folder on the remote server.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the DeleteFolder EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the DeleteFolder EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update
   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopDeleteFolder(TALLOC_CTX *mem_ctx,
						 struct emsmdbp_context *emsmdbp_ctx,
						 struct EcDoRpc_MAPI_REQ *mapi_req,
						 struct EcDoRpc_MAPI_REPL *mapi_repl,
						 uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	enum mapistore_error	ret;
	struct mapi_handles	*rec = NULL;
	uint32_t		handle;
	void			*handle_priv_data;
	struct emsmdbp_object	*handle_object = NULL;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] DeleteFolder (0x1d)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	/* Initialize default empty DeleteFolder reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: factor this out to be convenience API */
	/* Convert the handle index into a handle, and then get the folder id */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	mapi_handles_get_private_data(rec, &handle_priv_data);
	handle_object = (struct emsmdbp_object *)handle_priv_data;
	if (!handle_object) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] DeleteFolder null object\n"));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		return MAPI_E_SUCCESS;
	}

	if (handle_object->type != EMSMDBP_OBJECT_FOLDER) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] DeleteFolder wrong object type: 0x%x\n", handle_object->type));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		return MAPI_E_SUCCESS;
	}

	retval = MAPI_E_SUCCESS;
	ret = emsmdbp_folder_delete(emsmdbp_ctx, handle_object, mapi_req->u.mapi_DeleteFolder.FolderId, mapi_req->u.mapi_DeleteFolder.DeleteFolderFlags);
	if (ret == MAPISTORE_ERR_EXIST) {
		mapi_repl->u.mapi_DeleteFolder.PartialCompletion = true;
	}
	else if (ret != MAPISTORE_SUCCESS) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] DeleteFolder failed to delete fid 0x%.16"PRIx64" (0x%x)",
			  mapi_req->u.mapi_DeleteFolder.FolderId, retval));
		retval = MAPI_E_NOT_FOUND;
	}
	mapi_repl->error_code = retval;

	*size += libmapiserver_RopDeleteFolder_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc DeleteMessage (0x1e) Rop. This operation (soft) deletes
   a message on the server.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the DeleteMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the DeleteMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopDeleteMessages(TALLOC_CTX *mem_ctx,
						   struct emsmdbp_context *emsmdbp_ctx,
						   struct EcDoRpc_MAPI_REQ *mapi_req,
						   struct EcDoRpc_MAPI_REPL *mapi_repl,
						   uint32_t *handles, uint16_t *size)
{
	uint32_t		parent_folder_handle;
	struct mapi_handles	*parent_folder = NULL;
	void			*parent_folder_private_data;
	struct emsmdbp_object	*parent_object;
	char			*owner;
	enum MAPISTATUS		retval;
	uint32_t		contextID;
	int 			i;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] DeleteMessage (0x1e)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_DeleteMessages.PartialCompletion = false;

	parent_folder_handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, parent_folder_handle, &parent_folder);
	if (retval != MAPI_E_SUCCESS) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto delete_message_response;
	}

	retval = mapi_handles_get_private_data(parent_folder, &parent_folder_private_data);
	parent_object = (struct emsmdbp_object *)parent_folder_private_data;
	if (!parent_object || parent_object->type != EMSMDBP_OBJECT_FOLDER) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto delete_message_response;
	}

	if (!emsmdbp_is_mapistore(parent_object) ) {
		DEBUG(0, ("Got parent folder not in mapistore\n"));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto delete_message_response;
	}

	contextID = emsmdbp_get_contextID(parent_object);
	owner = emsmdbp_get_owner(parent_object);
	for (i = 0; i < mapi_req->u.mapi_DeleteMessages.cn_ids; ++i) {
		int ret;
		uint64_t mid = mapi_req->u.mapi_DeleteMessages.message_ids[i];
		DEBUG(0, ("MID %i to delete: 0x%.16"PRIx64"\n", i, mid));
		ret = mapistore_folder_delete_message(emsmdbp_ctx->mstore_ctx, contextID, parent_object->backend_object, mid, MAPISTORE_SOFT_DELETE);
		if (ret != MAPISTORE_SUCCESS && ret != MAPISTORE_ERR_NOT_FOUND) {
			if (ret == MAPISTORE_ERR_DENIED) {
				mapi_repl->error_code = MAPI_E_NO_ACCESS;
			}
			else {
				mapi_repl->error_code = MAPI_E_CALL_FAILED;
			}
			goto delete_message_response;
		}

		ret = mapistore_indexing_record_del_mid(emsmdbp_ctx->mstore_ctx, contextID, owner, mid, MAPISTORE_SOFT_DELETE);
		if (ret != MAPISTORE_SUCCESS) {
			mapi_repl->error_code = MAPI_E_CALL_FAILED;
			goto delete_message_response;
		}
	}

delete_message_response:
	*size += libmapiserver_RopDeleteMessage_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc SetSearchCriteria (0x30) Rop. This operation sets
   the search criteria for a search folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SetSearchCriteria EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the SetSearchCriteria EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error  
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSetSearchCriteria(TALLOC_CTX *mem_ctx,
						      struct emsmdbp_context *emsmdbp_ctx,
						      struct EcDoRpc_MAPI_REQ *mapi_req,
						      struct EcDoRpc_MAPI_REPL *mapi_repl,
						      uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] SetSearchCriteria (0x30)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* TODO: actually implement this */

	*size += libmapiserver_RopSetSearchCriteria_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc GetSearchCriteria (0x31) Rop. This operation gets
   the search criteria for a search folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetSearchCriteria EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the GetSearchCriteria EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error  
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetSearchCriteria(TALLOC_CTX *mem_ctx,
						      struct emsmdbp_context *emsmdbp_ctx,
						      struct EcDoRpc_MAPI_REQ *mapi_req,
						      struct EcDoRpc_MAPI_REPL *mapi_repl,
						      uint32_t *handles, uint16_t *size)
{
	/* struct mapi_SRestriction *res; */

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] GetSearchCriteria (0x31)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* res = NULL; */
	mapi_repl->u.mapi_GetSearchCriteria.RestrictionDataSize = 0;
	mapi_repl->u.mapi_GetSearchCriteria.LogonId = mapi_req->logon_id;
	mapi_repl->u.mapi_GetSearchCriteria.FolderIdCount = 0;
	mapi_repl->u.mapi_GetSearchCriteria.FolderIds = NULL;
	mapi_repl->u.mapi_GetSearchCriteria.SearchFlags = 0;

	/* TODO: actually implement this */

	*size += libmapiserver_RopGetSearchCriteria_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS RopEmptyFolder_GenericFolder(TALLOC_CTX *mem_ctx,
                                                    struct emsmdbp_context *emsmdbp_ctx,
                                                    struct EmptyFolder_req request,
                                                    struct EmptyFolder_repl *response,
                                                    struct mapi_handles *folder)
{
	enum MAPISTATUS		ret = MAPI_E_SUCCESS;
	void                    *folder_priv;
	struct emsmdbp_object   *folder_object = NULL;
	uint32_t                context_id;
	enum mapistore_error	retval;
	uint64_t		*childFolders;
	uint32_t		childFolderCount;
	uint32_t		i;
	uint8_t			flags = DELETE_HARD_DELETE| DEL_MESSAGES | DEL_FOLDERS;
	TALLOC_CTX		*local_mem_ctx;
	void			*subfolder;

	/* Step 1. Retrieve the fid for the folder, given the handle */
	mapi_handles_get_private_data(folder, &folder_priv);
	folder_object = (struct emsmdbp_object *) folder_priv;
	if (!folder_object) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] EmptyFolder null object"));
		return MAPI_E_NO_SUPPORT;
	}

	if (folder_object->type != EMSMDBP_OBJECT_FOLDER) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] EmptyFolder wrong object type: 0x%x\n", folder_object->type));
		return MAPI_E_NO_SUPPORT;
	}
	context_id = emsmdbp_get_contextID(folder_object);

	local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	retval = mapistore_folder_get_child_fmids(emsmdbp_ctx->mstore_ctx, context_id, folder_object->backend_object, MAPISTORE_FOLDER_TABLE, local_mem_ctx,
						  &childFolders, &childFolderCount);
	if (retval) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] EmptyFolder bad retval: 0x%x", retval));
		ret = MAPI_E_NOT_FOUND;
		goto end;
	}

	/* Step 3. Delete contents of the folder in mapistore */
	for (i = 0; i < childFolderCount; ++i) {
		retval = mapistore_folder_open_folder(emsmdbp_ctx->mstore_ctx, context_id, folder, local_mem_ctx, childFolders[i], &subfolder);
		if (retval != MAPISTORE_SUCCESS) {
			ret = MAPI_E_NOT_FOUND;
			goto end;
		}

		retval = mapistore_folder_delete(emsmdbp_ctx->mstore_ctx, context_id, subfolder, flags);
		if (retval) {
			  DEBUG(4, ("exchange_emsmdb: [OXCFOLD] EmptyFolder failed to delete fid 0x%.16"PRIx64" (0x%x)", childFolders[i], retval));
			  ret = MAPI_E_NOT_FOUND;
			  goto end;
		}
	}

end:
	talloc_free(local_mem_ctx);

	return ret;
}

/**
   \details EcDoRpc EmptyFolder (0x58) Rop. This operation removes the sub-folders
   and messages from a given parent folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the EmptyFolder EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the EmptyFolder EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopEmptyFolder(TALLOC_CTX *mem_ctx,
                                                struct emsmdbp_context *emsmdbp_ctx,
                                                struct EcDoRpc_MAPI_REQ *mapi_req,
                                                struct EcDoRpc_MAPI_REPL *mapi_repl,
                                                uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS                 retval;
	struct mapi_handles             *folder = NULL;
        struct emsmdbp_object           *folder_object;
        void                            *private_data;
	bool                            mapistore = false;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] EmptyFolder (0x58)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->u.mapi_EmptyFolder.PartialCompletion = 0;

	/* Step 1. Retrieve folder handle */
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handles[mapi_req->handle_idx], &folder);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);
	mapi_handles_get_private_data(folder, &private_data);
        folder_object = private_data;

	mapistore = emsmdbp_is_mapistore(folder_object);
	switch (mapistore) {
	case false:
		/* system/special folder */
		DEBUG(0, ("TODO Empty system/special folder\n"));
#if 0
                retval = RopEmptyFolder_SystemSpecialFolder(mem_ctx, emsmdbp_ctx,
                                                           mapi_req->u.mapi_EmptyFolder,
                                                           &mapi_repl->u.mapi_EmptyFolder);
#endif
		retval = MAPI_E_SUCCESS; // TODO: temporary hack.
		mapi_repl->error_code = retval;
		break;
	case true:
		/* handled by mapistore */
		retval = RopEmptyFolder_GenericFolder(mem_ctx, emsmdbp_ctx,
                                                      mapi_req->u.mapi_EmptyFolder,
                                                      &mapi_repl->u.mapi_EmptyFolder,
                                                      folder);
		mapi_repl->error_code = retval;
		break;
	}

	*size += libmapiserver_RopEmptyFolder_size(mapi_repl);

	/* reply filled in above */

	return MAPI_E_SUCCESS;
}

/**

 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopMoveCopyMessages(TALLOC_CTX *mem_ctx,
						     struct emsmdbp_context *emsmdbp_ctx,
						     struct EcDoRpc_MAPI_REQ *mapi_req,
						     struct EcDoRpc_MAPI_REPL *mapi_repl,
						     uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	uint32_t		handle;
	uint32_t                contextID;
	struct mapi_handles	*rec = NULL;
	void			*private_data = NULL;
	struct emsmdbp_object	*destination_object;
	struct emsmdbp_object   *source_object;
	uint64_t                *targetMIDs;
        uint32_t                i;
	bool			mapistore = false;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] RopMoveCopyMessages (0x33)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	mapi_repl->u.mapi_MoveCopyMessages.PartialCompletion = 0;

	/* Get the destionation information */
	handle = handles[mapi_req->u.mapi_MoveCopyMessages.handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);

	/* object is our destination folder */
        destination_object = private_data;
	if (!destination_object) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  object (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}
	
	/* Get the source folder information */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);
        source_object = private_data;
	if (!source_object) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  object (%x) not found: %x\n", handle, mapi_req->u.mapi_MoveCopyMessages.handle_idx));
		goto end;
	}

	contextID = emsmdbp_get_contextID(destination_object);
	mapistore = emsmdbp_is_mapistore(source_object);
	if (mapistore) {
		/* We prepare a set of new MIDs for the backend */
		targetMIDs = talloc_array(NULL, uint64_t, mapi_req->u.mapi_MoveCopyMessages.count);
		for (i = 0; i < mapi_req->u.mapi_MoveCopyMessages.count; i++) {
			openchangedb_get_new_folderID(emsmdbp_ctx->oc_ctx, &targetMIDs[i]);
		}

		/* We invoke the backend method */
		mapistore_folder_move_copy_messages(emsmdbp_ctx->mstore_ctx, contextID, destination_object->backend_object, source_object->backend_object, mem_ctx, mapi_req->u.mapi_MoveCopyMessages.count, mapi_req->u.mapi_MoveCopyMessages.message_id, targetMIDs, NULL, mapi_req->u.mapi_MoveCopyMessages.WantCopy);
		talloc_free(targetMIDs);

		/* /\* The backend might do this for us. In any case, we try to add it ourselves *\/ */
		/* mapistore_indexing_record_add_mid(emsmdbp_ctx->mstore_ctx, contextID, targetMID); */
	}
	else {
		DEBUG(0, ("["__location__"] - mapistore support not implemented yet - shouldn't occur\n"));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
	}

end:
	*size += libmapiserver_RopMoveCopyMessages_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc EmptyFolder (0x58) Rop. This operation removes the sub-folders
   and messages from a given parent folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the EmptyFolder EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the EmptyFolder EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
enum MAPISTATUS EcDoRpc_RopMoveFolder(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct EcDoRpc_MAPI_REQ *mapi_req, struct EcDoRpc_MAPI_REPL *mapi_repl, uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	enum mapistore_error	ret;
	uint32_t		handle;
	struct mapi_handles	*handle_object;
	void			*private_data;
	struct MoveFolder_req	*request;
	struct MoveFolder_repl	*response;
	struct emsmdbp_object	*source_parent;
	struct emsmdbp_object	*move_folder;
	struct emsmdbp_object	*target_folder;

	DEBUG(4, ("exchange_emsmdb: [OXCSTOR] MoveFolder (0x35)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	request = &mapi_req->u.mapi_MoveFolder;
	response = &mapi_repl->u.mapi_MoveFolder;

	/* Retrieve the source parent handle in the hierarchy */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &handle_object);
	if (retval) {
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}
	mapi_handles_get_private_data(handle_object, &private_data);
        source_parent = private_data;
	if (!source_parent || source_parent->type != EMSMDBP_OBJECT_FOLDER) {
		DEBUG(5, ("  invalid handle (%x): %x\n", handle, mapi_req->handle_idx));
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	/* Open the folder being moved as it will be the actor object in this process */
	ret = emsmdbp_object_open_folder(mem_ctx, emsmdbp_ctx, source_parent, request->FolderId, &move_folder);
	if (ret != MAPISTORE_SUCCESS) {
		mapi_repl->error_code = mapistore_error_to_mapi(ret);
		goto end;
	}

	/* Retrieve the destination parent handle in the hierarchy */
	handle = handles[request->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &handle_object);
	if (retval) {
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}
	mapi_handles_get_private_data(handle_object, &private_data);
        target_folder = private_data;
	if (!target_folder || target_folder->type != EMSMDBP_OBJECT_FOLDER) {
		DEBUG(5, ("  invalid handle (%x): %x\n", handle, mapi_req->handle_idx));
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	ret = emsmdbp_folder_move_folder(emsmdbp_ctx, move_folder, target_folder, request->NewFolderName.lpszW);
	mapi_repl->error_code = mapistore_error_to_mapi(ret);
	response->PartialCompletion = false;

end:
	*size += libmapiserver_RopMoveFolder_size(mapi_repl);

	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EmptyFolder (0x58) Rop. This operation removes the sub-folders
   and messages from a given parent folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the EmptyFolder EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the EmptyFolder EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
enum MAPISTATUS EcDoRpc_RopCopyFolder(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct EcDoRpc_MAPI_REQ *mapi_req, struct EcDoRpc_MAPI_REPL *mapi_repl, uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	enum mapistore_error	ret;
	uint32_t		handle;
	struct mapi_handles	*handle_object;
	void			*private_data;
	struct CopyFolder_req	*request;
	struct CopyFolder_repl	*response;
	struct emsmdbp_object	*source_parent;
	struct emsmdbp_object	*copy_folder;
	struct emsmdbp_object	*target_folder;
	uint32_t		contextID;

	DEBUG(4, ("exchange_emsmdb: [OXCSTOR] CopyFolder (0x36)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	request = &mapi_req->u.mapi_CopyFolder;
	response = &mapi_repl->u.mapi_CopyFolder;

	/* Retrieve the source parent handle in the hierarchy */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &handle_object);
	if (retval) {
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}
	mapi_handles_get_private_data(handle_object, &private_data);
        source_parent = private_data;
	if (!source_parent || source_parent->type != EMSMDBP_OBJECT_FOLDER) {
		DEBUG(5, ("  invalid handle (%x): %x\n", handle, mapi_req->handle_idx));
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	/* Open the folder being copied as it will be the actor object in this process */
	ret = emsmdbp_object_open_folder(mem_ctx, emsmdbp_ctx, source_parent, request->FolderId, &copy_folder);
	if (ret != MAPISTORE_SUCCESS) {
		mapi_repl->error_code = mapistore_error_to_mapi(ret);
		goto end;
	}
	/* TODO: we should provide the ability to perform this operation between non-mapistore objects or between mapistore and non-mapistore objects */
	if (!emsmdbp_is_mapistore(copy_folder)) {
		mapi_repl->error_code = MAPI_E_NO_ACCESS;
		goto end;
	}

	/* Retrieve the destination parent handle in the hierarchy */
	handle = handles[request->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &handle_object);
	if (retval) {
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}
	mapi_handles_get_private_data(handle_object, &private_data);
        target_folder = private_data;
	if (!target_folder || target_folder->type != EMSMDBP_OBJECT_FOLDER) {
		DEBUG(5, ("  invalid handle (%x): %x\n", handle, mapi_req->handle_idx));
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}
	if (!emsmdbp_is_mapistore(target_folder)) {
		mapi_repl->error_code = MAPI_E_NO_ACCESS;
		goto end;
	}
	
	contextID = emsmdbp_get_contextID(copy_folder);
	ret = mapistore_folder_copy_folder(emsmdbp_ctx->mstore_ctx, contextID, copy_folder->backend_object, target_folder->backend_object, request->WantRecursive, request->NewFolderName.lpszW);
	mapi_repl->error_code = mapistore_error_to_mapi(ret);
	response->PartialCompletion = false;

end:
	*size += libmapiserver_RopCopyFolder_size(mapi_repl);

	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return MAPI_E_SUCCESS;
}
