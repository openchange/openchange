/*
   MAPI Proxy - Exchange NSPI Server

   OpenChange Project

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
   \file dcesrv_exchange_nsp.c

   \brief OpenChange NSPI Server implementation
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/fault_util.h"
#include "dcesrv_exchange_nsp.h"

static struct exchange_nsp_session	*nsp_session = NULL;
static TDB_CONTEXT			*emsabp_tdb_ctx = NULL;

static struct exchange_nsp_session *dcesrv_find_nsp_session(struct GUID *uuid)
{
	struct exchange_nsp_session	*session, *found_session = NULL;

	for (session = nsp_session; !found_session && session; session = session->next) {
		if (GUID_equal(uuid, &session->uuid)) {
			found_session = session;
		}
	}

	return found_session;
}

static struct emsabp_context *dcesrv_find_emsabp_context(struct GUID *uuid)
{
	struct exchange_nsp_session	*session;
	struct emsabp_context		*emsabp_ctx = NULL;

	session = dcesrv_find_nsp_session(uuid);
	if (session) {
		emsabp_ctx = (struct emsabp_context *)session->session->private_data;;
	}

	return emsabp_ctx;
}


/**
 * Make PropertyRow record with empty value and PT_ERROR flag for PropTag.
 * We should return such rows when client requests an MId we can't find.
 */
static void dcesrv_make_ptyp_error_property_row(TALLOC_CTX* mem_ctx,
						struct SPropTagArray* pPropTags,
						struct PropertyRow_r* aRow)
{
	int i;
	uint32_t ulPropTag;
	aRow->Reserved = 0x0;
	aRow->cValues = pPropTags->cValues;
	aRow->lpProps = talloc_array(mem_ctx, struct PropertyValue_r, aRow->cValues);
	for (i = 0; i < aRow->cValues; i++) {
		ulPropTag = pPropTags->aulPropTag[i];
		ulPropTag = (ulPropTag & 0xFFFF0000) | PT_ERROR;

		aRow->lpProps[i].ulPropTag = (enum MAPITAGS) ulPropTag;
		aRow->lpProps[i].dwAlignPad = 0x0;
		set_PropertyValue(&(aRow->lpProps[i]), NULL);
	}
}


/**
   \details exchange_nsp NspiBind (0x0) function, Initiates a NSPI
   session with the client.

   This function checks if the user is an Exchange user and input
   parameters like codepage are valid. If it passes the tests, the
   function initializes an emsabp context and returns to the client a
   valid policy_handle and expected reply parameters.

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiBind call structure

   \return MAPI_E_SUCCESS on success, otherwise a MAPI error
 */
static void dcesrv_NspiBind(struct dcesrv_call_state *dce_call,
				       TALLOC_CTX *mem_ctx,
				       struct NspiBind *r)
{
	struct GUID			*guid = (struct GUID *) NULL;
	struct emsabp_context		*emsabp_ctx;
	struct dcesrv_handle		*handle;
	struct policy_handle		wire_handle;
	struct exchange_nsp_session	*session;

	DEBUG(5, ("exchange_nsp: NspiBind (0x0)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call) && (r->in.dwFlags & fAnonymousLogin)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));

		wire_handle.handle_type = EXCHANGE_HANDLE_NSP;
		wire_handle.uuid = GUID_zero();
		*r->out.handle = wire_handle;

		r->out.mapiuid = r->in.mapiuid;
		DCESRV_NSP_RETURN(r, MAPI_E_FAILONEPROVIDER, NULL);
	}

	/* Step 1. Initialize the emsabp context */
	emsabp_ctx = emsabp_init(dce_call->conn->dce_ctx->lp_ctx, emsabp_tdb_ctx);
	if (!emsabp_ctx) {
		OC_ABORT(false, ("[exchange_nsp] Unable to initialize emsabp context"));

		wire_handle.handle_type = EXCHANGE_HANDLE_NSP;
		wire_handle.uuid = GUID_zero();
		*r->out.handle = wire_handle;

		r->out.mapiuid = r->in.mapiuid;
		DCESRV_NSP_RETURN(r, MAPI_E_FAILONEPROVIDER, NULL);
	}

	if (lpcfg_parm_bool(dce_call->conn->dce_ctx->lp_ctx, NULL, 
			    "exchange_nsp", "debug", false)) {
		emsabp_enable_debug(emsabp_ctx);
	}

	/* Step 2. Check if incoming user belongs to the Exchange organization */
	if ((emsabp_verify_user(dce_call, emsabp_ctx) == false) && (r->in.dwFlags & fAnonymousLogin)) {
		talloc_free(emsabp_ctx);

		wire_handle.handle_type = EXCHANGE_HANDLE_NSP;
		wire_handle.uuid = GUID_zero();
		*r->out.handle = wire_handle;

		r->out.mapiuid = r->in.mapiuid;
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, emsabp_tdb_ctx);
	}

	/* Step 3. Check if valid cpID has been supplied */
	if (emsabp_verify_codepage(emsabp_ctx, r->in.pStat->CodePage) == false) {
		talloc_free(emsabp_ctx);

		wire_handle.handle_type = EXCHANGE_HANDLE_NSP;
		wire_handle.uuid = GUID_zero();
		*r->out.handle = wire_handle;

		r->out.mapiuid = r->in.mapiuid;
		DCESRV_NSP_RETURN(r, MAPI_E_UNKNOWN_CPID, emsabp_tdb_ctx);
	}

	/* Step 4. Retrieve OpenChange server GUID */
	guid = (struct GUID *) samdb_ntds_objectGUID(emsabp_ctx->samdb_ctx);
	if (!guid) {
		DCESRV_NSP_RETURN(r, MAPI_E_FAILONEPROVIDER, emsabp_ctx);
	}

	/* Step 5. Fill NspiBind reply */
	handle = dcesrv_handle_new(dce_call->context, EXCHANGE_HANDLE_NSP);
	if (!handle) {
		DCESRV_NSP_RETURN(r, MAPI_E_NOT_ENOUGH_RESOURCES, emsabp_ctx);
	}

	handle->data = (void *) emsabp_ctx;
	*r->out.handle = handle->wire_handle;
	r->out.mapiuid = guid;

	/* Search for an existing session and increment ref_count, otherwise create it */
	session = dcesrv_find_nsp_session(&handle->wire_handle.uuid);
	if (session) {
		mpm_session_increment_ref_count(session->session);
		DEBUG(5, ("  [unexpected]: existing nsp_session: %p; session: %p (ref++)\n", session, session->session));
	}
	else {
		DEBUG(5, ("%s: Creating new session\n", __func__));

		/* Step 6. Associate this emsabp context to the session */
		session = talloc((TALLOC_CTX *)nsp_session, struct exchange_nsp_session);
		if (!session) {
			DCESRV_NSP_RETURN(r, MAPI_E_NOT_ENOUGH_RESOURCES, emsabp_ctx);
		}

		session->session = mpm_session_init((TALLOC_CTX *)nsp_session, dce_call);
		if (!session->session) {
			DCESRV_NSP_RETURN(r, MAPI_E_NOT_ENOUGH_RESOURCES, emsabp_ctx);
		}

		session->uuid = handle->wire_handle.uuid;

		mpm_session_set_private_data(session->session, (void *) emsabp_ctx);
		mpm_session_set_destructor(session->session, emsabp_destructor);

		DLIST_ADD_END(nsp_session, session, struct exchange_nsp_session *);
	}

