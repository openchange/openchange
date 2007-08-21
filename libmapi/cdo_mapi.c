/*
 * OpenChange MAPI implementation.
 *
 *  Copyright (C) Julien Kerihuel  2007.
 *  Copyright (C) Fabien Le Mentec 2007.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <libmapi/dlinklist.h>
#include <param.h>
#include <ldb_errors.h>
#include <ldb.h>

struct mapi_ctx *global_mapi_ctx = NULL;

/**
 * Open providers stored in the profile and return a pointer on a IMAPISession object.
 */

_PUBLIC_ enum MAPISTATUS MapiLogonEx(struct mapi_session **session, 
				     const char *profname, const char *password)
{
	struct mapi_session	*tmp_session = NULL;
	enum MAPISTATUS		retval;

	retval = MapiLogonProvider(&tmp_session, profname, password, PROVIDER_ID_NSPI);
	if (retval != MAPI_E_SUCCESS) return retval;

	retval = MapiLogonProvider(&tmp_session, profname, password, PROVIDER_ID_EMSMDB);
	if (retval != MAPI_E_SUCCESS) return retval;

	*session = tmp_session;

	return MAPI_E_SUCCESS;
}

/**
 * Initialize a session on the specified provider
 */

_PUBLIC_ enum MAPISTATUS MapiLogonProvider(struct mapi_session **session,
					   const char *profname, const char *password,
					   enum PROVIDER_ID provider)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		status;
	struct mapi_provider	*provider_emsmdb;
	struct mapi_provider	*provider_nspi;
	struct mapi_profile	*profile;


	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	/* allocate session if it doesn't already exist */
	if (!*session || !global_mapi_ctx->session) {
		*session = talloc_zero(global_mapi_ctx->mem_ctx, struct mapi_session);
		MAPI_RETVAL_IF(!(*session), MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
		global_mapi_ctx->session = *session;
	}
	
	mem_ctx = (TALLOC_CTX *) global_mapi_ctx->session;

	/* Open the profile */
	if (!global_mapi_ctx->session->profile) {
		profile = talloc_zero(mem_ctx, struct mapi_profile);
		MAPI_RETVAL_IF(!profile, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);

		status = OpenProfile(profile, profname, password);
		if (status != MAPI_E_SUCCESS) return status;
		
		status = LoadProfile(profile);
		if (status != MAPI_E_SUCCESS) return status;

		global_mapi_ctx->session->profile = profile;
	}

	switch (provider) {
	case PROVIDER_ID_EMSMDB:
		provider_emsmdb = talloc_zero(mem_ctx, struct mapi_provider);
		MAPI_RETVAL_IF(!provider_emsmdb, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
		talloc_set_destructor((void *)provider_emsmdb, (int (*)(void *))emsmdb_disconnect_dtor);
		status = Logon(provider_emsmdb, PROVIDER_ID_EMSMDB);
		if (status) return status;
		global_mapi_ctx->session->emsmdb = provider_emsmdb;
		break;
	case PROVIDER_ID_NSPI:
		provider_nspi = talloc_zero(mem_ctx, struct mapi_provider);
		MAPI_RETVAL_IF(!provider_nspi, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
		talloc_set_destructor((void *)provider_nspi, (int (*)(void *))nspi_disconnect_dtor);
		status = Logon(provider_nspi, PROVIDER_ID_NSPI);
		if (status) return status;
		global_mapi_ctx->session->nspi = provider_nspi;
		break;
	default:
		MAPI_RETVAL_IF(1, MAPI_E_NO_SUPPORT, NULL);
		break;
	}

	return MAPI_E_SUCCESS;
}

/**
 * Initialize mapi context global structure
 */

_PUBLIC_ enum MAPISTATUS MAPIInitialize(const char *profiledb)
{
	enum MAPISTATUS	status;
	TALLOC_CTX	*mem_ctx;

	/* Set the initial errno value for GetLastError */
	errno = 0;

	MAPI_RETVAL_IF(global_mapi_ctx, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!profiledb, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_init("MAPIInitialize");
	MAPI_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);

	/* global context */
	global_mapi_ctx = talloc_zero(mem_ctx, struct mapi_ctx);
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);
	global_mapi_ctx->mem_ctx = mem_ctx;
	global_mapi_ctx->dumpdata = false;

	/* profile store */
	status = OpenProfileStore(mem_ctx, &global_mapi_ctx->ldb_ctx, profiledb);
	MAPI_RETVAL_IF(status, status, mem_ctx);

	lp_load();
	dcerpc_init();

	errno = 0;
	return MAPI_E_SUCCESS;
}



/**
 * Uninitialize MAPI context and destroy recursively the whole mapi
 * session and associated objects hierarchy
 */

_PUBLIC_ void MAPIUninitialize(void)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_session	*session;

	if (!global_mapi_ctx) return;

	session = global_mapi_ctx->session;
	if (session && session->notify_ctx && session->notify_ctx->fd != -1) {
		DEBUG(3, ("emsmdb_disconnect_dtor: unbind udp\n"));
		shutdown(session->notify_ctx->fd, SHUT_RDWR);
		close(session->notify_ctx->fd);
	}
	
	mem_ctx = global_mapi_ctx->mem_ctx;
	talloc_free(mem_ctx);
	global_mapi_ctx = NULL;
}
