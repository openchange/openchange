/*
   Stand-alone MAPI testsuite

   OpenChange Project - Zentyal functional testing tests

   Copyright (C) Julien Kerihuel 2014
                 Enrique J. Hernandez 2014

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
   \file module_zentyal.c

   \brief Zentyal tests
 */

/**
   \details Test #1872 NspiQueryRows

   \param mt pointer to the top level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_zentyal_1872(struct mapitest *mt)
{
	TALLOC_CTX			*mem_ctx;
	struct nspi_context		*nspi_ctx;
	struct PropertyRowSet_r		*RowSet;
	struct PropertyTagArray_r	*MIds;
	enum MAPISTATUS	retval;

	/* Sanity checks */
	mem_ctx = talloc_named(NULL, 0, "mapitest_zentyal_1872");
	if (!mem_ctx) return false;

	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;
	if (!nspi_ctx) return false;

	MIds = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	if (!MIds) return false;

	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	if (!RowSet) return false;

	/* Update pStat with incorrect data */
	nspi_ctx->pStat->NumPos = 99;

	retval = nspi_QueryRows(nspi_ctx, mem_ctx, NULL, MIds, 1, &RowSet);
	MAPIFreeBuffer(RowSet);
	mapitest_print_retval_clean(mt, "1872", retval);
	if (retval != MAPI_E_INVALID_PARAMETER) {
		MAPIFreeBuffer(MIds);
		talloc_free(mem_ctx);
		return false;
	}

	talloc_free(mem_ctx);
	return true;
}


/**
   \details Test #1863 NspiQueryRows and try to build PR_ENTRYID for
   AD user which is not part of OpenChange.

   \param mt pointer to the top level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_zentyal_1863(struct mapitest *mt)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct nspi_context		*nspi_ctx;
	struct PropertyTagArray_r	*MIds;
	struct PropertyRowSet_r		*RowSet;
	struct SPropTagArray		*SPropTagArray;
	struct PropertyValue_r		*lpProp;
	struct Restriction_r		Filter;

	mem_ctx = talloc_named(NULL, 0, "mapitest_zentyal_1863");
	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;

	/* Build the array of columns we want to retrieve */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2, PR_DISPLAY_NAME,
					  PR_DISPLAY_TYPE);

	/* Build the restriction we want for NspiGetMatches on
	 * existing AD user but not OpenChange one
	 */
	lpProp = talloc_zero(mem_ctx, struct PropertyValue_r);
	lpProp->ulPropTag = PR_ACCOUNT;
	lpProp->dwAlignPad = 0;
	lpProp->value.lpszA = talloc_strdup(lpProp, mt->profile->username);

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
		talloc_free(mem_ctx);
		return false;
	}

	/* Query the rows */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_ENTRYID);
	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	retval = nspi_QueryRows(nspi_ctx, mem_ctx, SPropTagArray, MIds, 1, &RowSet);
	MAPIFreeBuffer(SPropTagArray);
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
   \details Test #1645 NspiUpdateStat and try to sort the result
   with an, for now, unsupported sorting type SortTypePhoneticDisplayName

   \param mt pointer to the top level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_zentyal_1645(struct mapitest *mt)
{
	TALLOC_CTX			*mem_ctx;
	struct nspi_context		*nspi_ctx;
	uint32_t        		plDelta = 1;
	enum MAPISTATUS	retval;

	/* Sanity checks */
	mem_ctx = talloc_named(NULL, 0, "mapitest_zentyal_1645");
	if (!mem_ctx) return false;

	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;
	if (!nspi_ctx) return false;

	/* Update pStat with unsupported SortTypePhoneticDisplayName */
	nspi_ctx->pStat->ContainerID = 0;  // Global Access List
	nspi_ctx->pStat->CurrentRec = MID_END_OF_TABLE;
        nspi_ctx->pStat->Delta = -46;
        nspi_ctx->pStat->NumPos = 3;
        nspi_ctx->pStat->TotalRecs = 3;
	nspi_ctx->pStat->SortType = SortTypePhoneticDisplayName;

	retval = nspi_UpdateStat(nspi_ctx, mem_ctx, &plDelta);
	mapitest_print_retval_clean(mt, "NspiUpdateStat", retval);
	if (retval != MAPI_E_CALL_FAILED) {
		talloc_free(mem_ctx);
		return false;
	}

	talloc_free(mem_ctx);
	return true;
}


