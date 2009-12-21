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


/**
   \file IStoreFolder.c
 
   \brief Open messages
*/


/**
   \details Opens a specific message and retrieves a MAPI object that
   can be used to get or set message properties.

   This function opens a specific message defined by a combination of
   object store, folder ID, and message ID and which read/write access
   is defined by ulFlags.

   \param obj_store the store to read from
   \param id_folder the folder ID
   \param id_message the message ID
   \param obj_message the resulting message object
   \param ulFlags

   Possible ulFlags values:
   - 0x0: read only access
   - 0x1: ReadWrite
   - 0x3: Create

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_store is undefined
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa MAPIInitialize, GetLastError
*/
_PUBLIC_ enum MAPISTATUS OpenMessage(mapi_object_t *obj_store, 
				     mapi_id_t id_folder, 
				     mapi_id_t id_message, 
				     mapi_object_t *obj_message,
				     uint8_t ulFlags)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct OpenMessage_req		request;
	struct OpenMessage_repl		*reply;
	struct mapi_session		*session;
	mapi_object_message_t		*message;
	struct SPropValue		lpProp;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint32_t			i = 0;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(NULL, 0, "OpenMessage");

	/* Fill the OpenMessage operation */
	request.handle_idx = 0x1;
	request.CodePageId = 0xfff;
	request.FolderId = id_folder;
	request.OpenModeFlags = ulFlags;
	request.MessageId = id_message;
	size = sizeof (uint8_t) + sizeof(uint16_t) + sizeof(mapi_id_t) + sizeof(uint8_t) + sizeof(mapi_id_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenMessage;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_OpenMessage = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_store);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(session->emsmdb->ctx, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Set object session and handle */
	mapi_object_set_session(obj_message, session);
	mapi_object_set_handle(obj_message, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_message, logon_id);

	/* Store OpenMessage reply data */
	reply = &mapi_response->mapi_repl->u.mapi_OpenMessage;

	message = talloc_zero((TALLOC_CTX *)session, mapi_object_message_t);
	message->cValues = reply->RecipientColumns.cValues;
	message->SRowSet.cRows = reply->RowCount;
	message->SRowSet.aRow = talloc_array((TALLOC_CTX *)message, struct SRow, reply->RowCount + 1);

	message->SPropTagArray.cValues = reply->RecipientColumns.cValues;
	message->SPropTagArray.aulPropTag = talloc_steal(message, reply->RecipientColumns.aulPropTag);

	for (i = 0; i < reply->RowCount; i++) {
		emsmdb_get_SRow((TALLOC_CTX *)message, global_mapi_ctx->lp_ctx,
				&(message->SRowSet.aRow[i]), &message->SPropTagArray, 
				reply->recipients[i].RecipientRow.prop_count,
				&reply->recipients[i].RecipientRow.prop_values,
				reply->recipients[i].RecipientRow.layout, 1);

		lpProp.ulPropTag = PR_RECIPIENT_TYPE;
		lpProp.value.l = reply->recipients[i].RecipClass;
		SRow_addprop(&(message->SRowSet.aRow[i]), lpProp);

		lpProp.ulPropTag = PR_INTERNET_CPID;
		lpProp.value.l = reply->recipients[i].codepage;
		SRow_addprop(&(message->SRowSet.aRow[i]), lpProp);
	}

	/* add SPropTagArray elements we automatically append to SRow */
	SPropTagArray_add((TALLOC_CTX *)message, &message->SPropTagArray, PR_RECIPIENT_TYPE);
	SPropTagArray_add((TALLOC_CTX *)message, &message->SPropTagArray, PR_INTERNET_CPID);

	obj_message->private_data = (void *) message;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Retrieve the message properties for an already open message.

   This function is very similar to OpenMessage, but works on an already
   open message object.

   \param obj_message the message object to retrieve the properties for.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_store is undefined
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenMessage
*/
_PUBLIC_ enum MAPISTATUS ReloadCachedInformation(mapi_object_t *obj_message)
{
	struct mapi_request			*mapi_request;
	struct mapi_response			*mapi_response;
	struct EcDoRpc_MAPI_REQ			*mapi_req;
	struct ReloadCachedInformation_req	request;
	struct ReloadCachedInformation_repl	*reply;
	struct mapi_session			*session;
	mapi_object_message_t			*message;
	struct SPropValue			lpProp;
	NTSTATUS				status;
	enum MAPISTATUS				retval;
	uint32_t				size = 0;
	TALLOC_CTX				*mem_ctx;
	uint32_t				i = 0;
	uint8_t					logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_message);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_message, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(NULL, 0, "ReloadCachedInformation");

	/* Fill the ReloadCachedInformation operation */
	request.Reserved = 0x0000;
	size += sizeof (uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_ReloadCachedInformation;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_ReloadCachedInformation = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_message);

	status = emsmdb_transaction(session->emsmdb->ctx, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;

	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Store ReloadCachedInformation reply data */
	reply = &mapi_response->mapi_repl->u.mapi_ReloadCachedInformation;

	message = talloc_zero((TALLOC_CTX *)session, mapi_object_message_t);
	message->cValues = reply->RecipientColumns.cValues;
	message->SRowSet.cRows = reply->RowCount;
	message->SRowSet.aRow = talloc_array((TALLOC_CTX *)message, struct SRow, reply->RowCount + 1);

	message->SPropTagArray.cValues = reply->RecipientColumns.cValues;
	message->SPropTagArray.aulPropTag = talloc_steal(message, reply->RecipientColumns.aulPropTag);

	for (i = 0; i < reply->RowCount; i++) {
		emsmdb_get_SRow((TALLOC_CTX *)message, global_mapi_ctx->lp_ctx,
				&(message->SRowSet.aRow[i]), &message->SPropTagArray, 
				reply->RecipientRows[i].RecipientRow.prop_count,
				&reply->RecipientRows[i].RecipientRow.prop_values,
				reply->RecipientRows[i].RecipientRow.layout, 1);

		lpProp.ulPropTag = PR_RECIPIENT_TYPE;
		lpProp.value.l = reply->RecipientRows[i].RecipientType;
		SRow_addprop(&(message->SRowSet.aRow[i]), lpProp);

		lpProp.ulPropTag = PR_INTERNET_CPID;
		lpProp.value.l = reply->RecipientRows[i].CodePageId;
		SRow_addprop(&(message->SRowSet.aRow[i]), lpProp);
	}

	/* add SPropTagArray elements we automatically append to SRow */
	SPropTagArray_add((TALLOC_CTX *)message, &message->SPropTagArray, PR_RECIPIENT_TYPE);
	SPropTagArray_add((TALLOC_CTX *)message, &message->SPropTagArray, PR_INTERNET_CPID);

	talloc_free(obj_message->private_data);
	obj_message->private_data = (void *) message;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	errno = 0;
	return MAPI_E_SUCCESS;
}
