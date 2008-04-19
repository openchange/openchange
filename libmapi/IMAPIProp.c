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
#include <gen_ndr/ndr_exchange.h>


/**
   \file IMAPIProp.c

   \brief Properties and named properties operations.
 */


/**
   \details Returns values of one or more properties for an object
 
   The function takes a pointer on the object obj, a MAPITAGS array
   specified in mapitags, and the count of properties.  The function
   returns associated values within the SPropValue values pointer.

   \param obj the object to get properties on
   \param tags an array of MAPI property tags
   \param vals the result of the query
   \param cn_vals the count of property tags

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa SetProps, GetPropList, GetPropsAll, DeleteProps, SaveChanges,
   GetLastError
*/
_PUBLIC_ enum MAPISTATUS GetProps(mapi_object_t *obj, 
				  struct SPropTagArray *tags,
				  struct SPropValue **vals, uint32_t *cn_vals)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct GetProps_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	enum MAPISTATUS		mapistatus;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("GetProps");

	/* Reset */
	*cn_vals = 0;
	*vals = 0;
	size = 0;

	/* Fill the GetProps operation */
	request.unknown = 0x0;
	request.prop_count = (uint16_t)tags->cValues;
	request.properties = tags->aulPropTag;
	size = sizeof(uint32_t) + sizeof(uint16_t) + request.prop_count * sizeof(uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetProps;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetProps = request;
	size += 5;
       
	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF((retval && retval != MAPI_W_ERRORS_RETURNED), retval, mem_ctx);
	
	/* Read the SPropValue array from data blob.
	   fixme: replace the memory context by the object one.
	*/
	mapistatus = emsmdb_get_SPropValue((TALLOC_CTX *)mapi_ctx->session,
					   &mapi_response->mapi_repl->u.mapi_GetProps.prop_data,
					   tags, vals, cn_vals, 
					   mapi_response->mapi_repl->u.mapi_GetProps.layout);
	MAPI_RETVAL_IF(!mapistatus && (retval == MAPI_W_ERRORS_RETURNED), retval, mem_ctx);
	
	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}


/**
   \details Set one or more properties on a given object

   This function sets one or more properties on a specified object.

   \param obj the object to set properties on
   \param sprops the list of properties to set
   \param cn_props the number of properties

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetProps, GetPropList, GetPropsAll, DeleteProps, SaveChanges,
   GetLastError
*/
_PUBLIC_ enum MAPISTATUS SetProps(mapi_object_t *obj, struct SPropValue *sprops, 
				  unsigned long cn_props)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SetProps_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	unsigned long		i_prop;
	struct mapi_SPropValue	*mapi_props;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("SetProps");

	/* build the array */
	request.values.lpProps = talloc_array(mem_ctx, struct mapi_SPropValue, cn_props);
	mapi_props = request.values.lpProps;
	for (i_prop = 0; i_prop < cn_props; i_prop++) {
		size += cast_mapi_SPropValue(&mapi_props[i_prop], &sprops[i_prop]);
		size += sizeof(uint32_t);
	}

	request.values.cValues = cn_props;
	size += sizeof(uint16_t);

	/* add the size of the subcontext that will be added on ndr layer */
	size += sizeof(uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetProps;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SetProps = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Makes permanent any changes made to an object since the
   last save operation.

   \param obj_parent the parent of the object to save changes for
   \param obj_child the object to save changes for
   \param flags the access flags to set on the saved object 

   Possible flags:
   - KEEP_OPEN_READONLY
   - KEEP_OPEN_READWRITE
   - FORCE_SAVE

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa SetProps, ModifyRecipients, GetLastError
 */
_PUBLIC_ enum MAPISTATUS SaveChanges(mapi_object_t *obj_parent, 
				     mapi_object_t *obj_child,
				     uint8_t flags)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SaveChanges_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("SaveChanges");

	size = 0;

	/* Fill the SaveChanges operation */
	request.handle_idx = 0x0;
	request.ulFlags = flags;
	size += sizeof(uint8_t) + sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SaveChanges;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SaveChanges = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_child);
	mapi_request->handles[1] = mapi_object_get_handle(obj_parent);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve all the properties associated with a given object

   \param obj the object to retrieve properties for
   \param proptags the resulting list of properties associated with
   the object

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:   
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetProps, GetPropsAll, GetLastError
 */
