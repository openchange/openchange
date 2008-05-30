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
#include <libmapi/mapicode.h>
#include <libmapi/proto_private.h>
#include <gen_ndr/ndr_exchange.h>
#include <core/error.h>
#include <param.h>
#include <credentials.h>


/**
   \file IMSProvider.c

   \brief Provider operations
*/


/*
 * Log MAPI to one instance of a message store provider
 */

static NTSTATUS provider_rpc_connection(TALLOC_CTX *parent_ctx, 
					struct dcerpc_pipe **p, 
					const char *binding,
					struct cli_credentials *credentials,
					const struct ndr_interface_table *table,
					struct loadparm_context *lp_ctx)
{
	NTSTATUS		status;
	struct event_context	*ev;

	if (!binding) {
		DEBUG(3, ("You must specify a ncacn binding string\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	ev = event_context_init(talloc_autofree_context());

	status = dcerpc_pipe_connect(parent_ctx, 
				     p, binding, table,
				     credentials, ev, lp_ctx); 

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("Failed to connect to remote server: %s %s\n", 
			  binding, nt_errstr(status)));
	}

	/* dcerpc_pipe_connect set errno, we have to unset it */
	errno = 0;
	return status;
}

enum MAPISTATUS Logon(struct mapi_provider *provider, enum PROVIDER_ID provider_id)
{
	NTSTATUS		status;
	TALLOC_CTX		*mem_ctx;
	struct dcerpc_pipe	*pipe;
	struct mapi_profile	*profile;
	char			*binding;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!global_mapi_ctx->session, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = (TALLOC_CTX *)provider;
	profile = global_mapi_ctx->session->profile;
	
	if (global_mapi_ctx->dumpdata == false) {
		binding = talloc_asprintf(mem_ctx, "ncacn_ip_tcp:%s", profile->server);
	} else {
		binding = talloc_asprintf(mem_ctx, "ncacn_ip_tcp:%s[print]", profile->server);
	}

	switch(provider_id) {
	case PROVIDER_ID_EMSMDB:
		status = provider_rpc_connection(mem_ctx, &pipe, binding, profile->credentials, &ndr_table_exchange_emsmdb, global_mapi_ctx->lp_ctx);
		talloc_free(binding);
		MAPI_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_CONNECTION_REFUSED), MAPI_E_NETWORK_ERROR, NULL);
		MAPI_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_HOST_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
		MAPI_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_PORT_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
		MAPI_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT), MAPI_E_NETWORK_ERROR, NULL);
		MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_LOGON_FAILED, NULL);
		provider->ctx = emsmdb_connect(mem_ctx, pipe, profile->credentials);
		MAPI_RETVAL_IF(!provider->ctx, MAPI_E_LOGON_FAILED, NULL);
		break;
	case PROVIDER_ID_NSPI:
		status = provider_rpc_connection(mem_ctx, &pipe, binding, profile->credentials, &ndr_table_exchange_nsp, global_mapi_ctx->lp_ctx);
		talloc_free(binding);
		MAPI_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_CONNECTION_REFUSED), MAPI_E_NETWORK_ERROR, NULL);
		MAPI_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_HOST_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
		MAPI_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_PORT_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
		MAPI_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT), MAPI_E_NETWORK_ERROR, NULL);
		MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_LOGON_FAILED, NULL);
		provider->ctx = (void *)nspi_bind(provider, pipe, profile->credentials, 
						  profile->codepage, profile->language, profile->method);
		MAPI_RETVAL_IF(!provider->ctx, MAPI_E_LOGON_FAILED, NULL);
		break;
	default:
		MAPI_RETVAL_IF("Logon", MAPI_E_NOT_FOUND, NULL);
		break;
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Initialize the notification subsystem

   This function initializes the notification subsystem, binds a local
   UDP port to receive Exchange (server side) notifications and
   configures the server to send notifications on this port.

   \param ulEventMask the mask of events to provide notifications
   for. This should be set to 0 for the moment.

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa Subscribe, Unsubscribe, MonitorNotification, GetLastError 
*/
_PUBLIC_ enum MAPISTATUS RegisterNotification(uint32_t ulEventMask)
{
	NTSTATUS		status;
	struct emsmdb_context	*emsmdb;
	struct mapi_session	*session;
	TALLOC_CTX		*mem_ctx;
	struct NOTIFKEY		*lpKey;
	static uint8_t		rand = 0;
	static uint8_t		attempt = 0;
	
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!global_mapi_ctx->session, MAPI_E_SESSION_LIMIT, NULL);

	session = (struct mapi_session *)global_mapi_ctx->session;
	MAPI_RETVAL_IF(!session->emsmdb, MAPI_E_SESSION_LIMIT, NULL);

	emsmdb = (struct emsmdb_context *)global_mapi_ctx->session->emsmdb->ctx;
	MAPI_RETVAL_IF(!emsmdb, MAPI_E_SESSION_LIMIT, NULL);

	mem_ctx = emsmdb->mem_ctx;

	/* bind local udp port */
	session->notify_ctx = emsmdb_bind_notification(mem_ctx);
	if (!session->notify_ctx) return MAPI_E_CANCEL;

	/* tell exchange where to send notifications */
	lpKey = talloc_zero(mem_ctx, struct NOTIFKEY);
	lpKey->cb = 8;
	lpKey->ab = talloc_array((TALLOC_CTX *)lpKey, uint8_t, lpKey->cb);
	memcpy(lpKey->ab, "libmapi", 7);
retry:
	lpKey->ab[7] = rand;

	status = emsmdb_register_notification(lpKey, ulEventMask);
	if (!NT_STATUS_IS_OK(status)) {
		if (attempt < 5) {
			rand++;
			attempt++;
			errno = 0;
			goto retry;
		} else {
			talloc_free(lpKey);
			return MAPI_E_CALL_FAILED;
		}
	}
	talloc_free(lpKey);
	return MAPI_E_SUCCESS;
}
