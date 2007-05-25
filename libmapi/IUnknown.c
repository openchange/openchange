/*
 *  Copyright (C) Julien Kerihuel 2007.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <gen_ndr/ndr_exchange.h>

_PUBLIC_ enum MAPISTATUS MAPIAllocateBuffer(uint32_t size, void **ptr)
{
	TALLOC_CTX	*mem_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!global_mapi_ctx->session, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = (TALLOC_CTX *) global_mapi_ctx->session;

	*ptr = talloc_size(mem_ctx, size);
	MAPI_RETVAL_IF(!ptr, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);

	return MAPI_E_SUCCESS;
}

_PUBLIC_ enum MAPISTATUS MAPIFreeBuffer(void *ptr)
{
	uint32_t	ret;

	MAPI_RETVAL_IF(!ptr, MAPI_E_INVALID_PARAMETER, NULL);

	ret = talloc_free(ptr);
	MAPI_RETVAL_IF(ret == -1, MAPI_E_INVALID_PARAMETER, NULL);

	return MAPI_E_SUCCESS;
}

_PUBLIC_ enum MAPISTATUS Release(mapi_object_t *obj)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ *mapi_req;
	NTSTATUS		status;
	TALLOC_CTX		*mem_ctx;
	uint32_t		size = 0;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("Release");

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_Release;
	mapi_req->mapi_flags = 0;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	errno = 0;
	return MAPI_E_SUCCESS;
}

/*
 * GetLastError returns the latest error code.
 * If it is needed by openchange, we will return some kind of
 * MAPIERROR structure in the future.
 */
_PUBLIC_ enum MAPISTATUS GetLastError(void)
{
	return errno;
}
