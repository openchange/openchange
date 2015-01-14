/*
   OpenChange MAPI implementation.

   Copyright (C) Brad Hards <bradh@openchange.org> 2010.
 
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
#include "gen_ndr/ndr_exchange.h"
#include "gen_ndr/ndr_exchange_c.h"

/**
   \file async_emsmdb.c

   \brief Async_EMSMDB stack functions
 */

/**
   \details Create an asynchronous wait call

   This basically "parks" a call on the AsyncEMSMDB interface to allow
   asynchronous notification to the client of changes on the server.
   This call (probably) won't return immediately, but will return when
   the server makes a change, or 300 seconds (5 minutes) elapses. This
   call will then need to be re-queued if further change notifications
   are wanted.

   \param emsmdb_ctx pointer to the EMSMDB context
   \param flagsIn input flags (currently must be 0x00000000)
   \param flagsOut output flags (zero for a call completion with no changes, non-zero if there are changes)

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
enum MAPISTATUS emsmdb_async_waitex(struct emsmdb_context *emsmdb_ctx, uint32_t flagsIn, uint32_t *flagsOut)
{
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	struct EcDoAsyncWaitEx		r;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!emsmdb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!(emsmdb_ctx->mem_ctx), MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!(emsmdb_ctx->async_rpc_connection), MAPI_E_NOT_INITIALIZED, NULL);

	r.in.async_handle = &(emsmdb_ctx->async_handle);
	r.in.ulFlagsIn = flagsIn;
	r.out.pulFlagsOut = flagsOut;
	dcerpc_binding_handle_set_timeout(emsmdb_ctx->async_rpc_connection->binding_handle, 400);
	status = dcerpc_EcDoAsyncWaitEx_r(emsmdb_ctx->async_rpc_connection->binding_handle, emsmdb_ctx->mem_ctx, &r);

	/* If the call failed, map NT_STATUS to MAPISTATUS. r.out.result isn't valid unless status is OK */
	OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT), MAPI_E_TIMEOUT, NULL);
	OPENCHANGE_RETVAL_IF(NT_STATUS_EQUAL(status, NT_STATUS_CONNECTION_DISCONNECTED), MAPI_E_END_OF_SESSION, NULL);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), NT_STATUS_V(status), NULL);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}
