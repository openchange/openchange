/*
   MAPI Proxy - Exchange NSPI Server

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2013
   Copyright (C) Carlos Pérez-Aradros Herce 2015

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
#include "mapiproxy/util/samdb.h"
#include "mapiproxy/libmapiproxy/fault_util.h"
#include "dcesrv_exchange_nsp.h"

static TDB_CONTEXT			*emsabp_tdb_ctx = NULL;


static struct emsabp_context *dcesrv_find_emsabp_context(struct GUID *uuid)
{
	struct mpm_session	*session;
	struct emsabp_context	*emsabp_ctx = NULL;

	session = mpm_session_find_by_uuid(uuid);
	if (session) {
		emsabp_ctx = (struct emsabp_context *)session->private_data;
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
			    TALLOC_CTX *mem_ctx, struct NspiBind *r)
{
	struct GUID			*guid = (struct GUID *) NULL;
	struct emsabp_context		*emsabp_ctx;
	struct dcesrv_handle		*handle;
	struct policy_handle		wire_handle;
	struct mpm_session		*session;
	char				*uuid_str;
	enum MAPISTATUS			retval = MAPI_E_SUCCESS;

	OC_DEBUG(5, "exchange_nsp: NspiBind (0x0)\n");

	/* Step 0. Ensure incoming user is authenticated and code page is correct*/
	if (!dcesrv_call_authenticated(dce_call) && (r->in.dwFlags & fAnonymousLogin)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate\n");
		retval = MAPI_E_FAILONEPROVIDER;
		goto failure;
	}

	DCESRV_NSP_RETURN_IF(r->in.pStat->CodePage == CP_UNICODE, r, MAPI_E_NO_SUPPORT, NULL);

	/* Step 1. Initialize the emsabp context */
	emsabp_ctx = emsabp_init(dce_call->conn->dce_ctx->lp_ctx, emsabp_tdb_ctx);
	if (!emsabp_ctx) {
		OC_PANIC(false, ("[exchange_nsp] Unable to initialize emsabp context\n"));
		retval = MAPI_E_FAILONEPROVIDER;
		goto failure;
	}

	if (lpcfg_parm_bool(dce_call->conn->dce_ctx->lp_ctx, NULL, "exchange_nsp", "debug", false)) {
		emsabp_enable_debug(emsabp_ctx);
	}

	/* Step 2. Check if incoming user belongs to the Exchange organization */
	if ((emsabp_verify_user(dce_call, emsabp_ctx) == false) && (r->in.dwFlags & fAnonymousLogin)) {
		retval = MAPI_E_LOGON_FAILED;
		goto failure;
	}

	/* Step 3. Check if valid cpID has been supplied */
	if (emsabp_verify_codepage(emsabp_ctx, r->in.pStat->CodePage) == false) {
		retval = MAPI_E_UNKNOWN_CPID;
		goto failure;
	}

	/* Step 4. Retrieve OpenChange server GUID */
	guid = (struct GUID *) samdb_ntds_objectGUID(emsabp_ctx->samdb_ctx);
	DCESRV_NSP_RETURN_IF(!guid, r, MAPI_E_FAILONEPROVIDER, emsabp_ctx);

	/* Step 5. Fill NspiBind reply */
	handle = dcesrv_handle_new(dce_call->context, EXCHANGE_HANDLE_NSP);
	DCESRV_NSP_RETURN_IF(!handle, r, MAPI_E_NOT_ENOUGH_RESOURCES, emsabp_ctx);

	handle->data = (void *) emsabp_ctx;
	*r->out.handle = handle->wire_handle;
	r->out.mapiuid = guid;

	/* Search for an existing session, create if it doesn't exist */
	session = mpm_session_find_by_uuid(&handle->wire_handle.uuid);

	uuid_str = GUID_string(mem_ctx, &handle->wire_handle.uuid);
	DCESRV_NSP_RETURN_IF(!uuid_str, r, MAPI_E_NOT_ENOUGH_RESOURCES, emsabp_ctx);

	if (session) {
		OC_DEBUG(5, "[exchange_nsp]: Reusing existing nsp_session: %s", uuid_str);
	} else {
		OC_DEBUG(5, "[exchange_nsp]: Creating new session");

		/* Step 6. Associate this emsabp context to the session */
		session = mpm_session_init(dce_call, &handle->wire_handle.uuid);
		DCESRV_NSP_RETURN_IF(!session, r, MAPI_E_NOT_ENOUGH_RESOURCES, emsabp_ctx);

		mpm_session_set_private_data(session, (void *) emsabp_ctx);
		mpm_session_set_destructor(session, emsabp_destructor);

		OC_DEBUG(5, "[exchange_nsp]: New session added: %s", uuid_str);
	}
	talloc_free(uuid_str);

	DCESRV_NSP_RETURN(r, MAPI_E_SUCCESS, NULL);

failure:
	wire_handle.handle_type = EXCHANGE_HANDLE_NSP;
	wire_handle.uuid = GUID_zero();
	*r->out.handle = wire_handle;

	r->out.mapiuid = r->in.mapiuid;
	DCESRV_NSP_RETURN(r, retval, emsabp_ctx);
}


/**
   \details exchange_nsp NspiUnbind (0x1) function, Terminates a NSPI
   session with the client

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiUnbind call structure
 */
