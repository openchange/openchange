/*
   MAPI Proxy - Exchange EMSMDB Server

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2015

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
   \file dcesrv_exchange_emsmdb.c

   \brief OpenChange EMSMDB Server implementation
 */

#include <sys/time.h>

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/fault_util.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"

struct exchange_emsmdb_session		*emsmdb_session = NULL;
void					*openchange_db_ctx = NULL;

static struct exchange_emsmdb_session *dcesrv_find_emsmdb_session(struct GUID *uuid)
{
	struct exchange_emsmdb_session	*session, *found_session = NULL;

	for (session = emsmdb_session; !found_session && session; session = session->next) {
		if (GUID_equal(uuid, &session->uuid)) {
			found_session = session;
		}
	}

	return found_session;
}

/* FIXME: See _unbind below */
/* static struct exchange_emsmdb_session *dcesrv_find_emsmdb_session_by_server_id(const struct server_id *server_id, uint32_t context_id) */
/* { */
/* 	struct exchange_emsmdb_session	*session; */

/* 	for (session = emsmdb_session; session; session = session->next) { */
/* 		if (session->session */
/* 		    && session->session->server_id.id == server_id->id && session->session->server_id.id2 == server_id->id2 && session->session->server_id.node == server_id->node */
/* 		    && session->session->context_id == context_id) { */
/* 			return session; */
/* 		} */
/* 	} */

/* 	return NULL; */
/* } */

/**
   \details exchange_emsmdb EcDoConnect (0x0) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoConnect request data

   \note Session linking is not supported at the moment

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcDoConnect(struct dcesrv_call_state *dce_call,
					  TALLOC_CTX *mem_ctx,
					  struct EcDoConnect *r)
{
	struct emsmdbp_context		*emsmdbp_ctx;
	struct dcesrv_handle		*handle;
	struct policy_handle		wire_handle;
	struct exchange_emsmdb_session	*session;
	struct ldb_message		*msg;
	const char			*mailNickname;
	const char			*userDN;
	char				*dnprefix;

	OC_DEBUG(3, "exchange_emsmdb: EcDoConnect (0x0)\n");

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate\n");
	failure:
		wire_handle.handle_type = EXCHANGE_HANDLE_EMSMDB;
		wire_handle.uuid = GUID_zero();
		*r->out.handle = wire_handle;

		r->out.pcmsPollsMax = talloc_zero(mem_ctx, uint32_t);
		r->out.pcRetry = talloc_zero(mem_ctx, uint32_t);
		r->out.pcmsRetryDelay = talloc_zero(mem_ctx, uint32_t);
		r->out.picxr = talloc_zero(mem_ctx, uint32_t);
		r->out.pullTimeStamp = talloc_zero(mem_ctx, uint32_t);

		*r->out.pcmsPollsMax = 0;
		*r->out.pcRetry = 0;
		*r->out.pcmsRetryDelay = 0;
		*r->out.szDisplayName = NULL;
		*r->out.szDNPrefix = NULL;
		r->out.rgwServerVersion[0] = 0;
		r->out.rgwServerVersion[1] = 0;
		r->out.rgwServerVersion[2] = 0;
		r->out.rgwClientVersion[0] = 0;
		r->out.rgwClientVersion[1] = 0;
		r->out.rgwClientVersion[2] = 0;
		*r->out.pullTimeStamp = 0;

		r->out.result = MAPI_E_LOGON_FAILED;
		return MAPI_E_LOGON_FAILED;
	}

	/* Step 1. Initialize the emsmdbp context */
	emsmdbp_ctx = emsmdbp_init(dce_call->conn->dce_ctx->lp_ctx, 
				   dcesrv_call_account_name(dce_call),
				   openchange_db_ctx);
	if (!emsmdbp_ctx) {
		OC_PANIC(false, ("[exchange_emsmdb] EcDoConnect failed: unable to initialize emsmdbp context\n"));
		goto failure;
	}

	/* Step 2. Check if incoming user belongs to the Exchange organization */
	if (emsmdbp_verify_user(dce_call, emsmdbp_ctx) == false) {
		talloc_free(emsmdbp_ctx);
		goto failure;
	}

	/* Step 3. Check if input user DN belongs to the Exchange organization */
	if (emsmdbp_verify_userdn(dce_call, emsmdbp_ctx, r->in.szUserDN, &msg) == false) {
		talloc_free(emsmdbp_ctx);
		goto failure;
	}

	emsmdbp_ctx->szUserDN = talloc_strdup(emsmdbp_ctx, r->in.szUserDN);
	emsmdbp_ctx->userLanguage = r->in.ulLcidString;

	/* Step 4. Retrieve the display name of the user */
	*r->out.szDisplayName = ldb_msg_find_attr_as_string(msg, "displayName", NULL);
	emsmdbp_ctx->szDisplayName = talloc_strdup(emsmdbp_ctx, *r->out.szDisplayName);

	/* Step 5. Retrieve the dinstinguished name of the server */
	mailNickname = ldb_msg_find_attr_as_string(msg, "mailNickname", NULL);
	userDN = ldb_msg_find_attr_as_string(msg, "legacyExchangeDN", NULL);
	dnprefix = strstr(userDN, mailNickname);
	if (!dnprefix) {
		talloc_free(emsmdbp_ctx);
		goto failure;
	}

	*dnprefix = '\0';
	*r->out.szDNPrefix = strupper_talloc(mem_ctx, userDN);

	/* Step 6. Fill EcDoConnect reply */
	handle = dcesrv_handle_new(dce_call->context, EXCHANGE_HANDLE_EMSMDB);
	OPENCHANGE_RETVAL_IF(!handle, MAPI_E_NOT_ENOUGH_RESOURCES, emsmdbp_ctx);
	
	handle->data = (void *) emsmdbp_ctx;
	*r->out.handle = handle->wire_handle;

	r->out.pcmsPollsMax = talloc_zero(mem_ctx, uint32_t);
	*r->out.pcmsPollsMax = EMSMDB_PCMSPOLLMAX;

	r->out.pcRetry = talloc_zero(mem_ctx, uint32_t);
	*r->out.pcRetry = EMSMDB_PCRETRY;

	r->out.pcmsRetryDelay = talloc_zero(mem_ctx, uint32_t);
	*r->out.pcmsRetryDelay = EMSMDB_PCRETRYDELAY;

	r->out.picxr = talloc_zero(mem_ctx, uint32_t);
	*r->out.picxr = 0;

	/* The following server version is not supported by Outlook 2010 */
	r->out.rgwServerVersion[0] = 0x6;
	r->out.rgwServerVersion[1] = 0x1141;
	r->out.rgwServerVersion[2] = 0x5;

	/* This one is but requires EcDoConnectEx/EcDoRpcExt2/Async
	 * EMSMDB implementation */

	/* r->out.rgwServerVersion[0] = 0x8; */
	/* r->out.rgwServerVersion[1] = 0x82B4; */
	/* r->out.rgwServerVersion[2] = 0x3; */

	r->out.rgwClientVersion[0] = r->in.rgwClientVersion[0];
	r->out.rgwClientVersion[1] = r->in.rgwClientVersion[1];
	r->out.rgwClientVersion[2] = r->in.rgwClientVersion[2];

	r->out.pullTimeStamp = talloc_zero(mem_ctx, uint32_t);
	*r->out.pullTimeStamp = time(NULL);

	r->out.result = MAPI_E_SUCCESS;

	/* Search for an existing session and increment ref_count, otherwise create it */
	session = dcesrv_find_emsmdb_session(&handle->wire_handle.uuid);
	if (session) {
		OC_DEBUG(0, "[exchange_emsmdb]: Increment session ref count for %d\n",
				 session->session->context_id);
		mpm_session_increment_ref_count(session->session);
	}
	else {
		/* Step 7. Associate this emsmdbp context to the session */
		session = talloc((TALLOC_CTX *)emsmdb_session, struct exchange_emsmdb_session);
		OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_ENOUGH_RESOURCES, emsmdbp_ctx);

		session->pullTimeStamp = *r->out.pullTimeStamp;
		session->session = mpm_session_init((TALLOC_CTX *)emsmdb_session, dce_call);
		OPENCHANGE_RETVAL_IF(!session->session, MAPI_E_NOT_ENOUGH_RESOURCES, emsmdbp_ctx);

                session->uuid = handle->wire_handle.uuid;

		mpm_session_set_private_data(session->session, (void *) emsmdbp_ctx);
		mpm_session_set_destructor(session->session, emsmdbp_destructor);

		OC_DEBUG(0, "[exchange_emsmdb]: New session added: %d\n", session->session->context_id);

		DLIST_ADD_END(emsmdb_session, session, struct exchange_emsmdb_session *);
	}

	return MAPI_E_SUCCESS;
}


