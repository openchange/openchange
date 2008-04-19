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
   \file IMAPIContainer.c

   \brief Containers and tables related operations
*/


/**
   \details Returns a pointer to a container's table object

   This function takes a pointer to a container object and returns a
   pointer to its associated contents

   \param obj_container the object to get the contents of
   \param obj_table the resulting table containing the container's
   contents.

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenFolder, GetHierarchyTable, GetLastError
*/
_PUBLIC_ enum MAPISTATUS GetContentsTable(mapi_object_t *obj_container, mapi_object_t *obj_table)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct GetContentsTable_req request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("GetContentsTable");

	/* Fill the GetContentsTable operation */
	request.handle_idx = 0x1;
	request.unknown = 0x0;
	size += 2;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetContentsTable;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetContentsTable = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_container);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* set handle */
	mapi_object_set_handle(obj_table, mapi_response->handles[1]);
	
	/* new table */
	mapi_object_table_init(obj_table);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Returns a pointer to a container's table object

   This function takes a pointer to a container object and returns a
   pointer to its associated hierarchy table

   \param obj_container the object to get the contents of
   \param obj_table the resulting table containing the container's
   hierarchy

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenFolder, GetContentsTable, GetLastError
*/
_PUBLIC_ enum MAPISTATUS GetHierarchyTable(mapi_object_t *obj_container, mapi_object_t *obj_table)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct GetHierarchyTable_req request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("GetHierarchyTable");

	/* Fill the GetHierarchyTable operation */
	request.handle_idx = 0x1;
	request.unknown = 0x0;
	size += 2;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetHierarchyTable;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetHierarchyTable = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_container);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* set handle */
	mapi_object_set_handle(obj_table, mapi_response->handles[1]);
	
	/* new table */
	mapi_object_table_init(obj_table);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Returns a pointer to the permission's table object.  

   This function takes a pointer to a container object and returns a
   pointer to its associated permission table

   \param obj_container the object to get the contents of
   \param obj_table the resulting table containing the container's
   permissions

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa ModifyTable
 */
_PUBLIC_ enum MAPISTATUS GetTable(mapi_object_t *obj_container, mapi_object_t *obj_table)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct GetTable_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_container, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("GetTable");

	/* Fill the GetTable operation */
	request.handle_idx = 0x1;
	request.padding = 0x0;
	size += 2;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetTable;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx= 0;
	mapi_req->u.mapi_GetTable = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_container);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* set handle */
	mapi_object_set_handle(obj_table, mapi_response->handles[1]);

	/* new table */
	mapi_object_table_init(obj_table);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Modify the entries of a permission table

   This function takes a pointer to a table object, a list of entries
   to modify and alter the permission table of its associated
   container. This function can be used to add, modify or remove
   permissions.

   \param obj_table the table containing the container's permissions
   \param rowList the list of table entries to modify

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetTable, AddUserPermission, ModifyUserPermission,
   RemoveUserPermission
 */
_PUBLIC_ enum MAPISTATUS ModifyTable(mapi_object_t *obj_table, struct mapi_SRowList *rowList)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct ModifyTable_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;
	uint32_t		i, j;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!rowList, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("ModifyTable");

	/* Fill the GetTable operation */
	request.rowList = *rowList;
	request.rowList.padding = 0;
	size += sizeof (uint8_t) + sizeof (uint16_t);

	for (i = 0; i < rowList->cEntries; i++) {
		size += sizeof (uint8_t);
			for (j = 0; j < rowList->aEntries[i].lpProps.cValues; j++) {
				size += get_mapi_property_size(&(rowList->aEntries[i].lpProps.lpProps[j]));
				size += sizeof (uint32_t);
			}
	size += sizeof (uint16_t);
	}

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_ModifyTable;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx= 0;
	mapi_req->u.mapi_ModifyTable = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}

