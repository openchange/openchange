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
   \file IMAPIFolder.c

   \brief Folder related functions
 */


/**
   \details The function creates a message in the specified folder,
   and returns a pointer on this message.

   \param obj_folder the folder to create the message in.
   \param obj_message pointer to the newly created message.

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenFolder, DeleteMessage, GetLastError
*/
_PUBLIC_ enum MAPISTATUS CreateMessage(mapi_object_t *obj_folder, mapi_object_t *obj_message)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct CreateMessage_req request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("CreateMessage");

	/* Fill the OpenFolder operation */
	request.handle_idx = 0x1;
	request.max_data = 0xfff;
	request.folder_id = mapi_object_get_id(obj_folder);
	request.padding = 0;
	size = sizeof (uint8_t) + sizeof(uint16_t) + sizeof(mapi_id_t) + sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_CreateMessage;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_CreateMessage = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof(mapi_handle_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, mapi_handle_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_folder);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* set object handle */
	mapi_object_set_handle(obj_message, mapi_response->handles[1]);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Delete one or more messages

   This function deletes one or more messages based on their ids from
   a specified folder.

   \param obj_folder the folder to delete messages from
   \param id_messages the list of ids
   \param cn_messages the number of messages in the id list.

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenFolder, DeleteMessage, GetLastError
*/
_PUBLIC_ enum MAPISTATUS DeleteMessage(mapi_object_t *obj_folder, mapi_id_t *id_messages,
				       uint32_t cn_messages)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct DeleteMessages_req request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("DeleteMessages");

	size = 0;

	/* Fill the DeleteMessages operation */
	request.WantAsynchronous = 0x0;
	size += sizeof (uint8_t);
	request.NotifyNonRead = 0x1;
	size += sizeof(uint8_t);
	request.cn_ids = (uint16_t)cn_messages;
	size += sizeof(uint16_t);
	request.message_ids = id_messages;
	size += request.cn_ids * sizeof(mapi_id_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_DeleteMessages;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_DeleteMessages = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_folder);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Obtain the status associated with a message

   This function obtains the status associated with a message in the
   given folder.

   \param obj_folder the folder where the message is located
   \param msgid the message ID
   \param ulStatus the message status

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
 */
_PUBLIC_ enum MAPISTATUS GetMessageStatus(mapi_object_t *obj_folder, 
					  mapi_id_t msgid, 
					  uint32_t *ulStatus)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetMessageStatus_req	request;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	mapi_ctx_t			*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!msgid, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("GetMessageStatus");

	size = 0;

	/* Fill the GetMessageStatus operation */
	request.msgid = msgid;
	size += sizeof (uint64_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetMessageStatus;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetMessageStatus = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_folder);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	*ulStatus = mapi_response->mapi_repl->u.mapi_SetMessageStatus.ulOldStatus;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Set the status associated with a message

   This function sets the status associated with a message in the
   given folder.

   \param obj_folder the folder where the message is located
   \param msgid the message ID
   \param ulNewStatus the new status to be assigned
   \param ulNewStatusMask bitmask of flags hat is applied to the new
   status indicating the flags to be set
   \param ulOldStatus pointer on the previous status of the message

   ulNewStatusMask possible values:
   - MSGSTATUS_DELMARKED: the message is marked for deletion
   - MSGSTATUS_HIDDEN: the message is not to be displayed
   - MSGSTATUS_HIGHLIGHTED: the message is to be displayed highlighted
   - MSGSTATUS_REMOTE_DELETE: the message has been marked for deletion
     on the remote message store without downloading to the local
     client.
   - MSGSTATUS_REMOTE_DOWNLOAD: the message has been marked for
     downloading from the remote message store to the local client.
   - MSGSTATUS_TAGGED: The message has been tagged for a
     client-defined purpose.

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
 */
_PUBLIC_ enum MAPISTATUS SetMessageStatus(mapi_object_t *obj_folder,
					  mapi_id_t msgid,
					  uint32_t ulNewStatus,
					  uint32_t ulNewStatusMask,
					  uint32_t *ulOldStatus)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SetMessageStatus_req    	request;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	mapi_ctx_t			*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!msgid, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("SetMessageStatus");

	size = 0;

	/* Fill the SetMessageStatus operation */
	request.msgid = msgid;
	size += sizeof (uint64_t);

	request.ulNewStatus = ulNewStatus;
	size += sizeof (uint32_t);

	request.ulNewStatusMask = ulNewStatusMask;
	size += sizeof (uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetMessageStatus;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SetMessageStatus = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_folder);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	*ulOldStatus = mapi_response->mapi_repl->u.mapi_SetMessageStatus.ulOldStatus;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}


