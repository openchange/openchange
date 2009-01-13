/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2008.

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

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <gen_ndr/ndr_exchange.h>


/**
   \file IMAPISession.c

   \brief Session initialization options
 */


/**
   \details Open the Public Folder store

   This function opens the public folder store. This allows access to
   the public folders.

   \param obj_store the result of opening the store
   \param session pointer to the MAPI session context

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa MAPIInitialize which is required before opening the store
   \sa GetLastError to check the result of a failed call, if necessary
   \sa OpenMsgStore if you need access to the message store folders
*/
_PUBLIC_ enum MAPISTATUS OpenPublicFolder(struct mapi_session *session,
					  mapi_object_t *obj_store)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct Logon_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_object_store_t	*store;

	/* Sanity checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!session->profile, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = talloc_init("OpenPublicFolder");
	size = 0;

	/* Fill the Logon operation */
 	request.LogonFlags = 0;
	size += sizeof (uint8_t);
	request.OpenFlags = PUBLIC;
	size += sizeof (uint32_t);
	request.StoreState = 0;
	size += sizeof (uint32_t);
	request.EssDN = NULL;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_Logon;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_Logon = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) + 2;
	mapi_request->length = size + 2;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = 0xffffffff;

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* retrieve object session and handle */
	mapi_object_set_session(obj_store, session);
	mapi_object_set_handle(obj_store, mapi_response->handles[0]);

	/* retrieve store content */
	obj_store->private_data = talloc((TALLOC_CTX *)session, mapi_object_store_t);
	store = (mapi_object_store_t*)obj_store->private_data;
	MAPI_RETVAL_IF(!obj_store->private_data, MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	store->fid_pf_public_root = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.FolderIds[0];
	store->fid_pf_ipm_subtree = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.FolderIds[1];
	store->fid_pf_non_ipm_subtree = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.FolderIds[2];
	store->fid_pf_EFormsRegistryRoot = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.FolderIds[3];
	store->fid_pf_FreeBusyRoot = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.FolderIds[4];
	store->fid_pf_OfflineAB = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.FolderIds[5];
	store->fid_pf_EFormsRegistry = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.FolderIds[6];
	store->fid_pf_LocalSiteFreeBusy = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.FolderIds[7];
	store->fid_pf_LocalSiteOfflineAB = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.FolderIds[8];
	store->fid_pf_NNTPArticle = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.FolderIds[9];
	store->cached_mailbox_fid = false;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Open the Message Store

   This function opens the main message store. This allows access to
   the normal user folders.

   \param session pointer to the MAPI session context
   \param obj_store the result of opening the store

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa MAPIInitialize which is required before opening the store
   \sa GetLastError to check the result of a failed call, if necessary
   \sa OpenPublicFolder if you need access to the public folders
*/
_PUBLIC_ enum MAPISTATUS OpenMsgStore(struct mapi_session *session,
				      mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;

	/* sanity checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!session->profile, MAPI_E_NOT_INITIALIZED, NULL);

	retval = OpenUserMailbox(session, session->profile->username, obj_store);

	/* Exchange clustered case */
	if ((retval != MAPI_E_SUCCESS) && 
	    ((GetLastError() == ecUnknownUser) || (GetLastError() == MAPI_E_LOGON_FAILED))) {
		errno = 0;
		retval = OpenUserMailbox(session, NULL, obj_store);
	}

	return retval;
}


/**
   \details Open another user mailbox

   This function opens the main message store. This allows access to
   the normal user folders.

   \param session pointer to the MAPI session context
   \param username name of the user's mailbox to open
   \param obj_store the result of opening the store

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa MAPIInitialize which is required before opening the store
   \sa GetLastError to check the result of a failed call, if necessary
   \sa OpenPublicFolder if you need access to the public folders
 */
_PUBLIC_ enum MAPISTATUS OpenUserMailbox(struct mapi_session *session,
					 const char *username,
					 mapi_object_t *obj_store)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct Logon_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_object_store_t	*store;
	char			*mailbox;

	/* sanity checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!session->profile, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = talloc_init("OpenMsgStore");
	size = 0;

	if (!username) {
		mailbox = talloc_strdup(mem_ctx, session->profile->mailbox);
	} else {
		mailbox = talloc_asprintf(mem_ctx, "/o=%s/ou=%s/cn=Recipients/cn=%s", session->profile->org,
					  session->profile->ou, username);
	}

	/* Fill the Logon operation */
	request.LogonFlags = LogonPrivate;
	size += sizeof (uint8_t);
	request.OpenFlags = HOME_LOGON | TAKE_OWNERSHIP | NO_MAIL;
	size += sizeof (uint32_t);
	request.StoreState = 0;
	size += sizeof (uint32_t);
	request.EssDN = talloc_strdup(mem_ctx, mailbox);
	size += strlen(request.EssDN) + 1;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_Logon;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_Logon = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) + 2;
	mapi_request->length = size + 2;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = 0xffffffff;

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* set object session and handle */
	mapi_object_set_session(obj_store, session);
	mapi_object_set_handle(obj_store, mapi_response->handles[0]);

	/* retrieve store content */
	obj_store->private_data = talloc((TALLOC_CTX *)session, mapi_object_store_t);
	store = (mapi_object_store_t *)obj_store->private_data;
	MAPI_RETVAL_IF(!obj_store->private_data, MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	store->fid_non_ipm_subtree = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.FolderIds[0];
	store->fid_deferred_actions = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.FolderIds[1];
	store->fid_spooler_queue = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.FolderIds[2];
	store->fid_top_information_store = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.FolderIds[3];
	store->fid_inbox = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.FolderIds[4]; 
	store->fid_outbox = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.FolderIds[5];
	store->fid_sent_items = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.FolderIds[6];
	store->fid_deleted_items = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.FolderIds[7];
	store->fid_common_views = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.FolderIds[8];
	store->fid_schedule = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.FolderIds[9];
	store->fid_search = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.FolderIds[10];
	store->fid_views = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.FolderIds[11];
	store->fid_shortcuts = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.FolderIds[12];
	store->cached_mailbox_fid = false;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