	DCESRV_NSP_RETURN(r, MAPI_E_SUCCESS, NULL);
}


/**
   \details exchange_nsp NspiUnbind (0x1) function, Terminates a NSPI
   session with the client

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiUnbind call structure
 */
static void dcesrv_NspiUnbind(struct dcesrv_call_state *dce_call,
					 TALLOC_CTX *mem_ctx,
					 struct NspiUnbind *r)
{
	struct dcesrv_handle		*h;
	struct exchange_nsp_session	*session;
	bool				ret;

	DEBUG(5, ("exchange_nsp: NspiUnbind (0x1)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	/* Step 1. Retrieve handle and free if emsabp context and session are available */
	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	if (h) {
		session = dcesrv_find_nsp_session(&r->in.handle->uuid);
		if (session) {
			ret = mpm_session_release(session->session);
			if (ret == true) {
				DLIST_REMOVE(nsp_session, session);
				DEBUG(5, ("[%s:%d]: Session found and released\n",
					  __FUNCTION__, __LINE__));
			} else {
				DEBUG(5, ("[%s:%d]: Session found and ref_count decreased\n",
					  __FUNCTION__, __LINE__));
			}
		}
		else {
			DEBUG(5, ("  nsp_session NOT found\n"));
		}
	}

	r->out.handle->uuid = GUID_zero();
	r->out.handle->handle_type = 0;
	r->out.result = (enum MAPISTATUS) 1;

	DCESRV_NSP_RETURN(r, 1, NULL);
}


/**
   \details exchange_nsp NspiUpdateStat (0x2) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiUpdateStat request data

   \return MAPI_E_SUCCESS on success
*/
static void dcesrv_NspiUpdateStat(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, struct NspiUpdateStat *r)
{
	enum MAPISTATUS			retval = MAPI_E_SUCCESS;
        enum MAPISTATUS                 ret;
	struct emsabp_context		*emsabp_ctx = NULL;
	uint32_t			row, row_max;
	TALLOC_CTX			*local_mem_ctx;
	struct PropertyTagArray_r	*mids;

	DEBUG(3, ("exchange_nsp: NspiUpdateStat (0x2)"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	if (!emsabp_ctx) {
		DCESRV_NSP_RETURN(r, MAPI_E_CALL_FAILED, NULL);
	}

	local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	/* Step 1. Sanity Checks (MS-NSPI Server Processing Rules) */
	if (r->in.pStat->ContainerID && (emsabp_tdb_lookup_MId(emsabp_ctx->tdb_ctx, r->in.pStat->ContainerID) == false)) {
		retval = MAPI_E_INVALID_BOOKMARK;
		goto end;
	}

	mids = talloc_zero(local_mem_ctx, struct PropertyTagArray_r);
        if (!mids) {
                DCESRV_NSP_RETURN(r, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
        }

        ret = emsabp_search(local_mem_ctx, emsabp_ctx, mids, NULL, r->in.pStat, 0);
	if (ret != MAPI_E_SUCCESS) {
		row_max = 0;
                if (ret == MAPI_E_CALL_FAILED) {
                        retval = ret;
                        goto end;
                }
	}
	else {
		row_max = mids->cValues;
	}

	if (r->in.pStat->CurrentRec == MID_CURRENT) {
		/* Fractional positioning (3.1.1.4.2) */
		row = r->in.pStat->NumPos * row_max / r->in.pStat->TotalRecs;
		if (row > row_max) {
			row = row_max;
		}
	}
	else {
		if (r->in.pStat->CurrentRec == MID_BEGINNING_OF_TABLE) {
			row = 0;
		}
		else if (r->in.pStat->CurrentRec == MID_END_OF_TABLE) {
			row = row_max;
		}
		else {
			retval = MAPI_E_NOT_FOUND;
			row = 0;
			while (row < row_max) {
				if ((uint32_t) mids->aulPropTag[row] == (uint32_t) r->in.pStat->CurrentRec) {
					retval = MAPI_E_SUCCESS;
					break;
				}
				else {
					row++;
				}
			}
			if (retval == MAPI_E_NOT_FOUND) {
				goto end;
			}
		}
	}

	if (-r->in.pStat->Delta > row) {
		row = 0;
		r->in.pStat->CurrentRec = mids->aulPropTag[row];
	}
	else if (r->in.pStat->Delta + row >= row_max) {
		row = row_max;
		r->in.pStat->CurrentRec = MID_END_OF_TABLE;
	}
	else {
		row += r->in.pStat->Delta;
		r->in.pStat->CurrentRec = mids->aulPropTag[row];
	}

	r->in.pStat->Delta = 0;
	r->in.pStat->NumPos = row;
	r->in.pStat->TotalRecs = row_max;

end:
	r->out.pStat = r->in.pStat;

	DCESRV_NSP_RETURN(r, retval, local_mem_ctx);
}

/**
   \details exchange_nsp NspiQueryRows (0x3) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiQueryRows request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_NspiQueryRows(struct dcesrv_call_state *dce_call,
				 TALLOC_CTX *mem_ctx,
				 struct NspiQueryRows *r)
{
	enum MAPISTATUS			retval = MAPI_E_SUCCESS;
	struct emsabp_context		*emsabp_ctx = NULL;
	struct SPropTagArray		*pPropTags;
	struct PropertyRowSet_r		*pRows;
	uint32_t			count = 0;
	uint32_t			i, j;

	DEBUG(3, ("exchange_nsp: NspiQueryRows (0x3)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	if (!emsabp_ctx) {
		DCESRV_NSP_RETURN(r, MAPI_E_CALL_FAILED, NULL);
	}

	/* Step 1. Sanity Checks (MS-NSPI Server Processing Rules) */
	if (r->in.pStat->ContainerID && r->in.lpETable == NULL && (emsabp_tdb_lookup_MId(emsabp_ctx->tdb_ctx, r->in.pStat->ContainerID) == false)) {
		retval = MAPI_E_INVALID_BOOKMARK;
		goto failure;
	}

	if (r->in.pPropTags == NULL) {
		pPropTags = set_SPropTagArray(mem_ctx, 0x7,
					      PR_EMS_AB_CONTAINERID,
					      PR_OBJECT_TYPE,
					      PR_DISPLAY_TYPE,
					      PR_DISPLAY_NAME,
					      PR_OFFICE_TELEPHONE_NUMBER,
					      PR_COMPANY_NAME,
					      PR_OFFICE_LOCATION);
	} else {
		pPropTags = r->in.pPropTags;
	}

	/* Allocate RowSet to be filled in */
	pRows = talloc_zero(mem_ctx, struct PropertyRowSet_r);

	/* Step 2. Fill ppRows  */
	if (r->in.lpETable == NULL) {
		/* Step 2.1 Fill ppRows for supplied Container ID */
		struct ldb_result	*ldb_res;

		retval = emsabp_ab_container_enum(mem_ctx, emsabp_ctx,
						  r->in.pStat->ContainerID, &ldb_res);
		if (retval != MAPI_E_SUCCESS)  {
			goto failure;
		}

		if (ldb_res->count < r->in.pStat->NumPos) {
			/* Bad position */
			retval = MAPI_E_INVALID_PARAMETER;
			goto failure;
		}

		count = ldb_res->count - r->in.pStat->NumPos;

		if (r->in.Count < count) {
			count = r->in.Count;
		}
		if (count) {
			pRows->cRows = count;
			pRows->aRow = talloc_array(mem_ctx, struct PropertyRow_r, count);
		}

		/* fetch required attributes for every entry found */
		for (i = 0; i < count; i++) {
			retval = emsabp_fetch_attrs_from_msg(mem_ctx, emsabp_ctx, pRows->aRow + i,
							     ldb_res->msgs[i+r->in.pStat->NumPos], 0, r->in.dwFlags, pPropTags);
			if (retval != MAPI_E_SUCCESS) {
				goto failure;
			}
		}
		r->in.pStat->NumPos = r->in.pStat->Delta + pRows->cRows;
		r->in.pStat->CurrentRec = MID_END_OF_TABLE;
		r->in.pStat->TotalRecs = pRows->cRows;
		r->in.pStat->Delta = 0;
	} else {
		/* Step 2.2 Fill ppRows for supplied table of MIds */
		j = 0;
		if (r->in.pStat->NumPos < r->in.dwETableCount) {
			pRows->cRows = r->in.dwETableCount - r->in.pStat->NumPos;
			pRows->aRow = talloc_array(mem_ctx, struct PropertyRow_r, pRows->cRows);
			for (i = r->in.pStat->NumPos; i < r->in.dwETableCount; i++) {
				retval = emsabp_fetch_attrs(mem_ctx, emsabp_ctx, &(pRows->aRow[j]), r->in.lpETable[i], r->in.dwFlags, pPropTags);
				if (retval != MAPI_E_SUCCESS) {
					dcesrv_make_ptyp_error_property_row(mem_ctx, pPropTags, &(pRows->aRow[j]));
				}
				j++;
			}
		}
		r->in.pStat->CurrentRec = MID_END_OF_TABLE;
		r->in.pStat->TotalRecs = j;
		r->in.pStat->Delta = 0;
	}

	/* Step 3. Fill output params */
	*r->out.ppRows = pRows;

	memcpy(r->out.pStat, r->in.pStat, sizeof (struct STAT));

	DCESRV_NSP_RETURN(r, MAPI_E_SUCCESS, NULL);

failure:
	r->out.pStat = r->in.pStat;
	*r->out.ppRows = NULL;
	DCESRV_NSP_RETURN(r, retval, NULL);
}


/**
   \details exchange_nsp NspiSeekEntries (0x4) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiSeekEntries request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_NspiSeekEntries(struct dcesrv_call_state *dce_call,
					      TALLOC_CTX *mem_ctx,
					      struct NspiSeekEntries *r)
{
	enum MAPISTATUS			retval = MAPI_E_SUCCESS, ret;
	struct emsabp_context		*emsabp_ctx = NULL;
	uint32_t			row;
	struct PropertyTagArray_r	*mids, *all_mids;
	struct Restriction_r		*seek_restriction;

	DEBUG(3, ("exchange_nsp: NspiSeekEntries (0x4)\n"));

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	if (!emsabp_ctx) {
		DCESRV_NSP_RETURN(r, MAPI_E_CALL_FAILED, NULL);
	}

	/* Step 1. Sanity Checks (MS-NSPI Server Processing Rules) */
	if (r->in.pStat->ContainerID && (emsabp_tdb_lookup_MId(emsabp_ctx->tdb_ctx, r->in.pStat->ContainerID) == false)) {
		retval = MAPI_E_INVALID_BOOKMARK;
		goto end;
	}

	if (!r->in.pTarget) {
		retval = MAPI_E_INVALID_PARAMETER;
		goto end;
	}

	if (r->in.lpETable) {
		all_mids = r->in.lpETable;
	}
	else {
		all_mids = talloc_zero(mem_ctx, struct PropertyTagArray_r);
		emsabp_search(mem_ctx, emsabp_ctx, all_mids, NULL, r->in.pStat, 0);
	}

	/* find the records matching the qualifier */
	seek_restriction = talloc_zero(mem_ctx, struct Restriction_r);
	seek_restriction->rt = RES_PROPERTY;
	seek_restriction->res.resProperty.relop = RELOP_GE;
	seek_restriction->res.resProperty.ulPropTag = r->in.pTarget->ulPropTag;
	seek_restriction->res.resProperty.lpProp = r->in.pTarget;

	mids = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	if (emsabp_search(mem_ctx, emsabp_ctx, mids, seek_restriction, r->in.pStat, 0) != MAPI_E_SUCCESS) {
		mids = all_mids;
		retval = MAPI_E_NOT_FOUND;
	}

	r->in.pStat->CurrentRec = MID_END_OF_TABLE;
	r->in.pStat->NumPos = r->in.pStat->TotalRecs = all_mids->cValues;
	for (row = 0; row < all_mids->cValues; row++) {
		if (all_mids->aulPropTag[row] == mids->aulPropTag[0]) {
			r->in.pStat->CurrentRec = mids->aulPropTag[0];
			r->in.pStat->NumPos = row;
			break;
		}
	}

	/* now we need to populate the rows, if properties were requested */
	r->out.pStat = r->in.pStat;
	if (!r->in.pPropTags || !r->in.pPropTags->cValues) {
		*r->out.pRows = NULL;
		goto end;
	}

	r->out.pRows = talloc_zero(mem_ctx, struct PropertyRowSet_r *);
	r->out.pRows[0] = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	r->out.pRows[0]->cRows = mids->cValues;
	r->out.pRows[0]->aRow = talloc_array(mem_ctx, struct PropertyRow_r, mids->cValues);
	for (row = 0; row < mids->cValues; row++) {
		ret = emsabp_fetch_attrs(mem_ctx, emsabp_ctx, &(r->out.pRows[0]->aRow[row]), 
					    mids->aulPropTag[row], fEphID, r->in.pPropTags);
		if (ret) {
			retval = ret;
			DEBUG(5, ("failure looking up value %d\n", row));
			goto end;
		}
	}

end:

	DCESRV_NSP_RETURN(r, retval, NULL);
}


/**
   \details exchange_nsp NspiGetMatches (0x5) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiGetMatches request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_NspiGetMatches(struct dcesrv_call_state *dce_call,
					     TALLOC_CTX *mem_ctx,
					     struct NspiGetMatches *r)
{
	enum MAPISTATUS			retval;
	struct emsabp_context		*emsabp_ctx = NULL;
	struct PropertyTagArray_r	*ppOutMIds = NULL;
	uint32_t			i;
	

	DEBUG(3, ("exchange_nsp: NspiGetMatches (0x5)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	if (!emsabp_ctx) {
		DCESRV_NSP_RETURN(r, MAPI_E_CALL_FAILED, NULL);
	}

	/* Step 1. Retrieve MIds array given search criterias */
	ppOutMIds = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	ppOutMIds->cValues = 0;
	ppOutMIds->aulPropTag = NULL;

	retval = emsabp_search(mem_ctx, emsabp_ctx, ppOutMIds, r->in.Filter, r->in.pStat, r->in.ulRequested);
	if (retval != MAPI_E_SUCCESS) {
	failure:
		r->out.pStat = r->in.pStat;
		*r->out.ppOutMIds = ppOutMIds;	
		r->out.ppRows = talloc(mem_ctx, struct PropertyRowSet_r *);
		r->out.ppRows[0] = NULL;
		DCESRV_NSP_RETURN(r, retval, NULL);
	}

	*r->out.ppOutMIds = ppOutMIds;

	/* Step 2. Retrieve requested properties for these MIds */
	r->out.ppRows = talloc_zero(mem_ctx, struct PropertyRowSet_r *);
	r->out.ppRows[0] = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	r->out.ppRows[0]->cRows = ppOutMIds->cValues;
	r->out.ppRows[0]->aRow = talloc_array(mem_ctx, struct PropertyRow_r, ppOutMIds->cValues);

	for (i = 0; i < ppOutMIds->cValues; i++) {
		retval = emsabp_fetch_attrs(mem_ctx, emsabp_ctx, &(r->out.ppRows[0]->aRow[i]), 
					    ppOutMIds->aulPropTag[i], fEphID, r->in.pPropTags);
		if (retval) {
			DEBUG(5, ("failure looking up value %d\n", i));
			goto failure;
		}
	}

	DCESRV_NSP_RETURN(r, MAPI_E_SUCCESS, NULL);
}


/**
   \details exchange_nsp NspiResortRestriction (0x6) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiResortRestriction request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_NspiResortRestriction(struct dcesrv_call_state *dce_call,
						    TALLOC_CTX *mem_ctx,
						    struct NspiResortRestriction *r)
{
	DEBUG(3, ("exchange_nsp: NspiResortRestriction (0x6) not implemented\n"));
	DCESRV_NSP_RETURN(r, DCERPC_FAULT_OP_RNG_ERROR, NULL);
}


/**
   \details exchange_nsp NspiDNToMId (0x7) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiDNToMId request data

   \note Only searches within configuration.ldb are supported at the
   moment.

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_NspiDNToMId(struct dcesrv_call_state *dce_call,
					  TALLOC_CTX *mem_ctx,
					  struct NspiDNToMId *r)
{
	enum MAPISTATUS			retval;
	struct emsabp_context		*emsabp_ctx = NULL;
	struct ldb_message		*msg;
	uint32_t			i;
	uint32_t			MId;
	const char			*dn;
	bool				pbUseConfPartition;

	DEBUG(3, ("exchange_nsp: NspiDNToMId (0x7)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	if (!emsabp_ctx) {
		DCESRV_NSP_RETURN(r, MAPI_E_CALL_FAILED, NULL);
	}
	
	r->out.ppMIds = talloc_array(mem_ctx, struct PropertyTagArray_r *, 2);
	r->out.ppMIds[0] = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	r->out.ppMIds[0]->cValues = r->in.pNames->Count;
	r->out.ppMIds[0]->aulPropTag = talloc_array(mem_ctx, uint32_t, r->in.pNames->Count);

	for (i = 0; i < r->in.pNames->Count; i++) {
		/* Step 1. Check if the input legacyDN exists */
	  retval = emsabp_search_legacyExchangeDN(emsabp_ctx, r->in.pNames->Strings[i], &msg, &pbUseConfPartition);
		if (retval != MAPI_E_SUCCESS) {
		  r->out.ppMIds[0]->aulPropTag[i] = (enum MAPITAGS) 0;
		} else {
			TDB_CONTEXT *tdb_ctx = (pbUseConfPartition ? emsabp_ctx->tdb_ctx : emsabp_ctx->ttdb_ctx);
			dn = ldb_msg_find_attr_as_string(msg, "distinguishedName", NULL);
			retval = emsabp_tdb_fetch_MId(tdb_ctx, dn, &MId);
			if (retval) {
				retval = emsabp_tdb_insert(tdb_ctx, dn);
				retval = emsabp_tdb_fetch_MId(tdb_ctx, dn, &MId);
			}
			r->out.ppMIds[0]->aulPropTag[i] = (enum MAPITAGS) MId;
		}
	}

