/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2005 - 2011.
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
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include "gen_ndr/ndr_exchange.h"
#include "gen_ndr/ndr_exchange_c.h"
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
	if (!session->profile->mailbox) return NULL;

	mem_ctx = talloc_named(parent_mem_ctx, 0, "emsmdb_connect");

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

	r.out.szDNPrefix = (const char **)&ret->info.szDNPrefix;
	r.out.szDisplayName = (const char **)&ret->info.szDisplayName;
	r.out.handle = &ret->handle;
	r.out.pcmsPollsMax = &ret->info.pcmsPollsMax;
	r.out.pcRetry = &ret->info.pcRetry;
	r.out.pcmsRetryDelay = &ret->info.pcmsRetryDelay;
	r.out.picxr = &ret->info.picxr;
	r.out.pullTimeStamp = &pullTimeStamp;

	status = dcerpc_EcDoConnect_r(p->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	if (!NT_STATUS_IS_OK(status) || retval) {
		*return_value = retval;
		mapi_errstr("EcDoConnect", retval);
		talloc_free(mem_ctx);
		return NULL;
	}

	ret->info.szDNPrefix = talloc_steal(parent_mem_ctx, ret->info.szDNPrefix);
	ret->info.szDisplayName = talloc_steal(parent_mem_ctx, ret->info.szDisplayName);

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
   \details Establishes a new Session Context with the server on the
   exchange_emsmdb pipe using 0xA EcDoConnectEx opnum

   \param mem_ctx pointer to the memory context
   \param session pointer to the MAPI session context
   \param p pointer to the DCERPC pipe
   \param cred pointer to the user credentials
   \param return_value pointer on EcDoConnectEx MAPI return value

   \return an allocated emsmdb_context structure on success, otherwise
   NULL
 */
struct emsmdb_context *emsmdb_connect_ex(TALLOC_CTX *mem_ctx,
					 struct mapi_session *session,
					 struct dcerpc_pipe *p,
					 struct cli_credentials *cred,
					 int *return_value)
{
	TALLOC_CTX		*tmp_ctx;
	struct EcDoConnectEx	r;
	struct emsmdb_context	*ctx;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		pulTimeStamp = 0;
	uint32_t		pcbAuxOut = 0x00001008;
	struct mapi2k7_AuxInfo	*rgbAuxOut;

	/* Sanity Checks */
	if (!session) return NULL;
	if (!p) return NULL;
	if (!cred) return NULL;
	if (!return_value) return NULL;

	tmp_ctx = talloc_named(mem_ctx, 0, "emsmdb_connect_ex");

	ctx = talloc_zero(mem_ctx, struct emsmdb_context);
	ctx->rpc_connection = p;
	ctx->mem_ctx = mem_ctx;

	ctx->info.szDisplayName = NULL;
	ctx->info.szDNPrefix = NULL;
	
	r.out.handle = &ctx->handle;

	r.in.szUserDN = session->profile->mailbox;
	r.in.ulFlags = 0x00000000;
	r.in.ulConMod = emsmdb_hash(r.in.szUserDN);
	r.in.cbLimit = 0x00000000;
	r.in.ulCpid = session->profile->codepage;
	r.in.ulLcidString = session->profile->language;
	r.in.ulLcidSort = session->profile->method;
	r.in.ulIcxrLink = 0xFFFFFFFF;
	r.in.usFCanConvertCodePages = 0x1;

	r.out.szDNPrefix = (const char **) &ctx->info.szDNPrefix;
	r.out.szDisplayName = (const char **) &ctx->info.szDisplayName;
	r.out.pcmsPollsMax = &ctx->info.pcmsPollsMax;
	r.out.pcRetry = &ctx->info.pcRetry;
	r.out.pcmsRetryDelay = &ctx->info.pcmsRetryDelay;
	r.out.picxr = &ctx->info.picxr;
	r.out.pulTimeStamp = &pulTimeStamp;

	r.in.rgwClientVersion[0] = 0x000c;
	r.in.rgwClientVersion[1] = 0x183e;
	r.in.rgwClientVersion[2] = 0x03e8;
	r.in.pulTimeStamp = &pulTimeStamp;
	r.in.rgbAuxIn = NULL;
	r.in.cbAuxIn = 0x00000000;

	rgbAuxOut = talloc_zero(ctx->mem_ctx, struct mapi2k7_AuxInfo);
	rgbAuxOut->AUX_HEADER = NULL;
	r.out.rgbAuxOut = rgbAuxOut;

	r.in.pcbAuxOut = &pcbAuxOut;
	r.out.pcbAuxOut = &pcbAuxOut;

	status = dcerpc_EcDoConnectEx_r(p->binding_handle, tmp_ctx, &r);
	retval = r.out.result;
	if (!NT_STATUS_IS_OK(status) || retval) {
		*return_value = retval;
		mapi_errstr("EcDoConnectEx", retval);
		talloc_free(tmp_ctx);
		return NULL;
	}

	ctx->info.szDisplayName = talloc_steal(mem_ctx, ctx->info.szDisplayName);
	ctx->info.szDNPrefix = talloc_steal(mem_ctx, ctx->info.szDNPrefix);

	ctx->info.rgwServerVersion[0] = r.out.rgwServerVersion[0];
	ctx->info.rgwServerVersion[1] = r.out.rgwServerVersion[1];
	ctx->info.rgwServerVersion[2] = r.out.rgwServerVersion[2];
	
	ctx->cred = cred;
	ctx->max_data = 0xFFF0;
	ctx->setup = false;

	talloc_free(tmp_ctx);
	return ctx;
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
	if (!emsmdb_ctx) {
		return MAPI_E_SUCCESS;
	}

	emsmdb_disconnect(emsmdb_ctx);	

	if (emsmdb_ctx == NULL) return 0;

	talloc_free(emsmdb_ctx->cache_requests);

	if (emsmdb_ctx->info.szDisplayName) {
		talloc_free(emsmdb_ctx->info.szDisplayName);
	}

	if (emsmdb_ctx->info.szDNPrefix) {
		talloc_free(emsmdb_ctx->info.szDNPrefix);
	}

	return MAPI_E_SUCCESS;
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

	status = dcerpc_EcDoDisconnect_r(emsmdb_ctx->rpc_connection->binding_handle, emsmdb_ctx, &r);
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

	status = dcerpc_EcDoRpc_r(emsmdb_ctx->rpc_connection->binding_handle, emsmdb_ctx->mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	*res = mapi_response;

	return status;
}


static int mapi_response_destructor(void *data)
{
	struct mapi_response	*mapi_response = (struct mapi_response *)data;

	if (!mapi_response) return 0;

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

	status = dcerpc_EcDoRpc_r(emsmdb_ctx->rpc_connection->binding_handle, mem_ctx, &r);
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
		talloc_set_destructor((void *)mapi_response, NULL);
		r.out.mapi_response->handles = NULL;
	}

	*repl = r.out.mapi_response;

	return status;
}


/**
   \details Make a EMSMDB EXT2 transaction.

   \param emsmdb_ctx pointer to the EMSMDB connection context
   \param mem_ctx pointer to the memory context
   \param req pointer to the MAPI request to send
   \param repl pointer on pointer to the MAPI reply returned by the
   server

   \return NT_STATUS_OK on success, otherwise NT status error
 */
_PUBLIC_ NTSTATUS emsmdb_transaction_ext2(struct emsmdb_context *emsmdb_ctx,
					  TALLOC_CTX *mem_ctx,
					  struct mapi_request *req,
					  struct mapi_response **repl)
{
	NTSTATUS		status;
	struct EcDoRpcExt2	r;
	struct mapi2k7_response	mapi2k7_response;
	struct ndr_push		*ndr_uncomp_rgbIn;
	struct ndr_push		*ndr_comp_rgbIn;
	struct ndr_push		*ndr_rgbIn;
	struct ndr_pull		*ndr_pull = NULL;
	uint32_t		pulFlags = 0x0;
	uint32_t		pcbOut = 0x8007;
	uint32_t		pcbAuxOut = 0x1008;
	uint32_t		pulTransTime = 0;
	DATA_BLOB		rgbOut;
	struct RPC_HEADER_EXT	RPC_HEADER_EXT;
	enum ndr_err_code ndr_err;

	r.in.handle = r.out.handle = &emsmdb_ctx->handle;
	r.in.pulFlags = r.out.pulFlags = &pulFlags;

	/* Step 1. Push mapi_request in a data blob */
	ndr_uncomp_rgbIn = ndr_push_init_ctx(mem_ctx);
	ndr_set_flags(&ndr_uncomp_rgbIn->flags, LIBNDR_FLAG_NOALIGN);
	ndr_push_mapi_request(ndr_uncomp_rgbIn, NDR_SCALARS|NDR_BUFFERS, req);

	/* Step 2. Compress the blob */
	/* 	ndr_comp_rgbIn = ndr_push_init_ctx(mem_ctx); */
	/* 	ndr_push_lzxpress_compress(ndr_comp_rgbIn, ndr_uncomp_rgbIn); */

	/* If the compressed blob is larger than the uncompressed one, use obfuscation */
	/* 	if (ndr_comp_rgbIn->offset > ndr_uncomp_rgbIn->offset) { */
	/* 		talloc_free(ndr_comp_rgbIn); */
	ndr_comp_rgbIn = ndr_uncomp_rgbIn;
	obfuscate_data(ndr_comp_rgbIn->data, ndr_comp_rgbIn->offset, 0xA5);
		
	RPC_HEADER_EXT.Version = 0x0000;
	RPC_HEADER_EXT.Flags = RHEF_XorMagic|RHEF_Last;
	RPC_HEADER_EXT.Size = ndr_comp_rgbIn->offset;
	RPC_HEADER_EXT.SizeActual = ndr_comp_rgbIn->offset;

	ndr_rgbIn = ndr_push_init_ctx(mem_ctx);
	ndr_set_flags(&ndr_rgbIn->flags, LIBNDR_FLAG_NOALIGN);
	ndr_push_RPC_HEADER_EXT(ndr_rgbIn, NDR_SCALARS|NDR_BUFFERS, &RPC_HEADER_EXT);
	ndr_push_bytes(ndr_rgbIn, ndr_comp_rgbIn->data, ndr_comp_rgbIn->offset);
		/* 	} else { */
		/* 		RPC_HEADER_EXT.Version = 0x0000; */
		/* 		RPC_HEADER_EXT.Flags = RHEF_Compressed|RHEF_Last; */
		/* 		RPC_HEADER_EXT.Size = ndr_comp_rgbIn->offset; */
		/* 		RPC_HEADER_EXT.SizeActual = ndr_uncomp_rgbIn->offset; */

		/* 		ndr_rgbIn = ndr_push_init_ctx(mem_ctx); */
		/* 		ndr_set_flags(&ndr_rgbIn->flags, LIBNDR_FLAG_NOALIGN); */
		/* 		ndr_push_RPC_HEADER_EXT(ndr_rgbIn, NDR_SCALARS|NDR_BUFFERS, &RPC_HEADER_EXT); */
		/* 		ndr_push_bytes(ndr_rgbIn, ndr_comp_rgbIn->data, ndr_comp_rgbIn->offset); */
		/* 	} */

		/* Plain request (no obfuscation or compression) */
		/* ndr_comp_rgbIn = ndr_uncomp_rgbIn; */
		/* RPC_HEADER_EXT.Version = 0x0000; */
		/* RPC_HEADER_EXT.Flags = RHEF_Last; */
		/* RPC_HEADER_EXT.Size = ndr_uncomp_rgbIn->offset; */
		/* RPC_HEADER_EXT.SizeActual = ndr_uncomp_rgbIn->offset; */

		/* Pull the complete rgbIn */
		/* ndr_rgbIn = ndr_push_init_ctx(mem_ctx); */
		/* ndr_set_flags(&ndr_rgbIn->flags, LIBNDR_FLAG_NOALIGN); */
		/* ndr_push_RPC_HEADER_EXT(ndr_rgbIn, NDR_SCALARS|NDR_BUFFERS, &RPC_HEADER_EXT); */
		/* ndr_push_bytes(ndr_rgbIn, ndr_comp_rgbIn->data, ndr_comp_rgbIn->offset); */

	r.in.rgbIn = ndr_rgbIn->data;
	r.in.cbIn = ndr_rgbIn->offset;
	r.in.pcbOut = r.out.pcbOut = &pcbOut;

	r.in.rgbAuxIn = NULL;
	r.in.cbAuxIn = 0;

	r.in.pcbAuxOut = r.out.pcbAuxOut = &pcbAuxOut;

	r.out.pulTransTime = &pulTransTime;

	status = dcerpc_EcDoRpcExt2_r(emsmdb_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	talloc_free(ndr_rgbIn);
	talloc_free(ndr_comp_rgbIn);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	} else if (r.out.result) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* Pull MAPI response form rgbOut */
	rgbOut.data = r.out.rgbOut;
	rgbOut.length = *r.out.pcbOut;
	ndr_pull = ndr_pull_init_blob(&rgbOut, mem_ctx);
	ndr_set_flags(&ndr_pull->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REF_ALLOC);

	ndr_err = ndr_pull_mapi2k7_response(ndr_pull, NDR_SCALARS|NDR_BUFFERS, &mapi2k7_response);
	if (ndr_err != NDR_ERR_SUCCESS) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	*repl = mapi2k7_response.mapi_response;

	return status;
}


_PUBLIC_ NTSTATUS emsmdb_transaction_wrapper(struct mapi_session *session,
					     TALLOC_CTX *mem_ctx,
					     struct mapi_request *req,
					     struct mapi_response **repl)
{
	if (session->emsmdb->ctx == NULL) return NT_STATUS_INVALID_PARAMETER;
	switch (session->profile->exchange_version) {
	case 0x0:
	  return emsmdb_transaction((struct emsmdb_context *)session->emsmdb->ctx, mem_ctx, req, repl);
	case 0x1:
	case 0x2:
	  return emsmdb_transaction_ext2((struct emsmdb_context *)session->emsmdb->ctx, mem_ctx, req, repl);
		break;
	}

	return NT_STATUS_OK;
}


/**
   \details Initialize the notify context structure and bind a local
   UDP port to receive notifications from the server

   \param mapi_ctx pointer to the MAPI context
   \param mem_ctx pointer to the memory context

   \return an allocated mapi_notify_ctx structure on success,
   otherwise NULL
 */
struct mapi_notify_ctx *emsmdb_bind_notification(struct mapi_context *mapi_ctx,
						 TALLOC_CTX *mem_ctx)
{
	struct interface	*ifaces;
	struct mapi_notify_ctx	*notify_ctx = NULL;
	unsigned short		port = DFLT_NOTIF_PORT;
	const char		*ipaddr = NULL;
	uint32_t		attempt = 0;

	/* Sanity Checks */
	if (!mapi_ctx) return NULL;
	if (!mapi_ctx->session) return NULL;
	if (!mapi_ctx->session->profile) return NULL;

	notify_ctx = talloc_zero(mem_ctx, struct mapi_notify_ctx);

	notify_ctx->notifications = talloc_zero((TALLOC_CTX *)notify_ctx, struct notifications);
	notify_ctx->notifications->prev = NULL;
	notify_ctx->notifications->next = NULL;

	openchange_load_interfaces(mem_ctx, lpcfg_interfaces(mapi_ctx->lp_ctx), &ifaces);
	ipaddr = iface_best_ip(ifaces, mapi_ctx->session->profile->server);
	if (!ipaddr) {
		talloc_free(notify_ctx->notifications);
		talloc_free(notify_ctx);
		return NULL;
	}
	notify_ctx->addr = talloc_zero(mem_ctx, struct sockaddr);
	notify_ctx->addr->sa_family = AF_INET;
	((struct sockaddr_in *)(notify_ctx->addr))->sin_addr.s_addr = inet_addr(ipaddr);
retry:
	if (attempt) port++;
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
		if (attempt < 3) {
			attempt++;
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
   
   \param session Pointer to the current MAPI session
   \param notifkey The opaque client-generated context data

   \return NTSTATUS_OK on success, otherwise NT status error
 */
NTSTATUS emsmdb_register_notification(struct mapi_session *session,
				      struct NOTIFKEY *notifkey)
{
	struct EcRRegisterPushNotification	request;
	NTSTATUS				status;
	enum MAPISTATUS				retval;
	TALLOC_CTX				*mem_ctx;
	struct emsmdb_context			*emsmdb_ctx;
	struct mapi_notify_ctx			*notify_ctx;
	struct policy_handle			handle;
	uint32_t				hNotification = 0;

	/* Sanity Checks*/
	if (!notifkey) return NT_STATUS_INVALID_PARAMETER;

	emsmdb_ctx = (struct emsmdb_context *)session->emsmdb->ctx;
	notify_ctx = (struct mapi_notify_ctx *)session->notify_ctx;
	mem_ctx = talloc_named(session, 0, "emsmdb_register_notification");

	request.in.handle = &emsmdb_ctx->handle;
	request.in.iRpc = 0x0;
	request.in.cbContext = notifkey->cb;
	request.in.rgbContext = talloc_array(mem_ctx, uint8_t, request.in.cbContext);
	memcpy(request.in.rgbContext, notifkey->ab, request.in.cbContext);
	request.in.grbitAdviseBits = 0xffffffff;
	request.in.rgbCallbackAddress = talloc_array(mem_ctx, uint8_t, sizeof (struct sockaddr));
	/* cp address family and length */
	request.in.rgbCallbackAddress[0] = (notify_ctx->addr->sa_family & 0xFF);
	request.in.rgbCallbackAddress[1] = (notify_ctx->addr->sa_family & 0xFF00) >> 8;
	memcpy(&request.in.rgbCallbackAddress[2], notify_ctx->addr->sa_data, 14);
	request.in.cbCallbackAddress = sizeof (struct sockaddr);

	request.out.handle = &handle;
	request.out.hNotification = &hNotification;

	status = dcerpc_EcRRegisterPushNotification_r(emsmdb_ctx->rpc_connection->binding_handle, emsmdb_ctx->mem_ctx, &request);
	retval = request.out.result;
	if (!NT_STATUS_IS_OK(status) || retval) {
		status = NT_STATUS_RPC_CALL_FAILED;
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
	if (!session->emsmdb->ctx) {
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
   \param offset pointer on pointer to the current offset
   \param tag the property tag which value is to be retrieved
   \param data pointer to the data

   \return pointer on constant generic data on success, otherwise NULL
 */
const void *pull_emsmdb_property(TALLOC_CTX *mem_ctx,
				 uint32_t *offset, 
				 enum MAPITAGS tag, 
				 DATA_BLOB *data)
{
	struct ndr_pull			*ndr;
	const char			*pt_string8 = NULL;
	const char			*pt_unicode = NULL;
	uint16_t			*pt_i2 = NULL;
	uint64_t			*pt_i8 = NULL;
	uint32_t			*pt_long = NULL;
	uint8_t				*pt_boolean = NULL;
	double				*pt_double = NULL;
	struct FILETIME			*pt_filetime = NULL;
	struct GUID			*pt_clsid = NULL;
	struct SBinary_short		pt_binary;
	struct Binary_r			*sbin = NULL;
	struct mapi_SLPSTRArray		pt_slpstr;
	struct StringArray_r		*slpstr = NULL;
	struct mapi_SLPSTRArrayW	pt_slpstrw;
	struct StringArrayW_r		*slpstrw = NULL;
	struct mapi_MV_LONG_STRUCT	pt_MVl;
	struct LongArray_r		*MVl = NULL;
	struct mapi_SBinaryArray	pt_MVbin;
	struct BinaryArray_r		*MVbin = NULL;
	uint32_t			i;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	ndr->offset = *offset;
	ndr->data = data->data;
	ndr->data_size = data->length;
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);

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
	case PT_DOUBLE:
		pt_double = talloc_zero(mem_ctx, double);
		ndr_pull_double(ndr, NDR_SCALARS, pt_double);
		*offset = ndr->offset;
		talloc_free(ndr);
		return (const void *) pt_double;
	case PT_UNICODE:
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
		ndr_pull_string(ndr, NDR_SCALARS, &pt_unicode);
		*offset = ndr->offset;
		talloc_free(ndr);
		return (const void *) pt_unicode;
	case PT_STRING8:
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_RAW8|LIBNDR_FLAG_STR_NULLTERM);
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
		sbin->lpb = (uint8_t *)talloc_memdup(sbin, pt_binary.lpb, pt_binary.cb);
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
	case PT_MV_UNICODE:
		ndr_pull_mapi_SLPSTRArrayW(ndr, NDR_SCALARS, &pt_slpstrw);
		*offset = ndr->offset;
		slpstrw = talloc_zero(mem_ctx, struct StringArrayW_r);
		slpstrw->cValues = pt_slpstrw.cValues;
		slpstrw->lppszW = talloc_array(mem_ctx, const char *, pt_slpstrw.cValues);
		for (i = 0; i < slpstrw->cValues; i++) {
			slpstrw->lppszW[i] = talloc_strdup(mem_ctx, pt_slpstrw.strings[i].lppszW);
		}
		talloc_free(ndr);
		return (const void *) slpstrw;
	case PT_MV_BINARY:
		ndr_pull_mapi_SBinaryArray(ndr, NDR_SCALARS, &pt_MVbin);
		*offset = ndr->offset;
		MVbin = talloc_zero(mem_ctx, struct BinaryArray_r);
		MVbin->cValues = pt_MVbin.cValues;
		MVbin->lpbin = talloc_array(mem_ctx, struct Binary_r, pt_MVbin.cValues);
		for (i = 0; i < MVbin->cValues; i++) {
			MVbin->lpbin[i].cb = pt_MVbin.bin[i].cb;
			MVbin->lpbin[i].lpb = (uint8_t *)talloc_size(mem_ctx, MVbin->lpbin[i].cb);
			memcpy(MVbin->lpbin[i].lpb, pt_MVbin.bin[i].lpb, MVbin->lpbin[i].cb);
		}
		talloc_free(ndr);
		return (const void *) MVbin;
	default:
		fprintf (stderr, "unhandled type case in pull_emsmdb_property(): 0x%x\n", (tag & 0xFFFF));
		return NULL;
	}
}


/**
   \details Get a SPropValue array from a DATA blob

   \param mem_ctx pointer to the memory context
   \param content pointer to the DATA blob content
   \param tags pointer to a list of property tags to lookup
   \param propvals pointer on pointer to the returned SPropValues
   \param cn_propvals pointer to the number of propvals
   \param flag describes the type data

   \return MAPI_E_SUCCESS on success
 */
enum MAPISTATUS emsmdb_get_SPropValue(TALLOC_CTX *mem_ctx,
				      DATA_BLOB *content,
				      struct SPropTagArray *tags,
				      struct SPropValue **propvals, 
				      uint32_t *cn_propvals,
				      uint8_t flag)
{
	struct SPropValue	*p_propval;
	uint32_t		i_propval;
	uint32_t		i_tag;
	int			proptag;
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
				proptag = (int)tags->aulPropTag[i_tag];
				proptag &= 0xFFFF0000;
				proptag |= PT_ERROR;
				tags->aulPropTag[i_tag] = (enum MAPITAGS) proptag;
			}
			offset += sizeof (uint8_t);
		}

		data = pull_emsmdb_property(mem_ctx, &offset, tags->aulPropTag[i_tag], content);
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

	(*propvals)[i_propval].ulPropTag = (enum MAPITAGS) 0x0;
	*cn_propvals = i_propval;
	return MAPI_E_SUCCESS;
}


/**
   \details Get a SPropValue array from a DATA blob

   \param mem_ctx pointer to the memory context
   \param content pointer to the DATA blob content
   \param tags pointer to a list of property tags to lookup
   \param propvals pointer on pointer to the returned SPropValues
   \param cn_propvals pointer to the number of propvals
   \param _offset the offset to return

   \return MAPI_E_SUCCESS on success
 */
enum MAPISTATUS emsmdb_get_SPropValue_offset(TALLOC_CTX *mem_ctx,
					     DATA_BLOB *content,
					     struct SPropTagArray *tags,
					     struct SPropValue **propvals,
					     uint32_t *cn_propvals,
					     uint32_t *_offset)
{
	struct SPropValue	*lpProps;
	uint32_t		i_propval;
	uint32_t		i_tag;
	int			proptag;
	uint32_t		cn_tags;
	uint32_t		offset = *_offset;
	const void		*data;
	bool			is_FlaggedPropertyRow = false;
	bool			havePropertyValue;
	uint8_t			flag;

	i_propval = 0;
	cn_tags = tags->cValues;
	*cn_propvals = 0;

	if (0x1 == *(content->data + offset)) {
		is_FlaggedPropertyRow = true;
	} else {
		is_FlaggedPropertyRow = false;
	}
	++offset;

	lpProps = talloc_array(mem_ctx, struct SPropValue, cn_tags);
	for (i_tag = 0; i_tag < cn_tags; i_tag++) {
		havePropertyValue = true;
		lpProps[i_tag].ulPropTag = tags->aulPropTag[i_tag];
		if (is_FlaggedPropertyRow) {
			flag = (uint8_t)(*(content->data + offset));
			++offset;
			switch (flag) {
			case 0x0:
				break;
			case 0x1:
				havePropertyValue = false;
				break;
			case PT_ERROR:
				proptag = (int)tags->aulPropTag[i_tag];
				proptag &= 0xFFFF0000;
				proptag |= PT_ERROR;
				lpProps[i_tag].ulPropTag = (enum MAPITAGS) proptag;
				break;
			default:
				break;
			}
		}

		if (havePropertyValue) {
			lpProps[i_tag].dwAlignPad = 0x0;
			data = pull_emsmdb_property(mem_ctx, &offset, lpProps[i_tag].ulPropTag, content);
			talloc_steal(lpProps, data);
			set_SPropValue(&lpProps[i_tag], data);
			free_emsmdb_property(&lpProps[i_tag], (void *)data);
			i_propval++;
		}
	}

	*propvals = lpProps;
	*cn_propvals = i_propval;
	*_offset = offset;
	return MAPI_E_SUCCESS;
}


/**
   \details Get a SRowSet from a DATA blob

   \param mem_ctx pointer on the memory context
   \param rowset pointer on the returned SRowSe
   \param proptags pointer on a list of property tags to lookup
   \param content pointer on the DATA blob content

   \return MAPI_E_SUCCESS on success

   \note TODO: this doesn't yet handle the TypedPropertyValue and
   FlaggedPropertyValueWithTypeSpecified variants
 */
_PUBLIC_ void emsmdb_get_SRowSet(TALLOC_CTX *mem_ctx,
				 struct SRowSet *rowset, 
				 struct SPropTagArray *proptags, 
				 DATA_BLOB *content)
{
	struct SRow		*rows;
	struct SPropValue	*lpProps;
	int			proptag;
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
					proptag = (int) lpProps[prop].ulPropTag;
					proptag &= 0xFFFF0000;
					proptag |= PT_ERROR;
					lpProps[prop].ulPropTag = (enum MAPITAGS) proptag;
					break;
				default:
					/* unknown FlaggedPropertyValue flag */
					break;

				}
			}
			if (havePropertyValue) {
				lpProps[prop].dwAlignPad = 0x0;
				data = pull_emsmdb_property(mem_ctx, &offset, lpProps[prop].ulPropTag, content);
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
		     struct SRow *aRow, 
		     struct SPropTagArray *proptags, 
		     uint16_t propcount, 
		     DATA_BLOB *content, 
		     uint8_t flag, 
		     uint8_t align)
{
	uint32_t		i;
	uint32_t		offset = 0;
	enum MAPITAGS		aulPropTag = (enum MAPITAGS) 0;
	int			proptag;
	const void		*data;

	aRow->cValues = propcount;
	aRow->lpProps = talloc_array(mem_ctx, struct SPropValue, propcount);

	for (i = 0; i < propcount; i++) {
		aulPropTag = proptags->aulPropTag[i];
		if (flag) {
			if (((uint8_t)(*(content->data + offset))) == PT_ERROR) {
				proptag = (int) aulPropTag;
				proptag &= 0xFFFF0000;
				proptag |= 0xA;			
				aulPropTag = (enum MAPITAGS)proptag;
			}
			offset += align;
		} 

		data = pull_emsmdb_property(mem_ctx, &offset, aulPropTag, content);
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

/**
   \details Get an async notification context handle

   \param emsmdb_ctx pointer to the EMSMDB context

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
enum MAPISTATUS emsmdb_async_connect(struct emsmdb_context *emsmdb_ctx)
{
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	struct EcDoAsyncConnectEx	r;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!emsmdb_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	r.in.handle = &(emsmdb_ctx->handle);
	r.out.async_handle = &(emsmdb_ctx->async_handle);
	status = dcerpc_EcDoAsyncConnectEx_r(emsmdb_ctx->rpc_connection->binding_handle, emsmdb_ctx->mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);
	
	return MAPI_E_SUCCESS;
}

bool server_version_at_least(struct emsmdb_context *ctx, uint16_t major_ver, uint16_t minor_ver, uint16_t major_build, uint16_t minor_build)
{
	/* See MS-OXCRPC Section 3.1.9 to understand this */
	uint16_t normalisedword0;
	uint16_t normalisedword1;
	uint16_t normalisedword2;
	uint16_t normalisedword3;

	if (ctx->info.rgwServerVersion[1] & 0x8000) {
		/* new format */
		normalisedword0 = (ctx->info.rgwServerVersion[0] & 0xFF00) >> 8;
		normalisedword1 = (ctx->info.rgwServerVersion[0] & 0x00FF);
		normalisedword2 = (ctx->info.rgwServerVersion[1] & 0x7FFF);
		normalisedword3 = ctx->info.rgwServerVersion[2];
	} else {
		normalisedword0 = ctx->info.rgwServerVersion[0];
		normalisedword1 = 0;
		normalisedword2 = ctx->info.rgwServerVersion[1];
		normalisedword3 = ctx->info.rgwServerVersion[2];
	}
	if (normalisedword0 < major_ver) {
		/* the server major version is less than the minimum we wanted */
		return false;
	}
	if (normalisedword0 > major_ver) {
		/* the server major version is greater than we wanted */
		return true;
	}
	/* the server major number matches the minimum we wanted, so proceed to check further */
	if (normalisedword1 < minor_ver) {
		/* major numbers match, but minor version was too low */
		return false;
	}
	if (normalisedword1 > minor_ver) {
		/* major numbers match, and minor number was greater, so thats enough */
		return true;
	}
	/* both major and minor versions match, start testing build numbers */
	if (normalisedword2 < major_build) {
		/* major and minor numbers match, build number less than required */
		return false;
	}
	if (normalisedword2 > major_build) {
		/* major and minor numbers match, build number was greater */
		return true;
	}
	/* major and minor versions and major build numbers match */
	if (normalisedword3 < minor_build) {
		/* not quite high enough */
		return false;
	}
	/* if we get here, then major and minor build numbers match, major build matches
	   and minor build was greater than or equal to that required */
	return true;
}
