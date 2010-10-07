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
   \file FXICS.c

   \brief Incremental Change Synchronization operations
 */


/**
   \details Reserves a range of IDs to be used by a local replica

   \param obj_store pointer on the store MAPI object
   \param IdCount ID range length to reserve
   \param ReplGuid pointer to the GUID structure returned by the
   server
   \param GlobalCount byte array that specifies the first allocated
   field

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameters is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS GetLocalReplicaIds(mapi_object_t *obj_store, 
					    uint32_t IdCount,
					    struct GUID *ReplGuid,
					    uint8_t GlobalCount[6])
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetLocalReplicaIds_req	request;
	struct GetLocalReplicaIds_repl	*reply;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ReplGuid, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(NULL, 0, "GetLocalReplicaIds");
	size = 0;

	/* Fill the GetLocalReplicaIds operation */
	request.IdCount = IdCount;
	size += sizeof (uint32_t);

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetLocalReplicaIds;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetLocalReplicaIds = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_store);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);
	
	/* Retrieve output parameters */
	reply = &mapi_response->mapi_repl->u.mapi_GetLocalReplicaIds;
	*ReplGuid = reply->ReplGuid;
	memcpy(GlobalCount, reply->GlobalCount, 6);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Prepare a server for Fast Transfer receive

   This function is used to configure a server for fast-transfer receive operation.
   This could be the target server in a server->client->server copy, or for a
   client->server upload.

   \param obj the target object for the upload (folder, message or attachment)
   \param sourceOperation the type of transfer (one of FastTransferDest_CopyTo,
   FastTransferDest_CopyProperties,FastTransferDest_CopyMessages or
   FastTransferDest_CopyFolder)
   \param obj_destination_context the fast transfer context for future ROPs.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameters is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS FXDestConfigure(mapi_object_t *obj,
					 enum FastTransferDestConfig_SourceOperation sourceOperation,
					 mapi_object_t *obj_destination_context)
{
	struct mapi_request				*mapi_request;
	struct mapi_response				*mapi_response;
	struct EcDoRpc_MAPI_REQ				*mapi_req;
	struct FastTransferDestinationConfigure_req	request;
	struct mapi_session				*session;
	NTSTATUS					status;
	enum MAPISTATUS					retval;
	uint32_t					size = 0;
	TALLOC_CTX					*mem_ctx;
	uint8_t 					logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_destination_context, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(NULL, 0, "FXDestConfigure");
	size = 0;

	/* Fill the ConfigureDestination operation */
	request.handle_idx = 0x01;
	size += sizeof(uint8_t);
	request.SourceOperation = sourceOperation;
	size += sizeof(uint8_t);
	request.CopyFlags = 0x00; /* we don't support move yet */
	size += sizeof(uint8_t);

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_FastTransferDestConfigure;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_FastTransferDestinationConfigure = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj);
	mapi_request->handles[1] = 0xFFFFFFFF;

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Set object session and handle */
	mapi_object_set_session(obj_destination_context, session);
	mapi_object_set_handle(obj_destination_context, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_destination_context, logon_id);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Advise a server of the "other server" version

   This function is used to set up a fast server-client-server transfer.

   \param obj_store pointer to the store MAPI object
   \param version the server version

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameter is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS TellVersion(mapi_object_t *obj_store, uint16_t version[3])
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct TellVersion_req		request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!version, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(NULL, 0, "TellVersion");
	size = 0;

	/* Fill the operation */
	request.version[0] = version[0];
	request.version[1] = version[1];
	request.version[2] = version[2];
	size += 3 * sizeof (uint16_t);



	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_TellVersion;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_TellVersion = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = (uint16_t)size;
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

