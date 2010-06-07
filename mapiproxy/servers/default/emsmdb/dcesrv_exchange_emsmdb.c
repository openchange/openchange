/*
   MAPI Proxy - Exchange EMSMDB Server

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009

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

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "dcesrv_exchange_emsmdb.h"

struct exchange_emsmdb_session		*emsmdb_session = NULL;
void					*openchange_ldb_ctx = NULL;

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
	const char			*cn;
	const char			*userDN;
	char				*dnprefix;
	bool				found = false;

	DEBUG(3, ("exchange_emsmdb: EcDoConnect (0x0)\n"));

	/* Step 0. Ensure incoming use is authenticated */
	if (!NTLM_AUTH_IS_OK(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
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
		r->out.szDisplayName = NULL;
		r->out.szDNPrefix = NULL;
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
				   dce_call->context->conn->auth_state.session_info->server_info->account_name,
				   openchange_ldb_ctx);
	if (!emsmdbp_ctx) {
		smb_panic("unable to initialize emsmdbp context");
		OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_FAILONEPROVIDER, NULL);
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
	r->out.szDisplayName = ldb_msg_find_attr_as_string(msg, "displayName", NULL);
	emsmdbp_ctx->szDisplayName = talloc_strdup(emsmdbp_ctx, r->out.szDisplayName);

	/* Step 5. Retrieve the dinstinguished name of the server */
	cn = ldb_msg_find_attr_as_string(msg, "cn", NULL);
	userDN = ldb_msg_find_attr_as_string(msg, "legacyExchangeDN", NULL);
	dnprefix = strstr(userDN, cn);
	if (!dnprefix) {
		talloc_free(emsmdbp_ctx);
		goto failure;
	}

	*dnprefix = '\0';
	r->out.szDNPrefix = strupper_talloc(mem_ctx, userDN);

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

	r->out.rgwServerVersion[0] = 0x6;
	r->out.rgwServerVersion[1] = 0x1141;
	r->out.rgwServerVersion[2] = 0x5;

	r->out.rgwClientVersion[0] = r->in.rgwClientVersion[0];
	r->out.rgwClientVersion[1] = r->in.rgwClientVersion[1];
	r->out.rgwClientVersion[2] = r->in.rgwClientVersion[2];

	r->out.pullTimeStamp = talloc_zero(mem_ctx, uint32_t);
	*r->out.pullTimeStamp = time(NULL);

	r->out.result = MAPI_E_SUCCESS;

	/* Search for an existing session and increment ref_count, otherwise create it */
	for (session = emsmdb_session; session; session = session->next) {
		if ((mpm_session_cmp(session->session, dce_call) == true)) {
			DEBUG(0, ("[exchange_emsmdb]: Increment session ref count for %d\n", 
				  session->session->context_id));
			mpm_session_increment_ref_count(session->session);
			found = true;
			break;
		}
	}

	if (found == false) {
		/* Step 7. Associate this emsmdbp context to the session */
		session = talloc((TALLOC_CTX *)emsmdb_session, struct exchange_emsmdb_session);
		OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_ENOUGH_RESOURCES, emsmdbp_ctx);

		session->pullTimeStamp = *r->out.pullTimeStamp;
		session->session = mpm_session_init((TALLOC_CTX *)emsmdb_session, dce_call);
		OPENCHANGE_RETVAL_IF(!session->session, MAPI_E_NOT_ENOUGH_RESOURCES, emsmdbp_ctx);

		mpm_session_set_private_data(session->session, (void *) emsmdbp_ctx);
		mpm_session_set_destructor(session->session, emsmdbp_destructor);

		DEBUG(0, ("[exchange_emsmdb]: New session added: %d\n", session->session->context_id));

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

	DEBUG(3, ("exchange_emsmdb: EcDoDisconnect (0x1)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!NTLM_AUTH_IS_OK(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		return MAPI_E_LOGON_FAILED;
	}

	/* Step 1. Retrieve handle and free if emsmdbp context and session are available */
	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	if (h) {
		for (session = emsmdb_session; session; session = session->next) {
			if ((mpm_session_cmp(session->session, dce_call) == true)) {
				ret = mpm_session_release(session->session);
				if (ret == true) {
					DLIST_REMOVE(emsmdb_session, session);
					DEBUG(5, ("[%s:%d]: Session found and released\n", 
						  __FUNCTION__, __LINE__));
				} else {
					DEBUG(5, ("[%s:%d]: Session found and ref_count decreased\n",
						  __FUNCTION__, __LINE__));
				}
				break;
			}
		}
	}

	r->out.result = MAPI_E_SUCCESS;

	return MAPI_E_SUCCESS;
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
	enum MAPISTATUS			retval;
	uint32_t			handles_length;
	uint16_t			size = 0;
	uint32_t			i;
	uint32_t			idx;
	bool				found = false;

	DEBUG(3, ("exchange_emsmdb: EcDoRpc (0x2)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!NTLM_AUTH_IS_OK(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		return MAPI_E_LOGON_FAILED;
	}

	/* Retrieve the emsmdbp_context from the session management system */
	for (session = emsmdb_session; session; session = session->next) {
		if ((mpm_session_cmp(session->session, dce_call)) == true) {
			emsmdbp_ctx = (struct emsmdbp_context *)session->session->private_data;
			found = true;
		}
	}
	OPENCHANGE_RETVAL_IF(found == false, MAPI_E_LOGON_FAILED, NULL);

	mapi_request = r->in.mapi_request;
	mapi_response = talloc_zero(mem_ctx, struct mapi_response);
	mapi_response->handles = mapi_request->handles;

	/* Step 1. Idle requests case */
	if (mapi_request->mapi_len <= 2) {
		mapi_response->mapi_len = 2;
		goto end;
	}

	/* Step 2. Process serialized MAPI requests */
	mapi_response->mapi_repl = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REPL);
	for (i = 0, idx = 0, size = 0; mapi_request->mapi_req[i].opnum != 0; i++) {
		DEBUG(0, ("MAPI Rop: 0x%.2x (%d)\n", mapi_request->mapi_req[i].opnum, size));

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
		/* op_MAPI_GetPropsAll: 0x8 */
		/* op_MAPI_GetPropList: 0x9 */
		case op_MAPI_SetProps: /* 0x09 */
			retval = EcDoRpc_RopSetProperties(mem_ctx, emsmdbp_ctx,
							  &(mapi_request->mapi_req[i]),
							  &(mapi_response->mapi_repl[idx]),
							  mapi_response->handles, &size);
			break;
		/* op_MAPI_DeleteProps: 0xb */
		case op_MAPI_SaveChangesMessage: /* 0x0c */
			retval = EcDoRpc_RopSaveChangesMessage(mem_ctx, emsmdbp_ctx,
							  &(mapi_request->mapi_req[i]),
							  &(mapi_response->mapi_repl[idx]),
							  mapi_response->handles, &size);
			break;
		/* op_MAPI_SaveChangesMessage: 0xc */
		/* op_MAPI_RemoveAllRecipients: 0xd */
		case op_MAPI_ModifyRecipients: /* 0xe */
			retval = EcDoRpc_RopModifyRecipients(mem_ctx, emsmdbp_ctx,
							     &(mapi_request->mapi_req[i]),
							     &(mapi_response->mapi_repl[idx]),
							     mapi_response->handles, &size);
			break;

		/* op_MAPI_ReadRecipients: 0xf */
		/* op_MAPI_ReloadCachedInformation: 0x10 */
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
			retval = EcDoRpc_RopDeleteMessage(mem_ctx, emsmdbp_ctx,
							  &(mapi_request->mapi_req[i]),
							  &(mapi_response->mapi_repl[idx]),
							  mapi_response->handles, &size);
			break;
		/* op_MAPI_GetMessageStatus: 0x1f */
		/* op_MAPI_SetMessageStatus: 0x20 */
		case op_MAPI_GetAttachmentTable: /* 0x21 */
			retval = EcDoRpc_RopGetAttachmentTable(mem_ctx, emsmdbp_ctx,
							       &(mapi_request->mapi_req[i]),
							       &(mapi_response->mapi_repl[idx]),
							       mapi_response->handles, &size);
			break;
		/* op_MAPI_OpenAttach: 0x22 */
		/* op_MAPI_CreateAttach: 0x23 */
		/* op_MAPI_DeleteAttach: 0x24 */
		/* op_MAPI_SaveChangesAttachment: 0x25 */
		/* op_MAPI_SetReceiveFolder: 0x26 */
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
		/* op_MAPI_ReadStream: 0x2c */
		/* op_MAPI_WriteStream: 0x2d */
		/* op_MAPI_SeekStream: 0x2e */
		/* op_MAPI_SetStreamSize: 0x2f */
		/* op_MAPI_SetSearchCriteria: 0x30 */
		/* op_MAPI_GetSearchCriteria: 0x31 */
		case op_MAPI_SubmitMessage: /* 0x32 */
			retval = EcDoRpc_RopSubmitMessage(mem_ctx, emsmdbp_ctx,
							  &(mapi_request->mapi_req[i]),
							  &(mapi_response->mapi_repl[idx]),
							  mapi_response->handles, &size);
			break;
		/* op_MAPI_MoveCopyMessages: 0x33 */
		/* op_MAPI_AbortSubmit: 0x34 */
		/* op_MAPI_MoveFolder: 0x35 */
		/* op_MAPI_CopyFolder: 0x36 */
		/* op_MAPI_QueryColumnsAll: 0x37 */
		/* op_MAPI_Abort: 0x38 */
		/* op_MAPI_CopyTo: 0x39 */
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
		/* op_MAPI_ModifyPermissions: 0x40 */
		/* op_MAPI_ModifyRules: 0x41 */
		/* op_MAPI_GetOwningServers: 0x42 */
		/* op_MAPI_LongTermIdFromId: 0x43 */
		/* op_MAPI_IdFromLongTermId: 0x44 */
		/* op_MAPI_PublicFolderIsGhosted: 0x45 */
		/* op_MAPI_OpenEmbeddedMessage: 0x46 */
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
		/* op_MAPI_TransportSend: 0x4a */
		/* op_MAPI_FastTransferSourceGetBuffer: 0x4e */
		case op_MAPI_FindRow: /* 0x4f */
			retval = EcDoRpc_RopFindRow(mem_ctx, emsmdbp_ctx, 
						    &(mapi_request->mapi_req[i]),
						    &(mapi_response->mapi_repl[idx]),
						    mapi_response->handles, &size);
			break;
		/* op_MAPI_Progress: 0x50 */
		/* op_MAPI_TransportNewMail: 0x51 */
		/* op_MAPI_GetValidAttachments: 0x52 */
		/* op_MAPI_GetNamesFromIDs: 0x55 */
		case op_MAPI_GetIDsFromNames: /* 0x56 */
			retval = EcDoRpc_RopGetPropertyIdsFromNames(mem_ctx, emsmdbp_ctx,
								    &(mapi_request->mapi_req[i]),
								    &(mapi_response->mapi_repl[idx]),
								    mapi_response->handles, &size);
			break;
		/* op_MAPI_UpdateDeferredActionMessages: 0x57 */ 
		/* op_MAPI_EmptyFolder: 0x58 */
		/* op_MAPI_ExpandRow: 0x59 */
		/* op_MAPI_CollapseRow: 0x5a */
		/* op_MAPI_LockRegionStream: 0x5b */
		/* op_MAPI_UnlockRegionStream: 0x5c */
		/* op_MAPI_CommitStream: 0x5d */
		/* op_MAPI_GetStreamSize: 0x5e */
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
		/* op_MAPI_ReadPerUserInformation: 0x63 */
		/* op_MAPI_SetReadFlags: 0x66 */
		/* op_MAPI_CopyProperties: 0x67 */
		/* op_MAPI_GetReceiveFolderTable: 0x68 */
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
		/* op_MAPI_SyncConfigure: 0x70 */
		/* op_MAPI_SyncImportMessageChange: 0x72 */
		/* op_MAPI_SyncImportHierarchyChange: 0x73 */
		/* op_MAPI_SyncImportDeletes: 0x74 */
		/* op_MAPI_SyncUploadStateStreamBegin: 0x75 */
		/* op_MAPI_SyncUploadStateStreamContinue: 0x76 */
		/* op_MAPI_SyncUploadStateStreamEnd: 0x77 */
		/* op_MAPI_SyncImportMessageMove: 0x78 */
		/* op_MAPI_SetPropertiesNoReplicate: 0x79 */
		/* op_MAPI_DeletePropertiesNoReplicate: 0x7a */
		/* op_MAPI_GetStoreState: 0x7b */
		/* op_MAPI_SyncOpenCollector: 0x7e */
		/* op_MAPI_GetLocalReplicaIds: 0x7f */
		/* op_MAPI_SyncImportReadStateChanges: 0x80 */
		/* op_MAPI_ResetTable: 0x81 */
		/* op_MAPI_SyncGetTransferState: 0x82 */
		/* op_MAPI_OpenPublicFolderByName: 0x87 */
		/* op_MAPI_SetSyncNotificationGuid: 0x88 */
		/* op_MAPI_FreeBookmark: 0x89 */
		/* op_MAPI_WriteAndCommitStream: 0x90 */
		/* op_MAPI_HardDeleteMessages: 0x91 */
		/* op_MAPI_HardDeleteMessagesAndSubfolders: 0x92 */
		case op_MAPI_Logon: /* 0xfe */
			retval = EcDoRpc_RopLogon(mem_ctx, emsmdbp_ctx,
						  &(mapi_request->mapi_req[i]),
						  &(mapi_response->mapi_repl[idx]),
						  mapi_response->handles, &size);
			break;
		default:
			DEBUG(1, ("MAPI Rop: 0x%.2x not implemented!\n",
				  mapi_request->mapi_req[i].opnum));
		}

		if (mapi_request->mapi_req[i].opnum != op_MAPI_Release) {
			idx++;
		}

	}

	/* Step 3. Notifications/Pending calls should be processed here */
	mapi_response->mapi_repl[idx].opnum = 0;

	/* Step 4. Fill mapi_response structure */
	handles_length = mapi_request->mapi_len - mapi_request->length;
	mapi_response->length = size + sizeof (mapi_response->length);
	mapi_response->mapi_len = mapi_response->length + handles_length;

	/* Step 5. Fill EcDoRpc reply */
end:
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
	DEBUG(3, ("exchange_emsmdb: EcGetMoreRpc (0x3) not implemented\n"));
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
	DEBUG(3, ("exchange_emsmdb: EcRRegisterPushNotification (0x4) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
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
	DEBUG(3, ("exchange_emsmdb: EcRUnregisterPushNotification (0x5) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcDummyRpc (0x6) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDummyRpc request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_EcDummyRpc(struct dcesrv_call_state *dce_call,
			      TALLOC_CTX *mem_ctx,
			      struct EcDummyRpc *r)
{
	DEBUG(3, ("exchange_emsmdb: EcDummyRpc (0x6) not implemented\n"));
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
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
	DEBUG(3, ("exchange_emsmdb: EcRGetDCName (0x7) not implemented\n"));
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
	DEBUG(3, ("exchange_emsmdb: EcRNetGetDCName (0x8) not implemented\n"));
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcDoRpcExt (0x9) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoRpcExt request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_EcDoRpcExt(struct dcesrv_call_state *dce_call,
			      TALLOC_CTX *mem_ctx,
			      struct EcDoRpcExt *r)
{
	DEBUG(3, ("exchange_emsmdb: EcDoRpcExt (0x9) not implemented\n"));
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
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
	struct policy_handle		wire_handle;

	DEBUG(3, ("exchange_emsmdb: EcDoConnectEx (0xA) not implemented\n"));

	wire_handle.handle_type = EXCHANGE_HANDLE_EMSMDB;
	wire_handle.uuid = GUID_zero();
	*r->out.handle = wire_handle;

	r->out.pcmsPollsMax = talloc_zero(mem_ctx, uint32_t);
	r->out.pcRetry = talloc_zero(mem_ctx, uint32_t);
	r->out.pcmsRetryDelay = talloc_zero(mem_ctx, uint32_t);
	r->out.picxr = talloc_zero(mem_ctx, uint32_t);
	r->out.pulTimeStamp = talloc_zero(mem_ctx, uint32_t);
	r->out.pcbAuxOut = talloc_zero(mem_ctx, uint32_t);

	*r->out.pcmsPollsMax = 0;
	*r->out.pcRetry = 0;
	*r->out.pcmsRetryDelay = 0;
	*r->out.picxr = 0;
	r->out.szDNPrefix = NULL;
	r->out.szDisplayName = NULL;
	r->out.rgwServerVersion[0] = 0;
	r->out.rgwServerVersion[1] = 0;
	r->out.rgwServerVersion[2] = 0;
	r->out.rgwBestVersion[0] = 0;
	r->out.rgwBestVersion[1] = 0;
	r->out.rgwBestVersion[2] = 0;
	*r->out.pulTimeStamp = 0;
	r->out.rgbAuxOut = NULL;
	*r->out.pcbAuxOut = 0;

	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
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
	enum MAPISTATUS				retval;
	const struct ndr_interface_table	*table;
	uint16_t				opnum;

	table = (const struct ndr_interface_table *) dce_call->context->iface->private_data;
	opnum = dce_call->pkt.u.request.opnum;

	/* Sanity checks */
	if (!table) return NT_STATUS_UNSUCCESSFUL;
	if (table->name && strcmp(table->name, NDR_EXCHANGE_EMSMDB_NAME)) return NT_STATUS_UNSUCCESSFUL;

	switch (opnum) {
	case NDR_ECDOCONNECT:
		retval = dcesrv_EcDoConnect(dce_call, mem_ctx, (struct EcDoConnect *)r);
		break;
	case NDR_ECDODISCONNECT:
		retval = dcesrv_EcDoDisconnect(dce_call, mem_ctx, (struct EcDoDisconnect *)r);
		break;
	case NDR_ECDORPC:
		retval = dcesrv_EcDoRpc(dce_call, mem_ctx, (struct EcDoRpc *)r);
		break;
	case NDR_ECGETMORERPC:
		dcesrv_EcGetMoreRpc(dce_call, mem_ctx, (struct EcGetMoreRpc *)r);
		break;
	case NDR_ECRREGISTERPUSHNOTIFICATION:
		retval = dcesrv_EcRRegisterPushNotification(dce_call, mem_ctx, (struct EcRRegisterPushNotification *)r);
		break;
	case NDR_ECRUNREGISTERPUSHNOTIFICATION:
		retval = dcesrv_EcRUnregisterPushNotification(dce_call, mem_ctx, (struct EcRUnregisterPushNotification *)r);
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
		retval = dcesrv_EcDoConnectEx(dce_call, mem_ctx, (struct EcDoConnectEx *)r);
		return NT_STATUS_NET_WRITE_FAULT;
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
	openchange_ldb_ctx = emsmdbp_openchange_ldb_init(dce_ctx->lp_ctx);
	if (!openchange_ldb_ctx) {
		smb_panic("unable to initialize 'openchange.ldb' context");
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
static NTSTATUS dcesrv_exchange_emsmdb_unbind(struct server_id server_id, uint32_t context_id)
{
	struct exchange_emsmdb_session	*session;

	for (session = emsmdb_session; session; session = session->next) {
		if ((mpm_session_cmp_sub(session->session, server_id, context_id) == true)) {
			mpm_session_release(session->session);
			DLIST_REMOVE(emsmdb_session, session);
			DEBUG(6, ("[%s:%d]: Session found and released\n", __FUNCTION__, __LINE__));
			return NT_STATUS_OK;
		}
	}

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
		DEBUG(0, ("Failed to register the 'exchange_emsmdb' default mapiproxy server!\n"));
		return ret;
	}

	return ret;
}
