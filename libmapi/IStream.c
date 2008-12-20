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
   \file IStream.c

   \brief Functions for operating on Streams on MAPI objects
 */


/**
   \details Open a stream

   This function opens a stream on the property \a prop set in \a
   obj_related with access flags set to \a access_flags and returns an
   object \a obj_stream.

   \param obj_related the object to open.
   \param PropertyTag the property name for the object to create a stream
   for.
   \param OpenModeFlags sets the access mode for the stream and is one
   of the following values:
   * 0x0: ReadOnly
   * 0x1: ReadWrite
   * 0x2: Create
   * 0x3: BestAccess
   \param obj_stream the resulting stream object.

   \return MAPI_E_SUCCESS on success, otherwise -1. 

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: A problem occured obtaining the session context
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa ReadStream, WriteStream, GetLastError
 */

_PUBLIC_ enum MAPISTATUS OpenStream(mapi_object_t *obj_related, uint32_t PropertyTag,
				    uint8_t OpenModeFlags, mapi_object_t *obj_stream)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct OpenStream_req	request;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;

	/* Sanity checks */
	session = mapi_object_get_session(obj_related);
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_init("OpenStream");

	size = 0;

	/* Fill the OpenStream operation */
	request.handle_idx = 0x1;
	request.PropertyTag = PropertyTag;

	/* STGM_XXX (obj_base.h windows header) */
	request.OpenModeFlags = OpenModeFlags;
	size += sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenStream;
	mapi_req->logon_id = 0;
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

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* Set object session and handle */
	mapi_object_set_session(obj_stream, session);
	mapi_object_set_handle(obj_stream, mapi_response->handles[1]);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Read buffer from a stream

   This function reads from an open data stream. It will read up to
   ByteCount bytes from the stream, and return the data in data_buf.
   ByteRead is set to the number of bytes actually read.

   \param obj_stream the opened stream object
   \param buf_data the buffer where data read from the stream will be
   stored
   \param ByteCount the number of bytes requested to be read from the
   stream
   \param ByteRead the number of bytes read from the stream

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
				    uint16_t ByteCount, uint16_t *ByteRead)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct ReadStream_req	request;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;

	/* Sanity checks */
	session = mapi_object_get_session(obj_stream);
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_init("ReadStream");

	*ByteRead = 0;
	size = 0;

	/* Fill the ReadStream operation */
	request.ByteCount = ByteCount;
	size += sizeof(uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_ReadStream;
	mapi_req->logon_id = 0;
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

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* copy no more than sz_data into buffer */
	*ByteRead = mapi_response->mapi_repl->u.mapi_ReadStream.data.length;
	if (*ByteRead > 0) {
		if (*ByteRead > ByteCount) {
			*ByteRead = ByteCount;
		}
		memcpy(buf_data, mapi_response->mapi_repl->u.mapi_ReadStream.data.data, *ByteRead);
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
   \param WrittenSize the actual number of bytes written to the
   stream

   \return MAPI_E_SUCCESS on success, otherwise -1. 

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: A problem occured obtaining the session
     context, or blob was null.
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
   - MAPI_E_TOO_BIG: the data blob was too large to process

   \note The data size intended to be written to the stream shouldn't
   extend a maximum size each time you call WriteStream. This size
   depends on Exchange server version. However 0x1000 is known to be a
   reliable write size value.

   \sa OpenStream, ReadStream, GetLastError
  */
_PUBLIC_ enum MAPISTATUS WriteStream(mapi_object_t *obj_stream, DATA_BLOB *blob, uint16_t *WrittenSize)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct WriteStream_req	request;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	uint32_t		size;

	/* Sanity Checks */
	session = mapi_object_get_session(obj_stream);
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!blob, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(blob->length > 0x7000, MAPI_E_TOO_BIG, NULL);

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
	mapi_req->logon_id = 0;
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

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	*WrittenSize = mapi_response->mapi_repl->u.mapi_WriteStream.WrittenSize;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	
	errno = 0;
	return MAPI_E_SUCCESS;
}


/**
   \details Commits stream operations

   \param obj_stream the stream object to commit

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: Either the network stream or session
   context are not valid.
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
   
   \sa OpenStream, ReadStream, WriteStream
 */
_PUBLIC_ enum MAPISTATUS CommitStream(mapi_object_t *obj_stream)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;

	/* Sanity checks */
	session = mapi_object_get_session(obj_stream);
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_stream, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_init("CommitStream");
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_CommitStream;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_stream);

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Gets the size of a stream

   \param obj_stream the stream object we retrieve size from
   \param StreamSize pointer on the stream size

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_BOOKMARK: the bookmark specified is invalid or
     beyond the last row requested.
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
   
   \sa OpenStream, ReadStream
 */