	DCESRV_NSP_RETURN(r, MAPI_E_SUCCESS, NULL);
}


/**
   \details exchange_nsp NspiGetPropList (0x8) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiGetPropList request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_NspiGetPropList(struct dcesrv_call_state *dce_call,
					      TALLOC_CTX *mem_ctx,
					      struct NspiGetPropList *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetPropList (0x8) not implemented\n"));
	DCESRV_NSP_RETURN(r, DCERPC_FAULT_OP_RNG_ERROR, NULL);
}


/**
   \details exchange_nsp NspiGetProps (0x9) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiGetProps request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_NspiGetProps(struct dcesrv_call_state *dce_call,
					   TALLOC_CTX *mem_ctx,
					   struct NspiGetProps *r)
{
	enum MAPISTATUS			retval;
	struct emsabp_context		*emsabp_ctx = NULL;
	uint32_t			MId;
	int				i;
	struct SPropTagArray		*pPropTags;

	DEBUG(3, ("exchange_nsp: NspiGetProps (0x9)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	if (!emsabp_ctx) {
		DCESRV_NSP_RETURN(r, MAPI_E_CALL_FAILED, NULL);
	}

	MId = r->in.pStat->CurrentRec;
	
	/* Step 1. Sanity Checks (MS-NSPI Server Processing Rules) */
	if (r->in.pStat->ContainerID && (emsabp_tdb_lookup_MId(emsabp_ctx->tdb_ctx, r->in.pStat->ContainerID) == false)) {
		DCESRV_NSP_RETURN(r, MAPI_E_INVALID_BOOKMARK, NULL);
	}

	/* Step 2. Fetch properties */
	r->out.ppRows = talloc_array(mem_ctx, struct PropertyRow_r *, 2);
	r->out.ppRows[0] = talloc_zero(r->out.ppRows, struct PropertyRow_r);

	pPropTags = r->in.pPropTags;
	if (!pPropTags) {
		pPropTags = talloc_zero(r, struct SPropTagArray);
		pPropTags->cValues = 9;
		pPropTags->aulPropTag = talloc_array(pPropTags, enum MAPITAGS, pPropTags->cValues + 1);
		pPropTags->aulPropTag[0] = PR_ADDRTYPE_UNICODE;
		pPropTags->aulPropTag[1] = PR_SMTP_ADDRESS_UNICODE;
		pPropTags->aulPropTag[2] = PR_OBJECT_TYPE;
		pPropTags->aulPropTag[3] = PR_DISPLAY_TYPE;
		pPropTags->aulPropTag[4] = PR_ENTRYID;
		pPropTags->aulPropTag[5] = PR_ORIGINAL_ENTRYID;
		pPropTags->aulPropTag[6] = PR_SEARCH_KEY;
		pPropTags->aulPropTag[7] = PR_INSTANCE_KEY;
		pPropTags->aulPropTag[8] = PR_EMAIL_ADDRESS;
		pPropTags->aulPropTag[pPropTags->cValues] = 0;
		r->in.pPropTags = pPropTags;
	}

	retval = emsabp_fetch_attrs(mem_ctx, emsabp_ctx, r->out.ppRows[0], MId, r->in.dwFlags, pPropTags);
	if (retval != MAPI_E_SUCCESS) {
		/* Is MId is not found, proceed as if no attributes were found */
		if (retval == MAPI_E_INVALID_BOOKMARK) {
			dcesrv_make_ptyp_error_property_row(mem_ctx, pPropTags, r->out.ppRows[0]);
			retval = MAPI_W_ERRORS_RETURNED;
		} else {
			talloc_free(r->out.ppRows);
			r->out.ppRows = NULL;
		}
		DCESRV_NSP_RETURN(r, retval, NULL);
	}

	/* Step 3. Properties are fetched. Provide proper return
	 value.  ErrorsReturned should be returned when at least one
	 property is not found */
	for (i = 0; i < r->out.ppRows[0]->cValues; i++) {
		if ((r->out.ppRows[0]->lpProps[i].ulPropTag & 0xFFFF) == PT_ERROR) {
			retval = MAPI_W_ERRORS_RETURNED;
			break;
		}
	}

	DCESRV_NSP_RETURN(r, retval, NULL);
}


