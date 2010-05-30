/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2010

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

/**
   \file oxcperm.c

   \brief Access and Operation Permissions Rops
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"


/**
   \details EcDoRpc GetPermissionsTable (0x3e) Rop. This operation get
   the permissions table of a folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetPermissionsTable EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the GetPermissionsTable
   EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetPermissionsTable(TALLOC_CTX *mem_ctx,
							struct emsmdbp_context *emsmdbp_ctx,
							struct EcDoRpc_MAPI_REQ *mapi_req,
							struct EcDoRpc_MAPI_REPL *mapi_repl,
							uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	struct mapi_handles	*rec = NULL;
	uint32_t		handle;

	DEBUG(4, ("exchange_emsmdb: [OXCPERM] GetPermissionsTable (0x3e)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->u.mapi_GetPermissionsTable.handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	*size += libmapiserver_RopGetPermissionsTable_size(mapi_repl);

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
	handles[mapi_repl->handle_idx] = rec->handle;

	return MAPI_E_SUCCESS;
}
