/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2008.
   Copyright (C) Brad Hards 2008.

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
   \file IMAPITable.c

   \brief Operations on tables
 */


/**
   \details Defines the particular properties and order of properties
  to appear as columns in the table.
 
  \param obj_table the table the function is setting columns for
  \param properties the properties intended to be set

  \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table is NULL
   - MAPI_W_ERROR_RETURNED: Problem encountered while trying to set
   one or more properties
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

  \sa QueryRows, QueryColumns, SeekRow, GetLastError
 */
_PUBLIC_ enum MAPISTATUS SetColumns(mapi_object_t *obj_table, 
				    struct SPropTagArray *properties)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SetColumns_req	request;
	struct mapi_session	*session;
	TALLOC_CTX		*mem_ctx;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	mapi_object_table_t	*table;
	uint8_t 		logon_id = 0;

	/* sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	
	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "SetColumns");
	size = 0;

	/* Fill the SetColumns operation */
	request.SetColumnsFlags = SetColumns_TBL_SYNC;
	request.prop_count = properties->cValues;
	request.properties = properties->aulPropTag;
	size += 3 + request.prop_count * sizeof (uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetColumns;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SetColumns = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval && (retval != MAPI_W_ERRORS_RETURNED), retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* recopy property tags into table */
	/* fixme: obj_table->private_data should be initialized during opening, not here */
	if (obj_table->private_data == NULL) {
		obj_table->private_data = talloc((TALLOC_CTX *)session, mapi_object_table_t);
	}

	table = (mapi_object_table_t *)obj_table->private_data;
	if (table) {
		table->proptags.cValues = properties->cValues;
		table->proptags.aulPropTag = talloc_array((TALLOC_CTX *) table,
							  enum MAPITAGS, table->proptags.cValues);
		memcpy((void*)table->proptags.aulPropTag, (void*)properties->aulPropTag,
		       table->proptags.cValues * sizeof(enum MAPITAGS));
	}

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Returns the approximate cursor position
 
   \param obj_table pointer to the table's object
   \param Numerator pointer to the numerator of the fraction
   identifying the table position
   \param Denominator pointer to the denominator of the fraction
   identifying the table position

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table is NULL
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa QueryRows
*/
_PUBLIC_ enum MAPISTATUS QueryPosition(mapi_object_t *obj_table, 
				       uint32_t *Numerator,
				       uint32_t *Denominator)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	uint8_t 		logon_id = 0;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "QueryPosition");
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_QueryPosition;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);
	
	if (Numerator) {
		*Numerator = mapi_response->mapi_repl->u.mapi_QueryPosition.Numerator;
	}

	if (Denominator) {
		*Denominator = mapi_response->mapi_repl->u.mapi_QueryPosition.Denominator;
	}
	
	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Returns a RowSet with the properties returned by the
   server

   \param obj_table the table we are requesting properties from
   \param row_count the maximum number of rows to retrieve
   \param flags flags to use for the query
   \param forward_read the direction to read
   \param rowSet the results

   flags possible values:
   - TBL_ADVANCE: index automatically increased from last rowcount
   - TBL_NOADVANCE: should be used for a single QueryRows call
   - TBL_ENABLEPACKEDBUFFERS: (not yet implemented)

   forward_read possible values:
   - TBL_FORWARD_READ: to read forwards
   - TBL_BACKWARD_READ: to read backwards

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table is NULL
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa SetColumns, QueryPosition, QueryColumns, SeekRow
 */
