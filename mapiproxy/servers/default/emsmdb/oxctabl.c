/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009

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
   \file oxctabl.c

   \brief Table object routines and Rops
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"

/**
   \details EcDoRpc SetColumns (0x12) Rop. This operation sets the
   properties to be included in the table.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SetColumns EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the SetColumns EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSetColumns(TALLOC_CTX *mem_ctx,
					       struct emsmdbp_context *emsmdbp_ctx,
					       struct EcDoRpc_MAPI_REQ *mapi_req,
					       struct EcDoRpc_MAPI_REPL *mapi_repl,
					       uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCTABL] SetColumns (0x12)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_SetColumns.TableStatus = 0;

	*size += libmapiserver_RopSetColumns_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc SortTable (0x13) Rop. This operation defines the
   order of rows of a table based on sort criteria.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SortTable EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the SortTable EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSortTable(TALLOC_CTX *mem_ctx,
					      struct emsmdbp_context *emsmdbp_ctx,
					      struct EcDoRpc_MAPI_REQ *mapi_req,
					      struct EcDoRpc_MAPI_REPL *mapi_repl,
					      uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCTABL] SortTable (0x13)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_SortTable.TableStatus = 0;

	*size += libmapiserver_RopSortTable_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc QueryRows (0x15) Rop. This operation retrieves
   rows from a table.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the QueryRows EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the QueryRows EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopQueryRows(TALLOC_CTX *mem_ctx,
					      struct emsmdbp_context *emsmdbp_ctx,
					      struct EcDoRpc_MAPI_REQ *mapi_req,
					      struct EcDoRpc_MAPI_REPL *mapi_repl,
					      uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCTABL] QueryRows (0x15)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_QueryRows.Origin = 0;
	mapi_repl->u.mapi_QueryRows.RowCount = 0;
	mapi_repl->u.mapi_QueryRows.RowData.length = 0;
	mapi_repl->u.mapi_QueryRows.RowData.data = NULL;

	*size += libmapiserver_RopQueryRows_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc SeekRow (0x18) Rop. This operation moves the
   cursor to a specific position in a table.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SeekRow EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the SeekRow EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSeekRow(TALLOC_CTX *mem_ctx,
					    struct emsmdbp_context *emsmdbp_ctx,
					    struct EcDoRpc_MAPI_REQ *mapi_req,
					    struct EcDoRpc_MAPI_REPL *mapi_repl,
					    uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCTABL] SeekRow (0x18)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_SeekRow.HasSoughtLess = 0;
	mapi_repl->u.mapi_SeekRow.RowsSought = 0;

	*size += libmapiserver_RopSeekRow_size(mapi_repl);

	return MAPI_E_SUCCESS;
}
