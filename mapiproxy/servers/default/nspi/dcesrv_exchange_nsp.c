/*
   MAPI Proxy - Exchange NSPI Server

   OpenChange Project

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
   \file dcesrv_exchange_nsp.c

   \brief OpenChange NSPI Server implementation
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "dcesrv_exchange_nsp.h"

struct exchange_nsp_session	*nsp_session = NULL;
TDB_CONTEXT			*emsabp_tdb_ctx = NULL;

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
static enum MAPISTATUS dcesrv_NspiBind(struct dcesrv_call_state *dce_call,
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
	if (!NTLM_AUTH_IS_OK(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));

		wire_handle.handle_type = EXCHANGE_HANDLE_NSP;
		wire_handle.uuid = GUID_zero();
		*r->out.handle = wire_handle;

		r->out.mapiuid = r->in.mapiuid;
		r->out.result = MAPI_E_LOGON_FAILED;
		return MAPI_E_LOGON_FAILED;		
	}

	/* Step 1. Initialize the emsabp context */
	emsabp_ctx = emsabp_init(dce_call->conn->dce_ctx->lp_ctx, emsabp_tdb_ctx);
	if (!emsabp_ctx) {
		smb_panic("unable to initialize emsabp context");
		OPENCHANGE_RETVAL_IF(!emsabp_ctx, MAPI_E_FAILONEPROVIDER, NULL);
	}

	/* Step 2. Check if incoming user belongs to the Exchange organization */
	if (emsabp_verify_user(dce_call, emsabp_ctx) == false) {
		talloc_free(emsabp_ctx);

		wire_handle.handle_type = EXCHANGE_HANDLE_NSP;
		wire_handle.uuid = GUID_zero();
		*r->out.handle = wire_handle;

		r->out.mapiuid = r->in.mapiuid;
		r->out.result = MAPI_E_LOGON_FAILED;
		return MAPI_E_LOGON_FAILED;
	}

	/* Step 3. Check if valid cpID has been supplied */
	if (emsabp_verify_codepage(emsabp_ctx, r->in.pStat->CodePage) == false) {
		talloc_free(emsabp_ctx);

		wire_handle.handle_type = EXCHANGE_HANDLE_NSP;
		wire_handle.uuid = GUID_zero();
		*r->out.handle = wire_handle;

		r->out.mapiuid = r->in.mapiuid;
		r->out.result = MAPI_E_UNKNOWN_CPID;
		return MAPI_E_UNKNOWN_CPID;
	}

	/* Step 4. Retrieve OpenChange server GUID */
	guid = (struct GUID *) samdb_ntds_objectGUID(emsabp_ctx->samdb_ctx);
	OPENCHANGE_RETVAL_IF(!guid, MAPI_E_FAILONEPROVIDER, emsabp_ctx);

	/* Step 5. Fill NspiBind reply */
	handle = dcesrv_handle_new(dce_call->context, EXCHANGE_HANDLE_NSP);
	OPENCHANGE_RETVAL_IF(!handle, MAPI_E_NOT_ENOUGH_RESOURCES, emsabp_ctx);

	handle->data = (void *) emsabp_ctx;
	*r->out.handle = handle->wire_handle;
	r->out.mapiuid = guid;
	r->out.result = MAPI_E_SUCCESS;

	/* Step 6. Associate this emsabp context to the session */
	session = talloc((TALLOC_CTX *)nsp_session, struct exchange_nsp_session);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_ENOUGH_RESOURCES, emsabp_ctx);

	session->session = mpm_session_init((TALLOC_CTX *)nsp_session, dce_call);
	OPENCHANGE_RETVAL_IF(!session->session, MAPI_E_NOT_ENOUGH_RESOURCES, emsabp_ctx);

	mpm_session_set_private_data(session->session, (void *) emsabp_ctx);
	mpm_session_set_destructor(session->session, emsabp_destructor);

	DLIST_ADD_END(nsp_session, session, struct exchange_nsp_session *);

	return MAPI_E_SUCCESS;
}


