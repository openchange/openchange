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


static enum MAPISTATUS RopOpenFolder_GenericFolder(TALLOC_CTX *mem_ctx,
						   struct emsmdbp_context *emsmdbp_ctx,
						   struct OpenFolder_req request,
						   struct OpenFolder_repl *response,
						   struct mapi_handles *parent)
{
	struct emsmdbp_object	*parent_object = NULL;
	void			*data;
	uint64_t		parent_fid;
	int			retval;
	uint32_t		context_id;

	/* Step 1. Retrieve the parent fid given the handle */
	mapi_handles_get_private_data(parent, &data);
	parent_object = (struct emsmdbp_object *) data;
	if (!parent_object) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] OpenFolder null object"));
		return MAPI_E_NO_SUPPORT;
	}

	if (parent_object->type != EMSMDBP_OBJECT_FOLDER) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] OpenFolder wrong object type: 0x%x\n", parent_object->type));
		return MAPI_E_NO_SUPPORT;
	}
	parent_fid = parent_object->object.folder->folderID;
	context_id = parent_object->object.folder->contextID;

	/* Step 2. Open folder from mapistore */
	retval = mapistore_opendir(emsmdbp_ctx->mstore_ctx, context_id, parent_fid, request.folder_id);
	if (retval) return MAPI_E_NOT_FOUND;

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
	struct mapi_handles		*parent = NULL;
	struct mapi_handles		*rec = NULL;
	struct emsmdbp_object		*object;
	uint32_t			handle;
	bool				mapistore = false;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] OpenFolder (0x02)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->u.mapi_OpenFolder.HasRules = 0;
	mapi_repl->u.mapi_OpenFolder.IsGhosted = 0;

	/* Step 1. Retrieve parent handle in the hierarchy */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	mapistore = emsmdbp_is_mapistore(parent);
	switch (mapistore) {
	case false:
		/* system/special folder */
		DEBUG(0, ("Opening system/special folder\n"));
		retval = RopOpenFolder_SystemSpecialFolder(mem_ctx, emsmdbp_ctx, 
							   mapi_req->u.mapi_OpenFolder, 
							   &mapi_repl->u.mapi_OpenFolder);
		mapi_repl->error_code = retval;
		break;
	case true:
		/* handled by mapistore */
		DEBUG(0, ("Opening Generic folder\n"));
		retval = RopOpenFolder_GenericFolder(mem_ctx, emsmdbp_ctx, 
						     mapi_req->u.mapi_OpenFolder,
						     &mapi_repl->u.mapi_OpenFolder, parent);
		mapi_repl->error_code = retval;
		break;
	}

	*size += libmapiserver_RopOpenFolder_size(mapi_repl);

	/* Fill EcDoRpc_MAPI_REPL reply */
	if (!mapi_repl->error_code) {
		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);

		object = emsmdbp_object_folder_init((TALLOC_CTX *)emsmdbp_ctx, emsmdbp_ctx, 
						    mapi_req->u.mapi_OpenFolder.folder_id, parent);
		if (object) {
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
	uint64_t		folderID;
	uint32_t		contextID;
	uint32_t		handle;
	bool			mapistore = false;

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

	/* Initialize default empty GetHierarchyTable reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->u.mapi_GetHierarchyTable.handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* GetHierarchyTable can only be called for mailbox/folder objects */
	mapi_handles_get_private_data(parent, &data);
	object = (struct emsmdbp_object *)data;
	if (!object) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		*size = libmapiserver_RopGetHierarchyTable_size(NULL);
		return MAPI_E_SUCCESS;
	}

	switch (object->type) {
	case EMSMDBP_OBJECT_MAILBOX:
		folderID = object->object.mailbox->folderID;
		contextID = object->object.folder->contextID;
		break;
	case EMSMDBP_OBJECT_FOLDER:
		folderID = object->object.folder->folderID;
		contextID = object->object.folder->contextID;
		break;
	default:
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		*size = libmapiserver_RopGetHierarchyTable_size(NULL);
		return MAPI_E_SUCCESS;
	}

	mapistore = emsmdbp_is_mapistore(parent);
	switch (mapistore) {
	case false:
		/* system/special folder */
		retval = openchangedb_get_folder_count(emsmdbp_ctx->oc_ctx, folderID, 
						       &mapi_repl->u.mapi_GetHierarchyTable.RowCount);
		break;
	case true:
		/* handled by mapistore */
		retval = mapistore_get_folder_count(emsmdbp_ctx->mstore_ctx, contextID, folderID, 
						    &mapi_repl->u.mapi_GetHierarchyTable.RowCount);
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
		object->object.table->ulType = EMSMDBP_TABLE_FOLDER_TYPE;
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
	struct mapi_handles	*parent;
	struct mapi_handles	*rec = NULL;
	struct emsmdbp_object	*object = NULL;
	void			*data;
	uint64_t		folderID;
	uint32_t		contextID;
	uint32_t		handle;
	bool			mapistore = false;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] GetContentsTable (0x05)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Initialize default empty GetContentsTable reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->u.mapi_GetContentsTable.handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_GetContentsTable.RowCount = 0;

	/* GetContentsTable can only be called for folder objects */
	mapi_handles_get_private_data(parent, &data);
	object = (struct emsmdbp_object *)data;
	if (!object) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		*size = libmapiserver_RopGetContentsTable_size(NULL);
		return MAPI_E_SUCCESS;
	}

	switch (object->type) {
	case EMSMDBP_OBJECT_FOLDER:
		folderID = object->object.folder->folderID;
		contextID = object->object.folder->contextID;
		break;
	default:
		mapi_repl->u.mapi_GetContentsTable.RowCount = 0;
		*size = libmapiserver_RopGetContentsTable_size(NULL);
		return MAPI_E_SUCCESS;
	}

	mapistore = emsmdbp_is_mapistore(parent);
	switch (mapistore) {
	case false:
		/* system/special folder */
		mapi_repl->u.mapi_GetContentsTable.RowCount = 0;
		break;
	case true:
		/* handled by mapistore */
		retval = mapistore_get_message_count(emsmdbp_ctx->mstore_ctx, contextID, folderID,
						     &mapi_repl->u.mapi_GetContentsTable.RowCount);
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
		object->object.table->ulType = EMSMDBP_TABLE_MESSAGE_TYPE;
	}
	
	*size += libmapiserver_RopGetContentsTable_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


