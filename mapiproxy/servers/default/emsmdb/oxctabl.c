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
#include "libmapi/libmapi_private.h"

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
	uint32_t			handle, i;

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
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		DEBUG(5, ("  handle data not found, idx = %x\n", mapi_req->handle_idx));
		goto end;
	}

	object = (struct emsmdbp_object *) data;

	if (object) {
		table = object->object.table;
		OPENCHANGE_RETVAL_IF(!table, MAPI_E_INVALID_PARAMETER, NULL);

		request = mapi_req->u.mapi_SetColumns;

		DEBUG(5, ("  handle_idx: %.8x, folder_id: %.16lx; properties (%d):", mapi_req->handle_idx, table->folderID, request.prop_count));
		for (i = 0; i < request.prop_count; i++) {
		    DEBUG(5, (" %.8x", request.properties[i]));
		}
		DEBUG(5, ("\n"));

		if (request.prop_count) {
			table->prop_count = request.prop_count;
			table->properties = talloc_memdup(table, request.properties, 
							  request.prop_count * sizeof (uint32_t));
		}
	}
end:

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
	DEBUG(4, ("exchange_emsmdb: [OXCTABL] SortTable (0x13) -- stub\n"));

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
	enum MAPISTATUS			retval;
	struct mapi_handles		*parent;
	struct emsmdbp_object		*object;
	struct emsmdbp_object_table	*table;
	struct Restrict_req		request;
	uint32_t			handle;
	void				*data = NULL;
	uint8_t				status;

	DEBUG(4, ("exchange_emsmdb: [OXCTABL] Restrict (0x14)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	request = mapi_req->u.mapi_Restrict;

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_Restrict.TableStatus = TBLSTAT_COMPLETE;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		DEBUG(5, ("  handle data not found, idx = %x\n", mapi_req->handle_idx));
		goto end;
	}
	object = (struct emsmdbp_object *) data;

	/* Ensure referring object exists and is a table */
	if (!object || (object->type != EMSMDBP_OBJECT_TABLE)) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  missing object or not table\n"));
		goto end;
	}

	table = object->object.table;
	OPENCHANGE_RETVAL_IF(!table, MAPI_E_INVALID_PARAMETER, NULL);

	if (table->ulType == EMSMDBP_TABLE_RULE_TYPE) {
		DEBUG(5, ("  query on rules table are all faked right now\n"));
		goto end;
	}

	/* If parent folder has a mapistore context */
	if (table->mapistore) {
		status = TBLSTAT_COMPLETE;
		retval = mapistore_set_restrictions(emsmdbp_ctx->mstore_ctx, table->contextID, 
						    table->folderID, table->ulType, &request.restrictions, &status);
		if (retval) {
			mapi_repl->error_code = retval;
			goto end;
		}
		
		mapi_repl->u.mapi_Restrict.TableStatus = status;
		/* Parent folder doesn't have any mapistore context associated */
	} else {
		DEBUG(0, ("not mapistore Restrict: Not implemented yet\n"));
		goto end;
	}

