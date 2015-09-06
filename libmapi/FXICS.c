/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2011.
   Copyright (C) Brad Hards <bradh@openchange.org> 2010-2011.

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

   \brief Fast Transfer and Incremental Change Synchronization operations
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
   - MAPI_E_NOT_ENOUGH_MEMORY: Memory allocation failed
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
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ReplGuid, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "GetLocalReplicaIds");
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
	size = 0;

	/* Fill the GetLocalReplicaIds operation */
	request.IdCount = IdCount;
	size += sizeof (uint32_t);

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	mapi_req->opnum = op_MAPI_GetLocalReplicaIds;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetLocalReplicaIds = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	OPENCHANGE_RETVAL_IF(!mapi_request, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	OPENCHANGE_RETVAL_IF(!mapi_request->handles, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
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
   - MAPI_E_NOT_ENOUGH_MEMORY: Memory allocation failed
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
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_destination_context, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "FXDestConfigure");
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
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
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	mapi_req->opnum = op_MAPI_FastTransferDestConfigure;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_FastTransferDestinationConfigure = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	OPENCHANGE_RETVAL_IF(!mapi_request, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	OPENCHANGE_RETVAL_IF(!mapi_request->handles, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
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
   - MAPI_E_NOT_ENOUGH_MEMORY: Memory allocation failed
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
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!version, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "TellVersion");
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
	size = 0;

	/* Fill the operation */
	request.version[0] = version[0];
	request.version[1] = version[1];
	request.version[2] = version[2];
	size += 3 * sizeof (uint16_t);



	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	mapi_req->opnum = op_MAPI_TellVersion;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_TellVersion = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	OPENCHANGE_RETVAL_IF(!mapi_request, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	OPENCHANGE_RETVAL_IF(!mapi_request->handles, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
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
   - MAPI_E_NOT_ENOUGH_MEMORY: Memory allocation failed
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
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_source_context, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "FXCopyFolder");
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
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
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	mapi_req->opnum = op_MAPI_FastTransferSourceCopyFolder;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_FastTransferSourceCopyFolder = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	OPENCHANGE_RETVAL_IF(!mapi_request, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	OPENCHANGE_RETVAL_IF(!mapi_request->handles, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
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
   \param message_ids the message IDs for the messages to copy.
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
   - MAPI_E_NOT_ENOUGH_MEMORY: Memory allocation failed
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
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_source_context, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!message_ids, MAPI_E_INVALID_PARAMETER, NULL);
	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "FXCopyMessages");
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
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
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	mapi_req->opnum = op_MAPI_FastTransferSourceCopyMessages;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_FastTransferSourceCopyMessages = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	OPENCHANGE_RETVAL_IF(!mapi_request, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	OPENCHANGE_RETVAL_IF(!mapi_request->handles, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
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

   Be careful in setting \a level to something other than zero. In particular, if \a level is
   non-zero for a message, then the list of recipients, and any attachments or embedded messages, will
   not be transferred.

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
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_source_context, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!excludes, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "FXCopyTo");
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

   Be careful in setting \a level to something other than zero. In particular, if \a level is
   non-zero for a message, then the list of recipients, and any attachments or embedded messages, will
   not be transferred.

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
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_source_context, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!properties, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "FXCopyProperties");
	size = 0;

	/* Fill the CopyProperties operation */
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

    Fast transfers are done in blocks, each block transfered over a call to FXGetBuffer. If the block
    is small, it will fit into a single call, and the transferStatus will indicate completion. However
    larger transfers will require multiple calls.

    \param obj_source_context the source object (from FXCopyTo, FXCopyProperties, FXCopyFolder or FXCopyMessages)
    \param maxSize the maximum size (pass 0 to indicate maximum available size)
    \param transferStatus result of the transfer
    \param progressStepCount the approximate number of steps (of totalStepCount) completed
    \param totalStepCount the approximate number of steps (total)
    \param blob this part of the transfer
    
   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameters is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
*/
_PUBLIC_ enum MAPISTATUS FXGetBuffer(mapi_object_t *obj_source_context, uint16_t maxSize, enum TransferStatus *transferStatus,
				     uint16_t *progressStepCount, uint16_t *totalStepCount, DATA_BLOB *blob)
{
	struct mapi_request				*mapi_request;
	struct mapi_response				*mapi_response;
	struct EcDoRpc_MAPI_REQ				*mapi_req;
	struct FastTransferSourceGetBuffer_req		request;
	struct FastTransferSourceGetBuffer_repl		*reply;
	struct mapi_session				*session;
	NTSTATUS					status;
	enum MAPISTATUS					retval;
	uint32_t					size = 0;
	TALLOC_CTX					*mem_ctx;
	uint8_t 					logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_source_context, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_source_context);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_source_context, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "FXGetBuffer");
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

	// TODO: handle backoff (0x00000480)
	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Retrieve the result */
	reply = &(mapi_response->mapi_repl->u.mapi_FastTransferSourceGetBuffer);
	*transferStatus = reply->TransferStatus;
	*progressStepCount = reply->InProgressCount;
	*totalStepCount = reply->TotalStepCount;
	blob->length = reply->TransferBufferSize;
	blob->data = (uint8_t *)talloc_size((TALLOC_CTX *)session, blob->length);
	memcpy(blob->data, reply->TransferBuffer.data, blob->length);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
    Send data to a destination fast transfer object

    Fast transfers are done in blocks, each block transfered over a call to FXGetBuffer. If the block
    is small, it will fit into a single call, and the transferStatus will indicate completion. However
    larger transfers will require multiple calls.

    \param obj_dest_context the destination object (from FXDestConfigure)
    \param blob this part of the transfer
    \param usedSize how many bytes of this part of the transfer that were used (only less than the total if an error occurred)
    
    \return MAPI_E_SUCCESS on success, otherwise MAPI error.

    \note Developers may also call GetLastError() to retrieve the last
    MAPI error code. Possible MAPI error codes are:
    - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
    - MAPI_E_INVALID_PARAMETER: one of the function parameters is
      invalid
    - MAPI_E_CALL_FAILED: A network problem was encountered during the
      transaction
*/
_PUBLIC_ enum MAPISTATUS FXPutBuffer(mapi_object_t *obj_dest_context, DATA_BLOB *blob, uint16_t *usedSize)
{
	struct mapi_request				*mapi_request;
	struct mapi_response				*mapi_response;
	struct EcDoRpc_MAPI_REQ				*mapi_req;
	struct FastTransferDestinationPutBuffer_req	request;
	struct FastTransferDestinationPutBuffer_repl	*reply;
	struct mapi_session				*session;
	NTSTATUS					status;
	enum MAPISTATUS					retval;
	uint32_t					size = 0;
	TALLOC_CTX					*mem_ctx;
	uint8_t 					logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_dest_context, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!blob, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!usedSize, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_dest_context);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_dest_context, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "FXPutBuffer");
	size = 0;

	/* Fill the PutBuffer operation */
	request.TransferBufferSize = blob->length;
	size += sizeof(uint16_t);
	request.TransferBuffer = *blob;
	size += blob->length;

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_FastTransferDestPutBuffer;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_FastTransferDestinationPutBuffer = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_dest_context);

	// TODO: handle backoff (0x00000480)
	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Retrieve the result */
	reply = &(mapi_response->mapi_repl->u.mapi_FastTransferDestinationPutBuffer);
	*usedSize = reply->BufferUsedCount;
	/* we could pull more stuff here, but it doesn't seem useful */

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Prepare a server for ICS download

   This function is used to configure a server for ICS (Incremental Change Synchronization)
   download. You use the synchronization context handle for other ICS and Fast Transfer
   operations.

   \param obj the target object for the upload (folder)
   \param sync_type the type of synchronization that will be performed (just
		    folder contents, or whole folder hierachy)
   \param send_options flags that change the format of the transfer (see FXCopyMessages)
   \param sync_flags flags that change the behavior of the transfer (see below)
   \param sync_extraflags additional flags that change the behavior of the transfer (see below)
   \param restriction a Restriction structure to limit downloads (only for sync_type ==
   SynchronizationType_Content)
   \param property_tags the properties to exclude (or include, if
   SynchronizationFlag_OnlySpecifiedProperties flag is set) in the download
   \param obj_sync_context the resulting synchronization context handle

   \a sync_flags can be zero or more of the following:
   - SynchronizationFlag_Unicode to use Unicode format (must match in send_options)
   - SynchronizationFlag_NoDeletions - whether to download changes about message deletion
   - SynchronizationFlag_IgnoreNoLongerInScope - whether to download changes for messages that have
   gone out of scope.
   - SynchronizationFlag_ReadState - server to download changes to read state
   - SynchronizationFlag_FAI server to download changes to FAI messages
   - SynchronizationFlag_Normal - server to download changes to normal messages
   - SynchronizationFlag_OnlySpecifiedProperties - set means to include only properties
   that are listed in property_tags, not-set means to exclude properties that are listed
   in property_tags
   - SynchronizationFlag_NoForeignIdentifiers - ignore persisted values (usually want this set)
   - SynchronizationFlag_BestBody - format for outputting message bodies (set means original format,
   not-set means output in RTF)
   - SynchronizationFlag_IgnoreSpecifiedOnFAI - ignore property_tags restrictions for FAI messages
   - SynchronizationFlag_Progress - whether to output progress information.

   \note SynchronizationFlag_IgnoreNoLongerInScope, SynchronizationFlag_ReadState,
   SynchronizationFlag_FAI, SynchronizationFlag_Normal, SynchronizationFlag_OnlySpecifiedProperties,
   SynchronizationFlag_BestBody and SynchronizationFlag_IgnoreSpecifiedOnFAI are only
   valid if the synchronization type is SynchronizationType_Content.

   \a sync_extraflags can be zero or more of the following:
   - SynchronizationExtraFlag_Eid - whether the server includes the PR_FID / PR_MID in the download
   - SynchronizationExtraFlag_MessageSize - whether the server includes the PR_MESSAGE_SIZE property
   in the download (only for sync_type == SynchronizationType_Content)
   - SynchronizationExtraFlag_Cn - whether the server includes the PR_CHANGE_NUM property in the
   download.
   - SynchronizationExtraFlag_OrderByDeliveryTime - whether the server sends messages sorted by
   delivery time (only for sync_type == SynchronizationType_Content)

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameters is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS ICSSyncConfigure(mapi_object_t *obj, enum SynchronizationType sync_type,
					  uint8_t send_options, uint16_t sync_flags,
					  uint32_t sync_extraflags, DATA_BLOB restriction,
					  struct SPropTagArray *property_tags,
					  mapi_object_t *obj_sync_context)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SyncConfigure_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_sync_context, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "RopSynchronizationConfigure");
	size = 0;

	/* Fill the SyncConfigure operation */
	request.handle_idx = 0x01;
	size += sizeof(uint8_t);
	request.SynchronizationType = sync_type;
	size += sizeof(uint8_t);
	request.SendOptions = send_options;
	size += sizeof(uint8_t);
	request.SynchronizationFlag = sync_flags;
	size += sizeof(uint16_t);
	request.RestrictionSize = restriction.length;
	size += sizeof(uint16_t);
	request.RestrictionData = restriction;
	size += request.RestrictionSize; /* for the RestrictionData BLOB */
	request.SynchronizationExtraFlags = sync_extraflags;
	size += sizeof(uint32_t);
	request.PropertyTags.cValues = property_tags->cValues;
	size += sizeof(uint16_t);
	request.PropertyTags.aulPropTag = property_tags->aulPropTag;
	size += property_tags->cValues * sizeof(enum MAPITAGS);

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SyncConfigure;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SyncConfigure = request;
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
	mapi_object_set_session(obj_sync_context, session);
	mapi_object_set_handle(obj_sync_context, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_sync_context, logon_id);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
    Initialize an ICS Initial State upload

    This is one of three operations (along with ICSSyncUploadStateContinue and ICSSyncUploadStateEnd)
    used to send the initial state for an ICS download to the server.

    \param obj_sync_context the synchronization context (from ICSSyncConfigure)
    \param state_property the type of ICS state that will be uploaded (see below)
    \param length the length (in bytes) of the ICS state that will be uploaded

    state_property can be one of the following:
    - PidTagIdsetGiven
    - PidTagCnsetSeen
    - PidTagCnsetSeenFAI
    - PidTagCnsetRead

    \return MAPI_E_SUCCESS on success, otherwise MAPI error.

    \note Developers may also call GetLastError() to retrieve the last
    MAPI error code. Possible MAPI error codes are:
    - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
    - MAPI_E_INVALID_PARAMETER: one of the function parameters is
      invalid
    - MAPI_E_CALL_FAILED: A network problem was encountered during the
      transaction
