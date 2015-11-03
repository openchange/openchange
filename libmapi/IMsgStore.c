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

#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"


/**
   \file IMsgStore.c

   \brief Folders related operations
 */


/**
   \details Open a folder from the store
 
   \param obj_store the store to open a folder in (i.e. the parent)
   \param id_folder the folder identifier
   \param obj_folder the resulting open folder

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa MAPIInitialize, OpenMsgStore, GetLastError
 */
_PUBLIC_ enum MAPISTATUS OpenFolder(mapi_object_t *obj_store, 
				    mapi_id_t id_folder,
				    mapi_object_t *obj_folder)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct OpenFolder_req	request;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	uint8_t			logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "OpenFolder");

	/* Fill the OpenFolder operation */
	request.handle_idx = 0x1;
	request.folder_id = id_folder;
	request.OpenModeFlags = OpenModeFlags_Folder;
	size += sizeof (uint8_t) + sizeof(uint64_t) + sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenFolder;
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);	

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Set object session, id and handle */
	mapi_object_set_session(obj_folder, session);
	mapi_object_set_id(obj_folder, id_folder);
	mapi_object_set_handle(obj_folder, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_folder, logon_id);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Determine if a public folder is ghosted.

   This function returns whether a public folder is ghosted or not.

   \param obj_store the store of the public folder
   \param obj_folder the folder we are querying for ghost
   \param IsGhosted pointer on the boolean value returned

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
*/
_PUBLIC_ enum MAPISTATUS PublicFolderIsGhosted(mapi_object_t *obj_store,
					       mapi_object_t *obj_folder,
					       bool *IsGhosted)
{
	struct mapi_request			*mapi_request;
	struct mapi_response			*mapi_response;
	struct EcDoRpc_MAPI_REQ			*mapi_req;
	struct PublicFolderIsGhosted_req	request;
	struct mapi_session			*session[2];
	NTSTATUS				status;
	enum MAPISTATUS				retval;
	uint32_t				size = 0;
	TALLOC_CTX				*mem_ctx;
	mapi_id_t				folderId;
	uint8_t					logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	
	session[0] = mapi_object_get_session(obj_store);
	session[1] = mapi_object_get_session(obj_folder);
	OPENCHANGE_RETVAL_IF(!session[0], MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!session[1], MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(session[0] != session[1], MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	folderId = mapi_object_get_id(obj_folder);
	OPENCHANGE_RETVAL_IF(!folderId, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(session[0], 0, "PublicFolderIsGhosted");
	size = 0;

	/* Fill the PublicFolderIsGhosted operation */
	request.FolderId = folderId;
	size += sizeof (uint64_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_PublicFolderIsGhosted;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_PublicFolderIsGhosted = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_store);

	status = emsmdb_transaction_wrapper(session[0], mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session[0], mapi_response);

	*IsGhosted = mapi_response->mapi_repl->u.mapi_PublicFolderIsGhosted.IsGhosted;
	
	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}


/**
   \details Open a NNTP Public Folder given its name

   \param obj_folder the parent folder
   \param obj_child the resulting open folder
   \param name the folder name

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
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
	struct mapi_session			*session;
	NTSTATUS				status;
	enum MAPISTATUS				retval;
	uint32_t				size = 0;
	TALLOC_CTX				*mem_ctx;
	uint8_t					logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_child, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!name, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_folder);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_folder, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "OpenPublicFolderByName");
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
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Set object session and handle */
	mapi_object_set_session(obj_child, session);
	mapi_object_set_handle(obj_child, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_child, logon_id);

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

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
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
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!lpszMessageClass, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "SetReceiveFolder");

	/* Fill the SetReceiveFolder operation */
	size = 0;
	request.fid = mapi_object_get_id(obj_folder);
	size += sizeof (uint64_t);
	request.lpszMessageClass = lpszMessageClass;
	size += strlen(lpszMessageClass) + 1;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetReceiveFolder;
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Gets the receive folder for incoming messages of a
   particular message class.

   This function obtains the folder that was established as the
   destination for incoming messages of a specified message class, or
   the default receive folder for the message store.

   \param obj_store the store to get the receiver folder for
   \param id_folder the resulting folder identification
   \param MessageClass which message class to find the receivefolder
   for

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa MAPIInitialize, OpenMsgStore, GetLastError, SetReceiveFolder,
   GetReceiveFolderTable
*/
_PUBLIC_ enum MAPISTATUS GetReceiveFolder(mapi_object_t *obj_store, 
					  mapi_id_t *id_folder,
					  const char *MessageClass)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetReceiveFolder_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "GetReceiveFolder");

	*id_folder = 0;

	/* Fill the GetReceiveFolder operation */
	if (!MessageClass) {
		request.MessageClass = "";
		size += 1;
	} else {
		request.MessageClass = MessageClass;
		size += strlen (MessageClass) + 1;
	}

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetReceiveFolder;
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

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

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
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
	struct mapi_session			*session;
	NTSTATUS				status;
	enum MAPISTATUS				retval;
	uint32_t				size = 0;
	TALLOC_CTX				*mem_ctx;
	uint32_t				i;
	uint8_t					logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "GetReceiveFolderTable");
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetReceiveFolderTable;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_store);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	reply = &mapi_response->mapi_repl->u.mapi_GetReceiveFolderTable;

	/* Retrieve the ReceiveFolderTable entries */
	SRowSet->cRows = reply->cValues;
	SRowSet->aRow = talloc_array((TALLOC_CTX *)session, struct SRow, reply->cValues);
	
	for (i = 0; i < reply->cValues; i++) {
		SRowSet->aRow[i].ulAdrEntryPad = 0;
		SRowSet->aRow[i].cValues = 3;
		SRowSet->aRow[i].lpProps = talloc_array((TALLOC_CTX *)SRowSet->aRow, struct SPropValue, 
							SRowSet->aRow[i].cValues);

		SRowSet->aRow[i].lpProps[0].ulPropTag = PR_FID;
		SRowSet->aRow[i].lpProps[0].dwAlignPad = 0x0;
		SRowSet->aRow[i].lpProps[0].value.d = reply->entries[i].fid;

		if (reply->entries[i].lpszMessageClass && strlen(reply->entries[i].lpszMessageClass)) {
			SRowSet->aRow[i].lpProps[1].ulPropTag = PR_MESSAGE_CLASS;
			SRowSet->aRow[i].lpProps[1].dwAlignPad = 0x0;
			SRowSet->aRow[i].lpProps[1].value.lpszA = (uint8_t *) talloc_strdup((TALLOC_CTX *)SRowSet->aRow[i].lpProps, reply->entries[i].lpszMessageClass);
		} else {
			SRowSet->aRow[i].lpProps[1].ulPropTag = PR_MESSAGE_CLASS;
			SRowSet->aRow[i].lpProps[1].dwAlignPad = 0x0;
			SRowSet->aRow[i].lpProps[1].value.lpszA =  (uint8_t *) "";
		}

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
   \details Retrieves the folder ID of the temporary transport folder.

   \param obj_store the server object
   \param FolderId pointer on the returning Folder identifier

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
 */
_PUBLIC_ enum MAPISTATUS GetTransportFolder(mapi_object_t *obj_store,
					    mapi_id_t *FolderId)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetTransportFolder_repl	*reply;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!FolderId, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "GetTransportFolder");
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetTransportFolder;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_store);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Retrieve the FolderId parameter */
	reply = &mapi_response->mapi_repl->u.mapi_GetTransportFolder;
	*FolderId = reply->FolderId;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Get the list of servers that host replicas of a given
   public folder.

   \param obj_store the public folder store object
   \param obj_folder the folder object we search replica for
   \param OwningServersCount number of OwningServers
   \param CheapServersCount number of low-cost servers
   \param OwningServers pointer on the list of NULL terminated ASCII
   string representing replica servers

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note ecNoReplicaAvailable (0x469) can be returned if no replica is
   available for the folder.

   Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
 */
