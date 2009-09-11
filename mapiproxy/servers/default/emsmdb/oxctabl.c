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
#include "libmapi/defs_private.h"

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
	enum MAPISTATUS			retval;
	struct mapi_handles		*parent;
	struct emsmdbp_object		*object;
	struct emsmdbp_object_table	*table;
	struct SetColumns_req		request;
	void				*data = NULL;
	uint32_t			handle;

	DEBUG(4, ("exchange_emsmdb: [OXCTABL] SetColumns (0x12)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	/* Initialize default empty SetColumns reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_SetColumns.TableStatus = TBLSTAT_COMPLETE;

	*size += libmapiserver_RopSetColumns_size(mapi_repl);

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	retval = mapi_handles_get_private_data(parent, &data);
	object = (struct emsmdbp_object *) data;

	if (object) {
		table = object->object.table;
		OPENCHANGE_RETVAL_IF(!table, MAPI_E_INVALID_PARAMETER, NULL);

		request = mapi_req->u.mapi_SetColumns;
		if (request.prop_count) {
			table->prop_count = request.prop_count;
			table->properties = talloc_memdup(table, request.properties, 
							  request.prop_count * sizeof (uint32_t));
		}
	}

	DEBUG(0, ("RopSetColumns: returns MAPI_E_SUCCESS\n"));
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
	mapi_repl->u.mapi_SortTable.TableStatus = TBLSTAT_COMPLETE;

	*size += libmapiserver_RopSortTable_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc SortTable (0x14) Rop. This operation establishes a
   filter for a table.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the Restrict EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the Restrict EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopRestrict(TALLOC_CTX *mem_ctx,
					     struct emsmdbp_context *emsmdbp_ctx,
					     struct EcDoRpc_MAPI_REQ *mapi_req,
					     struct EcDoRpc_MAPI_REPL *mapi_repl,
					     uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCTABL] Restrict (0x14)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_Restrict.TableStatus = TBLSTAT_COMPLETE;

	*size += libmapiserver_RopRestrict_size(mapi_repl);

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
	enum MAPISTATUS			retval;
	struct mapi_handles		*parent;
	struct emsmdbp_object		*object;
	struct emsmdbp_object_table	*table;
	struct QueryRows_req		request;
	struct QueryRows_repl		response;
	void				*data;
	char				*table_filter = NULL;
	uint32_t			handle;
	uint32_t			count;
	uint32_t			property;
	uint8_t				flagged;
	uint32_t			i, j;

	DEBUG(4, ("exchange_emsmdb: [OXCTABL] QueryRows (0x15)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = mapi_req->u.mapi_QueryRows;
	response = mapi_repl->u.mapi_QueryRows;

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_NOT_FOUND;
	
	response.RowData.length = 0;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) goto end;

	retval = mapi_handles_get_private_data(parent, &data);
	object = (struct emsmdbp_object *) data;

	/* Ensure referring object exists and is a table */
	if (!object || (object->type != EMSMDBP_OBJECT_TABLE)) {
		goto end;
	}

	table = object->object.table;
	if (!table->folderID) {
		goto end;
	}
	table_filter = talloc_asprintf(mem_ctx, "(&(PidTagParentFolderId=0x%.16"PRIx64")(PidTagFolderId=*))", table->folderID);
	DEBUG(0, ("table_filter: %s\n", table_filter));

	if ((request.RowCount + table->numerator) > table->denominator) {
		request.RowCount = table->denominator - table->numerator;
	}

	/* Lookup the properties and check if we need to flag the PropertyRow blob */
	for (i = 0, count = 0; i < request.RowCount; i++, count++) {
		flagged = 0;

		/* Lookup for flagged property row */
		for (j = 0; j < table->prop_count; j++) {
			retval = openchangedb_get_table_property(mem_ctx, emsmdbp_ctx->oc_ctx, 
								 emsmdbp_ctx->szDisplayName,
								 table_filter, table->properties[j], 
								 table->numerator, &data);	
			if (retval == MAPI_E_INVALID_OBJECT) {
				goto finish;
			}
			if (retval == MAPI_E_NOT_FOUND) {
				flagged = 1;
				libmapiserver_push_property(mem_ctx, lp_iconv_convenience(emsmdbp_ctx->lp_ctx),
							    0x0000000b, (const void *)&flagged, &response.RowData,
							    0, 0);
				break;
			}			
		}

		/* Push the property */
		for (j = 0; j < table->prop_count; j++) {
			property = table->properties[j];
			retval = openchangedb_get_table_property(mem_ctx, emsmdbp_ctx->oc_ctx, 
								 emsmdbp_ctx->szDisplayName,
								 table_filter, table->properties[j], 
								 table->numerator, &data);
			if (retval == MAPI_E_INVALID_OBJECT) {
				goto finish;
			}
			if (retval == MAPI_E_NOT_FOUND) {
				property = (property & 0xFFFF0000) + PT_ERROR;
				data = (void *)&retval;
			}

			libmapiserver_push_property(mem_ctx, lp_iconv_convenience(emsmdbp_ctx->lp_ctx),
						    property, (const void *)data,
						    &response.RowData, flagged?PT_ERROR:0, flagged);

		}

		table->numerator++;
	}
finish:
	talloc_free(table_filter);

	/* QueryRows reply parameters */
	if (count) {
		if (count < request.RowCount) {
			mapi_repl->u.mapi_QueryRows.Origin = 0;
		} else {
			mapi_repl->u.mapi_QueryRows.Origin = 2;
		}
		mapi_repl->error_code = MAPI_E_SUCCESS;
		mapi_repl->u.mapi_QueryRows.RowCount = count;
		mapi_repl->u.mapi_QueryRows.RowData.length = response.RowData.length;
		mapi_repl->u.mapi_QueryRows.RowData.data = response.RowData.data;
	} else {
		/* useless code for the moment */
		mapi_repl->error_code = MAPI_E_SUCCESS;
		mapi_repl->u.mapi_QueryRows.Origin = 2;
		mapi_repl->u.mapi_QueryRows.RowCount = 0;
		mapi_repl->u.mapi_QueryRows.RowData.length = 0;
		mapi_repl->u.mapi_QueryRows.RowData.data = NULL;
	}

end:
	*size += libmapiserver_RopQueryRows_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc QueryPosition (0x17) Rop. This operation returns
   the location of cursor in the table.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the QueryPosition EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the QueryPosition EcDoRpc_MAPI_REPL structure
   \handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopQueryPosition(TALLOC_CTX *mem_ctx,
						  struct emsmdbp_context *emsmdbp_ctx,
						  struct EcDoRpc_MAPI_REQ *mapi_req,
						  struct EcDoRpc_MAPI_REPL *mapi_repl,
						  uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	struct mapi_handles		*parent;
	struct emsmdbp_object		*object;
	struct emsmdbp_object_table	*table;
	void				*data;
	uint32_t			handle;

	DEBUG(4, ("exchange_emsmdb: [OXCTABL] QueryPosition (0x17)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_NOT_FOUND;
	
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) goto end;

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) goto end;
	object = (struct emsmdbp_object *) data;

	/* Ensure object exists and is table type */
	if (!object || (object->type != EMSMDBP_OBJECT_TABLE)) goto end;

	table = object->object.table;
	if (!table->folderID) goto end;

	mapi_repl->u.mapi_QueryPosition.Numerator = table->numerator;
	mapi_repl->u.mapi_QueryPosition.Denominator = table->denominator;
	mapi_repl->error_code = MAPI_E_SUCCESS;

end:
	*size += libmapiserver_RopQueryPosition_size(mapi_repl);

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


/**
   \details EcDoRpc FindRow (0x4f) Rop. This operation moves the
   cursor to a row in a table that matches specific search criteria.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the FindRow EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the FindRow EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopFindRow(TALLOC_CTX *mem_ctx,
					    struct emsmdbp_context *emsmdbp_ctx,
					    struct EcDoRpc_MAPI_REQ *mapi_req,
					    struct EcDoRpc_MAPI_REPL *mapi_repl,
					    uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCTABL] FindRow (0x4f)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_FindRow.RowNoLongerVisible = 0;
	mapi_repl->u.mapi_FindRow.HasRowData = 0;
	mapi_repl->u.mapi_FindRow.row.length = 0;
	mapi_repl->u.mapi_FindRow.row.data = 0;

	*size += libmapiserver_RopFindRow_size(mapi_repl);

	return MAPI_E_SUCCESS;
}