/**
   \details exchange_nsp NspiCompareMIds (0xA) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiCompareMIds request data

   \return MAPI_E_SUCCESS on success
*/
static void dcesrv_NspiCompareMIds(struct dcesrv_call_state *dce_call,
					      TALLOC_CTX *mem_ctx,
					      struct NspiCompareMIds *r)
{
	DEBUG(3, ("exchange_nsp: NspiCompareMIds (0xA) not implemented\n"));
	DCESRV_NSP_RETURN(r, DCERPC_FAULT_OP_RNG_ERROR, NULL);
}


/**
   \details exchange_nsp NspiModProps (0xB) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiModProps request data

   \return MAPI_E_SUCCESS on success

 */
static void dcesrv_NspiModProps(struct dcesrv_call_state *dce_call,
					   TALLOC_CTX *mem_ctx,
					   struct NspiModProps *r)
{
	DEBUG(3, ("exchange_nsp: NspiModProps (0xB) not implemented\n"));
	DCESRV_NSP_RETURN(r, DCERPC_FAULT_OP_RNG_ERROR, NULL);
}


/**
   \details exchange_nsp NspiGetSpecialTable (0xC) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiGetSpecialTable request data

   \note MS-NSPI specifies lpVersion "holds the value of the version
   number of the hierarchy table that the client has." We will ignore
   this for the moment.

   \return MAPI_E_SUCCESS on success

 */