_PUBLIC_ enum MAPISTATUS QueryRows(mapi_object_t *obj_table, 
				   uint16_t row_count,
				   enum QueryRowsFlags flags,
				   enum ForwardRead forward_read,
				   struct SRowSet *rowSet)
{
	struct mapi_context	*mapi_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct QueryRows_req	request;
	struct QueryRows_repl	*reply;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_object_table_t	*table;
	uint8_t 		logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = session->mapi_ctx;
	OPENCHANGE_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "QueryRows");
	size = 0;

	/* Fill the QueryRows operation */
	request.QueryRowsFlags = flags;
	request.ForwardRead = forward_read;
	request.RowCount = row_count;
	size += 4;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_QueryRows;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_QueryRows = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* table contains mapitags from previous SetColumns */
	table = (mapi_object_table_t *)obj_table->private_data;
	OPENCHANGE_RETVAL_IF(!table, MAPI_E_INVALID_OBJECT, mem_ctx);

	/* TODO: handle Origin */
	reply = &mapi_response->mapi_repl->u.mapi_QueryRows;
	rowSet->cRows = reply->RowCount;
	rowSet->aRow = talloc_array((TALLOC_CTX *)table, struct SRow, rowSet->cRows);
	emsmdb_get_SRowSet((TALLOC_CTX *)rowSet->aRow, rowSet, &table->proptags, &reply->RowData);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieves the set of columns defined in the current table
   view

   \param obj_table the table we are retrieving columns from
   \param cols pointer to an array of property tags

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table is NULL
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa SetColumns, QueryRows
*/
_PUBLIC_ enum MAPISTATUS QueryColumns(mapi_object_t *obj_table, 
				      struct SPropTagArray *cols)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct QueryColumnsAll_repl	*reply;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	mapi_object_table_t		*table;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	
	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "QueryColumns");

	cols->cValues = 0;
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_QueryColumnsAll;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* get columns SPropTagArray */
	table = (mapi_object_table_t *)obj_table->private_data;
	OPENCHANGE_RETVAL_IF(!table, MAPI_E_INVALID_OBJECT, mem_ctx);

	reply = &mapi_response->mapi_repl->u.mapi_QueryColumnsAll;
	cols->cValues = reply->PropertyTagCount;
	cols->aulPropTag = talloc_array((TALLOC_CTX *)table, enum MAPITAGS, cols->cValues);
	memcpy((void *)cols->aulPropTag, (const void *)reply->PropertyTags, cols->cValues * sizeof(enum MAPITAGS));

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Move the table cursor at a specific location

   \param obj_table the table we are moving cursor on
   \param origin the table position where we start to seek 
   \param offset a particular offset in the table
   \param row the position of the seeked row is returned in rows

   origin possible values:
   - BOOKMARK_BEGINNING: Beginning of the table
   - BOOKMARK_CURRENT: Current position in the table
   - BOKMARK_END: End of the table

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table is NULL
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa SetColumns, QueryRows
*/
_PUBLIC_ enum MAPISTATUS SeekRow(mapi_object_t *obj_table, 
				 enum BOOKMARK origin, 
				 int32_t offset, uint32_t *row)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SeekRow_req	request;
	struct SeekRow_repl	*reply;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	uint8_t 		logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "SeekRow");
	*row = 0;

	/* Fill the SeekRow operation */
	size = 0;
	request.origin = origin;
	size += 1;
	request.offset = offset;
	size += 4;
	request.WantRowMovedCount = 0;
	size += 1;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SeekRow;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SeekRow = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	reply = &mapi_response->mapi_repl->u.mapi_SeekRow;
	*row = reply->RowsSought;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Move the table cursor at a specific location given a
   bookmark

   \param obj_table the table we are moving cursor on
   \param lpbkPosition the bookmarked position 
   \param RowCount a relative number of rows to the bookmark
   \param row the position of the seeked row is returned in rows

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table is NULL
   - MAPI_E_INVALID_BOOKMARK: The bookmark specified is invalid or
     beyond the last row requested
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa CreateBookmark, FreeBookmark
 */
