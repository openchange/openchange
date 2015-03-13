/*
   Stand-alone MAPI testsuite

   OpenChange Project - NSPI tests

   Copyright (C) Julien Kerihuel 2008

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

#include "utils/mapitest/mapitest.h"
#include "utils/mapitest/proto.h"

/**
   \file module_nspi.c

   \brief NSPI tests
 */


/**
   \details Test the NspiUpdateStat RPC operation (0x02)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_UpdateStat(struct mapitest *mt)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
	int32_t       		plDelta = 1;
	struct PropertyRowSet_r	*RowSet;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_UpdateStat");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	retval = nspi_GetSpecialTable(nspi_ctx, mem_ctx, 0x2, &RowSet);
	MAPIFreeBuffer(RowSet);
	if (retval != MAPI_E_SUCCESS) {
		talloc_free(mem_ctx);
		return false;
	}

	retval = nspi_UpdateStat(nspi_ctx, mem_ctx, &plDelta);
	mapitest_print_retval(mt, "NspiUpdateStat");
	if (retval != MAPI_E_SUCCESS) {
		talloc_free(mem_ctx);
		return false;
	}
	mapitest_print(mt, "* %-35s: %d\n", "plDelta", plDelta);
	talloc_free(mem_ctx);

	return true;
}


/**
   \details Test the NspiQueryRows RPC operation (0x3)
   
   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_QueryRows(struct mapitest *mt)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct nspi_context		*nspi_ctx;
	struct PropertyTagArray_r	*MIds;
	struct PropertyRowSet_r		*RowSet;
	struct SPropTagArray		*SPropTagArray;
	struct PropertyValue_r		*lpProp;
	struct Restriction_r		Filter;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_QueryRows");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	/* Build the array of columns we want to retrieve */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2, PR_DISPLAY_NAME,
					  PR_DISPLAY_TYPE);

	/* Build the restriction we want for NspiGetMatches */
	lpProp = talloc_zero(mem_ctx, struct PropertyValue_r);
	lpProp->ulPropTag = PR_ACCOUNT;
	lpProp->dwAlignPad = 0;
	lpProp->value.lpszA = mt->mapi_ctx->session->profile->username;

	Filter.rt = RES_PROPERTY;
	Filter.res.resProperty.relop = RES_PROPERTY;
	Filter.res.resProperty.ulPropTag = PR_ACCOUNT;
	Filter.res.resProperty.lpProp = lpProp;

	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	MIds = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	retval = nspi_GetMatches(nspi_ctx, mem_ctx, SPropTagArray, &Filter, 5000, &RowSet, &MIds);
	MAPIFreeBuffer(lpProp);
	MAPIFreeBuffer(RowSet);
	MAPIFreeBuffer(SPropTagArray);
	mapitest_print_retval_clean(mt, "NspiGetMatches", retval);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		talloc_free(mem_ctx);
		return false;
	}

	/* Query the rows */
	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	retval = nspi_QueryRows(nspi_ctx, mem_ctx, NULL, MIds, 1, &RowSet);
	MAPIFreeBuffer(RowSet);
	mapitest_print_retval_clean(mt, "NspiQueryRows", retval);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		talloc_free(mem_ctx);
		return false;
	}

	talloc_free(mem_ctx);

	return true;
}


