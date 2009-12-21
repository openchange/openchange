/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2005 - 2008.
   Copyright (C) Jelmer Vernooij 2005.
 
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

#include <unistd.h>
#include <fcntl.h>
#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <gen_ndr/ndr_exchange.h>
#include <gen_ndr/ndr_exchange_c.h>
#include <gen_ndr/ndr_misc.h>

#include <param.h>

/**
   \file emsmdb.c

   \brief EMSMDB stack functions
 */


/**
   \details Hash a string and returns a unsigned integer hash value

   \param str the string to hash

   \return a hash value greater than 0 on success, otherwise 0

   \note This function is based on the hash algorithm from gdbm and
   from Samba4 TDB code.
 */
static unsigned int emsmdb_hash(const char *str)
{
	uint32_t	value;	/* Used to compute the hash value.  */
	uint32_t	i;	/* Used to cycle through random values. */
	uint32_t	len;

	/* Sanity check */
	if (!str) return 0;

	len = strlen(str);

	/* Set the initial value from the key size. */
	for (value = 0x238F13AF * len, i = 0; i < len; i++)
		value = (value + (str[i] << (i * 5 % 24)));

	return (1103515243 * value + 12345);  
}


/**
   \details Establishes a new Session Context with the server on the
   exchange_emsmdb pipe

   \param parent_mem_ctx pointer to the memory context
   \param session pointer to the MAPI session context
   \param p pointer to the DCERPC pipe
   \param cred pointer to the user credentials
   \param return_value pointer on EcDoConnect MAPI return value

   \return an allocated emsmdb_context on success, otherwise NULL
 */
struct emsmdb_context *emsmdb_connect(TALLOC_CTX *parent_mem_ctx, 
				      struct mapi_session *session,
				      struct dcerpc_pipe *p, 
				      struct cli_credentials *cred,
				      int *return_value)
{
	TALLOC_CTX		*mem_ctx;
	struct EcDoConnect	r;
	struct emsmdb_context	*ret;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		pullTimeStamp = 0;

	/* Sanity Checks */
	if (!session) return NULL;
	if (!p) return NULL;
	if (!cred) return NULL;
	if (!return_value) return NULL;

	mem_ctx = talloc_named(NULL, 0, "emsmdb_connect");

	ret = talloc_zero(parent_mem_ctx, struct emsmdb_context);
	ret->rpc_connection = p;
	ret->mem_ctx = parent_mem_ctx;

	ret->cache_requests = talloc(parent_mem_ctx, struct EcDoRpc_MAPI_REQ *);
	ret->info.szDisplayName = NULL;
	ret->info.szDNPrefix = NULL;

	r.in.szUserDN = session->profile->mailbox;
	r.in.ulFlags = 0x00000000;
	r.in.ulConMod = emsmdb_hash(r.in.szUserDN);
	r.in.cbLimit = 0x00000000;
	r.in.ulCpid = session->profile->codepage;
	r.in.ulLcidString = session->profile->language;
	r.in.ulLcidSort = session->profile->method;
	r.in.ulIcxrLink = 0xFFFFFFFF;
	r.in.usFCanConvertCodePages = 0x1;
	r.in.rgwClientVersion[0] = 0x000c;
	r.in.rgwClientVersion[1] = 0x183e;
	r.in.rgwClientVersion[2] = 0x03e8;
	r.in.pullTimeStamp = &pullTimeStamp;

	r.out.handle = &ret->handle;
	r.out.pcmsPollsMax = &ret->info.pcmsPollsMax;
	r.out.pcRetry = &ret->info.pcRetry;
	r.out.pcmsRetryDelay = &ret->info.pcmsRetryDelay;
	r.out.picxr = &ret->info.picxr;
	r.out.pullTimeStamp = &pullTimeStamp;

	status = dcerpc_EcDoConnect(p, mem_ctx, &r);
	retval = r.out.result;
	if (!NT_STATUS_IS_OK(status) || retval) {
		*return_value = retval;
		mapi_errstr("EcDoConnect", retval);
		talloc_free(mem_ctx);
		return NULL;
	}

	ret->info.szDisplayName = talloc_strdup(parent_mem_ctx, r.out.szDisplayName);
	ret->info.szDNPrefix = talloc_strdup(parent_mem_ctx, r.out.szDNPrefix);

	ret->info.rgwServerVersion[0] = r.out.rgwServerVersion[0];
	ret->info.rgwServerVersion[1] = r.out.rgwServerVersion[1];
	ret->info.rgwServerVersion[2] = r.out.rgwServerVersion[2];

	ret->cred = cred;
	ret->max_data = 0xFFF0;
	ret->setup = false;

	talloc_free(mem_ctx);

	return ret;
}