_PUBLIC_ enum MAPISTATUS SeekRowBookmark(mapi_object_t *obj_table,
					 uint32_t lpbkPosition,
					 uint32_t RowCount,
					 uint32_t *row)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SeekRowBookmark_req	request;
	struct SeekRowBookmark_repl	*reply;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	struct SBinary_short   		bin;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	retval = mapi_object_bookmark_find(obj_table, lpbkPosition, &bin);
	OPENCHANGE_RETVAL_IF(retval, MAPI_E_INVALID_BOOKMARK, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "SeekRowBookmark");

	/* Fill the SeekRowBookmark operation */
	size = 0;
	request.Bookmark.cb = bin.cb;
	size += sizeof (uint16_t);
	request.Bookmark.lpb = bin.lpb;
	size += bin.cb;
	request.RowCount = RowCount;
	size += sizeof (uint32_t);
	request.WantRowMovedCount = 0x1;
	size += sizeof (uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SeekRowBookmark;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SeekRowBookmark = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	reply = &mapi_response->mapi_repl->u.mapi_SeekRowBookmark;
	*row = reply->RowsSought;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Moves the cursor to an approximate fractional position in
   the table

   \param obj_table the table we are moving cursor on
   \param ulNumerator numerator of the fraction representing the table
   position.
   \param ulDenominator denominator of the fraction representing the
   table position

   - If ulDenominator is NULL, then SeekRowApprox returns
   MAPI_E_INVALID_PARAMETER.
   - If ulNumerator is NULL, then SeekRowApprox moves the cursor to
     the beginning of the table. In such situation, SeekRowApprox call
     is similar to SeekRow with BOOKMARK_BEGINNING
   - If ulNumerator and ulDenominator have the same value, then
     SeekRowApprox moves the cursor to the end of the table. In such
     situation, SeekRowApprox call is similar to SeekRow with
     BOOKMARK_END

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table is NULL
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa SeekRow, SeekRowBookmark
 */
_PUBLIC_ enum MAPISTATUS SeekRowApprox(mapi_object_t *obj_table,
				       uint32_t ulNumerator,
				       uint32_t ulDenominator)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SeekRowApprox_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ulDenominator, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "SeekRowApprox");
	
	/* Fill the SeekRowApprox operation */
	size = 0;
	request.ulNumerator = ulNumerator;
	size += sizeof (uint32_t);
	request.ulDenominator = ulDenominator;
	size += sizeof (uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SeekRowApprox;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SeekRowApprox = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

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
   \details Marks the table current position

   \param obj_table the table we are creating a bookmark in
   \param lpbkPosition pointer to the bookmark value. This bookmark
   can be passed in a call to the SeekRowBookmark method

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table is NULL
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
   
   \sa SeekRowBookmark, FreeBookmark
 */
_PUBLIC_ enum MAPISTATUS CreateBookmark(mapi_object_t *obj_table, 
					uint32_t *lpbkPosition)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct CreateBookmark_repl	*reply;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	mapi_object_table_t	       	*mapi_table;
	mapi_object_bookmark_t		*bookmark;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "CreateBookmark");	
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_CreateBookmark;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	reply = &mapi_response->mapi_repl->u.mapi_CreateBookmark;

	mapi_table = (mapi_object_table_t *)obj_table->private_data;
	OPENCHANGE_RETVAL_IF(!mapi_table, MAPI_E_INVALID_PARAMETER, mem_ctx);

	/* Store CreateBookmark data in mapi_object_table private_data */
	bookmark = talloc_zero((TALLOC_CTX *)mapi_table->bookmark, mapi_object_bookmark_t);
	mapi_table->bk_last++;
	bookmark->index = mapi_table->bk_last;
	bookmark->bin.cb = reply->bookmark.cb;
	bookmark->bin.lpb = talloc_array((TALLOC_CTX *)bookmark, uint8_t, reply->bookmark.cb);
	memcpy(bookmark->bin.lpb, reply->bookmark.lpb, reply->bookmark.cb);

	DLIST_ADD(mapi_table->bookmark, bookmark);

	*lpbkPosition = mapi_table->bk_last;

	obj_table->private_data = mapi_table;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Release the resources associated with a bookmark

   \param obj_table the table the bookmark is associated to
   \param bkPosition the bookmark to be freed

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table is NULL
   - MAPI_E_INVALID_BOOKMARK: The bookmark specified is invalid or
     beyond the last row requested
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 
   \sa CreateBookmark
