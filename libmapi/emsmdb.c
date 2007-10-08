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

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <gen_ndr/ndr_exchange.h>
#include <gen_ndr/ndr_exchange_c.h>

#include <param.h>
#include <credentials.h>

#define	ECDOCONNECT_FORMAT	"/o=%s/ou=%s/cn=Recipients/cn=%s"

/**
 * Create a connection on the exchange_emsmdb pipe
 */

struct emsmdb_context *emsmdb_connect(TALLOC_CTX *mem_ctx, struct dcerpc_pipe *p, struct cli_credentials *cred)
{
	struct EcDoConnect	r;
	NTSTATUS		status;
	struct emsmdb_context	*ret;
	mapi_ctx_t		*mapi_ctx;

	mapi_ctx = global_mapi_ctx;
	if (mapi_ctx == 0) return NULL;

	ret = talloc_zero(mem_ctx, struct emsmdb_context);
	ret->rpc_connection = p;
	ret->mem_ctx = mem_ctx;

	ret->cache_requests = talloc(mem_ctx, struct EcDoRpc_MAPI_REQ *);

	r.in.name = mapi_ctx->session->profile->mailbox;
	r.in.unknown1[0] = 0x0;
	r.in.unknown1[1] = 0x1eeebaac;
	r.in.unknown1[2] = 0x0;
	r.in.code_page = mapi_ctx->session->profile->codepage;
	r.in.input_locale.language = mapi_ctx->session->profile->language;
	r.in.input_locale.method = mapi_ctx->session->profile->method;
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

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		mapi_errstr("EcDoConnect", r.out.result);
		return NULL;
	}

	DEBUG(3, ("emsmdb_connect\n"));
	DEBUG(3, ("\t\t user = %s\n", r.out.user));
	DEBUG(3, ("\t\t organization = %s\n", r.out.org_group));
	
	ret->cred = cred;
	ret->max_data = 0xFFF0;
	ret->setup = false;
	
	return ret;
}

/**
 * Destructor
 */

int emsmdb_disconnect_dtor(void *data)
{
	NTSTATUS		status;
	struct mapi_provider	*provider = (struct mapi_provider *)data;
	struct emsmdb_context	*emsmdb_ctx;

	emsmdb_ctx = (struct emsmdb_context *)provider->ctx;
	status = emsmdb_disconnect(provider->ctx);	
		
	return 0;
}

/**
 * Close the connection on the initialized exchange_emsmdb pipe
 */

NTSTATUS emsmdb_disconnect(struct emsmdb_context *emsmdb)
{
	NTSTATUS		status;
	struct EcDoDisconnect	r;

	r.in.handle = r.out.handle = &emsmdb->handle;

	status = dcerpc_EcDoDisconnect(emsmdb->rpc_connection, emsmdb, &r);
	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		mapi_errstr("EcDoDisconnect", r.out.result);
		return status;
	}

	return NT_STATUS_OK;
}

/**
 * Send a null MAPI packet. 
 * Useful to keep connection up or force notifications
 */

_PUBLIC_ NTSTATUS emsmdb_transaction_null(struct emsmdb_context *emsmdb, struct mapi_response **res)
{
	struct EcDoRpc		r;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	NTSTATUS		status;
	uint16_t		*length;

	mapi_request = talloc_zero(emsmdb->mem_ctx, struct mapi_request);
	mapi_response = talloc_zero(emsmdb->mem_ctx, struct mapi_response);

	r.in.mapi_request = mapi_request;
	r.in.mapi_request->mapi_len = 2;
	r.in.mapi_request->length = 2;

	r.in.handle = r.out.handle = &emsmdb->handle;
	r.in.size = emsmdb->max_data;
	r.in.offset = 0x0;
	r.in.max_data = emsmdb->max_data;
	length = talloc_zero(emsmdb->mem_ctx, uint16_t);
	*length = r.in.mapi_request->mapi_len;
	r.in.length = r.out.length = length;

	r.out.mapi_response = mapi_response;

	status = dcerpc_EcDoRpc(emsmdb->rpc_connection, emsmdb->mem_ctx, &r);

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		return status;
	}

	*res = mapi_response;

	return status;
}

NTSTATUS emsmdb_transaction(struct emsmdb_context *emsmdb, struct mapi_request *req, struct mapi_response **repl)
{
	struct EcDoRpc		r;
	struct mapi_response	*mapi_response;
	uint16_t		*length;
	NTSTATUS		status;
	struct EcDoRpc_MAPI_REQ	*multi_req;
	uint8_t			i = 0;

start:
	r.in.handle = r.out.handle = &emsmdb->handle;
	r.in.size = emsmdb->max_data;
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
	r.in.max_data = (*length >= 0x4000) ? 0x7FFF : emsmdb->max_data;

	status = dcerpc_EcDoRpc(emsmdb->rpc_connection, emsmdb->mem_ctx, &r);

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		if (emsmdb->setup == false) {
			errno = 0;
			emsmdb->max_data = 0x7FFF;
			emsmdb->setup = true;
			goto start;
		} else {
			return status;
		}
	} else {
		emsmdb->setup = true;
	}
	emsmdb->cache_size = emsmdb->cache_count = 0;

	*repl = r.out.mapi_response;

	return status;
}