/**
   \details exchange_emsmdb EcDoDisconnect (0x1) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoDisconnect request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcDoDisconnect(struct dcesrv_call_state *dce_call,
					     TALLOC_CTX *mem_ctx,
					     struct EcDoDisconnect *r)
{
	struct dcesrv_handle		*h;
	struct exchange_emsmdb_session	*session;
	bool				ret;

	OC_DEBUG(3, "exchange_emsmdb: EcDoDisconnect (0x1)\n");

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate\n");
		return MAPI_E_LOGON_FAILED;
	}

	/* Step 1. Retrieve handle and free if emsmdbp context and session are available */
	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	if (h) {
		session = dcesrv_find_emsmdb_session(&r->in.handle->uuid);
		if (session) {
			ret = mpm_session_release(session->session);
			if (ret == true) {
				DLIST_REMOVE(emsmdb_session, session);
				OC_DEBUG(5, "Session found and released\n");
			} else {
				OC_DEBUG(5, "Session found and ref_count decreased\n");
			}
		} else {
			OC_DEBUG(5, "  emsmdb_session NOT found\n");
		}
	}

	r->out.handle->handle_type = 0;
	r->out.handle->uuid = GUID_zero();

	r->out.result = MAPI_E_SUCCESS;

	return MAPI_E_SUCCESS;
}

