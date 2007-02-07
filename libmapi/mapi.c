/*
 *  OpenChange MAPI implementation.
 *
 *  Copyright (C) Julien Kerihuel 2007.
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

#include "openchange.h"
#include "exchange.h"
#include "ndr_exchange.h"
#include "libmapi/include/mapidefs.h"
#include "libmapi/include/nspi.h"
#include "libmapi/include/emsmdb.h"
#include "libmapi/mapicode.h"
#include "libmapi/include/proto.h"
#include "libmapi/include/mapi_proto.h"

/**
 * Initiates a connection on the exchange_nsp pipe
 *
 */

MAPISTATUS MAPIInitialize(struct emsmdb_context *emsmdb, struct cli_credentials *cred)
{
	struct dcerpc_pipe      *p;
        NTSTATUS status;
	const char *binding = lp_parm_string(-1, "exchange", "nspi_binding");

	if (!binding) {
		printf("You must specify a binding string\n");
		return MAPI_E_LOGON_FAILED;
	}

	status = dcerpc_pipe_connect(emsmdb->mem_ctx, 
				     &p, binding, &dcerpc_table_exchange_nsp,
				     cred, NULL);

	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to connect to remote server: %s %s\n", binding, nt_errstr(status));
	}

	emsmdb->nspi = talloc_zero(emsmdb->mem_ctx, struct nspi_context);
	emsmdb->nspi->rpc_connection = p;
	emsmdb->nspi = nspi_bind(emsmdb->mem_ctx, p, cred);

	if (!emsmdb->nspi) {
		return MAPI_E_LOGON_FAILED;
	}

	return MAPI_E_SUCCESS;
}

/**
 * Close the connection to the exchange nspi pipe.
 *
 */

MAPISTATUS MAPIUninitialize(struct emsmdb_context *emsmdb)
{
	BOOL ret;

	if ((ret = nspi_unbind(emsmdb->nspi)) != True) {
		/* FIXME with a valid error value */
		return MAPI_E_LOGON_FAILED;
	}
	return MAPI_E_SUCCESS;
}