/**
   \details Destructor for the EMSMDB context. Call the EcDoDisconnect
   function.

   \param data generic pointer to data with mapi_provider information

   \return MAPI_E_SUCCESS on success, otherwise -1
 */
int emsmdb_disconnect_dtor(void *data)
{
	struct mapi_provider	*provider = (struct mapi_provider *)data;
	struct emsmdb_context	*emsmdb_ctx;

	emsmdb_ctx = (struct emsmdb_context *)provider->ctx;
	emsmdb_disconnect(provider->ctx);	

	talloc_free(emsmdb_ctx->cache_requests);

	if (emsmdb_ctx->info.szDisplayName) {
		talloc_free(emsmdb_ctx->info.szDisplayName);
	}

	if (emsmdb_ctx->info.szDNPrefix) {
		talloc_free(emsmdb_ctx->info.szDNPrefix);
	}

	return 0;
}


/**
   \details Destroy the EMSMDB context handle

   \param emsmdb_ctx pointer to the EMSMDB context

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
enum MAPISTATUS emsmdb_disconnect(struct emsmdb_context *emsmdb_ctx)
{
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	struct EcDoDisconnect	r;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!emsmdb_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	r.in.handle = r.out.handle = &emsmdb_ctx->handle;

	status = dcerpc_EcDoDisconnect(emsmdb_ctx->rpc_connection, emsmdb_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Send an empty MAPI packet - useful to keep connection up
   or force notifications.

   \param emsmdb_ctx pointer to the EMSMDB connection context
   \param res pointer on pointer to a MAPI response structure

   \return NT_STATUS_OK on success, otherwise NT status error
 */
_PUBLIC_ NTSTATUS emsmdb_transaction_null(struct emsmdb_context *emsmdb_ctx, 
					  struct mapi_response **res)
{
	struct EcDoRpc		r;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	NTSTATUS		status;
	uint16_t		*length;

	/* Sanity checks */
	if(!emsmdb_ctx) return NT_STATUS_INVALID_PARAMETER;
	if (!res) return NT_STATUS_INVALID_PARAMETER;

	mapi_request = talloc_zero(emsmdb_ctx->mem_ctx, struct mapi_request);
	mapi_response = talloc_zero(emsmdb_ctx->mem_ctx, struct mapi_response);

	r.in.mapi_request = mapi_request;
	r.in.mapi_request->mapi_len = 2;
	r.in.mapi_request->length = 2;

	r.in.handle = r.out.handle = &emsmdb_ctx->handle;
	r.in.size = emsmdb_ctx->max_data;
	r.in.offset = 0x0;
	r.in.max_data = emsmdb_ctx->max_data;
	length = talloc_zero(emsmdb_ctx->mem_ctx, uint16_t);
	*length = r.in.mapi_request->mapi_len;
	r.in.length = r.out.length = length;

	r.out.mapi_response = mapi_response;

	status = dcerpc_EcDoRpc(emsmdb_ctx->rpc_connection, emsmdb_ctx->mem_ctx, &r);
	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		return status;
	}

	*res = mapi_response;

	return status;
}


