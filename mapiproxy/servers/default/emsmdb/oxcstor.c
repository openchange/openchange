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
	enum MAPISTATUS		retval;
	char			*recipient;
	struct Logon_req	request;
	struct Logon_repl	response;
	struct tm		*LogonTime;
	time_t			t;
	NTTIME			nttime;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!mapi_req->u.mapi_Logon.EssDN, MAPI_E_INVALID_PARAMETER, NULL);

	request = mapi_req->u.mapi_Logon;
	response = mapi_repl->u.mapi_Logon;

	OPENCHANGE_RETVAL_IF(strcmp(request.EssDN, emsmdbp_ctx->szUserDN), MAPI_E_INVALID_PARAMETER, NULL);

	/* Step 0. Retrieve recipient name */
	recipient = x500_get_dn_element(mem_ctx, request.EssDN, "/cn=Recipients/cn=");
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);

	/* Step 1. Check if mailbox pointed by recipient belongs to the Exchange organisation */

	/* Step 2. Set LogonFlags */
	response.LogonFlags = request.LogonFlags;

	/* Step 3. Build FolderIds list */
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, recipient, EMSMDBP_MAILBOX_ROOT, &response.LogonType.store_mailbox.FolderIds[0]);
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, recipient, EMSMDBP_DEFERRED_ACTIONS, &response.LogonType.store_mailbox.FolderIds[1]);
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, recipient, EMSMDBP_SPOOLER_QUEUE, &response.LogonType.store_mailbox.FolderIds[2]);
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, recipient, EMSMDBP_TOP_INFORMATION_STORE, &response.LogonType.store_mailbox.FolderIds[3]);
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, recipient, EMSMDBP_INBOX, &response.LogonType.store_mailbox.FolderIds[4]);
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, recipient, EMSMDBP_OUTBOX, &response.LogonType.store_mailbox.FolderIds[5]);
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, recipient, EMSMDBP_SENT_ITEMS, &response.LogonType.store_mailbox.FolderIds[6]);
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, recipient, EMSMDBP_DELETED_ITEMS, &response.LogonType.store_mailbox.FolderIds[7]);
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, recipient, EMSMDBP_COMMON_VIEWS, &response.LogonType.store_mailbox.FolderIds[8]);
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, recipient, EMSMDBP_SCHEDULE, &response.LogonType.store_mailbox.FolderIds[9]);
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, recipient, EMSMDBP_SEARCH, &response.LogonType.store_mailbox.FolderIds[10]);
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, recipient, EMSMDBP_VIEWS, &response.LogonType.store_mailbox.FolderIds[11]);
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, recipient, EMSMDBP_SHORTCUTS, &response.LogonType.store_mailbox.FolderIds[12]);

	/* Step 4. Set ResponseFlags */
	response.LogonType.store_mailbox.ResponseFlags = ResponseFlags_Reserved | ResponseFlags_OwnerRight | ResponseFlags_SendAsRight;

	/* Step 5. Retrieve MailboxGuid */
	retval = openchangedb_get_MailboxGuid(emsmdbp_ctx->oc_ctx, recipient, &response.LogonType.store_mailbox.MailboxGuid);

	/* Step 6. Retrieve mailbox replication information */
	retval = openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, recipient,
						 &response.LogonType.store_mailbox.ReplId,
						 &response.LogonType.store_mailbox.ReplGUID);

	/* Step 7. Set LogonTime both in openchange dispatcher database and reply */
	t = time(NULL);
	LogonTime = localtime(&t);
	response.LogonType.store_mailbox.LogonTime.Seconds = LogonTime->tm_sec;
	response.LogonType.store_mailbox.LogonTime.Minutes = LogonTime->tm_min;
	response.LogonType.store_mailbox.LogonTime.Hour = LogonTime->tm_hour;
	response.LogonType.store_mailbox.LogonTime.DayOfWeek = LogonTime->tm_wday;
	response.LogonType.store_mailbox.LogonTime.Day = LogonTime->tm_mday;
	response.LogonType.store_mailbox.LogonTime.Month = LogonTime->tm_mon + 1;
	response.LogonType.store_mailbox.LogonTime.Year = LogonTime->tm_year + 1900;

	/* Step 8. Retrieve GwartTime */
	unix_to_nt_time(&nttime, t);
	response.LogonType.store_mailbox.GwartTime = nttime - 1000000;

	/* Step 9. Set StoreState */
	response.LogonType.store_mailbox.StoreState = 0x0;

	mapi_repl->u.mapi_Logon = response;

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
	struct Logon_req	request;
	struct Logon_repl	response;

	request = mapi_req->u.mapi_Logon;
	response = mapi_repl->u.mapi_Logon;

	response.LogonFlags = request.LogonFlags;

	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, EMSMDBP_PF_ROOT, &(response.LogonType.store_pf.FolderIds[0]));
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, EMSMDBP_PF_IPMSUBTREE, &(response.LogonType.store_pf.FolderIds[1]));
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, EMSMDBP_PF_NONIPMSUBTREE, &(response.LogonType.store_pf.FolderIds[2]));
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, EMSMDBP_PF_EFORMSREGISTRY, &(response.LogonType.store_pf.FolderIds[3]));
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, EMSMDBP_PF_FREEBUSY, &(response.LogonType.store_pf.FolderIds[4]));
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, EMSMDBP_PF_OAB, &(response.LogonType.store_pf.FolderIds[5]));
	response.LogonType.store_pf.FolderIds[6] = 0x00000000000000000000; /* Eforms Registry */
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, EMSMDBP_PF_LOCALFREEBUSY, &(response.LogonType.store_pf.FolderIds[7]));
	openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, EMSMDBP_PF_LOCALOAB, &(response.LogonType.store_pf.FolderIds[8]));
	response.LogonType.store_pf.FolderIds[9] = 0x00000000000000000000; /* NNTP Article Index */
	response.LogonType.store_pf.FolderIds[10] = 0x00000000000000000000; /* Empty */
	response.LogonType.store_pf.FolderIds[11] = 0x00000000000000000000; /* Empty */
	response.LogonType.store_pf.FolderIds[12] = 0x00000000000000000000; /* Empty */

	openchangedb_get_PublicFolderReplica(emsmdbp_ctx->oc_ctx,
					     &(response.LogonType.store_pf.ReplId),
					     &(response.LogonType.store_pf.Guid));
	memset(&(response.LogonType.store_pf.PerUserGuid), 0, sizeof(struct GUID));

	mapi_repl->u.mapi_Logon = response;

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
	struct Logon_req		request;
	struct mapi_handles		*rec = NULL;
	struct emsmdbp_object		*object;
	bool				mailboxstore = true;

	DEBUG(4, ("exchange_emsmdb: [OXCSTOR] Logon (0xFE)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = mapi_req->u.mapi_Logon;

	/* Fill EcDoRpc_MAPI_REPL reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	if (request.LogonFlags & LogonPrivate) {
		retval = RopLogon_Mailbox(mem_ctx, emsmdbp_ctx, mapi_req, mapi_repl);
		mapi_repl->error_code = retval;
		*size += libmapiserver_RopLogon_size(mapi_req, mapi_repl);
	} else {
		/* retval = RopLogon_PublicFolder(mem_ctx, emsmdbp_ctx, mapi_req, mapi_repl); */
		mapi_repl->error_code = MAPI_E_LOGON_FAILED;
		/* mailboxstore = false; */
		*size += libmapiserver_RopLogon_size(mapi_req, mapi_repl);
	}

	if (!mapi_repl->error_code) {
		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, 0, &rec);
		object = emsmdbp_object_mailbox_init((TALLOC_CTX *)rec, emsmdbp_ctx, mapi_req, mailboxstore);
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
        struct mapistore_subscription_list *subscription_list, *subscription_holder;
        struct mapistore_subscription *subscription;
	enum MAPISTATUS		retval;
	uint32_t		handle;

	handle = handles[request->handle_idx];

	/* If we have notification's subscriptions attached to this handle, we
	   obviously remove them in order to avoid invoking them once all ROPs
	   are processed */
	subscription_list = emsmdbp_ctx->mstore_ctx->subscriptions;
	subscription_holder = subscription_list;
	while (subscription_holder) {
	        subscription = subscription_holder->subscription;
		  
		if (handle == subscription->handle)
		          DLIST_REMOVE(subscription_list, subscription_holder);


		subscription_holder = subscription_holder->next;
       	}

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
	if (MessageClass[0] && (MessageClass[len] == '.')) {
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
	
	/* Step 2. Verify MessageClass string */
	fid = mapi_req->u.mapi_SetReceiveFolder.fid;
	MessageClass = mapi_req->u.mapi_SetReceiveFolder.lpszMessageClass;
	if (!MessageClass || (strcmp(MessageClass, "") == 0)) {
		MessageClass="All";
	}
	if ((fid == 0x0) && (strcmp(MessageClass, "All"))) {
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
	retval = openchangedb_set_ReceiveFolder(mem_ctx, emsmdbp_ctx->oc_ctx, object->object.mailbox->owner_username,
						MessageClass, fid);
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

	DEBUG(4, ("exchange_emsmdb: [OXCSTOR] SetReceiveFolder (0x26)\n"));

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
	
	/* Step 2. Verify MessageClass string */
	MessageClass = mapi_req->u.mapi_GetReceiveFolder.MessageClass;
	if (!MessageClass || !strcmp(MessageClass, "")) {
		MessageClass="All";
	}
	if (! MessageClassIsValid(MessageClass) ) {
		return MAPI_E_INVALID_PARAMETER;
	}

	/* Step 3. Search for the specified MessageClass substring within user mailbox */
	retval = openchangedb_get_ReceiveFolder(mem_ctx, emsmdbp_ctx->oc_ctx, object->object.mailbox->owner_username,
						MessageClass, &mapi_repl->u.mapi_GetReceiveFolder.folder_id,
						&mapi_repl->u.mapi_GetReceiveFolder.MessageClass);
	OPENCHANGE_RETVAL_IF(retval, ecNoReceiveFolder, NULL);

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

	DEBUG(4, ("exchange_emsmdb: [OXCSTOR] GetReceiveFolder (0x27)\n"));

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
	uint16_t			req_repl_id;
	uint64_t			id;
	uint8_t				i;

	DEBUG(4, ("exchange_emsmdb: [OXCSTOR] LongTermIdFromId (0x43)\n"));

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

	if (emsmdbp_replid_to_guid(emsmdbp_ctx, req_repl_id, &response->LongTermId.DatabaseGuid)) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

	/* openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, NULL, &response->LongTermId.DatabaseGuid); */

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
	uint16_t			repl_id;
	uint64_t			fmid, base;
	uint8_t				i, ctr_byte;
	/* struct GUID			replica_guid; */

	DEBUG(4, ("exchange_emsmdb: [OXCSTOR] RopIdFromLongTermId (0x44)\n"));

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

	if (emsmdbp_guid_to_replid(emsmdbp_ctx, &request->LongTermId.DatabaseGuid, &repl_id)) {
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
	DEBUG(4, ("exchange_emsmdb: [OXCSTOR] GetPerUserLongTermIds (0x60) - valid stub\n"));

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
	DEBUG(4, ("exchange_emsmdb: [OXCSTOR] GetPerUserGuid (0x61) - stub\n"));

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
	DEBUG(4, ("exchange_emsmdb: [OXCSTOR] ReadPerUserInformation (0x63) - stub\n"));

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
	DEBUG(4, ("exchange_emsmdb: [OXCSTOR] GetStoreState (0x63) - stub\n"));

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
