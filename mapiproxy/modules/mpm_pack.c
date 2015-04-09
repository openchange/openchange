/*
   MAPI Proxy - Unpack/Pack Module

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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

/**
   \file mpm_pack.c

   \brief Pack/Unpack specified MAPI calls into/from a custom MAPI call
 */

#include "libmapi/libmapi.h"
#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"

#define	MPM_NAME	"mpm_pack"
#define	MPM_PACK_ERROR	"[ERROR] mpm_pack:"

NTSTATUS samba_init_module(void);

static struct mpm_pack {
	uint8_t		*mapi_calls;
	bool		lasthop;
} *mpm = NULL;


static uint32_t proxypack(TALLOC_CTX *mem_ctx, struct EcDoRpc_MAPI_REQ *mapi_req, 
			  struct ndr_push *ndr)
{
	struct proxypack_req	request;
	uint32_t		size;

	/* Fill in the proxypack operation */
	size = 0;	
	request.bin.cb = ndr->offset;
	size += sizeof (uint16_t);
	request.bin.lpb = talloc_memdup(mem_ctx, ndr->data, ndr->offset);
	size += ndr->offset;

	/* Fill the MAPI_REQ request */
	mapi_req->opnum = op_MAPI_proxypack;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_proxypack = request;
	size += 5;

	return size;
}

/**
   \details unpack proxypack contents and restore the original EcDoRpc
   request
 */
static bool unpack(TALLOC_CTX *mem_ctx, struct EcDoRpc *EcDoRpc)
{
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct EcDoRpc_MAPI_REQ	*mapi_newreq;
	struct ndr_pull		*ndr;
	bool			found;
	uint32_t		i;
	uint8_t			pos;
	uint32_t		count;
	uint32_t		nopack_count = 0;
	uint32_t		nopack_idx = 0;
	uint32_t		pack_idx = 0;

	mapi_req = EcDoRpc->in.mapi_request->mapi_req;

	/* Seek the unpack call */
	for (i = 0, found = false; mapi_req[i].opnum; i++) {
		if (mapi_req[i].opnum == op_MAPI_proxypack) {
			found = true;
			break;
		}
	}
	/* Nothing to unpack */
	if (found == false) return false;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);

	ndr->data_size = mapi_req[i].u.mapi_proxypack.bin.cb;
	ndr->data = mapi_req[i].u.mapi_proxypack.bin.lpb;

	for (nopack_count = 0; mapi_req[nopack_count].opnum; nopack_count++);

	/* Merge unpacked and non packed calls < last packed call position */
	mapi_newreq = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	for (count = 0; ndr->offset != ndr->data_size; count++) {
		NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &pos));
		if (pack_idx < pos) {
			while (pack_idx < pos) {
				if (nopack_idx >= nopack_count) break;
				
				mapi_newreq = talloc_realloc(mem_ctx, mapi_newreq, struct EcDoRpc_MAPI_REQ, pack_idx + 2);
				mapi_newreq[pack_idx] = mapi_req[nopack_idx];
				nopack_idx++;
				pack_idx++;
			}
		}
		
		if (pos > pack_idx) {
			pack_idx = pos;
		}
		
		mapi_newreq = talloc_realloc(mem_ctx, mapi_newreq, struct EcDoRpc_MAPI_REQ, pack_idx + 2);
		NDR_CHECK(ndr_pull_EcDoRpc_MAPI_REQ(ndr, NDR_SCALARS, &mapi_newreq[pos]));
		pack_idx++;
	}

	/* Append remaining non packed calls */
	mapi_newreq[pack_idx].opnum = 0;
	while (mapi_req[nopack_idx].opnum) {
		if (nopack_idx > nopack_count) break;
		
		if (mapi_req[nopack_idx].opnum != op_MAPI_proxypack) {
			mapi_newreq = talloc_realloc(mem_ctx, mapi_newreq, struct EcDoRpc_MAPI_REQ, pack_idx + 2);
			mapi_newreq[pack_idx] = mapi_req[nopack_idx];
			pack_idx++;
			mapi_newreq[pack_idx].opnum = 0;
		}
		nopack_idx++;
	}

	/* Update mapi_request length and mapi_req pointer */
	EcDoRpc->in.mapi_request->mapi_len -= (5 + count);
	EcDoRpc->in.mapi_request->length -= (5 + count);
	EcDoRpc->in.mapi_request->mapi_req = mapi_newreq;

	return true;
}

