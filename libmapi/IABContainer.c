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
 * Wrapper on nspi_ResolveNames and nspi_ResolveNamesW
 *
 * Set flags to MAPI_UNICODE for ResolveNamesW call 
 * otherwise ResolveNames is called 
 *
 * Fill an adrlist and flaglist structure
 *
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