static enum MAPISTATUS EcDoRpc_RopCreateSystemSpecialFolder(struct emsmdbp_context *emsmdbp_ctx,
							    struct SRow *aRow, 
							    enum FOLDER_FLAGS folderFlags,
							    uint64_t parentFolder,
							    struct CreateFolder_repl *response)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct ldb_message              *msg;
	struct ldb_dn                   *basedn;
	char                            *dn;
	char				*parentfid;
	int				ret = 0;
	char				*displayName;
	char				*comment;
	uint32_t			*folderType;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateSystemSpecialFolder\n"));

	displayName = (char *) find_SPropValue_data(aRow, PR_DISPLAY_NAME);
	if (!displayName) {
		displayName = (char *) find_SPropValue_data(aRow, PR_DISPLAY_NAME_UNICODE);
	}

	/* Step 0. Determine if the folder already exists */
	if (openchangedb_get_fid_by_name(emsmdbp_ctx->oc_ctx, parentFolder,
					 displayName, &response->folder_id) == MAPI_E_SUCCESS) {
		/* this folder already exists */
		if ( folderFlags & OPEN_IF_EXISTS ) {
		  	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder Duplicate Folder\n"));
			response->IsExistingFolder = true;
			return MAPI_E_SUCCESS;
		} else {
			DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder Duplicate Folder error\n"));
			return MAPI_E_COLLISION;
		}
	}

	mem_ctx = talloc_named(NULL, 0, "RopCreateSystemSpecialFolder");

	/* Step 1. Retrieve the next available folderID */
	retval = openchangedb_get_new_folderID(emsmdbp_ctx->oc_ctx, &response->folder_id);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Retrieve dn of parentfolder */
	retval = openchangedb_get_distinguishedName(mem_ctx, emsmdbp_ctx->oc_ctx, parentFolder, &parentfid);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Step 2. Create the folder LDB record for openchange.ldb */
	dn = talloc_asprintf(mem_ctx, "CN=%016"PRIx64",%s", response->folder_id, parentfid);

	/* Ensure dn is within user mailbox / prevent from creating
	 * folders in other mailboxes: check dn vs emsmdbp_ctx->username */

	basedn = ldb_dn_new(mem_ctx, emsmdbp_ctx->oc_ctx, dn);
	talloc_free(dn);
	OPENCHANGE_RETVAL_IF(!ldb_dn_validate(basedn), MAPI_E_BAD_VALUE, mem_ctx);
	
	msg = ldb_msg_new(mem_ctx);
	msg->dn = ldb_dn_copy(mem_ctx, basedn);
	ldb_msg_add_string(msg, "objectClass", "systemfolder");
	ldb_msg_add_fmt(msg, "cn", "0x%.16"PRIx64, response->folder_id);
	ldb_msg_add_string(msg, "PidTagContentUnreadCount", "0");
	ldb_msg_add_string(msg, "PidTagContentCount", "0");
	ldb_msg_add_string(msg, "PidTagContainerClass", "IPF.Note");
	ldb_msg_add_string(msg, "PidTagAttrHidden", "0");
	ldb_msg_add_string(msg, "PidTagDisplayName", displayName);

	folderType = (uint32_t *) find_SPropValue_data(aRow, PR_FOLDER_TYPE);
	ldb_msg_add_fmt(msg, "PidTagFolderType", "%d", *folderType);

	comment = (char *) find_SPropValue_data(aRow, PR_COMMENT);
	if (!comment) {
		comment = (char *) find_SPropValue_data(aRow, PR_COMMENT_UNICODE);
	}
	ldb_msg_add_string(msg, "PidTagComment", comment);

	ldb_msg_add_fmt(msg, "PidTagParentFolderId", "0x%.16"PRIx64, parentFolder);
	ldb_msg_add_fmt(msg, "PidTagFolderId", "0x%.16"PRIx64, response->folder_id);
	ldb_msg_add_fmt(msg, "mapistore_uri", "fsocpf:///usr/local/samba/private/mapistore/%s/0x%.16"PRIx64, 
			emsmdbp_ctx->username, response->folder_id);
	ldb_msg_add_string(msg, "PidTagSubFolders", "0");
	ldb_msg_add_string(msg, "FolderType", "1");
	ldb_msg_add_fmt(msg, "distinguishedName", "%s", ldb_dn_get_linearized(msg->dn));

	msg->elements[0].flags = LDB_FLAG_MOD_ADD;

	ret = ldb_add(emsmdbp_ctx->oc_ctx, msg);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(parentfid);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


