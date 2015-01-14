/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2011.

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
#include "libmapi/mapicode.h"
#include "libmapi/libmapi_private.h"
#include "gen_ndr/ndr_exchange.h"
#include "gen_ndr/ndr_exchange_c.h"
#include <core/error.h>
#include <param.h>

#define TEVENT_DEPRECATED 1
#include <tevent.h>

/**
   \file IMSProvider.c

   \brief Provider operations
*/

/*
 * Log MAPI to one instance of a message store provider
 */

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
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

	ev = tevent_context_init(parent_ctx);
	tevent_loop_allow_nesting(ev);

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
#pragma GCC diagnostic warning "-Wdeprecated-declarations"


/**
   \details Build the binding string and flags given profile and
   global options.

   \param mapi_ctx pointer to the MAPI context
   \param mem_ctx pointer to the memory allocation context
   \param server string representing the server FQDN or IP address
   \param profile pointer to the MAPI profile structure

   \return valid allocated string on success, otherwise NULL
 */
static char *build_binding_string(struct mapi_context *mapi_ctx,
				  TALLOC_CTX *mem_ctx,
				  const char *rpcserver,
				  struct mapi_profile *profile)
{
	char	*binding;

	/* Sanity Checks */
	if (!profile) return NULL;
	if (!rpcserver) return NULL;
	if (!mapi_ctx) return NULL;

	if (profile->roh == true) {
		binding = talloc_asprintf(mem_ctx, "ncacn_http:%s[rpcproxy=%s:%d,",
				rpcserver, profile->roh_rpc_proxy_server,
				profile->roh_rpc_proxy_port);
		if (!binding) return NULL;
		if (profile->roh_tls == true) {
			binding = talloc_strdup_append(binding, "tls,");
			if (!binding) return NULL;
		}
	} else {
		binding = talloc_asprintf(mem_ctx, "ncacn_ip_tcp:%s[", rpcserver);
		if (!binding) return NULL;
	}

	/* If dump-data option is enabled */
	if (mapi_ctx->dumpdata == true) {
		binding = talloc_strdup_append(binding, "print,");
		if (!binding) return NULL;
	}
	/* If seal option is enabled in the profile */
	if (profile->seal == true) {
		binding = talloc_strdup_append(binding, "seal,");
		if (!binding) return NULL;
	}
	/* If localaddress parameter is available in the profile */
	if (profile->localaddr) {
		binding = talloc_asprintf_append(binding, "localaddress=%s,", profile->localaddr);
		if (!binding) return NULL;
	}

	binding = talloc_strdup_append(binding, "]");
	if (!binding) return NULL;

	return binding;
}

/**
   \details Returns the name of an NSPI server

   \param mapi_ctx pointer to the MAPI context
   \param session pointer to the MAPI session context
   \param server the Exchange server address (IP or FQDN)
   \param userDN optional user mailbox DN
   \param dsa pointer to a new dsa (return value), containing
      a valid allocated string on success, otherwise NULL

   \return MAPI_E_SUCCESS on success, otherwise a MAPI error and
   serverFQDN content set to NULL.

   \note The string returned can either be RfrGetNewDSA one on
   success, or a copy of the server's argument one on failure. If no
   server string is provided, NULL is returned.

   It is up to the developer to free the returned string when
   not needed anymore.
 */
_PUBLIC_ enum MAPISTATUS RfrGetNewDSA(struct mapi_context *mapi_ctx,
			    struct mapi_session *session,
			    const char *server,
			    const char *userDN,
			    char **dsa)
{
	NTSTATUS		status;
	TALLOC_CTX		*mem_ctx;
	struct mapi_profile	*profile;
	struct RfrGetNewDSA	r;
	struct dcerpc_pipe	*pipe;
	char			*binding;
	char			*ppszServer = NULL;

	/* Sanity Checks */
	if (!mapi_ctx) return MAPI_E_NOT_INITIALIZED;
	if (!mapi_ctx->session) return MAPI_E_NOT_INITIALIZED;

	mem_ctx = talloc_named(session, 0, "RfrGetNewDSA");
	profile = session->profile;

	binding = build_binding_string(mapi_ctx, mem_ctx, server, profile);
	status = provider_rpc_connection(mem_ctx, &pipe, binding, profile->credentials, &ndr_table_exchange_ds_rfr, mapi_ctx->lp_ctx);
	talloc_free(binding);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return MAPI_E_NETWORK_ERROR;
	}


	r.in.ulFlags = 0x0;
	r.in.pUserDN = userDN ? userDN : "";
	r.in.ppszUnused = NULL;
	r.in.ppszServer = (const char **) &ppszServer;

	status = dcerpc_RfrGetNewDSA_r(pipe->binding_handle, mem_ctx, &r);
	if ((!NT_STATUS_IS_OK(status) || !r.out.ppszServer || !*r.out.ppszServer) && server) {
		ppszServer = talloc_strdup((TALLOC_CTX *)session, server);
	} else {
		ppszServer = (char *)talloc_steal((TALLOC_CTX *)session, *r.out.ppszServer);
	}

	talloc_free(mem_ctx);

	*dsa = ppszServer;

	return MAPI_E_SUCCESS;
}