static struct mapi_response *EcDoRpc_process_transaction(TALLOC_CTX *mem_ctx, 
							 struct emsmdbp_context *emsmdbp_ctx,
							 struct mapi_request *mapi_request,
							 struct GUID uuid)
{
	enum MAPISTATUS		retval;
	struct mapi_response	*mapi_response;
	uint32_t		handles_length;
	uint16_t		size = 0;
	uint32_t		i;
	uint32_t		idx;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!mapi_request) return NULL;

	/* Allocate mapi_response */
	mapi_response = talloc_zero(mem_ctx, struct mapi_response);
	mapi_response->handles = mapi_request->handles;

	/* Step 1. Handle Idle requests case */
	if (mapi_request->mapi_len <= 2) {
		mapi_response->mapi_len = 2;
		idx = 0;
		goto notif;
	}

	/* Step 2. Process serialized MAPI requests */
	mapi_response->mapi_repl = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REPL);
	for (i = 0, idx = 0, size = 0; mapi_request->mapi_req[i].opnum != 0; i++) {
		OC_DEBUG(0, "MAPI Rop: 0x%.2x (%d)\n", mapi_request->mapi_req[i].opnum, size);

		if (mapi_request->mapi_req[i].opnum != op_MAPI_Release) {
			mapi_response->mapi_repl = talloc_realloc(mem_ctx, mapi_response->mapi_repl,
								  struct EcDoRpc_MAPI_REPL, idx + 2);
		}

		switch (mapi_request->mapi_req[i].opnum) {
		case op_MAPI_Release: /* 0x01 */
			retval = EcDoRpc_RopRelease(mem_ctx, emsmdbp_ctx, 
						    &(mapi_request->mapi_req[i]),
						    mapi_request->handles, &size);
			break;
		case op_MAPI_OpenFolder: /* 0x02 */
			retval = EcDoRpc_RopOpenFolder(mem_ctx, emsmdbp_ctx,
						       &(mapi_request->mapi_req[i]),
						       &(mapi_response->mapi_repl[idx]),
						       mapi_response->handles, &size);
			break;
		case op_MAPI_OpenMessage: /* 0x3 */
			retval = EcDoRpc_RopOpenMessage(mem_ctx, emsmdbp_ctx,
							&(mapi_request->mapi_req[i]),
							&(mapi_response->mapi_repl[idx]),
							mapi_response->handles, &size);
			break;
		case op_MAPI_GetHierarchyTable: /* 0x04 */
			retval = EcDoRpc_RopGetHierarchyTable(mem_ctx, emsmdbp_ctx,
							      &(mapi_request->mapi_req[i]),
							      &(mapi_response->mapi_repl[idx]),
							      mapi_response->handles, &size);
			break;
		case op_MAPI_GetContentsTable: /* 0x05 */
			retval = EcDoRpc_RopGetContentsTable(mem_ctx, emsmdbp_ctx,
							     &(mapi_request->mapi_req[i]),
							     &(mapi_response->mapi_repl[idx]),
							     mapi_response->handles, &size);
			break;
		case op_MAPI_CreateMessage: /* 0x06 */
			retval = EcDoRpc_RopCreateMessage(mem_ctx, emsmdbp_ctx,
							  &(mapi_request->mapi_req[i]),
							  &(mapi_response->mapi_repl[idx]),
							  mapi_response->handles, &size);
			break;
		case op_MAPI_GetProps: /* 0x07 */
			retval = EcDoRpc_RopGetPropertiesSpecific(mem_ctx, emsmdbp_ctx,
								  &(mapi_request->mapi_req[i]),
								  &(mapi_response->mapi_repl[idx]),
								  mapi_response->handles, &size);
			break;
		case op_MAPI_GetPropsAll: /* 0x8 */
			retval = EcDoRpc_RopGetPropertiesAll(mem_ctx, emsmdbp_ctx,
							     &(mapi_request->mapi_req[i]),
							     &(mapi_response->mapi_repl[idx]),
							     mapi_response->handles, &size);
			break;
		case op_MAPI_GetPropList: /* 0x9 */
			retval = EcDoRpc_RopGetPropertiesList(mem_ctx, emsmdbp_ctx,
							      &(mapi_request->mapi_req[i]),
							      &(mapi_response->mapi_repl[idx]),	
							      mapi_response->handles, &size);
			break;
		case op_MAPI_SetProps: /* 0x0a */
			retval = EcDoRpc_RopSetProperties(mem_ctx, emsmdbp_ctx,
							  &(mapi_request->mapi_req[i]),
							  &(mapi_response->mapi_repl[idx]),
							  mapi_response->handles, &size);
			break;
		case op_MAPI_DeleteProps: /* 0xb */
			retval = EcDoRpc_RopDeleteProperties(mem_ctx, emsmdbp_ctx,
							     &(mapi_request->mapi_req[i]),
							     &(mapi_response->mapi_repl[idx]),
							     mapi_response->handles, &size);
			break;
		case op_MAPI_SaveChangesMessage: /* 0x0c */
			retval = EcDoRpc_RopSaveChangesMessage(mem_ctx, emsmdbp_ctx,
                                                               &(mapi_request->mapi_req[i]),
                                                               &(mapi_response->mapi_repl[idx]),
                                                               mapi_response->handles, &size);
			break;
		case op_MAPI_RemoveAllRecipients: /* 0xd */
			retval = EcDoRpc_RopRemoveAllRecipients(mem_ctx, emsmdbp_ctx,
								&(mapi_request->mapi_req[i]),
								&(mapi_response->mapi_repl[idx]),
								mapi_response->handles, &size);
			break;
		case op_MAPI_ModifyRecipients: /* 0xe */
			retval = EcDoRpc_RopModifyRecipients(mem_ctx, emsmdbp_ctx,
							     &(mapi_request->mapi_req[i]),
							     &(mapi_response->mapi_repl[idx]),
							     mapi_response->handles, &size);
			break;

		/* op_MAPI_ReadRecipients: 0xf */
		case op_MAPI_ReloadCachedInformation: /* 0x10 */
			retval = EcDoRpc_RopReloadCachedInformation(mem_ctx, emsmdbp_ctx,
								    &(mapi_request->mapi_req[i]),
								    &(mapi_response->mapi_repl[idx]),
								    mapi_response->handles, &size);
			break;
		case op_MAPI_SetMessageReadFlag: /* 0x11 */
			retval = EcDoRpc_RopSetMessageReadFlag(mem_ctx, emsmdbp_ctx,
							       &(mapi_request->mapi_req[i]),
							       &(mapi_response->mapi_repl[idx]),
							       mapi_response->handles, &size);
			break;
		case op_MAPI_SetColumns: /* 0x12 */
			retval = EcDoRpc_RopSetColumns(mem_ctx, emsmdbp_ctx,
						       &(mapi_request->mapi_req[i]),
						       &(mapi_response->mapi_repl[idx]),
						       mapi_response->handles, &size);
			break;
		case op_MAPI_SortTable: /* 0x13 */
			retval = EcDoRpc_RopSortTable(mem_ctx, emsmdbp_ctx,
						      &(mapi_request->mapi_req[i]),
						      &(mapi_response->mapi_repl[idx]),
						      mapi_response->handles, &size);
			break;
		case op_MAPI_Restrict: /* 0x14 */
			retval = EcDoRpc_RopRestrict(mem_ctx, emsmdbp_ctx,
						     &(mapi_request->mapi_req[i]),
						     &(mapi_response->mapi_repl[idx]),
						     mapi_response->handles, &size);
			break;
		case op_MAPI_QueryRows: /* 0x15 */
			retval = EcDoRpc_RopQueryRows(mem_ctx, emsmdbp_ctx,
						      &(mapi_request->mapi_req[i]),
						      &(mapi_response->mapi_repl[idx]),
						      mapi_response->handles, &size);
			break;
		/* op_MAPI_GetStatus: 0x16 */
		case op_MAPI_QueryPosition: /* 0x17 */
			retval = EcDoRpc_RopQueryPosition(mem_ctx, emsmdbp_ctx,
							  &(mapi_request->mapi_req[i]),
							  &(mapi_response->mapi_repl[idx]),
							  mapi_response->handles, &size);
			break;
		case op_MAPI_SeekRow: /* 0x18 */
			retval = EcDoRpc_RopSeekRow(mem_ctx, emsmdbp_ctx,
						    &(mapi_request->mapi_req[i]),
						    &(mapi_response->mapi_repl[idx]),
						    mapi_response->handles, &size);
			break;
		/* op_MAPI_SeekRowBookmark: 0x19 */
		/* op_MAPI_SeekRowApprox: 0x1a */
		/* op_MAPI_CreateBookmark: 0x1b */
		case op_MAPI_CreateFolder: /* 0x1c */
			retval = EcDoRpc_RopCreateFolder(mem_ctx, emsmdbp_ctx,
							 &(mapi_request->mapi_req[i]),
							 &(mapi_response->mapi_repl[idx]),
							 mapi_response->handles, &size);
			break;
		case op_MAPI_DeleteFolder: /* 0x1d */
			retval = EcDoRpc_RopDeleteFolder(mem_ctx, emsmdbp_ctx,
							 &(mapi_request->mapi_req[i]),
							 &(mapi_response->mapi_repl[idx]),
							 mapi_response->handles, &size);
			break;
		case op_MAPI_DeleteMessages: /* 0x1e */
			retval = EcDoRpc_RopDeleteMessages(mem_ctx, emsmdbp_ctx,
							   &(mapi_request->mapi_req[i]),
							   &(mapi_response->mapi_repl[idx]),
							   mapi_response->handles, &size);
			break;
		case op_MAPI_GetMessageStatus: /* 0x1f */
			retval = EcDoRpc_RopGetMessageStatus(mem_ctx, emsmdbp_ctx,
							     &(mapi_request->mapi_req[i]),
							     &(mapi_response->mapi_repl[idx]),
							     mapi_response->handles, &size);
			break;
		/* op_MAPI_SetMessageStatus: 0x20 */
		case op_MAPI_GetAttachmentTable: /* 0x21 */
			retval = EcDoRpc_RopGetAttachmentTable(mem_ctx, emsmdbp_ctx,
							       &(mapi_request->mapi_req[i]),
							       &(mapi_response->mapi_repl[idx]),
							       mapi_response->handles, &size);
			break;
                case op_MAPI_OpenAttach: /* 0x22 */
			retval = EcDoRpc_RopOpenAttach(mem_ctx, emsmdbp_ctx,
                                                       &(mapi_request->mapi_req[i]),
                                                       &(mapi_response->mapi_repl[idx]),
                                                       mapi_response->handles, &size);
			break;
                case op_MAPI_CreateAttach: /* 0x23 */
			retval = EcDoRpc_RopCreateAttach(mem_ctx, emsmdbp_ctx,
                                                         &(mapi_request->mapi_req[i]),
                                                         &(mapi_response->mapi_repl[idx]),
                                                         mapi_response->handles, &size);
			break;
		/* op_MAPI_DeleteAttach: 0x24 */
		case op_MAPI_SaveChangesAttachment: /* 0x25 */
			retval = EcDoRpc_RopSaveChangesAttachment(mem_ctx, emsmdbp_ctx,
                                                                  &(mapi_request->mapi_req[i]),
                                                                  &(mapi_response->mapi_repl[idx]),
                                                                  mapi_response->handles, &size);
			break;
		case op_MAPI_SetReceiveFolder: /* 0x26 */
			retval = EcDoRpc_RopSetReceiveFolder(mem_ctx, emsmdbp_ctx,
							     &(mapi_request->mapi_req[i]),
							     &(mapi_response->mapi_repl[idx]),
							     mapi_response->handles, &size);
			break;
		case op_MAPI_GetReceiveFolder: /* 0x27 */
			retval = EcDoRpc_RopGetReceiveFolder(mem_ctx, emsmdbp_ctx,
							     &(mapi_request->mapi_req[i]),
							     &(mapi_response->mapi_repl[idx]),
							     mapi_response->handles, &size);
			break;
		case op_MAPI_RegisterNotification: /* 0x29 */
			retval = EcDoRpc_RopRegisterNotification(mem_ctx, emsmdbp_ctx,
								 &(mapi_request->mapi_req[i]),
								 &(mapi_response->mapi_repl[idx]),
								 mapi_response->handles, &size);
			break;
		/* op_MAPI_Notify: 0x2a */
		case op_MAPI_OpenStream: /* 0x2b */
			retval = EcDoRpc_RopOpenStream(mem_ctx, emsmdbp_ctx,
						       &(mapi_request->mapi_req[i]),
						       &(mapi_response->mapi_repl[idx]),
						       mapi_response->handles, &size);
			break;
		case op_MAPI_ReadStream: /* 0x2c */
			retval = EcDoRpc_RopReadStream(mem_ctx, emsmdbp_ctx,
						       &(mapi_request->mapi_req[i]),
						       &(mapi_response->mapi_repl[idx]),
						       mapi_response->handles, &size);
			break;
		case op_MAPI_WriteStream: /* 0x2d */
			retval = EcDoRpc_RopWriteStream(mem_ctx, emsmdbp_ctx,
							&(mapi_request->mapi_req[i]),
							&(mapi_response->mapi_repl[idx]),
							mapi_response->handles, &size);
			break;
                case op_MAPI_SeekStream: /* 0x2e */
			retval = EcDoRpc_RopSeekStream(mem_ctx, emsmdbp_ctx,
                                                       &(mapi_request->mapi_req[i]),
                                                       &(mapi_response->mapi_repl[idx]),
                                                       mapi_response->handles, &size);
			break;
                case op_MAPI_SetStreamSize: /* 0x2f */
			retval = EcDoRpc_RopSetStreamSize(mem_ctx, emsmdbp_ctx,
                                                          &(mapi_request->mapi_req[i]),
                                                          &(mapi_response->mapi_repl[idx]),
                                                          mapi_response->handles, &size);
			break;
		case op_MAPI_SetSearchCriteria: /* 0x30 */
			retval = EcDoRpc_RopSetSearchCriteria(mem_ctx, emsmdbp_ctx,
							      &(mapi_request->mapi_req[i]),
							      &(mapi_response->mapi_repl[idx]),
							      mapi_response->handles, &size);
			break;
		case op_MAPI_GetSearchCriteria: /* 0x31 */
			retval = EcDoRpc_RopGetSearchCriteria(mem_ctx, emsmdbp_ctx,
							      &(mapi_request->mapi_req[i]),
							      &(mapi_response->mapi_repl[idx]),
							      mapi_response->handles, &size);
			break;
		case op_MAPI_SubmitMessage: /* 0x32 */
			retval = EcDoRpc_RopSubmitMessage(mem_ctx, emsmdbp_ctx,
							  &(mapi_request->mapi_req[i]),
							  &(mapi_response->mapi_repl[idx]),
							  mapi_response->handles, &size);
			break;
		case op_MAPI_MoveCopyMessages: /* 0x33 */
			retval = EcDoRpc_RopMoveCopyMessages(mem_ctx, emsmdbp_ctx,
							    &(mapi_request->mapi_req[i]),
							    &(mapi_response->mapi_repl[idx]),
							    mapi_response->handles, &size);
		        break;
		/* op_MAPI_AbortSubmit: 0x34 */
		case op_MAPI_MoveFolder: /* 0x35 */
			retval = EcDoRpc_RopMoveFolder(mem_ctx, emsmdbp_ctx,
						       &(mapi_request->mapi_req[i]),
						       &(mapi_response->mapi_repl[idx]),
						       mapi_response->handles, &size);
		        break;
		case op_MAPI_CopyFolder: /* 0x36 */
			retval = EcDoRpc_RopCopyFolder(mem_ctx, emsmdbp_ctx,
						       &(mapi_request->mapi_req[i]),
						       &(mapi_response->mapi_repl[idx]),
						       mapi_response->handles, &size);
		        break;
		/* op_MAPI_QueryColumnsAll: 0x37 */
		/* op_MAPI_Abort: 0x38 */
		case op_MAPI_CopyTo: /* 0x39 */
                        retval = EcDoRpc_RopCopyTo(mem_ctx, emsmdbp_ctx,
                                                   &(mapi_request->mapi_req[i]),
                                                   &(mapi_response->mapi_repl[idx]),
                                                   mapi_response->handles, &size);
			break;
		/* op_MAPI_CopyToStream: 0x3a */
		/* op_MAPI_CloneStream: 0x3b */
		case op_MAPI_GetPermissionsTable: /* 0x3e */
			retval = EcDoRpc_RopGetPermissionsTable(mem_ctx, emsmdbp_ctx,
								&(mapi_request->mapi_req[i]),
								&(mapi_response->mapi_repl[idx]),
								mapi_response->handles, &size);
			break;
		case op_MAPI_GetRulesTable: /* 0x3f */
			retval = EcDoRpc_RopGetRulesTable(mem_ctx, emsmdbp_ctx,
							  &(mapi_request->mapi_req[i]),
							  &(mapi_response->mapi_repl[idx]),
							  mapi_response->handles, &size);
			break;
		case op_MAPI_ModifyPermissions: /* 0x40 */
			retval = EcDoRpc_RopModifyPermissions(mem_ctx, emsmdbp_ctx,
							      &(mapi_request->mapi_req[i]),
							      &(mapi_response->mapi_repl[idx]),
							      mapi_response->handles, &size);
			break;
		case op_MAPI_ModifyRules: /* 0x41 */
			retval = EcDoRpc_RopModifyRules(mem_ctx, emsmdbp_ctx,
							&(mapi_request->mapi_req[i]),
							&(mapi_response->mapi_repl[idx]),
							mapi_response->handles, &size);
			break;
		/* op_MAPI_GetOwningServers: 0x42 */
		case op_MAPI_LongTermIdFromId: /* 0x43 */
			retval = EcDoRpc_RopLongTermIdFromId(mem_ctx, emsmdbp_ctx,
							     &(mapi_request->mapi_req[i]),
							     &(mapi_response->mapi_repl[idx]),
							     mapi_response->handles, &size);
			break;
		case op_MAPI_IdFromLongTermId: /* 0x44 */
			retval = EcDoRpc_RopIdFromLongTermId(mem_ctx, emsmdbp_ctx,
							     &(mapi_request->mapi_req[i]),
							     &(mapi_response->mapi_repl[idx]),
							     mapi_response->handles, &size);
			break;
		/* op_MAPI_PublicFolderIsGhosted: 0x45 */
		case op_MAPI_OpenEmbeddedMessage: /* 0x46 */
			retval = EcDoRpc_RopOpenEmbeddedMessage(mem_ctx, emsmdbp_ctx,
                                                                &(mapi_request->mapi_req[i]),
                                                                &(mapi_response->mapi_repl[idx]),
                                                                mapi_response->handles, &size);
                        break;
		case op_MAPI_SetSpooler: /* 0x47 */
			retval = EcDoRpc_RopSetSpooler(mem_ctx, emsmdbp_ctx,
						       &(mapi_request->mapi_req[i]),
						       &(mapi_response->mapi_repl[idx]),
						       mapi_response->handles, &size);
			break;
		/* op_MAPI_SpoolerLockMessage: 0x48 */
		case op_MAPI_AddressTypes: /*x49 */
			retval = EcDoRpc_RopGetAddressTypes(mem_ctx, emsmdbp_ctx,
							    &(mapi_request->mapi_req[i]),
							    &(mapi_response->mapi_repl[idx]),
							    mapi_response->handles, &size);
			break;
		case op_MAPI_TransportSend: /* 0x4a */
			retval = EcDoRpc_RopTransportSend(mem_ctx, emsmdbp_ctx,
							  &(mapi_request->mapi_req[i]),
							  &(mapi_response->mapi_repl[idx]),
							  mapi_response->handles, &size);
			break;
		/* op_MAPI_FastTransferSourceCopyMessages: 0x4b */
		/* op_MAPI_FastTransferSourceCopyFolder: 0x4c */
		case op_MAPI_FastTransferSourceCopyTo: /* 0x4d */
			retval = EcDoRpc_RopFastTransferSourceCopyTo(mem_ctx, emsmdbp_ctx, 
								     &(mapi_request->mapi_req[i]),
								     &(mapi_response->mapi_repl[idx]),
								     mapi_response->handles, &size);
			break;
		case op_MAPI_FastTransferSourceGetBuffer: /* 0x4e */
			retval = EcDoRpc_RopFastTransferSourceGetBuffer(mem_ctx, emsmdbp_ctx, 
									&(mapi_request->mapi_req[i]),
									&(mapi_response->mapi_repl[idx]),
									mapi_response->handles, &size);
			break;
		case op_MAPI_FindRow: /* 0x4f */
			retval = EcDoRpc_RopFindRow(mem_ctx, emsmdbp_ctx, 
						    &(mapi_request->mapi_req[i]),
						    &(mapi_response->mapi_repl[idx]),
						    mapi_response->handles, &size);
			break;
		/* op_MAPI_Progress: 0x50 */
		/* op_MAPI_TransportNewMail: 0x51 */
		/* op_MAPI_GetValidAttachments: 0x52 */
		case op_MAPI_GetNamesFromIDs: /* 0x55 */
			retval = EcDoRpc_RopGetNamesFromIDs(mem_ctx, emsmdbp_ctx,
							    &(mapi_request->mapi_req[i]),
							    &(mapi_response->mapi_repl[idx]),
							    mapi_response->handles, &size);
			break;
		case op_MAPI_GetIDsFromNames: /* 0x56 */
			retval = EcDoRpc_RopGetPropertyIdsFromNames(mem_ctx, emsmdbp_ctx,
								    &(mapi_request->mapi_req[i]),
								    &(mapi_response->mapi_repl[idx]),
								    mapi_response->handles, &size);
			break;
		/* op_MAPI_UpdateDeferredActionMessages: 0x57 */ 
		case op_MAPI_EmptyFolder: /* 0x58 */
		retval = EcDoRpc_RopEmptyFolder(mem_ctx, emsmdbp_ctx,
						&(mapi_request->mapi_req[i]),
						&(mapi_response->mapi_repl[idx]),
						mapi_response->handles, &size);
			break;
		/* op_MAPI_ExpandRow: 0x59 */
		/* op_MAPI_CollapseRow: 0x5a */
		/* op_MAPI_LockRegionStream: 0x5b */
		/* op_MAPI_UnlockRegionStream: 0x5c */
		case op_MAPI_CommitStream: /* 0x5d */
			retval = EcDoRpc_RopCommitStream(mem_ctx, emsmdbp_ctx,
							 &(mapi_request->mapi_req[i]),
							 &(mapi_response->mapi_repl[idx]),
							 mapi_response->handles, &size);
			break;
		case op_MAPI_GetStreamSize: /* 0x5e */
			retval = EcDoRpc_RopGetStreamSize(mem_ctx, emsmdbp_ctx,
							  &(mapi_request->mapi_req[i]),
							  &(mapi_response->mapi_repl[idx]),
							  mapi_response->handles, &size);
			break;
		/* op_MAPI_QueryNamedProperties: 0x5f */
		case op_MAPI_GetPerUserLongTermIds: /* 0x60 */
			retval = EcDoRpc_RopGetPerUserLongTermIds(mem_ctx, emsmdbp_ctx,
								  &(mapi_request->mapi_req[i]),
								  &(mapi_response->mapi_repl[idx]),
								  mapi_response->handles, &size);
			break;
		case op_MAPI_GetPerUserGuid: /* 0x61 */
			retval = EcDoRpc_RopGetPerUserGuid(mem_ctx, emsmdbp_ctx,
							   &(mapi_request->mapi_req[i]),
							   &(mapi_response->mapi_repl[idx]),
							   mapi_response->handles, &size);
			break;
		case op_MAPI_ReadPerUserInformation: /* 0x63 */
			retval = EcDoRpc_RopReadPerUserInformation(mem_ctx, emsmdbp_ctx,
								   &(mapi_request->mapi_req[i]),
								   &(mapi_response->mapi_repl[idx]),
								   mapi_response->handles, &size);
			break;
		/* op_MAPI_ReadPerUserInformation: 0x63 */
		/* op_MAPI_SetReadFlags: 0x66 */
		/* op_MAPI_CopyProperties: 0x67 */
		case op_MAPI_GetReceiveFolderTable: /* 0x68 */
			retval = EcDoRpc_RopGetReceiveFolderTable(mem_ctx, emsmdbp_ctx,
								  &(mapi_request->mapi_req[i]),
								  &(mapi_response->mapi_repl[idx]),
								  mapi_response->handles, &size);
			break;

		/* op_MAPI_GetCollapseState: 0x6b */
		/* op_MAPI_SetCollapseState: 0x6c */
		case op_MAPI_GetTransportFolder: /* 0x6d */
			retval = EcDoRpc_RopGetTransportFolder(mem_ctx, emsmdbp_ctx,
							       &(mapi_request->mapi_req[i]),
							       &(mapi_response->mapi_repl[idx]),
							       mapi_response->handles, &size);
			break;
		/* op_MAPI_Pending: 0x6e */
		case op_MAPI_OptionsData: /* 0x6f */
			retval = EcDoRpc_RopOptionsData(mem_ctx, emsmdbp_ctx,
							&(mapi_request->mapi_req[i]),
							&(mapi_response->mapi_repl[idx]),
							mapi_response->handles, &size);
			break;
                case op_MAPI_SyncConfigure: /* 0x70 */
			retval = EcDoRpc_RopSyncConfigure(mem_ctx, emsmdbp_ctx,
							  &(mapi_request->mapi_req[i]),
							  &(mapi_response->mapi_repl[idx]),
							  mapi_response->handles, &size);
			break;
		case op_MAPI_SyncImportMessageChange: /* 0x72 */
			retval = EcDoRpc_RopSyncImportMessageChange(mem_ctx, emsmdbp_ctx,
								    &(mapi_request->mapi_req[i]),
								    &(mapi_response->mapi_repl[idx]),
								    mapi_response->handles, &size);
			break;
		case op_MAPI_SyncImportHierarchyChange: /* 0x73 */
			retval = EcDoRpc_RopSyncImportHierarchyChange(mem_ctx, emsmdbp_ctx,
								      &(mapi_request->mapi_req[i]),
								      &(mapi_response->mapi_repl[idx]),
								      mapi_response->handles, &size);
			break;
		case op_MAPI_SyncImportDeletes: /* 0x74 */
			retval = EcDoRpc_RopSyncImportDeletes(mem_ctx, emsmdbp_ctx,
							      &(mapi_request->mapi_req[i]),
							      &(mapi_response->mapi_repl[idx]),
							      mapi_response->handles, &size);
			break;
                case op_MAPI_SyncUploadStateStreamBegin: /* 0x75 */
			retval = EcDoRpc_RopSyncUploadStateStreamBegin(mem_ctx, emsmdbp_ctx,
								       &(mapi_request->mapi_req[i]),
								       &(mapi_response->mapi_repl[idx]),
								       mapi_response->handles, &size);
			break;
                case op_MAPI_SyncUploadStateStreamContinue: /* 0x76 */
			retval = EcDoRpc_RopSyncUploadStateStreamContinue(mem_ctx, emsmdbp_ctx,
									  &(mapi_request->mapi_req[i]),
									  &(mapi_response->mapi_repl[idx]),
									  mapi_response->handles, &size);
			break;
		case op_MAPI_SyncUploadStateStreamEnd: /* 0x77 */
			retval = EcDoRpc_RopSyncUploadStateStreamEnd(mem_ctx, emsmdbp_ctx,
								     &(mapi_request->mapi_req[i]),
								     &(mapi_response->mapi_repl[idx]),
								     mapi_response->handles, &size);
			break;
		case op_MAPI_SyncImportMessageMove: /* 0x78 */
			retval = EcDoRpc_RopSyncImportMessageMove(mem_ctx, emsmdbp_ctx,
								  &(mapi_request->mapi_req[i]),
								  &(mapi_response->mapi_repl[idx]),
								  mapi_response->handles, &size);
			break;
		/* op_MAPI_SetPropertiesNoReplicate: 0x79 */
		case op_MAPI_DeletePropertiesNoReplicate: /* 0x7a */
			retval = EcDoRpc_RopDeletePropertiesNoReplicate(mem_ctx, emsmdbp_ctx,
									&(mapi_request->mapi_req[i]),
									&(mapi_response->mapi_repl[idx]),
									mapi_response->handles, &size);
			break;
		case op_MAPI_GetStoreState: /* 0x7b */
			retval = EcDoRpc_RopGetStoreState(mem_ctx, emsmdbp_ctx,
							  &(mapi_request->mapi_req[i]),
							  &(mapi_response->mapi_repl[idx]),
							  mapi_response->handles, &size);
			break;
		case op_MAPI_SyncOpenCollector: /* 0x7e */
			retval = EcDoRpc_RopSyncOpenCollector(mem_ctx, emsmdbp_ctx,
							      &(mapi_request->mapi_req[i]),
							      &(mapi_response->mapi_repl[idx]),
							      mapi_response->handles, &size);
			break;
		case op_MAPI_GetLocalReplicaIds: /* 0x7f */
			retval = EcDoRpc_RopGetLocalReplicaIds(mem_ctx, emsmdbp_ctx,
                                                               &(mapi_request->mapi_req[i]),
                                                               &(mapi_response->mapi_repl[idx]),
                                                               mapi_response->handles, &size);
			break;
		case op_MAPI_SyncImportReadStateChanges: /* 0x80 */
			retval = EcDoRpc_RopSyncImportReadStateChanges(mem_ctx, emsmdbp_ctx,
								       &(mapi_request->mapi_req[i]),
								       &(mapi_response->mapi_repl[idx]),
								       mapi_response->handles, &size);
			break;
		case op_MAPI_ResetTable: /* 0x81 */
			retval = EcDoRpc_RopResetTable(mem_ctx, emsmdbp_ctx,
						       &(mapi_request->mapi_req[i]),
						       &(mapi_response->mapi_repl[idx]),
						       mapi_response->handles, &size);
			break;
		case op_MAPI_SyncGetTransferState: /* 0x82 */
			retval = EcDoRpc_RopSyncGetTransferState(mem_ctx, emsmdbp_ctx,
								 &(mapi_request->mapi_req[i]),
								 &(mapi_response->mapi_repl[idx]),
								 mapi_response->handles, &size);
			break;
		/* op_MAPI_OpenPublicFolderByName: 0x87 */
		/* op_MAPI_SetSyncNotificationGuid: 0x88 */
		/* op_MAPI_FreeBookmark: 0x89 */
		/* op_MAPI_WriteAndCommitStream: 0x90 */
		/* op_MAPI_HardDeleteMessages: 0x91 */
		/* op_MAPI_HardDeleteMessagesAndSubfolders: 0x92 */
		case op_MAPI_SetLocalReplicaMidsetDeleted: /* 0x93 */
			retval = EcDoRpc_RopSetLocalReplicaMidsetDeleted(mem_ctx, emsmdbp_ctx,
									 &(mapi_request->mapi_req[i]),
									 &(mapi_response->mapi_repl[idx]),
									 mapi_response->handles, &size);
			break;
		case op_MAPI_Logon: /* 0xfe */
			retval = EcDoRpc_RopLogon(mem_ctx, emsmdbp_ctx,
						  &(mapi_request->mapi_req[i]),
						  &(mapi_response->mapi_repl[idx]),
						  mapi_response->handles, &size);
			break;
		default:
			OC_DEBUG(1, "MAPI Rop: 0x%.2x not implemented!\n",
				  mapi_request->mapi_req[i].opnum);
		}

		if (mapi_request->mapi_req[i].opnum != op_MAPI_Release) {
			idx++;
		}

		if (retval) {
			OC_DEBUG(5, "MAPI Rop: 0x%.2x [retval=0x%.8x]\n", mapi_request->mapi_req[i].opnum, retval);
		}
	}

