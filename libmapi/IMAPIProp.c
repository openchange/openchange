/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2011.
   Copyright (C) Brad Hards 2008.

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
#include "libmapi/mapi_nameid.h"
#include "libmapi/libmapi_private.h"


/**
   \file IMAPIProp.c

   \brief Properties and named properties operations.
 */


/**
   \details Returns values of one or more properties for an object

   The function takes a pointer on the object obj, a MAPITAGS array
   specified in mapitags, and the count of properties.  The function
   returns associated values within the SPropValue values pointer.

   The array of MAPI property tags can be filled with both known and
   named properties.

   \param obj the object to get properties on
   \param flags Flags for behaviour; can be bit-OR of MAPI_UNICODE and
      MAPI_PROPS_SKIP_NAMEDID_CHECK constants
   \param SPropTagArray an array of MAPI property tags
   \param lpProps the result of the query
   \param PropCount the count of property tags

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj or SPropTagArray are null, or the
   session context could not be obtained
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa SetProps, GetPropList, GetPropsAll, DeleteProps, GetLastError
*/
_PUBLIC_ enum MAPISTATUS GetProps(mapi_object_t *obj,
				  uint32_t flags,
				  struct SPropTagArray *SPropTagArray,
				  struct SPropValue **lpProps,
				  uint32_t *PropCount)
{
	struct mapi_context	*mapi_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct GetProps_req	request;
	struct mapi_session	*session;
	struct mapi_nameid	*nameid;
	struct SPropTagArray	properties;
	struct SPropTagArray	*SPropTagArray2 = NULL;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	enum MAPISTATUS		mapistatus;
	uint32_t		size;
	TALLOC_CTX		*mem_ctx;
	bool			named = false;
	uint8_t			logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = session->mapi_ctx;
	OPENCHANGE_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "GetProps");

	/* Named property mapping */
	nameid = mapi_nameid_new(mem_ctx);
	if (!(flags & MAPI_PROPS_SKIP_NAMEDID_CHECK)) {
		retval = mapi_nameid_lookup_SPropTagArray(nameid, SPropTagArray);
		if (retval == MAPI_E_SUCCESS) {
			named = true;
			SPropTagArray2 = talloc_zero(mem_ctx, struct SPropTagArray);
			retval = GetIDsFromNames(obj, nameid->count, nameid->nameid, 0, &SPropTagArray2);
			OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
			mapi_nameid_map_SPropTagArray(nameid, SPropTagArray, SPropTagArray2);
			MAPIFreeBuffer(SPropTagArray2);
		}
	}
	errno = 0;

	/* Reset */
	*PropCount = 0;
	*lpProps = 0;
	size = 0;

	/* Fill the GetProps operation */
	request.PropertySizeLimit = 0x0;
	size += sizeof (uint16_t);
	request.WantUnicode = (flags & MAPI_UNICODE) != 0 ? true : 0x0;
	size += sizeof (uint16_t);
	request.prop_count = (uint16_t) SPropTagArray->cValues;
	size += sizeof (uint16_t);
	properties.cValues = SPropTagArray->cValues;
	properties.aulPropTag = talloc_memdup(mem_ctx, SPropTagArray->aulPropTag, SPropTagArray->cValues * sizeof(enum MAPITAGS));
	request.properties = properties.aulPropTag;
	size += request.prop_count * sizeof(uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetProps;
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF((retval && retval != MAPI_W_ERRORS_RETURNED), retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Read the SPropValue array from data blob.
	   fixme: replace the memory context by the object one.
	*/
	if (named == true) {
		mapi_nameid_unmap_SPropTagArray(nameid, SPropTagArray);
	}
	talloc_free(nameid);

	mapistatus = emsmdb_get_SPropValue((TALLOC_CTX *)session,
					   &mapi_response->mapi_repl->u.mapi_GetProps.prop_data,
					   &properties, lpProps, PropCount,
					   mapi_response->mapi_repl->u.mapi_GetProps.layout);
	OPENCHANGE_RETVAL_IF(!mapistatus && (retval == MAPI_W_ERRORS_RETURNED), retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Set one or more properties on a given object

   This function sets one or more properties on a specified object.

   \param obj the object to set properties on
   \param flags Flags for behaviour; can be MAPI_PROPS_SKIP_NAMEDID_CHECK
   \param lpProps the list of properties to set
   \param PropCount the number of properties

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetProps, GetPropList, GetPropsAll, DeleteProps, GetLastError
*/
_PUBLIC_ enum MAPISTATUS SetProps(mapi_object_t *obj,
				  uint32_t flags,
				  struct SPropValue *lpProps,
				  unsigned long PropCount)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SetProps_req	request;
	struct mapi_session	*session;
	struct mapi_nameid	*nameid;
	struct SPropTagArray	*SPropTagArray = NULL;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	unsigned long		i;
	struct mapi_SPropValue	*mapi_props;
	bool			named = false;
	uint8_t			logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "SetProps");
	size = 0;

	/* Named property mapping */
	nameid = mapi_nameid_new(mem_ctx);
	if (!(flags & MAPI_PROPS_SKIP_NAMEDID_CHECK)) {
		retval = mapi_nameid_lookup_SPropValue(nameid, lpProps, PropCount);
		if (retval == MAPI_E_SUCCESS) {
			named = true;
			SPropTagArray = talloc_zero(mem_ctx, struct SPropTagArray);
			retval = GetIDsFromNames(obj, nameid->count, nameid->nameid, 0, &SPropTagArray);
			OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
			mapi_nameid_map_SPropValue(nameid, lpProps, PropCount, SPropTagArray);
			MAPIFreeBuffer(SPropTagArray);
		}
	}
	errno = 0;

	/* build the array */
	request.values.lpProps = talloc_array(mem_ctx, struct mapi_SPropValue, PropCount);
	mapi_props = request.values.lpProps;
	for (i = 0; i < PropCount; i++) {
		size += cast_mapi_SPropValue((TALLOC_CTX *)request.values.lpProps, &mapi_props[i], &lpProps[i]);
		size += sizeof(uint32_t);
	}

	request.values.cValues = PropCount;
	size += sizeof(uint16_t);

	/* add the size of the subcontext that will be added on ndr layer */
	size += sizeof(uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetProps;
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	if (named == true) {
		mapi_nameid_unmap_SPropValue(nameid, lpProps, PropCount);
	}
	talloc_free(nameid);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Makes permanent any changes made to an attachment since the
   last save operation.

   \param obj_parent the parent of the object to save changes for
   \param obj_child the object to save changes for
   \param flags the access flags to set on the saved object

   Possible flags:
   - KeepOpenReadOnly
   - KeepOpenReadWrite
   - ForceSave

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa SetProps, ModifyRecipients, GetLastError
 */
_PUBLIC_ enum MAPISTATUS SaveChangesAttachment(mapi_object_t *obj_parent,
					       mapi_object_t *obj_child,
					       enum SaveFlags flags)
{
	struct mapi_request			*mapi_request;
	struct mapi_response			*mapi_response;
	struct EcDoRpc_MAPI_REQ			*mapi_req;
	struct SaveChangesAttachment_req	request;
	struct mapi_session			*session[2];
	NTSTATUS				status;
	enum MAPISTATUS				retval;
	uint32_t				size = 0;
	TALLOC_CTX				*mem_ctx;
	uint8_t					logon_id;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!obj_parent, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_child, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF((flags != 0x9) && (flags != 0xA) && (flags != 0xC),
		       MAPI_E_INVALID_PARAMETER, NULL);

	session[0] = mapi_object_get_session(obj_parent);
	session[1] = mapi_object_get_session(obj_child);
	OPENCHANGE_RETVAL_IF(!session[0], MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!session[1], MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(session[0] != session[1], MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_parent, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session[0], 0, "SaveChangesAttachment");
	size = 0;

	/* Fill the SaveChangesAttachment operation */
	request.handle_idx = 0x0;
	request.SaveFlags = flags;
	size += sizeof(uint8_t) + sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SaveChangesAttachment;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SaveChangesAttachment = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_child);
	mapi_request->handles[1] = mapi_object_get_handle(obj_parent);

	status = emsmdb_transaction_wrapper(session[0], mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session[0], mapi_response);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve all the properties associated with a given object

   \param obj the object to retrieve properties for
   \param proptags the resulting list of properties associated with
   the object

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

     The developer MUST provide an allocated SPropTagArray structure
     to the function.

   \sa GetProps, GetPropsAll, GetLastError
 */
_PUBLIC_ enum MAPISTATUS GetPropList(mapi_object_t *obj,
				     struct SPropTagArray *proptags)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	uint8_t			logon_id;

	/* sanity checks */
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "GetPropList");

	/* Reset */
	proptags->cValues = 0;
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetPropList;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 1;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Get the repsonse */
	proptags->cValues = mapi_response->mapi_repl->u.mapi_GetPropList.count;
	if (proptags->cValues) {
		size = proptags->cValues * sizeof(enum MAPITAGS);
		proptags->aulPropTag = talloc_array((TALLOC_CTX *) proptags, enum MAPITAGS, proptags->cValues);
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
   \param flags Flags for behaviour; can be a MAPI_UNICODE constant
   \param properties the properties / values for the object

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetProps, GetPropList, GetLastError
*/
_PUBLIC_ enum MAPISTATUS GetPropsAll(mapi_object_t *obj,
				     uint32_t flags,
				     struct mapi_SPropValue_array *properties)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct GetPropsAll_req	request;
	struct GetPropsAll_repl	*reply;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	uint8_t			logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "GetPropsAll");
	size = 0;

	/* Fill the GetPropsAll operation */
	request.PropertySizeLimit = 0;
	size += sizeof (uint16_t);
	request.WantUnicode = (flags & MAPI_UNICODE) != 0 ? true : 0x0;
	size += sizeof (uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetPropsAll;
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	reply = &mapi_response->mapi_repl->u.mapi_GetPropsAll;
	properties->cValues = reply->properties.cValues;
	properties->lpProps = talloc_steal((TALLOC_CTX *)session, reply->properties.lpProps);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}


/**
   \details Delete one or more properties from an object

   \param obj the object to remove properties from
   \param proptags the properties to remove from the given object

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa SetProps, GetLastError
*/
_PUBLIC_ enum MAPISTATUS DeleteProps(mapi_object_t *obj,
				     struct SPropTagArray *proptags)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct DeleteProps_req	request;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	uint8_t			logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!proptags, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "DeleteProps");
	size = 0;

	/* Fill the DeleteProps operation */
	request.count = proptags->cValues;
	size += sizeof(uint16_t);
	request.tags = proptags->aulPropTag;
	size += proptags->cValues * sizeof(enum MAPITAGS);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_DeleteProps;
	mapi_req->logon_id = logon_id;
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
   \details Set one or more properties on a given object without
    invoking replication.

   This function sets one or more properties on a specified object. It
   is the same as SetProps, except if the object is a folder, where
   this function does not result in folder properties being replicated.

   \param obj the object to set properties on
   \param flags Flags for behaviour; can be MAPI_PROPS_SKIP_NAMEDID_CHECK
   \param lpProps the list of properties to set
   \param PropCount the number of properties

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj is not valid
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa SetProps, DeletePropertiesNoReplicate
*/
_PUBLIC_ enum MAPISTATUS SetPropertiesNoReplicate(mapi_object_t *obj,
						  uint32_t flags,
						  struct SPropValue *lpProps,
						  unsigned long PropCount)
{
	TALLOC_CTX				*mem_ctx;
	struct mapi_request			*mapi_request;
	struct mapi_response			*mapi_response;
	struct EcDoRpc_MAPI_REQ			*mapi_req;
	struct SetPropertiesNoReplicate_req	request;
	struct mapi_session			*session;
	struct mapi_nameid			*nameid;
	struct SPropTagArray			*SPropTagArray = NULL;
	NTSTATUS				status;
	enum MAPISTATUS				retval;
	uint32_t				size = 0;
	unsigned long				i;
	struct mapi_SPropValue			*mapi_props;
	bool					named = false;
	uint8_t					logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "SetPropertiesNoReplicate");
	size = 0;

	/* Named property mapping */
	nameid = mapi_nameid_new(mem_ctx);
	if (!(flags & MAPI_PROPS_SKIP_NAMEDID_CHECK)) {
		retval = mapi_nameid_lookup_SPropValue(nameid, lpProps, PropCount);
		if (retval == MAPI_E_SUCCESS) {
			named = true;
			SPropTagArray = talloc_zero(mem_ctx, struct SPropTagArray);
			retval = GetIDsFromNames(obj, nameid->count, nameid->nameid, 0, &SPropTagArray);
			OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
			mapi_nameid_map_SPropValue(nameid, lpProps, PropCount, SPropTagArray);
			MAPIFreeBuffer(SPropTagArray);
		}
	}
	errno = 0;

	/* build the array */
	request.values.lpProps = talloc_array(mem_ctx, struct mapi_SPropValue, PropCount);
	mapi_props = request.values.lpProps;
	for (i = 0; i < PropCount; i++) {
		size += cast_mapi_SPropValue((TALLOC_CTX *)request.values.lpProps, &mapi_props[i], &lpProps[i]);
		size += sizeof(uint32_t);
	}

	request.values.cValues = PropCount;
	size += sizeof(uint16_t);

	/* add the size of the subcontext that will be added on ndr layer */
	size += sizeof(uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetPropertiesNoReplicate;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SetPropertiesNoReplicate = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	if (named == true) {
		mapi_nameid_unmap_SPropValue(nameid, lpProps, PropCount);
	}
	talloc_free(nameid);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Deletes property values from an object without invoking
   replication.

   \param obj the object to remove properties from
   \param proptags the properties to remove from the given object

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

     \sa DeleteProps
 */
_PUBLIC_ enum MAPISTATUS DeletePropertiesNoReplicate(mapi_object_t *obj,
						     struct SPropTagArray *proptags)
{
	TALLOC_CTX				*mem_ctx;
	struct mapi_request			*mapi_request;
	struct mapi_response			*mapi_response;
	struct EcDoRpc_MAPI_REQ			*mapi_req;
	struct DeletePropertiesNoReplicate_req	request;
	struct mapi_session			*session;
	NTSTATUS				status;
	enum MAPISTATUS				retval;
	uint32_t				size;
	uint8_t					logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!proptags, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "DeletePropertiesNoReplicate");
	size = 0;

	/* Fill the DeletePropertiesNoReplicate operation */
	request.PropertyTags.cValues = proptags->cValues;
	size += sizeof (uint16_t);
	request.PropertyTags.aulPropTag = proptags->aulPropTag;
	size += proptags->cValues * sizeof (enum MAPITAGS);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_DeletePropertiesNoReplicate;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_DeletePropertiesNoReplicate = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

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
   \details Provides the property names that correspond to one
   or more property identifiers.

   \param obj the object we are retrieving the names from
   \param ulPropTag the mapped property tag
   \param count count of property names pointed to by the nameid
   parameter returned by the server
   \param nameid pointer to a pointer to property names returned by
   the server

   ulPropTag must be a property with type set to PT_NULL

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetIDsFromNames, QueryNamesFromIDs
*/
_PUBLIC_ enum MAPISTATUS GetNamesFromIDs(mapi_object_t *obj,
					 enum MAPITAGS ulPropTag,
					 uint16_t *count,
					 struct MAPINAMEID **nameid)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetNamesFromIDs_req	request;
	struct GetNamesFromIDs_repl	*reply;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size= 0;
	TALLOC_CTX			*mem_ctx;
	uint8_t				logon_id;

	/* sanity checks */
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	/* Initialization */
	mem_ctx = talloc_named(session, 0, "GetNamesFromIDs");
	size = 0;

	/* Fill the GetNamesFromIDs operation */
	request.PropertyIdCount = 0x1;
	size += sizeof (uint16_t);
	request.PropertyIds = talloc_array(mem_ctx, uint16_t, request.PropertyIdCount);
	request.PropertyIds[0] = ((ulPropTag & 0xFFFF0000) >> 16);
	size += request.PropertyIdCount * sizeof (uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetNamesFromIDs;
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Fill in count */
	reply = &mapi_response->mapi_repl->u.mapi_GetNamesFromIDs;
	*count = reply->count;

	/* Fill MAPINAMEID struct */
	*nameid = talloc_steal((TALLOC_CTX *)session, reply->nameid);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

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

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
   - MAPI_W_ERRORS_RETURNED: Some property names where not valid and should be
     ignored. It's more a warning than an error.

   \sa GetNamesFromIds, QueryNamesFromIDs, mapi_nameid_new
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
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint32_t			i;
	uint8_t				logon_id;

	/* sanity checks */
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!count, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!nameid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!proptags, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!proptags[0], MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	/* Initialization */
	mem_ctx = talloc_named(session, 0, "GetIDsFromNames");
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
		  size +=  get_utf8_utf16_conv_length(request.nameid[i].kind.lpwstr.Name);
			size += sizeof (uint8_t);
			break;
		default:
			break;
		}
	}

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetIDsFromNames;
	mapi_req->logon_id = logon_id;
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

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF((retval != MAPI_W_ERRORS_RETURNED) && (retval != MAPI_E_SUCCESS), retval, mem_ctx);
	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Fill the SPropTagArray */
	proptags[0]->cValues = mapi_response->mapi_repl->u.mapi_GetIDsFromNames.count;
	proptags[0]->aulPropTag = (enum MAPITAGS *) talloc_array((TALLOC_CTX *)proptags[0], uint32_t, proptags[0]->cValues);
	for (i = 0; i < proptags[0]->cValues; i++) {
		if (mapi_response->mapi_repl->u.mapi_GetIDsFromNames.propID[i]) {
			proptags[0]->aulPropTag[i] = (enum MAPITAGS)(((int)mapi_response->mapi_repl->u.mapi_GetIDsFromNames.propID[i] << 16) | PT_UNSPECIFIED);
		} else {
			proptags[0]->aulPropTag[i] = PT_ERROR;
		}
	}

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return retval;
}


/**
   \details Provides the property names that correspond to one or more
   property identifiers.

   \param obj the object to obtain the properties for
   \param queryFlags A set of flags that can restrict the type of properties
   \param guid a pointer to the GUID for the property set to fetch (null for all
   property sets.
   \param count count of property names pointed to by the nameid and propID
   parameters returned by the server
   \param propID pointer to an array of property IDs returned by the server
   \param nameid pointer to an array of property names returned by
   the server

   \note queryFlags can be NoStrings (0x1) or NoIds (0x2), neither or both.
   NoStrings will produce only ID properties, NoIds will produce only named
   properties, and both will result in no output.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \sa GetNamesFromIDs
*/
_PUBLIC_ enum MAPISTATUS QueryNamedProperties(mapi_object_t *obj,
					      uint8_t queryFlags,
					      struct GUID *guid,
					      uint16_t *count,
					      uint16_t **propID,
					      struct MAPINAMEID **nameid)
{
	struct mapi_request			*mapi_request;
	struct mapi_response			*mapi_response;
	struct EcDoRpc_MAPI_REQ			*mapi_req;
	struct QueryNamedProperties_req		request;
	struct QueryNamedProperties_repl	*reply;
	struct mapi_session			*session;
	NTSTATUS				status;
	enum MAPISTATUS				retval;
	uint32_t				size;
	TALLOC_CTX				*mem_ctx;
	uint8_t					logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	/* Initialization */
	mem_ctx = talloc_named(session, 0, "QueryNamesFromIDs");
	size = 0;

	/* Fill the QueryNamedProperties operation */
	request.QueryFlags = queryFlags;
	size += sizeof (uint8_t);

	if (guid) {
		request.HasGuid = 0x1; /* true */
		size += sizeof (uint8_t);
		request.PropertyGuid.guid = *guid;
		size += sizeof (struct GUID);
	} else {
		request.HasGuid = 0x0; /* false */
		size += sizeof (uint8_t);
	}

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_QueryNamedProperties;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_QueryNamedProperties = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj);

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session, mapi_response);

	/* Fill [out] parameters */
	reply = &mapi_response->mapi_repl->u.mapi_QueryNamedProperties;

	*count = reply->IdCount;
	*propID = talloc_steal((TALLOC_CTX *)session, reply->PropertyIds);
	*nameid = talloc_steal((TALLOC_CTX *)session, reply->PropertyNames);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Copy properties from one object to another

   This function copies (or moves) specified properties from
   one object to another.

   \param obj_src the object to copy properties from
   \param obj_dst the object to set properties on
   \param copyFlags flags to determine whether to copy or
   move, and whether to overwrite existing properties.
   \param tags the list of properties to copy
   \param problemCount (return value) number of entries in the problems array
   \param problems (return value) array of problemCount entries.

   The caller is responsible for freeing the \b problems array
   using MAPIFreeBuffer(). If the \b problemCount pointer is NULL,
   then the problems array will not be returned.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetProps, SetProps, DeleteProps, CopyTo, GetLastError