*/
_PUBLIC_ enum MAPISTATUS ICSSyncUploadStateBegin(mapi_object_t *obj_sync_context,
						 enum StateProperty state_property,
						 uint32_t length)
{
	struct mapi_request				*mapi_request;
	struct mapi_response				*mapi_response;
	struct EcDoRpc_MAPI_REQ				*mapi_req;
	struct SyncUploadStateStreamBegin_req		request;
	struct mapi_session				*session;
	NTSTATUS					status;
	enum MAPISTATUS					retval;
	uint32_t					size = 0;
	TALLOC_CTX					*mem_ctx;
	uint8_t 					logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_sync_context, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_sync_context);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_sync_context, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "ICSSyncUploadStateBegin");
	size = 0;

	/* Fill the RopSynchronizationUploadStateBegin operation */
	request.StateProperty = state_property;
	size += sizeof(uint32_t);
	request.TransferBufferSize = length;
	size += sizeof(uint32_t);

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SyncUploadStateStreamBegin;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SyncUploadStateStreamBegin = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_sync_context);

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
    Send data for an ICS Initial State upload

    This is one of three operations (along with ICSSyncUploadStateBegin and ICSSyncUploadStateEnd)
    used to send the initial state for an ICS download to the server.

    \param obj_sync_context the synchronization context (from ICSSyncConfigure)
    \param state the state data for this part of the upload

    \return MAPI_E_SUCCESS on success, otherwise MAPI error.

    \note Developers may also call GetLastError() to retrieve the last
    MAPI error code. Possible MAPI error codes are:
    - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
    - MAPI_E_INVALID_PARAMETER: one of the function parameters is
      invalid
    - MAPI_E_CALL_FAILED: A network problem was encountered during the
      transaction
