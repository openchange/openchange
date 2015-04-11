/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009-2013

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
	uint32_t			handle;

	OC_DEBUG(4, "exchange_emsmdb: [OXCTABL] SetColumns (0x12)\n");

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
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		OC_DEBUG(5, "  handle data not found, idx = %x\n", mapi_req->handle_idx);
		goto end;
	}

	object = (struct emsmdbp_object *) data;

	if (object) {
		table = object->object.table;
		OPENCHANGE_RETVAL_IF(!table, MAPI_E_INVALID_PARAMETER, NULL);

		if (table->ulType == MAPISTORE_RULE_TABLE) {
			OC_DEBUG(5, "  query on rules table are all faked right now\n");
			goto end;
		}

		request = mapi_req->u.mapi_SetColumns;

		if (request.prop_count) {
			table->prop_count = request.prop_count;
			table->properties = talloc_memdup(table, request.properties, 
							  request.prop_count * sizeof (uint32_t));
                        if (emsmdbp_is_mapistore(object)) {
				OC_DEBUG(5, "object: %p, backend_object: %p\n", object, object->backend_object);
				mapistore_table_set_columns(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(object),
							    object->backend_object, request.prop_count, request.properties);
                        } else {
				/* openchangedb case */
				OC_DEBUG(5, "object: Setting Columns on openchangedb table\n");
			}
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
	enum MAPISTATUS			retval;
	enum mapistore_error		mretval;
	struct mapi_handles		*parent;
	struct emsmdbp_object		*object;
	struct emsmdbp_object_table	*table;
	struct SortTable_req		*request;
	uint32_t			handle;
	void				*data = NULL;
	uint8_t				status;

	OC_DEBUG(4, "exchange_emsmdb: [OXCTABL] SortTable (0x13)\n");

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

	if ((mapi_req->u.mapi_SortTable.SortTableFlags & TBL_ASYNC)) {
		OC_DEBUG(5, "  requested async operation -> failure\n");
		mapi_repl->error_code = MAPI_E_UNKNOWN_FLAGS;
		goto end;
	}

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		OC_DEBUG(5, "  handle data not found, idx = %x\n", mapi_req->handle_idx);
		goto end;
	}
	object = (struct emsmdbp_object *) data;

	/* Ensure referring object exists and is a table */
	if (!object || (object->type != EMSMDBP_OBJECT_TABLE)) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  missing object or not table\n");
		goto end;
	}

	table = object->object.table;
	OPENCHANGE_RETVAL_IF(!table, MAPI_E_INVALID_PARAMETER, NULL);

	if (table->ulType != MAPISTORE_MESSAGE_TABLE
            && table->ulType != MAPISTORE_FAI_TABLE) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		OC_DEBUG(5, "  query performed on non contents table\n");
		goto end;
	}

	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);

        /* we reset the cursor to the beginning of the table */
        table->numerator = 0;

        /* TODO: we should invalidate current bookmarks on the table */

	/* If parent folder has a mapistore context */
	request = &mapi_req->u.mapi_SortTable;
	if (emsmdbp_is_mapistore(object)) {
		status = TBLSTAT_COMPLETE;
		mretval = mapistore_table_set_sort_order(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(object), object->backend_object, &request->lpSortCriteria, &status);
                if (mretval) {
			mapi_repl->error_code = mapistore_error_to_mapi(mretval);
			goto end;
		}
		mapi_repl->u.mapi_SortTable.TableStatus = status;
	} else {
		/* Parent folder doesn't have any mapistore context associated */
		status = TBLSTAT_COMPLETE;
		mapi_repl->u.mapi_SortTable.TableStatus = status;
		retval = openchangedb_table_set_sort_order(emsmdbp_ctx->oc_ctx, object->backend_object, &request->lpSortCriteria);
		if (retval) {
			mapi_repl->error_code = retval;
			goto end;
		}
	}
        