/**
   \details Copy or Move a message from a folder to another

   \param obj_src The source folder
   \param obj_dst The destination folder
   \param msgid pointer on an array of message IDs
   \param WantCopy boolean value, defines whether the operation is a
   copy or a move

   Possible values for WantCopy:
   -# 0: Move
   -# 1: Copy

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenFolder, GetLastError
*/
_PUBLIC_ enum MAPISTATUS MoveCopyMessages(mapi_object_t *obj_src,
					  mapi_object_t *obj_dst,
					  mapi_id_t *message_id,
					  bool WantCopy)
{
	NTSTATUS			status;
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct MoveCopyMessages_req	request;
	uint32_t			size;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = talloc_init("CopyMessages");
	
	size = 0;

	/* FIXME: the function can take an array of msgids and is not
	 * limited to a unique msgid 
	 */

	/* Fill the CopyMessage operation */
	request.handle_idx = 0x1;
	size += sizeof (uint8_t);

	for (request.count = 0; message_id[request.count]; request.count++);
	size += sizeof (uint16_t);

	request.message_id = message_id;
	size += request.count * sizeof (mapi_id_t);

	request.WantAsynchronous = 0;
	size += sizeof (uint8_t);

	request.WantCopy = WantCopy;
	size += sizeof (uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_MoveCopyMessages;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_MoveCopyMessages = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_src);
	mapi_request->handles[1] = mapi_object_get_handle(obj_dst);

	status = emsmdb_transaction(global_mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);
	
	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Clear or set the MSGFLAG_READ flag for a given message

   This function clears or sets the MSGFLAG_READ flag in the
   PR_MESSAGE_FLAGS property of a given message.

   \param obj_folder the folder to operate in
   \param obj_child the message to set flags on
   \param flags the new flags (MSGFLAG_READ) value

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenMessage, GetLastError
 */
_PUBLIC_ enum MAPISTATUS SetReadFlags(mapi_object_t *obj_folder, 
				      mapi_object_t *obj_child,
				      uint8_t flags)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SetReadFlags_req request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("SetReadFlags");

	size = 0;

	/* Fill the SetReadFlags operation */
	request.handle_idx = 0x1;
	size += sizeof(uint8_t);
	request.flags = flags;
	size += sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetReadFlags;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SetReadFlags = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_folder);
	mapi_request->handles[1] = mapi_object_get_handle(obj_child);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Create a folder

   The function creates a folder (defined with its name, comment and type)
   within a specified folder.

   \param obj_parent the folder to create the new folder in
   \param ulFolderType the type of the folder
   \param name the name of the new folder
   \param comment the comment associated with the new folder
   \param ulFlags flags associated with folder creation
   \param obj_child pointer to the newly created folder

   ulFlags possible values:
   - MAPI_UNICODE: use UNICODE folder name and comment
   - OPEN_IF_EXISTS: open the folder if it already exists

   ulFolderType possible values:
   - FOLDER_GENERIC
   - FOLDER_SEARCH

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenFolder, DeleteFolder, EmptyFolder, GetLastError
*/
_PUBLIC_ enum MAPISTATUS CreateFolder(mapi_object_t *obj_parent, 
				      uint8_t ulFolderType,
				      const char *name,
				      const char *comment, 
				      uint32_t ulFlags,
				      mapi_object_t *obj_child)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct CreateFolder_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!name, MAPI_E_NOT_INITIALIZED, NULL);

	/* Sanitify check on the folder type */
	MAPI_RETVAL_IF((ulFolderType != FOLDER_GENERIC && 
			ulFolderType != FOLDER_SEARCH),
		       MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("CreateFolder");

	size = 0;

	/* Fill the CreateFolder operation */
	request.handle_idx = 0x1;
	size+= sizeof(uint8_t);
	switch (ulFlags & 0xFFFF0000) {
	case MAPI_UNICODE:
		request.ulType = MAPI_FOLDER_UNICODE;
		break;
	default:
		request.ulType = MAPI_FOLDER_ANSI;
		break;
	}
	request.ulFolderType = ulFolderType;
	size += sizeof(uint16_t);
	request.ulFlags = ulFlags & 0xFFFF;
	size += sizeof(uint16_t);

	switch (request.ulType) {
	case MAPI_FOLDER_ANSI:
		request.FolderName.lpszA = name;
		size += strlen(name) + 1;
		break;
	case MAPI_FOLDER_UNICODE:
		request.FolderName.lpszW = name;
		size += strlen(name) * 2 + 2;
		break;
	}

	if (comment) {
		switch(request.ulType) {
		case MAPI_FOLDER_ANSI:
			request.FolderComment.lpszA = comment;
			size += strlen(comment) + 1;
			break;
		case MAPI_FOLDER_UNICODE:
			request.FolderComment.lpszW = comment;
			size += strlen(comment) * 2 + 2;
			break;
		}
	} else {
		switch(request.ulType) {
		case MAPI_FOLDER_ANSI:
			request.FolderComment.lpszA = "";
			size += 1;
			break;
		case MAPI_FOLDER_UNICODE:
			request.FolderComment.lpszW = "";
			size += 2;
			break;
		}
	}

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_CreateFolder;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_CreateFolder = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + (2 * sizeof(uint32_t));
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_parent);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	mapi_object_init(obj_child);
	mapi_object_set_handle(obj_child, mapi_response->handles[1]);
	mapi_object_set_id(obj_child, mapi_response->mapi_repl->u.mapi_CreateFolder.folder_id);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Empty the contents of a folder

   This function empties (clears) the contents of a specified folder.

   \param obj_folder the folder to empty

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenFolder, CreateFolder, DeleteFolder, GetLastError
*/
_PUBLIC_ enum MAPISTATUS EmptyFolder(mapi_object_t *obj_folder)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct EmptyFolder_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("EmptyFolder");

	size = 0;

	/* Fill the DeleteMessages operation */
	request.unknown = 0;
	size += sizeof(uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_EmptyFolder;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_EmptyFolder = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof(uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_folder);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Delete a folder

   The function deletes a specified folder.

   \param obj_parent the folder containing the folder to be deleted
   \param folder_id the folder to delete

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenFolder, CreateFolder, EmptyFolder, GetLastError
*/
_PUBLIC_ enum MAPISTATUS DeleteFolder(mapi_object_t *obj_parent, mapi_id_t folder_id)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct DeleteFolder_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("DeleteFolder");

	size = 0;

	/* Fill the DeleteMessages operation */
	request.flags = 0x5;
	size += sizeof(uint8_t);
	request.folder_id = folder_id;
	size += 8;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_DeleteFolder;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_DeleteFolder = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof(uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_parent);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Moves a folder

   \param obj_folder the folder to move
   \param obj_src source object where the folder to move is stored
   \param obj_dst destination object where the folder will be moved
   \param NewFolderName the new folder name in the destination folder
   \param UseUnicode whether the folder name is unicode encoded or not

    \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenFolder, CopyFolder
 */
_PUBLIC_ enum MAPISTATUS MoveFolder(mapi_object_t *obj_folder,
				    mapi_object_t *obj_src, 
				    mapi_object_t *obj_dst,
				    char *NewFolderName,
				    bool UseUnicode)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct MoveFolder_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	/* Sanity checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!obj_src, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!obj_dst, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!NewFolderName, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("MoveFolder");

	size = 0;

	/* Fill the MoveFolder operation */
	request.handle_idx = 0x1;
	size += sizeof (uint8_t);

	request.WantAsynchronous = 0;
	size += sizeof (uint8_t);

	request.UseUnicode = UseUnicode;
	size += sizeof (uint8_t);

	request.FolderId = mapi_object_get_id(obj_folder);
	size += sizeof (uint64_t);

	if (!request.UseUnicode) {
		request.NewFolderName.lpszA = NewFolderName;
		size += strlen(NewFolderName) + 1;
	} else {
		request.NewFolderName.lpszW = NewFolderName;
		size += strlen(NewFolderName) * 2 + 2;
	}


	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_MoveFolder;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_MoveFolder = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_src);
	mapi_request->handles[1] = mapi_object_get_handle(obj_dst);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Copy a folder

   \param obj_folder the folder to copy
   \param obj_src source object where the folder to copy is stored
   \param obj_dst destination object where the folder will be copied
   \param NewFolderName the new folder name in the destination folder
   \param UseUnicode whether the folder name is unicode encoded or not
   \param WantRecursive whether we should copy folder's subdirectories
   or not

   \return MAPI_E_SUCCESS on success, otherwise -1.
   
   \note Developer should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenFolder, MoveFolder
 */