/**
 * initialize notify context structure and bind a local udp port for
 * notifications
 */
struct mapi_notify_ctx *emsmdb_bind_notification(TALLOC_CTX *mem_ctx)
{
	struct mapi_notify_ctx	*notify_ctx = NULL;
	unsigned short		port = DFLT_NOTIF_PORT;
	const char		*ipaddr = NULL;
	uint32_t		try = 0;

	if (!global_mapi_ctx) return NULL;
	if (!global_mapi_ctx->session) return NULL;
	if (!global_mapi_ctx->session->profile) return NULL;

	notify_ctx = talloc_zero(mem_ctx, struct mapi_notify_ctx);

	notify_ctx->notifications = talloc_zero((TALLOC_CTX *)notify_ctx, struct notifications);
	notify_ctx->notifications->prev = NULL;
	notify_ctx->notifications->next = NULL;

	ipaddr = iface_best_ip(global_mapi_ctx->session->profile->server);
	if (!ipaddr) return NULL;
	notify_ctx->addr = talloc_zero(mem_ctx, struct sockaddr);
	notify_ctx->addr->sa_family = AF_INET;
	((struct sockaddr_in *)(notify_ctx->addr))->sin_addr.s_addr = inet_addr(ipaddr);
retry:
	if (try) port++;
	((struct sockaddr_in *)(notify_ctx->addr))->sin_port = htons(port);

	notify_ctx->fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (notify_ctx->fd == -1) {
		return NULL;
	}

	if (bind(notify_ctx->fd, notify_ctx->addr, sizeof(struct sockaddr)) == -1) {
		shutdown(notify_ctx->fd, SHUT_RDWR);
		close(notify_ctx->fd);
		if (try < 3) {
			try++;
			errno = 0;
			goto retry;
		}
		return NULL;
	}

	return notify_ctx;
}

/**
 * Register for notifications on a Exchange server
 */
NTSTATUS emsmdb_register_notification(struct NOTIFKEY *notifkey, uint32_t ulEventMask)
{
	struct EcRRegisterPushNotification	request;
	NTSTATUS				status;
	struct emsmdb_context			*emsmdb;
	struct mapi_session			*session;
	struct mapi_notify_ctx			*notify_ctx;
	struct policy_handle			handle;

	if (!global_mapi_ctx) return NT_STATUS_UNSUCCESSFUL;
	if (!global_mapi_ctx->session) return NT_STATUS_UNSUCCESSFUL;
	if (!global_mapi_ctx->session->emsmdb) return NT_STATUS_UNSUCCESSFUL;
	if (!global_mapi_ctx->session->emsmdb->ctx) return NT_STATUS_UNSUCCESSFUL;

	session = (struct mapi_session *)global_mapi_ctx->session;
	emsmdb = (struct emsmdb_context *)session->emsmdb->ctx;
	notify_ctx = (struct mapi_notify_ctx *)session->notify_ctx;

	/* in */
	request.in.handle = &emsmdb->handle;
	request.in.ulEventMask = ulEventMask;

	request.in.notifkey = talloc_array(emsmdb->mem_ctx, uint8_t, request.in.notif_len);
	memcpy(request.in.notifkey, notifkey->ab, request.in.notif_len);
	request.in.notif_len = notifkey->cb;

	request.in.unknown2 = 0xffffffff;

	request.in.sockaddr = talloc_array(emsmdb->mem_ctx, uint8_t, sizeof (struct sockaddr));
	/* cp address family and length */
	request.in.sockaddr[0] = (notify_ctx->addr->sa_family & 0xFF);
	request.in.sockaddr[1] = (notify_ctx->addr->sa_family & 0xFF00) >> 8;
	memcpy(&request.in.sockaddr[2], notify_ctx->addr->sa_data, 14);
	request.in.sockaddr_len = sizeof (struct sockaddr);

	/* out */
	request.out.handle = &handle;
	request.out.retval = talloc_zero(emsmdb->mem_ctx, uint32_t);

	status = dcerpc_EcRRegisterPushNotification(emsmdb->rpc_connection, emsmdb->mem_ctx, &request);
	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		return status;
	}

	if (request.out.result != MAPI_E_SUCCESS) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (request.out.retval && *request.out.retval != MAPI_E_SUCCESS) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}


