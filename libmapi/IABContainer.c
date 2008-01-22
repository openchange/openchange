/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007.

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


/**
   \file IABContainer.c

   \brief Provides access to address book containers -- Used to
   perform name resolution
*/


/**
   \details Resolve user names against the Windows Address Book Provider

   \param usernames list of user names to resolve
   \param rowset resulting list of user details
   \param props resulting list of resolved names
   \param flaglist resulting list of resolution status (see below)
   \param flags if set to MAPI_UNICODE then UNICODE MAPITAGS can be
   used, otherwise only UTF8 encoded fields may be returned.

   Possible flaglist values are:
   - MAPI_UNRESOLVED: could not be resolved
   - MAPI_AMBIGUOUS: resolution match more than one entry
   - MAPI_RESOLVED: resolution matched a single entry
 
   \return MAPI_E_SUCCESS on success, otherwise -1.
   
   \note Developers should call GetLastError() to retrieve the last MAPI error
   code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_SESSION_LIMIT: No session has been opened on the provider
   - MAPI_E_NOT_ENOUGH_RESOURCES: MAPI subsystem failed to allocate
     the necessary resources to operate properly
   - MAPI_E_NOT_FOUND: No suitable profile database was found in the
     path pointed by profiledb
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
   
   \sa MAPILogonProvider, GetLastError
 */
_PUBLIC_ enum MAPISTATUS ResolveNames(const char **usernames, struct SPropTagArray *props, struct SRowSet **rowset, struct FlagList **flaglist, uint32_t flags)
{
	struct nspi_context	*nspi;
	enum MAPISTATUS		retval;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!global_mapi_ctx->session, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!global_mapi_ctx->session->nspi, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!global_mapi_ctx->session->nspi->ctx, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!rowset, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	nspi = (struct nspi_context *)mapi_ctx->session->nspi->ctx;

	*rowset = talloc_zero(mapi_ctx->session, struct SRowSet);
	*flaglist = talloc_zero(mapi_ctx->session, struct FlagList);

	switch (flags) {
	case MAPI_UNICODE:
		retval = nspi_ResolveNamesW(nspi, usernames, props, &rowset, &flaglist);
		break;
	default:
		retval = nspi_ResolveNames(nspi, usernames, props, &rowset, &flaglist);
		break;
	}

	if (retval != MAPI_E_SUCCESS) return retval;

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the global address list
   
   \param SPropTagArray pointer on an array of MAPI properties we want
   to fetch
   \param SRowSet pointer on the rows returned
   \param count the number of rows we want to fetch
   \param first specify if this is the first call or not

   Possible value for first:
   - true: Use it the first time you call GetGALTable or when you want
     to rewind to the beginning of the Global Address List table
   - false: For any other GetGALTable but the first one.

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \sa MapiLogonEx, MapiLogonProvider
 */
_PUBLIC_ enum MAPISTATUS GetGALTable(struct SPropTagArray *SPropTagArray, struct SRowSet **SRowSet, 
				     uint32_t count, bool first)
{
	struct nspi_context	*nspi;
	enum MAPISTATUS		retval;
	mapi_ctx_t		*mapi_ctx;
	uint32_t		instance_key = 0;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!global_mapi_ctx->session, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!global_mapi_ctx->session->nspi, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!global_mapi_ctx->session->nspi->ctx, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!SRowSet, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	nspi = (struct nspi_context *)mapi_ctx->session->nspi->ctx;

	instance_key = nspi->profile_instance_key;
	nspi->profile_instance_key = 0;
       
	if (first == true) {
		memset(nspi->settings->service_provider.ab, 0, 16);
		memset(nspi->settings->service_provider.ab + 12, 0xff, 4);
	}

	*SRowSet = talloc_zero(mapi_ctx->session, struct SRowSet);
	retval = nspi_QueryRows(nspi, SPropTagArray, *SRowSet, count);

	nspi->profile_instance_key = instance_key;

	if (retval != MAPI_E_SUCCESS) return retval;

	return MAPI_E_SUCCESS;
}
