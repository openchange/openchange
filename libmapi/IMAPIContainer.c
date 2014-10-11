/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2008.

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
   \param TableFlags flags controlling the type of table
   \param RowCount the number of rows in the hierarchy table

   TableFlags possible values:

   - TableFlags_Associated (0x2): Get the contents table for "Folder Associated
   Information" messages, rather than normal messages.

   - TableFlags_DeferredErrors (0x8): The call response can return immediately,
   possibly before the call execution is complete and in this case the
   ReturnValue as well the RowCount fields in the return buffer might
   not be accurate.  Only retval reporting failure can be considered
   valid in this case.

   - TableFlags_NoNotifications (0x10): Disables all notifications on .this Table
   object.

   - TableFlags_SoftDeletes (0x20): Enables the client to get a list of the soft
   deleted folders.

   - TableFlags_UseUnicode (0x40): Requests that the columns that contain string
   data be returned in Unicode format.

   - TableFlags_SuppressNotifications (0x80): Suppresses notifications generated
   by this client’s actions on this Table object.

   Developers can either set RowCount to a valid pointer on uint32_t
   or set it to NULL. In this last case, GetHierarchyTable won't
   return any value to the calling function.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenFolder, GetHierarchyTable, GetLastError
*/
_PUBLIC_ enum MAPISTATUS GetContentsTable(mapi_object_t *obj_container, mapi_object_t *obj_table,
					  uint8_t TableFlags, uint32_t *RowCount)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetContentsTable_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_container, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_container);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_container, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "GetContentsTable");
	size = 0;

	/* Fill the GetContentsTable operation */
	request.handle_idx = 0x1;
	request.TableFlags = TableFlags;
	size += 2;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetContentsTable;
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* set object session, handle and logon_id */
	mapi_object_set_session(obj_table, session);
	mapi_object_set_handle(obj_table, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_table, logon_id);

	/* Retrieve RowCount if a valid pointer was set */
	if (RowCount) {
		*RowCount = mapi_response->mapi_repl->u.mapi_GetContentsTable.RowCount;
	}
	
	/* new table */
	mapi_object_table_init((TALLOC_CTX *)session, obj_table);

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
   \param TableFlags flags controlling the type of table
   \param RowCount the number of rows in the hierarchy table

   TableFlags possible values:

   - TableFlags_Depth (0x4): Fills the hierarchy table with containers
   from all levels. If this flag is not set, the hierarchy table
   contains only the container's immediate child containers.

   - TableFlags_DeferredErrors (0x8): The call response can return immediately,
   possibly before the call execution is complete and in this case the
   ReturnValue as well the RowCount fields in the return buffer might
   not be accurate.  Only retval reporting failure can be considered
   valid in this case.

   - TableFlags_NoNotifications (0x10): Disables all notifications on .this Table
   object.

   - TableFlags_SoftDeletes (0x20): Enables the client to get a list of the soft
   deleted folders.

   - TableFlags_UseUnicode (0x40): Requests that the columns that contain string
   data be returned in Unicode format.

   - TableFlags_SuppressNotifications (0x80): Suppresses notifications generated
   by this client’s actions on this Table object.

   Developers can either set RowCount to a valid pointer on uint32_t
   or set it to NULL. In this last case, GetHierarchyTable won't
   return any value to the calling function.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenFolder, GetContentsTable, GetLastError
*/
_PUBLIC_ enum MAPISTATUS GetHierarchyTable(mapi_object_t *obj_container, mapi_object_t *obj_table,
					   uint8_t TableFlags, uint32_t *RowCount)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetHierarchyTable_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_container, MAPI_E_INVALID_PARAMETER, NULL);
	
	session = mapi_object_get_session(obj_container);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_container, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "GetHierarchyTable");
	size = 0;

	/* Fill the GetHierarchyTable operation */
	request.handle_idx = 0x1;
	request.TableFlags = TableFlags;
	size += 2;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetHierarchyTable;
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* set object session, handle and logon_id */
	mapi_object_set_session(obj_table, session);
	mapi_object_set_handle(obj_table, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_table, logon_id);

	/* Retrieve RowCount if a valid pointer was set */
	if (RowCount) {
		*RowCount = mapi_response->mapi_repl->u.mapi_GetHierarchyTable.RowCount;
	}

	/* new table */
	mapi_object_table_init((TALLOC_CTX *)session, obj_table);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Returns a pointer to the permission's table object.  

   This function takes a pointer to a container object and returns a
   pointer to its associated permission table

   \param obj_container the object to get the contents of
   \param flags any special flags to pass
   \param obj_table the resulting table containing the container's
   permissions
   
   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   The only meaningful value for flags is IncludeFreeBusy (0x02). This
   should be set when getting permissions on the Calendar folder when
   using Exchange 2007 and later. It should not be set in other situations.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa ModifyPermissions
 */
_PUBLIC_ enum MAPISTATUS GetPermissionsTable(mapi_object_t *obj_container, uint8_t flags, mapi_object_t *obj_table)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetPermissionsTable_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_container, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_container);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_container, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "GetPermissionsTable");
	size = 0;

	/* Fill the GetPermissionsTable operation */
	request.handle_idx = 0x1;
	request.TableFlags = flags;
	size += 2;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetPermissionsTable;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx= 0;
	mapi_req->u.mapi_GetPermissionsTable = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_container);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* set object session, handle and logon_id */
	mapi_object_set_session(obj_table, session);
	mapi_object_set_handle(obj_table, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_table, logon_id);

	/* new table */
	mapi_object_table_init((TALLOC_CTX *)session, obj_table);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Gets the rules table of a folder

   \param obj_folder the folder we want to retrieve the rules table
   from
   \param obj_table the rules table
   \param TableFlags bitmask associated to the rules table

   Possible values for TableFlags:

   - RulesTableFlags_Unicode (0x40): Set if the client is requesting
     that string values in the table to be returned as Unicode
     strings.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

    \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction  
 */
