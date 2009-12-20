/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2009.

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
#include <gen_ndr/ndr_exchange_c.h>
#include <core/error.h>
#include <param.h>


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
	struct tevent_context	*ev;

	if (!binding) {
		DEBUG(3, ("You must specify a ncacn binding string\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	ev = tevent_context_init(talloc_autofree_context());

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


/**
   \details Build the binding string and flags given profile and
   global options.

   \param mem_ctx pointer to the memory allocation context
   \param server string representing the server FQDN or IP address
   \param profile pointer to the MAPI profile structure

   \return valid allocated string on success, otherwise NULL
 */
static char *build_binding_string(TALLOC_CTX *mem_ctx, 
				  const char *server, 
				  struct mapi_profile *profile)
{
	char	*binding;

	/* Sanity Checks */
	if (!profile) return NULL;
	if (!server) return NULL;
	if (!global_mapi_ctx) return NULL;

	binding = talloc_asprintf(mem_ctx, "ncacn_ip_tcp:%s[", server);
	/* If dump-data option is enabled */
	if (global_mapi_ctx->dumpdata == true) {
		binding = talloc_strdup_append(binding, "print,");
	}
	/* If seal option is enabled in the profile */
	if (profile->seal == true) {
		binding = talloc_strdup_append(binding, "seal");
	}

	binding = talloc_strdup_append(binding, "]");

	return binding;
}

/**
   \details Returns the name of an NSPI server

   \param session pointer to the MAPI session context
   \param server the Exchange server address (IP or FQDN)
   \param userDN optional user mailbox DN

   \return a valid allocated string on success, otherwise NULL.

   \note The string returned can either be RfrGetNewDSA one on
   success, or a copy of the server's argument one on failure. If no
   server string is provided, NULL is returned.

   It is up to the developer to free the returned string when
   not needed anymore.
 */
_PUBLIC_ char *RfrGetNewDSA(struct mapi_session *session,
			    const char *server, 
			    const char *userDN)
{
	NTSTATUS		status;
	TALLOC_CTX		*mem_ctx;
	struct mapi_profile	*profile;
	struct RfrGetNewDSA	r;
	struct dcerpc_pipe	*pipe;
	char			*binding;
	char			*ppszServer = NULL;

	/* Sanity Checks */
	if (!global_mapi_ctx) return NULL;
	if (!global_mapi_ctx->session) return NULL;

	mem_ctx = talloc_named(NULL, 0, "RfrGetNewDSA");
	profile = session->profile;

	binding = build_binding_string(mem_ctx, server, profile);
	status = provider_rpc_connection(mem_ctx, &pipe, binding, profile->credentials, &ndr_table_exchange_ds_rfr, global_mapi_ctx->lp_ctx);
	talloc_free(binding);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return NULL;
	}


	r.in.ulFlags = 0x0;
	r.in.pUserDN = userDN ? userDN : "";
	r.in.ppszUnused = NULL;
	r.in.ppszServer = (const char **) &ppszServer;

	status = dcerpc_RfrGetNewDSA(pipe, mem_ctx, &r);
	if ((!NT_STATUS_IS_OK(status) || !ppszServer) && server) {
		ppszServer = talloc_strdup((TALLOC_CTX *)session, server);
	} else {
		ppszServer = talloc_steal((TALLOC_CTX *)session, ppszServer);
	}

	talloc_free(mem_ctx);

	return ppszServer;
}


/**
   \details Returns the FQDN of the NSPI server corresponding to a DN

   \param session pointer to the MAPI session context
   \param serverFQDN pointer to the server FQDN string (return value)

   \return MAPI_E_SUCCESS on success, otherwise a MAPI error and
   serverFQDN content set to NULL.
 */
_PUBLIC_ enum MAPISTATUS RfrGetFQDNFromLegacyDN(struct mapi_session *session,
						const char **serverFQDN)
{
	NTSTATUS			status;
	TALLOC_CTX			*mem_ctx;
	struct mapi_profile		*profile;
	struct RfrGetFQDNFromLegacyDN	r;
	struct dcerpc_pipe		*pipe;
	char				*binding;
	const char     			*ppszServerFQDN;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = (TALLOC_CTX *)session;
	profile = session->profile;
	*serverFQDN = NULL;

	binding = build_binding_string(mem_ctx, profile->server, profile);
	status = provider_rpc_connection(mem_ctx, &pipe, binding, profile->credentials, &ndr_table_exchange_ds_rfr, global_mapi_ctx->lp_ctx);
	talloc_free(binding);

	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, NULL);

	r.in.ulFlags = 0x0;
	r.in.cbMailboxServerDN = strlen(profile->homemdb) + 1;
	r.in.szMailboxServerDN = profile->homemdb;
	r.out.ppszServerFQDN = &ppszServerFQDN;

	status = dcerpc_RfrGetFQDNFromLegacyDN(pipe, mem_ctx, &r);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	
	if (ppszServerFQDN) {
		*serverFQDN = ppszServerFQDN;
	} else {
		*serverFQDN = NULL;
	}

	return MAPI_E_SUCCESS;
}

enum MAPISTATUS Logon(struct mapi_session *session,
		      struct mapi_provider *provider, 
		      enum PROVIDER_ID provider_id)
{
	NTSTATUS		status;
	TALLOC_CTX		*mem_ctx;
	struct dcerpc_pipe	*pipe;
	struct mapi_profile	*profile;
	char			*binding;
	char			*server;
	int			retval = 0;

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = (TALLOC_CTX *)provider;
	profile = session->profile;
	
	switch(provider_id) {
	case PROVIDER_ID_EMSMDB:
	emsmdb_retry:
		binding = build_binding_string(mem_ctx, profile->server, profile);
		status = provider_rpc_connection(mem_ctx, &pipe, binding, profile->credentials, &ndr_table_exchange_emsmdb, global_mapi_ctx->lp_ctx);
		talloc_free(binding);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_CONNECTION_REFUSED), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_HOST_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_PORT_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_LOGON_FAILED, NULL);
		provider->ctx = emsmdb_connect(mem_ctx, session, pipe, profile->credentials, &retval);
		if (retval == ecNotEncrypted) {
			profile->seal = true;
			retval = 0;
			goto emsmdb_retry;
		}
		OPENCHANGE_RETVAL_IF(!provider->ctx, MAPI_E_LOGON_FAILED, NULL);
		break;
	case PROVIDER_ID_NSPI:
		/* Call RfrGetNewDSA prior any NSPI call */
		server = RfrGetNewDSA(session, profile->server, profile->mailbox);
		binding = build_binding_string(mem_ctx, server, profile);
		talloc_free(server);
		status = provider_rpc_connection(mem_ctx, &pipe, binding, profile->credentials, &ndr_table_exchange_nsp, global_mapi_ctx->lp_ctx);
		talloc_free(binding);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_CONNECTION_REFUSED), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_HOST_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_PORT_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_LOGON_FAILED, NULL);
		provider->ctx = (void *)nspi_bind(provider, pipe, profile->credentials, 
						  profile->codepage, profile->language, profile->method);
		OPENCHANGE_RETVAL_IF(!provider->ctx, MAPI_E_LOGON_FAILED, NULL);
		break;
	default:
		OPENCHANGE_RETVAL_ERR(MAPI_E_NOT_FOUND, NULL);
		break;
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Logoff an Exchange store

   This function uninitializes the MAPI session associated to the
   object.

   \param obj_store pointer to the store object

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS Logoff(mapi_object_t *obj_store)
{
	struct mapi_session	*session;
	struct mapi_session	*el;
	bool			found = false;

	/* Sanity checks */
	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	for (el = global_mapi_ctx->session; el; el = el->next) {
		if (session == el) {
			found = true;
			mapi_object_release(obj_store);
			DLIST_REMOVE(global_mapi_ctx->session, el);
			break;
		}
	}

	return (found == true) ? MAPI_E_SUCCESS : MAPI_E_NOT_FOUND;
}


