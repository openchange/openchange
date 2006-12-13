/*
 *  OpenChange MAPI implementation.
 *
 *  Copyright (C) Julien Kerihuel 2005.
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
#include "ndr_exchange_c.h"
#include "libmapi/include/emsmdb.h"
#include "libmapi/tdr_IMAPISession.h"
#include "libmapi/mapicode.h"
#include "libmapi/include/proto.h"

#define	ECDOCONNECT_FORMAT	"/o=%s Organization/ou=First Administrative Group/cn=Recipients/cn=%s"

uint8_t	*previous = NULL;
uint32_t plength = 0;

static void debug_print_infos(const char *info, struct MAPI_DATA blob)
{
	uint32_t	length;
	static uint32_t	idx = 0;

	if (info && !strcmp(info, "request")) {
		length = blob.length + 2;
	} else {
		length = blob.length;
	}

  	printf("\n\n[%s][%d]:\t0x%.04X\n", info, idx, blob.opnum);
	printf("\t\tmapi_len = %d\n", blob.mapi_len);
	printf("\t\tlength = %d\n", blob.length);
	printf("\t\tcontent:\n");
	dump_data(1, blob.content, blob.length);
	printf("\t\tremaining:\n");
	dump_data(1, blob.remaining, blob.mapi_len - length);
	idx++;
}

/* !!! BROKEN !!! Code:  Investigation and fixes needed */

struct emsmdb_context *emsmdb_connect(TALLOC_CTX *mem_ctx, struct dcerpc_pipe *p, struct cli_credentials *cred)
{
	struct EcDoConnect	r;
	NTSTATUS		status;
	const char		*organization;
	const char		*username;
	uint32_t		codepage = lp_parm_int(-1, "locale", "codepage", 0);
	uint32_t		language = lp_parm_int(-1, "locale", "language", 0);
	uint32_t		method = lp_parm_int(-1, "locale", "method", 0);
	char			*session;
	uint32_t		length;
	struct emsmdb_context	*ret;

	organization = cli_credentials_get_domain(cred);
	username = cli_credentials_get_username(cred);

	if (!organization || !username) {
		DEBUG(1,("emsmdb_connect: username and domain required"));
		return NULL;
	}

	if (!codepage || !language || !method) {
		DEBUG(3, ("emsmdb_connect: Invalid parameter\n"));
		return NULL;
	}

	ret = talloc(mem_ctx, struct emsmdb_context);
	ret->rpc_connection = p;
	ret->mem_ctx = mem_ctx;
	
	session = talloc_asprintf(mem_ctx, ECDOCONNECT_FORMAT, organization, username);
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

	return ret;
}

NTSTATUS emsmdb_transaction(struct emsmdb_context *emsmdb, struct MAPI_DATA blob, struct MAPI_REQ *req)
{
	struct EcDoRpc	r;
	NTSTATUS	status;
	struct tdr_push	*push;
	uint16_t	length = blob.mapi_len;

	push = talloc_zero(emsmdb->mem_ctx, struct tdr_push);

	r.in.handle = r.out.handle = &emsmdb->handle;
	r.in.max_data = 0x7fff;
	r.in.length = &length;
	r.out.length = &length;

	r.in.data.max_data = 0x7fff;
	r.in.data.offset = 0x0;

	r.in.data.blob.mapi_len = blob.mapi_len;
	r.in.data.blob.opnum = req->opcode;

	TDR_CHECK(tdr_push_MAPI_REQ(push, req));

	r.in.data.blob.length = push->data.length + 1;
	r.in.data.blob.content = talloc_zero_array(emsmdb->mem_ctx, uint8_t, push->data.length - 1);
	memcpy(r.in.data.blob.content, push->data.data + 2, push->data.length - 2);

	r.in.data.blob.remaining = fill_remaining_blob(r.in.data.blob, previous, plength);

	debug_print_infos("request", r.in.data.blob);

	status = dcerpc_EcDoRpc(emsmdb->rpc_connection, emsmdb->mem_ctx, &r);

	debug_print_infos("response", r.out.data.blob);

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		mapi_errstr("EcDoRpc", r.out.result);
	}
	else {
		previous = r.out.data.blob.remaining;
		plength = r.out.data.blob.mapi_len - r.out.data.blob.length;
	}

	return status;
}

NTSTATUS emsmdb_transaction_unknown(struct emsmdb_context *emsmdb, struct MAPI_DATA blob)
{
	struct EcDoRpc	r;
	NTSTATUS	status;
	uint16_t	length = blob.mapi_len;

	r.in.handle = r.out.handle = &emsmdb->handle;
	r.in.max_data = 0x7fff;
	r.in.length = &length;
	r.out.length = &length;

	r.in.data.max_data = 0x7fff;
	r.in.data.offset = 0x0;

	r.in.data.blob.mapi_len = blob.mapi_len;
	r.in.data.blob.length = blob.length;
	r.in.data.blob.opnum = blob.opnum;
	r.in.data.blob.content = blob.content;
	r.in.data.blob.remaining = fill_remaining_blob(r.in.data.blob, previous, plength);

	printf("---------------------------------------\n");
	dump_data(0, r.in.data.blob.content, r.in.data.blob.length);
	printf("---------------------------------------\n");

	debug_print_infos("request", r.in.data.blob);

	status = dcerpc_EcDoRpc(emsmdb->rpc_connection, emsmdb->mem_ctx, &r);

	debug_print_infos("response", r.out.data.blob);

        if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		mapi_errstr("EcDoRpc", r.out.result);
        } else {
		previous = r.out.data.blob.remaining;
		plength = r.out.data.blob.mapi_len - r.out.data.blob.length;
	}

	return status;
}

BOOL emsmdb_registernotify(struct emsmdb_context *emsmdb, DATA_BLOB blob1, DATA_BLOB blob2) 
{
	struct EcRRegisterPushNotification	r;
	BOOL					ret = True;
	NTSTATUS				status;

	r.out.handle = r.in.handle = &emsmdb->handle;

	r.in.unknown = 0x0;
	r.in.blob1 = blob1;
	r.in.unknown1 = blob1.length;
	r.in.unknown2 = 0xffffffff;
	r.in.blob2 = blob2;
	r.in.unknown3 = blob2.length;

	r.out.unknown4 = talloc(emsmdb, uint32_t);

        status = dcerpc_EcRRegisterPushNotification(emsmdb->rpc_connection, emsmdb, &r);

	mapi_errstr("EcRRegisterPushNotification", r.out.result);

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		ret = False;
	} else {
		ret = True;
	}

	return ret;
}