static int mapi_response_destructor(void *data)
{
	struct mapi_response	*mapi_response = (struct mapi_response *)data;

	if (mapi_response->mapi_repl) {
		if (mapi_response->handles) {
			talloc_free(mapi_response->handles);
		}

		if (!mapi_response->mapi_repl->error_code) {
			talloc_free(mapi_response->mapi_repl);
		}
	}

	return 0;
}


/**
   \details Make a EMSMDB transaction.

   \param emsmdb_ctx pointer to the EMSMDB connection context
   \param mem_ctx pointer to the memory context
   \param req pointer to the MAPI request to send
   \param repl pointer on pointer to the MAPI reply returned by the
   server

   \return NT_STATUS_OK on success, otherwise NT status error
 */
_PUBLIC_ NTSTATUS emsmdb_transaction(struct emsmdb_context *emsmdb_ctx, 
				     TALLOC_CTX *mem_ctx,
				     struct mapi_request *req, 
				     struct mapi_response **repl)
{
	struct EcDoRpc		r;
	struct mapi_response	*mapi_response;
	uint16_t		*length;
	NTSTATUS		status;
	struct EcDoRpc_MAPI_REQ	*multi_req;
	uint8_t			i = 0;

start:
	r.in.handle = r.out.handle = &emsmdb_ctx->handle;
	r.in.size = emsmdb_ctx->max_data;
	r.in.offset = 0x0;

	mapi_response = talloc_zero(emsmdb_ctx->mem_ctx, struct mapi_response);
	mapi_response->mapi_repl = NULL;
	mapi_response->handles = NULL;
	talloc_set_destructor((void *)mapi_response, (int (*)(void *))mapi_response_destructor);
	r.out.mapi_response = mapi_response;

	/* process cached data */
	if (emsmdb_ctx->cache_count) {
		multi_req = talloc_array(mem_ctx, struct EcDoRpc_MAPI_REQ, emsmdb_ctx->cache_count + 2);
		for (i = 0; i < emsmdb_ctx->cache_count; i++) {
			multi_req[i] = *emsmdb_ctx->cache_requests[i];
		}
		multi_req[i] = req->mapi_req[0];
		req->mapi_req = multi_req;
	}

	req->mapi_req = talloc_realloc(mem_ctx, req->mapi_req, struct EcDoRpc_MAPI_REQ, emsmdb_ctx->cache_count + 2);
	req->mapi_req[i+1].opnum = 0;

	r.in.mapi_request = req;
	r.in.mapi_request->mapi_len += emsmdb_ctx->cache_size;
	r.in.mapi_request->length += emsmdb_ctx->cache_size;
	length = talloc_zero(mem_ctx, uint16_t);
	*length = r.in.mapi_request->mapi_len;
	r.in.length = r.out.length = length;
	r.in.max_data = (*length >= 0x4000) ? 0x7FFF : emsmdb_ctx->max_data;

	status = dcerpc_EcDoRpc(emsmdb_ctx->rpc_connection, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		if (emsmdb_ctx->setup == false) {
			errno = 0;
			emsmdb_ctx->max_data = 0x7FFF;
			emsmdb_ctx->setup = true;
			talloc_free(mapi_response);
			goto start;
		} else {
			talloc_free(mapi_response);
			return status;
		}
	} else {
		emsmdb_ctx->setup = true;
	}
	emsmdb_ctx->cache_size = emsmdb_ctx->cache_count = 0;

	if (r.out.mapi_response->mapi_repl && r.out.mapi_response->mapi_repl->error_code) {
		r.out.mapi_response->handles = NULL;
	}

	*repl = r.out.mapi_response;

	return status;
}


/**
   \details Initialize the notify context structure and bind a local
   UDP port to receive notifications from the server

   \param mem_ctx pointer to the memory context

   \return an allocated mapi_notify_ctx structure on success,
   otherwise NULL
 */