notif:
	/* Step 3. Notifications/Pending calls should be processed here */
	/* Note: GetProps and GetRows are filled with flag NDR_REMAINING, which may hide the content of the following replies. */

	if (mapi_response->mapi_repl) {
		mapi_response->mapi_repl[idx].opnum = 0;
	}
	
	/* Step 4. Fill mapi_response structure */
	handles_length = mapi_request->mapi_len - mapi_request->length;
	mapi_response->length = size + sizeof (mapi_response->length);
	mapi_response->mapi_len = mapi_response->length + handles_length;

	return mapi_response;
}

/**
   \details exchange_emsmdb EcDoRpc (0x2) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoRpc request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcDoRpc(struct dcesrv_call_state *dce_call,
				      TALLOC_CTX *mem_ctx,
				      struct EcDoRpc *r)
{
	struct exchange_emsmdb_session	*session;
	struct emsmdbp_context		*emsmdbp_ctx = NULL;
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;

	OC_DEBUG(3, "exchange_emsmdb: EcDoRpc (0x2)\n");

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate\n");
		r->out.handle->handle_type = 0;
		r->out.handle->uuid = GUID_zero();
		r->out.result = DCERPC_FAULT_CONTEXT_MISMATCH;
		return MAPI_E_LOGON_FAILED;
	}

	/* Retrieve the emsmdbp_context from the session management system */
        session = dcesrv_find_emsmdb_session(&r->in.handle->uuid);
        if (session) {
                emsmdbp_ctx = (struct emsmdbp_context *)session->session->private_data;
	}
	else {
		r->out.handle->handle_type = 0;
		r->out.handle->uuid = GUID_zero();
		r->out.result = DCERPC_FAULT_CONTEXT_MISMATCH;
		return MAPI_E_LOGON_FAILED;
	}

	/* Step 1. Process EcDoRpc requests */
	mapi_request = r->in.mapi_request;
	mapi_response = EcDoRpc_process_transaction(mem_ctx, emsmdbp_ctx, mapi_request, r->in.handle->uuid);

	/* Step 2. Fill EcDoRpc reply */
	r->out.handle = r->in.handle;
	r->out.size = r->in.size;
	r->out.offset = r->in.offset;
	r->out.mapi_response = mapi_response;
	r->out.length = talloc_zero(mem_ctx, uint16_t);
	*r->out.length = mapi_response->mapi_len;

	r->out.result = MAPI_E_SUCCESS;

	return MAPI_E_SUCCESS;
}