*/
_PUBLIC_ enum MAPISTATUS CopyProps(mapi_object_t *obj_src,
				   mapi_object_t *obj_dst,
				   struct SPropTagArray *tags,
				   uint8_t copyFlags,
				   uint16_t *problemCount,
				   struct PropertyProblem **problems)

{
	TALLOC_CTX			*mem_ctx;
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct CopyProperties_req	request;
	struct mapi_session		*session[2];
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	int				i;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_src, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_dst, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!tags, MAPI_E_INVALID_PARAMETER, NULL);

	session[0] = mapi_object_get_session(obj_src);
	session[1] = mapi_object_get_session(obj_dst);
	OPENCHANGE_RETVAL_IF(!session[0], MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!session[1], MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(session[0] != session[1], MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_src, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session[0], 0, "CopyProps");
	size = 0;

	/* Fill the CopyProperties operation */
	request.handle_idx = 0x1;
	size += sizeof(uint8_t);
	request.WantAsynchronous = 0x0;
	size += sizeof(uint8_t);
	request.CopyFlags = copyFlags;
	size += sizeof(uint8_t);
	request.PropertyTags.cValues = tags->cValues;
	size += sizeof(uint16_t);
	request.PropertyTags.aulPropTag = tags->aulPropTag;
	size += tags->cValues * sizeof(enum MAPITAGS);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_CopyProperties;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_CopyProperties = request;
	size += 5; // sizeof( EcDoRpc_MAPI_REQ )

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) *2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_src);
	mapi_request->handles[1] = mapi_object_get_handle(obj_dst);

	status = emsmdb_transaction_wrapper(session[0], mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session[0], mapi_response);

	if (problemCount) {
		*problemCount = mapi_response->mapi_repl->u.mapi_CopyProperties.PropertyProblemCount;
		*problems = talloc_array((TALLOC_CTX *)session[0], struct PropertyProblem, *problemCount);
		for(i = 0; i < *problemCount; i++) {
			(*(problems[i])).index = mapi_response->mapi_repl->u.mapi_CopyProperties.PropertyProblem[i].index;
			(*(problems[i])).property_tag = mapi_response->mapi_repl->u.mapi_CopyProperties.PropertyProblem[i].property_tag;
			(*(problems[i])).error_code = mapi_response->mapi_repl->u.mapi_CopyProperties.PropertyProblem[i].error_code;
		}
	}

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Copy multiple properties from one object to another

   This function copies (or moves) properties from one object to
   another. Unlike CopyProperties, this function copies all properties
   except those identified.

   \param obj_src the object to copy properties from
   \param obj_dst the object to set properties on
   \param excludeTags the list of properties to \em not copy
   \param copyFlags flags to determine whether to copy or
   move, and whether to overwrite existing properties.
   \param problemCount (return value) number of entries in the problems array
   \param problems (return value) array of problemCount entries.

   The caller is responsible for freeing the \b problems array
   using MAPIFreeBuffer(). If the \b problemCount pointer is NULL,
   then the problems array will not be returned.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetProps, SetProps, DeleteProps, CopyProps