struct mapi_notify_ctx *emsmdb_bind_notification(TALLOC_CTX *mem_ctx)
{
	struct interface	*ifaces;
	struct mapi_notify_ctx	*notify_ctx = NULL;
	unsigned short		port = DFLT_NOTIF_PORT;
	const char		*ipaddr = NULL;
	uint32_t		try = 0;

	/* Sanity Checks */
	if (!global_mapi_ctx) return NULL;
	if (!global_mapi_ctx->session) return NULL;
	if (!global_mapi_ctx->session->profile) return NULL;

	notify_ctx = talloc_zero(mem_ctx, struct mapi_notify_ctx);

	notify_ctx->notifications = talloc_zero((TALLOC_CTX *)notify_ctx, struct notifications);
	notify_ctx->notifications->prev = NULL;
	notify_ctx->notifications->next = NULL;

	load_interfaces(mem_ctx, lp_interfaces(global_mapi_ctx->lp_ctx), &ifaces);
	ipaddr = iface_best_ip(ifaces, global_mapi_ctx->session->profile->server);
	if (!ipaddr) {
		talloc_free(notify_ctx->notifications);
		talloc_free(notify_ctx);
		return NULL;
	}
	notify_ctx->addr = talloc_zero(mem_ctx, struct sockaddr);
	notify_ctx->addr->sa_family = AF_INET;
	((struct sockaddr_in *)(notify_ctx->addr))->sin_addr.s_addr = inet_addr(ipaddr);
retry:
	if (try) port++;
	((struct sockaddr_in *)(notify_ctx->addr))->sin_port = htons(port);

	notify_ctx->fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (notify_ctx->fd == -1) {
		talloc_free(notify_ctx->notifications);
		talloc_free(notify_ctx->addr);
		talloc_free(notify_ctx);
		return NULL;
	}

	fcntl(notify_ctx->fd, F_SETFL, O_NONBLOCK);

	if (bind(notify_ctx->fd, notify_ctx->addr, sizeof(struct sockaddr)) == -1) {
		shutdown(notify_ctx->fd, SHUT_RDWR);
		close(notify_ctx->fd);
		if (try < 3) {
			try++;
			errno = 0;
			goto retry;
		}

		talloc_free(notify_ctx->notifications);
		talloc_free(notify_ctx->addr);
		talloc_free(notify_ctx);
		return NULL;
	}

	return notify_ctx;
}


/**
   \details Register for notifications on the server
   
   \param notifkey The opaque client-generated context data
   \param ulEventMask Notification flags. Exchange completely ignores
   this value and it should be set to 0

   \return NTSTATUS_OK on success, otherwise NT status error
 */
NTSTATUS emsmdb_register_notification(struct NOTIFKEY *notifkey, 
				      uint16_t ulEventMask)
{
	struct EcRRegisterPushNotification	request;
	NTSTATUS				status;
	enum MAPISTATUS				retval;
	TALLOC_CTX				*mem_ctx;
	struct emsmdb_context			*emsmdb_ctx;
	struct mapi_session			*session;
	struct mapi_notify_ctx			*notify_ctx;
	struct policy_handle			handle;
	uint32_t				hNotification = 0;

	/* Sanity Checks*/
	if (!global_mapi_ctx) return NT_STATUS_INVALID_PARAMETER;
	if (!global_mapi_ctx->session) return NT_STATUS_INVALID_PARAMETER;
	if (!global_mapi_ctx->session->emsmdb) return NT_STATUS_INVALID_PARAMETER;
	if (!global_mapi_ctx->session->emsmdb->ctx) return NT_STATUS_INVALID_PARAMETER;
	if (!notifkey) return NT_STATUS_INVALID_PARAMETER;

	session = (struct mapi_session *)global_mapi_ctx->session;
	emsmdb_ctx = (struct emsmdb_context *)session->emsmdb->ctx;
	notify_ctx = (struct mapi_notify_ctx *)session->notify_ctx;
	mem_ctx = talloc_named(NULL, 0, "emsmdb_register_notification");

	request.in.handle = &emsmdb_ctx->handle;
	request.in.ulEventMask = ulEventMask;
	request.in.cbContext = notifkey->cb;
	request.in.rgbContext = talloc_array(mem_ctx, uint8_t, request.in.cbContext);
	memcpy(request.in.rgbContext, notifkey->ab, request.in.cbContext);
	request.in.grbitAdviseBits = 0xffffffff;
	request.in.rgCallbackAddress = talloc_array(mem_ctx, uint8_t, sizeof (struct sockaddr));
	/* cp address family and length */
	request.in.rgCallbackAddress[0] = (notify_ctx->addr->sa_family & 0xFF);
	request.in.rgCallbackAddress[1] = (notify_ctx->addr->sa_family & 0xFF00) >> 8;
	memcpy(&request.in.rgCallbackAddress[2], notify_ctx->addr->sa_data, 14);
	request.in.cbCallbackAddress = sizeof (struct sockaddr);

	request.out.handle = &handle;
	request.out.hNotification = &hNotification;

	status = dcerpc_EcRRegisterPushNotification(emsmdb_ctx->rpc_connection, emsmdb_ctx->mem_ctx, &request);
	retval = request.out.result;
	if (!NT_STATUS_IS_OK(status) || retval) {
		talloc_free(mem_ctx);
		return status;
	}

	talloc_free(mem_ctx);

	return status;
}