/**
   \details exchange_emsmdb EcGetMoreRpc (0x3) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcGetMoreRpc request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_EcGetMoreRpc(struct dcesrv_call_state *dce_call,
				TALLOC_CTX *mem_ctx,
				struct EcGetMoreRpc *r)
{
	OC_DEBUG(3, "exchange_emsmdb: EcGetMoreRpc (0x3) not implemented\n");
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcRRegisterPushNotification (0x4) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcRRegisterPushNotification request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcRRegisterPushNotification(struct dcesrv_call_state *dce_call,
							  TALLOC_CTX *mem_ctx,
							  struct EcRRegisterPushNotification *r)
{
	int				retval;
	struct exchange_emsmdb_session	*session;
	/* struct emsmdbp_context		*emsmdbp_ctx = NULL; */

	OC_DEBUG(3, "exchange_emsmdb: EcRRegisterPushNotification (0x4)\n");

	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate\n");
		r->out.handle->handle_type = 0;
		r->out.handle->uuid = GUID_zero();
		r->out.result = DCERPC_FAULT_CONTEXT_MISMATCH;
		return MAPI_E_LOGON_FAILED;
	}

	/* Retrieve the emsmdbp_context from the session management system */
	session = dcesrv_find_emsmdb_session(&r->in.handle->uuid);
	if (session) {
		/* emsmdbp_ctx = (struct emsmdbp_context *)session->session->private_data; */
	} else {
		r->out.handle->handle_type = 0;
		r->out.handle->uuid = GUID_zero();
		r->out.result = DCERPC_FAULT_CONTEXT_MISMATCH;
		return MAPI_E_LOGON_FAILED;
	}

	retval = MAPI_E_SUCCESS;

	if (retval == MAPI_E_SUCCESS) {
		r->out.handle = r->in.handle;
		/* FIXME: Create a notification object and return associated handle */
		*r->out.hNotification = 244;
	} 

	return MAPI_E_SUCCESS;
}


