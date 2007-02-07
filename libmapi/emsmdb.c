/*
 *  OpenChange MAPI implementation.
 *
 *  Copyright (C) Julien Kerihuel 2005 - 2007.
 *  Copyright (C) Jelmer Vernooij 2005.
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
#include "ndr_exchange_c.h"
#include "libmapi/include/emsmdb.h"
#include "libmapi/mapicode.h"
#include "libmapi/include/mapidefs.h"
#include "libmapi/include/mapi_proto.h"
#include "libmapi/include/proto.h"

#define	ECDOCONNECT_FORMAT	"/o=%s/ou=%s/cn=Recipients/cn=%s"

uint8_t	*previous = NULL;
uint32_t plength = 0;

struct emsmdb_context *emsmdb_connect(TALLOC_CTX *mem_ctx, struct dcerpc_pipe *p, struct cli_credentials *cred)
{
	struct EcDoConnect	r;
	NTSTATUS		status;
	const char		*organization = lp_parm_string(-1, "exchange", "organization");
	const char		*username;
	uint32_t		codepage = lp_parm_int(-1, "locale", "codepage", 0);
	uint32_t		language = lp_parm_int(-1, "locale", "language", 0);
	uint32_t		method = lp_parm_int(-1, "locale", "method", 0);
	const char		*group = lp_parm_string(-1, "exchange", "ou");
	char			*session;
	uint32_t		length;
	struct emsmdb_context	*ret;

	if (!organization) {
		organization = cli_credentials_get_domain(cred);
	}
	username = cli_credentials_get_username(cred);

	if (!organization || !username) {
		DEBUG(1,("emsmdb_connect: username and domain required"));
		return NULL;
	}

	if (!codepage || !language || !method) {
		DEBUG(3, ("emsmdb_connect: Invalid parameter\n"));
		return NULL;
	}

	if (!group) {
		DEBUG(3, ("emsmdb_connect: exchange organization group required\n"));
		return NULL;
	}

	ret = talloc_zero(mem_ctx, struct emsmdb_context);
	ret->rpc_connection = p;
	ret->mem_ctx = mem_ctx;

	ret->cache_requests = talloc(mem_ctx, struct EcDoRpc_MAPI_REQ *);
	
	session = talloc_asprintf(mem_ctx, ECDOCONNECT_FORMAT, organization, group, username);
	length = strlen(session);

	r.in.name = session;
	r.in.unknown1[0] = 0x0;
	r.in.unknown1[1] = 0x1eeebaac;
	r.in.unknown1[2] = 0x0;
	r.in.code_page = codepage;
	r.in.input_locale.language = language;
	r.in.input_locale.method = method;
	r.in.unknown2 = 0xffffffff;
	r.in.unknown3 = 0x1;
	r.in.emsmdb_client_version[0] = 0x000a;
	r.in.emsmdb_client_version[1] = 0x0000;
	r.in.emsmdb_client_version[2] = 0x1013;

	r.in.alloc_space = talloc(mem_ctx, uint32_t);
	*r.in.alloc_space = 0;

	r.out.unknown4[0] = (uint32_t)talloc(mem_ctx, uint32_t);
	r.out.unknown4[1] = (uint32_t)talloc(mem_ctx, uint32_t);
	r.out.unknown4[2] = (uint32_t)talloc(mem_ctx, uint32_t);
	r.out.session_nb = talloc(mem_ctx, uint16_t);
	r.out.alloc_space = talloc(mem_ctx, uint32_t);
	r.out.handle = &ret->handle;

	status = dcerpc_EcDoConnect(p, mem_ctx, &r);

	talloc_free(session);

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		mapi_errstr("EcDoConnect", r.out.result);
		return NULL;
	}

	DEBUG(3, ("emsmdb_connect\n"));
	DEBUG(3, ("\t\t user = %s\n", r.out.user));
	DEBUG(3, ("\t\t organization = %s\n", r.out.org_group));
	mapi_errstr("EcDoConnect", r.out.result);
	
	ret->cred = cred;
	
	return ret;
}

NTSTATUS emsmdb_transaction(struct emsmdb_context *emsmdb, struct mapi_request *req, struct mapi_response **repl)
{
	struct EcDoRpc		r;
	struct mapi_response	*mapi_response;
	uint16_t		*length;
	NTSTATUS		status;
	struct EcDoRpc_MAPI_REQ	*multi_req;
	uint8_t			i = 0;
		
	
	r.in.handle = r.out.handle = &emsmdb->handle;
	r.in.size = 0x200;
	r.in.offset = 0x0;

	mapi_response = talloc_zero(emsmdb->mem_ctx, struct mapi_response);	
	r.out.mapi_response = mapi_response;

	/* process cached data */
	if (emsmdb->cache_count) {
		multi_req = talloc_array(emsmdb->mem_ctx, struct EcDoRpc_MAPI_REQ, emsmdb->cache_count + 2);
		for (i = 0; i < emsmdb->cache_count; i++) {
			multi_req[i] = *emsmdb->cache_requests[i];
		}
		multi_req[i] = req->mapi_req[0];
		req->mapi_req = multi_req;
	} 

	req->mapi_req = talloc_realloc(emsmdb->mem_ctx, req->mapi_req, struct EcDoRpc_MAPI_REQ, emsmdb->cache_count + 2);
	req->mapi_req[i+1].opnum = 0;

	r.in.mapi_request = req;
	r.in.mapi_request->mapi_len += emsmdb->cache_size;
	r.in.mapi_request->length += emsmdb->cache_size;
	length = talloc_zero(emsmdb->mem_ctx, uint16_t);
	*length = r.in.mapi_request->mapi_len;
	r.in.length = r.out.length = length;
	r.in.max_data = 0x200;

	status = dcerpc_EcDoRpc(emsmdb->rpc_connection, emsmdb->mem_ctx, &r);

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		mapi_errstr("EcDoRpc", r.out.result);
	}
	emsmdb->cache_size = emsmdb->cache_count = 0;

	*repl = r.out.mapi_response;

	return status;
}

