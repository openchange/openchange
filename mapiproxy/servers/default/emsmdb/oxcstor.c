/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

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
   \file oxcstor.c

   \brief Server-side store objects routines and Rops
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/util/samdb.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"

#include <string.h>

/**
   \details Logs on a private mailbox

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the RopLogon EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the RopLogon EcDoRpc_MAPI_REPL
   structure the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
static enum MAPISTATUS RopLogon_Mailbox(TALLOC_CTX *mem_ctx,
					struct emsmdbp_context *emsmdbp_ctx,
					struct EcDoRpc_MAPI_REQ *mapi_req,
					struct EcDoRpc_MAPI_REPL *mapi_repl)
{
	struct Logon_req	*request;
	struct Logon_repl	*response;
	const char * const	attrs[] = { "*", NULL };
	enum MAPISTATUS		ret;
	struct ldb_result	*res = NULL;
	const char		*username;
	struct tm		*LogonTime;
	time_t			t;
	NTTIME			nttime;

	request = &mapi_req->u.mapi_Logon;
	response = &mapi_repl->u.mapi_Logon;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!request->EssDN, MAPI_E_INVALID_PARAMETER, NULL);

	/* Step 0. Retrieve user record */
	ret = safe_ldb_search(&emsmdbp_ctx->samdb_ctx, mem_ctx, &res, ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx), LDB_SCOPE_SUBTREE, attrs, "legacyExchangeDN=%s", request->EssDN);
	OPENCHANGE_RETVAL_IF((ret || res->count != 1), ecUnknownUser, NULL);

	/* Step 1. Retrieve username from record */
	username = ldb_msg_find_attr_as_string(res->msgs[0], "sAMAccountName", NULL);
	OPENCHANGE_RETVAL_IF(!username, ecUnknownUser, NULL);

	/* Step 2. Set the logon map */
	ret = mapi_logon_set(emsmdbp_ctx->logon_ctx, mapi_req->logon_id, username);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, NULL);

	/* Step 3. Init and or update the user mailbox (auto-provisioning) */
	ret = emsmdbp_mailbox_provision(emsmdbp_ctx, username);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, MAPI_E_DISK_ERROR, NULL);
	/* TODO: freebusy entry should be created only during freebusy lookups */
	if (strncmp(username, emsmdbp_ctx->auth_user, strlen(username)) == 0) {
		ret = emsmdbp_mailbox_provision_public_freebusy(emsmdbp_ctx, request->EssDN);
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, MAPI_E_DISK_ERROR, NULL);
	}

	/* Step 4. Set LogonFlags */
	response->LogonFlags = request->LogonFlags;

	/* Step 5. Build FolderIds list */
	openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_MAILBOX_ROOT, &response->LogonType.store_mailbox.Root);
	openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_DEFERRED_ACTION, &response->LogonType.store_mailbox.DeferredAction);
	openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_SPOOLER_QUEUE, &response->LogonType.store_mailbox.SpoolerQueue);
	openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_TOP_INFORMATION_STORE, &response->LogonType.store_mailbox.IPMSubTree);
	openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_INBOX, &response->LogonType.store_mailbox.Inbox);
	openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_OUTBOX, &response->LogonType.store_mailbox.Outbox);
	openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_SENT_ITEMS, &response->LogonType.store_mailbox.SentItems);
	openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_DELETED_ITEMS, &response->LogonType.store_mailbox.DeletedItems);
	openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_COMMON_VIEWS, &response->LogonType.store_mailbox.CommonViews);
	openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_SCHEDULE, &response->LogonType.store_mailbox.Schedule);
	openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_SEARCH, &response->LogonType.store_mailbox.Search);
	openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_VIEWS, &response->LogonType.store_mailbox.Views);
	openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_SHORTCUTS, &response->LogonType.store_mailbox.Shortcuts);

	/* Step 6. Set ResponseFlags */
	response->LogonType.store_mailbox.ResponseFlags = ResponseFlags_Reserved;
	if (username && emsmdbp_ctx->auth_user && strcmp(username, emsmdbp_ctx->auth_user) == 0) {
		response->LogonType.store_mailbox.ResponseFlags |= ResponseFlags_OwnerRight | ResponseFlags_SendAsRight;
	}

	/* Step 7. Retrieve MailboxGuid */
	openchangedb_get_MailboxGuid(emsmdbp_ctx->oc_ctx, username, &response->LogonType.store_mailbox.MailboxGuid);

	/* Step 8. Retrieve mailbox replication information */
	openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, username,
					&response->LogonType.store_mailbox.ReplId,
					&response->LogonType.store_mailbox.ReplGUID);

	/* Step 9. Set LogonTime both in openchange dispatcher database and reply */
	t = time(NULL);
	LogonTime = localtime(&t);
	response->LogonType.store_mailbox.LogonTime.Seconds = LogonTime->tm_sec;
	response->LogonType.store_mailbox.LogonTime.Minutes = LogonTime->tm_min;
	response->LogonType.store_mailbox.LogonTime.Hour = LogonTime->tm_hour;
	response->LogonType.store_mailbox.LogonTime.DayOfWeek = LogonTime->tm_wday;
	response->LogonType.store_mailbox.LogonTime.Day = LogonTime->tm_mday;
	response->LogonType.store_mailbox.LogonTime.Month = LogonTime->tm_mon + 1;
	response->LogonType.store_mailbox.LogonTime.Year = LogonTime->tm_year + 1900;

	/* Step 10. Retrieve GwartTime */
	unix_to_nt_time(&nttime, t);
	response->LogonType.store_mailbox.GwartTime = nttime - 1000000;

	/* Step 11. Set StoreState */
	response->LogonType.store_mailbox.StoreState = 0x0;

	return MAPI_E_SUCCESS;
}