_PUBLIC_ enum MAPISTATUS GetOwningServers(mapi_object_t *obj_store,
					  mapi_object_t *obj_folder,
					  uint16_t *OwningServersCount,
					  uint16_t *CheapServersCount,
					  char **OwningServers)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetOwningServers_req	request;
	struct GetOwningServers_repl	response;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	mapi_id_t			FolderId;
	uint32_t			i;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!OwningServersCount, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!CheapServersCount, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!OwningServers, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	FolderId = mapi_object_get_id(obj_folder);
	OPENCHANGE_RETVAL_IF(!FolderId, MAPI_E_INVALID_PARAMETER, NULL);
		
	mem_ctx = talloc_named(session, 0, "GetOwningServers");
	
	size = 0;

	/* Fill the GetOwningServers operation */
	request.FolderId = FolderId;
	size += sizeof (uint64_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetOwningServers;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetOwningServers = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_store);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Retrieve GetOwningServers response */
	response = mapi_response->mapi_repl->u.mapi_GetOwningServers;

	*OwningServersCount = response.OwningServersCount;
	*CheapServersCount = response.CheapServersCount;
	if (*OwningServersCount) {
		OwningServers = talloc_array((TALLOC_CTX *)session, char *, *OwningServersCount + 1);
		for (i = 0; i != *OwningServersCount; i++) {
			OwningServers[i] = talloc_strdup((TALLOC_CTX *)OwningServers, response.OwningServers[i]);
		}
		OwningServers[i] = NULL;
	} else {
		OwningServers = NULL;
	}

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Gets the current store state for the logged in user

   This operation must be performed against a user store (not against
   a Public Folder store). The StoreState will have the 
   STORE_HAS_SEARCHES flag set if there are any active search folders.
   There are (currently) no other flags in the StoreState.

   \param obj_store the store object
   \param StoreState pointer to the store state returned by the server

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_store or StoreState are not valid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
 */