end:
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
	enum mapistore_error		mretval;
	struct mapi_handles		*parent;
	struct emsmdbp_object		*object;
	struct emsmdbp_object_table	*table;
	struct Restrict_req		request;
	uint32_t			handle, contextID;
	void				*data = NULL;
	uint8_t				status;

	OC_DEBUG(4, "exchange_emsmdb: [OXCTABL] Restrict (0x14)\n");

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
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		OC_DEBUG(5, "  handle data not found, idx = %x\n", mapi_req->handle_idx);
		goto end;
	}
	object = (struct emsmdbp_object *) data;

	/* Ensure referring object exists and is a table */
	if (!object || (object->type != EMSMDBP_OBJECT_TABLE)) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  missing object or not table\n");
		goto end;
	}

	table = object->object.table;
	OPENCHANGE_RETVAL_IF(!table, MAPI_E_INVALID_PARAMETER, NULL);

	table->restricted = true;
	if (table->ulType == MAPISTORE_RULE_TABLE) {
		OC_DEBUG(5, "  query on rules table are all faked right now\n");
		goto end;
	}
 
	/* If parent folder has a mapistore context */
	if (emsmdbp_is_mapistore(object)) {
		status = TBLSTAT_COMPLETE;
		contextID = emsmdbp_get_contextID(object);
		mretval = mapistore_table_set_restrictions(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object, &request.restrictions, &status);
                if (mretval) {
                        mapi_repl->error_code = (enum MAPISTATUS) mretval;
			goto end;
		}

		table->numerator = 0;
		mapistore_table_get_row_count(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object, MAPISTORE_PREFILTERED_QUERY, &object->object.table->denominator);

		mapi_repl->u.mapi_Restrict.TableStatus = status;

		/* Parent folder doesn't have any mapistore context associated */
	} else {
		OC_DEBUG(0, "not mapistore Restrict: Not implemented yet\n");
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
	struct QueryRows_req		*request;
	struct QueryRows_repl		*response;
	enum MAPISTATUS			retval;
	void				*data;
	enum MAPISTATUS			*retvals;
	enum mapistore_error		mretval;
	uint64_t			folderID;
	void				**data_pointers;
	uint32_t			count;
	uint32_t			handle;
	uint16_t			flags = 0;
	int64_t			        i = 0, end;

	OC_DEBUG(4, "exchange_emsmdb: [OXCTABL] QueryRows (0x15)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = &mapi_req->u.mapi_QueryRows;
	response = &mapi_repl->u.mapi_QueryRows;

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_NOT_FOUND;
	
	response->RowData.length = 0;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		OC_DEBUG(5, "  handle data not found, idx = %x\n", mapi_req->handle_idx);
		goto end;
	}

	object = (struct emsmdbp_object *) data;

	/* Ensure referring object exists and is a table */
	if (!object) {
		OC_DEBUG(5, "  missing object\n");
		goto end;
	}
	if (object->type != EMSMDBP_OBJECT_TABLE) {
		OC_DEBUG(5, "  unhandled object type: %d\n", object->type);
		goto end;
	}

	table = object->object.table;

	count = 0;
	if (table->ulType == MAPISTORE_RULE_TABLE) {
		OC_DEBUG(5, "  query on rules table are all faked right now\n");
		goto finish;
	}

	/* Lookup the properties */
	if (request->ForwardRead) {
		end = table->numerator + request->RowCount;
		if (end > table->denominator) {
			end = table->denominator;
		}
	} else {
		if (table->numerator < request->RowCount) {
			end = -1;
		} else {
			end = table->numerator - request->RowCount;
		}
	}

	if (table->flags & TableFlags_Depth) {
		struct SPropTagArray		SPropTagArray;

		SPropTagArray.cValues = table->prop_count;
		SPropTagArray.aulPropTag = table->properties;

		switch (table->numerator) {
		case 0x0:
			count = 0;
			retval = emsmdbp_object_table_get_recursive_row_props(mem_ctx, emsmdbp_ctx, object, &response->RowData,
									      &SPropTagArray, 0, &end, &count);
			if (retval != MAPI_E_SUCCESS) {
				OC_DEBUG(OC_LOG_WARNING, "Unable to retrieve recursive folder rows");
				count = 0;
			}
			break;
		default:
			OC_DEBUG(OC_LOG_WARNING, "Can not move cursor with Depth flag enabled");
			count = 0;
			break;
		}
	} else {
		i = table->numerator;
		while (i != end) {
			data_pointers = emsmdbp_object_table_get_row_props(mem_ctx, emsmdbp_ctx, object, i, MAPISTORE_PREFILTERED_QUERY, &retvals);
			if (data_pointers) {
				emsmdbp_fill_table_row_blob(mem_ctx, emsmdbp_ctx,
							    &response->RowData, table->prop_count,
							    table->properties, data_pointers, retvals);
				talloc_free(retvals);
				talloc_free(data_pointers);
				count++;
			} else {
				count = 0;
				goto finish;
			}
			i = (request->ForwardRead) ? i + 1 : i - 1;
		}
	}

finish:
	if ((request->QueryRowsFlags & TBL_NOADVANCE) != TBL_NOADVANCE) {
		table->numerator = (i < 0) ? 0 : i;
	}

	/* QueryRows reply parameters */
	mapi_repl->error_code = MAPI_E_SUCCESS;
	response->RowCount = count;
	if (count) {
		if (count < request->RowCount) {
			response->Origin = (request->ForwardRead) ? BOOKMARK_END : BOOKMARK_BEGINNING;
		} else if (table->numerator > (table->denominator - 2)) {
			response->Origin = BOOKMARK_END;
		} else if (i < 0) {
			response->Origin = BOOKMARK_BEGINNING;
		} else {
			response->Origin = BOOKMARK_CURRENT;
		}
		/* dump_data(0, response.RowData.data, response.RowData.length); */
	} else {
		/* useless code for the moment */
		if (table->restricted) {
			response->Origin = BOOKMARK_BEGINNING;
		}
		else {
			response->Origin = (request->ForwardRead) ? BOOKMARK_END : BOOKMARK_BEGINNING;
		}
		response->RowData.length = 0;
		response->RowData.data = NULL;
		OC_DEBUG(5, "returning empty data set\n");
	}

	/* Add notification TableModified subscription */
	switch (object->parent_object->type) {
	case EMSMDBP_OBJECT_MAILBOX:
		if (table->flags & TableFlags_Depth) {
			/* FIXME: This statement is partially incorrect. TableFlags_Depth triggers
			 * TableModified notification for every child's table object created underneath,
			 * while WholeStore implies everything beneath and underneath.
			 */
			flags = 0x1;
		}
		folderID = object->parent_object->object.mailbox->folderID;
		break;
	case EMSMDBP_OBJECT_FOLDER:
		folderID = object->parent_object->object.folder->folderID;
		break;
	default:
		folderID = 0;
		break;
	}
	if (folderID) {
		flags |= fnevTableModified;
		mretval = mapistore_notification_subscription_add(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->session_uuid,
								  handle, flags, folderID, 0, table->prop_count,
								  table->properties);
		if (mretval != MAPISTORE_SUCCESS) {
			OC_DEBUG(0, "Failed to add subscription");
		} else {
			table->subscription = true;
		}
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

	OC_DEBUG(4, "exchange_emsmdb: [OXCTABL] QueryPosition (0x17)\n");

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
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		OC_DEBUG(5, "  no private data or object is not a table");
		goto end;
	}
	object = (struct emsmdbp_object *) data;

	/* Ensure object exists and is table type */
	if (!object || (object->type != EMSMDBP_OBJECT_TABLE)) {
		OC_DEBUG(5, "  no object or object is not a table\n");
		goto end;
	}

	table = object->object.table;

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
	uint32_t			handle;
	enum MAPISTATUS			retval;
	struct mapi_handles		*parent;
	struct emsmdbp_object		*object;
	struct emsmdbp_object_table	*table;
	void				*data;
	int32_t                         next_position;

	OC_DEBUG(4, "exchange_emsmdb: [OXCTABL] SeekRow (0x18)\n");

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
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		OC_DEBUG(5, "  handle data not found, idx = %x\n", mapi_req->handle_idx);
		goto end;
	}
	object = (struct emsmdbp_object *) data;

	/* Ensure object exists and is table type */
	if (!object || (object->type != EMSMDBP_OBJECT_TABLE)) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  no object or object is not a table\n");
		goto end;
	}

	/* We don't handle backward/forward yet , just go through the
	 * entire table, nor do we handle bookmarks */

	table = object->object.table;
	if (mapi_req->u.mapi_SeekRow.origin == BOOKMARK_BEGINNING) {
                next_position = mapi_req->u.mapi_SeekRow.offset;
	}
	else if (mapi_req->u.mapi_SeekRow.origin == BOOKMARK_CURRENT) {
                next_position = table->numerator + mapi_req->u.mapi_SeekRow.offset;
	}
	else if (mapi_req->u.mapi_SeekRow.origin == BOOKMARK_END) {
                next_position = table->denominator - 1 + mapi_req->u.mapi_SeekRow.offset;
	}
	else {
                next_position = 0;
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		OC_DEBUG(5, "  unhandled 'origin' type: %d\n", mapi_req->u.mapi_SeekRow.origin);
	}

	if (mapi_repl->error_code == MAPI_E_SUCCESS) {
		if (next_position < 0) {
			next_position = 0;
			mapi_repl->u.mapi_SeekRow.HasSoughtLess = 1;
		}
		else if (next_position >= table->denominator) {
			next_position = table->denominator - 1;
			mapi_repl->u.mapi_SeekRow.HasSoughtLess = 1;
		}
		if (mapi_req->u.mapi_SeekRow.WantRowMovedCount) {
			mapi_repl->u.mapi_SeekRow.RowsSought = (next_position - table->numerator);
		}
		else {
			mapi_repl->u.mapi_SeekRow.RowsSought = 0;
		}
		table->numerator = next_position;
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
	enum mapistore_error		mretval;
	void				*data = NULL;
	enum MAPISTATUS			*retvals;
	void				**data_pointers;
	uint32_t			handle;
	DATA_BLOB			row;
	uint32_t			property;
	uint8_t				flagged;
	uint8_t				status = 0;
	uint32_t			i;
	bool				found = false;

	OC_DEBUG(4, "exchange_emsmdb: [OXCTABL] FindRow (0x4f)\n");

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
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		OC_DEBUG(5, "  handle data not found, idx = %x\n", mapi_req->handle_idx);
		goto end;
	}
	object = (struct emsmdbp_object *) data;

	/* Ensure object exists and is table type */
	if (!object || (object->type != EMSMDBP_OBJECT_TABLE)) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  no object or object is not a table\n");
		goto end;
	}

	/* We don't handle backward/forward yet , just go through the
	 * entire table, nor do we handle bookmarks */

	table = object->object.table;
	if (table->ulType == MAPISTORE_RULE_TABLE) {
		OC_DEBUG(5, "  query on rules table are all faked right now\n");
		goto end;
	}

	if (mapi_req->u.mapi_FindRow.origin == BOOKMARK_BEGINNING) {
		table->numerator = 0;
	}
	if (mapi_req->u.mapi_FindRow.ulFlags == DIR_BACKWARD) {
		OC_DEBUG(5, "  only DIR_FORWARD is supported right now, using work-around\n");
		table->numerator = 0;
	}

	memset (&row, 0, sizeof(DATA_BLOB));

	switch ((int)emsmdbp_is_mapistore(object)) {
	case true:
		/* Restrict rows to be fetched */
		mretval = mapistore_table_set_restrictions(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(object), object->backend_object, &request.res, &status);
		if (mretval != MAPISTORE_SUCCESS) {
			OC_DEBUG(5, "mapistore_table_set_restrictions: %s\n", mapistore_errstr(mretval));
		}
		/* Then fetch rows */
		/* Lookup the properties and check if we need to flag the PropertyRow blob */

		while (!found && table->numerator < table->denominator) {
                        flagged = 0;

			data_pointers = emsmdbp_object_table_get_row_props(NULL, emsmdbp_ctx, object, table->numerator, MAPISTORE_LIVEFILTERED_QUERY, &retvals);
			if (data_pointers) {
				found = true;
				for (i = 0; i < table->prop_count; i++) {
					if (retvals[i] != MAPI_E_SUCCESS) {
						flagged = 1;
					}
				}

				if (flagged) {
					libmapiserver_push_property(mem_ctx, 
								    0x0000000b, (const void *)&flagged,
								    &row, 0, 0, 0);
				}
				else {
					libmapiserver_push_property(mem_ctx, 
								    0x00000000, (const void *)&flagged,
								    &row, 0, 1, 0);
				}
                                
				/* Push the properties */
				for (i = 0; i < table->prop_count; i++) {
					property = table->properties[i];
					retval = retvals[i];
					if (retval == MAPI_E_NOT_FOUND) {
						property = (property & 0xFFFF0000) + PT_ERROR;
						data = &retval;
					}
					else {
						data = data_pointers[i];
					}
                                
					libmapiserver_push_property(mem_ctx,
								    property, data, &row,
								    flagged?PT_ERROR:0, flagged, 0);
				}
				talloc_free(retvals);
				talloc_free(data_pointers);
                        }
                        else {
				table->numerator++;
			}
		}

		mretval = mapistore_table_set_restrictions(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(object), object->backend_object, NULL, &status);
		if (mretval != MAPISTORE_SUCCESS) {
			OC_DEBUG(5, "mapistore_table_set_restrictions: %s\n", mapistore_errstr(mretval));
		}

		/* Adjust parameters */
		if (found) {
			mapi_repl->u.mapi_FindRow.HasRowData = 1;
		}
		else {
                        mapi_repl->error_code = MAPI_E_NOT_FOUND;
		}

		mapi_repl->u.mapi_FindRow.row.length = row.length;
		mapi_repl->u.mapi_FindRow.row.data = row.data;

		break;
	case false:
		OC_DEBUG(0, "FindRow for openchangedb\n");
		/* Restrict rows to be fetched */
		retval = openchangedb_table_set_restrictions(emsmdbp_ctx->oc_ctx, object->backend_object, &request.res);
		/* Then fetch rows */
		/* Lookup the properties and check if we need to flag the PropertyRow blob */
		while (!found && table->numerator < table->denominator) {
                        flagged = 0;

			data_pointers = emsmdbp_object_table_get_row_props(NULL, emsmdbp_ctx, object, table->numerator, MAPISTORE_LIVEFILTERED_QUERY, &retvals);
			if (data_pointers) {
				found = true;
				for (i = 0; i < table->prop_count; i++) {
					if (retvals[i] != MAPI_E_SUCCESS) {
						flagged = 1;
					}
				}

				if (flagged) {
					libmapiserver_push_property(mem_ctx, 
								    0x0000000b, (const void *)&flagged,
								    &row, 0, 0, 0);
				}
				else {
					libmapiserver_push_property(mem_ctx, 
								    0x00000000, (const void *)&flagged,
								    &row, 0, 1, 0);
				}
                                
				/* Push the properties */
				for (i = 0; i < table->prop_count; i++) {
					property = table->properties[i];
					retval = retvals[i];
					if (retval == MAPI_E_NOT_FOUND) {
						property = (property & 0xFFFF0000) + PT_ERROR;
						data = &retval;
					}
					else {
						data = data_pointers[i];
					}
                                
					libmapiserver_push_property(mem_ctx,
								    property, data, &row,
								    flagged?PT_ERROR:0, flagged, 0);
				}
				talloc_free(retvals);
				talloc_free(data_pointers);
                        } else {
				table->numerator++;
			}
		}
		/* Reset restrictions */
		openchangedb_table_set_restrictions(emsmdbp_ctx->oc_ctx, object->backend_object, NULL);

		/* Adjust parameters */
		if (found) {
			mapi_repl->u.mapi_FindRow.HasRowData = 1;
		}
		else {
                        mapi_repl->error_code = MAPI_E_NOT_FOUND;
		}

		mapi_repl->u.mapi_FindRow.row.length = row.length;
		mapi_repl->u.mapi_FindRow.row.data = row.data;
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
	enum mapistore_error		mretval;
	struct mapi_handles		*parent;
	struct emsmdbp_object		*object;
	struct emsmdbp_object_table	*table;
	void				*data;
	uint32_t			handle, contextID;
	uint8_t				status; /* ignored */

	OC_DEBUG(4, "exchange_emsmdb: [OXCTABL] ResetTable (0x81)\n");

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
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		OC_DEBUG(5, "  handle data not found, idx = %x\n", mapi_req->handle_idx);
		goto end;
	}

	object = (struct emsmdbp_object *) data;
	/* Ensure referring object exists and is a table */
	if (!object || (object->type != EMSMDBP_OBJECT_TABLE)) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  missing object or not table\n");
		goto end;
	}

	mapi_repl->error_code = MAPI_E_SUCCESS;

	table = object->object.table;
	if (table->ulType == MAPISTORE_RULE_TABLE) {
		OC_DEBUG(5, "  query on rules table are all faked right now\n");
	}
	else {
		/* 1.1. removes the existing column set */
		if (table->properties) {
			talloc_free(table->properties);
			table->properties = NULL;
			table->prop_count = 0;
		}

		/* 1.2. empty restrictions */
		if (emsmdbp_is_mapistore(object)) {
			contextID = emsmdbp_get_contextID(object);
			mretval = mapistore_table_set_restrictions(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object, NULL, &status);
			if (mretval != MAPISTORE_SUCCESS) {
				OC_DEBUG(5, "mapistore_table_set_restrictions: %s\n", mapistore_errstr(mretval));
			}
			mapistore_table_get_row_count(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object, MAPISTORE_PREFILTERED_QUERY, &object->object.table->denominator);
		} else {
			OC_DEBUG(0, "  mapistore Restrict: Not implemented yet\n");
			goto end;
		}

		/* 3. reset the cursor to the beginning of the table. */
		table->numerator = 0;
	}

end:

	return MAPI_E_SUCCESS;
}