/**
   \details Test the NspiSeekEntries RPC operation (0x04)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_SeekEntries(struct mapitest *mt)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
	struct PropertyValue_r	pTarget;
	struct SPropTagArray	*pPropTags;
	struct PropertyRowSet_r		*RowSet;
	struct emsmdb_context	*emsmdb;
	bool			ret = true;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_SeekEntries");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	emsmdb = (struct emsmdb_context *) mt->session->emsmdb->ctx;
	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	
	pTarget.ulPropTag = PR_DISPLAY_NAME;
	pTarget.dwAlignPad = 0x0;
	pTarget.value.lpszA = emsmdb->info.szDisplayName;

	pPropTags = set_SPropTagArray(mem_ctx, 0x1, PR_ACCOUNT);
	retval = nspi_SeekEntries(nspi_ctx, mem_ctx, SortTypeDisplayName, &pTarget, pPropTags, NULL, &RowSet);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	mapitest_print_retval_clean(mt, "NspiSeekEntries", retval);
	MAPIFreeBuffer(RowSet);
	MAPIFreeBuffer(pPropTags);

	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);

	pTarget.ulPropTag = PR_DISPLAY_NAME_UNICODE;
	pTarget.dwAlignPad = 0x0;
	pTarget.value.lpszA = emsmdb->info.szDisplayName;

	pPropTags = set_SPropTagArray(mem_ctx, 0x1, PR_ACCOUNT);
	retval = nspi_SeekEntries(nspi_ctx, mem_ctx, SortTypeDisplayName, &pTarget, pPropTags, NULL, &RowSet);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	mapitest_print_retval_clean(mt, "NspiSeekEntries", retval);
	MAPIFreeBuffer(RowSet);
	MAPIFreeBuffer(pPropTags);

	talloc_free(mem_ctx);

	return ret;
}


/**
   \details Test the NspiGetMatches RPC operation (0x5)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_GetMatches(struct mapitest *mt)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct nspi_context		*nspi_ctx;
	struct PropertyTagArray_r	*MIds;
	struct PropertyRowSet_r		*RowSet;
	struct SPropTagArray		*SPropTagArray;
	struct PropertyValue_r		*lpProp;
	struct Restriction_r		Filter;
	bool				ret = true;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_GetMatches");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	/* Build the array of columns we want to retrieve */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2, PR_DISPLAY_NAME,
					  PR_DISPLAY_TYPE);

	/* Build the restriction we want for NspiGetMatches */
	lpProp = talloc_zero(mem_ctx, struct PropertyValue_r);
	lpProp->ulPropTag = PR_ACCOUNT;
	lpProp->dwAlignPad = 0;
	lpProp->value.lpszA = mt->mapi_ctx->session->profile->username;

	Filter.rt = RES_PROPERTY;
	Filter.res.resProperty.relop = RES_PROPERTY;
	Filter.res.resProperty.ulPropTag = PR_ACCOUNT;
	Filter.res.resProperty.lpProp = lpProp;

	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	MIds = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	retval = nspi_GetMatches(nspi_ctx, mem_ctx, SPropTagArray, &Filter, 5000, &RowSet, &MIds);
	MAPIFreeBuffer(lpProp);
	MAPIFreeBuffer(RowSet);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(MIds);
	mapitest_print_retval_clean(mt, "NspiGetMatches", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	talloc_free(mem_ctx);
	return ret;
}


/**
   \details Test the NspiResortRestriction RPC operation (0x6)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_ResortRestriction(struct mapitest *mt)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct nspi_context		*nspi_ctx;
	struct Restriction_r		Filter;
	struct PropertyRowSet_r		*RowSet = NULL;
	struct SPropTagArray		*SPropTagArray = NULL;
	struct PropertyValue_r		*lpProp = NULL;
	struct PropertyTagArray_r	*MIds = NULL;
	struct PropertyTagArray_r	*ppMIds = NULL;
	bool				ret = true;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_ResortRestriction");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	/* Build the array of columns we want to retrieve */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0xb,
					  PR_DISPLAY_NAME,
					  PR_OFFICE_TELEPHONE_NUMBER,
					  PR_OFFICE_LOCATION,
					  PR_TITLE,
					  PR_COMPANY_NAME,
					  PR_ACCOUNT,
					  PR_ADDRTYPE,
					  PR_ENTRYID,
					  PR_DISPLAY_TYPE,
					  PR_INSTANCE_KEY,
					  PR_EMAIL_ADDRESS
					  );

	/* Build the restriction we want for NspiGetMatches */
	lpProp = talloc_zero(mem_ctx, struct PropertyValue_r);
	lpProp->ulPropTag = PR_OBJECT_TYPE;
	lpProp->dwAlignPad = 0;
	lpProp->value.l = 6;

	Filter.rt = RES_PROPERTY;
	Filter.res.resProperty.relop = RES_PROPERTY;
	Filter.res.resProperty.ulPropTag = PR_OBJECT_TYPE;
	Filter.res.resProperty.lpProp = lpProp;

	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	MIds = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	retval = nspi_GetMatches(nspi_ctx, mem_ctx, SPropTagArray, &Filter, 5000, &RowSet, &MIds);
	MAPIFreeBuffer(lpProp);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(RowSet);
	mapitest_print_retval_clean(mt, "NspiGetMatches", retval);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		talloc_free(mem_ctx);
		return false;
	}

	ppMIds = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	retval = nspi_ResortRestriction(nspi_ctx, mem_ctx, SortTypeDisplayName, MIds, &ppMIds);
	mapitest_print_retval_clean(mt, "NspiResortRestriction", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	MAPIFreeBuffer(MIds);
	MAPIFreeBuffer(ppMIds);
	talloc_free(mem_ctx);

	return ret;
}