_PUBLIC_ enum MAPISTATUS GetRulesTable(mapi_object_t *obj_folder, 
				       mapi_object_t *obj_table,
				       uint8_t TableFlags)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetRulesTable_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	uint8_t				logon_id;

	/* Sanity check */
	OPENCHANGE_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_folder);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_folder, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "GetRulesTable");
	size = 0;

	/* Fill the GetRulesTable operation */
	request.handle_idx = 0x1;
	size += sizeof (uint8_t);

	request.TableFlags = TableFlags;
	size += sizeof (uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetRulesTable;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetRulesTable = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_folder);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* set object session, handle and logon_id */
	mapi_object_set_session(obj_table, session);
	mapi_object_set_handle(obj_table, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_table, logon_id);

	/* new table */
	mapi_object_table_init((TALLOC_CTX *)session, obj_table);

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
   \param flags any special flags to use
   \param permsdata the list of permissions table entries to modify

   Possible values for flags:

   - 0x02 for IncludeFreeBusy.  This should be set when modifying permissions
   on the Calendar folder when using Exchange 2007 and later. It should not
   be set in other situations.
   - 0x01 for ReplaceRows. This means "remove all current permissions and use
   this set instead", so the permsdata must consist of ROW_ADD operations.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetPermissionsTable, AddUserPermission, ModifyUserPermission,
   RemoveUserPermission
 */
_PUBLIC_ enum MAPISTATUS ModifyPermissions(mapi_object_t *obj_table, uint8_t flags, struct mapi_PermissionsData *permsdata)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct ModifyPermissions_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint32_t			i, j;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!permsdata, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_table);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_table, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "ModifyPermissions");

	/* Fill the ModifyPermissions operation */
	request.rowList = *permsdata;
	request.rowList.ModifyFlags = flags;
	size += sizeof (uint8_t) + sizeof (uint16_t);

	for (i = 0; i < permsdata->ModifyCount; i++) {
		size += sizeof (uint8_t);
			for (j = 0; j < permsdata->PermissionsData[i].lpProps.cValues; j++) {
				size += get_mapi_property_size(&(permsdata->PermissionsData[i].lpProps.lpProps[j]));
				size += sizeof (uint32_t);
			}
	size += sizeof (uint16_t);
	}

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_ModifyPermissions;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx= 0;
	mapi_req->u.mapi_ModifyPermissions = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_table);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}

/**
   \details Establishes search criteria for the container

   \param obj_container the object we apply search criteria to
   \param res pointer to a mapi_SRestriction structure defining the
   search criteria
   \param SearchFlags bitmask of flags that controls how the search
   is performed
   \param lpContainerList pointer to a list of identifiers
   representing containers to be included in the search

   SearchFlags can take the following values:

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

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: One or more parameters were invalid (usually null pointer)
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
     
   \sa GetSearchCriteria
 */
_PUBLIC_ enum MAPISTATUS SetSearchCriteria(mapi_object_t *obj_container, 
					   struct mapi_SRestriction *res, 
					   uint32_t SearchFlags,
					   mapi_id_array_t *lpContainerList)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SetSearchCriteria_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_container, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!res, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_container);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_container, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "SetSearchCriteria");
	size = 0;

	/* Fill the SetSearchCriteria operation */
	request.res = *res;
	size += get_mapi_SRestriction_size(res);
	if (lpContainerList != NULL) {
		request.FolderIdCount = lpContainerList->count;
		size += sizeof (uint16_t);

		mapi_id_array_get(mem_ctx, lpContainerList, &request.FolderIds);
		size += lpContainerList->count * sizeof (int64_t);
	} else {
		request.FolderIdCount = 0;
		size += sizeof (uint16_t);
		request.FolderIds = NULL;
	}
	request.SearchFlags = SearchFlags;
	size += sizeof (uint32_t);

	/* add subcontext size */
	size += sizeof (uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetSearchCriteria;
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Obtains the search criteria for a container

   \param obj_container the object we retrieve search criteria from
   \param res pointer to a mapi_SRestriction structure defining the
   search criteria 
   \param SearchFlags bitmask of flags that controls how the search
   is performed
   \param FolderIdCount number of FolderIds entries
   \param FolderIds pointer to a list of identifiers
   representing containers included in the search

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
     
   \sa SetSearchCriteria
 */
_PUBLIC_ enum MAPISTATUS GetSearchCriteria(mapi_object_t *obj_container,
					   struct mapi_SRestriction *res,
					   uint32_t *SearchFlags,
					   uint16_t *FolderIdCount,
					   int64_t **FolderIds)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetSearchCriteria_req	request;
	struct GetSearchCriteria_repl	*reply;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_container, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!SearchFlags, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!FolderIdCount, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!FolderIds, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_container);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_container, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "GetSearchCriteria");
	size = 0;

	/* Fill the GetSearchCriteria operation */
	request.UseUnicode = 0x1;
	request.IncludeRestriction = 0x1;
	request.IncludeFolders = 0x1;
	size += 3 * sizeof (uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetSearchCriteria;
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	reply = &mapi_response->mapi_repl->u.mapi_GetSearchCriteria;

	res = &reply->RestrictionData;
	*FolderIdCount = reply->FolderIdCount;
	*FolderIds = talloc_steal((TALLOC_CTX *)session, reply->FolderIds);
	*SearchFlags = reply->SearchFlags;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
