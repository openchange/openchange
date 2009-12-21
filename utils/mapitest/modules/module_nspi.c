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

#include <libmapi/libmapi.h>
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
	uint32_t       		plDelta = 1;
	struct SRowSet		*SRowSet;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_UpdateStat");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	retval = nspi_GetSpecialTable(nspi_ctx, mem_ctx, 0x2, &SRowSet);
	MAPIFreeBuffer(SRowSet);
	if (GetLastError() != MAPI_E_SUCCESS) {
		talloc_free(mem_ctx);
		return false;
	}

	retval = nspi_UpdateStat(nspi_ctx, mem_ctx, &plDelta);
	mapitest_print_retval(mt, "NspiUpdateStat");
	if (GetLastError() != MAPI_E_SUCCESS) {
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
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
	struct SPropTagArray	*MIds;
	struct SRowSet		*SRowSet;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProp;
	struct Restriction_r	Filter;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_QueryRows");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	/* Build the array of columns we want to retrieve */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2, PR_DISPLAY_NAME,
					  PR_DISPLAY_TYPE);

	/* Build the restriction we want for NspiGetMatches */
	lpProp = talloc_zero(mem_ctx, struct SPropValue);
	lpProp->ulPropTag = PR_ACCOUNT;
	lpProp->dwAlignPad = 0;
	lpProp->value.lpszA = global_mapi_ctx->session->profile->username;

	Filter.rt = RES_PROPERTY;
	Filter.res.resProperty.relop = RES_PROPERTY;
	Filter.res.resProperty.ulPropTag = PR_ACCOUNT;
	Filter.res.resProperty.lpProp = lpProp;

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	MIds = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = nspi_GetMatches(nspi_ctx, mem_ctx, SPropTagArray, &Filter, &SRowSet, &MIds);
	MAPIFreeBuffer(lpProp);
	MAPIFreeBuffer(SRowSet);
	MAPIFreeBuffer(SPropTagArray);
	mapitest_print_retval(mt, "NspiGetMatches");
	if (GetLastError() != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		talloc_free(mem_ctx);
		return false;
	}

	/* Query the rows */
	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	retval = nspi_QueryRows(nspi_ctx, mem_ctx, NULL, MIds, 1, &SRowSet);
	MAPIFreeBuffer(SRowSet);
	mapitest_print_retval(mt, "NspiQueryRows");
	if (GetLastError() != MAPI_E_SUCCESS) {
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
	struct SPropValue	pTarget;
	struct SPropTagArray	*pPropTags;
	struct SRowSet		*SRowSet;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_SeekEntries");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	
	pTarget.ulPropTag = PR_DISPLAY_NAME;
	pTarget.dwAlignPad = 0x0;
	pTarget.value.lpszA = global_mapi_ctx->session->profile->username;

	pPropTags = set_SPropTagArray(mem_ctx, 0x1, PR_ACCOUNT);

	retval = nspi_SeekEntries(nspi_ctx, mem_ctx, SortTypeDisplayName, &pTarget, pPropTags, NULL, &SRowSet);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "NspiSeekEntries");
		talloc_free(pPropTags);
		talloc_free(SRowSet);
		talloc_free(mem_ctx);
		return false;
	}

	mapitest_print_retval(mt, "NspiSeekEntries");
	MAPIFreeBuffer(SRowSet);
	MAPIFreeBuffer(pPropTags);
	talloc_free(mem_ctx);

	return true;
}


/**
   \details Test the NspiGetMatches RPC operation (0x5)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_GetMatches(struct mapitest *mt)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
	struct SPropTagArray	*MIds;
	struct SRowSet		*SRowSet;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProp;
	struct Restriction_r	Filter;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_GetMatches");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	/* Build the array of columns we want to retrieve */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2, PR_DISPLAY_NAME,
					  PR_DISPLAY_TYPE);

	/* Build the restriction we want for NspiGetMatches */
	lpProp = talloc_zero(mem_ctx, struct SPropValue);
	lpProp->ulPropTag = PR_ACCOUNT;
	lpProp->dwAlignPad = 0;
	lpProp->value.lpszA = global_mapi_ctx->session->profile->username;

	Filter.rt = RES_PROPERTY;
	Filter.res.resProperty.relop = RES_PROPERTY;
	Filter.res.resProperty.ulPropTag = PR_ACCOUNT;
	Filter.res.resProperty.lpProp = lpProp;

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	MIds = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = nspi_GetMatches(nspi_ctx, mem_ctx, SPropTagArray, &Filter, &SRowSet, &MIds);
	MAPIFreeBuffer(lpProp);
	MAPIFreeBuffer(SRowSet);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(MIds);
	mapitest_print_retval(mt, "NspiGetMatches");
	if (GetLastError() != MAPI_E_SUCCESS) {
		talloc_free(mem_ctx);
		return false;
	}

	talloc_free(mem_ctx);
	return true;
}