const void *pull_emsmdb_property(TALLOC_CTX *mem_ctx, uint32_t *offset, enum MAPITAGS tag, DATA_BLOB *data)
{
	struct ndr_pull		*ndr;
	const char		*pt_string8;
	uint16_t		*pt_i2;
	uint64_t		*pt_i8;
	uint32_t		*pt_long;
	uint8_t			*pt_boolean;
	struct FILETIME		*pt_filetime;
	struct SBinary_short	pt_binary;
	struct SBinary		*sbin;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	ndr->offset = *offset;
	ndr->data = data->data;
	ndr->data_size = data->length;
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);

	switch(tag & 0xFFFF) {
	case PT_BOOLEAN:
		pt_boolean = talloc_zero(mem_ctx, uint8_t);
		ndr_pull_uint8(ndr, NDR_SCALARS, pt_boolean);
		*offset = ndr->offset;
		return (void *) pt_boolean;
	case PT_I2:
		pt_i2 = talloc_zero(mem_ctx, uint16_t);
		ndr_pull_uint16(ndr, NDR_SCALARS, pt_i2);
		*offset = ndr->offset;
		return (void *) pt_i2;
	case PT_NULL:
	case PT_ERROR:
	case PT_LONG:
		pt_long = talloc_zero(mem_ctx, uint32_t);
		ndr_pull_uint32(ndr, NDR_SCALARS, pt_long);
		*offset = ndr->offset;
		return (void *) pt_long;
	case PT_I8:
		pt_i8 = talloc_zero(mem_ctx, uint64_t);
		ndr_pull_hyper(ndr, NDR_SCALARS, pt_i8);
		*offset = ndr->offset;
		return (void *) pt_i8;
	case PT_SYSTIME:
		pt_filetime = talloc_zero(mem_ctx, struct FILETIME);
		ndr_pull_hyper(ndr, NDR_SCALARS, (uint64_t*)pt_filetime);
		*offset = ndr->offset;
		return (void*) pt_filetime;
	case PT_UNICODE:
	case PT_STRING8:
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_NULLTERM);
		ndr_pull_string(ndr, NDR_SCALARS, &pt_string8);
		*offset = ndr->offset;
		return (const void *) pt_string8;
	case PT_OBJECT:
	case PT_BINARY:
		ndr_pull_SBinary_short(ndr, NDR_SCALARS, &pt_binary);
		*offset = ndr->offset;
		sbin = talloc_zero(mem_ctx, struct SBinary);
		sbin->cb = pt_binary.cb;
		sbin->lpb = pt_binary.lpb;
		return (void *) sbin;
	default:
		return NULL;
	}	
}

enum MAPISTATUS emsmdb_get_SPropValue(TALLOC_CTX *mem_ctx,
				      DATA_BLOB *content,
				      struct SPropTagArray *tags,
				      struct SPropValue **propvals, uint32_t *cn_propvals,
				      uint8_t layout)
{
	struct SPropValue	*p_propval;
	uint32_t		i_propval;
	uint32_t		i_tag;
	uint32_t		cn_tags;
	uint32_t		offset = 0;
	const void		*data;

	i_propval = 0;
	cn_tags = tags->cValues;
	*cn_propvals = 0;
	*propvals = talloc_array(mem_ctx, struct SPropValue, cn_tags);

	for (i_tag = 0; i_tag < cn_tags; i_tag++) {
		if (layout) { 
			if (((uint8_t)(*(content->data + offset))) == PT_ERROR) {
				tags->aulPropTag[i_tag] &= 0xFFFF0000;
				tags->aulPropTag[i_tag] |= PT_ERROR;
			}
			offset += sizeof (uint8_t);
		}

		data = pull_emsmdb_property(mem_ctx, &offset, tags->aulPropTag[i_tag], content);
		if (data) {
			p_propval = &((*propvals)[i_propval]);
			p_propval->ulPropTag = tags->aulPropTag[i_tag];
			p_propval->dwAlignPad = 0x0;
			set_SPropValue(p_propval, data);
			i_propval++;
		}
	}

	*cn_propvals = i_propval;
	return MAPI_E_SUCCESS;
}

void	emsmdb_get_SRowSet(TALLOC_CTX *mem_ctx, struct SRowSet *rowset, struct SPropTagArray *proptags, DATA_BLOB *content, uint8_t layout, uint8_t align)
{
	struct SRow		*rows;
	struct SPropValue	*lpProps;
	uint32_t		idx;
	uint32_t		prop;
	uint32_t		offset = 0;
	const void		*data;
	uint32_t		row_count;

	/* caller allocated */
	rows = rowset->aRow;
	row_count = rowset->cRows;

	for (idx = 0; idx < row_count; idx++) {
		lpProps = talloc_array(mem_ctx, struct SPropValue, proptags->cValues);
		for (prop = 0; prop < proptags->cValues; prop++) {
			if (layout) {
				  if (((uint8_t)(*(content->data + offset))) == PT_ERROR) {
					proptags->aulPropTag[prop] &= 0xFFFF0000;
					proptags->aulPropTag[prop] |= 0xA;
				}
				offset += align;
			}
			data = pull_emsmdb_property(mem_ctx, &offset, proptags->aulPropTag[prop], content);
			lpProps[prop].ulPropTag = proptags->aulPropTag[prop];
			lpProps[prop].dwAlignPad = 0x0;
			set_SPropValue(&lpProps[prop], data);
		}
		if (align) {
			offset += align;
		}

		rows[idx].ulAdrEntryPad = 0;
		rows[idx].cValues = proptags->cValues;
		rows[idx].lpProps = lpProps;
	}
}