*/
_PUBLIC_ enum MAPISTATUS ICSSyncUploadStateContinue(mapi_object_t *obj_sync_context, DATA_BLOB state)
{
	struct mapi_request				*mapi_request;
	struct mapi_response				*mapi_response;
	struct EcDoRpc_MAPI_REQ				*mapi_req;
	struct SyncUploadStateStreamContinue_req	request;
	struct mapi_session				*session;
	NTSTATUS					status;
	enum MAPISTATUS					retval;
	uint32_t					size = 0;
	TALLOC_CTX					*mem_ctx;
	uint8_t 					logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_sync_context, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_sync_context);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_sync_context, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "ICSSyncUploadStateContinue");
	size = 0;

	/* Fill the RopSynchronizationUploadStateBegin operation */
	request.StreamDataSize = state.length;
	size += sizeof(uint32_t);
	request.StreamData = state.data;
	size += state.length;

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SyncUploadStateStreamContinue;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SyncUploadStateStreamContinue = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_sync_context);

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
    Signal completion of an ICS Initial State upload

    This is one of three operations (along with ICSSyncUploadStateBegin and ICSSyncUploadStateContinue)
    used to send the initial state for an ICS download to the server.

    \param obj_sync_context the synchronization context (from ICSSyncConfigure)

    \return MAPI_E_SUCCESS on success, otherwise MAPI error.

    \note Developers may also call GetLastError() to retrieve the last
    MAPI error code. Possible MAPI error codes are:
    - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
    - MAPI_E_INVALID_PARAMETER: one of the function parameters is
      invalid
    - MAPI_E_CALL_FAILED: A network problem was encountered during the
      transaction