_PUBLIC_ enum MAPISTATUS GetStreamSize(mapi_object_t *obj_stream, uint32_t *StreamSize)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;

	/* Sanity checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_stream, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_stream);
	MAPI_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_init("GetStreamSize");
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetStreamSize;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_stream);

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	*StreamSize = mapi_response->mapi_repl->u.mapi_GetStreamSize.StreamSize;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Seek a specific position within the stream

   \param obj_stream the stream object
   \param Origin origin location for the seek operation
   \param Offset the seek offset
   \param NewPosition pointer on the new position after the operation

   Origin can either take one of the following values:

   * 0x0 The new seek pointer is an offset relative to the beginning
   of the stream.
   * 0x1 The new seek pointer is an offset relative to the current
   seek pointer location.
   * 0x2 The new seek pointer is an offset relative to the end of the
   stream.

   \return MAPI_E_SUCCESS on success, otherwise -1

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_BOOKMARK: the bookmark specified is invalid or
     beyond the last row requested.
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
   
   \sa OpenStream, ReadStream 
 */
_PUBLIC_ enum MAPISTATUS SeekStream(mapi_object_t *obj_stream, uint8_t Origin, uint64_t Offset, 
				    uint64_t *NewPosition)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SeekStream_req	request;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	uint32_t		size;

	/* Sanity checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_stream, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_stream);
	MAPI_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF((Origin > 2), MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!NewPosition, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_init("SeekStream");
	size = 0;

	/* Fill the SeekStream operation */
	request.Origin = Origin;
	size += sizeof (uint8_t);

	request.Offset = Offset;
	size += sizeof (uint64_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SeekStream;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SeekStream = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_stream);

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	*NewPosition = mapi_response->mapi_repl->u.mapi_SeekStream.NewPosition;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Set the stream size

   \param obj_stream the stream object
   \param SizeStream the size of the stream

   \return MAPI_E_SUCCESS on success, otherwise -1

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_BOOKMARK: the bookmark specified is invalid or
     beyond the last row requested.
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
   
   \sa OpenStream, GetStreamSize
 */
_PUBLIC_ enum MAPISTATUS SetStreamSize(mapi_object_t *obj_stream, uint64_t SizeStream)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SetStreamSize_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	TALLOC_CTX			*mem_ctx;
	uint32_t			size;

	/* Sanity checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_stream, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_stream);
	MAPI_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_init("SetStreamSize");
	size = 0;

	/* Fill the SetStreamSize operation */
	request.SizeStream = SizeStream;
	size += sizeof (uint64_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetStreamSize;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SetStreamSize = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_stream);

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Copy a number of bytes from a source stream to another
   stream

   \param obj_src the source stream object
   \param obj_dst the destination stream object
   \param ByteCount the number of bytes to copy
   \param ReadByteCount pointer on the number of bytes read from the
   source object
   \param WrittenByteCount pointer on the number of bytes written to
   the destination object
 
   \return MAPI_E_SUCCESS on success, otherwise -1.

   \return MAPI_E_SUCCESS on success, otherwise -1

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_BOOKMARK: the bookmark specified is invalid or
     beyond the last row requested.
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
   
   \sa OpenStream
*/
_PUBLIC_ enum MAPISTATUS CopyToStream(mapi_object_t *obj_src, mapi_object_t *obj_dst,
				      uint64_t ByteCount, uint64_t *ReadByteCount,
				      uint64_t *WrittenByteCount)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct CopyToStream_req	request;
	struct mapi_session	*session[2];
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	uint32_t		size;

	/* Sanity Check */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_src, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!obj_dst, MAPI_E_INVALID_PARAMETER, NULL);

	session[0] = mapi_object_get_session(obj_src);
	session[1] = mapi_object_get_session(obj_dst);

	MAPI_RETVAL_IF(!session[0], MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!session[1], MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(session[0] != session[1], MAPI_E_INVALID_PARAMETER, NULL);

	MAPI_RETVAL_IF(!ByteCount, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!ReadByteCount, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!WrittenByteCount, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_init("CopyToStream");
	size = 0;

	/* Fill the CopyToStream operation */
	request.handle_idx = 0x1;
	size += sizeof (uint8_t);

	request.ByteCount = ByteCount;
	size += sizeof (uint64_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_CopyToStream;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_CopyToStream = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_src);
	mapi_request->handles[1] = mapi_object_get_handle(obj_dst);

	status = emsmdb_transaction(session[0]->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	*ReadByteCount = mapi_response->mapi_repl->u.mapi_CopyToStream.ReadByteCount;
	*WrittenByteCount = mapi_response->mapi_repl->u.mapi_CopyToStream.WrittenByteCount;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