/**
   \details Test the NspiResortRestriction RPC operation (0x6)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_ResortRestriction(struct mapitest *mt)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
	struct Restriction_r	Filter;
	struct SRowSet		*SRowSet = NULL;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct SPropValue	*lpProp = NULL;
	struct SPropTagArray	*MIds = NULL;
	struct SPropTagArray	*ppMIds = NULL;

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
	lpProp = talloc_zero(mem_ctx, struct SPropValue);
	lpProp->ulPropTag = PR_OBJECT_TYPE;
	lpProp->dwAlignPad = 0;
	lpProp->value.l = 6;

	Filter.rt = RES_PROPERTY;
	Filter.res.resProperty.relop = RES_PROPERTY;
	Filter.res.resProperty.ulPropTag = PR_OBJECT_TYPE;
	Filter.res.resProperty.lpProp = lpProp;

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	MIds = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = nspi_GetMatches(nspi_ctx, mem_ctx, SPropTagArray, &Filter, &SRowSet, &MIds);
	MAPIFreeBuffer(lpProp);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(SRowSet);
	mapitest_print_retval(mt, "NspiGetMatches");
	if (GetLastError() != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		talloc_free(mem_ctx);
		return false;
	}

	ppMIds = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = nspi_ResortRestriction(nspi_ctx, mem_ctx, SortTypeDisplayName, MIds, &ppMIds);
	mapitest_print_retval(mt, "NspiResortRestriction");
	if (GetLastError() != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		MAPIFreeBuffer(ppMIds);
		talloc_free(mem_ctx);
		return false;
	}

	MAPIFreeBuffer(MIds);
	MAPIFreeBuffer(ppMIds);
	talloc_free(mem_ctx);

	return true;
}


/**
   \details Test the NspiDNToMId RPC operation (0x7)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_DNToMId(struct mapitest *mt)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
	struct StringsArray_r	pNames;
	struct SPropTagArray	*MId;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_DNToMId");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	pNames.Count = 0x1;
	pNames.Strings = (const char **) talloc_array(mem_ctx, char **, 1);
	pNames.Strings[0] = global_mapi_ctx->session->profile->homemdb;

	MId = talloc_zero(mem_ctx, struct SPropTagArray);

	retval = nspi_DNToMId(nspi_ctx, mem_ctx, &pNames, &MId);
	MAPIFreeBuffer((char **)pNames.Strings);
	MAPIFreeBuffer(MId);
	talloc_free(mem_ctx);

	mapitest_print_retval(mt, "NspiDNToMId");

	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	return true;
}


/**
   \details Test the NspiGetPropList RPC operation (0x08)

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_GetPropList(struct mapitest *mt)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
	struct SPropTagArray	*pPropTags;
	struct SPropTagArray	*MIds;
	struct SPropValue	*lpProp;
	struct Restriction_r	Filter;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		*SRowSet;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_GetPropList");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	/* Step 1. Query for current profile username */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_DISPLAY_NAME);
	lpProp = talloc_zero(mem_ctx, struct SPropValue);
	lpProp->ulPropTag = PR_ANR_UNICODE;
	lpProp->dwAlignPad = 0;
	lpProp->value.lpszW = global_mapi_ctx->session->profile->username;

	Filter.rt = RES_PROPERTY;
	Filter.res.resProperty.relop = RES_PROPERTY;
	Filter.res.resProperty.ulPropTag = PR_ANR_UNICODE;
	Filter.res.resProperty.lpProp = lpProp;

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	MIds = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = nspi_GetMatches(nspi_ctx, mem_ctx, SPropTagArray, &Filter, &SRowSet, &MIds);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(lpProp);
	MAPIFreeBuffer(SRowSet);
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

	if (GetLastError() != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(pPropTags);
		talloc_free(mem_ctx);
		return false;
	}

	mapitest_print(mt, "* %-35s: %d\n", "Properties number", pPropTags->cValues);
	MAPIFreeBuffer(pPropTags);
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
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
	struct StringsArray_r	pNames;
	struct SPropTagArray	*MId;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		*SRowSet;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_GetProps");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	pNames.Count = 0x1;
	pNames.Strings = (const char **) talloc_array(mem_ctx, char **, 1);
	pNames.Strings[0] = global_mapi_ctx->session->profile->homemdb;

	MId = talloc_zero(mem_ctx, struct SPropTagArray);

	retval = nspi_DNToMId(nspi_ctx, mem_ctx, &pNames, &MId);
	MAPIFreeBuffer((char **)pNames.Strings);
	mapitest_print_retval(mt, "NspiDNToMId");

	if (GetLastError() != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MId);
		talloc_free(mem_ctx);
		return false;
	}

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_EMS_AB_NETWORK_ADDRESS);
	retval = nspi_GetProps(nspi_ctx, mem_ctx, SPropTagArray, MId, &SRowSet);
	mapitest_print_retval(mt, "NspiGetProps");
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(MId);
	MAPIFreeBuffer(SRowSet);

	talloc_free(mem_ctx);

	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	return true;
}