_PUBLIC_ enum MAPISTATUS GetStoreState(mapi_object_t *obj_store,
				       uint32_t *StoreState)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint8_t				logon_id;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!StoreState, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "GetStoreState");
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetStoreState;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_store);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Retrieve the StoreState */
	*StoreState = mapi_response->mapi_repl->u.mapi_GetStoreState.StoreState;

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

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
   
   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa MAPIInitialize, OpenMsgStore, GetLastError, GetDefaultFolder
*/
_PUBLIC_ enum MAPISTATUS GetOutboxFolder(mapi_object_t *obj_store, 
					 mapi_id_t *outbox_id)
{
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);

	*outbox_id = ((mapi_object_store_t *)obj_store->private_data)->fid_outbox;

	return MAPI_E_SUCCESS;
}


/**
   \details Notify the store of a new message to be processed

   \param obj_store the store that the message is in (logon object)
   \param obj_folder the folder that the message is in
   \param obj_msg the message to be processed
   \param MessageClass the message class of the message to be processed
   \param MessageFlags the message flags on the message

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one the parameters is invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa GetReceiveFolder, GetReceiveFolderTable
 */
_PUBLIC_ enum MAPISTATUS TransportNewMail(mapi_object_t *obj_store, mapi_object_t *obj_folder,
					  mapi_object_t *obj_msg,
					  const char *MessageClass, uint32_t MessageFlags)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct TransportNewMail_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_msg, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!MessageClass, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_folder);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_folder, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "TransportNewMail");

	/* Fill the TransportNewMail operation */
	size = 0;
	request.MessageId = mapi_object_get_id(obj_msg);
	size += sizeof (uint64_t);
	request.FolderId = mapi_object_get_id(obj_folder);
	size += sizeof (uint64_t);
	request.MessageClass = MessageClass;
	size += strlen(MessageClass) + 1;
	request.MessageFlags = MessageFlags;
	size += sizeof(uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_TransportNewMail;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_TransportNewMail = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_store);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