_PUBLIC_ enum MAPISTATUS GetPropList(mapi_object_t *obj, 
				     struct SPropTagArray *proptags)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("GetPropList");

	/* Reset */
	proptags->cValues = 0;
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetPropList;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 1;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* Get the repsonse */
	proptags->cValues = mapi_response->mapi_repl->u.mapi_GetPropList.count;
	if (proptags->cValues) {
		size = proptags->cValues * sizeof(enum MAPITAGS);
		proptags->aulPropTag = talloc_array((TALLOC_CTX *)mapi_ctx->session,
						    enum MAPITAGS, proptags->cValues);
		memcpy((void*)proptags->aulPropTag,
		       (void*)mapi_response->mapi_repl->u.mapi_GetPropList.tags,
		       size);
	}

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve all properties and values associated with an
   object

   This function returns all the properties and and associated values
   for a given object.

   \param obj the object to get the properties for
   \param properties the properties / values for the object

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetProps, GetPropList, GetLastError
*/
_PUBLIC_ enum MAPISTATUS GetPropsAll(mapi_object_t *obj,
				     struct mapi_SPropValue_array *properties)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct GetPropsAll_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("GetPropsAll");

	/* Fill the GetPropsAll operation */
	size = 0;
	request.dwAlignPad = 0;
	size += sizeof (uint16_t);
	request.unknown = 0;
	size += sizeof (uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetPropsAll;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetPropsAll = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	*properties = mapi_response->mapi_repl->u.mapi_GetPropsAll.properties;

	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}


/**
   \details Delete one or more properties from an object

   \param obj the object to remove properties from
   \param tags the properties to remove from the given object
       
   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa SetProps, GetLastError
*/
_PUBLIC_ enum MAPISTATUS DeleteProps(mapi_object_t *obj, 
				     struct SPropTagArray *tags)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct DeleteProps_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx; 
	mem_ctx = talloc_init("DeleteProps");

	/* Fill the DeleteProps operation */
	size = 0;
	request.count = tags->cValues;
	size += sizeof(uint16_t);
	request.tags = tags->aulPropTag;
	size += tags->cValues * sizeof(enum MAPITAGS);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_DeleteProps;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_DeleteProps = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Provides the property names that correspond to one
   or more property identifiers.
   
   \param obj the object we are retrieving the names from
   \param ulPropTag the mapped property tag
   \param count count of property names pointed to by the nameid
   parameter returned by the server
   \param nameid pointer to a pointer to property names returned by
   the server

   ulPropTag must be a property with type set to PT_NULL
 
   \return MAPI_E_SUCCESS on success, otherwise -1.
  
   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetIDsFromNames, QueryNamesFromIDs
*/
_PUBLIC_ enum MAPISTATUS GetNamesFromIDs(mapi_object_t *obj,
					 uint32_t ulPropTag,
					 uint16_t *count,
					 struct MAPINAMEID **nameid)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetNamesFromIDs_req	request;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size= 0;
	TALLOC_CTX			*mem_ctx;
	mapi_ctx_t			*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	/* Initialization */
	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("GetNamesFromIDs");
	size = 0;

	/* Fill the GetNamesFromIDs operation */
	request.ulPropTag = ulPropTag;
	size += sizeof (uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetNamesFromIDs;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetNamesFromIDs = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, 
				    mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* Fill in count */
	*count = mapi_response->mapi_repl->u.mapi_GetNamesFromIDs.count;

	/* Fill MAPINAMEID struct */
	*nameid = mapi_response->mapi_repl->u.mapi_GetNamesFromIDs.nameid;

	return MAPI_E_SUCCESS;
}