end:
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
	struct mapi_handles		*parent;
	struct emsmdbp_object		*object;
	struct emsmdbp_object_table	*table;
	struct QueryRows_req		request;
	struct QueryRows_repl		response;
	enum MAPISTATUS			retval;
	void				*data;
	enum MAPISTATUS			*retvals;
	void				**data_pointers;
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
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		DEBUG(5, ("  handle data not found, idx = %x\n", mapi_req->handle_idx));
		goto end;
	}

	object = (struct emsmdbp_object *) data;

	/* Ensure referring object exists and is a table */
	if (!object) {
		DEBUG(5, ("  missing object\n"));
		goto end;
	}
	if (object->type != EMSMDBP_OBJECT_TABLE) {
		DEBUG(5, ("  unhandled object type: %d\n", object->type));
		goto end;
	}

	table = object->object.table;
	if (!table->folderID) {
		goto end;
	}

	count = 0;
	if (table->ulType == EMSMDBP_TABLE_RULE_TYPE) {
		DEBUG(5, ("  query on rules table are all faked right now\n"));
		goto finish;
	}

	if ((request.RowCount + table->numerator) > table->denominator) {
		request.RowCount = table->denominator - table->numerator;
	}

	DEBUG (5, ("  folderID: %.16lx\n", table->folderID));
	/* If parent folder has a mapistore context */
	if (table->mapistore == true) {
		/* Lookup the properties and check if we need to flag the PropertyRow blob */
		for (i = 0; i < request.RowCount; i++, count++) {
			flagged = 0;
			data_pointers = talloc_array(mem_ctx, void *, table->prop_count);
			retvals = talloc_array(mem_ctx, enum MAPISTATUS, table->prop_count);

			DEBUG(5, ("  row %d, property statuses(%d):", count, table->prop_count));
			/* Lookup for flagged property row */
			for (j = 0; j < table->prop_count; j++) {
				retval = mapistore_get_table_property(emsmdbp_ctx->mstore_ctx, table->contextID,
								      table->ulType,
								      MAPISTORE_PREFILTERED_QUERY,
								      table->folderID, 
								      table->properties[j],
								      table->numerator, data_pointers + j);
				DEBUG(5, ("  %.8x: %d", table->properties[j], retval));
				if (retval == MAPI_E_INVALID_OBJECT) {
					DEBUG(5, ("\n"));
					DEBUG(5, ("%s: invalid object (likely due to a restriction)\n", __location__));
					talloc_free(retvals);
					talloc_free(data_pointers);
					goto finish;
				}
				retvals[j] = retval;

				if (retval == MAPI_E_NOT_FOUND) {
					flagged = 1;
				}
			}
			DEBUG(5, ("\n"));

			if (flagged) {
				libmapiserver_push_property(mem_ctx,
							    lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
							    0x0000000b, (const void *)&flagged,
							    &response.RowData, 0, 0, 0);
			}
			else {
				libmapiserver_push_property(mem_ctx,
							    lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
							    0x00000000, (const void *)&flagged,
							    &response.RowData, 0, 1, 0);
			}

			for (j = 0; j < table->prop_count; j++) {
				property = table->properties[j];
				retval = retvals[j];
				if (retval == MAPI_E_NOT_FOUND) {
					property = (property & 0xFFFF0000) + PT_ERROR;
					data = &retval;
				}
				else {
					data = data_pointers[j];
				}

				libmapiserver_push_property(mem_ctx, lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
							    property, data, &response.RowData,
							    flagged?PT_ERROR:0, flagged, 0);
			}

			talloc_free(retvals);
			talloc_free(data_pointers);
			table->numerator++;
		}

	/* parent folder doesn't have any mapistore context associated */
	} else {
		table_filter = talloc_asprintf(mem_ctx, "(&(PidTagParentFolderId=0x%.16"PRIx64")(PidTagFolderId=*))", table->folderID);
		/* Lookup the properties and check if we need to flag the PropertyRow blob */
		for (i = 0; i < request.RowCount; i++, count++) {
			flagged = 0;
			data_pointers = talloc_array(mem_ctx, void *, table->prop_count);
			retvals = talloc_array(mem_ctx, enum MAPISTATUS, table->prop_count);

			/* Lookup for flagged property row */
			for (j = 0; j < table->prop_count; j++) {
				retval = openchangedb_get_table_property(mem_ctx, emsmdbp_ctx->oc_ctx, 
									 emsmdbp_ctx->szDisplayName,
									 table_filter, table->properties[j], 
									 table->numerator, data_pointers + j);
				/* DEBUG(5, ("  %.8x: %d", table->properties[j], retval)); */
				if (retval == MAPI_E_INVALID_OBJECT) {
					count = 0;
					DEBUG(5, ("%s: invalid object in non-mapistore folder, count set to 0\n", __location__));
					talloc_free(retvals);
					talloc_free(data_pointers);
					goto finish;
				}

				retvals[j] = retval;

				if (retval == MAPI_E_NOT_FOUND) {
					flagged = 1;
				}
			}

			if (flagged) {
				libmapiserver_push_property(mem_ctx, 
							    lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
							    0x0000000b, (const void *)&flagged, 
							    &response.RowData, 0, 0, 0);
			}
			else {
				libmapiserver_push_property(mem_ctx, 
							    lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
							    0x00000000, (const void *)&flagged,
							    &response.RowData, 0, 1, 0);
			}

			/* Push the property */
			for (j = 0; j < table->prop_count; j++) {
				property = table->properties[j];
				retval = retvals[j];
				if (retval == MAPI_E_NOT_FOUND) {
					property = (property & 0xFFFF0000) + PT_ERROR;
					data = &retval;
				}
				else {
					data = data_pointers[j];
				}

				libmapiserver_push_property(mem_ctx, lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
							    property, data, &response.RowData,
							    flagged?PT_ERROR:0, flagged, 0);
			}
			
			talloc_free(retvals);
			talloc_free(data_pointers);
			table->numerator++;
		}
	}