/**
   \details Logs on a public folder store

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the RopLogon EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the RopLogon EcDoRpc_MAPI_REPL 
   structure that the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
static enum MAPISTATUS RopLogon_PublicFolder(TALLOC_CTX *mem_ctx,
					     struct emsmdbp_context *emsmdbp_ctx,
					     struct EcDoRpc_MAPI_REQ *mapi_req,
					     struct EcDoRpc_MAPI_REPL *mapi_repl)
{
	struct Logon_req	*request;
	struct Logon_repl	*response;

	request = &mapi_req->u.mapi_Logon;
	response = &mapi_repl->u.mapi_Logon;

	response->LogonFlags = request->LogonFlags;

        /* FIXME we are getting the user from EcDoConnect(|Ex) instead of Logon ROP */
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->logon_user, EMSMDBP_PF_ROOT, &response->LogonType.store_pf.Root);
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->logon_user, EMSMDBP_PF_IPMSUBTREE, &response->LogonType.store_pf.IPMSubTree);
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->logon_user, EMSMDBP_PF_NONIPMSUBTREE, &response->LogonType.store_pf.NonIPMSubTree);
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->logon_user, EMSMDBP_PF_EFORMSREGISTRY, &response->LogonType.store_pf.EFormsRegistry);
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->logon_user, EMSMDBP_PF_FREEBUSY, &response->LogonType.store_pf.FreeBusy);
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->logon_user, EMSMDBP_PF_OAB, &response->LogonType.store_pf.OAB);
	response->LogonType.store_pf.LocalizedEFormsRegistry = 0;
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->logon_user, EMSMDBP_PF_LOCALFREEBUSY, &response->LogonType.store_pf.LocalFreeBusy);
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->logon_user, EMSMDBP_PF_LOCALOAB, &response->LogonType.store_pf.LocalOAB);
	response->LogonType.store_pf.NNTPIndex = 0;
	memset(response->LogonType.store_pf._empty, 0, sizeof(uint64_t) * 3);

	openchangedb_get_PublicFolderReplica(emsmdbp_ctx->oc_ctx,
					     emsmdbp_ctx->logon_user,
					     &response->LogonType.store_pf.ReplId,
					     &response->LogonType.store_pf.Guid);
	memset(&response->LogonType.store_pf.PerUserGuid, 0, sizeof(struct GUID));

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc Logon (0xFE) Rop. This operation logs on to a
   private mailbox or public folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the Logon EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the Logon EcDoRpc_MAPI_REPL structure
   the function returns
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \note Users are only allowed to open their own mailbox at the
   moment. This limitation will be removed when significant progress
   have been made.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopLogon(TALLOC_CTX *mem_ctx,
					  struct emsmdbp_context *emsmdbp_ctx,
					  struct EcDoRpc_MAPI_REQ *mapi_req,
					  struct EcDoRpc_MAPI_REPL *mapi_repl,
					  uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	struct Logon_req		*request;
	struct mapi_handles		*rec = NULL;
	struct emsmdbp_object		*object;
	bool				mailboxstore = true;
	const char			*essDN = NULL;

	OC_DEBUG(4, "exchange_emsmdb: [OXCSTOR] Logon (0xFE)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = &mapi_req->u.mapi_Logon;

	essDN = request->EssDN;

	/* Fill EcDoRpc_MAPI_REPL reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	if (request->LogonFlags & LogonPrivate) {
		retval = RopLogon_Mailbox(mem_ctx, emsmdbp_ctx, mapi_req, mapi_repl);
		mapi_repl->error_code = retval;
		*size += libmapiserver_RopLogon_size(mapi_req, mapi_repl);
	} else {
		retval = RopLogon_PublicFolder(mem_ctx, emsmdbp_ctx, mapi_req, mapi_repl);
		/* mapi_repl->error_code = MAPI_E_LOGON_FAILED; */
		mapi_repl->error_code = retval;
		mailboxstore = false;
		*size += libmapiserver_RopLogon_size(mapi_req, mapi_repl);

		/*
		 * EssDN for public logons may be empty string
		 * In this case, fallback to emsmdbp_ctx->szUserDN
		 */
		if (strlen(essDN) == 0) {
			essDN = emsmdbp_ctx->szUserDN;
		}
	}

	if (mapi_repl->error_code == MAPI_E_SUCCESS) {
		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, 0, &rec);
		object = emsmdbp_object_mailbox_init((TALLOC_CTX *)rec, emsmdbp_ctx, essDN, mailboxstore);
		retval = mapi_handles_set_private_data(rec, object);

		handles[mapi_repl->handle_idx] = rec->handle;
	}

	return retval;
}


