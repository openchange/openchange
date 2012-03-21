/*
   OpenChange MAPI implementation.

   Copyright (C) Brad Hards <bradh@openchange.org> 2011.

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
   \file notif.c

   \brief Notification (MS-OXCNOTIF) operations
 */


/**
   \details Obtain an ICS notification object

   This function is used to obtain a server object handle for an ICS notification 
   operation (RegisterSyncNotifications or SetSyncNotificationGuid). This operation
   is not supported on Exchange 2010.

   \param obj the logon object for which notifications are desired
   \param obj_notifier the notifier object for future ROPs.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   The caller should release the returned notifier object when it is no longer
   required, using the mapi_object_release function.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameters is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS SyncOpenAdvisor(mapi_object_t *obj, mapi_object_t *obj_notifier)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SyncOpenAdvisor_req	request;
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
	OPENCHANGE_RETVAL_IF(!obj_notifier, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, __FUNCTION__);

	/* Fill the SyncOpenAdvisor operation */
	request.handle_idx = 0x01;
	size += sizeof(uint8_t);

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SyncOpenAdvisor;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SyncOpenAdvisor = request;
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
	mapi_object_set_session(obj_notifier, session);
	mapi_object_set_handle(obj_notifier, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_notifier, logon_id);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Assign a notification GUID to an ICS Advisor object

   This function allows the client to set a specific GUID to an ICS
   advistor object (as returned from SyncOpenAdvisor). This operation
   is not supported on Exchange 2010.
   
   \param obj_advisor pointer to the ICS Advisor object
   \param Guid the GUID for the notification

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the function parameters is
     invalid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS SetSyncNotificationGuid(mapi_object_t *obj_advisor, 
						      const struct GUID Guid)
{
	struct mapi_request				*mapi_request;
	struct mapi_response				*mapi_response;
	struct EcDoRpc_MAPI_REQ				*mapi_req;
	struct SetSyncNotificationGuid_req		request;
	struct mapi_session				*session;
	NTSTATUS					status;
	enum MAPISTATUS					retval;
	uint32_t					size = 0;
	TALLOC_CTX					*mem_ctx;
	uint8_t 					logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_advisor, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_advisor);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_advisor, &logon_id)) != MAPI_E_SUCCESS) {
		return retval;
	}

	mem_ctx = talloc_named(session, 0, __FUNCTION__);

	/* Fill the SetSyncNotificationGuid operation */
	request.NotificationGuid = Guid;
	size += sizeof(struct GUID);

	/* Fill the MAPI_REQ structure */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetSyncNotificationGuid;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SetSyncNotificationGuid = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_advisor);

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