/**
   \details exchange_emsmdb EcRUnregisterPushNotification (0x5) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcRUnregisterPushNotification request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcRUnregisterPushNotification(struct dcesrv_call_state *dce_call,
							    TALLOC_CTX *mem_ctx,
							    struct EcRUnregisterPushNotification *r)
{
	OC_DEBUG(3, "exchange_emsmdb: EcRUnregisterPushNotification (0x5) not implemented\n");
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcDummyRpc (0x6) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDummyRpc request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcDummyRpc(struct dcesrv_call_state *dce_call,
					 TALLOC_CTX *mem_ctx,
					 struct EcDummyRpc *r)
{
	OC_DEBUG(3, "exchange_emsmdb: EcDummyRpc (0x6)\n");
	return MAPI_E_SUCCESS;
}


/**
   \details exchange_emsmdb EcRGetDCName (0x7) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcRGetDCName request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_EcRGetDCName(struct dcesrv_call_state *dce_call,
				TALLOC_CTX *mem_ctx,
				struct EcRGetDCName *r)
{
	OC_DEBUG(3, "exchange_emsmdb: EcRGetDCName (0x7) not implemented\n");
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcRNetGetDCName (0x8) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcRNetGetDCName request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_EcRNetGetDCName(struct dcesrv_call_state *dce_call,
				   TALLOC_CTX *mem_ctx,
				   struct EcRNetGetDCName *r)
{
	OC_DEBUG(3, "exchange_emsmdb: EcRNetGetDCName (0x8) not implemented\n");
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcDoRpcExt (0x9) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoRpcExt request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcDoRpcExt(struct dcesrv_call_state *dce_call,
					 TALLOC_CTX *mem_ctx,
					 struct EcDoRpcExt *r)
{
	OC_DEBUG(3, "exchange_emsmdb: EcDoRpcExt (0x9) not implemented\n");
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* check if a client version is too low to use */
/* TODO: this could be much more sophisticated */
static bool clientVersionIsTooLow(const uint16_t rgwClientVersion[3])
{
	if (rgwClientVersion[0] < 0x000B) {
		return true;
	} else {
		return false;
	}
}

/**
   \details exchange_emsmdb EcDoConnectEx (0xA) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoConnectEx request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcDoConnectEx(struct dcesrv_call_state *dce_call,
					    TALLOC_CTX *mem_ctx,
					    struct EcDoConnectEx *r)
{
	struct emsmdbp_context		*emsmdbp_ctx;
	struct dcesrv_handle		*handle;
	struct policy_handle		wire_handle;
	struct exchange_emsmdb_session	*session;
	struct ldb_message		*msg;
	const char			*mailNickname;
	const char			*userDN;
	char				*dnprefix;
	char				*tmp = "";

	OC_DEBUG(3, "exchange_emsmdb: EcDoConnectEx (0xA)\n");

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate\n");
	failure:
		wire_handle.handle_type = 0;
		wire_handle.uuid = GUID_zero();
		*r->out.handle = wire_handle;
		*r->out.pcmsPollsMax = 0;
		*r->out.pcRetry = 0;
		*r->out.pcmsRetryDelay = 0;
		*r->out.picxr = 0;

		r->out.szDNPrefix = (const char **)talloc_array(mem_ctx, char *, 2);
		r->out.szDNPrefix[0] = talloc_strdup(mem_ctx, tmp);

		r->out.szDisplayName = (const char **)talloc_array(mem_ctx, char *, 2);
		r->out.szDisplayName[0] = talloc_strdup(mem_ctx, tmp);

		r->out.rgwServerVersion[0] = 0;
		r->out.rgwServerVersion[1] = 0;
		r->out.rgwServerVersion[2] = 0;
		r->out.rgwBestVersion[0] = 0;
		r->out.rgwBestVersion[1] = 0;
		r->out.rgwBestVersion[2] = 0;
		*r->out.pulTimeStamp = 0;
		r->out.rgbAuxOut = NULL;
		*r->out.pcbAuxOut = 0;
		return r->out.result;
	}

	if (r->in.cbAuxIn < 0x8) {
	  OC_DEBUG(5, "r->in.cbAuxIn is > 0x0 and < 0x8\n");
		r->out.result = ecRpcFailed;
		goto failure;
	}

	if (!r->in.szUserDN || strlen(r->in.szUserDN) == 0) {
	  OC_DEBUG(5, "r->in.szUserDN is NULL or empty\n");
		r->out.result = MAPI_E_NO_ACCESS;
		goto failure;
	}

	/* Step 1. Initialize the emsmdbp context */
	emsmdbp_ctx = emsmdbp_init(dce_call->conn->dce_ctx->lp_ctx,
				   dcesrv_call_account_name(dce_call),
				   openchange_db_ctx);
	if (!emsmdbp_ctx) {
		OC_DEBUG(0, "FATAL: unable to initialize emsmdbp context");
		r->out.result = MAPI_E_LOGON_FAILED;
		goto failure;
	}

	/* Step 2. Check if incoming user belongs to the Exchange organization */
	if (emsmdbp_verify_user(dce_call, emsmdbp_ctx) == false) {
		talloc_free(emsmdbp_ctx);
		r->out.result = ecUnknownUser;
		goto failure;
	}

	/* Step 3. Check if input user DN belongs to the Exchange organization */
	if (emsmdbp_verify_userdn(dce_call, emsmdbp_ctx, r->in.szUserDN, &msg) == false) {
		talloc_free(emsmdbp_ctx);
		r->out.result = ecUnknownUser;
		goto failure;
	}

	emsmdbp_ctx->szUserDN = talloc_strdup(emsmdbp_ctx, r->in.szUserDN);
	emsmdbp_ctx->userLanguage = r->in.ulLcidString;

	/* Step 4. Retrieve the display name of the user */
	*r->out.szDisplayName = ldb_msg_find_attr_as_string(msg, "displayName", NULL);
	emsmdbp_ctx->szDisplayName = talloc_strdup(emsmdbp_ctx, *r->out.szDisplayName);

	/* Step 5. Retrieve the distinguished name of the server */
	mailNickname = ldb_msg_find_attr_as_string(msg, "mailNickname", NULL);
	userDN = ldb_msg_find_attr_as_string(msg, "legacyExchangeDN", NULL);
	dnprefix = strstr(userDN, mailNickname);
	if (!dnprefix) {
		talloc_free(emsmdbp_ctx);
		r->out.result = MAPI_E_LOGON_FAILED;
		goto failure;
	}

	*dnprefix = '\0';
	emsmdbp_ctx->szDNPrefix = talloc_strdup(emsmdbp_ctx, userDN);
	OPENCHANGE_RETVAL_IF(emsmdbp_ctx->szDNPrefix == NULL, MAPI_E_NOT_ENOUGH_RESOURCES, emsmdbp_ctx);
	*r->out.szDNPrefix = strupper_talloc(mem_ctx, userDN);
	OPENCHANGE_RETVAL_IF(*r->out.szDNPrefix == NULL, MAPI_E_NOT_ENOUGH_RESOURCES, emsmdbp_ctx);

	/* Step 6. Fill EcDoConnectEx reply */
	handle = dcesrv_handle_new(dce_call->context, EXCHANGE_HANDLE_EMSMDB);
	OPENCHANGE_RETVAL_IF(!handle, MAPI_E_NOT_ENOUGH_RESOURCES, emsmdbp_ctx);

	if (emsmdbp_set_session_uuid(emsmdbp_ctx, handle->wire_handle.uuid) == false) {
		talloc_free(emsmdbp_ctx);
		r->out.result = MAPI_E_LOGON_FAILED;
		goto failure;
	}

	handle->data = (void *) emsmdbp_ctx;
	*r->out.handle = handle->wire_handle;

	*r->out.pcmsPollsMax = EMSMDB_PCMSPOLLMAX;
	*r->out.pcRetry = EMSMDB_PCRETRY;
	*r->out.pcmsRetryDelay = EMSMDB_PCRETRYDELAY;
	*r->out.picxr = 0;

	r->out.rgwServerVersion[0] = 0x8;
	r->out.rgwServerVersion[1] = 0x82B4;
	r->out.rgwServerVersion[2] = 0x3;

	r->out.pulTimeStamp = talloc_zero(mem_ctx, uint32_t);
	*r->out.pulTimeStamp = time(NULL);

	*r->out.pcbAuxOut = 0;

	r->out.rgbAuxOut = NULL;

	r->out.result = MAPI_E_SUCCESS;

	if (clientVersionIsTooLow(r->in.rgwClientVersion)) {
		r->out.rgwBestVersion[0] = 0x000B;
		r->out.rgwBestVersion[1] = 0x8000;
		r->out.rgwBestVersion[2] = 0x0000;
		wire_handle.handle_type = 0;
		wire_handle.uuid = GUID_zero();
		*r->out.handle = wire_handle;
		r->out.result = MAPI_E_VERSION;
	} else {
		r->out.rgwBestVersion[0] = r->in.rgwClientVersion[0];
		r->out.rgwBestVersion[1] = r->in.rgwClientVersion[1];
		r->out.rgwBestVersion[2] = r->in.rgwClientVersion[2];

		r->out.result = MAPI_E_SUCCESS;
	}

	/* Search for an existing session and increment ref_count, otherwise create it */
	session = dcesrv_find_emsmdb_session(&handle->wire_handle.uuid);
	if (session) {
		OC_DEBUG(0, "[exchange_emsmdb]: Increment session ref count for %d\n",
				 session->session->context_id);
		mpm_session_increment_ref_count(session->session);
	}
	else {
		/* Step 7. Associate this emsmdbp context to the session */
		session = talloc((TALLOC_CTX *)emsmdb_session, struct exchange_emsmdb_session);
		OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_ENOUGH_RESOURCES, emsmdbp_ctx);

		session->pullTimeStamp = *r->out.pulTimeStamp;
		session->session = mpm_session_init((TALLOC_CTX *)emsmdb_session, dce_call);
		OPENCHANGE_RETVAL_IF(!session->session, MAPI_E_NOT_ENOUGH_RESOURCES, emsmdbp_ctx);
		
		session->uuid = handle->wire_handle.uuid;

		mpm_session_set_private_data(session->session, (void *) emsmdbp_ctx);
		mpm_session_set_destructor(session->session, emsmdbp_destructor);

		OC_DEBUG(0, "[exchange_emsmdb]: New session added: %d\n", session->session->context_id);

		DLIST_ADD_END(emsmdb_session, session, struct exchange_emsmdb_session *);
	}

	return MAPI_E_SUCCESS;
}

