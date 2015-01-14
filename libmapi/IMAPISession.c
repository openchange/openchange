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

#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"


/**
   \file IMAPISession.c

   \brief Session initialization options
 */


static enum MAPISTATUS FindGoodServer(struct mapi_session *session, 
				      const char *legacyDN, 
				      bool server)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct nspi_context		*nspi;
	struct StringsArray_r		pNames;
	struct PropertyRowSet_r		*PropertyRowSet;
	struct SPropTagArray		*SPropTagArray = NULL;
	struct PropertyTagArray_r	*MId_array;
	struct StringArray_r		*MVszA = NULL;
	const char			*binding = NULL;
	char				*HomeMDB = NULL;
	char				*server_dn;
	uint32_t			i;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!session->nspi->ctx, MAPI_E_END_OF_SESSION, NULL);
	OPENCHANGE_RETVAL_IF(!legacyDN, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(session, 0, "FindGoodServer");
	nspi = (struct nspi_context *) session->nspi->ctx;

	if (server == false) {
		/* Step 1. Retrieve a MID for our legacyDN */
		pNames.Count = 0x1;
		pNames.Strings = (const char **) talloc_array(mem_ctx, char *, 1);
		pNames.Strings[0] = (const char *) talloc_strdup(pNames.Strings, legacyDN);

		MId_array = talloc_zero(mem_ctx, struct PropertyTagArray_r);
		retval = nspi_DNToMId(nspi, mem_ctx, &pNames, &MId_array);
		MAPIFreeBuffer(pNames.Strings);
		OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

		/* Step 2. Retrieve the Server DN associated to this MId */
		PropertyRowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
		SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_EMS_AB_HOME_MDB);
		retval = nspi_GetProps(nspi, mem_ctx, SPropTagArray, MId_array, &PropertyRowSet);
		MAPIFreeBuffer(SPropTagArray);
		MAPIFreeBuffer(MId_array);
		OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

		HomeMDB = (char *)find_PropertyValue_data(&(PropertyRowSet->aRow[0]), PR_EMS_AB_HOME_MDB);
		OPENCHANGE_RETVAL_IF(!HomeMDB, MAPI_E_NOT_FOUND, mem_ctx);
		server_dn = x500_truncate_dn_last_elements(mem_ctx, HomeMDB, 1);
		MAPIFreeBuffer(PropertyRowSet);
	} else {
		server_dn = talloc_strdup(mem_ctx, legacyDN);
	}

	/* Step 3. Retrieve the MId for this server DN */
	pNames.Count = 0x1;
	pNames.Strings = (const char **) talloc_array(mem_ctx, char *, 1);
	pNames.Strings[0] = (const char *) talloc_strdup(pNames.Strings, server_dn);
	MId_array = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	retval = nspi_DNToMId(nspi, mem_ctx, &pNames, &MId_array);
	MAPIFreeBuffer(pNames.Strings);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Step 4. Retrieve the binding strings associated to this DN */
	PropertyRowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_EMS_AB_NETWORK_ADDRESS);
	retval = nspi_GetProps(nspi, mem_ctx, SPropTagArray, MId_array, &PropertyRowSet);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(MId_array);
	MAPIFreeBuffer(server_dn);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Step 5. Extract host from ncacn_ip_tcp binding string */
	MVszA = (struct StringArray_r *) find_PropertyValue_data(&(PropertyRowSet->aRow[0]), PR_EMS_AB_NETWORK_ADDRESS);
	OPENCHANGE_RETVAL_IF(!MVszA, MAPI_E_NOT_FOUND, mem_ctx);
	for (i = 0; i != MVszA->cValues; i++) {
		if (!strncasecmp(MVszA->lppszA[i], "ncacn_ip_tcp:", 13)) {
			binding = MVszA->lppszA[i] + 13;
			break;
		}
	}
	MAPIFreeBuffer(PropertyRowSet);
	OPENCHANGE_RETVAL_IF(!binding, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 6. Close the existing session and initiates it again */
	talloc_free(session->emsmdb);
	session->emsmdb = talloc_zero(session, struct mapi_provider);
	talloc_set_destructor((void *)session->emsmdb, (int (*)(void *))emsmdb_disconnect_dtor);
	session->profile->server = talloc_strdup(session->profile, binding);
	retval = Logon(session, session->emsmdb, PROVIDER_ID_EMSMDB);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Open the Public Folder store

   This function opens the public folder store. This allows access to
   the public folders.

   \param obj_store the result of opening the store
   \param session pointer to the MAPI session context

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
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
	uint8_t			logon_id;
	bool			retry = false;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!session->profile, MAPI_E_NOT_INITIALIZED, NULL);

	/* Find the first available logon id */
	retval = GetNewLogonId(session, &logon_id);
	OPENCHANGE_RETVAL_IF(retval, MAPI_E_FAILONEPROVIDER, NULL);

