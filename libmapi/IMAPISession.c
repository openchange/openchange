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
 * Open the Message Store
 */

enum MAPISTATUS	OpenMsgStore(struct emsmdb_context *emsmdb, uint32_t ulFlags, 
			     uint32_t *handle_id, uint64_t *id_outbox, const char *mailbox)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct OpenMsgStore_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;


	mem_ctx = talloc_init("OpenMsgStore");

	*id_outbox = 0;

	/* Fill the OpenMsgStore operation */
	request.col = 0x0;
 	request.codepage = 0xc01; /*ok values: 0xc01 0xc09 */
	request.padding = 0;
	request.row = 0x0;
	request.mailbox_path = talloc_strdup(mem_ctx, mailbox);
	size = 10 + strlen(mailbox) + 1;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenMsgStore;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_OpenMsgStore = request;
	size += 6;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = 0xffffffff;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	*handle_id = mapi_response->handles[0];

	/* get outbox folder id */
	*id_outbox = mapi_response->mapi_repl->u.mapi_OpenMsgStore.folder_id[3];

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