static void dcesrv_NspiUnbind(struct dcesrv_call_state *dce_call,
			      TALLOC_CTX *mem_ctx, struct NspiUnbind *r)
{
	struct dcesrv_handle		*h;
	struct mpm_session		*session;
	char				*uuid_str;

	OC_DEBUG(5, "exchange_nsp: NspiUnbind (0x1)\n");

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate\n");
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	/* Step 1. Retrieve handle and free if nsp context and session are available */
	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	if (h) {
		session = mpm_session_find_by_uuid(&r->in.handle->uuid);

		uuid_str = GUID_string(mem_ctx, &r->in.handle->uuid);
		DCESRV_NSP_RETURN_IF(!uuid_str, r, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
		if (session) {
			mpm_session_release(session);
			OC_DEBUG(5, "[exchange_nsp]: Session found and released: %s", uuid_str);
		} else {
			OC_DEBUG(0, "[exchange_nsp]: session NOT found: %s", uuid_str);
		}
		talloc_free(uuid_str);
	}

	r->out.handle->uuid = GUID_zero();
	r->out.handle->handle_type = 0;
	r->out.result = (enum MAPISTATUS) 1;

	DCESRV_NSP_RETURN(r, 1, NULL);
}

/**
   \details Get the current position and the last row position as defined by the pStat argument.
   In the case of an empty table it return 0 for both positions. The caller can discriminate between empty tables and one-row tables looking at the cValues of the PropertyTagArray_r


   \param pStat pointer to struct STAT which will be used to get the positions
   \param mids pointer to struct PropertyTagArray_r which contains the table as MIDs array
   \param[out] out_row pointer to the uint32_t which will cotaint the current row
   \param[out] out_last_row pointer to the uint32_t which will cotaint the last row in table
 */
static void position_in_table(struct STAT *pStat,
			      struct PropertyTagArray_r *mids,
			      uint32_t *out_row, uint32_t *out_last_row)
{
	bool		found;
	uint32_t	row;
	uint32_t	last_row;

	if (mids->cValues > 0) {
		last_row = mids->cValues - 1;
	} else {
		last_row = 0;
	}

	if (pStat->CurrentRec == MID_CURRENT) {
		/* Fractional positioning (3.1.4.5.2) */
		row = pStat->NumPos * (last_row+1) / pStat->TotalRecs;
		if (row > last_row) {
			row = last_row;
		}
	} else {
		/* Absolute positioning (3.1.4.5.1) */
		if (pStat->CurrentRec == MID_BEGINNING_OF_TABLE) {
			row = 0;
		}
		else if (pStat->CurrentRec == MID_END_OF_TABLE) {
			row = last_row;
		} else {
			found = false;
			row = 0;
			while (row <= last_row) {
				if (mids->aulPropTag[row] == pStat->CurrentRec) {
					found = true;
					break;
				} else {
					row++;
				}
			}
			if (!found) {
				/* In this case the position is undefined. To avoid problems we will use first row */
				row = 0;
			}
		}
	}

	*out_row = row;
	*out_last_row = last_row;
}

/**
   \details This method does the main work of NspiUpdateStat. It is separated from dcesrv_NspiUpdateStat to be called from NspiQueryRows to update correctly the pStat struct. Also is called from NspiUpdateStat to avoid repeating code.

   \param[in,out] r pointer to the NspiUpdateStat request data
   \param mids pointer to struct PropertyTagArray_r which contains the table as MIDs array
*/
static void dcesrv_do_NspiUpdateStat(struct NspiUpdateStat *r,
				     struct PropertyTagArray_r *mids)
{
	enum MAPISTATUS			retval = MAPI_E_SUCCESS;
	uint32_t			row, last_row;

	position_in_table(r->in.pStat, mids, &row, &last_row);

	if (r->in.pStat->Delta != 0) {
		/* Adjust row  by Delta */
		if (r->in.pStat->Delta > 0) {
			row += r->in.pStat->Delta;
			if (row > last_row) {
				row = last_row;
			}
		} else {
			if (abs(r->in.pStat->Delta) >= row) {
				row = 0;
			} else {
				row -= abs(r->in.pStat->Delta);
			}
		}
	}

	if (row == last_row) {
		/* even if row = 0 we should return MID_END_OF_TABLE */
		r->out.pStat->CurrentRec = MID_END_OF_TABLE;
	} else if (row == 0) {
		r->out.pStat->CurrentRec = MID_BEGINNING_OF_TABLE;
	} else {
		r->out.pStat->CurrentRec = mids->aulPropTag[row];
	}

	r->out.pStat->Delta = 0;
	r->out.pStat->NumPos = row;
	r->out.pStat->TotalRecs = mids->cValues;
	r->out.plDelta = r->in.plDelta;
	if (r->in.plDelta != NULL) {
		*(r->out.plDelta) = r->out.pStat->NumPos - r->in.pStat->NumPos;
	}

	DCESRV_NSP_RETURN(r, retval, NULL);
}

/**
   \details exchange_nsp NspiUpdateStat (0x2) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param[in,out] r pointer to the NspiUpdateStat request data

   \return MAPI_E_SUCCESS on success
*/
/* TODO:
 - Use SortType. sortLocale and ContainerId. (This should be done in emsabp_seach. There is already partial SortType support)
 */
static void dcesrv_NspiUpdateStat(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, struct NspiUpdateStat *r)
{
	struct emsabp_context		*emsabp_ctx = NULL;
	bool				container_exists;
	struct PropertyTagArray_r	*mids;
	enum MAPISTATUS			retval = MAPI_E_SUCCESS;

	OC_DEBUG(3, "exchange_nsp: NspiUpdateStat (0x2)");
	/* Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate\n");
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	/* Step 0. Sanity checks from Server Processing Rules */
	DCESRV_NSP_RETURN_IF(r->in.pStat->CodePage == CP_UNICODE, r, MAPI_E_NO_SUPPORT, NULL);

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	DCESRV_NSP_RETURN_IF(!emsabp_ctx, r, MAPI_E_CALL_FAILED, NULL);

	if (r->in.pStat->ContainerID) {
		container_exists = emsabp_tdb_lookup_MId(emsabp_ctx->tdb_ctx, r->in.pStat->ContainerID);
		DCESRV_NSP_RETURN_IF(!container_exists, r, MAPI_E_INVALID_BOOKMARK, NULL);
	}

	/* Step 1. Search in the directory */
	mids = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	DCESRV_NSP_RETURN_IF(!mids, r, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
	retval = emsabp_search(mem_ctx, emsabp_ctx, mids, NULL, r->in.pStat, 0);
	DCESRV_NSP_RETURN_IF(retval != MAPI_E_SUCCESS, r, retval, NULL);

	/* Step 2. Do the update stat with the result */
	dcesrv_do_NspiUpdateStat(r, mids);
}

/**
   \details This method does the main work of NspiQueryRows.
   It is separated from dcesrv_NspiQueryRows to be called from NspiSeekEntries to get the rows. Aslo is called from NspiQueryRows itself to avoid duplication.

   \param mem_ctx pointer to the memory context
   \param[in,out] r pointer to the NspiQueryRows request data
   \param emsabp_ctx pointer to the emsabp context
   \param mids pointer to struct PropertyTagArray_r which contains the table as MIDs array
   \param updateStat update the out stat struct inside the NspiQueryRows request data. The update is not necessary when called from NspiSeekEntries
*/
static void dcesrv_do_NspiQueryRows(TALLOC_CTX *mem_ctx, struct NspiQueryRows *r,
				    struct emsabp_context *emsabp_ctx,  struct PropertyTagArray_r *mids,
				    bool updateStat)
{
	enum MAPISTATUS			retval = MAPI_E_SUCCESS;
	struct SPropTagArray		*pPropTags;
	struct PropertyRowSet_r		*pRows;
	uint32_t			count = 0;
	uint32_t			i, j;
	struct NspiUpdateStat		r_UpdateStat;
	bool				container_exists;

	/* Step 1. Sanity Checks (MS-NSPI Server Processing Rules) */
	if (r->in.lpETable == NULL && r->in.pStat->ContainerID) {
		container_exists = emsabp_tdb_lookup_MId(emsabp_ctx->tdb_ctx, r->in.pStat->ContainerID);
		if (!container_exists) {
			retval = MAPI_E_INVALID_BOOKMARK;
			goto failure;
		}
	}

	if (r->in.lpETable == NULL && r->in.Count == 0) {
		retval = MAPI_E_INVALID_PARAMETER;
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
	if (pRows == NULL) {
		retval = MAPI_E_NOT_ENOUGH_MEMORY;
		goto failure;
	}

	/* Step 2. Fill ppRows  */
	if (r->in.lpETable == NULL) {
		/* Step 2.1 Fill ppRows for supplied Container ID */
		struct ldb_result	*ldb_res;
		uint32_t		start_pos;
		uint32_t		last_row;

		position_in_table(r->in.pStat, mids, &start_pos, &last_row);
		retval = emsabp_ab_container_enum(mem_ctx, emsabp_ctx,
						  r->in.pStat->ContainerID, &ldb_res);
		if (retval != MAPI_E_SUCCESS)  {
			goto failure;
		}
		if (ldb_res == NULL) {
			/* No elements in this container */
			*r->out.ppRows = pRows;
			DCESRV_NSP_RETURN(r, MAPI_E_SUCCESS, NULL);
		}

		if (r->in.pStat->Delta >= 0) {
			start_pos = start_pos + r->in.pStat->Delta;
			if (start_pos >= ldb_res->count) {
				start_pos = ldb_res->count;
			}
		} else {
			if (abs(r->in.pStat->Delta) > r->in.pStat->NumPos) {
				start_pos = 0;
			} else {
				start_pos = start_pos - abs(r->in.pStat->Delta);
			}
		}

		count = ldb_res->count - start_pos;
		if (r->in.Count < count) {
			count = r->in.Count;
		}

		if (count) {
			pRows->cRows = count;
			pRows->aRow = talloc_array(mem_ctx, struct PropertyRow_r, count);
			if (pRows->aRow == NULL) {
				retval = MAPI_E_NOT_ENOUGH_MEMORY;
				goto failure;
			}

			/* fetch required attributes for every entry found */
			for (i = 0; i < count; i++) {
				retval = emsabp_fetch_attrs_from_msg(mem_ctx, emsabp_ctx, pRows->aRow + i,
								     ldb_res->msgs[start_pos+i], 0, r->in.dwFlags, pPropTags);
				if (retval != MAPI_E_SUCCESS) {
					goto failure;
				}
			}
		} else {
			*r->out.ppRows = NULL;
		}

		if (updateStat) {
			/* Prepare call to  NspiUpdateStat */
			r_UpdateStat.in.handle = r->in.handle;
			r_UpdateStat.in.Reserved = 0;
			r_UpdateStat.in.pStat = r->in.pStat;
			r_UpdateStat.in.pStat->Delta += pRows->cRows;
			r_UpdateStat.in.plDelta = NULL;
			r_UpdateStat.in.pStat->TotalRecs = ldb_res->count;
			r_UpdateStat.out.pStat = r->out.pStat;
			dcesrv_do_NspiUpdateStat(&r_UpdateStat, mids);
			if (r_UpdateStat.out.result != MAPI_E_SUCCESS) {
				/* Not clear in the spec what to do if updateStat fails, ignoring it and logging error for the moment */
				OC_DEBUG(1, "NSPI UpdateStat after GetRows failed: %u\n", r_UpdateStat.out.result);
				r->out.result = MAPI_E_SUCCESS;
			}
		}

		r->out.result = MAPI_E_SUCCESS;
	} else {
		/* Step 2.2 Fill ppRows for supplied table of MIds */
		j = 0;
		if (r->in.pStat->NumPos < r->in.dwETableCount) {
			pRows->cRows = r->in.dwETableCount - r->in.pStat->NumPos;
			pRows->aRow = talloc_array(mem_ctx, struct PropertyRow_r, pRows->cRows);
			if (pRows->aRow == NULL) {
				retval = MAPI_E_NOT_ENOUGH_MEMORY;
				goto failure;
			}

			for (i = r->in.pStat->NumPos; i < r->in.dwETableCount; i++) {
				retval = emsabp_fetch_attrs(mem_ctx, emsabp_ctx, &(pRows->aRow[j]), r->in.lpETable[i], r->in.dwFlags, pPropTags);
				if (retval != MAPI_E_SUCCESS) {
					dcesrv_make_ptyp_error_property_row(mem_ctx, pPropTags, &(pRows->aRow[j]));
				}
				j++;
			}
		}
		r->out.pStat->CurrentRec = MID_END_OF_TABLE;
		r->out.pStat->TotalRecs = j;
		r->out.pStat->Delta = 0;
	}

	/* Step 3. Fill output params */
	*r->out.ppRows = pRows;

	DCESRV_NSP_RETURN(r, MAPI_E_SUCCESS, NULL);

failure:
	memcpy(r->out.pStat, r->in.pStat, sizeof (struct STAT));
	*r->out.ppRows = NULL;
	DCESRV_NSP_RETURN(r, retval, NULL);
}


/**
   \details exchange_nsp NspiQueryRows (0x3) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param[in,out] r pointer to the NspiQueryRows request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_NspiQueryRows(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				 struct NspiQueryRows *r)
{
	enum MAPISTATUS			retval = MAPI_E_SUCCESS;
	struct emsabp_context		*emsabp_ctx = NULL;
	struct PropertyTagArray_r	*mids;

	OC_DEBUG(3, "exchange_nsp: NspiQueryRows (0x3)\n");

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate\n");
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	/* Step 1. Sanity checks from Server Processing Rules */
	DCESRV_NSP_RETURN_IF(r->in.pStat->CodePage == CP_UNICODE, r, MAPI_E_NO_SUPPORT, NULL);

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	DCESRV_NSP_RETURN_IF(!emsabp_ctx, r, MAPI_E_CALL_FAILED, NULL);

	DCESRV_NSP_RETURN_IF(r->in.lpETable == NULL && r->in.Count == 0,
			     r, MAPI_E_INVALID_PARAMETER, NULL);

	if (r->in.lpETable == NULL && r->in.pStat->ContainerID) {
		DCESRV_NSP_RETURN_IF(
			!emsabp_tdb_lookup_MId(emsabp_ctx->tdb_ctx, r->in.pStat->ContainerID),
			r, MAPI_E_INVALID_BOOKMARK, NULL);
	}

	mids = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	DCESRV_NSP_RETURN_IF(!mids, r, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	/* Step 2. Perform the search */
	retval = emsabp_search(mem_ctx, emsabp_ctx, mids, NULL, r->in.pStat, 0);
	DCESRV_NSP_RETURN_IF(retval != MAPI_E_SUCCESS, r, retval, NULL);

	/* Step 3. Now we have passed session verifications and have
	   the data we can do the operation */
	dcesrv_do_NspiQueryRows(mem_ctx, r, emsabp_ctx, mids, true);
}


/**
   \details exchange_nsp NspiSeekEntries (0x4) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param[in,out] r pointer to the NspiSeekEntries request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_NspiSeekEntries(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				   struct NspiSeekEntries *r)
{
	enum MAPISTATUS			retval = MAPI_E_SUCCESS;
	struct emsabp_context		*emsabp_ctx = NULL;
	uint32_t			row;
	struct PropertyTagArray_r	*mids, *all_mids;
	struct Restriction_r		*seek_restriction;
	bool				container_exists;
	struct NspiQueryRows		r_QueryRows;
	struct STAT			r_QueryRowsStatOut;

	OC_DEBUG(3, "exchange_nsp: NspiSeekEntries (0x4)\n");

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	DCESRV_NSP_RETURN_IF(!emsabp_ctx, r, MAPI_E_CALL_FAILED, NULL);

	/* Step 1. Sanity Checks (MS-NSPI Server Processing Rules) */
	DCESRV_NSP_RETURN_IF(r->in.pStat->CodePage == CP_UNICODE, r, MAPI_E_NO_SUPPORT, NULL);

	DCESRV_NSP_RETURN_IF(r->in.Reserved != 0, r, MAPI_E_INVALID_PARAMETER, NULL);

	if (r->in.pStat->ContainerID) {
		container_exists = emsabp_tdb_lookup_MId(emsabp_ctx->tdb_ctx, r->in.pStat->ContainerID);
		if (!container_exists) {
			retval = MAPI_E_INVALID_BOOKMARK;
			goto failure;
		}
	}

	if (!r->in.pTarget) {
		retval = MAPI_E_INVALID_PARAMETER;
		goto failure;
	}

	/* SortTypePhoneticDisplayName is not supported, then apply
	   [MS-OXNSPI] Section 3.1.4.1.9 Rule 9 */
	if (r->in.pStat->SortType == SortTypePhoneticDisplayName) {
		retval = MAPI_E_CALL_FAILED;
		goto failure;
	}

	if ((r->in.pStat->SortType != SortTypePhoneticDisplayName)
	    && (r->in.pStat->SortType != SortTypeDisplayName)) {
		retval = MAPI_E_CALL_FAILED;
		goto failure;
	}

	if (r->in.lpETable) {
		all_mids = r->in.lpETable;
	} else {
		all_mids = talloc_zero(mem_ctx, struct PropertyTagArray_r);
		if (all_mids == NULL) {
			retval = MAPI_E_NOT_ENOUGH_MEMORY;
			goto failure;
		}

		retval = emsabp_search(mem_ctx, emsabp_ctx, all_mids, NULL, r->in.pStat, 0);
		if (retval != MAPI_E_SUCCESS) {
			goto failure;
		}
	}

	/* find the records matching the qualifier */
	seek_restriction = talloc_zero(mem_ctx, struct Restriction_r);
	if (seek_restriction == NULL) {
		retval = MAPI_E_NOT_ENOUGH_MEMORY;
		goto failure;
	}
	seek_restriction->rt = RES_PROPERTY;
	seek_restriction->res.resProperty.relop = RELOP_GE;
	seek_restriction->res.resProperty.ulPropTag = r->in.pTarget->ulPropTag;
	seek_restriction->res.resProperty.lpProp = r->in.pTarget;

	mids = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	if (mids == NULL) {
		retval = MAPI_E_NOT_ENOUGH_MEMORY;
		goto failure;
	}
	if (emsabp_search(mem_ctx, emsabp_ctx, mids, seek_restriction, r->in.pStat, 0) != MAPI_E_SUCCESS) {
		mids = all_mids;
		retval = MAPI_E_NOT_FOUND;
	}

	r->out.pStat->CurrentRec = MID_END_OF_TABLE;
	r->out.pStat->NumPos = all_mids->cValues - 1;
	r->out.pStat->TotalRecs = all_mids->cValues;
	for (row = 0; row < all_mids->cValues; row++) {
		if (all_mids->aulPropTag[row] == mids->aulPropTag[0]) {
			r->out.pStat->CurrentRec = mids->aulPropTag[0];
			r->out.pStat->NumPos = row;
			break;
		}
	}

	/* now we need to populate the rows, if properties were requested */
	if (!r->in.pPropTags || !r->in.pPropTags->cValues) {
		*r->out.pRows = NULL;
		goto end;
	}

	r->out.pStat->SortType = r->in.pStat->SortType;
	r->out.pStat->ContainerID = r->in.pStat->ContainerID;
	r->out.pStat->CodePage = r->in.pStat->CodePage;
	r->out.pStat->TemplateLocale = r->in.pStat->TemplateLocale;
	r->out.pStat->SortLocale = r->in.pStat->SortLocale;
	r->out.pStat->Delta = 0;

	r_QueryRows.in.pStat = r->out.pStat; /* assummed it will be unchanged */
	r_QueryRows.out.pStat = &r_QueryRowsStatOut; /* this return values is to be ignored */
	r_QueryRows.in.handle = r->in.handle;
	r_QueryRows.in.dwFlags = fEphID;

	/* MS_OXNSPI 3.1.4.1.9 Server Processing Rules 15 */
	if (r->in.lpETable) {
		/* Use r->in.lpEtable to construct a explicit table to feed QueryRows */
		uint32_t i;
		uint32_t *row_table;
		uint32_t row_table_size;

		row_table_size = r->in.lpETable->cValues - r->out.pStat->NumPos;
		row_table = talloc_array(mem_ctx, uint32_t, row_table_size);
		if (row_table == NULL) {
			retval = MAPI_E_NOT_ENOUGH_MEMORY;
			goto failure;
		}
		for (i = r->out.pStat->NumPos; i < row_table_size; i++) {
			row_table[i] = r->in.lpETable->aulPropTag[i];
		}
		r_QueryRows.in.dwETableCount = row_table_size;
		r_QueryRows.in.lpETable = row_table;
	} else {
		/* Use table from pStat in QueryRows */
		r_QueryRows.in.dwETableCount = 0;
		r_QueryRows.in.lpETable = NULL;
		r_QueryRows.in.Count = 20; /* server can choose any value > 0 */
	}
	r_QueryRows.in.pPropTags = r->in.pPropTags;
	/* The returned rows from QueryRows are used as returned value */
	r_QueryRows.out.ppRows = r->out.pRows;

	dcesrv_do_NspiQueryRows(mem_ctx, &r_QueryRows, emsabp_ctx, all_mids, false);
	if (r_QueryRows.out.result != MAPI_E_SUCCESS) {
		retval = r_QueryRows.out.result;
		goto failure;
	}
end:
	DCESRV_NSP_RETURN(r, retval, NULL);
failure:
	memcpy(r->out.pStat, r->in.pStat, sizeof(struct STAT));
	*r->out.pRows = NULL;

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

	OC_DEBUG(3, "exchange_nsp: NspiGetMatches (0x5)\n");

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate\n");
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	/* Step 1. Sanity checks from Server Processing Rules */
	DCESRV_NSP_RETURN_IF(r->in.pStat->CodePage == CP_UNICODE, r, MAPI_E_NO_SUPPORT, NULL);

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	DCESRV_NSP_RETURN_IF(!emsabp_ctx, r, MAPI_E_CALL_FAILED, NULL);

	DCESRV_NSP_RETURN_IF(r->in.Filter
			     && (r->in.pStat->SortType != SortTypePhoneticDisplayName)
			     && (r->in.pStat->SortType != SortTypeDisplayName),
			     r, MAPI_E_CALL_FAILED, NULL);

	DCESRV_NSP_RETURN_IF(r->in.Reserved1 != 0, r, MAPI_E_INVALID_PARAMETER, NULL);

	/* SortTypePhoneticDisplayName is not supported, then apply
	   [MS-OXNSPI] Section 3.1.4.1.10 Rule 6 */
	if (r->in.pStat->SortType == SortTypePhoneticDisplayName) {
		r->in.pStat->SortType = SortTypeDisplayName;
	}

	if (r->in.pStat->SortType == SortTypeDisplayName && r->in.pStat->ContainerID) {
		DCESRV_NSP_RETURN_IF(emsabp_tdb_lookup_MId(emsabp_ctx->tdb_ctx, r->in.pStat->ContainerID) == false,
				     r, MAPI_E_INVALID_BOOKMARK, NULL);
	}

	/* Step 2. Retrieve MIds array given search criterias */
	ppOutMIds = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	ppOutMIds->cValues = 0;
	ppOutMIds->aulPropTag = NULL;

	retval = emsabp_search(mem_ctx, emsabp_ctx, ppOutMIds, r->in.Filter, r->in.pStat, r->in.ulRequested);
	if (retval != MAPI_E_SUCCESS) {
	failure:
		memcpy(r->out.pStat, r->in.pStat, sizeof(struct STAT));
		*r->out.ppOutMIds = ppOutMIds;
		r->out.ppRows = talloc(mem_ctx, struct PropertyRowSet_r *);
		r->out.ppRows[0] = NULL;
		DCESRV_NSP_RETURN(r, retval, NULL);
	}

	*r->out.ppOutMIds = ppOutMIds;

	/* Step 3. Retrieve requested properties for these MIds */
	r->out.ppRows = talloc_zero(mem_ctx, struct PropertyRowSet_r *);
	r->out.ppRows[0] = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	r->out.ppRows[0]->cRows = ppOutMIds->cValues;
	r->out.ppRows[0]->aRow = talloc_array(mem_ctx, struct PropertyRow_r, ppOutMIds->cValues);

	for (i = 0; i < ppOutMIds->cValues; i++) {
		retval = emsabp_fetch_attrs(mem_ctx, emsabp_ctx, &(r->out.ppRows[0]->aRow[i]),
					    ppOutMIds->aulPropTag[i], fEphID, r->in.pPropTags);
		if (retval) {
			OC_DEBUG(5, "failure looking up value %d\n", i);
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
	OC_DEBUG(3, "exchange_nsp: NspiResortRestriction (0x6) not implemented\n");
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

	OC_DEBUG(3, "exchange_nsp: NspiDNToMId (0x7)\n");

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate\n");
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	DCESRV_NSP_RETURN_IF(!emsabp_ctx, r, MAPI_E_CALL_FAILED, NULL);

	r->out.ppMIds = talloc_array(mem_ctx, struct PropertyTagArray_r *, 2);
	r->out.ppMIds[0] = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	r->out.ppMIds[0]->cValues = r->in.pNames->Count;
	r->out.ppMIds[0]->aulPropTag = talloc_array(mem_ctx, uint32_t, r->in.pNames->Count);

	for (i = 0; i < r->in.pNames->Count; i++) {
		/* Step 1. Check if the input legacyDN exists */
		retval = emsabp_search_legacyExchangeDN(emsabp_ctx, (const char *) r->in.pNames->Strings[i], &msg, &pbUseConfPartition);
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
	OC_DEBUG(3, "exchange_nsp: NspiGetPropList (0x8) not implemented");
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
	bool				container_exists;

	OC_DEBUG(3, "exchange_nsp: NspiGetProps (0x9)");

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate");
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	DCESRV_NSP_RETURN_IF(!emsabp_ctx, r, MAPI_E_CALL_FAILED, NULL);

	MId = r->in.pStat->CurrentRec;

	/* Step 1. Sanity Checks (MS-NSPI Server Processing Rules) */
	if (r->in.pStat->ContainerID) {
		container_exists = emsabp_tdb_lookup_MId(emsabp_ctx->tdb_ctx, r->in.pStat->ContainerID);
		DCESRV_NSP_RETURN_IF(!container_exists, r, MAPI_E_INVALID_BOOKMARK, NULL);
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
	OC_DEBUG(3, "exchange_nsp: NspiCompareMIds (0xA) not implemented");
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
	OC_DEBUG(3, "exchange_nsp: NspiModProps (0xB) not implemented");
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
	bool				nspi_address_creation_templates;
	bool				nspi_unicode_strings;
	bool				unsupported_codepage;

	OC_DEBUG(3, "exchange_nsp: NspiGetSpecialTable (0xC)");

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate");
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}

	nspi_address_creation_templates = r->in.dwFlags & NspiAddressCreationTemplates;
	nspi_unicode_strings = r->in.dwFlags & NspiUnicodeStrings;
	/* Check in 3.1.4.1.3 Server Processing rules step 1 */
	unsupported_codepage = !nspi_address_creation_templates && !nspi_unicode_strings && r->in.pStat->CodePage == CP_UNICODE;
	DCESRV_NSP_RETURN_IF(unsupported_codepage, r, MAPI_E_NO_SUPPORT, NULL);

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	DCESRV_NSP_RETURN_IF(!emsabp_ctx, r, MAPI_E_CALL_FAILED, NULL);

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

	if (nspi_address_creation_templates) {
		OC_DEBUG(5, "CreationTemplates Table requested");
		r->out.result = emsabp_get_CreationTemplatesTable(mem_ctx, emsabp_ctx, r->in.dwFlags, r->out.ppRows);
	} else {
		OC_DEBUG(5, "Hierarchy Table requested");
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
	OC_DEBUG(3, "exchange_nsp: NspiGetTemplateInfo (0xD) not implemented");
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
	OC_DEBUG(3, "exchange_nsp: NspiModLinkAtt (0xE) not implemented");
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
	OC_DEBUG(3, "exchange_nsp: NspiDeleteEntries (0xF) not implemented");
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
	OC_DEBUG(3, "exchange_nsp: NspiQueryColumns (0x10) not implemented");
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
	OC_DEBUG(3, "exchange_nsp: NspiGetNamesFromIDs (0x11) not implemented");
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
	OC_DEBUG(3, "exchange_nsp: NspiGetIDsFromNames (0x12) not implemented");
	DCESRV_NSP_RETURN(r, DCERPC_FAULT_OP_RNG_ERROR, NULL);
}


/**
   \details common code called by NspiResolveNames and NspiResolveNamesW functions
            NspiResolveNames should before copy the values of its struct NspiResolveNames to a
            NspiResolveNamesW struct since const char* is used to both 8-bits and Unicode strings
            it could be done without problems.

            The ANR implementation in this function does a search of fields values beginning with the search string.
            A possible enhancement would be to trim superfluous spaces and/or split the search string by spaces so the order
            of the elements would not be important.

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the NspiResolveNamesW request data

   \return MAPI_E_SUCCESS on success

 */
static void dcesrv_do_NspiResolveNamesW(struct dcesrv_call_state *dce_call,
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
	/* Attributes for ANR search */
	const char * const		search_attr[] = { "mailNickName", "mail", "name",
							  "displayName", "givenName", "sn",
							  "sAMAccountName",
							  "legacyExchangeDN",
							  "physicalDeliveryOfficeName"};
	/* We do not use proxyAddresses attribute for search because we do not expect the user to type in the protocol.
	   We do not use RDN attribute because it seems samba lack support for it.
	   However in user objects RDN should be the same than name, so this case is covered. */

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate");
		DCESRV_NSP_RETURN(r, MAPI_E_LOGON_FAILED, NULL);
	}
	DCESRV_NSP_RETURN_IF(r->in.pStat->CodePage == CP_UNICODE, r, MAPI_E_NO_SUPPORT, NULL);
	/* MS-OXNPI 3.1.4.1.17 server processing rules 2 says that we should return
	   error if r->in.Reserved != 0, however Outlook 2010 always send non-zero so we skip it. */

	emsabp_ctx = dcesrv_find_emsabp_context(&r->in.handle->uuid);
	if (!emsabp_ctx) {
		retval = MAPI_E_CALL_FAILED;
		goto failure;
	}

	if (r->in.pStat->ContainerID) {
		if (!emsabp_tdb_lookup_MId(emsabp_ctx->tdb_ctx, r->in.pStat->ContainerID)) {
			retval = MAPI_E_INVALID_BOOKMARK;
			goto failure;
		}
	}

	/* Step 1. Prepare in/out data */
	retval = emsabp_ab_fetch_filter(mem_ctx, emsabp_ctx, r->in.pStat->ContainerID, &filter_search);
	if (retval != MAPI_E_SUCCESS) {
		OC_DEBUG(5, "[nspi] ab_fetch_filter failed");
		DCESRV_NSP_RETURN(r, MAPI_E_INVALID_BOOKMARK, NULL);
	} else if (filter_search == NULL) {
		OC_DEBUG(5, "[nspi] ab_fetch_filter called for an empty container");
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
	pMIds->aulPropTag = talloc_zero_array(pMIds, uint32_t, pMIds->cValues);
	DCESRV_NSP_RETURN_IF(!pMIds->aulPropTag, r, MAPI_E_NOT_ENOUGH_MEMORY, pMIds);

	pRows = talloc(mem_ctx, struct PropertyRowSet_r);
	if (!pRows) {
		retval = MAPI_E_NOT_ENOUGH_MEMORY;
		goto failure;
	}
	pRows->cRows = 0;
	pRows->aRow = talloc_zero_array(mem_ctx, struct PropertyRow_r, pMIds->cValues);
	if (!pRows->aRow) {
		retval = MAPI_E_NOT_ENOUGH_MEMORY;
		goto failure;
	}

	/* Step 2. Fetch AB container records */
	for (i = 0; i < paWStr->Count; i++) {
		struct ldb_result	*ldb_res;
		char			*filter;
		int			j;

		filter = talloc_strdup(mem_ctx, "");
		if (!filter) {
			retval = MAPI_E_NOT_ENOUGH_MEMORY;
			goto failure;
		}
		/* Build search filter */
		for (j = 0; j < ARRAY_SIZE(search_attr); j++) {
			char *attr_filter = talloc_asprintf(mem_ctx, "(%s=%s*)", search_attr[j],
							    ldb_binary_encode_string(mem_ctx, paWStr->Strings[i]));
			if (!attr_filter) {
				retval = MAPI_E_NOT_ENOUGH_MEMORY;
				goto failure;
			}
			filter = talloc_strdup_append(filter, attr_filter);
			if (!filter) {
				retval = MAPI_E_NOT_ENOUGH_MEMORY;
				goto failure;
			}
			talloc_free(attr_filter);
		}

		/* Search AD */
		filter = talloc_asprintf(mem_ctx, "(&%s(|%s))", filter_search, filter);
		if (!filter) {
			retval = MAPI_E_NOT_ENOUGH_MEMORY;
			goto failure;
		}

		ret = safe_ldb_search(&emsabp_ctx->samdb_ctx, mem_ctx, &ldb_res,
				      ldb_get_default_basedn(emsabp_ctx->samdb_ctx),
				      LDB_SCOPE_SUBTREE, recipient_attrs, "%s", filter);

		/* Determine name resolution status and fetch object upon success */
		if (ret != LDB_SUCCESS || ldb_res->count == 0) {
			pMIds->aulPropTag[i] = MAPI_UNRESOLVED;
		} else if (ldb_res->count > 1) {
			pMIds->aulPropTag[i] = MAPI_AMBIGUOUS;
		} else {
			pMIds->aulPropTag[i] = MAPI_RESOLVED;
			/* The standard says that we must have a call to NspiQueryRows to fill the rows.
			   However with our actual implementation it would imply extract the dn, use it to
			   get the mid and then call to QueryRows, where the element data would be fetched again.
			   So for efficiency we will build the row here, with the data we already have */
			retval = emsabp_fetch_attrs_from_msg(mem_ctx, emsabp_ctx, &pRows->aRow[pRows->cRows],
							     ldb_res->msgs[0], 0, 0, pPropTags);
			if (retval != MAPI_E_SUCCESS) {
				OC_DEBUG(5, "[nspi] emsabp_fetch_attrs_from_msg failed");
				goto failure;
			}
			pRows->cRows++;
		}
	}

	*r->out.ppMIds = pMIds;
	if (pRows->cRows) {
		*r->out.ppRows = pRows;
	}

	DCESRV_NSP_RETURN(r, retval, NULL);
failure:
	OC_DEBUG(5, "[nspi] unexpected error %d", retval);
	talloc_free(pMIds);
	talloc_free(pRows);
	*r->out.ppMIds = NULL;
	*r->out.ppRows = NULL;
	DCESRV_NSP_RETURN(r, retval, NULL);
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
	OC_DEBUG(3, "exchange_nsp: NspiResolveNames (0x13)");
	struct NspiResolveNamesW reqw;
	struct StringsArrayW_r reqw_strings_array;

	reqw.in.handle = r->in.handle;
	reqw.in.Reserved = r->in.Reserved;
	reqw.in.pStat = r->in.pStat;
	reqw.in.pPropTags = r->in.pPropTags;
	reqw.in.paWStr = &reqw_strings_array;
	reqw.in.paWStr->Count = r->in.paStr->Count;
	reqw.in.paWStr->Strings = (const char **) r->in.paStr->Strings;

	reqw.out.ppMIds = r->out.ppMIds;
	reqw.out.ppRows = r->out.ppRows ;

	dcesrv_do_NspiResolveNamesW(dce_call, mem_ctx, &reqw);
	/* Copy result, the other out fields were pointers so not need of it */
	r->out.result = reqw.out.result;
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
	OC_DEBUG(3, "exchange_nsp: NspiResolveNamesW (0x14)");
	dcesrv_do_NspiResolveNamesW(dce_call, mem_ctx, r);
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

	OC_DEBUG(5, "dcesrv_exchange_nsp_dispatch opnum: %u",  dce_call->pkt.u.request.opnum);

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
	OC_DEBUG(0, "dcesrv_exchange_nsp_init");

	/* Open a read-write pointer on the EMSABP TDB database */
	emsabp_tdb_ctx = emsabp_tdb_init((TALLOC_CTX *)dce_ctx, dce_ctx->lp_ctx);
	if (!emsabp_tdb_ctx) {
		OC_PANIC(false, ("[exchange_nsp] Unable to initialize emsabp_tdb context\n"));
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
	struct server_id_buf tmp;

	OC_DEBUG(5, "dcesrv_exchange_nsp_unbind: server_id=%s, context_id=%u",
		 server_id_str_buf(server_id, &tmp), context_id);

	mpm_session_unbind(&server_id, context_id);
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
		OC_DEBUG(0, "Failed to register the 'exchange_nsp' default mapiproxy server!");
		return ret;
	}

	return ret;
}