*/
_PUBLIC_ enum MAPISTATUS FreeBookmark(mapi_object_t *obj_table, 
				      uint32_t bkPosition)
{
	mapi_object_table_t		*table;
	mapi_object_bookmark_t		*bookmark;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct FreeBookmark_req		request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint8_t 			logon_id = 0;

	/* Sanity check */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	table = (mapi_object_table_t *)obj_table->private_data;
	OPENCHANGE_RETVAL_IF(!table, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(bkPosition > table->bk_last, MAPI_E_INVALID_BOOKMARK, NULL);

	bookmark = table->bookmark;
	OPENCHANGE_RETVAL_IF(!bookmark, MAPI_E_INVALID_BOOKMARK, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "FreeBookmark");

	while (bookmark) {
		if (bookmark->index == bkPosition) {
			if (bookmark->index == table->bk_last) {
				table->bk_last--;
			}
			size = 0;

			/* Fill the FreeBookmark operation */
			request.bookmark.cb = bookmark->bin.cb;
			size += sizeof (uint16_t);
			request.bookmark.lpb = bookmark->bin.lpb;
			size += bookmark->bin.cb;

			/* Fill the MAPI_REQ request */
			mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
			mapi_req->opnum = op_MAPI_FreeBookmark;
			mapi_req->logon_id = logon_id;
			mapi_req->handle_idx = 0;
			mapi_req->u.mapi_FreeBookmark = request;
			size += 5;

			/* Fill the mapi_request structure */
			mapi_request = talloc_zero(mem_ctx, struct mapi_request);
			mapi_request->mapi_len = size + sizeof (uint32_t);
			mapi_request->length = size;
			mapi_request->mapi_req = mapi_req;
			mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
			mapi_request->handles[0] = mapi_object_get_handle(obj_table);

			status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
			OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
			OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
			retval = mapi_response->mapi_repl->error_code;
			OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

			OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

			MAPIFreeBuffer(bookmark->bin.lpb);
			DLIST_REMOVE(table->bookmark, bookmark);

			talloc_free(mapi_response);
			talloc_free(mem_ctx);

			return MAPI_E_SUCCESS;
		}
		bookmark = bookmark->next;
	}
	talloc_free(mem_ctx);
	return MAPI_E_INVALID_BOOKMARK;
}


/**
   \details Order the rows of the table based on a criteria

   \param obj_table the table we are ordering rows on
   \param lpSortCriteria pointer on sort criterias to apply

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table or lpSortCriteria is NULL
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS SortTable(mapi_object_t *obj_table, 
				   struct SSortOrderSet *lpSortCriteria)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SortTable_req	request;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	uint8_t 		logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!lpSortCriteria, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "SortTable");

	/* Fill the SortTable operation */
	size = 0;
	request.SortTableFlags = 0;
	size += sizeof (uint8_t);
	request.lpSortCriteria.cSorts = lpSortCriteria->cSorts;
	size += sizeof (uint16_t);
	request.lpSortCriteria.cCategories = lpSortCriteria->cCategories;
	size += sizeof (uint16_t);
	request.lpSortCriteria.cExpanded = lpSortCriteria->cExpanded;
	size += sizeof (uint16_t);
	request.lpSortCriteria.aSort = lpSortCriteria->aSort;
	size += lpSortCriteria->cSorts * (sizeof (uint32_t) + sizeof (uint8_t));

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SortTable;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SortTable = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

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
   \details Get the size associated to a mapi SRestriction

   \param res pointer on the mapi_SRestriction

   \return mapi_SRestriction size
 */
uint32_t get_mapi_SRestriction_size(struct mapi_SRestriction *res)
{
	uint32_t	size;
	uint32_t	i;

	size = 0;

	size += sizeof (res->rt);

	switch(res->rt) {
	case RES_AND:
		size += sizeof (res->res.resAnd.cRes);
		for (i = 0; i < res->res.resAnd.cRes; i++) {
			size += get_mapi_SRestriction_size((struct mapi_SRestriction *)&(res->res.resAnd.res[i]));
		}
		break;
	case RES_OR:
		size += sizeof (res->res.resOr.cRes);
		for (i = 0; i < res->res.resOr.cRes; i++) {
			size += get_mapi_SRestriction_size((struct mapi_SRestriction *)&(res->res.resOr.res[i]));
		}
		break;
	/* case RES_NOT: */
	/* 	size += get_mapi_SRestriction_size(res->res.resNot.res); */
	/* 	break; */
	case RES_CONTENT:
		size += sizeof (res->res.resContent.fuzzy);
		size += sizeof (res->res.resContent.ulPropTag);
		size += sizeof (res->res.resContent.lpProp.ulPropTag);
		size += get_mapi_property_size(&(res->res.resContent.lpProp));
		break;
	case RES_PROPERTY:
		size += sizeof (res->res.resProperty.relop);
		size += sizeof (res->res.resProperty.ulPropTag);
		size += sizeof (res->res.resProperty.lpProp.ulPropTag);
		size += get_mapi_property_size(&(res->res.resProperty.lpProp));
		break;
	case RES_COMPAREPROPS:
		size += sizeof (uint8_t);
		size += sizeof (res->res.resCompareProps.ulPropTag1);
		size += sizeof (res->res.resCompareProps.ulPropTag2);
		break;
	case RES_BITMASK:
		size += sizeof (uint8_t);
		size += sizeof (res->res.resBitmask.ulPropTag);
		size += sizeof (res->res.resBitmask.ulMask);
		break;
	case RES_SIZE:
		size += sizeof (uint8_t);
		size += sizeof (res->res.resSize.ulPropTag);
		size += sizeof (res->res.resSize.size);
		break;
	case RES_EXIST:
		size += sizeof (res->res.resExist.ulPropTag);
		break;
	}
	return (size);
}

/**
   \details Removes all filters that are currently on a table

   \param obj_table the table object to reset

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table is NULL
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa Restrict
*/
_PUBLIC_ enum MAPISTATUS Reset(mapi_object_t *obj_table)
{
	TALLOC_CTX		*mem_ctx;
	uint32_t		size;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint8_t 		logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "Reset");
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_ResetTable;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

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
   \details Applies a filter to a table, reducing the row set to only
   those rows matching the specified criteria.

   \param obj_table the object we are filtering
   \param res the filters we want to apply
   \param TableStatus the table status result

   TableStatus can either hold:
	- TBLSTAT_COMPLETE (0x0)
	- TBLSTAT_SORTING (0x9)
	- TBLSTAT_SORT_ERROR (0xA)
	- TBLSTAT_SETTING_COLS (0xB)
	- TBLSTAT_SETCOL_ERROR (0xD)
	- TBLSTAT_RESTRICTING (0xE)
	- TBLSTAT_RESTRICT_ERROR (0xF)


   Unlike MAPI, you don't pass a null restriction argument to remove 
   the current restrictions. Use Reset() instead.

   TableStatus should be set to NULL if you don't want to retrieve the
   status of the table.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table is NULL
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa QueryRows, Reset
*/
_PUBLIC_ enum MAPISTATUS Restrict(mapi_object_t *obj_table, 
				  struct mapi_SRestriction *res,
				  uint8_t *TableStatus)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct Restrict_req	request;
	struct Restrict_repl	*reply;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	uint8_t 		logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!res, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "Restrict");

	/* Fill the Restrict operation */
	size = 0;
	request.handle_idx = 0;
	size += sizeof (request.handle_idx);
	request.restrictions = *res;
	size += get_mapi_SRestriction_size(res);

	/* add subcontext size */
	size += sizeof (uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_Restrict;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_Restrict = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	if (TableStatus) {
		reply = &mapi_response->mapi_repl->u.mapi_Restrict;
		*TableStatus = reply->TableStatus;
	}

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Find the next row in a table that matches specific search
   criteria

   \param obj_table the table we are searching in
   \param res pointer on search criterias
   \param bkOrigin bookmark identifying the row where FindRow should
   begin
   \param ulFlags controls the direction of the search
   \param SRowSet the resulting row

   bkOrigin can either take the value of a bookmark created with
   CreateBookmark or any of the default values:
   - BOOKMARK_BEGINNING
   - BOOKMARK_CURRENT
   - BOOKMARK_END

   ulFlags can be set either to DIR_FORWARD (0x0) or DIR_BACKWARD
   (0x1).

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table is NULL
   - MAPI_E_INVALID_BOOKMARK: the bookmark specified is invalid or
     beyond the last row requested.
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa CreateBookmark
 */
_PUBLIC_ enum MAPISTATUS FindRow(mapi_object_t *obj_table, 
				 struct mapi_SRestriction *res,
				 enum BOOKMARK bkOrigin,
				 enum FindRow_ulFlags ulFlags,
				 struct SRowSet *SRowSet)
{
	struct mapi_context	*mapi_ctx;
	struct mapi_request    	*mapi_request;
	struct mapi_response   	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct FindRow_req     	request;
	struct FindRow_repl    	*reply;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_object_table_t	*table;
	struct SBinary_short	bin;
	uint8_t 		logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!res, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = session->mapi_ctx;
	OPENCHANGE_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	if (bkOrigin >= 3) {
		retval = mapi_object_bookmark_find(obj_table, bkOrigin, &bin);
		OPENCHANGE_RETVAL_IF(retval, MAPI_E_INVALID_BOOKMARK, NULL);
	}

	mem_ctx = talloc_named(session, 0, "FindRow");

	/* Fill the FindRow operation */
	size = 0;
	request.ulFlags = ulFlags;
	size += sizeof (uint8_t);
	request.res = *res;
	size += get_mapi_SRestriction_size(res);
	request.origin = (bkOrigin > BOOKMARK_USER) ? BOOKMARK_USER : bkOrigin;
	size += sizeof (uint8_t);
	if (bkOrigin >= 3) {
		request.bookmark.cb = bin.cb;
		request.bookmark.lpb = bin.lpb;
		size += sizeof (uint16_t)+ bin.cb;
	} else {
		request.bookmark.cb = 0;
		request.bookmark.lpb = NULL;
		size += sizeof (uint16_t);
	}

	/* add subcontext size */
	size += sizeof (uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_FindRow;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_FindRow = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* table contains SPropTagArray from previous SetColumns call */
	table = (mapi_object_table_t *)obj_table->private_data;
	OPENCHANGE_RETVAL_IF(!table, MAPI_E_INVALID_OBJECT, mem_ctx);

	reply = &mapi_response->mapi_repl->u.mapi_FindRow;
	SRowSet->cRows = 1;
	SRowSet->aRow = talloc_array((TALLOC_CTX *)table, struct SRow, SRowSet->cRows);
	emsmdb_get_SRowSet((TALLOC_CTX *)table, SRowSet, &table->proptags, &reply->row);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Get the status of a table

   \param obj_table the table we are retrieving the status from
   \param TableStatus the table status result

   TableStatus can either hold:
	- TBLSTAT_COMPLETE (0x0)
	- TBLSTAT_SORTING (0x9)
	- TBLSTAT_SORT_ERROR (0xA)
	- TBLSTAT_SETTING_COLS (0xB)
	- TBLSTAT_SETCOL_ERROR (0xD)
	- TBLSTAT_RESTRICTING (0xE)
	- TBLSTAT_RESTRICT_ERROR (0xF)

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table is NULL
   - MAPI_E_INVALID_BOOKMARK: the bookmark specified is invalid or
     beyond the last row requested.
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa SetColumns, Restrict, FindRow, GetHierarchyTable, GetContentsTable
 */
_PUBLIC_ enum MAPISTATUS GetStatus(mapi_object_t *obj_table, uint8_t *TableStatus)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct GetStatus_repl	*reply;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	uint8_t 		logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	
	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "GetStatus");
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetStatus;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);
	
	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Retrieve TableStatus */
	reply = &mapi_response->mapi_repl->u.mapi_GetStatus;
	*TableStatus = reply->TableStatus;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Aborts an asynchronous table operation in progress

   \param obj_table the table object where we want to abort an
   asynchronous operation
   \param TableStatus pointer on the table status returned by the
   operation

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table or TableStatus are null
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
 */
_PUBLIC_ enum MAPISTATUS Abort(mapi_object_t *obj_table, uint8_t *TableStatus)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct Abort_repl	*reply;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	uint8_t 		logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!TableStatus, MAPI_E_INVALID_PARAMETER, NULL);
	
	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "Abort");
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_Abort;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Retrieve TableStatus */
	reply = &mapi_response->mapi_repl->u.mapi_Abort;
	*TableStatus = reply->TableStatus;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Expand a collapsed row in a table

   After a contents table has been sorted and categorized using
   SortTable, rows can be expanded and collapsed (using ExpandRow and
   CollapseRow repectively).

   \param obj_table the table we are collapsing the category in.
   \param categoryId the row identification for the heading row for
   the category being expanded.
   \param maxRows the maximum number of rows to retrieve (can be zero)
   \param rowData (result) the data rows under this category heading
   \param expandedRowCount (result) the number of rows that were added
   to the table when the row was expanded

   You obtain the categoryId argument from the PR_INST_ID property of
   the heading row for the category that is being collapsed.

   The maxRows argument specifies the upper limit on how many rows to
   return (as rowData) when the category is expanded. The
   expandedRowCount argument returns the number of rows that were
   added to the table. As an example, consider a collapsed category
   with 8 entries. If you set maxRows to 3, then rowData will contain
   the data for the first three rows, and expandedRowCount will be set
   to 8. If you now use QueryRows(), you can read the 5 additional
   rows. If you'd specified maxRows as 8 (or more), rowData would have
   contained all 8 rows and expandedRowCount still would have been 8.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table, rowData or rowCount are NULL
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa CollapseRow
 */
