/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2011.

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
   \file IUnknown.c

   \brief Various miscellaneous (ungrouped) functions
 */


/**
   \details Allocate memory using the MAPI memory context
   
   \param mapi_ctx pointer to the MAPI context
   \param size the number of bytes to allocate
   \param ptr pointer to the allocated byte region

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_SESSION_LIMIT: No session has been opened on the provider
   - MAPI_E_NOT_ENOUGH_RESOURCES: MAPI subsystem failed to allocate
     the necessary resources to operate properly
   - MAPI_E_INVALID_PARAMER: size is not set properly.

   \sa MAPIFreeBuffer, GetLastError
*/
_PUBLIC_ enum MAPISTATUS MAPIAllocateBuffer(struct mapi_context *mapi_ctx, uint32_t size, void **ptr)
{
	TALLOC_CTX	*mem_ctx;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = (TALLOC_CTX *) mapi_ctx->mem_ctx;

	*ptr = talloc_size(mem_ctx, size);
	OPENCHANGE_RETVAL_IF(!ptr, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Free allocated memory

   This function frees memory previously allocated with
   MAPIAllocateBuffer.

   \param ptr memory region to free
       
   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMER: ptr is not set properly.

   \sa MAPIAllocateBuffer, GetLastError
*/
_PUBLIC_ enum MAPISTATUS MAPIFreeBuffer(void *ptr)
{
	int		ret;

	OPENCHANGE_RETVAL_IF(!ptr, MAPI_E_INVALID_PARAMETER, NULL);

	ret = talloc_free(ptr);
	OPENCHANGE_RETVAL_IF(ret == -1, MAPI_E_INVALID_PARAMETER, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Release an object on the server

   The function releases the object \a obj on the server.

   \param obj the object to release

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetLastError
*/
_PUBLIC_ enum MAPISTATUS Release(mapi_object_t *obj)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ *mapi_req;
	struct mapi_session	*session;
	NTSTATUS		status;
	TALLOC_CTX		*mem_ctx;
	uint32_t		size = 0;
	enum MAPISTATUS		retval;
	uint8_t 		logon_id = 0;

	/* Sanity checks */
	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "Release");

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_Release;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);

	if (mapi_response->mapi_repl) {
		OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);
	}

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	errno = 0;
	return MAPI_E_SUCCESS;
}


/**
   \details Returns the latest error code.

   This function returns the error code set by a previous function
   call.

   \note Calls to this function may not work reliably in multi-threaded or
   multisession code. We suggest you capture the return value of the call,
   and check that instead.
*/
_PUBLIC_ enum MAPISTATUS GetLastError(void)
{
  return (enum MAPISTATUS)errno;
}


/**
   \details Convert an ID to a Long Term Id

   The function looks up the Long Term Id for a specified ID value.

   \param obj the object to look up on
   \param id the id to look up
   \param long_term_id the long term ID returned from the server

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj is null
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetIdFromLongTermId
*/
_PUBLIC_ enum MAPISTATUS GetLongTermIdFromId(mapi_object_t *obj, mapi_id_t id,
					     struct LongTermId *long_term_id)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ 	*mapi_req;
	struct LongTermIdFromId_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	TALLOC_CTX			*mem_ctx;
	uint32_t			size = 0;
	enum MAPISTATUS			retval;
	int				i;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "LongTermIdFromId");

	/* Fill the LongTermIdFromId operation */
	request.Id = id;
	size += sizeof(mapi_id_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_LongTermIdFromId;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_LongTermIdFromId = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	long_term_id->DatabaseGuid = mapi_response->mapi_repl->u.mapi_LongTermIdFromId.LongTermId.DatabaseGuid;
	for (i = 0; i < 6; ++i) {
		long_term_id->GlobalCounter[i] = mapi_response->mapi_repl->u.mapi_LongTermIdFromId.LongTermId.GlobalCounter[i];
	}
	long_term_id->padding = 0x0;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	errno = 0;
	return MAPI_E_SUCCESS;
}


/**
   \details Convert an Long Term Id into an Id

   The function looks up the Id for a specified Long Term Id value.

   \param obj the object to look up on
   \param long_term_id the id to look up
   \param id the id returned by the server

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj is null
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetLongTermIdFromId
*/
_PUBLIC_ enum MAPISTATUS GetIdFromLongTermId(mapi_object_t *obj, struct LongTermId long_term_id,
					     mapi_id_t *id)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ 	*mapi_req;
	struct IdFromLongTermId_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	TALLOC_CTX			*mem_ctx;
	uint32_t			size = 0;
	enum MAPISTATUS			retval;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "IdFromLongTermId");
	size = 0;

	/* Fill the IdFromLongTermId operation */
	request.LongTermId = long_term_id;
	size += sizeof(struct LongTermId);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_IdFromLongTermId;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_IdFromLongTermId = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = (uint16_t)size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	*id = mapi_response->mapi_repl->u.mapi_IdFromLongTermId.Id;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	errno = 0;
	return MAPI_E_SUCCESS;
}
