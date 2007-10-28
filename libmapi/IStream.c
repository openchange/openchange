/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007.

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
   \file IStream.c

   \brief Functions for operating on Streams on MAPI objects
 */


/**
   \details Open a stream

   This function opens a stream on the property \a prop set in \a
   obj_related with access flags set to \a access_flags and returns an
   object \a obj_stream.

   \param obj_related the object to open.
   \param prop the property name for the object to create a stream
   for.
   \param access_flags sets the access mode for the stream and is one
   of the following values: 0 for reading, 1 for writing, 2 for read
   and write access.
   \param obj_stream the resulting stream object.

   \return MAPI_E_SUCCESS on success, otherwise -1. 

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa ReadStream, WriteStream, GetLastError
 */

_PUBLIC_ enum MAPISTATUS OpenStream(mapi_object_t *obj_related, uint32_t prop,
				    uint32_t access_flags,
				    mapi_object_t *obj_stream)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct OpenStream_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("OpenStream");

	size = 0;

	/* Fill the OpenStream operation */
	request.stream_handle_idx = 0x1;
	request.prop = prop;

	/* STGM_XXX (obj_base.h windows header) */
	request.access_flags = access_flags;
	size += sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenStream;
	mapi_req->mapi_flags = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_OpenStream = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_related);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	mapi_object_set_handle(obj_stream, mapi_response->handles[1]);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Read buffer from a stream

   This function reads the stream opened in and reads data_size.  Data
   is returned within data and read_size is set to the size of data
   read.

   \param obj_stream the opened stream object
   \param buf_data the buffer where data read from the stream will be
   stored
   \param sz_data the number of bytes requested to be read from the
   stream
   \param sz_read the number of bytes effectively read from the stream

   \return MAPI_E_SUCCESS on success, otherwise -1. 

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \note The data size intended to be read from the stream shouldn't
   extend a maximum size each time you call ReadStream. This size
   depends on Exchange server version. However 0x1000 is known to be a
   reliable read size value.

   \sa OpenStream, WriteStream, GetLastError
*/
_PUBLIC_ enum MAPISTATUS ReadStream(mapi_object_t *obj_stream, unsigned char *buf_data, 
				    int sz_data, uint32_t *sz_read)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct ReadStream_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("ReadStream");

	*sz_read = 0;
	size = 0;

	/* Fill the ReadStream operation */
	request.size = sz_data;
	size += sizeof(uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_ReadStream;
	mapi_req->mapi_flags = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_ReadStream = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 1;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_stream);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* copy no more than sz_data into buffer */
	*sz_read = mapi_response->mapi_repl->u.mapi_ReadStream.data.length;
	if (*sz_read > 0) {
		if (*sz_read > sz_data) {
			*sz_read = sz_data;
		}
		memcpy(buf_data, mapi_response->mapi_repl->u.mapi_ReadStream.data.data, *sz_read);
	}

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Write buffer to the stream

   This function writes the stream specified as a DATA_BLOB in data to
   the stream obj_stream.

   \param obj_stream the opened stream object
   \param blob the DATA_BLOB to write to the stream
   \param write_size the effective number of bytes written to the
   stream

   \return MAPI_E_SUCCESS on success, otherwise -1. 

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \note The data size intended to be written to the stream shouldn't
   extend a maximum size each time you call WriteStream. This size
   depends on Exchange server version. However 0x1000 is known to be a
   reliable write size value.

   \sa OpenStream, ReadStream, GetLastError
  */
_PUBLIC_ enum MAPISTATUS WriteStream(mapi_object_t *obj_stream, DATA_BLOB *blob, uint16_t *write_size)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct WriteStream_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;
	uint32_t		size;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!blob, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(blob->length > 0x7000, MAPI_E_TOO_BIG, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("WriteStream");

	size = 0;

	/* Fill the WriteStream operation */
	request.data = *blob;
	size +=  blob->length;
	/* size for subcontext(2) */
	size += 2;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_WriteStream;
	mapi_req->mapi_flags = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_WriteStream = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_stream);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	*write_size = mapi_response->mapi_repl->u.mapi_WriteStream.size;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	
	errno = 0;
	return MAPI_E_SUCCESS;
}