_PUBLIC_ enum MAPISTATUS ExpandRow(mapi_object_t *obj_table, 
				   uint64_t categoryId,
				   uint16_t maxRows, 
				   struct SRowSet *rowData,
				   uint32_t *expandedRowCount)
{
	struct mapi_context		*mapi_ctx;
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct ExpandRow_req		request;
	struct ExpandRow_repl		*reply;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	mapi_object_table_t		*table;
	TALLOC_CTX			*mem_ctx;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!rowData, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!expandedRowCount, MAPI_E_INVALID_PARAMETER, NULL);
	
	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = session->mapi_ctx;
	OPENCHANGE_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "ExpandRow");
	size = 0;

	/* Fill the ExpandRow operation */
	request.MaxRowCount = maxRows;
	size += sizeof (uint16_t);
	request.CategoryId = categoryId;
	size += sizeof (uint64_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_ExpandRow;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_ExpandRow = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* table contains mapitags from previous SetColumns */
	table = (mapi_object_table_t *)obj_table->private_data;
	OPENCHANGE_RETVAL_IF(!table, MAPI_E_INVALID_OBJECT, mem_ctx);

	/* Retrieve the rowData and expandedRowCount */
	reply = &mapi_response->mapi_repl->u.mapi_ExpandRow;
	rowData->cRows = reply->RowCount;
	rowData->aRow = talloc_array((TALLOC_CTX *)table, struct SRow, reply->RowCount);
	emsmdb_get_SRowSet((TALLOC_CTX *)table, rowData, &table->proptags, &reply->RowData);
	*expandedRowCount = reply->ExpandedRowCount;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Collapse an expanded row in a table

   After a contents table has been sorted and categorized using
   SortTable, rows can be expanded and collapsed (using ExpandRow and
   CollapseRow repectively).

   \param obj_table the table we are collapsing the category in.
   \param categoryId the row identification for the heading row for
   the category being collapsed.
   \param rowCount (result) the number of rows that were removed from the 
   table when the row was collapsed.

   You obtain the categoryId argument from the PR_INST_ID property of
   the heading row for the category that is being collapsed.

   If you pass rowCount as null, the number of rows will not be returned.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table is NULL
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa ExpandRow
 */
_PUBLIC_ enum MAPISTATUS CollapseRow(mapi_object_t *obj_table, uint64_t categoryId,
				     uint32_t *rowCount)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct CollapseRow_req		request;
	struct CollapseRow_repl		*reply;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "CollapseRow");
	size = 0;

	/* Fill the CollapseRow operation */
	request.CategoryId = categoryId;
	size += sizeof (uint64_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_CollapseRow;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_CollapseRow = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);
	
	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Retrieve the RowCount */
	reply = &mapi_response->mapi_repl->u.mapi_CollapseRow;
	*rowCount = reply->CollapsedRowCount;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;

}