/**
   \details EcDoRpc Release (0x01) Rop. This operation releases an
   existing MAPI handle.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param request pointer to the Release EcDoRpc_MAPI_REQ
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopRelease(TALLOC_CTX *mem_ctx,
					    struct emsmdbp_context *emsmdbp_ctx,
					    struct EcDoRpc_MAPI_REQ *request,
					    uint32_t *handles,
					    uint16_t *size)
{
        /* struct mapistore_subscription_list *subscription_list, *subscription_holder;
	   struct mapistore_subscription *subscription;
	   struct mapistore_subscription_list	*el;*/
	enum MAPISTATUS				retval;
	uint32_t				handle;

	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx->logon_user, MAPI_E_LOGON_FAILED, NULL);

	handle = handles[request->handle_idx];
#if 0
next:
	for (el = emsmdbp_ctx->mstore_ctx->subscriptions; el; el = el->next) {
		if (handle == el->subscription->handle) {
			OC_DEBUG(0, ("*** DELETING SUBSCRIPTION ***\n"));
			OC_DEBUG(0, ("subscription: handle = 0x%x\n", el->subscription->handle));
			OC_DEBUG(0, ("subscription: types = 0x%x\n", el->subscription->notification_types));
			OC_DEBUG(0, ("subscription: mqueue = %d\n", el->subscription->mqueue));
			OC_DEBUG(0, ("subscription: mqueue name = %s\n", el->subscription->mqueue_name));
			DLIST_REMOVE(emsmdbp_ctx->mstore_ctx->subscriptions, el);
			goto next;
		}
	}
#endif

	/* If we have notification's subscriptions attached to this handle, we
	   obviously remove them in order to avoid invoking them once all ROPs
	   are processed */
/* retry: */
/* 	subscription_list = emsmdbp_ctx->mstore_ctx->subscriptions; */
/* 	subscription_holder = subscription_list; */
/* 	while (subscription_holder) { */
/* 	        subscription = subscription_holder->subscription; */
		  