/**
   \details exchange_nsp NspiUnbind (0x1) function, Terminates a NSPI
   session with the client

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiUnbind call structure
 */
static enum MAPISTATUS dcesrv_NspiUnbind(struct dcesrv_call_state *dce_call,
					 TALLOC_CTX *mem_ctx,
					 struct NspiUnbind *r)
{
	struct dcesrv_handle		*h;
	struct exchange_nsp_session	*session;

	DEBUG(5, ("exchange_nsp: NspiUnbind (0x1)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!NTLM_AUTH_IS_OK(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		return MAPI_E_LOGON_FAILED;
	}

	/* Step 1. Retrieve handle and free if emsabp context and session are available */
	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	if (h) {
		for (session = nsp_session; session; session = session->next) {
			if ((mpm_session_cmp(session->session, dce_call) == true)) {
				mpm_session_release(session->session);
				DLIST_REMOVE(nsp_session, session);
				DEBUG(6, ("[%s:%d]: Session found and released\n", __FUNCTION__, __LINE__));
			}
		}
	}

	r->out.result = 1;

	return MAPI_E_SUCCESS;
}


/**
   \details exchange_nsp NspiUpdateStat (0x2) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiUpdateStat request data

   \return MAPI_E_SUCCESS on success
*/
static enum MAPISTATUS dcesrv_NspiUpdateStat(struct dcesrv_call_state *dce_call, 
					     TALLOC_CTX *mem_ctx,
					     struct NspiUpdateStat *r)
{
	DEBUG(3, ("exchange_nsp: NspiUpdateStat (0x2) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiQueryRows (0x3) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiQueryRows request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_NspiQueryRows(struct dcesrv_call_state *dce_call,
					    TALLOC_CTX *mem_ctx,
					    struct NspiQueryRows *r)
{
	enum MAPISTATUS			retval = MAPI_E_SUCCESS;
	struct dcesrv_handle		*h;
	struct emsabp_context		*emsabp_ctx;
	struct SPropTagArray		*pPropTags;
	struct SRowSet			*pRows;
	uint32_t			i;

	DEBUG(3, ("exchange_nsp: NspiQueryRows (0x3)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!NTLM_AUTH_IS_OK(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		return MAPI_E_LOGON_FAILED;
	}

	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	if (!h) return MAPI_E_LOGON_FAILED;
	emsabp_ctx = (struct emsabp_context *) h->data;

	/* Step 1. Sanity Checks (MS-NSPI Server Processing Rules) */
	if (r->in.pStat->ContainerID && (emsabp_tdb_lookup_MId(emsabp_ctx->tdb_ctx, r->in.pStat->ContainerID) == false)) {
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
	pRows = talloc_zero(mem_ctx, struct SRowSet);

	/* Step 2. Fill ppRows  */
	if (r->in.lpETable == NULL) {
		/* Step 2.1 Fill ppRows for supplied Container ID */
		struct ldb_result	*ldb_res;
		
		retval = emsabp_ab_container_enum(mem_ctx, emsabp_ctx,
						  r->in.pStat->ContainerID, &ldb_res);
		if (!MAPI_STATUS_IS_OK(retval))  {
			goto failure;
		}
		if (ldb_res->count) {
			pRows->cRows = ldb_res->count;
			pRows->aRow = talloc_array(mem_ctx, struct SRow, ldb_res->count);
		}

		/* fetch required attributes for every entry found */
		for (i = 0; i < ldb_res->count; i++) {
			retval = emsabp_fetch_attrs_from_msg(mem_ctx, emsabp_ctx, &(pRows->aRow[i]),
							     ldb_res->msgs[i], 0, r->in.dwFlags, pPropTags);
			if (!MAPI_STATUS_IS_OK(retval)) {
				goto failure;
			}
		}
	} else {
		/* Step 2.2 Fill ppRows for supplied table of MIds */
		pRows->cRows = r->in.dwETableCount;
		pRows->aRow = talloc_array(mem_ctx, struct SRow, r->in.dwETableCount);
		for (i = 0; i < r->in.dwETableCount; i++) {
			retval = emsabp_fetch_attrs(mem_ctx, emsabp_ctx, &(pRows->aRow[i]), r->in.lpETable[i], r->in.dwFlags, pPropTags);
			if (retval != MAPI_E_SUCCESS) {
				goto failure;
			}
		}
	}

	/* Step 3. Fill output params */
	*r->out.ppRows = pRows;

	memcpy(r->out.pStat, r->in.pStat, sizeof (struct STAT));
	r->out.pStat->TotalRecs = pRows->cRows;
	r->out.pStat->NumPos = r->out.pStat->Delta + pRows->cRows;
	r->out.pStat->CurrentRec = MID_END_OF_TABLE;

	return MAPI_E_SUCCESS;

failure:
	r->out.pStat = r->in.pStat;
	*r->out.ppRows = NULL;
	r->out.result = retval;

	return retval;
}


/**
   \details exchange_nsp NspiSeekEntries (0x4) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiSeekEntries request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_NspiSeekEntries(struct dcesrv_call_state *dce_call,
					      TALLOC_CTX *mem_ctx,
					      struct NspiSeekEntries *r)
{
	DEBUG(3, ("exchange_nsp: NspiSeekEntries (0x4) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiGetMatches (0x5) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiGetMatches request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_NspiGetMatches(struct dcesrv_call_state *dce_call,
					     TALLOC_CTX *mem_ctx,
					     struct NspiGetMatches *r)
{
	enum MAPISTATUS			retval;
	struct dcesrv_handle		*h;
	struct emsabp_context		*emsabp_ctx;
	struct SPropTagArray		*ppOutMIds = NULL;
	uint32_t			i;
	

	DEBUG(3, ("exchange_nsp: NspiGetMatches (0x5)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!NTLM_AUTH_IS_OK(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		return MAPI_E_LOGON_FAILED;
	}

	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	if (!h) return MAPI_E_LOGON_FAILED;
	emsabp_ctx = (struct emsabp_context *) h->data;

	/* Step 1. Retrieve MIds array given search criterias */
	ppOutMIds = talloc_zero(mem_ctx, struct SPropTagArray);
	ppOutMIds->cValues = 0;
	ppOutMIds->aulPropTag = NULL;

	retval = emsabp_search(mem_ctx, emsabp_ctx, ppOutMIds, r->in.Filter, r->in.pStat, r->in.ulRequested);
	if (retval != MAPI_E_SUCCESS) {
	failure:
		r->out.pStat = r->in.pStat;
		*r->out.ppOutMIds = ppOutMIds;	
		r->out.ppRows = talloc(mem_ctx, struct SRowSet *);
		r->out.ppRows[0] = NULL;
		r->out.result = retval;
		
		return retval;
	}

	*r->out.ppOutMIds = ppOutMIds;

	/* Step 2. Retrieve requested properties for these MIds */
	r->out.ppRows = talloc_zero(mem_ctx, struct SRowSet *);
	r->out.ppRows[0] = talloc_zero(mem_ctx, struct SRowSet);
	r->out.ppRows[0]->cRows = ppOutMIds->cValues;
	r->out.ppRows[0]->aRow = talloc_array(mem_ctx, struct SRow, ppOutMIds->cValues);
	

	for (i = 0; i < ppOutMIds->cValues; i++) {
		retval = emsabp_fetch_attrs(mem_ctx, emsabp_ctx, &(r->out.ppRows[0]->aRow[i]), 
					    ppOutMIds->aulPropTag[i], fEphID, r->in.pPropTags);
		if (retval) goto failure;
	}

	r->out.result = MAPI_E_SUCCESS;

	return MAPI_E_SUCCESS;
}


/**
   \details exchange_nsp NspiResortRestriction (0x6) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiResortRestriction request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_NspiResortRestriction(struct dcesrv_call_state *dce_call,
						    TALLOC_CTX *mem_ctx,
						    struct NspiResortRestriction *r)
{
	DEBUG(3, ("exchange_nsp: NspiResortRestriction (0x6) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
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
static enum MAPISTATUS dcesrv_NspiDNToMId(struct dcesrv_call_state *dce_call,
					  TALLOC_CTX *mem_ctx,
					  struct NspiDNToMId *r)
{
	enum MAPISTATUS		retval;
	struct dcesrv_handle	*h;
	struct emsabp_context	*emsabp_ctx;
	struct ldb_message	*msg;
	uint32_t		i;
	uint32_t		MId;
	const char		*dn;
	bool			pbUseConfPartition;

	DEBUG(3, ("exchange_nsp: NspiDNToMId (0x7)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!NTLM_AUTH_IS_OK(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		return MAPI_E_LOGON_FAILED;
	}

	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	if (!h) return MAPI_E_LOGON_FAILED;
	OPENCHANGE_RETVAL_IF(!h, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);

	emsabp_ctx = (struct emsabp_context *) h->data;
	
	r->out.ppMIds = talloc_array(mem_ctx, struct SPropTagArray *, 2);
	r->out.ppMIds[0] = talloc_zero(mem_ctx, struct SPropTagArray);
	r->out.ppMIds[0]->cValues = r->in.pNames->Count;
	r->out.ppMIds[0]->aulPropTag = (enum MAPITAGS *) talloc_array(mem_ctx, uint32_t, r->in.pNames->Count);

	for (i = 0; i < r->in.pNames->Count; i++) {
		/* Step 1. Check if the input legacyDN exists */
	  retval = emsabp_search_legacyExchangeDN(emsabp_ctx, r->in.pNames->Strings[i], &msg, &pbUseConfPartition);
		if (retval != MAPI_E_SUCCESS) {
			r->out.ppMIds[0]->aulPropTag[i] = 0;
		} else {
			TDB_CONTEXT *tdb_ctx = (pbUseConfPartition ? emsabp_ctx->tdb_ctx : emsabp_ctx->ttdb_ctx);
			dn = ldb_msg_find_attr_as_string(msg, "distinguishedName", NULL);
			retval = emsabp_tdb_fetch_MId(tdb_ctx, dn, &MId);
			if (retval) {
				retval = emsabp_tdb_insert(tdb_ctx, dn);
				retval = emsabp_tdb_fetch_MId(tdb_ctx, dn, &MId);
			}
			r->out.ppMIds[0]->aulPropTag[i] = MId;
		}
	}

	r->out.result = MAPI_E_SUCCESS;

	return MAPI_E_SUCCESS;
}


/**
   \details exchange_nsp NspiGetPropList (0x8) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiGetPropList request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_NspiGetPropList(struct dcesrv_call_state *dce_call,
					      TALLOC_CTX *mem_ctx,
					      struct NspiGetPropList *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetPropList (0x8) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiGetProps (0x9) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiGetProps request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_NspiGetProps(struct dcesrv_call_state *dce_call,
					   TALLOC_CTX *mem_ctx,
					   struct NspiGetProps *r)
{
	enum MAPISTATUS		retval;
	struct dcesrv_handle	*h;
	struct emsabp_context	*emsabp_ctx;
	uint32_t		MId;
	int			i;

	DEBUG(3, ("exchange_nsp: NspiGetProps (0x9)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!NTLM_AUTH_IS_OK(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		return MAPI_E_LOGON_FAILED;
	}

	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	if (!h) return MAPI_E_LOGON_FAILED;
	emsabp_ctx = (struct emsabp_context *) h->data;

	MId = r->in.pStat->CurrentRec;
	
	/* Step 1. Sanity Checks (MS-NSPI Server Processing Rules) */
	if (r->in.pStat->ContainerID && (emsabp_tdb_lookup_MId(emsabp_ctx->tdb_ctx, r->in.pStat->ContainerID) == false)) {
		retval = MAPI_E_INVALID_BOOKMARK;
		r->out.result = retval;
		return retval;
	}