/**
   \details Test the NspiDNToMId RPC operation (0x7)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_DNToMId(struct mapitest *mt)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct nspi_context		*nspi_ctx;
	struct StringsArray_r		pNames;
	struct PropertyTagArray_r	*MId;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_DNToMId");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	pNames.Count = 0x1;
	pNames.Strings = (const char **) talloc_array(mem_ctx, char *, 1);
	pNames.Strings[0] = mt->mapi_ctx->session->profile->homemdb;

	MId = talloc_zero(mem_ctx, struct PropertyTagArray_r);

	retval = nspi_DNToMId(nspi_ctx, mem_ctx, &pNames, &MId);
	MAPIFreeBuffer((char **)pNames.Strings);
	MAPIFreeBuffer(MId);
	talloc_free(mem_ctx);

	mapitest_print_retval_clean(mt, "NspiDNToMId", retval);

	if (retval == MAPI_E_SUCCESS) {
	      return true;
	} else {
	      return false;
	}
}


/**
   \details Test the NspiGetPropList RPC operation (0x08)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_GetPropList(struct mapitest *mt)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct nspi_context		*nspi_ctx;
	struct SPropTagArray		*pPropTags = 0;
	struct PropertyTagArray_r	*MIds;
	struct PropertyValue_r		*lpProp;
	struct Restriction_r		Filter;
	struct SPropTagArray		*SPropTagArray;
	struct PropertyRowSet_r		*RowSet;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_GetPropList");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	/* Step 1. Query for current profile username */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_DISPLAY_NAME);
	lpProp = talloc_zero(mem_ctx, struct PropertyValue_r);
	lpProp->ulPropTag = PR_ANR_UNICODE;
	lpProp->dwAlignPad = 0;
	lpProp->value.lpszW = mt->mapi_ctx->session->profile->username;

	Filter.rt = RES_PROPERTY;
	Filter.res.resProperty.relop = RES_PROPERTY;
	Filter.res.resProperty.ulPropTag = PR_ANR_UNICODE;
	Filter.res.resProperty.lpProp = lpProp;

	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	MIds = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	retval = nspi_GetMatches(nspi_ctx, mem_ctx, SPropTagArray, &Filter, 5000, &RowSet, &MIds);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(lpProp);
	MAPIFreeBuffer(RowSet);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		talloc_free(mem_ctx);
		return retval;
	}


	/* Step 2. Call NspiGetPropList using the MId returned by NspiGetMatches */
	pPropTags = talloc_zero(mt->mem_ctx, struct SPropTagArray);
	retval = nspi_GetPropList(nspi_ctx, mem_ctx, 0, MIds->aulPropTag[0], &pPropTags);
	MAPIFreeBuffer(MIds);
	mapitest_print_retval(mt, "NspiGetPropList");

	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(pPropTags);
		talloc_free(mem_ctx);
		return false;
	}

	if (pPropTags) {
		mapitest_print(mt, "* %-35s: %d\n", "Properties number", pPropTags->cValues);
		MAPIFreeBuffer(pPropTags);
	}
	talloc_free(mem_ctx);

	return true;
}