/* 		if (handle == subscription->handle) { */
/* 			OC_DEBUG(0, ("*** DELETING SUBSCRIPTION ***\n")); */
/* 			OC_DEBUG(0, ("subscription: handle = 0x%x\n", subscription->handle)); */
/* 			OC_DEBUG(0, ("subscription: types = 0x%x\n", subscription->notification_types)); */
/* 			OC_DEBUG(0, ("subscription: mqueue = %d\n", subscription->mqueue)); */
/* 			OC_DEBUG(0, ("subscription: mqueue name = %s\n", subscription->mqueue_name)); */
/* 			DLIST_REMOVE(subscription_list, subscription_holder); */
/* 			talloc_free(subscription_holder); */
/* 			goto retry; */
/* 		} */


/* 		subscription_holder = subscription_holder->next; */
/*        	} */
	/* We finally really delete the handle */
	retval = mapi_handles_delete(emsmdbp_ctx->handles_ctx, handle);
	OPENCHANGE_RETVAL_IF(retval && retval != MAPI_E_NOT_FOUND, retval, NULL);
	
	return MAPI_E_SUCCESS;
}

/* Test MessageClass string according to [MS-OXCSTOR] section 2.2.1.2.1.1 and 2.2.1.3.1.2 */
static bool MessageClassIsValid(const char *MessageClass)
{
	size_t		len = strlen(MessageClass);
	int		i;
	
	if (len + 1 > 255) {
		return false;
	}

	for (i = 0; i < len; i++) {
		if ((MessageClass[i] < 32) || (MessageClass[i] > 126)) {
			return false;
		}
		if ((MessageClass[i] == '.') && (MessageClass[i + 1]) && (MessageClass[i + 1] == '.')) {
			return false;
		}
	}
	
	if (MessageClass[0] && (MessageClass[0] == '.')) {
		return false;
	}
	if (MessageClass[0] && (MessageClass[len - 1] == '.')) {
		return false;
	}
	
	return true;
}

