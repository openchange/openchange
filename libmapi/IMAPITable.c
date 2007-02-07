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


#include "openchange.h"
#include "exchange.h"
#include "ndr_exchange.h"
#include "libmapi/include/mapidefs.h"
#include "libmapi/include/nspi.h"
#include "libmapi/include/emsmdb.h"
#include "libmapi/mapicode.h"
#include "libmapi/include/proto.h"
#include "libmapi/include/mapi_proto.h"

/**
 * defines the particular properties and order of properties to appear
 * as columns in the table.
 *
 * SetColumns set a *cache* in emsmdb_context. The call will be
 * processed with another request such as QueryRows
 */

MAPISTATUS	SetColumns(struct emsmdb_context *emsmdb, uint32_t ulFlags, struct SPropTagArray *properties)
{
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SetColumns_req	request;

	/* Fill the SetColumns operation */
	request.handle = 0;
	request.unknown = 0;
	request.prop_count = properties->cValues - 1;
	request.properties = properties->aulPropTag;
	emsmdb->cache_size = 4 + request.prop_count * sizeof (uint32_t);

	/* Store the property tag array internally for further
	   decoding purpose 
	*/
	emsmdb->prop_count = properties->cValues - 1;
	emsmdb->properties = properties->aulPropTag;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(emsmdb->mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetColumns;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_SetColumns = request;
	emsmdb->cache_size += 2;
	
	emsmdb->cache_requests[0] = mapi_req;
	emsmdb->cache_count += 1;

	return MAPI_E_SUCCESS;
	
}


/**
 * QueryRows returns a RowSet with the properties returned by Exchange
 * Server
 */

MAPISTATUS	QueryRows(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t folder_id, uint16_t row_count, struct SRowSet **rowSet)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct QueryRows_req	request;
	struct QueryRows_repl	*reply;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;

	mem_ctx = talloc_init("QueryRows");

	/* Fill the QueryRows operation */
	request.unknown = 0;
	request.flag_noadvance = 0;
	request.unknown1 = 1;
	request.row_count = row_count;
	size += 5;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_QueryRows;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_QueryRows = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = folder_id;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	/* Fill in the SRowSet array */
	reply = &(mapi_response->mapi_repl[1].u.mapi_QueryRows);

	if (emsmdb->prop_count) {
		rowSet[0]->cRows = reply->results_count;		
		rowSet[0]->aRow = emsmdb_get_SRow(emsmdb, rowSet[0]->cRows, reply->inbox, 1);
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