/**
   \details Etablishes search criteria for the container

   \param obj_container the object we apply search criteria on
   \param res pointer to a mapi_SRestriction structure defining the
   search criteria
   \param ulSearchFlags bitmask of flags that controls how the search
   is performed
   \param lpContainerList pointer to a list of identifiers
   representing containers to be included in the search

   ulSearchFlags can take the following values:

   - BACKGROUND_SEARCH: Search run at normal priority relative to
     other searches. This flag is mutually exclusive with the
     FOREGROUND_SEARCH one.
   - FOREGROUND_SEARCH: Search run at high priority relative to other
     searches. This flag is mutually exclusive with the
     BACKGROUND_SEARCH one.
   - RECURSIVE_SEARCH: The search should include the containers
     specified in the lpContainerList parameter and all of their child
     container. This flag is mutually exclusive with the
     SHALLOW_SEARCH one.
   - RESTART_SEARCH: The search should be initiated, if this is the
     first call to SetSearchCriteria, or restarted, if the search is
     inactive. This flag is mutually exclusive with the STOP_SEARCH
     flag.
   - SHALLOW_SEARCH: The search should only look in the containers
     specified in the lpContainerList parameter for matching
     entries. This flag is mutually exclusive with the
     RECURSIVE_SEARCH one.
   - STOP_SEARCH: The search should be aborted. This flag is mutually
     exclusive with the RESTART_SEARCH one.

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
     
   \sa GetSearchCriteria
 */
_PUBLIC_ enum MAPISTATUS SetSearchCriteria(mapi_object_t *obj_container, 
					   struct mapi_SRestriction *res, 
					   uint32_t ulSearchFlags,
					   mapi_id_array_t *lpContainerList)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SetSearchCriteria_req	request;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	mapi_ctx_t			*mapi_ctx;
	
	/* Sanity checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_container, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!res, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("SetSearchCriteria");

	/* Fill the SetSearchCriteria operation */
	size = 0;
	request.res = *res;
	size += get_mapi_SRestriction_size(res);
	if (lpContainerList != NULL) {
		request.count = lpContainerList->count;
		size += sizeof (uint16_t);

		mapi_id_array_get(mem_ctx, lpContainerList, &request.lpContainerList);
		size += lpContainerList->count * sizeof (uint64_t);
	} else {
		request.count = 0;
		size += sizeof (uint16_t);
		request.lpContainerList = NULL;
	}
	request.ulSearchFlags = ulSearchFlags;
	size += sizeof (uint32_t);

	/* add subcontext size */
	size += sizeof (uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetSearchCriteria;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SetSearchCriteria = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_container);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Obtains the search criteria for a container

   \param obj_container the object we retrieve search criteria from
   \param res pointer to a mapi_SRestriction structure defining the
   search criteria 
   \param ulSearchFlags bitmask of flags that controls how the search
   is performed
   \param cValues number of lpContainerList entries
   \param lpContainerList pointer to a list of identifiers
   representing containers included in the search

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
     
   \sa SetSearchCriteria
 */
_PUBLIC_ enum MAPISTATUS GetSearchCriteria(mapi_object_t *obj_container,
					   struct mapi_SRestriction *res,
					   uint32_t *ulSearchFlags,
					   uint16_t *cValues,
					   uint64_t **lpContainerList)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetSearchCriteria_req	request;
	struct GetSearchCriteria_repl	*reply;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	mapi_ctx_t			*mapi_ctx;

	/* Sanity checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_container, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("GetSearchCriteria");

	/* Fill the GetSearchCriteria operation */
	size = 0;
	request.unknown[0] = 0x1;
	request.unknown[1] = 0x1;
	request.unknown[2] = 0x1;
	size += 3 * sizeof (uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetSearchCriteria;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetSearchCriteria = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_container);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	reply = &mapi_response->mapi_repl->u.mapi_GetSearchCriteria;

	res = &reply->res;
	*cValues = reply->count;
	*lpContainerList = reply->lpContainerList;
	*ulSearchFlags = reply->ulSearchFlags;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
