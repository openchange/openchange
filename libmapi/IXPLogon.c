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

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <gen_ndr/ndr_exchange.h>


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

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
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
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	mapi_ctx_t			*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("AddressTypes");
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_AddressTypes;
	mapi_req->mapi_flags = 0;
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

	/* Retrieve Address Types */
	response = &mapi_response->mapi_repl->u.mapi_AddressTypes;
	*lpcAdrType = response->cValues;
	*lpAdrTypeArray = response->transport;

	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}