*/
_PUBLIC_ enum MAPISTATUS ICSSyncUploadStateEnd(mapi_object_t *obj_sync_context)
{
	struct mapi_request				*mapi_request;
	struct mapi_response				*mapi_response;
	struct EcDoRpc_MAPI_REQ				*mapi_req;
	struct mapi_session				*session;
	NTSTATUS					status;
	enum MAPISTATUS					retval;
	uint32_t					size = 0;
	TALLOC_CTX					*mem_ctx;
	uint8_t 					logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_sync_context, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_sync_context);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_sync_context, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "ICSSyncUploadStateEnd");
	size = 0;

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SyncUploadStateStreamEnd;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_sync_context);

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
   \details Mark a range of Message Ids as deleted / unused

   This function allows the client to specify that a specific range
   of message identifiers will never be used on a particular folder.
   This allows the server to make optimisations for message identifier
   sets during incremental change synchronisation operations.
   
   \param obj_folder pointer to the folder MAPI object
   \param ReplGuid the GUID for the MIDSET
   \param GlobalCountLow lower end of the range to be marked as deleted
   \param GlobalCountHigh upper end of the range to be marked as deleted

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameters is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS SetLocalReplicaMidsetDeleted(mapi_object_t *obj_folder, 
						      const struct GUID ReplGuid,
						      const uint8_t GlobalCountLow[6],
						      const uint8_t GlobalCountHigh[6])
{
	struct mapi_request				*mapi_request;
	struct mapi_response				*mapi_response;
	struct EcDoRpc_MAPI_REQ				*mapi_req;
	struct SetLocalReplicaMidsetDeleted_req		request;
	struct mapi_session				*session;
	NTSTATUS					status;
	enum MAPISTATUS					retval;
	uint32_t					size = 0;
	TALLOC_CTX					*mem_ctx;
	uint8_t 					logon_id = 0;
	uint32_t					i = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_folder);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_folder, &logon_id)) != MAPI_E_SUCCESS) {
		return retval;
	}

	mem_ctx = talloc_named(session, 0, __FUNCTION__);
	size = 0;

	/* Fill the SetLocalReplicaMidsetDeleted operation */
	request.DataSize = 0;
	size += sizeof(uint16_t);
	request.LongTermIdRangeCount = 1; /* this version only handles a single range */
	request.DataSize += sizeof(uint32_t); /* the size of the LongTermIdRangeCount */
	request.LongTermIdRanges = talloc_array(mem_ctx, struct LongTermIdRange, request.LongTermIdRangeCount);
	for (i = 0; i < request.LongTermIdRangeCount; ++i) {
		request.LongTermIdRanges[i].MinLongTermId.DatabaseGuid = ReplGuid;
		request.DataSize += sizeof(struct GUID);

		memcpy(request.LongTermIdRanges[i].MinLongTermId.GlobalCounter, GlobalCountLow, 6);
		request.DataSize += 6;

		request.LongTermIdRanges[i].MinLongTermId.padding = 0x0000;
		request.DataSize += sizeof(uint16_t);

		request.LongTermIdRanges[i].MaxLongTermId.DatabaseGuid = ReplGuid;
		request.DataSize += sizeof(struct GUID);

		memcpy(request.LongTermIdRanges[i].MaxLongTermId.GlobalCounter, GlobalCountHigh, 6);
		request.DataSize += 6;

		request.LongTermIdRanges[i].MaxLongTermId.padding = 0x0000;
		request.DataSize += sizeof(uint16_t);
	}
	size += request.DataSize; /* add that datasize to the overall length */

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetLocalReplicaMidsetDeleted;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SetLocalReplicaMidsetDeleted = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_folder);

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
   \details Prepare a folder for ICS upload

   \param folder the folder for the collector creation
   \param isContentsCollector true for contents collector, false for hierachy collector
   \param obj_collector pointer to the resulting ICS collector

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameters is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS ICSSyncOpenCollector(mapi_object_t *folder,
					      bool isContentsCollector,
					      mapi_object_t *obj_collector)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SyncOpenCollector_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!folder, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(folder);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_collector, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(folder, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, __FUNCTION__);
	size = 0;

	/* Fill the SyncOpenCollector operation */
	request.handle_idx = 0x01;
	size += sizeof(uint8_t);
	if (isContentsCollector) {
		request.IsContentsCollector = 0x01;
	} else {
		request.IsContentsCollector = 0x00;
	}
	size += sizeof(uint8_t);

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SyncOpenCollector;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SyncOpenCollector = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(folder);
	mapi_request->handles[1] = 0xFFFFFFFF;

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Set object session and handle */
	mapi_object_set_session(obj_collector, session);
	mapi_object_set_handle(obj_collector, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_collector, logon_id);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details obtain an object to get the sync transfer state

   \param obj the source object
   \param obj_sync_context the synchronization transfer state object

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameters is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS ICSSyncGetTransferState(mapi_object_t *obj, mapi_object_t *obj_sync_context)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SyncGetTransferState_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_sync_context, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS) {
		return retval;
	}

	mem_ctx = talloc_named(session, 0, __FUNCTION__);

	/* Fill the SyncGetTransferState operation */
	request.handle_idx = 0x01;
	size += sizeof(uint8_t);

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SyncGetTransferState;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SyncGetTransferState = request;
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
	mapi_object_set_session(obj_sync_context, session);
	mapi_object_set_handle(obj_sync_context, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_sync_context, logon_id);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Import message or folder deletions.

   \param collector the ICS collector to use to upload changes
   \param flags the sync import delete flags. Accepted values are in SyncImportDeletesFlags enum type
   \param source_keys the GID array from which we import their deletions

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameters is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
   - MAPI_E_NOT_ENOUGH_MEMORY: If any dynamic memory allocated
   did not succeed
 */