/**
   \details exchange_emsmdb EcDoRpcExt2 (0xB) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoRpcExt2 request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcDoRpcExt2(struct dcesrv_call_state *dce_call,
					  TALLOC_CTX *mem_ctx,
					  struct EcDoRpcExt2 *r)
{
	enum ndr_err_code		ndr_err;
	struct exchange_emsmdb_session	*session;
	struct emsmdbp_context		*emsmdbp_ctx = NULL;
	struct mapi2k7_request		mapi2k7_request;
	struct mapi_response		*mapi_response;
	struct RPC_HEADER_EXT		RPC_HEADER_EXT;
	struct ndr_pull			*ndr_pull = NULL;
	struct ndr_push			*ndr_uncomp_rgbOut;
	struct ndr_push			*ndr_comp_rgbOut;
	struct ndr_push			*ndr_rgbOut;
	uint32_t			pulFlags = 0x0;
	uint32_t			pulTransTime = 0;
	DATA_BLOB			rgbIn;

	OC_DEBUG(3, "exchange_emsmdb: EcDoRpcExt2 (0xB)\n");

	r->out.rgbOut = NULL;
	*r->out.pcbOut = 0;
	r->out.rgbAuxOut = NULL;
	*r->out.pcbAuxOut = 0;

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate\n");
		r->out.handle->handle_type = 0;
		r->out.handle->uuid = GUID_zero();
		r->out.result = DCERPC_FAULT_CONTEXT_MISMATCH;
		return MAPI_E_LOGON_FAILED;
	}

	/* Retrieve the emsmdbp_context from the session management system */
        session = dcesrv_find_emsmdb_session(&r->in.handle->uuid);
	if (!session) {
		r->out.handle->handle_type = 0;
		r->out.handle->uuid = GUID_zero();
		r->out.result = DCERPC_FAULT_CONTEXT_MISMATCH;
		return MAPI_E_LOGON_FAILED;
	}
	emsmdbp_ctx = (struct emsmdbp_context *)session->session->private_data;

	/* Sanity checks on pcbOut input parameter */
	if (*r->in.pcbOut < 0x00000008) {
		r->out.result = ecRpcFailed;
		return ecRpcFailed;
	}

	/* Extract mapi_request from rgbIn */
	rgbIn.data = r->in.rgbIn;
	rgbIn.length = r->in.cbIn;

	ndr_pull = ndr_pull_init_blob(&rgbIn, mem_ctx);
	if (ndr_pull->data_size > *r->in.pcbOut) {
		r->out.result = ecBufferTooSmall;
		talloc_free(ndr_pull);
		return ecBufferTooSmall;
	}

	ndr_set_flags(&ndr_pull->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REF_ALLOC);
	ndr_err = ndr_pull_mapi2k7_request(ndr_pull, NDR_SCALARS|NDR_BUFFERS, &mapi2k7_request);
	talloc_free(ndr_pull);

	if (ndr_err != NDR_ERR_SUCCESS) {
		r->out.result = ecRpcFormat;
		return ecRpcFormat;
	}

	mapi_response = EcDoRpc_process_transaction(mem_ctx, emsmdbp_ctx, mapi2k7_request.mapi_request, r->in.handle->uuid);
	talloc_free(mapi2k7_request.mapi_request);

	/* Fill EcDoRpcExt2 reply */
	r->out.handle = r->in.handle;
	*r->out.pulFlags = pulFlags;

	/* Push MAPI response into a DATA blob */
	ndr_uncomp_rgbOut = ndr_push_init_ctx(mem_ctx);
	ndr_set_flags(&ndr_uncomp_rgbOut->flags, LIBNDR_FLAG_NOALIGN);
	ndr_push_mapi_response(ndr_uncomp_rgbOut, NDR_SCALARS|NDR_BUFFERS, mapi_response);
	talloc_free(mapi_response);

	/* TODO: compress if requested */
	ndr_comp_rgbOut = ndr_uncomp_rgbOut;

	/* Build RPC_HEADER_EXT header for MAPI response DATA blob */
	RPC_HEADER_EXT.Version = 0x0000;
	RPC_HEADER_EXT.Flags = RHEF_Last;
	RPC_HEADER_EXT.Flags |= (mapi2k7_request.header.Flags & RHEF_XorMagic);
	RPC_HEADER_EXT.Size = ndr_comp_rgbOut->offset;
	RPC_HEADER_EXT.SizeActual = ndr_comp_rgbOut->offset;

	/* Obfuscate content if applicable*/
	if (RPC_HEADER_EXT.Flags & RHEF_XorMagic) {
		obfuscate_data(ndr_comp_rgbOut->data, ndr_comp_rgbOut->offset, 0xA5);
	}

	/* Push the constructed blob */
	ndr_rgbOut = ndr_push_init_ctx(mem_ctx);
	ndr_set_flags(&ndr_rgbOut->flags, LIBNDR_FLAG_NOALIGN);
	ndr_push_RPC_HEADER_EXT(ndr_rgbOut, NDR_SCALARS|NDR_BUFFERS, &RPC_HEADER_EXT);
	ndr_push_bytes(ndr_rgbOut, ndr_comp_rgbOut->data, ndr_comp_rgbOut->offset);

	/* Push MAPI response into a DATA blob */
	r->out.rgbOut = ndr_rgbOut->data;
	*r->out.pcbOut = ndr_rgbOut->offset;

	*r->out.pulTransTime = pulTransTime;

	return MAPI_E_SUCCESS;
}