/**
   \details Test the NspiGetProps RPC operation (0x09)

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_GetProps(struct mapitest *mt)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct nspi_context		*nspi_ctx;
	struct StringsArray_r		pNames;
	struct PropertyTagArray_r	*MId;
	struct SPropTagArray		*SPropTagArray;
	struct PropertyRowSet_r		*RowSet;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_GetProps");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	pNames.Count = 0x1;
	pNames.Strings = (const char **) talloc_array(mem_ctx, char *, 1);
	pNames.Strings[0] = mt->mapi_ctx->session->profile->homemdb;

	MId = talloc_zero(mem_ctx, struct PropertyTagArray_r);

	retval = nspi_DNToMId(nspi_ctx, mem_ctx, &pNames, &MId);
	MAPIFreeBuffer((char **)pNames.Strings);
	mapitest_print_retval(mt, "NspiDNToMId");

	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MId);
		talloc_free(mem_ctx);
		return false;
	}

	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_EMS_AB_NETWORK_ADDRESS);
	retval = nspi_GetProps(nspi_ctx, mem_ctx, SPropTagArray, MId, &RowSet);
	mapitest_print_retval_clean(mt, "NspiGetProps", retval);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(MId);
	MAPIFreeBuffer(RowSet);

	talloc_free(mem_ctx);

	if (retval == MAPI_E_SUCCESS) {
	      return true;
	} else {
	      return false;
	}
}


/**
   \details Test the NspiCompareMIds RPC operation (0x0a)

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_CompareMIds(struct mapitest *mt)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct nspi_context		*nspi_ctx;
	uint32_t			plResult;
	struct PropertyTagArray_r	*MIds;
	struct PropertyRowSet_r		*RowSet;
	struct SPropTagArray		*SPropTagArray;
	struct PropertyValue_r		*lpProp;
	struct Restriction_r		Filter;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_CompareMIds");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	/* Build the array of columns we want to retrieve */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_DISPLAY_NAME);

	/* Build the restriction we want for NspiGetMatches */
	lpProp = talloc_zero(mem_ctx, struct PropertyValue_r);
	lpProp->ulPropTag = PR_OBJECT_TYPE;
	lpProp->dwAlignPad = 0;
	lpProp->value.l = 6;

	Filter.rt = RES_PROPERTY;
	Filter.res.resProperty.relop = RES_PROPERTY;
	Filter.res.resProperty.ulPropTag = PR_OBJECT_TYPE;
	Filter.res.resProperty.lpProp = lpProp;

	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	MIds = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	retval = nspi_GetMatches(nspi_ctx, mem_ctx, SPropTagArray, &Filter, 5000, &RowSet, &MIds);
	MAPIFreeBuffer(lpProp);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(RowSet);
	mapitest_print_retval_clean(mt, "NspiGetMatches", retval);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		talloc_free(mem_ctx);
		return false;
	}

	/* Ensure we have at least two result to compare */
	if (MIds->cValues < 2) {
		mapitest_print(mt, "* Only one result found, can't compare\n");
		MAPIFreeBuffer(MIds);
		talloc_free(mem_ctx);
		return false;
	}

	retval = nspi_CompareMIds(nspi_ctx, mem_ctx, MIds->aulPropTag[0], MIds->aulPropTag[1], &plResult);
	mapitest_print_retval_clean(mt, "NspiCompareMIds", retval);
	MAPIFreeBuffer(MIds);
	if (retval != MAPI_E_SUCCESS) {
		talloc_free(mem_ctx);
		return false;
	}

	mapitest_print(mt, "* %-35s: %d\n", "value of the comparison", plResult);
	talloc_free(mem_ctx);

	return true;
}