/**
   \details Test the NspiCompareMIds RPC operation (0x0a)

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_CompareMIds(struct mapitest *mt)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
	uint32_t		plResult;
	struct SPropTagArray	*MIds;
	struct SRowSet		*SRowSet;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProp;
	struct Restriction_r	Filter;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_CompareMIds");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	/* Build the array of columns we want to retrieve */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_DISPLAY_NAME);

	/* Build the restriction we want for NspiGetMatches */
	lpProp = talloc_zero(mem_ctx, struct SPropValue);
	lpProp->ulPropTag = PR_OBJECT_TYPE;
	lpProp->dwAlignPad = 0;
	lpProp->value.l = 6;

	Filter.rt = RES_PROPERTY;
	Filter.res.resProperty.relop = RES_PROPERTY;
	Filter.res.resProperty.ulPropTag = PR_OBJECT_TYPE;
	Filter.res.resProperty.lpProp = lpProp;

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	MIds = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = nspi_GetMatches(nspi_ctx, mem_ctx, SPropTagArray, &Filter, &SRowSet, &MIds);
	MAPIFreeBuffer(lpProp);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(SRowSet);
	mapitest_print_retval(mt, "NspiGetMatches");
	if (GetLastError() != MAPI_E_SUCCESS) {
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
	mapitest_print_retval(mt, "NspiCompareMIds");
	MAPIFreeBuffer(MIds);
	if (GetLastError() != MAPI_E_SUCCESS) {
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
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
	struct SRow		*pRow;
	struct SPropTagArray	*pPropTags;
	struct SPropValue	modProp;
	struct SPropTagArray	*MIds;
	struct SRowSet		*SRowSet;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProp;
	struct Restriction_r	Filter;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_ModProps");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	/* Build the array of columns we want to retrieve */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2, PR_DISPLAY_NAME,
					  PR_DISPLAY_TYPE);

	/* Build the restriction we want for NspiGetMatches */
	lpProp = talloc_zero(mem_ctx, struct SPropValue);
	lpProp->ulPropTag = PR_ACCOUNT;
	lpProp->dwAlignPad = 0;
	lpProp->value.lpszA = global_mapi_ctx->session->profile->username;

	Filter.rt = RES_PROPERTY;
	Filter.res.resProperty.relop = RES_PROPERTY;
	Filter.res.resProperty.ulPropTag = PR_ACCOUNT;
	Filter.res.resProperty.lpProp = lpProp;

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	MIds = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = nspi_GetMatches(nspi_ctx, mem_ctx, SPropTagArray, &Filter, &SRowSet, &MIds);
	MAPIFreeBuffer(lpProp);
	MAPIFreeBuffer(SRowSet);
	MAPIFreeBuffer(SPropTagArray);
	mapitest_print_retval(mt, "NspiGetMatches");
	if (GetLastError() != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		talloc_free(mem_ctx);
		return false;
	}

	/* Query the rows */
	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	retval = nspi_QueryRows(nspi_ctx, mem_ctx, NULL, MIds, 1, &SRowSet);
	MAPIFreeBuffer(SRowSet);
	mapitest_print_retval(mt, "NspiQueryRows");
	if (GetLastError() != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		talloc_free(mem_ctx);
		return false;
	}

	/* Build the SRow and SPropTagArray for NspiModProps */
	pRow = talloc_zero(mem_ctx, struct SRow);
	modProp.ulPropTag = PR_DISPLAY_NAME_UNICODE;
	modProp.value.lpszW = "mapitest ModProps";
	SRow_addprop(pRow, modProp);

	pPropTags = set_SPropTagArray(mem_ctx, 0x1, PR_DISPLAY_NAME_UNICODE);

	retval = nspi_ModProps(nspi_ctx, mem_ctx, MIds->aulPropTag[0], pPropTags, pRow);
	mapitest_print_retval(mt, "NspiModProps");
	MAPIFreeBuffer(MIds);
	MAPIFreeBuffer(pPropTags);
	MAPIFreeBuffer(pRow);
	talloc_free(mem_ctx);

	/* Assuming true for the moment */
	return true;
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
	struct SRowSet		*SRowSet;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_GetSpecialTable");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	retval = nspi_GetSpecialTable(nspi_ctx, mem_ctx, 0x0, &SRowSet);
	MAPIFreeBuffer(SRowSet);
	mapitest_print_retval(mt, "NspiGetSpecialTable (Hierarchy Table)");

	if (GetLastError() != MAPI_E_SUCCESS) {
		talloc_free(mem_ctx);
		return false;
	}

	SRowSet = talloc_zero(mt->mem_ctx, struct SRowSet);
	retval = nspi_GetSpecialTable(nspi_ctx, mem_ctx, 0x2, &SRowSet);
	MAPIFreeBuffer(SRowSet);
	mapitest_print_retval(mt, "NspiGetSpecialTable (Address Creation Template)");
	talloc_free(mem_ctx);

	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	return true;
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
	struct SRow		*ppData = NULL;

	mem_ctx = talloc_named(NULL, 0, "mapitest_nspi_GetTemplateInfo");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	ppData = talloc_zero(mem_ctx, struct SRow);
	retval = nspi_GetTemplateInfo(nspi_ctx, mem_ctx,
				      TI_TEMPLATE|TI_SCRIPT|TI_EMT|TI_HELPFILE_NAME|TI_HELPFILE_CONTENTS,
				      0, NULL, &ppData);
	mapitest_print_retval(mt, "NspiGetTemplateInfo");
	MAPIFreeBuffer(ppData);
	talloc_free(mem_ctx);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	return true;
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
	mapitest_print_retval(mt, "NspiModLinkAtt");
	MAPIFreeBuffer(lpEntryIds);

	/* Assuming true for the moment */
	return true;
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
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "NspiQueryColumns");
		MAPIFreeBuffer(SPropTagArray);
		talloc_free(mem_ctx);
		return false;
	}

	mapitest_print(mt, "* %d columns returned\n", SPropTagArray->cValues);
	mapitest_print_retval(mt, "NspiQueryColumns");
	MAPIFreeBuffer(SPropTagArray);
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
	mapitest_print_retval(mt, "NspiGetNamesFromIDs");
	MAPIFreeBuffer(ppReturnedPropTags);
	MAPIFreeBuffer(ppNames);
	talloc_free(mem_ctx);

	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	return true;
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
	mapitest_print_retval(mt, "NspiGetNamesFromIDs");
	MAPIFreeBuffer(ppReturnedPropTags);

	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(ppNames);
		talloc_free(mem_ctx);
		return false;
	}

	ppReturnedPropTags = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = nspi_GetIDsFromNames(nspi_ctx, mem_ctx, true, ppNames->cNames, ppNames->aNames, &ppReturnedPropTags);
	mapitest_print_retval(mt, "NspiGetIDsFromNames");
	MAPIFreeBuffer(ppReturnedPropTags);
	MAPIFreeBuffer(ppNames);
	talloc_free(mem_ctx);

	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	errno = retval;
	return true;
}