/**
   \details exchange_emsmdb EcUnknown0xC (0xc) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcUnknown0xC request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_EcUnknown0xC(struct dcesrv_call_state *dce_call,
				      TALLOC_CTX *mem_ctx,
				      struct EcUnknown0xC *r)
{
	OC_DEBUG(3, "exchange_emsmdb: EcUnknown0xC (0xc) not implemented\n");
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcUnknown0xD (0xc) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcUnknown0xD request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_EcUnknown0xD(struct dcesrv_call_state *dce_call,
				      TALLOC_CTX *mem_ctx,
				      struct EcUnknown0xD *r)
{
	OC_DEBUG(3, "exchange_emsmdb: EcUnknown0xC (0xd) not implemented\n");
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcGetMoreRpc (0xe) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoAsyncConnectExt request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcDoAsyncConnectEx(struct dcesrv_call_state *dce_call,
						 TALLOC_CTX *mem_ctx,
						 struct EcDoAsyncConnectEx *r)
{
	struct exchange_emsmdb_session	*session;
	enum mapistore_error		retval;
	struct emsmdbp_context		*emsmdbp_ctx;
	struct dcesrv_handle		*handle;

	OC_DEBUG(3, "exchange_emsmdb: EcDoAsyncConnectEx (0xe)\n");

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate");
		r->out.async_handle->handle_type = 0;
		r->out.async_handle->uuid = GUID_zero();
		r->out.result = DCERPC_FAULT_CONTEXT_MISMATCH;
		return MAPI_E_LOGON_FAILED;
	}

	/* Step 1. Retrieve the existing session */
	session = dcesrv_find_emsmdb_session(&r->in.handle->uuid);
	if (session) {
		emsmdbp_ctx = (struct emsmdbp_context *) session->session->private_data;
	} else {
		OC_DEBUG(0, "[EcDoAsyncConnectEx]: emsmdb session not found");
		r->out.async_handle->handle_type = 0;
		r->out.async_handle->uuid = GUID_zero();
		r->out.result = DCERPC_FAULT_CONTEXT_MISMATCH;
		return MAPI_E_LOGON_FAILED;
	}

	/* Step 2. Create the new session for async pipe */
	handle = dcesrv_handle_new(dce_call->context, EXCHANGE_HANDLE_ASYNCEMSMDB);
	if (!handle) {
		OC_DEBUG(0, "[EcDoAsyncConnectEx] Unable to create new handle");
		r->out.async_handle->handle_type = 0;
		r->out.async_handle->uuid = GUID_zero();
		r->out.result = DCERPC_FAULT_CONTEXT_MISMATCH;
		return MAPI_E_LOGON_FAILED;
	}

	/* Step 3. Register the global session */
	retval = mapistore_notification_session_add(emsmdbp_ctx->mstore_ctx, r->in.handle->uuid,
						    handle->wire_handle.uuid, dcesrv_call_account_name(dce_call));
	if (retval != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "[EcDoAsyncConnectEx]: session registration failed with '%s'\n",
			 mapistore_errstr(retval));
		r->out.async_handle->handle_type = 0;
		r->out.async_handle->uuid = GUID_zero();
		r->out.result = DCERPC_FAULT_CONTEXT_MISMATCH;
		return MAPI_E_LOGON_FAILED;
	}

	*r->out.async_handle = handle->wire_handle;
	r->out.result = MAPI_E_SUCCESS;

	return MAPI_E_SUCCESS;
}


/**
   \details Dispatch incoming EMSMDB call to the correct OpenChange
   server function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r generic pointer on EMSMDB data
   \param mapiproxy pointer to the mapiproxy structure controlling
   mapiproxy behavior

   \return NT_STATUS_OK;
 */
static NTSTATUS dcesrv_exchange_emsmdb_dispatch(struct dcesrv_call_state *dce_call,
						TALLOC_CTX *mem_ctx,
						void *r, struct mapiproxy *mapiproxy)
{
	const struct ndr_interface_table	*table;
	uint16_t				opnum;

	table = (const struct ndr_interface_table *) dce_call->context->iface->private_data;
	opnum = dce_call->pkt.u.request.opnum;

	/* Sanity checks */
	if (!table) return NT_STATUS_UNSUCCESSFUL;
	if (table->name && strcmp(table->name, NDR_EXCHANGE_EMSMDB_NAME)) return NT_STATUS_UNSUCCESSFUL;

	switch (opnum) {
	case NDR_ECDOCONNECT:
		dcesrv_EcDoConnect(dce_call, mem_ctx, (struct EcDoConnect *)r);
		break;
	case NDR_ECDODISCONNECT:
		dcesrv_EcDoDisconnect(dce_call, mem_ctx, (struct EcDoDisconnect *)r);
		break;
	case NDR_ECDORPC:
		dcesrv_EcDoRpc(dce_call, mem_ctx, (struct EcDoRpc *)r);
		break;
	case NDR_ECGETMORERPC:
		dcesrv_EcGetMoreRpc(dce_call, mem_ctx, (struct EcGetMoreRpc *)r);
		break;
	case NDR_ECRREGISTERPUSHNOTIFICATION:
		dcesrv_EcRRegisterPushNotification(dce_call, mem_ctx, (struct EcRRegisterPushNotification *)r);
		break;
	case NDR_ECRUNREGISTERPUSHNOTIFICATION:
		dcesrv_EcRUnregisterPushNotification(dce_call, mem_ctx, (struct EcRUnregisterPushNotification *)r);
		break;
	case NDR_ECDUMMYRPC:
		dcesrv_EcDummyRpc(dce_call, mem_ctx, (struct EcDummyRpc *)r);
		break;
	case NDR_ECRGETDCNAME:
		dcesrv_EcRGetDCName(dce_call, mem_ctx, (struct EcRGetDCName *)r);
		break;
	case NDR_ECRNETGETDCNAME:
		dcesrv_EcRNetGetDCName(dce_call, mem_ctx, (struct EcRNetGetDCName *)r);
		break;
	case NDR_ECDORPCEXT:
		dcesrv_EcDoRpcExt(dce_call, mem_ctx, (struct EcDoRpcExt *)r);
		break;
	case NDR_ECDOCONNECTEX:
		dcesrv_EcDoConnectEx(dce_call, mem_ctx, (struct EcDoConnectEx *)r);
		break;
	case NDR_ECDORPCEXT2:
		dcesrv_EcDoRpcExt2(dce_call, mem_ctx, (struct EcDoRpcExt2 *)r);
		break;
	case NDR_ECUNKNOWN0XC:
		dcesrv_EcUnknown0xC(dce_call, mem_ctx, (struct EcUnknown0xC *)r);
		break;
	case NDR_ECUNKNOWN0XD:
		dcesrv_EcUnknown0xD(dce_call, mem_ctx, (struct EcUnknown0xD *)r);
		break;
	case NDR_ECDOASYNCCONNECTEX:
		dcesrv_EcDoAsyncConnectEx(dce_call, mem_ctx, (struct EcDoAsyncConnectEx *)r);
		break;
	}

	return NT_STATUS_OK;
}


/**
   \details Initialize the EMSMDB OpenChange server

   \param dce_ctx pointer to the server context

   \return NT_STATUS_OK on success
 */
static NTSTATUS dcesrv_exchange_emsmdb_init(struct dcesrv_context *dce_ctx)
{
	/* Initialize exchange_emsmdb session */
	emsmdb_session = talloc_zero(dce_ctx, struct exchange_emsmdb_session);
	if (!emsmdb_session) return NT_STATUS_NO_MEMORY;
	emsmdb_session->session = NULL;

	/* Open read/write context on OpenChange dispatcher database */
	openchange_db_ctx = emsmdbp_openchangedb_init(dce_ctx->lp_ctx);
	if (!openchange_db_ctx) {
		OC_PANIC(false, ("[exchange_emsmdb] Unable to initialize openchangedb\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	return NT_STATUS_OK;
}


/**
   \details Terminate the EMSMDB connection and release the associated
   session and context if still available. This case occurs when the
   client doesn't call EcDoDisconnect but quit unexpectedly.

   \param server_id reference to the server identifier structure
   \param context_id the connection context identifier

   \return NT_STATUS_OK on success
 */

/* FIXME: code temporarily disabled as we don't master the logic behind session handles yet... */
static NTSTATUS dcesrv_exchange_emsmdb_unbind(struct server_id server_id, uint32_t context_id)
{
	/* struct exchange_emsmdb_session	*session; */
	/* bool ret; */

	OC_DEBUG(0, "dcesrv_exchange_emsmdb_unbind: server_id=%d, context_id=0x%x", server_id, context_id);

	/* session = dcesrv_find_emsmdb_session_by_server_id(&server_id, context_id); */
	/* if (session) { */
	/* 	ret = mpm_session_release(session->session); */
	/* 	if (ret == true) { */
	/* 		DLIST_REMOVE(emsmdb_session, session); */
	/* 		OC_DEBUG(5, ("[%s:%d]: Session found and released\n",  */
	/* 			  __FUNCTION__, __LINE__)); */
	/* 	} else { */
	/* 		OC_DEBUG(5, ("[%s:%d]: Session found and ref_count decreased\n", */
	/* 			  __FUNCTION__, __LINE__)); */
	/* 	} */
	/* } */

	return NT_STATUS_OK;
}


/**
   \details Entry point for the default OpenChange EMSMDB server

   \return NT_STATUS_OK on success, otherwise NTSTATUS error
 */
NTSTATUS samba_init_module(void)
{
	struct mapiproxy_module	server;
	NTSTATUS		ret;

	/* Fill in our name */
	server.name = "exchange_emsmdb";
	server.status = MAPIPROXY_DEFAULT;
	server.description = "OpenChange EMSMDB server";
	server.endpoint = "exchange_emsmdb";

	/* Fill in all the operations */
	server.init = dcesrv_exchange_emsmdb_init;
	server.unbind = dcesrv_exchange_emsmdb_unbind;
	server.dispatch = dcesrv_exchange_emsmdb_dispatch;
	server.push = NULL;
	server.pull = NULL;
	server.ndr_pull = NULL;

	/* Register ourselves with the MAPIPROXY server subsystem */
	ret = mapiproxy_server_register(&server);
	if (!NT_STATUS_IS_OK(ret)) {
		OC_DEBUG(0, "Failed to register the 'exchange_emsmdb' default mapiproxy server!\n");
		return ret;
	}

	return ret;
}