/**
   \details Get the Collapse State of a Table

   After a contents table has been sorted and categorized using
   SortTable, rows can be expanded and collapsed (using ExpandRow() and
   CollapseRow() repectively). You can save the state of the table using
   this function, and restore it using SetCollapseState.

   \param obj_table the table we are retrieving the state from
   \param rowId the row number for the cursor
   \param rowInstanceNumber the instance number for the cursor
   \param CollapseState (result) the returned table Collapse State

   You obtain the row number and row instance number arguments from
   the PR_INST_ID and  PR_INST_NUM properties of the row you want to
   use as the cursor.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table or CollapseState are null
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa SetCollapseState
 */
_PUBLIC_ enum MAPISTATUS GetCollapseState(mapi_object_t *obj_table, uint64_t rowId,
					  uint32_t rowInstanceNumber,
					  struct SBinary_short *CollapseState)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetCollapseState_req	request;
	struct GetCollapseState_repl	*reply;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;
	mem_ctx = talloc_named(session, 0, "GetCollapseState");
	size = 0;

	/* Fill the GetCollapseState operation */
	size = 0;
	request.RowId = rowId;
	size += sizeof (uint64_t);
	request.RowInstanceNumber = rowInstanceNumber;
	size += sizeof (uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetCollapseState;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetCollapseState = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);
	
	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Retrieve the CollapseState */
	reply = &mapi_response->mapi_repl->u.mapi_GetCollapseState;
	CollapseState->cb = reply->CollapseState.cb;
	CollapseState->lpb = talloc_array((TALLOC_CTX *)session, uint8_t, reply->CollapseState.cb);
	memcpy(CollapseState->lpb, reply->CollapseState.lpb, reply->CollapseState.cb);
	
	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Set the Collapse State of a Table

   After a contents table has been sorted and categorized using
   SortTable, rows can be expanded and collapsed (using ExpandRow() and
   CollapseRow() repectively). You can save the state of the table using
   GetCollapseState, and restore it using this function.

   \param obj_table the table we are restoring the state for
   \param CollapseState the Collapse State to restore

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_table or CollapseState are null
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa GetCollapseState
 */
_PUBLIC_ enum MAPISTATUS SetCollapseState(mapi_object_t *obj_table,
					  struct SBinary_short *CollapseState)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SetCollapseState_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	mapi_object_table_t	       	*mapi_table;
	uint8_t 			logon_id = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!CollapseState, MAPI_E_INVALID_PARAMETER, NULL);
	
	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "SetCollapseState");
	size = 0;

	/* Fill the SetCollapseState operation */
	size = 0;
	request.CollapseState.cb = CollapseState->cb;
	size += sizeof (uint16_t);
	request.CollapseState.lpb = CollapseState->lpb;
	size += CollapseState->cb;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetCollapseState;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SetCollapseState = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);
	
	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	mapi_table = (mapi_object_table_t *)obj_table->private_data;
	OPENCHANGE_RETVAL_IF(!mapi_table, MAPI_E_INVALID_PARAMETER, mem_ctx);

	obj_table->private_data = mapi_table;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
