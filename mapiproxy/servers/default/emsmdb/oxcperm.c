/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2010

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
   \file oxcperm.c

   \brief Access and Operation Permissions Rops
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"


/**
   \details EcDoRpc GetPermissionsTable (0x3e) Rop. This operation get
   the permissions table of a folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetPermissionsTable EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the GetPermissionsTable
   EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetPermissionsTable(TALLOC_CTX *mem_ctx,
							struct emsmdbp_context *emsmdbp_ctx,
							struct EcDoRpc_MAPI_REQ *mapi_req,
							struct EcDoRpc_MAPI_REPL *mapi_repl,
							uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	struct mapi_handles	*parent;
	struct mapi_handles	*rec;
	struct emsmdbp_object	*parent_object, *object;
	void			*data = NULL;
	uint32_t		handle;

	OC_DEBUG(4, "exchange_emsmdb: [OXCPERM] GetPermissionsTable (0x3e)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->u.mapi_GetPermissionsTable.handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* Ensure parent handle references a folder object */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	/* Check we have a logon user */
	if (!emsmdbp_ctx->logon_user) {
		mapi_repl->error_code = MAPI_E_LOGON_FAILED;
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval || !data) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		OC_DEBUG(5, "  handle data not found, idx = %x\n", mapi_req->handle_idx);
		goto end;
	}

	parent_object = (struct emsmdbp_object *) data;
	if (parent_object->type != EMSMDBP_OBJECT_FOLDER) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  unhandled object type: %d\n", parent_object->type);
		goto end;
	}

	/* Initialize Table object */
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
	handles[mapi_repl->handle_idx] = rec->handle;
	
	if (emsmdbp_is_mapistore(parent_object)) {
		object = emsmdbp_folder_open_table(rec, parent_object, MAPISTORE_PERMISSIONS_TABLE, mapi_repl->handle_idx);
	}
	else {
		object = emsmdbp_object_table_init((TALLOC_CTX *)rec, emsmdbp_ctx, parent_object);
	}
	if (object) {
		retval = mapi_handles_set_private_data(rec, object);
	}
	else {
		mapi_handles_delete(emsmdbp_ctx->handles_ctx, rec->handle);
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

end:
	*size += libmapiserver_RopGetPermissionsTable_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc ModifyPermissions (0x40) Rop. This operation
   gets the GUID of a public folder's per-user information.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetPerUserLongTermIds EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the GetPerUserLongTermIds EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopModifyPermissions(TALLOC_CTX *mem_ctx,
						      struct emsmdbp_context *emsmdbp_ctx,
						      struct EcDoRpc_MAPI_REQ *mapi_req,
						      struct EcDoRpc_MAPI_REPL *mapi_repl,
						      uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	enum mapistore_error		mretval;
	struct mapi_handles		*folder;
	struct emsmdbp_object		*folder_object;
	void				*data = NULL;
	uint32_t			handle;
	struct ModifyPermissions_req	*request;


	OC_DEBUG(4, "exchange_emsmdb: [OXCSTOR] ModifyPermissions (0x40)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* Ensure handle references a folder object */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &folder);
	if (retval) {
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	/* Check we have a logon user */
	if (!emsmdbp_ctx->logon_user) {
		mapi_repl->error_code = MAPI_E_LOGON_FAILED;
		goto end;
	}

	retval = mapi_handles_get_private_data(folder, &data);
	if (retval || !data) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  handle data not found, idx = %x\n", mapi_req->handle_idx);
		goto end;
	}

	folder_object = (struct emsmdbp_object *) data;
	if (folder_object->type != EMSMDBP_OBJECT_FOLDER) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  unhandled object type: %d\n", folder_object->type);
		goto end;
	}

	request = &mapi_req->u.mapi_ModifyPermissions;

	if (emsmdbp_is_mapistore(folder_object)) {
		mretval = mapistore_folder_modify_permissions(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(folder_object),
							      folder_object->backend_object, request->rowList.ModifyFlags,
							      request->rowList.ModifyCount, request->rowList.PermissionsData);
		if (mretval != MAPISTORE_SUCCESS) {
			OC_DEBUG(5, "mapistore_folder_modify_permissions: %s\n", mapistore_errstr(mretval));
			mapi_repl->error_code = mapistore_error_to_mapi(mretval);
		}
	}
	else {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
	}

end:
	*size += libmapiserver_RopModifyPermissions_size(mapi_repl);

	return MAPI_E_SUCCESS;
}