static void dcesrv_NspiGetSpecialTable(struct dcesrv_call_state *dce_call,
				       TALLOC_CTX *mem_ctx,
				       struct NspiGetSpecialTable *r)
{
	struct emsabp_context		*emsabp_ctx = NULL;

	DEBUG(3, ("exchange_nsp: NspiGetSpecialTable (0xC)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	if (!emsabp_ctx) {
		DCESRV_NSP_RETURN(r, MAPI_E_CALL_FAILED, NULL);
	}

	/* Step 1. (FIXME) We arbitrary set lpVersion to 0x1 */
	r->out.lpVersion = talloc_zero(mem_ctx, uint32_t);
	*r->out.lpVersion = 0x1;

	/* Step 2. Allocate output SRowSet and call associated emsabp function */
	r->out.ppRows = talloc_zero(mem_ctx, struct PropertyRowSet_r *);
	if (!r->out.ppRows) {
		DCESRV_NSP_RETURN(r, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
	}
	r->out.ppRows[0] = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	if (!r->out.ppRows[0]) {
		DCESRV_NSP_RETURN(r, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
	}

	if (r->in.dwFlags & NspiAddressCreationTemplates) {
		DEBUG(5, ("CreationTemplates Table requested\n"));
		r->out.result = emsabp_get_CreationTemplatesTable(mem_ctx, emsabp_ctx, r->in.dwFlags, r->out.ppRows);
	} else {
		DEBUG(5, ("Hierarchy Table requested\n"));
		r->out.result = emsabp_get_HierarchyTable(mem_ctx, emsabp_ctx, r->in.dwFlags, r->out.ppRows);
	}
}


/**
   \details exchange_nsp NspiGetTemplateInfo (0xD) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiGetTemplateInfo request data

   \return MAPI_E_SUCCESS on success

 */
static void dcesrv_NspiGetTemplateInfo(struct dcesrv_call_state *dce_call,
				       TALLOC_CTX *mem_ctx,
				       struct NspiGetTemplateInfo *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetTemplateInfo (0xD) not implemented\n"));
	DCESRV_NSP_RETURN(r, DCERPC_FAULT_OP_RNG_ERROR, NULL);
}


/**
   \details exchange_nsp NspiModLinkAtt (0xE) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiModLinkAtt request data

   \return MAPI_E_SUCCESS on success

 */
static void dcesrv_NspiModLinkAtt(struct dcesrv_call_state *dce_call,
				  TALLOC_CTX *mem_ctx,
				  struct NspiModLinkAtt *r)
{
	DEBUG(3, ("exchange_nsp: NspiModLinkAtt (0xE) not implemented\n"));
	DCESRV_NSP_RETURN(r, DCERPC_FAULT_OP_RNG_ERROR, NULL);
}


/**
   \details exchange_nsp NspiDeleteEntries (0xF) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiDeleteEntries request data

   \return MAPI_E_SUCCESS on success

 */
static void dcesrv_NspiDeleteEntries(struct dcesrv_call_state *dce_call,
				     TALLOC_CTX *mem_ctx,
				     struct NspiDeleteEntries *r)
{
	DEBUG(3, ("exchange_nsp: NspiDeleteEntries (0xF) not implemented\n"));
	DCESRV_NSP_RETURN(r, DCERPC_FAULT_OP_RNG_ERROR, NULL);
}


/**
   \details exchange_nsp NspiQueryColumns (0x10) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiQueryColumns request data

   \return MAPI_E_SUCCESS on success

 */
static void dcesrv_NspiQueryColumns(struct dcesrv_call_state *dce_call,
				    TALLOC_CTX *mem_ctx,
				    struct NspiQueryColumns *r)
{
	DEBUG(3, ("exchange_nsp: NspiQueryColumns (0x10) not implemented\n"));
	DCESRV_NSP_RETURN(r, DCERPC_FAULT_OP_RNG_ERROR, NULL);
}


/**
   \details exchange_nsp NspiGetNamesFromIDs (0x11) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiGetNamesFromIDs request data

   \return MAPI_E_SUCCESS on success

 */
static void dcesrv_NspiGetNamesFromIDs(struct dcesrv_call_state *dce_call,
				       TALLOC_CTX *mem_ctx,
				       struct NspiGetNamesFromIDs *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetNamesFromIDs (0x11) not implemented\n"));
	DCESRV_NSP_RETURN(r, DCERPC_FAULT_OP_RNG_ERROR, NULL);
}


/**
   \details exchange_nsp NspiGetIDsFromNames (0x12) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiGetIDsFromNames request data

   \return MAPI_E_SUCCESS on success

 */
static void dcesrv_NspiGetIDsFromNames(struct dcesrv_call_state *dce_call,
				       TALLOC_CTX *mem_ctx,
				       struct NspiGetIDsFromNames *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetIDsFromNames (0x12) not implemented\n"));
	DCESRV_NSP_RETURN(r, DCERPC_FAULT_OP_RNG_ERROR, NULL);
}


/**
   \details exchange_nsp NspiResolveNames (0x13) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiResolveNames request data

   \return MAPI_E_SUCCESS on success

 */
static void dcesrv_NspiResolveNames(struct dcesrv_call_state *dce_call,
				    TALLOC_CTX *mem_ctx,
				    struct NspiResolveNames *r)
{
	enum MAPISTATUS			retval = MAPI_E_SUCCESS;
	struct emsabp_context		*emsabp_ctx = NULL;
	struct SPropTagArray		*pPropTags;
	char				*filter_search = NULL;
	struct PropertyTagArray_r	*pMIds = NULL;
	struct PropertyRowSet_r		*pRows = NULL;
	struct StringsArray_r		*paStr;
	uint32_t			i;
	int				ret;
	const char * const		recipient_attrs[] = { "*", NULL };
	const char * const		search_attr[] = { "mailNickName", "mail", "name",
							  "displayName", "givenName", 
							  "sAMAccountName", "proxyAddresses" };

	DEBUG(3, ("exchange_nsp: NspiResolveNames (0x13)\n"));

	if (!dcesrv_call_authenticated(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	if (!emsabp_ctx) {
		DEBUG(5, ("[nspi][%s:%d] emsabp_context not found\n", __FUNCTION__, __LINE__));
		DCESRV_NSP_RETURN(r, MAPI_E_CALL_FAILED, NULL);
	}

	/* Step 1. Prepare in/out data */
	retval = emsabp_ab_fetch_filter(mem_ctx, emsabp_ctx, r->in.pStat->ContainerID, &filter_search);
	if (retval != MAPI_E_SUCCESS) {
		DEBUG(5, ("[nspi][%s:%d] ab_fetch_filter failed\n", __FUNCTION__, __LINE__));
		DCESRV_NSP_RETURN(r, MAPI_E_INVALID_BOOKMARK, NULL);
	}

	/* Set the default list of property tags if none were provided in input */
	if (!r->in.pPropTags) {
		pPropTags = set_SPropTagArray(mem_ctx, 0x7,
					      PR_EMS_AB_CONTAINERID,
					      PR_OBJECT_TYPE,
					      PR_DISPLAY_TYPE,
					      PR_DISPLAY_NAME,
					      PR_OFFICE_TELEPHONE_NUMBER,
					      PR_COMPANY_NAME,
					      PR_OFFICE_LOCATION);
	} else {
		pPropTags = r->in.pPropTags;
	}

	/* Allocate output MIds */
	paStr = r->in.paStr;
	pMIds = talloc(mem_ctx, struct PropertyTagArray_r);
	DCESRV_NSP_RETURN_IF(!pMIds, r, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
	pMIds->cValues = paStr->Count;
	pMIds->aulPropTag = (uint32_t *) talloc_array(pMIds, uint32_t, pMIds->cValues);
	DCESRV_NSP_RETURN_IF(!pMIds->aulPropTag, r, MAPI_E_NOT_ENOUGH_MEMORY, pMIds);

	pRows = talloc(mem_ctx, struct PropertyRowSet_r);
	DCESRV_NSP_RETURN_IF(!pRows, r, MAPI_E_NOT_ENOUGH_MEMORY, pMIds);
	pRows->cRows = 0;
	pRows->aRow = talloc_array(pRows, struct PropertyRow_r, pMIds->cValues);
	if (!pRows->aRow) {
		retval = MAPI_E_NOT_ENOUGH_MEMORY;
		goto error;
	}

	/* Step 2. Fetch AB container records */
	for (i = 0; i < paStr->Count; i++) {
		struct ldb_result	*ldb_res;
		char			*filter = talloc_strdup(mem_ctx, "");
		int			j;

		if (!filter) {
			retval = MAPI_E_NOT_ENOUGH_MEMORY;
			goto error;
		}
		/* Build search filter */
		for (j = 0; j < ARRAY_SIZE(search_attr); j++) {
			char *attr_filter = talloc_asprintf(mem_ctx, "(%s=%s)", search_attr[j],
							    ldb_binary_encode_string(mem_ctx, paStr->Strings[i]));
			if (!attr_filter) {
				retval = MAPI_E_NOT_ENOUGH_MEMORY;
				goto error;
			}
			filter = talloc_strdup_append(filter, attr_filter);
			if (!filter) {
				retval = MAPI_E_NOT_ENOUGH_MEMORY;
				goto error;
			}
			talloc_free(attr_filter);
		}

		/* Search AD */
		filter = talloc_asprintf(mem_ctx, "(&%s(|%s))", filter_search, filter);
		if (!filter) {
			retval = MAPI_E_NOT_ENOUGH_MEMORY;
			goto error;
		}
		ret = ldb_search(emsabp_ctx->samdb_ctx, mem_ctx, &ldb_res,
				 ldb_get_default_basedn(emsabp_ctx->samdb_ctx),
				 LDB_SCOPE_SUBTREE, recipient_attrs, "%s", filter);

		/* Determine name resolution status and fetch object upon success */
		if (ret != LDB_SUCCESS || ldb_res->count == 0) {
			pMIds->aulPropTag[i] = MAPI_UNRESOLVED;
		} else if (ldb_res->count > 1) {
			pMIds->aulPropTag[i] = MAPI_AMBIGUOUS;
		} else {
			pMIds->aulPropTag[i] = MAPI_RESOLVED;
			retval = emsabp_fetch_attrs_from_msg(mem_ctx, emsabp_ctx, &pRows->aRow[pRows->cRows],
							     ldb_res->msgs[0], 0, 0, pPropTags);
			if (retval != MAPI_E_SUCCESS) {
				DEBUG(5, ("[nspi][%s:%d] emsabp_fetch_attrs_from_msg failed\n", __FUNCTION__, __LINE__));
				goto error;
			}
			pRows->cRows++;
		}
	}

	*r->out.ppMIds = pMIds;
	if (pRows->cRows) {
		*r->out.ppRows = pRows;
	}

	DCESRV_NSP_RETURN(r, retval, NULL);
error:
	DEBUG(5, ("[nspi][%s:%d] unexpected error %d\n", __FUNCTION__, __LINE__, retval));
	talloc_free(pMIds);
	talloc_free(pRows);
	DCESRV_NSP_RETURN(r, retval, NULL);
}


/**
   \details exchange_nsp NspiResolveNamesW (0x14) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiResolveNamesW request data

   \return MAPI_E_SUCCESS on success

 */
static void dcesrv_NspiResolveNamesW(struct dcesrv_call_state *dce_call,
				     TALLOC_CTX *mem_ctx,
				     struct NspiResolveNamesW *r)
{
	enum MAPISTATUS			retval = MAPI_E_SUCCESS;
	struct emsabp_context		*emsabp_ctx = NULL;
	struct SPropTagArray		*pPropTags;
	char				*filter_search = NULL;
	struct PropertyTagArray_r	*pMIds = NULL;
	struct PropertyRowSet_r		*pRows = NULL;
	struct StringsArrayW_r		*paWStr;
	uint32_t			i;
	int				ret;
	const char * const		recipient_attrs[] = { "*", NULL };
	const char * const		search_attr[] = { "mailNickName", "mail", "name", 
							  "displayName", "givenName", "sAMAccountName" };

	DEBUG(3, ("exchange_nsp: NspiResolveNamesW (0x14)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	if (!emsabp_ctx) {
		DEBUG(5, ("[nspi][%s:%d] emsabp_context not found\n", __FUNCTION__, __LINE__));
		DCESRV_NSP_RETURN(r, MAPI_E_CALL_FAILED, NULL);
	}

	/* Step 1. Prepare in/out data */
	retval = emsabp_ab_fetch_filter(mem_ctx, emsabp_ctx, r->in.pStat->ContainerID, &filter_search);
	if (retval != MAPI_E_SUCCESS) {
		DEBUG(5, ("[nspi][%s:%d] ab_fetch_filter failed\n", __FUNCTION__, __LINE__));
		DCESRV_NSP_RETURN(r, MAPI_E_INVALID_BOOKMARK, NULL);
	}

	/* Set default list of property tags if none were provided in input */
	if (!r->in.pPropTags) {
		pPropTags = set_SPropTagArray(mem_ctx, 0x7,
					      PR_EMS_AB_CONTAINERID,
					      PR_OBJECT_TYPE,
					      PR_DISPLAY_TYPE,
					      PR_DISPLAY_NAME,
					      PR_OFFICE_TELEPHONE_NUMBER,
					      PR_COMPANY_NAME,
					      PR_OFFICE_LOCATION);
	} else {
		pPropTags = r->in.pPropTags;
	}

	/* Allocate output MIds */
	paWStr = r->in.paWStr;
	pMIds = talloc(mem_ctx, struct PropertyTagArray_r);
	DCESRV_NSP_RETURN_IF(!pMIds, r, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
	pMIds->cValues = paWStr->Count;
	pMIds->aulPropTag = talloc_array(pMIds, uint32_t, pMIds->cValues);
	DCESRV_NSP_RETURN_IF(!pMIds->aulPropTag, r, MAPI_E_NOT_ENOUGH_MEMORY, pMIds);

	pRows = talloc(mem_ctx, struct PropertyRowSet_r);
	DCESRV_NSP_RETURN_IF(!pRows, r, MAPI_E_NOT_ENOUGH_MEMORY, pMIds);
	pRows->cRows = 0;
	pRows->aRow = talloc_array(mem_ctx, struct PropertyRow_r, pMIds->cValues);
	if (!pRows->aRow) {
		retval = MAPI_E_NOT_ENOUGH_MEMORY;
		goto error;
	}

	/* Step 2. Fetch AB container records */
	for (i = 0; i < paWStr->Count; i++) {
		struct ldb_result	*ldb_res;
		char			*filter = talloc_strdup(mem_ctx, "");
		int			j;

		if (!filter) {
			retval = MAPI_E_NOT_ENOUGH_MEMORY;
			goto error;
		}
		/* Build search filter */
		for (j = 0; j < ARRAY_SIZE(search_attr); j++) {
			char *attr_filter = talloc_asprintf(mem_ctx, "(%s=%s)", search_attr[j],
							    ldb_binary_encode_string(mem_ctx, paWStr->Strings[i]));
			if (!attr_filter) {
				retval = MAPI_E_NOT_ENOUGH_MEMORY;
				goto error;
			}
			filter = talloc_strdup_append(filter, attr_filter);
			if (!filter) {
				retval = MAPI_E_NOT_ENOUGH_MEMORY;
				goto error;
			}
			talloc_free(attr_filter);
		}

		/* Search AD */
		filter = talloc_asprintf(mem_ctx, "(&%s(|%s))", filter_search, filter);
		if (!filter) {
			retval = MAPI_E_NOT_ENOUGH_MEMORY;
			goto error;
		}
		ret = ldb_search(emsabp_ctx->samdb_ctx, mem_ctx, &ldb_res,
				 ldb_get_default_basedn(emsabp_ctx->samdb_ctx),
				 LDB_SCOPE_SUBTREE, recipient_attrs, "%s", filter);

		/* Determine name resolutation status and fetch object upon success */
		if (ret != LDB_SUCCESS || ldb_res->count == 0) {
			pMIds->aulPropTag[i] = MAPI_UNRESOLVED;
		} else if (ldb_res->count > 1) {
			pMIds->aulPropTag[i] = MAPI_AMBIGUOUS;
		} else {
			pMIds->aulPropTag[i] = MAPI_RESOLVED;
			retval = emsabp_fetch_attrs_from_msg(mem_ctx, emsabp_ctx, &pRows->aRow[pRows->cRows],
							     ldb_res->msgs[0], 0, 0, pPropTags);
			if (retval != MAPI_E_SUCCESS) {
				DEBUG(5, ("[nspi][%s:%d] emsabp_fetch_attrs_from_msg failed\n", __FUNCTION__, __LINE__));
				goto error;
			}
			pRows->cRows++;
		}
	}

	*r->out.ppMIds = pMIds;
	if (pRows->cRows) {
		*r->out.ppRows = pRows;
	}

	DCESRV_NSP_RETURN(r, retval, NULL);
error:
	DEBUG(5, ("[nspi][%s:%d] unexpected error %d\n", __FUNCTION__, __LINE__, retval));
	talloc_free(pMIds);
	talloc_free(pRows);
	DCESRV_NSP_RETURN(r, retval, NULL);
}


/**
   \details Dispatch incoming NSPI call to the correct OpenChange
   server function.

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r generic pointer on NSPI data
   \param mapiproxy pointer to the mapiproxy structure controlling 
   mapiproxy behavior

   \return NT_STATUS_OK
 */
static NTSTATUS dcesrv_exchange_nsp_dispatch(struct dcesrv_call_state *dce_call,
					     TALLOC_CTX *mem_ctx,
					     void *r, struct mapiproxy *mapiproxy)
{
	const struct ndr_interface_table	*table;
	uint16_t				opnum;

	DEBUG (5, ("dcesrv_exchange_nsp_dispatch\n"));

	table = (const struct ndr_interface_table *) dce_call->context->iface->private_data;
	opnum = dce_call->pkt.u.request.opnum;

	/* Sanity checks */
	if (!table) return NT_STATUS_UNSUCCESSFUL;
	if (table->name && strcmp(table->name, NDR_EXCHANGE_NSP_NAME)) return NT_STATUS_UNSUCCESSFUL;

	switch (opnum) {
	case NDR_NSPIBIND:
		dcesrv_NspiBind(dce_call, mem_ctx, (struct NspiBind *)r);
		break;
	case NDR_NSPIUNBIND:
		dcesrv_NspiUnbind(dce_call, mem_ctx, (struct NspiUnbind *)r);
		break;
	case NDR_NSPIUPDATESTAT:
		dcesrv_NspiUpdateStat(dce_call, mem_ctx, (struct NspiUpdateStat *)r);
		break;
	case NDR_NSPIQUERYROWS:
		dcesrv_NspiQueryRows(dce_call, mem_ctx, (struct NspiQueryRows *)r);
		break;
	case NDR_NSPISEEKENTRIES:
		dcesrv_NspiSeekEntries(dce_call, mem_ctx, (struct NspiSeekEntries *)r);
		break;
	case NDR_NSPIGETMATCHES:
		dcesrv_NspiGetMatches(dce_call, mem_ctx, (struct NspiGetMatches *)r);
		break;
	case NDR_NSPIRESORTRESTRICTION:
		dcesrv_NspiResortRestriction(dce_call, mem_ctx, (struct NspiResortRestriction *)r);
		break;
	case NDR_NSPIDNTOMID:
		dcesrv_NspiDNToMId(dce_call, mem_ctx, (struct NspiDNToMId *)r);
		break;
	case NDR_NSPIGETPROPLIST:
		dcesrv_NspiGetPropList(dce_call, mem_ctx, (struct NspiGetPropList *)r);
		break;
	case NDR_NSPIGETPROPS:
		dcesrv_NspiGetProps(dce_call, mem_ctx, (struct NspiGetProps *)r);
		break;
	case NDR_NSPICOMPAREMIDS:
		dcesrv_NspiCompareMIds(dce_call, mem_ctx, (struct NspiCompareMIds *)r);
		break;
	case NDR_NSPIMODPROPS:
		dcesrv_NspiModProps(dce_call, mem_ctx, (struct NspiModProps *)r);
		break;
	case NDR_NSPIGETSPECIALTABLE:
		dcesrv_NspiGetSpecialTable(dce_call, mem_ctx, (struct NspiGetSpecialTable *)r);
		break;
	case NDR_NSPIGETTEMPLATEINFO:
		dcesrv_NspiGetTemplateInfo(dce_call, mem_ctx, (struct NspiGetTemplateInfo *)r);
		break;
	case NDR_NSPIMODLINKATT:
		dcesrv_NspiModLinkAtt(dce_call, mem_ctx, (struct NspiModLinkAtt *)r);
		break;
	case NDR_NSPIDELETEENTRIES:
		dcesrv_NspiDeleteEntries(dce_call, mem_ctx, (struct NspiDeleteEntries *)r);
		break;
	case NDR_NSPIQUERYCOLUMNS:
		dcesrv_NspiQueryColumns(dce_call, mem_ctx, (struct NspiQueryColumns *)r);
		break;
	case NDR_NSPIGETNAMESFROMIDS:
		dcesrv_NspiGetNamesFromIDs(dce_call, mem_ctx, (struct NspiGetNamesFromIDs *)r);
		break;
	case NDR_NSPIGETIDSFROMNAMES:
		dcesrv_NspiGetIDsFromNames(dce_call, mem_ctx, (struct NspiGetIDsFromNames *)r);
		break;
	case NDR_NSPIRESOLVENAMES:
		dcesrv_NspiResolveNames(dce_call, mem_ctx, (struct NspiResolveNames *)r);
		break;
	case NDR_NSPIRESOLVENAMESW:
		dcesrv_NspiResolveNamesW(dce_call, mem_ctx, (struct NspiResolveNamesW *)r);
		break;
	}

	return NT_STATUS_OK;
}


/**
   \details Initialize the NSPI OpenChange server

   \param dce_ctx pointer to the server context

   \return NT_STATUS_OK on success, otherwise NT_STATUS_NO_MEMORY
 */
static NTSTATUS dcesrv_exchange_nsp_init(struct dcesrv_context *dce_ctx)
{
	DEBUG (0, ("dcesrv_exchange_nsp_init\n"));
	/* Initialize exchange_nsp session */
	nsp_session = talloc_zero(dce_ctx, struct exchange_nsp_session);
	if (!nsp_session) return NT_STATUS_NO_MEMORY;
	nsp_session->session = NULL;

	/* Open a read-write pointer on the EMSABP TDB database */
	emsabp_tdb_ctx = emsabp_tdb_init((TALLOC_CTX *)dce_ctx, dce_ctx->lp_ctx);
	if (!emsabp_tdb_ctx) {
		OC_ABORT(false, ("[exchange_nsp] Unable to initialize emsabp_tdb context"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	return NT_STATUS_OK;
}


/**
   \details Terminates the NSPI connection and release the associated
   session and context if still available. This case occurs when the
   client doesn't call NspiUnbind but quit unexpectedly.

   \param server_id reference to the server identifier structure
   \param context_id the connection context identifier

   \return NT_STATUS_OK on success
 */
static NTSTATUS dcesrv_exchange_nsp_unbind(struct server_id server_id, uint32_t context_id)
{
	DEBUG (0, ("dcesrv_exchange_nsp_unbind\n"));
	return NT_STATUS_OK;
}


/**
   \details Entry point for the default OpenChange NSPI server

   \return NT_STATUS_OK on success, otherwise NTSTATUS error
 */
NTSTATUS samba_init_module(void)
{
	struct mapiproxy_module	server;
	NTSTATUS		ret;

	/* Fill in our name */
	server.name = "exchange_nsp";
	server.status = MAPIPROXY_DEFAULT;
	server.description = "OpenChange NSPI server";
	server.endpoint = "exchange_nsp";

	/* Fill in all the operations */
	server.init = dcesrv_exchange_nsp_init;
	server.unbind = dcesrv_exchange_nsp_unbind;
	server.dispatch = dcesrv_exchange_nsp_dispatch;
	server.push = NULL;
	server.pull = NULL;
	server.ndr_pull = NULL;

	/* Register ourselves with the MAPIPROXY server subsystem */
	ret = mapiproxy_server_register(&server);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0, ("Failed to register the 'exchange_nsp' default mapiproxy server!\n"));
		return ret;
	}

	return ret;
}