/**
   \details Retrieves the EMSMDB context server information structure

   \param session pointer to the MAPI session context

   \return the server info structure on success, otherwise NULL
 */
_PUBLIC_ struct emsmdb_info *emsmdb_get_info(struct mapi_session *session)
{
	if (!global_mapi_ctx || !session->emsmdb->ctx) {
		return NULL;
	}

	return &((struct emsmdb_context *)session->emsmdb->ctx)->info;
}


/**
   \details Free property values retrieved with pull_emsmdb_property

   \param lpProp pointer to SPropValue structure
   \param data generic pointer to associated lpProp data

 */
void free_emsmdb_property(struct SPropValue *lpProp, void *data)
{
	if (!data) return;
	if (!lpProp) return;

	switch (lpProp->ulPropTag & 0xFFFF) {
	case PT_I2:
		talloc_free((uint16_t *)data);
		break;
	case PT_ERROR:
	case PT_LONG:
		talloc_free((uint32_t *)data);
		break;
	case PT_I8:
		talloc_free((uint64_t *)data);
		break;
	case PT_BOOLEAN:
		talloc_free((uint8_t *)data);
		break;
	default:
		break;
		
	}
}


/**
   \details Retrieves a property value from a DATA blob

   \param mem_ctx pointer to the memory context
   \param lp_ctx pointer to the loadparm context
   \param offset pointer on pointer to the current offset
   \param tag the property tag which value is to be retrieved
   \param data pointer to the data

   \return pointer on constant generic data on success, otherwise NULL
 */
