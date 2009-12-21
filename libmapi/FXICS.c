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
   - MAPI_E_INVALID_PARAMETER: one of the function parameter is
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

	status = emsmdb_transaction(session->emsmdb->ctx, mem_ctx, mapi_request, &mapi_response);
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