/**
   \details Test the NspiModProps RPC operation (0xb)
 
   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
*/
_PUBLIC_ bool mapitest_nspi_ModProps(struct mapitest *mt)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct nspi_context		*nspi_ctx;
	struct PropertyRow_r		*pRow;
	struct SPropTagArray		*pPropTags;
	struct PropertyValue_r		modProp;
	struct PropertyTagArray_r	*MIds;
	struct PropertyRowSet_r		*RowSet;
	struct SPropTagArray		*SPropTagArray;
	struct PropertyValue_r		*lpProp;
	struct Restriction_r		Filter;
	const char			*original_office_location;
	bool				ret = true;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_ModProps");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	/* Build the array of columns we want to retrieve */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2, PR_DISPLAY_NAME,
					  PR_DISPLAY_TYPE);

	/* Build the restriction we want for NspiGetMatches */
	lpProp = talloc_zero(mem_ctx, struct PropertyValue_r);
	lpProp->ulPropTag = PR_ACCOUNT;
	lpProp->dwAlignPad = 0;
	lpProp->value.lpszA = mt->mapi_ctx->session->profile->username;

	Filter.rt = RES_PROPERTY;
	Filter.res.resProperty.relop = RES_PROPERTY;
	Filter.res.resProperty.ulPropTag = PR_ACCOUNT;
	Filter.res.resProperty.lpProp = lpProp;

	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	MIds = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	retval = nspi_GetMatches(nspi_ctx, mem_ctx, SPropTagArray, &Filter, 5000, &RowSet, &MIds);
	MAPIFreeBuffer(lpProp);
	MAPIFreeBuffer(RowSet);
	MAPIFreeBuffer(SPropTagArray);
	mapitest_print_retval_clean(mt, "nspi_GetMatches", retval);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		talloc_free(mem_ctx);
		return false;
	}

	/* Query the rows */
	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	retval = nspi_QueryRows(nspi_ctx, mem_ctx, NULL, MIds, 1, &RowSet);
	mapitest_print_retval_clean(mt, "nspi_QueryRows", retval);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		MAPIFreeBuffer(RowSet);
		talloc_free(mem_ctx);
		return false;
	}
	if (RowSet->cRows != 1) {
		mapitest_print(mem_ctx, "unexpected number of rows: %i\n", RowSet->cRows);
		MAPIFreeBuffer(MIds);
		MAPIFreeBuffer(RowSet);
		talloc_free(mem_ctx);
		return false;
	}
	original_office_location = (const char *)find_PropertyValue_data(&(RowSet->aRow[0]), PR_OFFICE_LOCATION);
	mapitest_print(mt, "original PR_OFFICE_LOCATION value: %s\n", original_office_location);

	/* Build the SRow and SPropTagArray for NspiModProps */
	pRow = talloc_zero(mem_ctx, struct PropertyRow_r);
	modProp.ulPropTag = PR_OFFICE_LOCATION;
	modProp.value.lpszA = "MT office location";
	PropertyRow_addprop(pRow, modProp);

	pPropTags = set_SPropTagArray(mem_ctx, 0x1, PR_OFFICE_LOCATION);
	retval = nspi_ModProps(nspi_ctx, mem_ctx, MIds->aulPropTag[0], pPropTags, pRow);
	mapitest_print_retval_clean(mt, "nspi_ModProps", retval);
	MAPIFreeBuffer(pRow);

	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		MAPIFreeBuffer(pPropTags);
		talloc_free(mem_ctx);
		return false;
	}

	/* Check that the property was set correctly */
	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	retval = nspi_QueryRows(nspi_ctx, mem_ctx, NULL, MIds, 1, &RowSet);
	mapitest_print_retval_clean(mt, "nspi_QueryRows", retval);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		MAPIFreeBuffer(RowSet);
		talloc_free(mem_ctx);
		return false;
	}
	if (RowSet->cRows != 1) {
		mapitest_print(mem_ctx, "unexpected number of rows: %i\n", RowSet->cRows);
		MAPIFreeBuffer(MIds);
		MAPIFreeBuffer(RowSet);
		talloc_free(mem_ctx);
		return false;
	}
	if (strcmp((const char *)find_PropertyValue_data(&(RowSet->aRow[0]), PR_OFFICE_LOCATION), "MT office location") != 0) {
		mapitest_print(mt, "PR_OFFICE_LOCATION string value mismatch: %s", (const char *)find_PropertyValue_data(&(RowSet->aRow[0]), PR_OFFICE_LOCATION));
		ret = false;
	} else {
		mapitest_print(mt, "correctly set PR_OFFICE_LOCATION\n");
	}

	/* try to reset the office location back to the original value */
	pRow = talloc_zero(mem_ctx, struct PropertyRow_r);
	modProp.ulPropTag = PR_OFFICE_LOCATION;
	modProp.value.lpszA = original_office_location;
	PropertyRow_addprop(pRow, modProp);

	retval = nspi_ModProps(nspi_ctx, mem_ctx, MIds->aulPropTag[0], pPropTags, pRow);
	mapitest_print_retval_clean(mt, "nspi_ModProps (reset original value)", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	MAPIFreeBuffer(MIds);
	MAPIFreeBuffer(pPropTags);
	MAPIFreeBuffer(pRow);

	talloc_free(mem_ctx);
	
	return ret;
}