const void *pull_emsmdb_property(TALLOC_CTX *mem_ctx,
				 struct loadparm_context *lp_ctx,
				 uint32_t *offset, 
				 enum MAPITAGS tag, 
				 DATA_BLOB *data)
{
	struct ndr_pull			*ndr;
	const char			*pt_string8;
	const char			*pt_unicode;
	uint16_t			*pt_i2;
	uint64_t			*pt_i8;
	uint32_t			*pt_long;
	uint8_t				*pt_boolean;
	struct FILETIME			*pt_filetime;
	struct GUID			*pt_clsid;
	struct SBinary_short		pt_binary;
	struct Binary_r			*sbin;
	struct mapi_SLPSTRArray		pt_slpstr;
	struct StringArray_r		*slpstr;
	struct mapi_MV_LONG_STRUCT	pt_MVl;
	struct LongArray_r		*MVl;
	struct mapi_SBinaryArray	pt_MVbin;
	struct BinaryArray_r		*MVbin;
	uint32_t			i;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	ndr->offset = *offset;
	ndr->data = data->data;
	ndr->data_size = data->length;
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	ndr->iconv_convenience = lp_iconv_convenience(lp_ctx);

	switch(tag & 0xFFFF) {
	case PT_I2:
		pt_i2 = talloc_zero(mem_ctx, uint16_t);
		ndr_pull_uint16(ndr, NDR_SCALARS, pt_i2);
		*offset = ndr->offset;
		talloc_free(ndr);
		return (const void *) pt_i2;
	case PT_ERROR:
	case PT_LONG:
		pt_long = talloc_zero(mem_ctx, uint32_t);
		ndr_pull_uint32(ndr, NDR_SCALARS, pt_long);
		*offset = ndr->offset;
		talloc_free(ndr);
		return (const void *) pt_long;
	case PT_BOOLEAN:
		pt_boolean = talloc_zero(mem_ctx, uint8_t);
		ndr_pull_uint8(ndr, NDR_SCALARS, pt_boolean);
		*offset = ndr->offset;
		talloc_free(ndr);
		return (const void *) pt_boolean;
	case PT_I8:
		pt_i8 = talloc_zero(mem_ctx, uint64_t);
		ndr_pull_hyper(ndr, NDR_SCALARS, pt_i8);
		*offset = ndr->offset;
		talloc_free(ndr);
		return (const void *) pt_i8;
	case PT_UNICODE:
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
		ndr_pull_string(ndr, NDR_SCALARS, &pt_unicode);
		*offset = ndr->offset;
		talloc_free(ndr);
		return (const void *) pt_unicode;
	case PT_STRING8:
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_NULLTERM);
		ndr_pull_string(ndr, NDR_SCALARS, &pt_string8);
		*offset = ndr->offset;
		talloc_free(ndr);
		return (const void *) pt_string8;
	case PT_SYSTIME:
		pt_filetime = talloc_zero(mem_ctx, struct FILETIME);
		ndr_pull_hyper(ndr, NDR_SCALARS, (uint64_t *) pt_filetime);
		*offset = ndr->offset;
		talloc_free(ndr);
		return (const void *) pt_filetime;
	case PT_CLSID:
		pt_clsid = talloc_zero(mem_ctx, struct GUID);
		ndr_pull_GUID(ndr, NDR_SCALARS, pt_clsid);
		*offset = ndr->offset;
		talloc_free(ndr);
		return (const void *) pt_clsid;
	case 0xFB:
	case PT_BINARY:
		ndr_pull_SBinary_short(ndr, NDR_SCALARS, &pt_binary);
		*offset = ndr->offset;
		sbin = talloc_zero(mem_ctx, struct Binary_r);
		sbin->cb = pt_binary.cb;
		sbin->lpb = talloc_memdup(sbin, pt_binary.lpb, pt_binary.cb);
		talloc_free(ndr);
		return (const void *) sbin;
	case PT_MV_LONG:
		ndr_pull_mapi_MV_LONG_STRUCT(ndr, NDR_SCALARS, &pt_MVl);
		*offset = ndr->offset;
		MVl = talloc_zero(mem_ctx, struct LongArray_r);
		MVl->cValues = pt_MVl.cValues;
		MVl->lpl = talloc_array(mem_ctx, uint32_t, pt_MVl.cValues);
		for (i = 0; i < MVl->cValues; i++) {
			MVl->lpl[i] = pt_MVl.lpl[i];
		}
		talloc_free(ndr);
		return (const void *) MVl;
	case PT_MV_STRING8:
		ndr_pull_mapi_SLPSTRArray(ndr, NDR_SCALARS, &pt_slpstr);
		*offset = ndr->offset;
		slpstr = talloc_zero(mem_ctx, struct StringArray_r);
		slpstr->cValues = pt_slpstr.cValues;
		slpstr->lppszA = talloc_array(mem_ctx, const char *, pt_slpstr.cValues);
		for (i = 0; i < slpstr->cValues; i++) {
			slpstr->lppszA[i] = talloc_strdup(mem_ctx, pt_slpstr.strings[i].lppszA);
		}
		talloc_free(ndr);
		return (const void *) slpstr;
	case PT_MV_BINARY:
		ndr_pull_mapi_SBinaryArray(ndr, NDR_SCALARS, &pt_MVbin);
		*offset = ndr->offset;
		MVbin = talloc_zero(mem_ctx, struct BinaryArray_r);
		MVbin->cValues = pt_MVbin.cValues;
		MVbin->lpbin = talloc_array(mem_ctx, struct Binary_r, pt_MVbin.cValues);
		for (i = 0; i < MVbin->cValues; i++) {
			MVbin->lpbin[i].cb = pt_MVbin.bin[i].cb;
			MVbin->lpbin[i].lpb = talloc_size(mem_ctx, MVbin->lpbin[i].cb);
			memcpy(MVbin->lpbin[i].lpb, pt_MVbin.bin[i].lpb, MVbin->lpbin[i].cb);
		}
		talloc_free(ndr);
		return (const void *) MVbin;
	default:
		return NULL;
	}	
}