/**
   \details Retrieve a free logon identifier within the session

   \param session pointer to the MAPI session context
   \param logon_id pointer to the logon identifier the function
   returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI eorr
 */
enum MAPISTATUS GetNewLogonId(struct mapi_session *session, uint8_t *logon_id)
{
	int	i = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!logon_id, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; i < 255; i++) {
		if (!session->logon_ids[i]) {
			session->logon_ids[i] = 1;
			*logon_id = i;
			return MAPI_E_SUCCESS;
		}
	}

	return MAPI_E_NOT_FOUND;
}


/**
   \details Initialize the notification subsystem

   This function initializes the notification subsystem, binds a local
   UDP port to receive Exchange (server side) notifications and
   configures the server to send notifications on this port.

   \param ulEventMask the mask of events to provide notifications for.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa Subscribe, Unsubscribe, MonitorNotification, GetLastError 
*/
_PUBLIC_ enum MAPISTATUS RegisterNotification(uint16_t ulEventMask)
{
	NTSTATUS		status;
	struct emsmdb_context	*emsmdb;
	struct mapi_session	*session;
	TALLOC_CTX		*mem_ctx;
	struct NOTIFKEY		*lpKey;
	static uint8_t		rand = 0;
	static uint8_t		attempt = 0;
	
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx->session, MAPI_E_SESSION_LIMIT, NULL);

	session = (struct mapi_session *)global_mapi_ctx->session;
	OPENCHANGE_RETVAL_IF(!session->emsmdb, MAPI_E_SESSION_LIMIT, NULL);

	emsmdb = (struct emsmdb_context *)global_mapi_ctx->session->emsmdb->ctx;
	OPENCHANGE_RETVAL_IF(!emsmdb, MAPI_E_SESSION_LIMIT, NULL);

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