/**
    \details Test #1804 ModifyRecipients and try to build RecipientRow
    with multi-value properties (e.g. PidTagUserX509Certificate)

    \param mt pointer to the top level mapitest structure

    \return true on success, otherwise false
*/
_PUBLIC_ bool mapitest_zentyal_1804(struct mapitest *mt)
{
	enum MAPISTATUS			retval;
	mapi_object_t			obj_store;
	mapi_object_t			obj_folder;
	mapi_object_t			obj_message;
	mapi_id_t			id_folder;
	char				**username = NULL;
	struct SPropTagArray		*SPropTagArray = NULL;
	struct PropertyValue_r		value;
	struct PropertyRowSet_r		*RowSet = NULL;
	struct SRowSet			*SRowSet = NULL;
	struct PropertyTagArray_r	*flaglist = NULL;
	mapi_id_t			id_msgs[1];

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Outbox folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderOutbox);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create the message */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_folder, &obj_message);
	mapitest_print_retval(mt, "CreateMessage");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}


	/* Step 4. Resolve the recipients and call ModifyRecipients */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0xA,
					  PR_ENTRYID,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_TRANSMITTABLE_DISPLAY_NAME_UNICODE,
					  PR_EMAIL_ADDRESS_UNICODE,
					  PR_ADDRTYPE_UNICODE,
					  PR_SEND_RICH_INFO,
					  PR_7BIT_DISPLAY_NAME_UNICODE,
					  PR_SMTP_ADDRESS_UNICODE);

	username = talloc_array(mt->mem_ctx, char *, 2);
	username[0] = (char *)mt->profile->username;
	username[1] = NULL;

	retval = ResolveNames(mapi_object_get_session(&obj_message),
			      (const char **)username, SPropTagArray,
			      &RowSet, &flaglist, MAPI_UNICODE);
	mapitest_print_retval_clean(mt, "ResolveNames", retval);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	if (!RowSet) {
		mapitest_print(mt, "Null RowSet\n");
		return false;
	}
	if (!RowSet->cRows) {
		mapitest_print(mt, "No values in RowSet\n");
		MAPIFreeBuffer(RowSet);
		return false;
	}

	value.ulPropTag = PR_SEND_INTERNET_ENCODING;
	value.value.l = 0;
	PropertyRowSet_propcpy(mt->mem_ctx, RowSet, value);

	/* Fake multi-value property on RecipientRow */
	/* PT_MV_STRING8 */
	value.ulPropTag = PR_EMS_AB_PROXY_ADDRESSES;
	value.value.MVszA.cValues = 2;
	value.value.MVszA.lppszA = talloc_array(mt->mem_ctx, const char *, value.value.MVszA.cValues);
	value.value.MVszA.lppszA[0] = "smtp:user@test.com";
	value.value.MVszA.lppszA[1] = "X400:c=US;a= ;p=First Organizati;o=Exchange;s=test";
	PropertyRowSet_propcpy(mt->mem_ctx, RowSet, value);
	/* PT_MV_UNICODE - same layout as PT_MV_STRING8 */
	value.ulPropTag = PR_EMS_AB_PROXY_ADDRESSES_UNICODE;
	PropertyRowSet_propcpy(mt->mem_ctx, RowSet, value);
	/* PT_MV_BINARY */
	value.ulPropTag = PidTagUserX509Certificate;
	value.value.MVbin.cValues = 2;
	value.value.MVbin.lpbin = talloc_array(mt->mem_ctx, struct Binary_r, value.value.MVbin.cValues);
	value.value.MVbin.lpbin[0].cb = 9;
	value.value.MVbin.lpbin[0].lpb = (uint8_t *)"string 1";
	value.value.MVbin.lpbin[1].cb = 9;
	value.value.MVbin.lpbin[1].lpb = (uint8_t *)"string 2";
	PropertyRowSet_propcpy(mt->mem_ctx, RowSet, value);

	SRowSet = talloc_zero(RowSet, struct SRowSet);
	cast_PropertyRowSet_to_SRowSet(SRowSet, RowSet, SRowSet);

	SetRecipientType(&(SRowSet->aRow[0]), MAPI_TO);
	mapitest_print_retval(mt, "SetRecipientType");
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapitest_print_retval_fmt(mt, "ModifyRecipients", "(%s)", "MAPI_TO");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}

	/* Step 5. Delete the message */
	id_msgs[0] = mapi_object_get_id(&obj_message);
	retval = DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print_retval(mt, "DeleteMessage");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}
	/* Release */
	MAPIFreeBuffer(RowSet);
	MAPIFreeBuffer(flaglist);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}