_PUBLIC_ enum MAPISTATUS CopyFolder(mapi_object_t *obj_folder,
				    mapi_object_t *obj_src,
				    mapi_object_t *obj_dst,
				    char *NewFolderName,
				    bool UseUnicode,
				    bool WantRecursive)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct CopyFolder_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	/* Sanity checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!obj_src, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!obj_dst, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!NewFolderName, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("CopyFolder");

	size = 0;

	/* Fill the CopyFolder operation */
	request.handle_idx = 0x1;
	size += sizeof (uint8_t);
	
	request.WantAsynchronous = 0x0;
	size += sizeof (uint8_t);

	request.WantRecursive = WantRecursive;
	size += sizeof (uint8_t);

	request.UseUnicode = UseUnicode;
	size += sizeof (uint8_t);

	request.FolderId = mapi_object_get_id(obj_folder);
	size += sizeof (uint64_t);

	if (!request.UseUnicode) {
		request.NewFolderName.lpszA = NewFolderName;
		size += strlen(NewFolderName) + 1;
	} else {
		request.NewFolderName.lpszW = NewFolderName;
		size += strlen(NewFolderName) * 2 + 2;
	}

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_CopyFolder;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_CopyFolder = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_src);
	mapi_request->handles[1] = mapi_object_get_handle(obj_dst);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}
