/*
 *  OpenChange MAPI implementation.
 *
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
#include <gen_ndr/ndr_exchange.h>

/**
 * Open a stream
 */

enum MAPISTATUS	OpenStream(struct emsmdb_context *emsmdb, uint8_t access_flags, uint32_t prop, uint32_t hdl_object, uint32_t *hdl_result)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct OpenStream_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
 
	mem_ctx = talloc_init("OpenStream");

	*hdl_result = 0;
	size = 0;

	/* Fill the OpenStream operation */
	request.handle_idx = 0x0;
	request.stream_handle_idx = 0x1;
	request.prop = prop;
	request.access_flags = access_flags;
	size += sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenStream;
	mapi_req->mapi_flags = access_flags;
	mapi_req->u.mapi_OpenStream = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = hdl_object;
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	*hdl_result = mapi_response->handles[1];

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}



/**
 * Read buffer from a stream
 */

enum MAPISTATUS	ReadStream(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t hdl_stream, unsigned char *buf_data, int sz_data, uint32_t *sz_read)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct ReadStream_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
 
	mem_ctx = talloc_init("ReadStream");

	*sz_read = 0;
	size = 0;

	/* Fill the ReadStream operation */
	request.unknown = 0x0;
	size += sizeof(uint8_t);
	request.size = sz_data;
	size += sizeof(uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_ReadStream;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_ReadStream = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 1;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = hdl_stream;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	/* copy no more than sz_data into buffer */
	*sz_read = mapi_response->mapi_repl->u.mapi_ReadStream.data.length;
	if (*sz_read > 0) {
	    if (*sz_read > sz_data)
	      *sz_read = sz_data;
	    memcpy(buf_data, mapi_response->mapi_repl->u.mapi_ReadStream.data.data, *sz_read);
	  }

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