/**
   \details Prepare a server for Fast Transfer transmission of a folder hierachy

   This function is used to configure a server for a fast-transfer folder hierachy 
   send operation. This could be the origin server in a server->client->server copy, or for a
   server to client download.
   
   This operation copies the folder object, and any sub-objects (including folder properties
   and messages). It can optionally copy sub-folders, depending on copyFlags.

   \param obj the source object for the operation (folder)
   \param copyFlags flags that change the copy behaviour (see below)
   \param sendOptions flags that change the format of the transfer (see FXCopyMessages)
   \param obj_source_context the fast transfer source context for future ROPs

   \a copyflags can be zero or more of the following:
   - FastTransferCopyFolder_CopySubfolders to recursively copy any sub-folders and contained messages
   - FastTransferCopyFolder_NoGhostedContent to omit any ghosted content when copying public folders

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameters is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS FXCopyFolder(mapi_object_t *obj, uint8_t copyFlags, uint8_t sendOptions,
				      mapi_object_t *obj_source_context)
{
	struct mapi_request				*mapi_request;
	struct mapi_response				*mapi_response;
	struct EcDoRpc_MAPI_REQ				*mapi_req;
	struct FastTransferSourceCopyFolder_req		request;
	struct mapi_session				*session;
	NTSTATUS					status;
	enum MAPISTATUS					retval;
	uint32_t					size = 0;
	TALLOC_CTX					*mem_ctx;
	uint8_t 					logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_source_context, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(NULL, 0, "FXCopyFolder");
	size = 0;

	/* Fill the CopyFolder operation */
	request.handle_idx = 0x01;
	size += sizeof(uint8_t);
	request.CopyFlags = copyFlags;
	size += sizeof(uint8_t);
	request.SendOptions = sendOptions;
	size += sizeof(uint8_t);

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_FastTransferSourceCopyFolder;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_FastTransferSourceCopyFolder = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Set object session and handle */
	mapi_object_set_session(obj_source_context, session);
	mapi_object_set_handle(obj_source_context, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_source_context, logon_id);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Prepare a server for Fast Transfer transmission of a list of messages
   
   This function is used to configure a server for a fast-transfer message
   send operation. This could be the origin server in a server->client->server copy, or for a
   server to client download.
   
   This operation copies the message objects, and any sub-objects (including attachments and embedded
   messages).

   \param obj the source object for the operation (folder)
   \param mids the message IDs for the messages to copy.
   \param copyFlags flags that change the copy behaviour (see below)
   \param sendOptions flags that change the format of the transfer (see below)
   \param obj_source_context the fast transfer source context for future ROPs

   \a copyflags can be zero or more of the following:
   - FastTransferCopyMessage_Move - configure output for move
   - FastTransferCopyMessage_BestBody - output message bodies in original format (if not set, output in RTF)
   - FastTransferCopyMessage_SendEntryId - include message and change identification in the output stream

   \a sendOptions can be zero or more of the following:
   - FastTransfer_Unicode - enable Unicode output
   - FastTransfer_UseCpid (not normally used directly - implied by ForUpload)
   - FastTransfer_ForUpload - (enable Unicode, and advise the server that transfer is server->client->server)
   - FastTransfer_RecoverMode - advise the server that the client supports recovery mode
   - FastTransfer_ForceUnicode - force Unicode output
   - FastTransfer_PartialItem - used for synchronisation download
   
   If the \a FastTransfer_ForUpload is set, the next call must be TellVersion()

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameters is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS FXCopyMessages(mapi_object_t *obj, mapi_id_array_t *message_ids,
					uint8_t copyFlags, uint8_t sendOptions,
					mapi_object_t *obj_source_context)
{
	struct mapi_request				*mapi_request;
	struct mapi_response				*mapi_response;
	struct EcDoRpc_MAPI_REQ				*mapi_req;
	struct FastTransferSourceCopyMessages_req	request;
	struct mapi_session				*session;
	NTSTATUS					status;
	enum MAPISTATUS					retval;
	uint32_t					size = 0;
	TALLOC_CTX					*mem_ctx;
	uint8_t 					logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_source_context, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!message_ids, MAPI_E_INVALID_PARAMETER, NULL);
	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(NULL, 0, "FXCopyMessages");
	size = 0;

	/* Fill the CopyMessages operation */
	request.handle_idx = 0x01;
	size += sizeof(uint8_t);
	request.MessageIdCount = message_ids->count;
	size += sizeof (uint16_t);
	retval = mapi_id_array_get(mem_ctx, message_ids, &(request.MessageIds));
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
	size += request.MessageIdCount * sizeof (mapi_id_t);
	request.CopyFlags = copyFlags;
	size += sizeof(uint8_t);
	request.SendOptions = sendOptions;
	size += sizeof(uint8_t);

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_FastTransferSourceCopyMessages;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_FastTransferSourceCopyMessages = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Set object session and handle */
	mapi_object_set_session(obj_source_context, session);
	mapi_object_set_handle(obj_source_context, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_source_context, logon_id);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Prepare a server for Fast Transfer transmission of a folder, message or attachment

   This function is used to configure a server for a fast-transfer download of a folder, message
   or attachment. This could be the origin server in a server->client->server copy, or for a
   server to client download.
   
   This operation copies the source object, potentially omitting some properties. It can optionally
   copy sub-objects, depending on \a Level.

   \param obj the source object for the operation (folder, message or attachment)
   \param level whether to copy sub-objects of folders or messages (set to 0) or not (set to any other value)
   \param copyFlags flags that change the copy behaviour (see below)
   \param sendOptions flags that change the format of the transfer (see FXCopyMessages)
   \param excludes the list of properties to exclude from the transfer
   \param obj_source_context the fast transfer source context for future ROPs

   \a copyflags can be zero or more of the following:
   - FastTransferCopyTo_Move to configure as part of a move operation
   - FastTransferCopyTo_BestBody to use original format for message bodies (if not set, use RTF instead)

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameters is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS FXCopyTo(mapi_object_t *obj, uint8_t level, uint32_t copyFlags,
				  uint8_t sendOptions, struct SPropTagArray *excludes,
				  mapi_object_t *obj_source_context)
{
	struct mapi_request				*mapi_request;
	struct mapi_response				*mapi_response;
	struct EcDoRpc_MAPI_REQ				*mapi_req;
	struct FastTransferSourceCopyTo_req		request;
	struct mapi_session				*session;
	NTSTATUS					status;
	enum MAPISTATUS					retval;
	uint32_t					size = 0;
	TALLOC_CTX					*mem_ctx;
	uint8_t 					logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_source_context, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!excludes, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(NULL, 0, "FXCopyTo");
	size = 0;

	/* Fill the CopyTo operation */
	request.handle_idx = 0x01;
	size += sizeof(uint8_t);
	request.Level = level;
	size += sizeof(uint8_t);
	request.CopyFlags = copyFlags;
	size += sizeof(uint32_t);
	request.SendOptions = sendOptions;
	size += sizeof(uint8_t);
	request.PropertyTags.cValues = excludes->cValues;
	size += sizeof(uint16_t);
	request.PropertyTags.aulPropTag = excludes->aulPropTag;
	size += excludes->cValues * sizeof(enum MAPITAGS);

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_FastTransferSourceCopyTo;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_FastTransferSourceCopyTo = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Set object session and handle */
	mapi_object_set_session(obj_source_context, session);
	mapi_object_set_handle(obj_source_context, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_source_context, logon_id);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Prepare a server for Fast Transfer transmission of the properties of a folder, message or attachment

   This function is used to configure a server for a fast-transfer download of properties. This could be the
   origin server in a server->client->server copy, or for a server to client download.
   
   This operation copies the specified properties of the source object. It can optionally copy properties of
   sub-objects, depending on \a Level.

   \param obj the source object for the operation (folder, message or attachment)
   \param level whether to copy properties of sub-objects of folders or messages (set to 0) or not (set to any other value)
   \param copyFlags flags that change the copy behaviour (see below)
   \param sendOptions flags that change the format of the transfer (see FXCopyMessages)
   \param properties the list of properties to transfer
   \param obj_source_context the fast transfer source context for future ROPs

   \a copyflags may be the following:
   - FastTransferCopyProperties_Move to configure as part of a move operation

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameters is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS FXCopyProperties(mapi_object_t *obj, uint8_t level, uint32_t copyFlags,
					  uint8_t sendOptions, struct SPropTagArray *properties,
					  mapi_object_t *obj_source_context)
{
	struct mapi_request				*mapi_request;
	struct mapi_response				*mapi_response;
	struct EcDoRpc_MAPI_REQ				*mapi_req;
	struct FastTransferSourceCopyProperties_req	request;
	struct mapi_session				*session;
	NTSTATUS					status;
	enum MAPISTATUS					retval;
	uint32_t					size = 0;
	TALLOC_CTX					*mem_ctx;
	uint8_t 					logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_source_context, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!properties, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(NULL, 0, "FXCopyProperties");
	size = 0;

	/* Fill the CopyTo operation */
	request.handle_idx = 0x01;
	size += sizeof(uint8_t);
	request.Level = level;
	size += sizeof(uint8_t);
	request.CopyFlags = copyFlags;
	size += sizeof(uint8_t);
	request.SendOptions = sendOptions;
	size += sizeof(uint8_t);
	request.PropertyTags.cValues = properties->cValues;
	size += sizeof(uint16_t);
	request.PropertyTags.aulPropTag = properties->aulPropTag;
	size += properties->cValues * sizeof(enum MAPITAGS);

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_FastTransferSourceCopyProps;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_FastTransferSourceCopyProperties = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Set object session and handle */
	mapi_object_set_session(obj_source_context, session);
	mapi_object_set_handle(obj_source_context, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_source_context, logon_id);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
    Get data from source fast transfer object

    TODO:
    - finish documenting this
    - add return parameter(s)

    \param obj_source_context the source object (from FXCopyTo, FXCopyProperties, FXCopyFolder or FXCopyMessages)
    \param maxSize the maximum size (pass 0 to indicate maximum available size)
*/
_PUBLIC_ enum MAPISTATUS FXGetBuffer(mapi_object_t *obj_source_context, uint16_t maxSize)
{
	struct mapi_request				*mapi_request;
	struct mapi_response				*mapi_response;
	struct EcDoRpc_MAPI_REQ				*mapi_req;
	struct FastTransferSourceGetBuffer_req		request;
	struct mapi_session				*session;
	NTSTATUS					status;
	enum MAPISTATUS					retval;
	uint32_t					size = 0;
	TALLOC_CTX					*mem_ctx;
	uint8_t 					logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_source_context, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_source_context);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_source_context, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(NULL, 0, "FXGetBuffer");
	size = 0;

	/* Fill the GetBuffer operation */
	if ((maxSize == 0) || (maxSize < 15480)) {
		request.BufferSize = 0xBABE;
		size += sizeof(uint16_t);
		request.MaximumBufferSize.MaximumBufferSize = 0x8000;
		size += sizeof(uint16_t);
	} else {
		request.BufferSize = maxSize;
		size += sizeof(uint16_t);
	}

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_FastTransferSourceGetBuffer;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_FastTransferSourceGetBuffer = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_source_context);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