*/
_PUBLIC_ enum MAPISTATUS CopyTo(mapi_object_t *obj_src,
				mapi_object_t *obj_dst,
				struct SPropTagArray *excludeTags,
				uint8_t copyFlags,
				uint16_t *problemCount,
				struct PropertyProblem **problems)

{
	TALLOC_CTX		*mem_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct CopyTo_req	request;
	struct mapi_session	*session[2];
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size;
	int			i;
	uint8_t			logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_src, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_dst, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!excludeTags, MAPI_E_INVALID_PARAMETER, NULL);

	session[0] = mapi_object_get_session(obj_src);
	session[1] = mapi_object_get_session(obj_dst);
	OPENCHANGE_RETVAL_IF(!session[0], MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!session[1], MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(session[0] != session[1], MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj_src, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session[0], 0, "CopyProps");
	size = 0;

	/* Fill the CopyProperties operation */
	request.handle_idx = 0x1;
	size += sizeof(uint8_t);
	request.WantAsynchronous = 0x0;
	size += sizeof(uint8_t);
	request.WantSubObjects = 0x1;
	size += sizeof(uint8_t);
	request.CopyFlags = copyFlags;
	size += sizeof(uint8_t);
	request.ExcludedTags.cValues = (uint16_t)excludeTags->cValues;
	size += sizeof(uint16_t);
	request.ExcludedTags.aulPropTag = excludeTags->aulPropTag;
	size += excludeTags->cValues * sizeof(enum MAPITAGS);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_CopyTo;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_CopyTo = request;
	size += 5; // sizeof( EcDoRpc_MAPI_REQ )

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) *2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_src);
	mapi_request->handles[1] = mapi_object_get_handle(obj_dst);

	status = emsmdb_transaction_wrapper(session[0], mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_CHECK_NOTIFICATION(session[0], mapi_response);

	if (problemCount) {
		*problemCount = mapi_response->mapi_repl->u.mapi_CopyTo.PropertyProblemCount;
		*problems = talloc_array((TALLOC_CTX *)session[0], struct PropertyProblem, *problemCount);
		for(i=0; i < *problemCount; ++i) {
			(*(problems[i])).index = mapi_response->mapi_repl->u.mapi_CopyTo.PropertyProblem[i].index;
			(*(problems[i])).property_tag = mapi_response->mapi_repl->u.mapi_CopyTo.PropertyProblem[i].property_tag;
			(*(problems[i])).error_code = mapi_response->mapi_repl->u.mapi_CopyTo.PropertyProblem[i].error_code;
		}
	}

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