	/* Step 2. Fetch properties */
	r->out.ppRows = talloc_array(mem_ctx, struct SRow *, 2);
	r->out.ppRows[0] = talloc_zero(r->out.ppRows, struct SRow);
	r->out.ppRows[0]->ulAdrEntryPad = 0;

	retval = emsabp_fetch_attrs(mem_ctx, emsabp_ctx, r->out.ppRows[0], MId, r->in.dwFlags, r->in.pPropTags);
	if (retval != MAPI_E_SUCCESS) {
		/* Is MId is not found, proceed as if no attributes were found */
		if (retval == MAPI_E_INVALID_BOOKMARK) {
			uint32_t	ulPropTag;
			struct SRow	*aRow;
			
			aRow = r->out.ppRows[0];
			aRow->ulAdrEntryPad = 0x0;
			aRow->cValues = r->in.pPropTags->cValues;
			aRow->lpProps = talloc_array(mem_ctx, struct SPropValue, aRow->cValues);
			for (i = 0; i < aRow->cValues; i++) {
				ulPropTag = r->in.pPropTags->aulPropTag[i];
				ulPropTag = (ulPropTag & 0xFFFF0000) | PT_ERROR;

				aRow->lpProps[i].ulPropTag = ulPropTag;
				aRow->lpProps[i].dwAlignPad = 0x0;
				set_SPropValue(&(aRow->lpProps[i]), NULL);
			}
			retval = MAPI_W_ERRORS_RETURNED;
		} else {
			talloc_free(r->out.ppRows);
			r->out.ppRows = NULL;
		}
		r->out.result = retval;
		return r->out.result;
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

	r->out.result = retval;
	return retval;
}


/**
   \details exchange_nsp NspiCompareMIds (0xA) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiCompareMIds request data

   \return MAPI_E_SUCCESS on success
*/
static enum MAPISTATUS dcesrv_NspiCompareMIds(struct dcesrv_call_state *dce_call,
					      TALLOC_CTX *mem_ctx,
					      struct NspiCompareMIds *r)
{
	DEBUG(3, ("exchange_nsp: NspiCompareMIds (0xA) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiModProps (0xB) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiModProps request data

   \return MAPI_E_SUCCESS on success

 */
static enum MAPISTATUS dcesrv_NspiModProps(struct dcesrv_call_state *dce_call,
					   TALLOC_CTX *mem_ctx,
					   struct NspiModProps *r)
{
	DEBUG(3, ("exchange_nsp: NspiModProps (0xB) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
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
static enum MAPISTATUS dcesrv_NspiGetSpecialTable(struct dcesrv_call_state *dce_call,
						  TALLOC_CTX *mem_ctx,
						  struct NspiGetSpecialTable *r)
{
	struct dcesrv_handle		*h;
	struct emsabp_context		*emsabp_ctx;

	DEBUG(3, ("exchange_nsp: NspiGetSpecialTable (0xC)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!NTLM_AUTH_IS_OK(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		return MAPI_E_LOGON_FAILED;
	}

	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	if (!h) return MAPI_E_LOGON_FAILED;
	emsabp_ctx = (struct emsabp_context *) h->data;

	/* Step 1. (FIXME) We arbitrary set lpVersion to 0x1 */
	r->out.lpVersion = talloc_zero(mem_ctx, uint32_t);
	*r->out.lpVersion = 0x1;

	/* Step 2. Allocate output SRowSet and call associated emsabp function */
	r->out.ppRows = talloc_zero(mem_ctx, struct SRowSet *);
	OPENCHANGE_RETVAL_IF(!r->out.ppRows, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
	r->out.ppRows[0] = talloc_zero(mem_ctx, struct SRowSet);
	OPENCHANGE_RETVAL_IF(!r->out.ppRows[0], MAPI_E_NOT_ENOUGH_RESOURCES, NULL);

	switch (r->in.dwFlags) {
	case NspiAddressCreationTemplates:
	case NspiAddressCreationTemplates|NspiUnicodeStrings:
		DEBUG(0, ("CreationTemplates Table requested\n"));
		r->out.result = emsabp_get_CreationTemplatesTable(mem_ctx, emsabp_ctx, r->in.dwFlags, r->out.ppRows);
		break;
	case NspiUnicodeStrings:
	case 0x0:
		DEBUG(0, ("Hierarchy Table requested\n"));
		r->out.result = emsabp_get_HierarchyTable(mem_ctx, emsabp_ctx, r->in.dwFlags, r->out.ppRows);
		break;
	default:
		talloc_free(r->out.ppRows);
		talloc_free(r->out.ppRows[0]);
		return MAPI_E_NO_SUPPORT;
	}

	return r->out.result;
}


/**
   \details exchange_nsp NspiGetTemplateInfo (0xD) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiGetTemplateInfo request data

   \return MAPI_E_SUCCESS on success

 */
static enum MAPISTATUS dcesrv_NspiGetTemplateInfo(struct dcesrv_call_state *dce_call,
						  TALLOC_CTX *mem_ctx,
						  struct NspiGetTemplateInfo *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetTemplateInfo (0xD) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiModLinkAtt (0xE) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiModLinkAtt request data

   \return MAPI_E_SUCCESS on success

 */
static enum MAPISTATUS dcesrv_NspiModLinkAtt(struct dcesrv_call_state *dce_call,
					     TALLOC_CTX *mem_ctx,
					     struct NspiModLinkAtt *r)
{
	DEBUG(3, ("exchange_nsp: NspiModLinkAtt (0xE) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiDeleteEntries (0xF) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiDeleteEntries request data

   \return MAPI_E_SUCCESS on success

 */
static enum MAPISTATUS dcesrv_NspiDeleteEntries(struct dcesrv_call_state *dce_call,
						TALLOC_CTX *mem_ctx,
						struct NspiDeleteEntries *r)
{
	DEBUG(3, ("exchange_nsp: NspiDeleteEntries (0xF) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiQueryColumns (0x10) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiQueryColumns request data

   \return MAPI_E_SUCCESS on success

 */
static enum MAPISTATUS dcesrv_NspiQueryColumns(struct dcesrv_call_state *dce_call,
					       TALLOC_CTX *mem_ctx,
					       struct NspiQueryColumns *r)
{
	DEBUG(3, ("exchange_nsp: NspiQueryColumns (0x10) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiGetNamesFromIDs (0x11) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiGetNamesFromIDs request data

   \return MAPI_E_SUCCESS on success

 */
static enum MAPISTATUS dcesrv_NspiGetNamesFromIDs(struct dcesrv_call_state *dce_call,
						  TALLOC_CTX *mem_ctx,
						  struct NspiGetNamesFromIDs *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetNamesFromIDs (0x11) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiGetIDsFromNames (0x12) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiGetIDsFromNames request data

   \return MAPI_E_SUCCESS on success

 */
static enum MAPISTATUS dcesrv_NspiGetIDsFromNames(struct dcesrv_call_state *dce_call,
						  TALLOC_CTX *mem_ctx,
						  struct NspiGetIDsFromNames *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetIDsFromNames (0x12) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiResolveNames (0x13) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiResolveNames request data

   \return MAPI_E_SUCCESS on success

 */
static enum MAPISTATUS dcesrv_NspiResolveNames(struct dcesrv_call_state *dce_call,
					       TALLOC_CTX *mem_ctx,
					       struct NspiResolveNames *r)
{
	DEBUG(3, ("exchange_nsp: NspiResolveNames (0x13) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiResolveNamesW (0x14) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiResolveNamesW request data

   \return MAPI_E_SUCCESS on success

 */
static enum MAPISTATUS dcesrv_NspiResolveNamesW(struct dcesrv_call_state *dce_call,
						TALLOC_CTX *mem_ctx,
						struct NspiResolveNamesW *r)
{
	enum MAPISTATUS		retval = MAPI_E_SUCCESS;
	struct dcesrv_handle	*h;
	struct emsabp_context	*emsabp_ctx;
	struct ldb_message	*ldb_msg_ab;
	struct SPropTagArray	*pPropTags;
	const char		*purportedSearch;
	struct SPropTagArray	*pMIds = NULL;
	struct SRowSet		*pRows = NULL;
	struct WStringsArray_r	*paWStr;
	uint32_t		i;
	int			ret;
	const char * const	recipient_attrs[] = { "*", NULL };
	const char * const	search_attr[] = { "mailNickName", "mail", "name", 
						  "displayName", "givenName", "sAMAccountName" };

	DEBUG(3, ("exchange_nsp: NspiResolveNamesW (0x14)\n"));

	/* Step 0. Ensure incoming user is authenticated */
	if (!NTLM_AUTH_IS_OK(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		return MAPI_E_LOGON_FAILED;
	}

	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	OPENCHANGE_RETVAL_IF(!h, MAPI_E_LOGON_FAILED, NULL);
	emsabp_ctx = (struct emsabp_context *) h->data;

	/* Step 1. Prepare in/out data */
	retval = emsabp_ab_container_by_id(mem_ctx, emsabp_ctx, r->in.pStat->ContainerID, &ldb_msg_ab);
	OPENCHANGE_RETVAL_IF(!MAPI_STATUS_IS_OK(retval), MAPI_E_INVALID_BOOKMARK, NULL);

	purportedSearch = ldb_msg_find_attr_as_string(ldb_msg_ab, "purportedSearch", NULL);
	OPENCHANGE_RETVAL_IF(!purportedSearch, MAPI_E_INVALID_BOOKMARK, NULL);

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
	pMIds = talloc(mem_ctx, struct SPropTagArray);
	pMIds->cValues = paWStr->Count;
	pMIds->aulPropTag = (enum MAPITAGS *) talloc_array(mem_ctx, uint32_t, pMIds->cValues);
	pRows = talloc(mem_ctx, struct SRowSet);
	pRows->cRows = 0;
	pRows->aRow = talloc_array(mem_ctx, struct SRow, pMIds->cValues);

	/* Step 2. Fetch AB container records */
	for (i = 0; i < paWStr->Count; i++) {
		struct ldb_result	*ldb_res;
		char			*filter = talloc_strdup(mem_ctx, "");
		int			j;

		/* Build search filter */
		for (j = 0; j < ARRAY_SIZE(search_attr); j++) {
			char	*attr_filter = talloc_asprintf(mem_ctx, "(%s=%s)", search_attr[j], paWStr->Strings[i]);
			filter = talloc_strdup_append(filter, attr_filter);
			talloc_free(attr_filter);
		}

		/* Search AD */
		filter = talloc_asprintf(mem_ctx, "(&%s(|%s))", purportedSearch, filter);
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
			emsabp_fetch_attrs_from_msg(mem_ctx, emsabp_ctx, &pRows->aRow[pRows->cRows],
						    ldb_res->msgs[0], 0, 0, pPropTags);
			pRows->cRows++;
		}
	}

	*r->out.ppMIds = pMIds;
	if (pRows->cRows) {
		*r->out.ppRows = pRows;
	}

	r->out.result = retval;
	return retval;
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
	enum MAPISTATUS				retval;
	const struct ndr_interface_table	*table;
	uint16_t				opnum;

	table = (const struct ndr_interface_table *) dce_call->context->iface->private_data;
	opnum = dce_call->pkt.u.request.opnum;

	/* Sanity checks */
	if (!table) return NT_STATUS_UNSUCCESSFUL;
	if (table->name && strcmp(table->name, NDR_EXCHANGE_NSP_NAME)) return NT_STATUS_UNSUCCESSFUL;

	switch (opnum) {
	case NDR_NSPIBIND:
		retval = dcesrv_NspiBind(dce_call, mem_ctx, (struct NspiBind *)r);
		break;
	case NDR_NSPIUNBIND:
		retval = dcesrv_NspiUnbind(dce_call, mem_ctx, (struct NspiUnbind *)r);
		break;
	case NDR_NSPIUPDATESTAT:
		retval = dcesrv_NspiUpdateStat(dce_call, mem_ctx, (struct NspiUpdateStat *)r);
		break;
	case NDR_NSPIQUERYROWS:
		retval = dcesrv_NspiQueryRows(dce_call, mem_ctx, (struct NspiQueryRows *)r);
		break;
	case NDR_NSPISEEKENTRIES:
		retval = dcesrv_NspiSeekEntries(dce_call, mem_ctx, (struct NspiSeekEntries *)r);
		break;
	case NDR_NSPIGETMATCHES:
		retval = dcesrv_NspiGetMatches(dce_call, mem_ctx, (struct NspiGetMatches *)r);
		break;
	case NDR_NSPIRESORTRESTRICTION:
		retval = dcesrv_NspiResortRestriction(dce_call, mem_ctx, (struct NspiResortRestriction *)r);
		break;
	case NDR_NSPIDNTOMID:
		retval = dcesrv_NspiDNToMId(dce_call, mem_ctx, (struct NspiDNToMId *)r);
		break;
	case NDR_NSPIGETPROPLIST:
		retval = dcesrv_NspiGetPropList(dce_call, mem_ctx, (struct NspiGetPropList *)r);
		break;
	case NDR_NSPIGETPROPS:
		retval = dcesrv_NspiGetProps(dce_call, mem_ctx, (struct NspiGetProps *)r);
		break;
	case NDR_NSPICOMPAREMIDS:
		retval = dcesrv_NspiCompareMIds(dce_call, mem_ctx, (struct NspiCompareMIds *)r);
		break;
	case NDR_NSPIMODPROPS:
		retval = dcesrv_NspiModProps(dce_call, mem_ctx, (struct NspiModProps *)r);
		break;
	case NDR_NSPIGETSPECIALTABLE:
		retval = dcesrv_NspiGetSpecialTable(dce_call, mem_ctx, (struct NspiGetSpecialTable *)r);
		break;
	case NDR_NSPIGETTEMPLATEINFO:
		retval = dcesrv_NspiGetTemplateInfo(dce_call, mem_ctx, (struct NspiGetTemplateInfo *)r);
		break;
	case NDR_NSPIMODLINKATT:
		retval = dcesrv_NspiModLinkAtt(dce_call, mem_ctx, (struct NspiModLinkAtt *)r);
		break;
	case NDR_NSPIDELETEENTRIES:
		retval = dcesrv_NspiDeleteEntries(dce_call, mem_ctx, (struct NspiDeleteEntries *)r);
		break;
	case NDR_NSPIQUERYCOLUMNS:
		retval = dcesrv_NspiQueryColumns(dce_call, mem_ctx, (struct NspiQueryColumns *)r);
		break;
	case NDR_NSPIGETNAMESFROMIDS:
		retval = dcesrv_NspiGetNamesFromIDs(dce_call, mem_ctx, (struct NspiGetNamesFromIDs *)r);
		break;
	case NDR_NSPIGETIDSFROMNAMES:
		retval = dcesrv_NspiGetIDsFromNames(dce_call, mem_ctx, (struct NspiGetIDsFromNames *)r);
		break;
	case NDR_NSPIRESOLVENAMES:
		retval = dcesrv_NspiResolveNames(dce_call, mem_ctx, (struct NspiResolveNames *)r);
		break;
	case NDR_NSPIRESOLVENAMESW:
		retval = dcesrv_NspiResolveNamesW(dce_call, mem_ctx, (struct NspiResolveNamesW *)r);
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
	/* Initialize exchange_nsp session */
	nsp_session = talloc_zero(dce_ctx, struct exchange_nsp_session);
	if (!nsp_session) return NT_STATUS_NO_MEMORY;
	nsp_session->session = NULL;

	/* Open a read-write pointer on the EMSABP TDB database */
	emsabp_tdb_ctx = emsabp_tdb_init((TALLOC_CTX *)dce_ctx, dce_ctx->lp_ctx);
	if (!emsabp_tdb_ctx) {
		smb_panic("unable to initialize EMSABP context");
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
	struct exchange_nsp_session	*session;

	for (session = nsp_session; session; session = session->next) {
		if ((mpm_session_cmp_sub(session->session, server_id, context_id) == true)) {
			mpm_session_release(session->session);
			DLIST_REMOVE(nsp_session, session);
			DEBUG(6, ("[%s:%d]: Session found and released\n", __FUNCTION__, __LINE__));
			return NT_STATUS_OK;
		}
	}

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