/**
   \details Test the NspiResolveNames and NspiResolveNamesW RPC
   operations (0x13 and 0x14)

   \param mt pointer on the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_nspi_ResolveNames(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi_ctx;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct SRowSet		*SRowSet = NULL;
	struct SPropTagArray	*flaglist = NULL;
	const char     		*username[2];

	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	/* Build the username array */
	username[0] = (const char *)mt->info.szDisplayName;
	username[1] = NULL;

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

	/* NspiResolveNames (0x13) */
	flaglist = talloc_zero(mt->mem_ctx, struct SPropTagArray);
	SRowSet = talloc_zero(mt->mem_ctx, struct SRowSet);

	retval = ResolveNames(mt->session, (const char **)username, SPropTagArray, &SRowSet, &flaglist, 0);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "NspiResolveNames");
		MAPIFreeBuffer(SPropTagArray);
		talloc_free(flaglist);
		talloc_free(SRowSet);
		return false;
	}
	talloc_free(flaglist);
	talloc_free(SRowSet);
	mapitest_print_retval(mt, "NspiResolveNames");

	/* NspiResolveNamesW (0x14) */
	retval = ResolveNames(mt->session, (const char **)username, SPropTagArray, &SRowSet, &flaglist, MAPI_UNICODE);
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "NspiResolveNamesW");
		talloc_free(flaglist);
		talloc_free(SRowSet);
		return false;
	}
	talloc_free(flaglist);
	talloc_free(SRowSet);
	mapitest_print_retval(mt, "NspiResolveNamesW");

	return true;
}
