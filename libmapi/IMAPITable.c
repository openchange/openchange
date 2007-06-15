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
#include <libmapi/proto_private.h>
#include <gen_ndr/ndr_exchange.h>


/**
 * defines the particular properties and order of properties to appear
 * as columns in the table.
 *
 * SetColumns set a *cache* in emsmdb_context. The call will be
 * processed with another request such as QueryRows
 */

_PUBLIC_ enum MAPISTATUS SetColumns(mapi_object_t *obj_table, 
				    struct SPropTagArray *properties)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	TALLOC_CTX		*mem_ctx;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SetColumns_req	request;
	uint32_t		size;
	mapi_object_table_t	*table;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("SetColumns");

	size = 0;

	/* Fill the SetColumns operation */
	request.unknown = 0;
	request.prop_count = properties->cValues;
	request.properties = properties->aulPropTag;
	size += 3 + request.prop_count * sizeof (uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetColumns;
	mapi_req->mapi_flags = 0;
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

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval && (retval != MAPI_W_ERRORS_RETURNED), retval, mem_ctx);

	/* recopy property tags into table */
	/* fixme: obj_table->private_data should be initialized during opening, not here */
	if (obj_table->private_data == 0) {
		obj_table->private_data = talloc((TALLOC_CTX *)obj_table->session, mapi_object_table_t);
	}

	table = (mapi_object_table_t *)obj_table->private_data;
	if (table) {
		table->proptags.cValues = properties->cValues;
		table->proptags.aulPropTag = talloc_array((TALLOC_CTX *)obj_table->private_data,
							  enum MAPITAGS, table->proptags.cValues);
		memcpy((void*)table->proptags.aulPropTag, (void*)properties->aulPropTag,
		       table->proptags.cValues * sizeof(enum MAPITAGS));
	}

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
 *  GetRowCount
 */

_PUBLIC_ enum MAPISTATUS GetRowCount(mapi_object_t *obj_table, 
				     uint32_t *cn_rows)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("GetRowCount");

	*cn_rows = 0;
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetRowCount;
	mapi_req->mapi_flags = 0;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);
	
	*cn_rows = mapi_response->mapi_repl->u.mapi_GetRowCount.count;
	
	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
 * QueryRows returns a RowSet with the properties returned by Exchange
 * Server
 */

_PUBLIC_ enum MAPISTATUS QueryRows(mapi_object_t *obj_table, uint16_t row_count, enum tbl_advance flg_advance, 
				   struct SRowSet *rowSet)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct QueryRows_req	request;
	struct QueryRows_repl	*reply;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_object_table_t	*table;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("QueryRows");

	/* Fill the QueryRows operation */
	request.flag_advance = flg_advance;
	request.layout = 1;
	request.row_count = row_count;
	size += 4;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_QueryRows;
	mapi_req->mapi_flags = 0;
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

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* table contains mapitags from previous SetColumns */
	table = (mapi_object_table_t *)obj_table->private_data;
	MAPI_RETVAL_IF(!table, MAPI_E_INVALID_OBJECT, mem_ctx);

	reply = &mapi_response->mapi_repl->u.mapi_QueryRows;
	rowSet->cRows = reply->results_count;
	rowSet->aRow = talloc_array((TALLOC_CTX *)table, struct SRow, rowSet->cRows);
	emsmdb_get_SRowSet((TALLOC_CTX *)table, rowSet, &table->proptags, &reply->inbox, reply->layout, 1);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
 * QueryColumns
 */

_PUBLIC_ enum MAPISTATUS QueryColumns(mapi_object_t *obj_table, 
				      struct SPropTagArray *cols)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct QueryColumns_repl *reply;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_object_table_t	*table;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("QueryColumns");

	cols->cValues = 0;
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_QueryColumns;
	mapi_req->mapi_flags = 0;
	mapi_req->handle_idx = 0;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* get columns SPropTagArray */
	table = (mapi_object_table_t *)obj_table->private_data;
	MAPI_RETVAL_IF(!table, MAPI_E_INVALID_OBJECT, mem_ctx);

	reply = &mapi_response->mapi_repl->u.mapi_QueryColumns;
	cols->cValues = reply->count;
	cols->aulPropTag = talloc_array((TALLOC_CTX*)table, enum MAPITAGS, cols->cValues);
	memcpy((void*)cols->aulPropTag, (const void*)reply->tags, cols->cValues * sizeof(enum MAPITAGS));

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
 * SeekRow
 */

_PUBLIC_ enum MAPISTATUS SeekRow(mapi_object_t *obj_table, enum BOOKMARK origin, 
				 uint32_t offset, uint32_t *row)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SeekRow_req	request;
	struct SeekRow_repl	*reply;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("SeekRow");

	*row = 0;

	/* Fill the SeekRow operation */
	size = 0;
	request.origin = origin;
	size += 1;
	request.offset = offset;
	size += 4;
	request.unknown_1 = 0;
	size += 1;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SeekRow;
	mapi_req->mapi_flags = 0;
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

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	reply = &mapi_response->mapi_repl->u.mapi_SeekRow;
	*row = reply->row;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
 * Restrict a table 
 */

static uint32_t get_mapi_SRestriction_size(struct mapi_SRestriction *res)
{
	uint32_t	size;

	size = 0;

	switch(res->rt) {
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

_PUBLIC_ enum MAPISTATUS Restrict(mapi_object_t *obj, struct mapi_SRestriction *res)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct Restrict_req	request;
	struct Restrict_repl	*reply;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!res, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("Restrict");

	/* Fill the Restrict operation */
	size = 0;
	request.handle_idx = 0;
	size += sizeof (request.handle_idx);
	request.restrictions = *res;
	size += sizeof (res->rt);
	size += get_mapi_SRestriction_size(res);

	/* add subcontext size */
	size += sizeof (uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_Restrict;
	mapi_req->mapi_flags = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_Restrict = request;
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
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	reply = &mapi_response->mapi_repl->u.mapi_Restrict;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
