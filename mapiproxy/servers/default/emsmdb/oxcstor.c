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
	response.LogonFlags = request.LogonFlags & LogonPrivate;

	/* Step 3. Build FolderIds list */
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, recipient, EMSMDBP_NON_IPM_SUBTREE, &response.LogonType.store_mailbox.FolderIds[0]);
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
   \details EcDoRpc Logon (0xFE) Rop. This operations logs on to a
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
					  struct EcDoRpc_MAPI_REQ *request,
					  struct EcDoRpc_MAPI_REPL *response,
					  uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;

	DEBUG(4, ("exchange_emsmdb: [OXCSTOR] Logon (0xFE)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!request, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!response, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	/* Fill EcDoRpc_MAPI_REPL reply */
	response->opnum = request->opnum;
	response->handle_idx = request->handle_idx;

	switch (request->u.mapi_Logon.LogonFlags) {
	case LogonPrivate:
		retval = RopLogon_Mailbox(mem_ctx, emsmdbp_ctx, request, response);
		response->error_code = retval;
		*size = libmapiserver_RopLogon_size(request, response);
		break;
	default:
		DEBUG(4, ("exchange_emsmdb: [OXCSTOR] Logon on Public Folders not implemented\n"));
		retval = MAPI_E_NO_SUPPORT;
		response->error_code = retval;
		*size = libmapiserver_RopLogon_size(request, NULL);
		break;
	}

	/* FIXME: Implement handles management API and make a call here */
	if (!response->error_code) {
		handles[response->handle_idx] = 0x42;
	}

	return retval;
}