finish:
	if (table_filter) {
		talloc_free(table_filter);
	}

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
		dump_data(0, response.RowData.data, response.RowData.length);
	} else {
		/* useless code for the moment */
		mapi_repl->error_code = MAPI_E_SUCCESS;
		mapi_repl->u.mapi_QueryRows.Origin = 2;
		mapi_repl->u.mapi_QueryRows.RowCount = 0;
		mapi_repl->u.mapi_QueryRows.RowData.length = 0;
		mapi_repl->u.mapi_QueryRows.RowData.data = NULL;
		DEBUG(5, ("%s: returning empty data set\n", __location__));
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
   \param handles pointer to the MAPI handles array
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
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		DEBUG(5, ("  no private data or object is not a table"));
		goto end;
	}
	object = (struct emsmdbp_object *) data;

	/* Ensure object exists and is table type */
	if (!object || (object->type != EMSMDBP_OBJECT_TABLE)) {
		DEBUG(5, ("  no object or object is not a table\n"));
		goto end;
	}

	table = object->object.table;
	if (!table->folderID) goto end;

	if (table->numerator < table->denominator) {
		mapi_repl->u.mapi_QueryPosition.Numerator = table->numerator;
	}
	else {
		mapi_repl->u.mapi_QueryPosition.Numerator = 0xffffffff;
	}
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
	uint32_t			handle;
	enum MAPISTATUS			retval;
	struct mapi_handles		*parent;
	struct emsmdbp_object		*object;
	struct emsmdbp_object_table	*table;
	void				*data;

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

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		DEBUG(5, ("  handle data not found, idx = %x\n", mapi_req->handle_idx));
		goto end;
	}
	object = (struct emsmdbp_object *) data;

	/* Ensure object exists and is table type */
	if (!object || (object->type != EMSMDBP_OBJECT_TABLE)) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  no object or object is not a table\n"));
		goto end;
	}

	/* We don't handle backward/forward yet , just go through the
	 * entire table, nor do we handle bookmarks */

	table = object->object.table;
	DEBUG(5, ("  handle_idx: %.8x, folder_id: %.16lx\n", mapi_req->handle_idx, table->folderID));
	if (mapi_req->u.mapi_SeekRow.origin == BOOKMARK_BEGINNING) {
          table->numerator = mapi_req->u.mapi_SeekRow.offset;
	}
	else {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		DEBUG(5, ("  unhandled 'origin' type: %d\n", mapi_req->u.mapi_SeekRow.origin));
	}
