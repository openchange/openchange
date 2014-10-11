/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2011.

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
#include <ctype.h>
#include <time.h>

/**
   \file freebusy.c

   \brief Convenient API to access FreeBusy
 */


/**
   \details Retrieve FreeBusy data associated with the specified
   recipient

   \param obj_store pointer to the public folder MAPI object
   \param recipient name of the recipient to fetch freebusy data
   \param pSRow pointer to the returned properties

   \note The function returns a SRow structure with the following
   property tags:
   -# PR_NORMALIZED_SUBJECT
   -# PR_FREEBUSY_RANGE_TIMESTAMP
   -# PR_FREEBUSY_PUBLISH_START
   -# PR_FREEBUSY_PUBLISH_END
   -# PR_SCHDINFO_MONTHS_MERGED
   -# PR_SCHDINFO_FREEBUSY_MERGED
   -# PR_SCHDINFO_MONTHS_TENTATIVE
   -# PR_SCHDINFO_FREEBUSY_TENTATIVE
   -# PR_SCHDINFO_MONTHS_BUSY
   -# PR_SCHDINFO_FREEBUSY_BUSY
   -# PR_SCHDINFO_MONTHS_OOF
   -# PR_SCHDINFO_FREEBUSY_OOF

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS GetUserFreeBusyData(mapi_object_t *obj_store, 
					     const char *recipient,
					     struct SRow *pSRow)
{
	enum MAPISTATUS			retval;
	TALLOC_CTX			*mem_ctx;
	struct mapi_session		*session;
	mapi_id_t			id_freebusy;
	mapi_object_t			obj_freebusy;
	mapi_object_t			obj_exfreebusy;
	mapi_object_t			obj_message;
	mapi_object_t			obj_htable;
	mapi_object_t			obj_ctable;
	struct PropertyRowSet_r		*pRowSet;
	struct SRowSet			SRowSet;
	struct SPropValue		*lpProps;
	struct mapi_SRestriction	res;
	struct SSortOrderSet		criteria;
	struct SPropTagArray		*SPropTagArray = NULL;
	char				*message_name;
	char				*folder_name;
	const char			*email = NULL;
	char				*o = NULL;
	char				*ou = NULL;
	char				*username;
	const int64_t			*fid;
	const int64_t			*mid;
	uint32_t			i;
	uint32_t			count;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!pSRow, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_SESSION_LIMIT, NULL);

	mem_ctx = (TALLOC_CTX *) session;

	/* Step 0. Retrieve the user Email Address and build FreeBusy strings */
	pRowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	retval = GetABRecipientInfo(session, recipient, NULL, &pRowSet);
	OPENCHANGE_RETVAL_IF(retval, retval, pRowSet);

	email = (const char *) get_PropertyValue_PropertyRowSet_data(pRowSet, PR_EMAIL_ADDRESS_UNICODE);
	o = x500_get_dn_element(mem_ctx, email, ORG);
	ou = x500_get_dn_element(mem_ctx, email, ORG_UNIT);
	username = x500_get_dn_element(mem_ctx, email, "/cn=Recipients/cn=");

	if (!username) {
		MAPIFreeBuffer(o);
		MAPIFreeBuffer(ou);
		MAPIFreeBuffer(pRowSet);
		
		return MAPI_E_NOT_FOUND;
	}

	/* toupper username */
	for (i = 0; username[i]; i++) {
		username[i] = toupper((unsigned char)username[i]);
	}

	message_name = talloc_asprintf(mem_ctx, FREEBUSY_USER, username);
	folder_name = talloc_asprintf(mem_ctx, FREEBUSY_FOLDER, o, ou);

	MAPIFreeBuffer(username);
	MAPIFreeBuffer(o);
	MAPIFreeBuffer(ou);
	MAPIFreeBuffer(pRowSet);

	/* Step 1. Open the FreeBusy root folder */
	retval = GetDefaultPublicFolder(obj_store, &id_freebusy, olFolderPublicFreeBusyRoot);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	mapi_object_init(&obj_freebusy);
	retval = OpenFolder(obj_store, id_freebusy, &obj_freebusy);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Step 2. Open the hierarchy table */
	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_freebusy, &obj_htable, 0, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Step 3. Customize Hierarchy Table view */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2,
					  PR_FID,
					  PR_DISPLAY_NAME);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Step 4. Find FreeBusy folder row */
	res.rt = RES_PROPERTY;
	res.res.resProperty.relop = RELOP_EQ;
	res.res.resProperty.ulPropTag = PR_DISPLAY_NAME;
	res.res.resProperty.lpProp.ulPropTag = PR_DISPLAY_NAME;
	res.res.resProperty.lpProp.value.lpszA = folder_name;
	retval = FindRow(&obj_htable, &res, BOOKMARK_BEGINNING, DIR_FORWARD, &SRowSet);
	MAPIFreeBuffer(folder_name);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Step 5. Open the folder */
	fid = (const int64_t *) get_SPropValue_SRowSet_data(&SRowSet, PR_FID);
	if (!fid || *fid == MAPI_E_NOT_FOUND) return MAPI_E_NOT_FOUND;

	mapi_object_init(&obj_exfreebusy);
	retval = OpenFolder(&obj_freebusy, *fid, &obj_exfreebusy);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Step 6. Open the contents table */
	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(&obj_exfreebusy, &obj_ctable, 0, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Step 7. Customize Contents Table view */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_ADDRBOOK_MID,
					  PR_INSTANCE_NUM,
					  PR_NORMALIZED_SUBJECT);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Step 8. Sort the table */
	memset(&criteria, 0x0, sizeof (struct SSortOrderSet));
	criteria.cSorts = 1;
	criteria.aSort = talloc_array(mem_ctx, struct SSortOrder, criteria.cSorts);
	criteria.aSort[0].ulPropTag = PR_NORMALIZED_SUBJECT;
	criteria.aSort[0].ulOrder = TABLE_SORT_ASCEND;
	retval = SortTable(&obj_ctable, &criteria);
	MAPIFreeBuffer(criteria.aSort);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Step 9. Find the user FreeBusy message row */
	res.rt = RES_PROPERTY;
	res.res.resProperty.relop = RELOP_EQ;
	res.res.resProperty.ulPropTag = PR_NORMALIZED_SUBJECT;
	res.res.resProperty.lpProp.ulPropTag = PR_NORMALIZED_SUBJECT;
	res.res.resProperty.lpProp.value.lpszA = message_name;
	retval = FindRow(&obj_ctable, &res, BOOKMARK_BEGINNING, DIR_FORWARD, &SRowSet);
	MAPIFreeBuffer(message_name);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Step 10. Open the message */
	fid = (const int64_t *)get_SPropValue_SRowSet_data(&SRowSet, PR_FID);	
	mid = (const int64_t *)get_SPropValue_SRowSet_data(&SRowSet, PR_MID);
	OPENCHANGE_RETVAL_IF(!fid || *fid == MAPI_E_NOT_FOUND, MAPI_E_NOT_FOUND, NULL);
	OPENCHANGE_RETVAL_IF(!mid || *mid == MAPI_E_NOT_FOUND, MAPI_E_NOT_FOUND, NULL);

	mapi_object_init(&obj_message);
	retval = OpenMessage(&obj_exfreebusy, *fid, *mid, &obj_message, 0x0);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Step 11. Get FreeBusy properties */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0xc, 
					  PR_NORMALIZED_SUBJECT,
					  PR_FREEBUSY_RANGE_TIMESTAMP,
					  PR_FREEBUSY_PUBLISH_START,
					  PR_FREEBUSY_PUBLISH_END,
					  PR_SCHDINFO_MONTHS_MERGED,
					  PR_SCHDINFO_FREEBUSY_MERGED,
					  PR_SCHDINFO_MONTHS_TENTATIVE,
					  PR_SCHDINFO_FREEBUSY_TENTATIVE,
					  PR_SCHDINFO_MONTHS_BUSY,
					  PR_SCHDINFO_FREEBUSY_BUSY,
					  PR_SCHDINFO_MONTHS_OOF,
					  PR_SCHDINFO_FREEBUSY_OOF);
	retval = GetProps(&obj_message, 0, SPropTagArray, &lpProps, &count);
	MAPIFreeBuffer(SPropTagArray);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	pSRow->cValues = count;
	pSRow->lpProps = lpProps;

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_ctable);
	mapi_object_release(&obj_exfreebusy);
	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_freebusy);

	return MAPI_E_SUCCESS;
}