/**
   \details Provides the property identifiers that correspond to one
   or more property names.
   
   \param obj the object we are retrieving the identifiers from
   \param count count of property names pointed to by the nameid
   parameter.
   \param nameid pointer to an array of property names
   \param ulFlags indicates how the property identifiers should be
   returned
   \param proptags pointer to a pointer to an array of property tags
   containing existing or newly assigned property
   identifiers. Property types in this array are set to PT_NULL.

   ulFlags can be set to:
   - 0 retrieves named properties from the server
   - MAPI_CREATE create the named properties if they don't exist on
     the server

   \note count and nameid parameter can automatically be built
   using the mapi_nameid API.

   \return MAPI_E_SUCCESS on success, otherwise -1.
   
   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetIDsFromNames, QueryNamesFromIDs, mapi_nameid_new
*/
_PUBLIC_ enum MAPISTATUS GetIDsFromNames(mapi_object_t *obj,
					 uint16_t count,
					 struct MAPINAMEID *nameid,
					 uint32_t ulFlags,
					 struct SPropTagArray **proptags)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetIDsFromNames_req	request;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	mapi_ctx_t			*mapi_ctx;
	uint32_t			i;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!count, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!nameid, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!proptags, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!proptags[0], MAPI_E_INVALID_PARAMETER, NULL);

	/* Initialization */
	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("GetIDsFromNames");
	size = 0;
	
	/* Fill the GetIDsFromNames operation */
	request.ulFlags = ulFlags;
	request.count = count;
	size += sizeof (uint8_t) + sizeof (uint16_t);

	request.nameid = nameid;
	for (i = 0; i < count; i++) {
		size += sizeof (uint8_t) + sizeof (request.nameid[i].lpguid);
		switch (request.nameid[i].ulKind) {
		case MNID_ID:
			size += sizeof (request.nameid[i].kind.lid);
			break;
		case MNID_STRING:
			size += strlen(request.nameid[i].kind.lpwstr.lpwstrName) * 2 + 2;
			size += sizeof (uint8_t);
			break;
		default:
			break;
		}
	}

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetIDsFromNames;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetIDsFromNames = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, 
				    mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* Fill the SPropTagArray */
	proptags[0]->cValues = mapi_response->mapi_repl->u.mapi_GetIDsFromNames.count;
	proptags[0]->aulPropTag = talloc_array((TALLOC_CTX *)proptags[0], uint32_t, proptags[0]->cValues);
	for (i = 0; i < proptags[0]->cValues; i++) {
		proptags[0]->aulPropTag[i] = (mapi_response->mapi_repl->u.mapi_GetIDsFromNames.propID[i] << 16) | PT_UNSPECIFIED;
	}
	
	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Provides the property names that correspond to one or more
   property identifiers.

   variant of GetNamesFromIDs(). This function works almost like
   GetNamesFromIDs() except that it can take ulPropTag = 0 and return
   the whole set of named properties available.

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \sa GetNamesFromIDs
*/
_PUBLIC_ enum MAPISTATUS QueryNamesFromIDs(mapi_object_t *obj,
					   uint16_t ulPropTag,
					   uint16_t *count,
					   uint16_t **propID,
					   struct MAPINAMEID **nameid)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct QueryNamesFromIDs_req	request;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;
	mapi_ctx_t			*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	/* Initialization */
	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("QueryNamesFromIDs");
	size = 0;

	/* Fill the QueryNamesFromIDs operation */
	request.propID = ulPropTag;
	size += sizeof (uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_QueryNamesFromIDs;
	mapi_req->logon_id = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_QueryNamesFromIDs = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx,
				    mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* Fill [out] parameters */
	*count = mapi_response->mapi_repl->u.mapi_QueryNamesFromIDs.count;
	*propID = mapi_response->mapi_repl->u.mapi_QueryNamesFromIDs.propID;
	*nameid = mapi_response->mapi_repl->u.mapi_QueryNamesFromIDs.nameid;

	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}