/**
   \details Get a SPropValue array from a DATA blob

   \param mem_ctx pointer to the memory context
   \param lp_ctx pointer to the loadparm context
   \param content pointer to the DATA blob content
   \param tags pointer to a list of property tags to lookup
   \param propvals pointer on pointer to the returned SPropValues
   \param cn_propvals pointer to the number of propvals
   \param flag describes the type data

   \return MAPI_E_SUCCESS on success
 */
enum MAPISTATUS emsmdb_get_SPropValue(TALLOC_CTX *mem_ctx,
				      struct loadparm_context *lp_ctx,
				      DATA_BLOB *content,
				      struct SPropTagArray *tags,
				      struct SPropValue **propvals, 
				      uint32_t *cn_propvals,
				      uint8_t flag)
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
	*propvals = talloc_array(mem_ctx, struct SPropValue, cn_tags + 1);

	for (i_tag = 0; i_tag < cn_tags; i_tag++) {
		if (flag) { 
			if (((uint8_t)(*(content->data + offset))) == PT_ERROR) {
				tags->aulPropTag[i_tag] &= 0xFFFF0000;
				tags->aulPropTag[i_tag] |= PT_ERROR;
			}
			offset += sizeof (uint8_t);
		}

		data = pull_emsmdb_property(mem_ctx, lp_ctx, &offset, tags->aulPropTag[i_tag], content);
		if (data) {
			data = talloc_steal(*propvals, data);
			p_propval = &((*propvals)[i_propval]);
			p_propval->ulPropTag = tags->aulPropTag[i_tag];
			p_propval->dwAlignPad = 0x0;

			set_SPropValue(p_propval, data);
			free_emsmdb_property(p_propval, (void *) data);
			i_propval++;
		}
	}

	(*propvals)[i_propval].ulPropTag = 0x0;
	*cn_propvals = i_propval;
	return MAPI_E_SUCCESS;
}


/**
   \details Get a SRowSet from a DATA blob

   \param mem_ctx pointer on the memory context
   \param lp_ctx pointer on the loadparm context
   \param rowset pointer on the returned SRowSe
   \param proptags pointer on a list of property tags to lookup
   \param content pointer on the DATA blob content

   \return MAPI_E_SUCCESS on success

   \note TODO: this doesn't yet handle the TypedPropertyValue and
   FlaggedPropertyValueWithTypeSpecified variants
 */