/**
   \details EcDoRpc SetReceiveFolder (0x26) Rop Internals. This
   routine performs the SetReceiveFolder internals.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SetReceiveFolder EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the SetReceiveFolder EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
static enum MAPISTATUS RopSetReceiveFolder(TALLOC_CTX *mem_ctx,
					   struct emsmdbp_context *emsmdbp_ctx,
					   struct EcDoRpc_MAPI_REQ *mapi_req,
					   struct EcDoRpc_MAPI_REPL *mapi_repl,
					   uint32_t *handles)
{
	enum MAPISTATUS		retval;
	struct mapi_handles	*rec = NULL;
	struct emsmdbp_object	*object = NULL;
	const char		*MessageClass = NULL;
	void			*private_data = NULL;
	uint32_t		handle;
	uint64_t		fid;

	/* Step 1. Ensure the referring MAPI handle is mailbox one */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	retval = mapi_handles_get_private_data(rec, (void **)&private_data);
	object = (struct emsmdbp_object *) private_data;
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);
	OPENCHANGE_RETVAL_IF(object->type != EMSMDBP_OBJECT_MAILBOX, MAPI_E_NO_SUPPORT, NULL);

	if (object->object.mailbox->mailboxstore == false) {
		return MAPI_E_NO_SUPPORT;
	}
	
	/* Step 2. Verify MessageClass string */
	fid = mapi_req->u.mapi_SetReceiveFolder.fid;
	MessageClass = mapi_req->u.mapi_SetReceiveFolder.lpszMessageClass;
	if (!MessageClass || (strcmp(MessageClass, "") == 0)) {
		MessageClass="All";
	}
	if ((fid == 0x0) && (!strcmp(MessageClass, "All"))) {
		return MAPI_E_CALL_FAILED;
	}
	if (strcasecmp(MessageClass, "IPM") == 0) {
		return MAPI_E_NO_ACCESS;
	}
	if (strcasecmp(MessageClass, "Report.IPM") == 0) {
		return MAPI_E_NO_ACCESS;
	}
	if (! MessageClassIsValid(MessageClass) ) {
		return MAPI_E_INVALID_PARAMETER;
	}

	/* Step 3.Set the receive folder for this message class within user mailbox */
	retval = openchangedb_set_ReceiveFolder(emsmdbp_ctx->oc_ctx, object->object.mailbox->owner_username, MessageClass, fid);
	OPENCHANGE_RETVAL_IF(retval, ecNoReceiveFolder, NULL);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc SetReceiveFolder (0x26) Rop. This operation sets
   the receive folder for incoming messages of a particular message
   class

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SetReceiveFolder EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the SetReceiveFolder EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSetReceiveFolder(TALLOC_CTX *mem_ctx,
						     struct emsmdbp_context *emsmdbp_ctx,
						     struct EcDoRpc_MAPI_REQ *mapi_req,
						     struct EcDoRpc_MAPI_REPL *mapi_repl,
						     uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;

	OC_DEBUG(4, "exchange_emsmdb: [OXCSTOR] SetReceiveFolder (0x26)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	/* Call effective code */
	retval = RopSetReceiveFolder(mem_ctx, emsmdbp_ctx, mapi_req, mapi_repl, handles);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = retval;

	*size += libmapiserver_RopSetReceiveFolder_size(mapi_repl);
	
	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return retval;
}

/**
   \details EcDoRpc GetReceiveFolder (0x27) Rop Internals. This
   routine performs the GetReceiveFolder internals.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetReceiveFolder EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the GetReceiveFolder EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
static enum MAPISTATUS RopGetReceiveFolder(TALLOC_CTX *mem_ctx,
					   struct emsmdbp_context *emsmdbp_ctx,
					   struct EcDoRpc_MAPI_REQ *mapi_req,
					   struct EcDoRpc_MAPI_REPL *mapi_repl,
					   uint32_t *handles)
{
	enum MAPISTATUS		retval;
	struct mapi_handles	*rec = NULL;
	struct emsmdbp_object	*object = NULL;
	const char		*MessageClass = NULL;
	void			*private_data = NULL;
	uint32_t		handle;

	/* Step 1. Ensure the referring MAPI handle is mailbox one */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	retval = mapi_handles_get_private_data(rec, (void **)&private_data);
	object = (struct emsmdbp_object *) private_data;
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);
	OPENCHANGE_RETVAL_IF(object->type != EMSMDBP_OBJECT_MAILBOX, MAPI_E_NO_SUPPORT, NULL);

	/* Step 2. Do not allow over Public Folder */
	if (object->object.mailbox->mailboxstore == false) {
		return MAPI_E_NO_SUPPORT;
	}
	
	/* Step 3. Verify MessageClass string */
	MessageClass = mapi_req->u.mapi_GetReceiveFolder.MessageClass;
	if (!MessageClass || !strcmp(MessageClass, "")) {
		MessageClass="All";
	}
	if (! MessageClassIsValid(MessageClass) ) {
		return MAPI_E_INVALID_PARAMETER;
	}

	/* Step 4. Search for the specified MessageClass substring within user mailbox */
	mapi_repl->u.mapi_GetReceiveFolder.MessageClass = NULL;
	retval = openchangedb_get_ReceiveFolder(mem_ctx, emsmdbp_ctx->oc_ctx, object->object.mailbox->owner_username,
						MessageClass, &mapi_repl->u.mapi_GetReceiveFolder.folder_id,
						&mapi_repl->u.mapi_GetReceiveFolder.MessageClass);
	OPENCHANGE_RETVAL_IF(retval, ecNoReceiveFolder, NULL);

	if (mapi_repl->u.mapi_GetReceiveFolder.MessageClass == NULL) {
		openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->logon_user, EMSMDBP_INBOX,
						&mapi_repl->u.mapi_GetReceiveFolder.folder_id);
		mapi_repl->u.mapi_GetReceiveFolder.MessageClass = "";
	}

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc GetReceiveFolder (0x27) Rop. This operation gets
   the receive folder for incoming messages of a particular message
   class

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetReceiveFolder EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the GetReceiveFolder EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetReceiveFolder(TALLOC_CTX *mem_ctx,
						     struct emsmdbp_context *emsmdbp_ctx,
						     struct EcDoRpc_MAPI_REQ *mapi_req,
						     struct EcDoRpc_MAPI_REPL *mapi_repl,
						     uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;

	OC_DEBUG(4, "exchange_emsmdb: [OXCSTOR] GetReceiveFolder (0x27)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	/* Call effective code */
	retval = RopGetReceiveFolder(mem_ctx, emsmdbp_ctx, mapi_req, mapi_repl, handles);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = retval;

	*size += libmapiserver_RopGetReceiveFolder_size(mapi_repl);
	
	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return retval;
}

/**
   \details EcDoRpc GetReceiveFoldertable (0x68) Rop Internals. This
   routine performs the GetReceiveFolderTable internals.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetReceiveFolderTable EcDoRpc_MAPI_REQ
   \param mapi_reqpl pointer to the GetReceiveFolderTable EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
static enum MAPISTATUS RopGetReceiveFolderTable(TALLOC_CTX *mem_ctx,
						struct emsmdbp_context *emsmdbp_ctx,
						struct EcDoRpc_MAPI_REQ *mapi_req,
						struct EcDoRpc_MAPI_REPL *mapi_repl,
						uint32_t *handles)
{
	enum MAPISTATUS		retval;
	struct mapi_handles	*rec = NULL;
	struct emsmdbp_object	*object = NULL;
	void			*private_data = NULL;
	uint32_t		handle;

	/* Step 1. Ensure the referring MAPI handle is mailbox one */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	retval = mapi_handles_get_private_data(rec, (void **)&private_data);
	object = (struct emsmdbp_object *) private_data;
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);
	OPENCHANGE_RETVAL_IF(object->type != EMSMDBP_OBJECT_MAILBOX, MAPI_E_NO_SUPPORT, NULL);

	/* Step 2. Do not allow over Public Folder */
	if (object->object.mailbox->mailboxstore == false) {
		return MAPI_E_NO_SUPPORT;
	}

	/* Step 3. Retrieve reveive folder table and properties from user mailbox */
	retval = openchangedb_get_ReceiveFolderTable(mem_ctx, emsmdbp_ctx->oc_ctx,
						     object->object.mailbox->owner_username,
						     &mapi_repl->u.mapi_GetReceiveFolderTable.cValues,
						     &mapi_repl->u.mapi_GetReceiveFolderTable.entries);

	OPENCHANGE_RETVAL_IF(retval, ecNoReceiveFolder, NULL);
	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc GetReceiveFolderTable (0x68) Rop. This operation
   obtain a comprehensive list of all configured message classes and
   their associated Receive folders

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the esmsmdb provider context
   \param mapi_req pointer to the GetReceiveFolderTable EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the GetReceiveFolderTable EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