/**
   \details Returns the FQDN of the NSPI server corresponding to a DN

   \param mapi_ctx pointer to the MAPI context
   \param session pointer to the MAPI session context
   \param serverFQDN pointer to the server FQDN string (return value)

   \return MAPI_E_SUCCESS on success, otherwise a MAPI error and
   serverFQDN content set to NULL.
 */
_PUBLIC_ enum MAPISTATUS RfrGetFQDNFromLegacyDN(struct mapi_context *mapi_ctx, struct mapi_session *session,
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
	OPENCHANGE_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = (TALLOC_CTX *)session;
	profile = session->profile;
	*serverFQDN = NULL;

	binding = build_binding_string(mapi_ctx, mem_ctx, profile->server, profile);
	status = provider_rpc_connection(mem_ctx, &pipe, binding, profile->credentials, &ndr_table_exchange_ds_rfr, mapi_ctx->lp_ctx);
	talloc_free(binding);

	OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_CONNECTION_REFUSED), MAPI_E_NETWORK_ERROR, NULL);
	OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_HOST_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
	OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_PORT_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
	OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT), MAPI_E_NETWORK_ERROR, NULL);
	OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND), MAPI_E_NETWORK_ERROR, NULL);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, NULL);

	r.in.ulFlags = 0x0;
	r.in.cbMailboxServerDN = strlen(profile->homemdb) + 1;
	r.in.szMailboxServerDN = profile->homemdb;
	r.out.ppszServerFQDN = &ppszServerFQDN;

	status = dcerpc_RfrGetFQDNFromLegacyDN_r(pipe->binding_handle, mem_ctx, &r);
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
	struct mapi_context	*mapi_ctx;
	NTSTATUS		status;
	TALLOC_CTX		*mem_ctx;
	struct dcerpc_pipe	*pipe;
	struct mapi_profile	*profile;
	char			*binding;
	char			*server;
	int			retval = 0;
	enum MAPISTATUS		mapistatus;

	/*Sanity checks */
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = (TALLOC_CTX *)provider;
	profile = session->profile;
	mapi_ctx = session->mapi_ctx;

	switch(provider_id) {
	case PROVIDER_ID_EMSMDB:
	emsmdb_retry:
		if (profile->server_name != NULL) {
			binding = build_binding_string(mapi_ctx, mem_ctx, profile->server_name, profile);
		} else {
			binding = build_binding_string(mapi_ctx, mem_ctx, profile->server, profile);
		}
		status = provider_rpc_connection(mem_ctx, &pipe, binding, profile->credentials, &ndr_table_exchange_emsmdb, mapi_ctx->lp_ctx);
		talloc_free(binding);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_CONNECTION_REFUSED), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_HOST_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_PORT_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_LOGON_FAILED, NULL);
		switch (profile->exchange_version) {
		case 0x0:
			provider->ctx = emsmdb_connect(mem_ctx, session, pipe, profile->credentials, &retval);
			break;
		case 0x1:
		case 0x2:
			provider->ctx = emsmdb_connect_ex(mem_ctx, session, pipe, profile->credentials, &retval);
			break;
		}
		if (retval == ecNotEncrypted) {
			profile->seal = true;
			retval = 0;
			goto emsmdb_retry;
		}
		OPENCHANGE_RETVAL_IF(!provider->ctx, MAPI_E_LOGON_FAILED, NULL);

		if (server_version_at_least((struct emsmdb_context *)provider->ctx, 8, 0, 835, 0)){
		  struct emsmdb_context *prov_ctx = (struct emsmdb_context *)provider->ctx;
			status = dcerpc_secondary_context(pipe, &(prov_ctx->async_rpc_connection), &ndr_table_exchange_async_emsmdb);
			OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_CONNECTION_REFUSED), MAPI_E_NETWORK_ERROR, NULL);
			OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_HOST_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
			OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_PORT_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
			OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT), MAPI_E_NETWORK_ERROR, NULL);
			OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND), MAPI_E_NETWORK_ERROR, NULL);
			OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_LOGON_FAILED, NULL);
			mapistatus = emsmdb_async_connect(prov_ctx);
			OPENCHANGE_RETVAL_IF(mapistatus, mapistatus, NULL);
		}

		break;
	case PROVIDER_ID_NSPI:
		/* Call RfrGetNewDSA prior any NSPI call */
		mapistatus = RfrGetNewDSA(mapi_ctx, session, profile->server, profile->mailbox, &server);
		OPENCHANGE_RETVAL_IF(mapistatus != MAPI_E_SUCCESS, mapistatus, NULL);
		binding = build_binding_string(mapi_ctx, mem_ctx, server, profile);
		talloc_free(server);
		status = provider_rpc_connection(mem_ctx, &pipe, binding, profile->credentials, &ndr_table_exchange_nsp, mapi_ctx->lp_ctx);
		talloc_free(binding);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_CONNECTION_REFUSED), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_HOST_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_PORT_UNREACHABLE), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT), MAPI_E_NETWORK_ERROR, NULL);
		OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND), MAPI_E_NETWORK_ERROR, NULL);
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
	struct mapi_context	*mapi_ctx;
	struct mapi_session	*session;
	struct mapi_session	*el;
	bool			found = false;

	/* Sanity checks */
	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = session->mapi_ctx;
	OPENCHANGE_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	for (el = mapi_ctx->session; el; el = el->next) {
		if (session == el) {
			found = true;
			mapi_object_release(obj_store);
			DLIST_REMOVE(mapi_ctx->session, el);
			MAPIFreeBuffer(session);
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

   \param session the session context to register for notifications on.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa RegisterAsyncNotification, Subscribe, Unsubscribe, MonitorNotification, GetLastError
*/
_PUBLIC_ enum MAPISTATUS RegisterNotification(struct mapi_session *session)
{
	NTSTATUS		status;
	struct mapi_context	*mapi_ctx;
	struct emsmdb_context	*emsmdb;
	TALLOC_CTX		*mem_ctx;
	struct NOTIFKEY		*lpKey;
	static uint8_t		rand = 0;
	static uint8_t		attempt = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!session->emsmdb, MAPI_E_SESSION_LIMIT, NULL);

	emsmdb = (struct emsmdb_context *)session->emsmdb->ctx;
	OPENCHANGE_RETVAL_IF(!emsmdb, MAPI_E_SESSION_LIMIT, NULL);

	mapi_ctx = session->mapi_ctx;
	OPENCHANGE_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = emsmdb->mem_ctx;

	/* bind local udp port */
	session->notify_ctx = emsmdb_bind_notification(mapi_ctx, mem_ctx);
	if (!session->notify_ctx) return MAPI_E_CANCEL;

	/* tell exchange where to send notifications */
	lpKey = talloc_zero(mem_ctx, struct NOTIFKEY);
	lpKey->cb = 8;
	lpKey->ab = talloc_array((TALLOC_CTX *)lpKey, uint8_t, lpKey->cb);
	memcpy(lpKey->ab, "libmapi", 7);
retry:
	lpKey->ab[7] = rand;

	status = emsmdb_register_notification(session, lpKey);
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

	attempt = 0;
	talloc_free(lpKey);
	return MAPI_E_SUCCESS;
}

/**
   \details Create an asynchronous notification

   This function initializes the notification subsystem and configures the
   server to send notifications. Note that this call will block.

   \param session the session context to register for notifications on.
   \param resultFlag the result of the operation (true if there was anything returned)

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa RegisterNotification
*/
_PUBLIC_ enum MAPISTATUS RegisterAsyncNotification(struct mapi_session *session, uint32_t *resultFlag)
{
	enum MAPISTATUS		mapistatus;
	struct emsmdb_context	*emsmdb;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!session->emsmdb, MAPI_E_SESSION_LIMIT, NULL);

	emsmdb = (struct emsmdb_context *)session->emsmdb->ctx;

	session->notify_ctx = talloc_zero(emsmdb->mem_ctx, struct mapi_notify_ctx);

	session->notify_ctx->notifications = talloc_zero((TALLOC_CTX *)session->notify_ctx, struct notifications);
	session->notify_ctx->notifications->prev = NULL;
	session->notify_ctx->notifications->next = NULL;

	mapistatus = emsmdb_async_waitex(emsmdb, 0, resultFlag);
	OPENCHANGE_RETVAL_IF(mapistatus, mapistatus, NULL);

	return MAPI_E_SUCCESS;
}