/**
   \details pack EcDoRpc calls into proxypack
 */
static bool pack(TALLOC_CTX *mem_ctx, struct EcDoRpc *EcDoRpc)
{
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct EcDoRpc_MAPI_REQ	*mapi_newreq;
	struct ndr_push		*ndr;
	struct ndr_push		*nopack_ndr;
	uint32_t		size;
	uint32_t		handle_size;
	uint32_t		i, j;
	uint32_t		idx;
	bool			found;

	mapi_req = EcDoRpc->in.mapi_request->mapi_req;

	ndr = talloc_zero(mem_ctx, struct ndr_push);

	nopack_ndr = talloc_zero(mem_ctx, struct ndr_push);

	mapi_newreq = talloc_array(mem_ctx, struct EcDoRpc_MAPI_REQ, 2);

	for (i = 0, idx = 0; mapi_req[i].opnum; i++) {
		found = false;
		for (j = 0; mpm->mapi_calls[j]; j++) {
			if (mapi_req[i].opnum == mpm->mapi_calls[j]) {
				ndr_push_uint8(ndr, NDR_SCALARS, i);
				ndr_push_EcDoRpc_MAPI_REQ(ndr, NDR_SCALARS, &mapi_req[i]);
				found = true;
				break;
			}
		}
		if (found == false) {
			mapi_newreq = talloc_realloc(mem_ctx, mapi_newreq, struct EcDoRpc_MAPI_REQ, idx + 2);
			ndr_push_EcDoRpc_MAPI_REQ(nopack_ndr, NDR_SCALARS, &mapi_req[i]);
			mapi_newreq[idx] = mapi_req[i];
			idx++;
		}
	}

	if (ndr->offset == 0) {
		talloc_free(mapi_newreq);
		talloc_free(nopack_ndr);
		talloc_free(ndr);
		return false;
	}

	OC_DEBUG(3, "============ non packed =============");
	dump_data(3, nopack_ndr->data, nopack_ndr->offset);
	OC_DEBUG(3, "=====================================");

	OC_DEBUG(3, "");
	OC_DEBUG(3, "============ packed =============");
	dump_data(3, ndr->data, ndr->offset);
	OC_DEBUG(3, "=================================");


	/* Fill in the proxypack operation */
	mapi_newreq = talloc_realloc(mem_ctx, mapi_newreq, struct EcDoRpc_MAPI_REQ, idx + 2);
	size = proxypack(mem_ctx, &mapi_newreq[idx], ndr);
	talloc_free(ndr);

	if (!size) return false;

	idx++;
	mapi_newreq[idx].opnum = 0;

	/* Recalculate the EcDoRpc request size */
	handle_size = EcDoRpc->in.mapi_request->mapi_len - EcDoRpc->in.mapi_request->length;
	EcDoRpc->in.mapi_request->mapi_len = nopack_ndr->offset + size + handle_size;
	EcDoRpc->in.mapi_request->length = nopack_ndr->offset + size;

	/* Replace EcDoRpc_MAPI_REQ */
	talloc_free(EcDoRpc->in.mapi_request->mapi_req);
	EcDoRpc->in.mapi_request->mapi_req = mapi_newreq;

	/* Free memory */
	talloc_free(nopack_ndr);

	return true;
}

static NTSTATUS pack_push(struct dcesrv_call_state *dce_call,
			  TALLOC_CTX *mem_ctx, void *r)
{
	return NT_STATUS_OK;
}


static NTSTATUS pack_ndr_pull(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, struct ndr_pull *pull)
{
	return NT_STATUS_OK;
}


/**
   \details pack EcDoRpc MAPI requests

   This function searches for MAPI opnums to pack in the requests,
   add this opnums to the mapiproxy opnum DATA blob and refactor the
   request to remove references to these calls in the original
   request.
 */