end:
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
	struct mapi_handles		*parent;
	struct emsmdbp_object		*object;
	struct emsmdbp_object_table	*table;
	struct FindRow_req		request;
	enum MAPISTATUS			retval;
	void				*data = NULL;
	enum MAPISTATUS			*retvals;
	void				**data_pointers;
	uint32_t			handle;
	DATA_BLOB			row;
	uint32_t			property;
	uint8_t				flagged;
	uint8_t				status = 0;
	uint32_t			i,j;
	bool				found = false;

	DEBUG(4, ("exchange_emsmdb: [OXCTABL] FindRow (0x4f)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = mapi_req->u.mapi_FindRow;
	
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_FindRow.RowNoLongerVisible = 0;
	mapi_repl->u.mapi_FindRow.HasRowData = 0;
	mapi_repl->u.mapi_FindRow.row.length = 0;
	mapi_repl->u.mapi_FindRow.row.data = NULL;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		DEBUG(5, ("  handle data not found, idx = %x\n", mapi_req->handle_idx));
		goto end;
	}
	object = (struct emsmdbp_object *) data;

	/* Ensure object exists and is table type */
	if (!object || (object->type != EMSMDBP_OBJECT_TABLE)) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  no object or object is not a table\n"));
		goto end;
	}

	/* We don't handle backward/forward yet , just go through the
	 * entire table, nor do we handle bookmarks */

	table = object->object.table;
	if (!table->folderID) {
		DEBUG(5, ("  no folderID\n"));
		goto end;
	}
	if (table->ulType == EMSMDBP_TABLE_RULE_TYPE) {
		DEBUG(5, ("  query on rules table are all faked right now\n"));
		goto end;
	}

	DEBUG(5, ("  handle_idx: %.8x, folder_id: %.16lx\n", mapi_req->handle_idx, table->folderID));

	if (mapi_req->u.mapi_FindRow.origin == BOOKMARK_BEGINNING) {
		table->numerator = 0;
	}
	if (mapi_req->u.mapi_FindRow.ulFlags == DIR_BACKWARD) {
		DEBUG(5, ("  only DIR_FORWARD is supported right now, using work-around\n"));
		table->numerator = 0;
	}

	switch (table->mapistore) {
	case true:
		memset (&row, 0, sizeof(DATA_BLOB));

		/* Restrict rows to be fetched */
		retval = mapistore_set_restrictions(emsmdbp_ctx->mstore_ctx, table->contextID,
						    table->folderID,
						    table->ulType,
						    &request.res, &status);
		/* Then fetch rows */
		/* Lookup the properties and check if we need to flag the PropertyRow blob */
		for (i = 0; !found && table->numerator < table->denominator; i++) {
			flagged = 0;

			retval = mapistore_get_table_property(emsmdbp_ctx->mstore_ctx, table->contextID,
							      table->ulType,
							      MAPISTORE_LIVEFILTERED_QUERY,
							      table->folderID,
							      (table->ulType == MAPISTORE_MESSAGE_TABLE) ? PR_MID : PR_FID,
							      table->numerator, &data);
			if (retval == MAPI_E_INVALID_OBJECT) {
				table->numerator++;
			}
			else {
				data_pointers = talloc_array(mem_ctx, void *, table->prop_count);
				retvals = talloc_array(mem_ctx, enum MAPISTATUS, table->prop_count);

				found = true;
				flagged = 0;

				/* Lookup for flagged property row */
				for (j = 0; j < table->prop_count; j++) {
					retval = mapistore_get_table_property(emsmdbp_ctx->mstore_ctx, table->contextID,
									      table->ulType,
									      MAPISTORE_LIVEFILTERED_QUERY,
									      table->folderID, 
									      table->properties[j],
									      table->numerator, data_pointers + j);
					retvals[j] = retval;
					if (retval == MAPI_E_NOT_FOUND) {
						flagged = 1;
					}
				}

				if (flagged) {
					libmapiserver_push_property(mem_ctx, 
								    lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
								    0x0000000b, (const void *)&flagged,
								    &row, 0, 0, 0);
				}
				else {
					libmapiserver_push_property(mem_ctx, 
								    lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
								    0x00000000, (const void *)&flagged,
								    &row, 0, 1, 0);
				}

				/* Push the properties */
				for (j = 0; j < table->prop_count; j++) {
					property = table->properties[j];
					retval = retvals[j];
					if (retval == MAPI_E_NOT_FOUND) {
						property = (property & 0xFFFF0000) + PT_ERROR;
						data = &retval;
					}
					else {
						data = data_pointers[j];
					}

					libmapiserver_push_property(mem_ctx, lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
								    property, data, &row,
								    flagged?PT_ERROR:0, flagged, 0);
				}

				talloc_free(retvals);
				talloc_free(data_pointers);
			}
		}

		retval = mapistore_set_restrictions(emsmdbp_ctx->mstore_ctx, table->contextID,
						    table->folderID,
						    table->ulType,
						    NULL, &status);

		/* Adjust parameters */
		if (row.length) {
			mapi_repl->u.mapi_FindRow.HasRowData = 1;
		}
		else {
                        mapi_repl->error_code = MAPI_E_NOT_FOUND;
		}

		mapi_repl->u.mapi_FindRow.row.length = row.length;
		mapi_repl->u.mapi_FindRow.row.data = row.data;

		break;
	case false:
		break;
	}

end:
	*size += libmapiserver_RopFindRow_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc ResetTable (0x81) Rop. This operation resets the
   table as follows:
     - Removes the existing column set, restriction, and sort order (ignored) from the table.
     - Invalidates bookmarks. (ignored)
     - Resets the cursor to the beginning of the table.

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
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopResetTable(TALLOC_CTX *mem_ctx,
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
	uint8_t				status; /* ignored */

	DEBUG(4, ("exchange_emsmdb: [OXCTABL] ResetTable (0x81)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	/* Initialize default empty ResetTable reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	*size += libmapiserver_RopResetTable_size(mapi_repl);

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		DEBUG(5, ("  handle data not found, idx = %x\n", mapi_req->handle_idx));
		goto end;
	}

	object = (struct emsmdbp_object *) data;
	/* Ensure referring object exists and is a table */
	if (!object || (object->type != EMSMDBP_OBJECT_TABLE)) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  missing object or not table\n"));
		goto end;
	}

	mapi_repl->error_code = MAPI_E_SUCCESS;

	table = object->object.table;
	if (table->ulType == EMSMDBP_TABLE_RULE_TYPE) {
		DEBUG(5, ("  query on rules table are all faked right now\n"));
	}
	else {
		/* 1.1. removes the existing column set */
		if (table->properties) {
			talloc_free(table->properties);
			table->properties = NULL;
			table->prop_count = 0;
		}

		/* 1.2. empty restrictions */
		if (table->mapistore) {
			retval = mapistore_set_restrictions(emsmdbp_ctx->mstore_ctx, table->contextID, 
							    table->folderID, table->ulType, NULL, &status);
		} else {
			DEBUG(0, ("  mapistore Restrict: Not implemented yet\n"));
			goto end;
		}

		/* 3. reset the cursor to the beginning of the table. */
		table->numerator = 0;
	}

end:

	return MAPI_E_SUCCESS;
}