/**
   \details Test the NspiGetSpecialTable RPC operation (0x0c)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_GetSpecialTable(struct mapitest *mt)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
	struct PropertyRowSet_r		*RowSet;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_GetSpecialTable");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	retval = nspi_GetSpecialTable(nspi_ctx, mem_ctx, 0x0, &RowSet);
	MAPIFreeBuffer(RowSet);
	mapitest_print_retval_clean(mt, "NspiGetSpecialTable (Hierarchy Table)", retval);

	if (retval != MAPI_E_SUCCESS) {
		talloc_free(mem_ctx);
		return false;
	}

	RowSet = talloc_zero(mt->mem_ctx, struct PropertyRowSet_r);
	retval = nspi_GetSpecialTable(nspi_ctx, mem_ctx, 0x2, &RowSet);
	MAPIFreeBuffer(RowSet);
	mapitest_print_retval_clean(mt, "NspiGetSpecialTable (Address Creation Template)", retval);
	talloc_free(mem_ctx);

	if (retval == MAPI_E_SUCCESS) {
	      return true;
	} else {
	      return false;
	}
}


/**
   \details Test the NspiGetTemplateInfo RPC operation (0x0d)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_GetTemplateInfo(struct mapitest *mt)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
	struct PropertyRow_r	*ppData = NULL;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_GetTemplateInfo");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	ppData = talloc_zero(mem_ctx, struct PropertyRow_r);
	retval = nspi_GetTemplateInfo(nspi_ctx, mem_ctx,
				      TI_TEMPLATE|TI_SCRIPT|TI_EMT|TI_HELPFILE_NAME|TI_HELPFILE_CONTENTS,
				      0, NULL, &ppData);
	mapitest_print_retval_clean(mt, "NspiGetTemplateInfo", retval);
	MAPIFreeBuffer(ppData);
	talloc_free(mem_ctx);

	if (retval == MAPI_E_SUCCESS) {
	      return true;
	} else {
	      return false;
	}
}


/**
   \details Test the NspiModLinkAtt RPC operation (0x0e)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_ModLinkAtt(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
/* 	struct SPropTagArray	*MIds; */
	struct BinaryArray_r	*lpEntryIds;

	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	lpEntryIds = talloc_zero(mt->mem_ctx, struct BinaryArray_r);
	lpEntryIds->cValues = 0;
	lpEntryIds->lpbin = NULL;

	retval = nspi_ModLinkAtt(nspi_ctx, false, PR_EMS_AB_REPORTS, 0x0, lpEntryIds);
	mapitest_print_retval_clean(mt, "NspiModLinkAtt", retval);
	MAPIFreeBuffer(lpEntryIds);

	if (retval == MAPI_E_SUCCESS) {
	      return true;
	} else {
	      return false;
	}
}



