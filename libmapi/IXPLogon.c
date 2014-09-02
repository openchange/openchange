/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2008.

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
   \file IXPLogon.c

   \brief Transport provider status information
*/


/**
   \details Returns the types of recipients that the transport
   provider handles.

   \param obj_store the object to get recipients types from
   \param lpcAdrType the count of recipients types returned
   \param lpAdrTypeArray pointer on pointer of returned transport
   provider types

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_store is not initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

 */
_PUBLIC_ enum MAPISTATUS AddressTypes(mapi_object_t *obj_store,
				      uint16_t *lpcAdrType,
				      struct mapi_LPSTR **lpAdrTypeArray)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct AddressTypes_repl	*response;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	
	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "AddressTypes");
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_AddressTypes;
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

	/* Retrieve Address Types */
	response = &mapi_response->mapi_repl->u.mapi_AddressTypes;
	*lpcAdrType = response->cValues;
	*lpAdrTypeArray = talloc_steal((TALLOC_CTX *)session, response->transport);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}


/**
   \details Informs the server that the client intends to act as a
   mail spooler.

   \param obj_store: the object server store object

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_store is not initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction 

   \sa SpoolerLockMessage
 */
_PUBLIC_ enum MAPISTATUS SetSpooler(mapi_object_t *obj_store)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	uint8_t 		logon_id = 0;
	
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "SetSpooler");
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetSpooler;
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

	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}


/**
   \details Locks the specified message for spooling.

   \param obj_store the store object
   \param obj_message the message object we want to lock
   \param lockstate the lock state

   Possible values for the lock state:
   -# LockState_1stLock (0x0): Mark the message as locked
   -# LockState_1stUnlock (0x1): Mark the message as unlocked
   -# LockState_1stFinished (0x2): Mark the message as ready for
      processing by the server

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_store is not initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction 

     \sa SetSPooler
 */
_PUBLIC_ enum MAPISTATUS SpoolerLockMessage(mapi_object_t *obj_store,
					    mapi_object_t *obj_message, 
					    enum LockState lockstate)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SpoolerLockMessage_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(lockstate > 2, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "SpoolerLockMessage");
	size = 0;

	/* Fill the SpoolerLockMessage operation */
	request.MessageId = mapi_object_get_id(obj_message);
	size += sizeof (int64_t);

	request.LockState = lockstate;
	size += sizeof (uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SpoolerLockMessage;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SpoolerLockMessage = request;
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
   \details Returns options information for the types of recipients
      that the transport provider handles.

   \param [in] obj_store the object to get recipients types from
   \param [in] addrtype string name of the address type to get options for
   \param [out] OptionsData the options data for this addrtype
   \param [out] OptionsLength length of the OptionsData array
   \param [out] HelpFile the help file data for this addrtype (often empty)
   \param [out] HelpFileLength length of the HelpFile array
   \param [out] HelpFileName the name of the help file (often null)

   The caller is responsible for talloc_free()ing the OptionsData array.
   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_store is not initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

 */
_PUBLIC_ enum MAPISTATUS OptionsData(mapi_object_t *obj_store, const char *addrtype,
				     uint8_t **OptionsData, uint16_t *OptionsLength,
				     uint8_t **HelpFile, uint16_t *HelpFileLength,
				     const char** HelpFileName)
{
	struct mapi_session             *session;
	TALLOC_CTX                      *mem_ctx;
	uint32_t                        size;
	struct OptionsData_req          request;
	struct mapi_request             *mapi_request;
	struct mapi_response            *mapi_response;
	struct EcDoRpc_MAPI_REQ         *mapi_req;
	struct OptionsData_repl         *response;
	NTSTATUS                        status;
	enum MAPISTATUS                 retval;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!addrtype, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_store, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "RecipientOptions");
	size = 0;

	/* Build the OptionsData request */
	request.AddressType = addrtype;
	size += strlen(addrtype) + 1;
	request.WantWin32 = 0x1;
	size += sizeof (uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OptionsData;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_OptionsData = request;
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

	/* Retrieve the Options Data */
	response = &mapi_response->mapi_repl->u.mapi_OptionsData;

	*OptionsLength = response->OptionsInfo.cb;
	*OptionsData = talloc_steal((TALLOC_CTX *)session, response->OptionsInfo.lpb);
	*HelpFileLength = response->HelpFileSize;
	*HelpFile = talloc_steal((TALLOC_CTX *)session, response->HelpFile);
	if (*HelpFileLength != 0) {
		*HelpFileName = talloc_steal((TALLOC_CTX *)session, response->HelpFileName.HelpFileName);
	} else {
		*HelpFileName = 0;
	}

	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}