static NTSTATUS pack_pull(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, void *r)
{
	struct EcDoRpc		*EcDoRpc;
	bool			ret = false;

	if (dce_call->pkt.u.request.opnum != 0x2) {
		return NT_STATUS_OK;
	}

	EcDoRpc = (struct EcDoRpc *) r;
	if (!EcDoRpc->in.mapi_request->mapi_req) return NT_STATUS_OK;

	/* If this is an idle request, do not go further */
	if (EcDoRpc->in.mapi_request->length == 2) {
		return NT_STATUS_OK;
	}

	/* If this is not a last-hop */
	if (mpm->lasthop == false) return NT_STATUS_OK;

	ret = unpack(mem_ctx, EcDoRpc);
	if (ret == false) {
		ret = pack(mem_ctx, EcDoRpc);
	}

	return NT_STATUS_OK;
}


/**
   \details Initialize the pack module and retrieve configuration from
   smb.conf.

   Possible parameters:
   * mpm_pack:opnums = 0x1, 0x2, 0x3
   * mpm_pack:lasthop = true|false
   
 */
static NTSTATUS pack_init(struct dcesrv_context *dce_ctx)
{
	char			**calls;
	unsigned long		opnum;
	int			i;
	int			j;
	struct loadparm_context	*lp_ctx;

	/* Fetch the mapi call list from smb.conf */
	calls = str_list_make(dce_ctx, lpcfg_parm_string(dce_ctx->lp_ctx, NULL, MPM_NAME, "opnums"), NULL);

	mpm = talloc_zero(dce_ctx, struct mpm_pack);
	mpm->mapi_calls = talloc_zero(mpm, uint8_t);

	for (i = 0; calls[i]; i++) {
		opnum = strtol(calls[i], NULL, 16);
		if (opnum <= 0 || opnum >= 0xFF) {
			OC_DEBUG(0, "%s: invalid MAPI opnum 0x%.2x", MPM_PACK_ERROR, (uint32_t)opnum);
			talloc_free(mpm);
			return NT_STATUS_INVALID_PARAMETER;
		}
		/* avoid duplicated opnums */
		for (j = 0; j < i; j++) {
			if (opnum == mpm->mapi_calls[j]) {
				OC_DEBUG(0, "%s: duplicated opnum: 0x%.2x", MPM_PACK_ERROR, (uint32_t)opnum);
				talloc_free(mpm);
				return NT_STATUS_INVALID_PARAMETER;
			}
		}
		mpm->mapi_calls = talloc_realloc(mpm, mpm->mapi_calls, uint8_t, i + 2);
		mpm->mapi_calls[i] = (uint8_t) opnum;
	}
	mpm->mapi_calls[i] = 0;

	/* Fetch the lasthop parameter from smb.conf */
	mpm->lasthop = lpcfg_parm_bool(dce_ctx->lp_ctx, NULL, MPM_NAME, "lasthop", true);

	lp_ctx = loadparm_init(dce_ctx);
	lpcfg_load_default(lp_ctx);
	dcerpc_init();

	return NT_STATUS_OK;
}


/**
   \details Entry point for the pack mapiproxy module

   \return NT_STATUS_OK on success, otherwise NTSTATUS error
 */
NTSTATUS samba_init_module(void)
{
	struct mapiproxy_module	module;
	NTSTATUS		ret;

	/* Fill in our name */
	module.name = "pack";
	module.description = "Pack specified MAPI calls into a custom MAPI call";
	module.endpoint = "exchange_emsmdb";

	/* Fill in all the operations */
	module.init = pack_init;
	module.unbind = NULL;
	module.push = pack_push;
	module.ndr_pull = pack_ndr_pull;
	module.pull = pack_pull;
	module.dispatch = NULL;

	/* Register ourselves with the MAPIPROXY subsystem */
	ret = mapiproxy_module_register(&module);
	if (!NT_STATUS_IS_OK(ret)) {
		OC_DEBUG(0, "Failed to register the 'pack' mapiproxy module!");
		return ret;
	}

	return ret;
}