/**
   \details Test the NspiQueryColumns RPC operation (0x10)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_QueryColumns(struct mapitest *mt)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
	struct SPropTagArray	*SPropTagArray = NULL;
	
	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_QueryColumns");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;
	
	SPropTagArray = talloc_zero(mem_ctx, struct SPropTagArray);

	retval = nspi_QueryColumns(nspi_ctx, mem_ctx, true, &SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval_clean(mt, "NspiQueryColumns", retval);
		MAPIFreeBuffer(SPropTagArray);
		talloc_free(mem_ctx);
		return false;
	}

	if (SPropTagArray) {
		mapitest_print(mt, "* %d columns returned\n", SPropTagArray->cValues);
		mapitest_print_retval_clean(mt, "NspiQueryColumns", retval);
		MAPIFreeBuffer(SPropTagArray);
	}
	talloc_free(mem_ctx);

	return true;
}


/**
   \details Test the NspiGetNamesFromIDs RPC operation (0x11)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_GetNamesFromIDs(struct mapitest *mt)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct nspi_context		*nspi_ctx;
	struct SPropTagArray		*ppReturnedPropTags;
	struct PropertyNameSet_r	*ppNames;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_GetNamesFromIDs");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;


	ppReturnedPropTags = talloc_zero(mem_ctx, struct SPropTagArray);
	ppNames = talloc_zero(mem_ctx, struct PropertyNameSet_r);
	retval = nspi_GetNamesFromIDs(nspi_ctx, mem_ctx, NULL, NULL, &ppReturnedPropTags, &ppNames);
	mapitest_print_retval_clean(mt, "NspiGetNamesFromIDs", retval);
	MAPIFreeBuffer(ppReturnedPropTags);
	MAPIFreeBuffer(ppNames);
	talloc_free(mem_ctx);

	if (retval == MAPI_E_SUCCESS) {
	      return true;
	} else {
	      return false;
	}
}


/**
   \details Test the NspiGetIDsFromNames RPC operation (0x12)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_GetIDsFromNames(struct mapitest *mt)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct nspi_context		*nspi_ctx;
	struct SPropTagArray		*ppReturnedPropTags;
	struct PropertyNameSet_r	*ppNames;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_GetIDsFromNames");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;


	ppReturnedPropTags = talloc_zero(mem_ctx, struct SPropTagArray);
	ppNames = talloc_zero(mem_ctx, struct PropertyNameSet_r);
	retval = nspi_GetNamesFromIDs(nspi_ctx, mem_ctx, NULL, NULL, &ppReturnedPropTags, &ppNames);
	mapitest_print_retval_clean(mt, "NspiGetNamesFromIDs", retval);
	MAPIFreeBuffer(ppReturnedPropTags);

	if ( (retval != MAPI_E_SUCCESS) || !ppNames ) {
		MAPIFreeBuffer(ppNames);
		talloc_free(mem_ctx);
		return false;
	}

	ppReturnedPropTags = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = nspi_GetIDsFromNames(nspi_ctx, mem_ctx, true, ppNames->cNames, ppNames->aNames, &ppReturnedPropTags);
	mapitest_print_retval_clean(mt, "NspiGetIDsFromNames", retval);
	MAPIFreeBuffer(ppReturnedPropTags);
	MAPIFreeBuffer(ppNames);
	talloc_free(mem_ctx);

	if (retval == MAPI_E_SUCCESS) {
	      return true;
	} else {
	      return false;
	}
}


/**
   \details Test the NspiResolveNames and NspiResolveNamesW RPC
   operations (0x13 and 0x14)

   \param mt pointer on the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_ResolveNames(struct mapitest *mt)
{
	enum MAPISTATUS			retval;
	struct SPropTagArray		*SPropTagArray = NULL;
	struct PropertyRowSet_r		*RowSet = NULL;
	struct PropertyTagArray_r	*flaglist = NULL;
	const char			*username[2];
	const char     			*username_err[2];

	/* Build the username array */
	username[0] = (const char *)mt->profile->mailbox;
	username[1] = NULL;
	/* Build the err username array */
	username_err[0] = talloc_asprintf(mt->mem_ctx, "%s%s", mt->info.szDNPrefix, "nspi_resolve_testcase");
	username_err[1] = NULL;

	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0xd,
					  PR_ENTRYID,
					  PR_DISPLAY_NAME,
					  PR_ADDRTYPE,
					  PR_GIVEN_NAME,
					  PR_SMTP_ADDRESS,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_EMAIL_ADDRESS,
					  PR_SEND_INTERNET_ENCODING,
					  PR_SEND_RICH_INFO,
					  PR_SEARCH_KEY,
					  PR_TRANSMITTABLE_DISPLAY_NAME,
					  PR_7BIT_DISPLAY_NAME);

	/* Test with existing username */
	/* NspiResolveNames (0x13) */
	retval = ResolveNames(mt->session, (const char **)username, SPropTagArray, &RowSet, &flaglist, 0);
	mapitest_print_retval_clean(mt, "NspiResolveNames - existing", retval);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(SPropTagArray);
		return false;
	}
	if ( ! flaglist) {
		mapitest_print(mt, "\tNULL flaglist, which wasn't expected\n");
		MAPIFreeBuffer(SPropTagArray);
		return false;
	}
	if (flaglist->aulPropTag[0] != MAPI_RESOLVED) {
		mapitest_print(mt, "Expected 2 (MAPI_RESOLVED), but NspiResolveNames returned: %i\n", flaglist->aulPropTag[0]);
	} else {
		mapitest_print(mt, "\tGot expected resolution flag\n");
	}
	talloc_free(flaglist);
	talloc_free(RowSet);

	/* NspiResolveNamesW (0x14) */
	retval = ResolveNames(mt->session, (const char **)username, SPropTagArray, &RowSet, &flaglist, MAPI_UNICODE);
	mapitest_print_retval_clean(mt, "NspiResolveNamesW - existing", retval);
	if (flaglist->aulPropTag[0] != MAPI_RESOLVED) {
		mapitest_print(mt, "Expected 2 (MAPI_RESOLVED), but NspiResolveNamesW returned: %i\n", flaglist->aulPropTag[0]);
	} else {
		mapitest_print(mt, "\tGot expected resolution flag\n");
	}
	talloc_free(flaglist);
	talloc_free(RowSet);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(SPropTagArray);
		return false;
	}

	/* Test with non-existant username */
	/* NspiResolveNames (0x13) */
	retval = ResolveNames(mt->session, (const char **)username_err, SPropTagArray, &RowSet, &flaglist, 0);
	mapitest_print_retval_clean(mt, "NspiResolveNames - non existant user name", retval);
	if (flaglist->aulPropTag[0] != MAPI_UNRESOLVED) {
		mapitest_print(mt, "Expected 0 (MAPI_UNRESOLVED), but NspiResolveNames returned: %i\n", flaglist->aulPropTag[0]);
	} else {
		mapitest_print(mt, "\tGot expected resolution flag\n");
	}
	talloc_free(flaglist);
	talloc_free(RowSet);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(SPropTagArray);
		return false;
	}

	/* NspiResolveNamesW (0x14) */
	retval = ResolveNames(mt->session, (const char **)username_err, SPropTagArray, &RowSet, &flaglist, MAPI_UNICODE);
	mapitest_print_retval_clean(mt, "NspiResolveNamesW - non existant user name", retval);
	if (flaglist->aulPropTag[0] != MAPI_UNRESOLVED) {
		mapitest_print(mt, "Expected 0 (MAPI_UNRESOLVED), but NspiResolveNamesW returned: %i\n", flaglist->aulPropTag[0]);
	} else {
		mapitest_print(mt, "\tGot expected resolution flag\n");
	}
	talloc_free(flaglist);
	talloc_free(RowSet);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(SPropTagArray);
		return false;
	}
	MAPIFreeBuffer(SPropTagArray);
	return true;
}