/**
   \details Check if a date conflicts with existing FreeBusy Busy/Out
   Of Office events

   \param obj_store pointer to the public folder MAPI object
   \param date pointer to the date to check
   \param conflict pointer to the returned boolean value

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS IsFreeBusyConflict(mapi_object_t *obj_store,
					    struct FILETIME *date,
					    bool *conflict)
{
	enum MAPISTATUS			retval;
	struct mapi_session		*session;
	struct SRow			aRow;
	const struct LongArray_r	*all_months;
	const struct BinaryArray_r	*all_events;
	struct Binary_r			bin;
	const uint32_t			*publish_start;
	NTTIME				nttime;
	time_t				time;
	struct tm			*tm;
	uint32_t			fbusytime;
	uint32_t			fmonth;
	uint32_t			month;
	int				year;
	uint32_t			idx;
	uint32_t			i;
	bool				found = false;
	uint32_t			start;
	uint32_t			end;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!date, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!conflict, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_store);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_SESSION_LIMIT, NULL);

	*conflict = false;

	/* Step 1. Retrieve the freebusy data for the user */
	retval = GetUserFreeBusyData(obj_store, session->profile->username, &aRow);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	publish_start = (const uint32_t *) find_SPropValue_data(&aRow, PR_FREEBUSY_PUBLISH_START);
	all_months = (const struct LongArray_r *) find_SPropValue_data(&aRow, PR_SCHDINFO_MONTHS_MERGED);
	all_events = (const struct BinaryArray_r *) find_SPropValue_data(&aRow, PR_SCHDINFO_FREEBUSY_MERGED);

	if (!all_months || (*(const uint32_t *)all_months) == MAPI_E_NOT_FOUND ||
	    !all_events || (*(const uint32_t *)all_events) == MAPI_E_NOT_FOUND) {
		return MAPI_E_SUCCESS;
	}

	/* Step 2. Convert the input date to freebusy */
	nttime = ((uint64_t) date->dwHighDateTime << 32);
	nttime |= (uint64_t) date->dwLowDateTime;
	time = nt_time_to_unix(nttime);
	tm = localtime(&time);

	fmonth = tm->tm_mon + 1;
	fbusytime = ((tm->tm_mday - 1) * 60 * 24) + (tm->tm_hour * 60);

	/* Step 3. Check if the years matches */
	year = GetFreeBusyYear(publish_start);

	if (year != (tm->tm_year + 1900)) {
		return MAPI_E_SUCCESS;
	}

	/* Step 4. Check if we have already registered events for the month */
	
	for (idx = 0; idx < all_months->cValues; idx++) {
		month = all_months->lpl[idx] - (year * 16);
		if (month == fmonth) {
			found = true;
			break;
		}
	}
	if (found == false) return MAPI_E_SUCCESS;

	/* Step 5. Check if one this months events conflicts with the date */
	bin = all_events->lpbin[idx];
	if (bin.cb % 4) {
		return MAPI_E_INVALID_PARAMETER;
	}

	for (i = 0; i < bin.cb; i += 4) {
		start = (bin.lpb[i + 1] << 8) | bin.lpb[i];
		end = (bin.lpb[i + 3] << 8) | bin.lpb[i + 2];
		if ((fbusytime >= start) && (fbusytime <= end)) {
			*conflict = true;
			return MAPI_E_SUCCESS;
		}
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Return the year associated with the FreeBusy start range

   \param publish_start pointer to the publish start integer

   \return a valid year on success, otherwise 0
 */
_PUBLIC_ int GetFreeBusyYear(const uint32_t *publish_start)
{
	struct tm	*tm;
	uint32_t	year;
	time_t		time;
	NTTIME		nttime;

	if (!publish_start) return 0;

	nttime = *publish_start;
	nttime *= 60;
	nttime *= 10000000;
	time = nt_time_to_unix(nttime);
	tm = localtime(&time);
	year = (tm->tm_year + 1900);

	return year;
}
