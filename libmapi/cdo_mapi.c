/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2010.
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

#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include <param.h>
#include <ldb.h>

/**
   \file cdo_mapi.c

   \brief MAPI subsystem related operations
*/


/**
   \details Create a full MAPI session
 
   Open providers stored in the profile and return a pointer on a
   IMAPISession object.

   \param mapi_ctx pointer to the MAPI context
   \param session pointer to a pointer to a MAPI session object
   \param profname profile name to use
   \param password password to use for the profile

   password should be set to NULL if the password has been stored in
   the profile.

   \return MAPI_E_SUCCESS on success otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_NOT_ENOUGH_RESOURCES: MAPI subsystem failed to allocate
     the necessary resources to operate properly
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa MAPIInitialize, OpenProfile, MapiLogonProvider
*/
_PUBLIC_ enum MAPISTATUS MapiLogonEx(struct mapi_context *mapi_ctx,
				     struct mapi_session **session, 
				     const char *profname, 
				     const char *password)
{
	struct mapi_session	*tmp_session = NULL;
	enum MAPISTATUS		retval;

	retval = MapiLogonProvider(mapi_ctx, &tmp_session, profname, password, PROVIDER_ID_NSPI);
	if (retval != MAPI_E_SUCCESS) return retval;

	retval = MapiLogonProvider(mapi_ctx, &tmp_session, profname, password, PROVIDER_ID_EMSMDB);
	if (retval != MAPI_E_SUCCESS) return retval;

	*session = tmp_session;

	return MAPI_E_SUCCESS;
}