/**
   \details Test the GetGALTable function

   \param mt pointer to the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_GetGALTable(struct mapitest *mt)
{
	struct SPropTagArray	*SPropTagArray;
	struct PropertyRowSet_r		*RowSet;
	enum MAPISTATUS		retval;
	uint32_t		i;
	uint32_t		count;
	uint8_t			ulFlags;
	uint32_t		rowsFetched = 0;
	uint32_t		totalRowsFetched = 0;
	bool			ret = true;

	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0xc,
					  PR_INSTANCE_KEY,
					  PR_ENTRYID,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_EMAIL_ADDRESS_UNICODE,
					  PR_DISPLAY_TYPE,
					  PR_OBJECT_TYPE,
					  PR_ADDRTYPE_UNICODE,
					  PR_OFFICE_TELEPHONE_NUMBER_UNICODE,
					  PR_OFFICE_LOCATION_UNICODE,
					  PR_TITLE_UNICODE,
					  PR_COMPANY_NAME_UNICODE,
					  PR_ACCOUNT_UNICODE);

	count = 0x20;
	ulFlags = TABLE_START;
	do {
		retval = GetGALTable(mt->session, SPropTagArray, &RowSet, count, ulFlags);
		mapitest_print_retval_clean(mt, "GetGALTable", retval);
		if ((!RowSet) || (!(RowSet->aRow))) {
			ret = false;
			goto cleanup;
		}
		rowsFetched = RowSet->cRows;
		totalRowsFetched += rowsFetched;
		if (rowsFetched) {
			for (i = 0; i < rowsFetched; i++) {
				mapitest_print_PAB_entry(mt, &RowSet->aRow[i]);
			}
		}
		ulFlags = TABLE_CUR;
		MAPIFreeBuffer(RowSet);
	} while (rowsFetched == count);

	if (totalRowsFetched < 1) {
		/* We should always have at least ourselves in the list */
		/* So if we got no rows at all, there is a problem */
		ret = false;
	}
cleanup:
	MAPIFreeBuffer(SPropTagArray);

	return ret;
}