static enum MAPISTATUS EcDoRpc_RopCreateGenericFolder(struct emsmdbp_context *emsmdbp_ctx,
						      struct mapi_handles *parent,
						      struct SRow *aRow,
						      enum FOLDER_FLAGS folderFlags,
						      struct CreateFolder_repl *response)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	int			ret;
	struct ldb_result	*res = NULL;
	struct ldb_message	*msg;
	
	struct ldb_dn		*ldb_dn;
	struct emsmdbp_object	*parent_object = NULL;
	const char * const	attrs[] = { "*", NULL };
	void			*data;
	uint64_t		parent_fid;
	uint32_t		context_id;
	char			*parentfid;
	int			count;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateGenericFolder\n"));

	/* Step 1. Retrieve the parent fid given the handle */
	mapi_handles_get_private_data(parent, &data);
	parent_object = (struct emsmdbp_object *) data;
	/* checks are already done in upper function / code factorization required */

	parent_fid = parent_object->object.folder->folderID;
	context_id = parent_object->object.folder->contextID;

	/* Step 0. Determine if the folder already exists */

	/* Step 1. Retrieve the next available folderID */
	retval = openchangedb_get_new_folderID(emsmdbp_ctx->oc_ctx, &response->folder_id);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Step 2. Create folder in mapistore */
	retval = mapistore_mkdir(emsmdbp_ctx->mstore_ctx, context_id, parent_fid, response->folder_id, 
				 aRow);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Step 3. Update openchangedb record if needed */
	if (parent_object->type == EMSMDBP_OBJECT_FOLDER && parent_object->object.folder->mapistore_root == true) {
		mem_ctx = talloc_named(NULL, 0, "RopCreateGenericFolder");

		/* Retrieve previous value */
		ret = ldb_search(emsmdbp_ctx->oc_ctx, mem_ctx, &res, ldb_get_default_basedn(emsmdbp_ctx->oc_ctx),
				 LDB_SCOPE_SUBTREE, attrs, "PidTagFolderId=0x%.16"PRIx64, parent_fid);
		OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

		count = ldb_msg_find_attr_as_int(res->msgs[0], "PidTagFolderChildCount", 0);
		
		/* Update record */
		retval = openchangedb_get_distinguishedName(mem_ctx, emsmdbp_ctx->oc_ctx, parent_fid, &parentfid);
		OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

		ldb_dn = ldb_dn_new(mem_ctx, emsmdbp_ctx->oc_ctx, parentfid);
		OPENCHANGE_RETVAL_IF(!ldb_dn_validate(ldb_dn), MAPI_E_BAD_VALUE, mem_ctx);

		msg = ldb_msg_new(mem_ctx);
		msg->dn = ldb_dn_copy(mem_ctx, ldb_dn);
		ldb_msg_add_fmt(msg, "PidTagFolderChildCount", "%d", count + 1);
		ldb_msg_add_fmt(msg, "PidTagSubFolders", "TRUE");
		msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;
		msg->elements[1].flags = LDB_FLAG_MOD_REPLACE;

		ret = ldb_modify(emsmdbp_ctx->oc_ctx, msg);
		OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

		talloc_free(mem_ctx);
	}

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
	struct mapi_handles		*parent = NULL;
	uint32_t			handle;
	uint64_t			parent_fid;
	struct emsmdbp_object		*parent_object = NULL;
	struct emsmdbp_object		*object = NULL;
	struct SRow			*aRow;
	void				*data;
	struct mapi_handles		*rec = NULL;
	bool				mapistore = false;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder (0x1c)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->u.mapi_CreateFolder.IsExistingFolder = false;

	mapi_repl->opnum = mapi_req->opnum;

	/* Step 1. Retrieve parent handle in the hierarchy */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	mapi_handles_get_private_data(parent, &data);
	parent_object = (struct emsmdbp_object *)data;
	if (!parent_object) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder null object\n"));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		return MAPI_E_SUCCESS;
	}

	if (parent_object->type != EMSMDBP_OBJECT_FOLDER) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder wrong object type: 0x%x\n", parent_object->type));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		return MAPI_E_SUCCESS;
	}
	parent_fid = parent_object->object.folder->folderID;
	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder parent: 0x%"PRIx64"\n", parent_fid));
	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] Creating %s\n", mapi_req->u.mapi_CreateFolder.FolderName.lpszA));
	
	/* Step 3. Turn CreateFolder parameters into MAPI property array */
	aRow = libmapiserver_ROP_request_to_properties(mem_ctx, (void *)&mapi_req->u.mapi_CreateFolder, op_MAPI_CreateFolder);
	aRow->lpProps = add_SPropValue(mem_ctx, aRow->lpProps, &(aRow->cValues), PR_PARENT_FID, (void *)(&parent_fid));

	/* Step 4. Do effective work here */
	mapistore = emsmdbp_is_mapistore(parent);
	switch (mapistore) {
	case false:
		switch (mapi_req->u.mapi_CreateFolder.ulFolderType) {
		case FOLDER_GENERIC:
			mapi_repl->error_code = EcDoRpc_RopCreateSystemSpecialFolder(emsmdbp_ctx, aRow,
										     mapi_req->u.mapi_CreateFolder.ulFlags,
										     parent_fid, &mapi_repl->u.mapi_CreateFolder);
			break;
		case FOLDER_SEARCH:
			DEBUG(4, ("exchange_emsmdb: [OXCFOLD] FOLDER_SEARCH not implemented\n"));
			break;
		default:
			DEBUG(4, ("exchange_emsmdb: [OXCFOLD] Unexpected folder type 0x%x\n", mapi_req->u.mapi_CreateFolder.ulType));
		}
		break;
	case true:
		mapi_repl->error_code = EcDoRpc_RopCreateGenericFolder(emsmdbp_ctx, parent, aRow,
								       mapi_req->u.mapi_CreateFolder.ulFlags,
								       &mapi_repl->u.mapi_CreateFolder);
		break;
	}

	mapi_repl->handle_idx = mapi_req->u.mapi_CreateFolder.handle_idx;

	if (mapi_repl->u.mapi_CreateFolder.IsExistingFolder == true) {
		mapi_repl->u.mapi_CreateFolder.GhostUnion.GhostInfo.HasRules = false;
		mapi_repl->u.mapi_CreateFolder.GhostUnion.GhostInfo.IsGhosted = false;
	}

	if (!mapi_repl->error_code) {
		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
		object = emsmdbp_object_folder_init((TALLOC_CTX *)rec, emsmdbp_ctx, 
						    mapi_repl->u.mapi_CreateFolder.folder_id, parent);
		if (object) {
			retval = mapi_handles_set_private_data(rec, object);
		}

		mapi_repl->opnum = mapi_req->opnum;
		mapi_repl->handle_idx = mapi_req->u.mapi_CreateFolder.handle_idx;

		handles[mapi_repl->handle_idx] = rec->handle;
	}

	*size += libmapiserver_RopCreateFolder_size(mapi_repl);

	if (aRow) {
		talloc_free(aRow);
	}

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS DoDeleteFolder(struct emsmdbp_context *emsmdbp_ctx,
				      uint64_t parent_fid, uint64_t fid,
				      uint8_t flags)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_dn			*dn;
	char				*dn_str;
	int				ret = 0;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] DeleteFolder parent FID: 0x%"PRIx64"\n", parent_fid));
	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] DeleteFolder target FID: 0x%"PRIx64"\n", fid));

	mem_ctx = talloc_named(NULL, 0, "DoDeleteFolder");

	/* TODO:
		1. This only works on top level folders.
		2. We should be careful not to delete special folders
		3. We need to handle deleting associated folders and messages (based on the flags)
	*/
	dn_str = talloc_asprintf(mem_ctx, "CN=0x%016"PRIx64",CN=0x%016"PRIx64",CN=%s,%s",
				 fid, parent_fid, emsmdbp_ctx->username,
			         ldb_dn_get_linearized(ldb_get_root_basedn(emsmdbp_ctx->oc_ctx)));
	dn = ldb_dn_new(mem_ctx, emsmdbp_ctx->oc_ctx, dn_str);
	talloc_free(dn_str);
	OPENCHANGE_RETVAL_IF(!ldb_dn_validate(dn), MAPI_E_BAD_VALUE, mem_ctx);

	ret = ldb_delete(emsmdbp_ctx->oc_ctx, dn);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(mem_ctx);
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
	struct mapi_handles	*rec = NULL;
	uint32_t		handle;
	void			*handle_priv_data;
	struct emsmdbp_object	*handle_object = NULL;
	uint64_t		parent_fid = 0;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] DeleteFolder (0x1d)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

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
	parent_fid = handle_object->object.folder->folderID;

	/* Initialize default empty DeleteFolder reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->u.mapi_DeleteFolder.PartialCompletion = false;

	mapi_repl->error_code = DoDeleteFolder(emsmdbp_ctx, parent_fid,
					       mapi_req->u.mapi_DeleteFolder.FolderId,
					       mapi_req->u.mapi_DeleteFolder.DeleteFolderFlags);

	*size += libmapiserver_RopDeleteFolder_size(mapi_repl);

	return retval;
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
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopDeleteMessage(TALLOC_CTX *mem_ctx,
						  struct emsmdbp_context *emsmdbp_ctx,
						  struct EcDoRpc_MAPI_REQ *mapi_req,
						  struct EcDoRpc_MAPI_REPL *mapi_repl,
						  uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] DeleteMessage (0x1e)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* TODO: actually implement this */

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
	struct mapi_SRestriction *res;

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

	res = NULL;
	mapi_repl->u.mapi_GetSearchCriteria.RestrictionDataSize = 0;
	mapi_repl->u.mapi_GetSearchCriteria.LogonId = mapi_req->logon_id;
	mapi_repl->u.mapi_GetSearchCriteria.FolderIdCount = 0;
	mapi_repl->u.mapi_GetSearchCriteria.FolderIds = NULL;
	mapi_repl->u.mapi_GetSearchCriteria.SearchFlags = 0;

	/* TODO: actually implement this */

	*size += libmapiserver_RopGetSearchCriteria_size(mapi_repl);

	return MAPI_E_SUCCESS;
}