/**
   \details Initialize a session on the specified provider

   \param mapi_ctx pointer to the MAPI context
   \param session pointer to a pointer to a MAPI session object
   \param profname profile name
   \param password profile password
   \param provider provider we want to establish a connection on

   password should be set to NULL if the password has been stored in
   the profile.

   Supported providers are:
   - PROVIDER_ID_NSPI: Address Book provider
   - PROVIDER_ID_EMSMDB: MAPI Store provider

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_NOT_ENOUGH_RESOURCES: MAPI subsystem failed to allocate
     the necessary resources to operate properly
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \sa MapiLogonEx, OpenProfile, LoadProfile
*/
_PUBLIC_ enum MAPISTATUS MapiLogonProvider(struct mapi_context *mapi_ctx,
					   struct mapi_session **session,
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
	OPENCHANGE_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);

	/* If no MAPI session has already been created */
	if (!mapi_ctx->session) {
		mapi_ctx->session = talloc_zero(mapi_ctx->mem_ctx, struct mapi_session);
	}
	
	/* If the session doesn't exist, create a new one */
	if (!*session) {
		el = talloc_zero(mapi_ctx->mem_ctx, struct mapi_session);
		memset(el->logon_ids, 0, 255);
		el->mapi_ctx = mapi_ctx;
		OPENCHANGE_RETVAL_IF(!el, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
	} else {
		/* Lookup the session within the chained list */
		for (el = mapi_ctx->session; el; el = el->next) {
			if (*session == el) {
				found = true;
				break;
			}
		}
		OPENCHANGE_RETVAL_IF(found == false, MAPI_E_NOT_FOUND, NULL);
		exist = true;
	}

	mem_ctx = (TALLOC_CTX *) el;

	/* Open the profile */
	if (!el->profile) {
		profile = talloc_zero(mem_ctx, struct mapi_profile);
		OPENCHANGE_RETVAL_IF(!profile, MAPI_E_NOT_ENOUGH_RESOURCES, el);

		retval = OpenProfile(mapi_ctx, profile, profname, password);
		OPENCHANGE_RETVAL_IF(retval, retval, el);
		
		retval = LoadProfile(mapi_ctx, profile);
		OPENCHANGE_RETVAL_IF(retval, retval, el);

		el->profile = profile;
	}

	switch (provider) {
	case PROVIDER_ID_EMSMDB:
		provider_emsmdb = talloc_zero(mem_ctx, struct mapi_provider);
		OPENCHANGE_RETVAL_IF(!provider_emsmdb, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
		talloc_set_destructor((void *)provider_emsmdb, (int (*)(void *))emsmdb_disconnect_dtor);
		retval = Logon(el, provider_emsmdb, PROVIDER_ID_EMSMDB);
		if (retval) return retval;
		el->emsmdb = provider_emsmdb;
		break;
	case PROVIDER_ID_NSPI:
		provider_nspi = talloc_zero(mem_ctx, struct mapi_provider);
		OPENCHANGE_RETVAL_IF(!provider_nspi, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
		talloc_set_destructor((void *)provider_nspi, (int (*)(void *))nspi_disconnect_dtor);
		retval = Logon(el, provider_nspi, PROVIDER_ID_NSPI);
		if (retval) return retval;
		el->nspi = provider_nspi;
		break;
	default:
		OPENCHANGE_RETVAL_IF(1, MAPI_E_NO_SUPPORT, el);
		break;
	}

	/* Add the element to the session list */
	if (exist == false) {
		DLIST_ADD(mapi_ctx->session, el);
		*session = el;
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Initialize mapi context structure

   This function inititalizes the MAPI subsystem and open the profile
   database pointed by profiledb .

   \param _mapi_ctx pointer to the MAPI context
   \param profiledb profile database path

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
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
_PUBLIC_ enum MAPISTATUS MAPIInitialize(struct mapi_context **_mapi_ctx, const char *profiledb)
{
	enum MAPISTATUS		retval;
	struct mapi_context	*mapi_ctx;
	TALLOC_CTX		*mem_ctx;

	/* Set the initial errno value for GetLastError */
	errno = 0;

	OPENCHANGE_RETVAL_IF(!_mapi_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!profiledb, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "MAPIInitialize");
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);

	mapi_ctx = talloc_zero(mem_ctx, struct mapi_context);
	OPENCHANGE_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);
	mapi_ctx->mem_ctx = mem_ctx;
	mapi_ctx->dumpdata = false;
	mapi_ctx->session = NULL;
	mapi_ctx->lp_ctx = loadparm_init_global(true);
	OPENCHANGE_RETVAL_IF(!mapi_ctx->lp_ctx, MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	/* Enable logging on stdout */
	oc_log_init_stdout();

	/* profile store */
	retval = OpenProfileStore(mapi_ctx, &mapi_ctx->ldb_ctx, profiledb);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Initialize dcerpc subsystem */
	dcerpc_init();

	errno = 0;
	
	*_mapi_ctx = mapi_ctx;
	
	return MAPI_E_SUCCESS;
}



/**
   \details Uninitialize MAPI subsystem

   \param mapi_ctx pointer to the MAPI context

   This function uninitializes the MAPI context and destroy
   recursively the whole mapi session and associated objects hierarchy

   \sa MAPIInitialize, GetLastError
 */
_PUBLIC_ void MAPIUninitialize(struct mapi_context *mapi_ctx)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_session	*session;

	if (!mapi_ctx) return;

	session = mapi_ctx->session;
	if (session && session->notify_ctx && session->notify_ctx->fd != -1) {
		OC_DEBUG(3, "emsmdb_disconnect_dtor: unbind udp");
		shutdown(session->notify_ctx->fd, SHUT_RDWR);
		close(session->notify_ctx->fd);
	}
	
	mem_ctx = mapi_ctx->mem_ctx;
	talloc_free(mem_ctx);
	mapi_ctx = NULL;
}


/**
   \details Enable MAPI network trace output

   \param mapi_ctx pointer to the MAPI context
   \param status the status

   possible status values/behavior:
   -# true:  Network traces are displayed on stdout
   -# false: Network traces are not displayed on stdout

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_INITIALIZED
 */
_PUBLIC_ enum MAPISTATUS SetMAPIDumpData(struct mapi_context *mapi_ctx, bool status)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	
	mapi_ctx->dumpdata = status;

	return MAPI_E_SUCCESS;
}


/**
   \details Set MAPI debug level

   \param mapi_ctx pointer to the MAPI context
   \param level the debug level to set

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: the function parameter is invalid
 */
_PUBLIC_ enum MAPISTATUS SetMAPIDebugLevel(struct mapi_context *mapi_ctx, uint32_t level)
{
	char	*debuglevel;
	bool	ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	debuglevel = talloc_asprintf(mapi_ctx->mem_ctx, "%u", level);
	ret = lpcfg_set_cmdline(mapi_ctx->lp_ctx, "log level", debuglevel);
	talloc_free(debuglevel);

	return (ret == true) ? MAPI_E_SUCCESS : MAPI_E_INVALID_PARAMETER;
}


/**
   \details Retrieve the MAPI loadparm context for specified MAPI
   context

   \param mapi_ctx pointer to the MAPI context
   \param lp_ctx pointer to a pointer to the loadparm context that the
   function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_INITIALIZED
   or MAPI_E_INVALID_PARAMETER
 */
_PUBLIC_ enum MAPISTATUS GetLoadparmContext(struct mapi_context *mapi_ctx, 
					    struct loadparm_context **lp_ctx)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!lp_ctx, MAPI_E_INVALID_PARAMETER, NULL);

	*lp_ctx = mapi_ctx->lp_ctx;

	return MAPI_E_SUCCESS;
}