_PUBLIC_ enum MAPISTATUS SyncImportDeletes(mapi_object_t *collector,
					   uint8_t flags,
					   struct BinaryArray_r *source_keys)
{
	bool				       ret;
	struct EcDoRpc_MAPI_REQ		       *mapi_req;
	enum MAPISTATUS			       retval;
	struct mapi_request		       *mapi_request;
	struct mapi_response		       *mapi_response;
	struct mapi_session		       *session;
	struct mapi_SBinaryArray	       property_values;
	struct mapi_SPropValue		       *lp_prop;
	NTSTATUS			       status;
	size_t				       i;
	struct SyncImportDeletes_req	       request;
	TALLOC_CTX			       *local_mem_ctx;
	uint8_t				       logon_id = 0;
	uint32_t			       size = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!collector, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!source_keys, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(source_keys->cValues < 1, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(collector);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(collector, &logon_id)) != MAPI_E_SUCCESS) {
		return retval;
	}

	local_mem_ctx = talloc_new(session);
	OPENCHANGE_RETVAL_IF(!local_mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	/* Fill the SyncImportDeletes operation */
	request.Flags = flags;
	size += sizeof(uint8_t);
	request.PropertyValues.cValues = 1;
	size += sizeof(uint16_t);

	property_values.cValues = source_keys->cValues;
	size += sizeof(uint32_t);
	property_values.bin = talloc_zero_array(local_mem_ctx, struct SBinary_short, source_keys->cValues);
	OPENCHANGE_RETVAL_IF(!property_values.bin, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

	for (i = 0; i < property_values.cValues; i++) {
		property_values.bin[i].cb = (uint16_t)source_keys->lpbin[i].cb;
		size += sizeof(uint16_t);
		property_values.bin[i].lpb = source_keys->lpbin[i].lpb;
		size += source_keys->lpbin[i].cb;
	}

	lp_prop = talloc_zero(local_mem_ctx, struct mapi_SPropValue);
	OPENCHANGE_RETVAL_IF(!lp_prop, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	size += sizeof(uint32_t);

	ret = set_mapi_SPropValue_proptag(local_mem_ctx, lp_prop,
					  PT_MV_BINARY, (const void*)&property_values);
	OPENCHANGE_RETVAL_IF(!ret, MAPI_E_CALL_FAILED, local_mem_ctx);
	request.PropertyValues.lpProps = lp_prop;

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(local_mem_ctx, struct EcDoRpc_MAPI_REQ);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	mapi_req->opnum = op_MAPI_SyncImportDeletes;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SyncImportDeletes = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(local_mem_ctx, struct mapi_request);
	OPENCHANGE_RETVAL_IF(!mapi_request, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(local_mem_ctx, uint32_t, 2);
	OPENCHANGE_RETVAL_IF(!mapi_request->handles, MAPI_E_NOT_ENOUGH_MEMORY,
			     local_mem_ctx);
	mapi_request->handles[0] = mapi_object_get_handle(collector);
	mapi_request->handles[1] = 0xFFFFFFFF;

	status = emsmdb_transaction_wrapper(session, local_mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, local_mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, local_mem_ctx);
	retval = mapi_response->mapi_repl->error_code;

	talloc_free(mapi_response);
	talloc_free(local_mem_ctx);

	return retval;
}
