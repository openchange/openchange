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
   \file ocxstor.c

   \brief Server-side store objects routines and Rops
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"

#include <string.h>


/**
   \details Logs on a private mailbox

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
   \details EcDoRpc Logon (0xFE) Rop. This operation logs on to a
   private mailbox or public folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param request pointer to the Logon EcDoRpc_MAPI_REQ structure
   \param response pointer to the Logon EcDoRpc_MAPI_REPL structure
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
		*size = libmapiserver_RopLogon_size(mapi_req, mapi_repl);
	} else {
		DEBUG(4, ("exchange_emsmdb: [OXCSTOR] Logon on Public Folders not implemented\n"));
		retval = MAPI_E_NO_SUPPORT;
		mapi_repl->error_code = retval;
		*size = libmapiserver_RopLogon_size(mapi_req, NULL);
	}

	if (!mapi_repl->error_code) {
		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, 0, &rec);
		retval = mapi_handles_set_systemfolder(rec, 0);

		object = emsmdbp_object_mailbox_init((TALLOC_CTX *)rec, emsmdbp_ctx, mapi_req);
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
	enum MAPISTATUS		retval;
	uint32_t		handle;

	handle = handles[request->handle_idx];
	retval = mapi_handles_delete(emsmdbp_ctx->handles_ctx, handle);
	OPENCHANGE_RETVAL_IF(retval && retval != MAPI_E_NOT_FOUND, retval, NULL);

	return MAPI_E_SUCCESS;
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
	int			i;

	/* Step 1. Ensure the referring MAPI handle is mailbox one */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	retval = mapi_handles_get_private_data(rec, (void **)&private_data);
	object = (struct emsmdbp_object *) private_data;
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);
	OPENCHANGE_RETVAL_IF(object->type != EMSMDBP_OBJECT_MAILBOX, MAPI_E_NO_SUPPORT, NULL);
	
	/* Step 2. Test Message class string according to [MS-OXCSTOR] section 2.2.1.2.1.1 */
	MessageClass = mapi_req->u.mapi_GetReceiveFolder.MessageClass;
	if (!MessageClass || !strcmp(MessageClass, "")) {
		MessageClass="All";
	}

	OPENCHANGE_RETVAL_IF(strlen(MessageClass) + 1 > 255, MAPI_E_INVALID_PARAMETER, NULL);
	for (i = 0; MessageClass[i]; i++) {
		OPENCHANGE_RETVAL_IF((MessageClass[i] < 32) || (MessageClass[i] > 126), 
				     MAPI_E_INVALID_PARAMETER, NULL);

		OPENCHANGE_RETVAL_IF(MessageClass[i] == '.' && MessageClass[i + 1] && MessageClass[i + 1] == '.',
				     MAPI_E_INVALID_PARAMETER, NULL);
	}
	OPENCHANGE_RETVAL_IF(MessageClass[0] && MessageClass[0] == '.', MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(MessageClass[0] && MessageClass[strlen(MessageClass)] == '.', MAPI_E_INVALID_PARAMETER, NULL);

	
	/* Step 3. Search for the specified MessageClass substring within user mailbox */
	retval = openchangedb_get_ReceiveFolder(mem_ctx, emsmdbp_ctx->oc_ctx, object->object.mailbox->owner_Name,
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

	*size = libmapiserver_RopGetReceiveFolder_size(mapi_repl);
	
	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return retval;
}