void *pull_emsmdb_property(struct emsmdb_context *emsmdb, uint32_t *offset, uint32_t property, DATA_BLOB data)
{
	struct ndr_pull		*ndr;
	const char		*pt_string8;
	uint64_t		*pt_i8;
	uint32_t		*pt_long;
	struct SBinary_short	pt_binary;
	struct SBinary		*sbin;

	ndr = talloc_zero(emsmdb->mem_ctx, struct ndr_pull);
	ndr->offset = *offset;
	ndr->data = data.data;
	ndr->data_size = data.length;
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);

	switch(property & 0xFFFF) {
	case PT_LONG:
		pt_long = talloc_zero(emsmdb->mem_ctx, uint32_t);
		ndr_pull_uint32(ndr, NDR_SCALARS, pt_long);
		*offset = ndr->offset;
		return (void *) pt_long;
	case PT_I8:
		pt_i8 = talloc_zero(emsmdb->mem_ctx, uint64_t);
		ndr_pull_hyper(ndr, NDR_SCALARS, pt_i8);
		*offset = ndr->offset;
		return (void *) pt_i8;
	case PT_STRING8:
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_NULLTERM);
		ndr_pull_string(ndr, NDR_SCALARS, &pt_string8);
		*offset = ndr->offset;
		return (void *) pt_string8;
	case PT_BINARY:
		ndr_pull_SBinary_short(ndr, NDR_SCALARS, &pt_binary);
		*offset = ndr->offset;
		sbin = talloc_zero(emsmdb->mem_ctx, struct SBinary);
		sbin->cb = pt_binary.cb;
		sbin->lpb = pt_binary.lpb;
		return (void *) sbin;
	default:
		return NULL;
	}	
}

struct SRow *emsmdb_get_SRow(struct emsmdb_context *emsmdb, uint32_t row_count, DATA_BLOB content, uint8_t align)
{
	struct SRow	*SRow;
	struct SPropValue *lpProps;
	uint32_t	idx;
	uint32_t	prop;
	uint32_t	offset = 0;
	void		*data;

	SRow = talloc_array(emsmdb->mem_ctx, struct SRow, row_count);

	for (idx = 0; idx < row_count; idx++) {
		lpProps = talloc_size(emsmdb->mem_ctx, sizeof(struct SPropValue) * emsmdb->prop_count);

		for (prop = 0; prop < emsmdb->prop_count; prop++) {
			data = pull_emsmdb_property(emsmdb, &offset, emsmdb->properties[prop], content);
			lpProps[prop].ulPropTag = emsmdb->properties[prop];
			lpProps[prop].dwAlignPad = 0x0;
			set_SPropValue(&lpProps[prop], data);
		}
		if (align) {
			offset += align;
		}

		SRow[idx].ulAdrEntryPad = 0;
		SRow[idx].cValues = emsmdb->prop_count;
		SRow[idx].lpProps = lpProps;
	}
	
	return SRow;
}

/* BOOL emsmdb_registernotify(struct emsmdb_context *emsmdb, DATA_BLOB blob1, DATA_BLOB blob2)  */
/* { */
/* 	struct EcRRegisterPushNotification	r; */
/* 	BOOL					ret = True; */
/* 	NTSTATUS				status; */

/* 	r.out.handle = r.in.handle = &emsmdb->handle; */

/* 	r.in.unknown = 0x0; */
/* 	r.in.blob1 = blob1; */
/* 	r.in.unknown1 = blob1.length; */
/* 	r.in.unknown2 = 0xffffffff; */
/* 	r.in.blob2 = blob2; */
/* 	r.in.unknown3 = blob2.length; */

/* 	r.out.unknown4 = talloc(emsmdb, uint32_t); */

/*         status = dcerpc_EcRRegisterPushNotification(emsmdb->rpc_connection, emsmdb, &r); */

/* 	mapi_errstr("EcRRegisterPushNotification", r.out.result); */

/* 	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) { */
/* 		ret = False; */
/* 	} else { */
/* 		ret = True; */
/* 	} */

/* 	return ret; */
/* } */
