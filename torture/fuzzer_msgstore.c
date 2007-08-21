/* 
   OpenChange MAPI implementation testsuite

   Fuzzer test used to find EcDoRpc max_data packet size

   Copyright (C) Julien Kerihuel 2007
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <gen_ndr/ndr_exchange.h>
#include <gen_ndr/ndr_exchange_c.h>
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture/torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>

BOOL torture_fuzzer_msgstore(struct torture_context *torture)
{
	NTSTATUS		ntstatus;
	TALLOC_CTX		*mem_ctx;
	struct dcerpc_pipe	*p;
	struct mapi_session	*session;
	mapi_ctx_t		*mapi_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct OpenMsgStore_req	request;
	struct EcDoRpc		r;
	struct emsmdb_context	*emsmdb;
	const char		*mailbox;
	uint16_t		*length;
	uint32_t		size;
	int			i;

	/* init torture */
	mem_ctx = talloc_init("torture_fuzzer_msgtore");
	ntstatus = torture_rpc_connection(mem_ctx, &p, &ndr_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(ntstatus)) {
		talloc_free(mem_ctx);
		return False;
	}

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx)) == NULL) return False;

	mapi_ctx = global_mapi_ctx;
	mailbox = mapi_ctx->session->profile->mailbox;

	for (i = 0; i < 0xFFFF; i++) {
		size = 0;

		/* Fill the OpenMsgStore operation */
		request.codepage = 0xc01;
		request.padding = 0;
		request.row = 0x0;
		request.mailbox_path = talloc_strdup(mem_ctx, mailbox);
		size += 9 + strlen(mailbox) + 1;

		/* Fill the MAPI_REQ request */
		mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
		mapi_req->opnum = op_MAPI_OpenMsgStore;
		mapi_req->mapi_flags = 0;
		mapi_req->handle_idx = 0;
		mapi_req->u.mapi_OpenMsgStore = request;
		size += 5;

		/* Fill the mapi_request structure */
		mapi_request = talloc_zero(mem_ctx, struct mapi_request);
		mapi_request->mapi_len = size + sizeof (uint32_t) + 2;
		mapi_request->length = size +2;
		mapi_request->mapi_req = mapi_req;
		mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
		mapi_request->handles[0] = 0xffffffff;

		emsmdb = mapi_ctx->session->emsmdb->ctx;
		r.in.handle = r.out.handle = &emsmdb->handle;
		r.in.size = i;
		r.in.offset = 0x0;

		mapi_response = talloc_zero(emsmdb->mem_ctx, struct mapi_response);
		r.out.mapi_response = mapi_response;
		
		mapi_request->mapi_req = talloc_realloc(emsmdb->mem_ctx, mapi_request->mapi_req, struct EcDoRpc_MAPI_REQ, 2);
		mapi_request->mapi_req[1].opnum = 0;

		r.in.mapi_request = mapi_request;
		length = talloc_zero(emsmdb->mem_ctx, uint16_t);
		*length = r.in.mapi_request->mapi_len;
		r.in.length = r.out.length = length;
		r.in.max_data = i;

		ntstatus = dcerpc_EcDoRpc(emsmdb->rpc_connection, emsmdb->mem_ctx, &r);
		
		if (MAPI_STATUS_IS_OK(NT_STATUS_V(ntstatus))) {
			DEBUG(0, ("SUCCESS: max_data = 0x%x\n", i));
		} else {
			DEBUG(0, ("FAILURE: max_data = 0x%x\n", i));
		}
	}

	return True;
}