*/
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetReceiveFolderTable(TALLOC_CTX *mem_ctx,
							  struct emsmdbp_context *emsmdbp_ctx,
							  struct EcDoRpc_MAPI_REQ *mapi_req,
							  struct EcDoRpc_MAPI_REPL *mapi_repl,
							  uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;

	OC_DEBUG(4, "exchange_emsmdb: [OXCSTOR] GetReceiveFolderTable (0x68)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	/* Call effective code */
	retval = RopGetReceiveFolderTable(mem_ctx, emsmdbp_ctx, mapi_req, mapi_repl, handles);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = retval;

	*size = libmapiserver_RopGetReceiveFolderTable_size(mapi_repl);

	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return retval;
}


/**
   \details EcDoRpc EcDoRpc_RopLongTermIdFromId (0x43) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the LongTermIdFromId EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the LongTermIdFromId EcDoRpc_MAPI_REPL structure

   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopLongTermIdFromId(TALLOC_CTX *mem_ctx,
						     struct emsmdbp_context *emsmdbp_ctx,
						     struct EcDoRpc_MAPI_REQ *mapi_req,
						     struct EcDoRpc_MAPI_REPL *mapi_repl,
						     uint32_t *handles, uint16_t *size)
{
	struct LongTermIdFromId_req	*request;
	struct LongTermIdFromId_repl	*response;
	enum MAPISTATUS			retval;
	uint32_t			handle;
	struct mapi_handles		*rec = NULL;
	struct emsmdbp_object		*object = NULL;
	void				*data;
	uint16_t			req_repl_id;
	uint64_t			id;
	uint8_t				i;

	OC_DEBUG(4, "exchange_emsmdb: [OXCSTOR] LongTermIdFromId (0x43)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	request = &mapi_req->u.mapi_LongTermIdFromId;
	response = &mapi_repl->u.mapi_LongTermIdFromId;

	req_repl_id = request->Id & 0xffff;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	/* Check we have a logon user */
	if (!emsmdbp_ctx->logon_user) {
		mapi_repl->error_code = MAPI_E_LOGON_FAILED;
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		OC_DEBUG(5, "  handle data not found, idx = %x\n", mapi_req->handle_idx);
		goto end;
	}
	object = (struct emsmdbp_object *) data;
	if (!object || object->type != EMSMDBP_OBJECT_MAILBOX) {
		abort();
		OC_DEBUG(5, "  no object or object is not a mailbox\n");
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}
	if (emsmdbp_replid_to_guid(emsmdbp_ctx, object->object.mailbox->owner_username, req_repl_id, &response->LongTermId.DatabaseGuid)) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

	id = request->Id >> 16;
	for (i = 0; i < 6; i++) {
		response->LongTermId.GlobalCounter[i] = id & 0xff;
		id >>= 8;
	}
	response->LongTermId.padding = 0;

end:
	*size += libmapiserver_RopLongTermIdFromId_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/* NEW and cleaner version that does not work: */

/**
   \details EcDoRpc  (0x44) Rop. This operation sets
   or clears the message read flag.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SetMessageReadFlag EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the SetMessageReadFlag
   EcDoRpc_MAPI_REPL structure

   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopIdFromLongTermId(TALLOC_CTX *mem_ctx,
						     struct emsmdbp_context *emsmdbp_ctx,
						     struct EcDoRpc_MAPI_REQ *mapi_req,
						     struct EcDoRpc_MAPI_REPL *mapi_repl,
						     uint32_t *handles, uint16_t *size)
{
	struct IdFromLongTermId_req	*request;
	struct IdFromLongTermId_repl	*response;
	enum MAPISTATUS			retval;
	uint32_t			handle;
	struct mapi_handles		*rec = NULL;
	struct emsmdbp_object		*object = NULL;
	void				*data;
	uint16_t			repl_id;
	uint64_t			fmid, base;
	uint8_t				i, ctr_byte;

	OC_DEBUG(4, "exchange_emsmdb: [OXCSTOR] RopIdFromLongTermId (0x44)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	request = &mapi_req->u.mapi_IdFromLongTermId;
	response = &mapi_repl->u.mapi_IdFromLongTermId;

	if (GUID_all_zero(&request->LongTermId.DatabaseGuid)) {
		mapi_repl->error_code = MAPI_E_INVALID_PARAMETER;
		goto end;
	}

	ctr_byte = 0;
	for (i = 0; i < 6; i++) {
		ctr_byte = request->LongTermId.GlobalCounter[i];
		if (ctr_byte) break;
	}
	if (ctr_byte == 0) {
		mapi_repl->error_code = MAPI_E_INVALID_PARAMETER;
		goto end;
	}

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	/* Check we have a logon user */
	if (!emsmdbp_ctx->logon_user) {
		mapi_repl->error_code = MAPI_E_LOGON_FAILED;
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		OC_DEBUG(5, "  handle data not found, idx = %x\n", mapi_req->handle_idx);
		goto end;
	}
	object = (struct emsmdbp_object *) data;
	if (!object || object->type != EMSMDBP_OBJECT_MAILBOX) {
		abort();
		OC_DEBUG(5, "  no object or object is not a mailbox\n");
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}
	if (emsmdbp_guid_to_replid(emsmdbp_ctx, object->object.mailbox->owner_username, &request->LongTermId.DatabaseGuid, &repl_id)) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

	fmid = 0;
	base = 1;
	for (i = 0; i < 6; i++) {
		fmid |= (uint64_t) request->LongTermId.GlobalCounter[i] * base;
		base <<= 8;
	}
	response->Id = fmid << 16 | repl_id;

end:
	*size += libmapiserver_RopIdFromLongTermId_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc GetPerUserLongTermIds (0x60) Rop. This operations
   gets the long-term ID of a public folder that is identified by the
   per-user GUID of the logged on user.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetPerUserLongTermIds EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the GetPerUserLongTermIds EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetPerUserLongTermIds(TALLOC_CTX *mem_ctx,
							  struct emsmdbp_context *emsmdbp_ctx,
							  struct EcDoRpc_MAPI_REQ *mapi_req,
							  struct EcDoRpc_MAPI_REPL *mapi_repl,
							  uint32_t *handles, uint16_t *size)
{
	OC_DEBUG(4, "exchange_emsmdb: [OXCSTOR] GetPerUserLongTermIds (0x60) - valid stub\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	/* Ensure the request is performed against a private mailbox
	 * logon, not a public folders logon. If the operation is
	 * performed against a public folders logon, return
	 * MAPI_E_NO_SUPPORT */

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* TODO effective work here */
	mapi_repl->u.mapi_GetPerUserLongTermIds.LongTermIdCount = 0;
	mapi_repl->u.mapi_GetPerUserLongTermIds.LongTermIds = NULL;

	*size += libmapiserver_RopGetPerUserLongTermIds_size(mapi_repl);

	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc GetPerUserGuid (0x61) Rop. This operation
   gets the GUID of a public folder's per-user information.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetPerUserLongTermIds EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the GetPerUserLongTermIds EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetPerUserGuid(TALLOC_CTX *mem_ctx,
						   struct emsmdbp_context *emsmdbp_ctx,
						   struct EcDoRpc_MAPI_REQ *mapi_req,
						   struct EcDoRpc_MAPI_REPL *mapi_repl,
						   uint32_t *handles, uint16_t *size)
{
	OC_DEBUG(4, "exchange_emsmdb: [OXCSTOR] GetPerUserGuid (0x61) - stub\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	/* Ensure the request is performed against a private mailbox
	 * logon, not a public folders logon. If the operation is
	 * performed against a public folders logon, return
	 * MAPI_E_NO_SUPPORT */

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_NOT_FOUND;

	/* TODO effective work here */

	*size += libmapiserver_RopGetPerUserGuid_size(mapi_repl);
	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc ReadPerUserInformation (0x63) Rop. This operation
   gets per-user information for a public folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the ReadPerUserInformation EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the ReadPerUserInformation EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopReadPerUserInformation(TALLOC_CTX *mem_ctx,
							   struct emsmdbp_context *emsmdbp_ctx,
							   struct EcDoRpc_MAPI_REQ *mapi_req,
							   struct EcDoRpc_MAPI_REPL *mapi_repl,
							   uint32_t *handles, uint16_t *size)
{
	OC_DEBUG(4, "exchange_emsmdb: [OXCSTOR] ReadPerUserInformation (0x63) - stub\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	mapi_repl->u.mapi_ReadPerUserInformation.HasFinished = true;
	mapi_repl->u.mapi_ReadPerUserInformation.DataSize = 0x0;
	mapi_repl->u.mapi_ReadPerUserInformation.Data.length = 0x0;
	mapi_repl->u.mapi_ReadPerUserInformation.Data.data = NULL;

	*size += libmapiserver_RopReadPerUserInformation_size(mapi_repl);
	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc GetStoreState (0x63) Rop. This operation
   gets per-user information for a public folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetStoreState EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the GetStoreState EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetStoreState(TALLOC_CTX *mem_ctx,
						  struct emsmdbp_context *emsmdbp_ctx,
						  struct EcDoRpc_MAPI_REQ *mapi_req,
						  struct EcDoRpc_MAPI_REPL *mapi_repl,
						  uint32_t *handles, uint16_t *size)
{
	OC_DEBUG(4, "exchange_emsmdb: [OXCSTOR] GetStoreState (0x63) - stub\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_NOT_IMPLEMENTED;

	*size += libmapiserver_RopGetStoreState_size(mapi_repl);
	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return MAPI_E_SUCCESS;
}