retry:
	mem_ctx = talloc_named(session, 0, "OpenPublicFolder");
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
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	if (retval == ecWrongServer && retry == false && mapi_response->mapi_repl->us.mapi_Logon.ServerName) {
		retval = FindGoodServer(session, mapi_response->mapi_repl->us.mapi_Logon.ServerName, true);
		OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
		talloc_free(mem_ctx);
		retry = true;
		goto retry;
	}
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* retrieve object session, handle and logon_id */
	mapi_object_set_session(obj_store, session);
	mapi_object_set_handle(obj_store, mapi_response->handles[0]);
	mapi_object_set_logon_id(obj_store, logon_id);
	mapi_object_set_logon_store(obj_store);

	/* retrieve store content */
	obj_store->private_data = talloc_zero((TALLOC_CTX *)session, mapi_object_store_t);
	store = (mapi_object_store_t*)obj_store->private_data;
	OPENCHANGE_RETVAL_IF(!obj_store->private_data, MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	store->fid_pf_public_root = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.Root;
	store->fid_pf_ipm_subtree = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.IPMSubTree;
	store->fid_pf_non_ipm_subtree = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.NonIPMSubTree;
	store->fid_pf_EFormsRegistryRoot = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.EFormsRegistry;
	store->fid_pf_FreeBusyRoot = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.FreeBusy;
	store->fid_pf_OfflineAB = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.OAB;
	store->fid_pf_EFormsRegistry = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.LocalizedEFormsRegistry;
	store->fid_pf_LocalSiteFreeBusy = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.LocalFreeBusy;
	store->fid_pf_LocalSiteOfflineAB = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.LocalOAB;
	store->fid_pf_NNTPArticle = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.NNTPIndex;
	store->store_type = PublicFolder;

	store->guid = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_pf.Guid;

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

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
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
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!session->profile, MAPI_E_NOT_INITIALIZED, NULL);

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

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
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
	uint8_t			logon_id;
	bool			retry = false;

	/* sanity checks */
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!session->profile, MAPI_E_NOT_INITIALIZED, NULL);

	/* Find the first available logon id */
	retval = GetNewLogonId(session, &logon_id);
	OPENCHANGE_RETVAL_IF(retval, MAPI_E_FAILONEPROVIDER, NULL);

retry:
	mem_ctx = talloc_named(session, 0, "OpenMsgStore");
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
	size = 0;

	mailbox = talloc_strdup(mem_ctx, session->profile->mailbox);
	OPENCHANGE_RETVAL_IF(!mailbox, MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	/* Fill the Logon operation */
	request.LogonFlags = LogonPrivate;
	size += sizeof (uint8_t);
	request.OpenFlags = USE_PER_MDB_REPLID_MAPPING | HOME_LOGON | TAKE_OWNERSHIP | NO_MAIL;
	size += sizeof (uint32_t);
	request.StoreState = 0;
	size += sizeof (uint32_t);
	request.EssDN = talloc_strdup(mem_ctx, mailbox);
	size += strlen(request.EssDN) + 1;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_Logon;
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	if (retval == ecWrongServer && retry == false) {
		retval = FindGoodServer(session, mailbox, false);
		OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
		talloc_free(mem_ctx);
		retry = true;
		goto retry;
	}

	OPENCHANGE_RETVAL_CALL_IF(retval, retval, mapi_response, mem_ctx);
	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* set object session, handle and logon_id */
	mapi_object_set_session(obj_store, session);
	mapi_object_set_handle(obj_store, mapi_response->handles[0]);
	mapi_object_set_logon_id(obj_store, logon_id);
	mapi_object_set_logon_store(obj_store);

	/* retrieve store content */
	obj_store->private_data = talloc_zero((TALLOC_CTX *)session, mapi_object_store_t);
	store = (mapi_object_store_t *)obj_store->private_data;
	OPENCHANGE_RETVAL_IF(!obj_store->private_data, MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	store->fid_mailbox_root = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.Root;
	store->fid_deferred_actions = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.DeferredAction;
	store->fid_spooler_queue = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.SpoolerQueue;
	store->fid_top_information_store = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.IPMSubTree;
	store->fid_inbox = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.Inbox;
	store->fid_outbox = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.Outbox;
	store->fid_sent_items = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.SentItems;
	store->fid_deleted_items = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.DeletedItems;
	store->fid_common_views = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.CommonViews;
	store->fid_schedule = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.Schedule;
	store->fid_search = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.Search;
	store->fid_views = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.Views;
	store->fid_shortcuts = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.Shortcuts;
	store->store_type = PrivateFolderWithoutCachedFids;

	store->guid = mapi_response->mapi_repl->u.mapi_Logon.LogonType.store_mailbox.MailboxGuid;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
