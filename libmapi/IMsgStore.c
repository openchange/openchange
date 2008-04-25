/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2008.

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

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <gen_ndr/ndr_exchange.h>


/**
   \file IMsgStore.c

   \brief Folders related operations
 */


/**
   \details Open a folder from the store
 
   \param obj_store the store to open a folder in (i.e. the parent)
   \param id_folder the folder identifier
   \param obj_folder the resulting open folder

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa MAPIInitialize, OpenMsgStore, GetLastError
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
	request.OpenModeFlags = OpenModeFlags_Folder;
	size += sizeof (uint8_t) + sizeof(uint64_t) + sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenFolder;
	mapi_req->logon_id = 0;
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
   \details Open a NNTP Public Folder given its name

   \param obj_folder the parent folder
   \param obj_child the resulting open folder
   \param name the folder name

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenPublicFolder
 */
_PUBLIC_ enum MAPISTATUS OpenPublicFolderByName(mapi_object_t *obj_folder,
						mapi_object_t *obj_child,
						const char *name)
{
	struct mapi_request			*mapi_request;
	struct mapi_response			*mapi_response;
	struct EcDoRpc_MAPI_REQ			*mapi_req;
	struct OpenPublicFolderByName_req	request;
	NTSTATUS				status;
	enum MAPISTATUS				retval;
	uint32_t				size = 0;
	TALLOC_CTX				*mem_ctx;
	mapi_ctx_t				*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!obj_child, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!name, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("OpenPublicFolderByName");
	size = 0;

	/* Fill the OpenPublicFolderByName operation */
	request.handle_idx = 0x1;
	size += sizeof (uint8_t);

	/* name is prefixed with 32 bit [size] */
	request.name = name;
	size += strlen(name) + 1 + sizeof (uint32_t);
	
	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenPublicFolderByName;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_OpenPublicFolderByName = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_folder);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	mapi_object_set_handle(obj_child, mapi_response->handles[1]);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Sets a folder as the destination for incoming messages of
   a particular message class.

   \param obj_store the store to set the receive folder for
   \param obj_folder the destination folder
   \param lpszMessageClass the message class the folder will receive

      \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa GetReceiveFolder, GetReceiveFolderTable
 */
_PUBLIC_ enum MAPISTATUS SetReceiveFolder(mapi_object_t *obj_store,
					  mapi_object_t *obj_folder,
					  const char *lpszMessageClass)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SetReceiveFolder_req	request;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	mapi_ctx_t			*mapi_ctx;

	/* Sanity Checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!lpszMessageClass, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("SetReceiveFolder");

	/* Fill the SetReceiveFolder operation */
	size = 0;
	request.fid = mapi_object_get_id(obj_folder);
	size += sizeof (uint64_t);
	request.lpszMessageClass = lpszMessageClass;
	size += strlen(lpszMessageClass) + 1;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetReceiveFolder;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SetReceiveFolder = request;
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

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieves the receive folder (INBOX) for a given store

   This function obtains the folder that was established as the
   destination for incoming messages of a specified message class, or
   the default receive folder for the message store.

   \param obj_store the store to get the receiver folder for
   \param id_folder the resulting folder identification

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa MAPIInitialize, OpenMsgStore, GetLastError, SetReceiveFolder,
   GetReceiveFolderTable
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
	mapi_req->logon_id = 0;
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
   \details Retrieve the receive folder table which includes all the
   information about the receive folders for the message store

   \param obj_store the message store object
   \param SRowSet pointer on a SRowSet structure with
   GetReceiveFolderTable results.

   Developers are required to call MAPIFreeBuffer(SRowSet.aRow) when
   they don't need the folder table data anymore.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetReceiveFolder, SetReceiveFolder
 */
_PUBLIC_ enum MAPISTATUS GetReceiveFolderTable(mapi_object_t *obj_store, 
					       struct SRowSet *SRowSet)
{
	struct mapi_request			*mapi_request;
	struct mapi_response			*mapi_response;
	struct EcDoRpc_MAPI_REQ			*mapi_req;
	struct GetReceiveFolderTable_repl	*reply;
	NTSTATUS				status;
	enum MAPISTATUS				retval;
	uint32_t				size = 0;
	TALLOC_CTX				*mem_ctx;
	mapi_ctx_t				*mapi_ctx;
	uint32_t				i;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("GetReceiveFolderTable");

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetReceiveFolderTable;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
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

	reply = &mapi_response->mapi_repl->u.mapi_GetReceiveFolderTable;

	/* Retrieve the ReceiveFolderTable entries */
	SRowSet->cRows = reply->cValues;
	SRowSet->aRow = talloc_array((TALLOC_CTX *)mapi_ctx->session, struct SRow, reply->cValues);
	
	for (i = 0; i < reply->cValues; i++) {
		SRowSet->aRow[i].ulAdrEntryPad = 0;
		SRowSet->aRow[i].cValues = 3;
		SRowSet->aRow[i].lpProps = talloc_array((TALLOC_CTX *)SRowSet->aRow, struct SPropValue, 
							SRowSet->aRow[i].cValues);

		SRowSet->aRow[i].lpProps[0].ulPropTag = PR_FID;
		SRowSet->aRow[i].lpProps[0].dwAlignPad = 0x0;
		SRowSet->aRow[i].lpProps[0].value.d = reply->entries[i].fid;

		SRowSet->aRow[i].lpProps[1].ulPropTag = PR_MESSAGE_CLASS;
		SRowSet->aRow[i].lpProps[1].dwAlignPad = 0x0;
		SRowSet->aRow[i].lpProps[1].value.lpszA = talloc_strdup((TALLOC_CTX *)SRowSet->aRow, reply->entries[i].lpszMessageClass);
	
		SRowSet->aRow[i].lpProps[2].ulPropTag = PR_LAST_MODIFICATION_TIME;
		SRowSet->aRow[i].lpProps[2].dwAlignPad = 0x0;
		SRowSet->aRow[i].lpProps[2].value.ft.dwLowDateTime = reply->entries[i].modiftime.dwLowDateTime;
		SRowSet->aRow[i].lpProps[2].value.ft.dwHighDateTime = reply->entries[i].modiftime.dwHighDateTime;
	}

	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}


/**
   \details Retrieves the sending folder (OUTBOX) for a given store

   This function obtains the folder that was established as the
   destination for outgoing messages of a specified message class.

   This function does not result in any network traffic.

   \param obj_store the store to get the outbox folder for
   \param outbox_id the resulting folder identification

   \return MAPI_E_SUCCESS on success, otherwise -1.
   
   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa MAPIInitialize, OpenMsgStore, GetLastError, GetDefaultFolder
*/
_PUBLIC_ enum MAPISTATUS GetOutboxFolder(mapi_object_t *obj_store, 
					 mapi_id_t *outbox_id)
{
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	*outbox_id = ((mapi_object_store_t *)obj_store->private_data)->fid_outbox;
	return MAPI_E_SUCCESS;
}
