/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2008.
   Copyright (C) Fabien Le Mentec 2007.

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
#include <libmapi/dlinklist.h>
#include <param.h>
#include <ldb_errors.h>
#include <ldb.h>

struct mapi_ctx *global_mapi_ctx = NULL;


/**
   \file cdo_mapi.c

   \brief MAPI subsystem related operations
*/


/**
   \details Create a full MAPI session
 
   Open providers stored in the profile and return a pointer on a
   IMAPISession object.

   \param session pointer to a pointer to a MAPI session object
   \param profname profile name to use
   \param password password to use for the profile

   password should be set to NULL if the password has been stored in
   the profile.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_NOT_ENOUGH_RESOURCES: MAPI subsystem failed to allocate
     the necessary resources to operate properly
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa MAPIInitialize, OpenProfile, MapiLogonProvider
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
   \details Initialize a session on the specified provider

   \param session pointer to a pointer to a MAPI session object
   \param profname profile name
   \param password profile password
   \param provider provider we want to establish a connection on

   password should be set to NULL if the password has been stored in
   the profile.

   Supported providers are:
   - PROVIDER_ID_NSPI: Address Book provider
   - PROVIDER_ID_EMSMDB: MAPI Store provider

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_NOT_ENOUGH_RESOURCES: MAPI subsystem failed to allocate
     the necessary resources to operate properly
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa MapiLogonEx, OpenProfile, LoadProfile
*/
_PUBLIC_ enum MAPISTATUS MapiLogonProvider(struct mapi_session **session,
					   const char *profname, 
					   const char *password,
					   enum PROVIDER_ID provider)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct mapi_provider	*provider_emsmdb;
	struct mapi_provider	*provider_nspi;
	struct mapi_profile	*profile;
	struct mapi_session	*el;
	bool			found = false;
	bool			exist = false;


	/* Sanity checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);

	/* If no MAPI session has already been created */
	if (!global_mapi_ctx->session) {
		global_mapi_ctx->session = talloc_zero(global_mapi_ctx->mem_ctx, struct mapi_session);
	}
	
	/* If the session doesn't exist, create a new one */
	if (!*session) {
		el = talloc_zero((TALLOC_CTX *)global_mapi_ctx->session, struct mapi_session);
		MAPI_RETVAL_IF(!el, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
	} else {
		/* Lookup the session within the chained list */
		for (el = global_mapi_ctx->session; el; el = el->next) {
			if (*session == el) {
				found = true;
				break;
			}
		}
		MAPI_RETVAL_IF(found == false, MAPI_E_NOT_FOUND, NULL);
		exist = true;
	}

	mem_ctx = (TALLOC_CTX *) el;

	/* Open the profile */
	if (!el->profile) {
		profile = talloc_zero(mem_ctx, struct mapi_profile);
		MAPI_RETVAL_IF(!profile, MAPI_E_NOT_ENOUGH_RESOURCES, el);

		retval = OpenProfile(profile, profname, password);
		MAPI_RETVAL_IF(retval, retval, el);
		
		retval = LoadProfile(profile);
		MAPI_RETVAL_IF(retval, retval, el);

		el->profile = profile;
	}

	switch (provider) {
	case PROVIDER_ID_EMSMDB:
		provider_emsmdb = talloc_zero(mem_ctx, struct mapi_provider);
		MAPI_RETVAL_IF(!provider_emsmdb, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
		talloc_set_destructor((void *)provider_emsmdb, (int (*)(void *))emsmdb_disconnect_dtor);
		retval = Logon(el, provider_emsmdb, PROVIDER_ID_EMSMDB);
		if (retval) return retval;
		el->emsmdb = provider_emsmdb;
		break;
	case PROVIDER_ID_NSPI:
		provider_nspi = talloc_zero(mem_ctx, struct mapi_provider);
		MAPI_RETVAL_IF(!provider_nspi, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
		talloc_set_destructor((void *)provider_nspi, (int (*)(void *))nspi_disconnect_dtor);
		retval = Logon(el, provider_nspi, PROVIDER_ID_NSPI);
		if (retval) return retval;
		el->nspi = provider_nspi;
		break;
	default:
		MAPI_RETVAL_IF(1, MAPI_E_NO_SUPPORT, el);
		break;
	}

	/* Add the element to the session list */
	if (exist == false) {
		DLIST_ADD(global_mapi_ctx->session, el);
		*session = el;
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Initialize mapi context global structure

   This function inititalizes the MAPI subsystem and open the profile
   database pointed by profiledb .

   \param profiledb profile database path

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_NOT_ENOUGH_RESOURCES: MAPI subsystem failed to allocate
     the necessary resources to operate properly
   - MAPI_E_NOT_FOUND: No suitable profile database was found in the
     path pointed by profiledb
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa MAPIUninitialize
*/
_PUBLIC_ enum MAPISTATUS MAPIInitialize(const char *profiledb)
{
	enum MAPISTATUS	retval;
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
	global_mapi_ctx->session = NULL;
	global_mapi_ctx->lp_ctx = loadparm_init(global_mapi_ctx);

	/* profile store */
	retval = OpenProfileStore(mem_ctx, &global_mapi_ctx->ldb_ctx, profiledb);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	lp_load_default(global_mapi_ctx->lp_ctx);
	dcerpc_init(global_mapi_ctx->lp_ctx);

	errno = 0;
	return MAPI_E_SUCCESS;
}



/**
   \details Uninitialize MAPI subsystem

   This function uninitializes the MAPI context and destroy
   recursively the whole mapi session and associated objects hierarchy

   \sa MAPIInitialize, GetLastError
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