_PUBLIC_ void emsmdb_get_SRowSet(TALLOC_CTX *mem_ctx,
				 struct loadparm_context *lp_ctx,
				 struct SRowSet *rowset, 
				 struct SPropTagArray *proptags, 
				 DATA_BLOB *content)
{
	struct SRow		*rows;
	struct SPropValue	*lpProps;
	uint32_t		idx;
	uint32_t		prop;
	uint32_t		offset = 0;
	const void		*data;
	uint32_t		row_count;
	bool			is_FlaggedPropertyRow = false;
	bool			havePropertyValue;
	uint8_t			flag;

	/* caller allocated */
	rows = rowset->aRow;
	row_count = rowset->cRows;

	for (idx = 0; idx < row_count; idx++) {
		if (0x1 == *(content->data + offset)) {
			is_FlaggedPropertyRow = true;
		} else {
			is_FlaggedPropertyRow = false;
		}
		++offset;

		lpProps = talloc_array(mem_ctx, struct SPropValue, proptags->cValues);
		for (prop = 0; prop < proptags->cValues; prop++) {
			havePropertyValue = true;
			lpProps[prop].ulPropTag = proptags->aulPropTag[prop];
			if (is_FlaggedPropertyRow) {
				flag = (uint8_t)(*(content->data + offset));
				++offset; /* advance offset for the flag */
				switch (flag) {
				case 0x0:
					/* Property Value is valid */
					break;
				case 0x1:
					/* Property Value is not present */
					havePropertyValue = false;
					break;
				case PT_ERROR:
					lpProps[prop].ulPropTag = proptags->aulPropTag[prop];
					lpProps[prop].ulPropTag &= 0xFFFF0000;
					lpProps[prop].ulPropTag |= PT_ERROR;
					break;
				default:
					/* unknown FlaggedPropertyValue flag */
					break;

				}
			}
			if (havePropertyValue) {
				lpProps[prop].dwAlignPad = 0x0;
				data = pull_emsmdb_property(mem_ctx, lp_ctx, &offset, lpProps[prop].ulPropTag, content);
				talloc_steal(lpProps, data);
				set_SPropValue(&lpProps[prop], data);
				free_emsmdb_property(&lpProps[prop], (void *) data);
			}
		}

		rows[idx].ulAdrEntryPad = 0;
		rows[idx].cValues = proptags->cValues;
		rows[idx].lpProps = lpProps;
	}
}


/**
   \details Get a SRow from a DATA blob

   \param mem_ctx pointer on the memory context
   \param lp_ctx pointer on the loadparm context
   \param aRow pointer on the returned SRow
   \param proptags pointer on a list of property tags to lookup
   \param propcount number of SPropValue entries in aRow
   \param content pointer on the DATA blob content
   \param flag the type data
   \param align alignment pad

   \return MAPI_E_SUCCESS on success

   \note TODO: We shouldn't have any alignment pad here
 */
void emsmdb_get_SRow(TALLOC_CTX *mem_ctx,
		     struct loadparm_context *lp_ctx,
		     struct SRow *aRow, 
		     struct SPropTagArray *proptags, 
		     uint16_t propcount, 
		     DATA_BLOB *content, 
		     uint8_t flag, 
		     uint8_t align)
{
	uint32_t		i;
	uint32_t		offset = 0;
	uint32_t		aulPropTag = 0;
	const void		*data;

	aRow->cValues = propcount;
	aRow->lpProps = talloc_array(mem_ctx, struct SPropValue, propcount);

	for (i = 0; i < propcount; i++) {
		aulPropTag = proptags->aulPropTag[i];
		if (flag) {
			if (((uint8_t)(*(content->data + offset))) == PT_ERROR) {
				aulPropTag &= 0xFFFF0000;
				aulPropTag |= 0xA;			
			}
			offset += align;
		} 

		data = pull_emsmdb_property(mem_ctx, lp_ctx, &offset, aulPropTag, content);
		talloc_steal(aRow->lpProps, data);
		aRow->lpProps[i].ulPropTag = aulPropTag;
		aRow->lpProps[i].dwAlignPad = 0x0;
		set_SPropValue(&(aRow->lpProps[i]), data);
		free_emsmdb_property(&aRow->lpProps[i], (void *) data);
	}
	if (align) {
		offset += align;
	}
}
