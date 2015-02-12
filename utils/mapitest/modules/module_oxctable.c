/*
   Stand-alone MAPI testsuite

   OpenChange Project - TABLE OBJECT PROTOCOL operations

   Copyright (C) Julien Kerihuel 2008
   Copyright (C) Brad Hards 2008

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
   \file module_oxctable.c

   \brief Table Object Protocol test suite
*/

/**
   \details Test the SetColumns (0x12) operation

   This function:
   -# Opens the Inbox folder and gets the hierarchy table
   -# Calls the SetColumns operation
   -# Cleans up

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_SetColumns(struct mapitest *mt)
{
	mapi_object_t		obj_htable;
	struct SPropTagArray	*SPropTagArray;

	/* Step 1. Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	/* Step 2. SetColumns */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	mapitest_print_retval(mt, "SetColumns");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Release */
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return true;
}


/**
   \details Test the QueryColumns (0x37) operation

   This function:
   	-# Opens the Inbox folder and gets the hierarchy table
	-# Calls the QueryColumn operation
	-# Calls SetColumns on the test folder
	-# Checks that QueryColumns on the test folder is correct
	-# Cleans up

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_QueryColumns(struct mapitest *mt)
{
	mapi_object_t		obj_htable;
	mapi_object_t		obj_test_folder;
	struct SPropTagArray	columns;
	struct mt_common_tf_ctx	*context;
	uint32_t		count;

	/* Step 1. Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	/* Step 2. QueryColumns */
	QueryColumns(&obj_htable, &columns);
	mapitest_print_retval(mt, "QueryColumns");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Get the test folder */
	context = mt->priv;
	mapi_object_init(&(obj_test_folder));
	GetContentsTable(&(context->obj_test_folder), &(obj_test_folder), 0, &count);
	mapitest_print_retval(mt, "GetContentsTable");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	if (count != 10) {
		mapitest_print(mt, "* %-35s: unexpected count (%i)\n", "GetContentsTable", count);
		/* This isn't a hard error for this test though, because it might be from a 
		   previous test failure. Clean up and try again */
	}


	/* Step 4. QueryColumns on a contents folder */
	QueryColumns(&(obj_test_folder), &columns);
	mapitest_print_retval(mt, "QueryColumns");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	/* TODO: check the return count against something */
	mapitest_print(mt, "column count: %i\n", columns.cValues);

	/* Step 6. Release */
	mapi_object_release(&obj_htable);
	mapi_object_release(&(obj_test_folder));
	mapitest_common_cleanup(mt);

	return true;
}

/**
   \details Test the Restrict (0x14) operation

   This function:
   -# Opens the Inbox folder and creates some test content
   -# Checks that the content is OK
   -# Applies a filter
   -# Checks the results are as expected.
   -# Resets the table
   -# Checks the results are as expected.
   -# Cleans up

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_Restrict(struct mapitest *mt)
{
	mapi_object_t		obj_htable;
	mapi_object_t		obj_test_folder;
	struct mt_common_tf_ctx	*context;
	uint32_t		count = 0;
	uint32_t		origcount = 0;
	uint32_t		Numerator = 0;
	uint32_t		Denominator = 0;
	struct mapi_SRestriction res;
	bool			ret = true;

	/* Step 1. Logon */
	if (! mapitest_common_setup(mt, &obj_htable, &count)) {
		return false;
	}

	/* Step 2. Get the test folder */
	context = mt->priv;
	mapi_object_init(&(obj_test_folder));
	GetContentsTable(&(context->obj_test_folder), &(obj_test_folder), 0, &origcount);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "GetContentsTable");
		ret = false;
		goto cleanup;
	}
	if (origcount != 10) {
		mapitest_print(mt, "* %-35s: unexpected count (%i)\n", "GetContentsTable", count);
		/* This isn't a hard error for this test though, because it might be from a 
		   previous test failure. Clean up and try again */
	}

	/* Apply a filter */
	res.rt = RES_PROPERTY;
	res.res.resProperty.relop = RES_PROPERTY;
	res.res.resProperty.ulPropTag = PR_SUBJECT;
	res.res.resProperty.lpProp.ulPropTag = PR_SUBJECT;
	res.res.resProperty.lpProp.value.lpszA = MT_MAIL_SUBJECT;

	Restrict(&(obj_test_folder), &res, NULL);
	mapitest_print_retval(mt, "Restrict");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Checks the results are as expected */
	context = mt->priv;
	QueryPosition(&(obj_test_folder), &Numerator, &Denominator);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "QueryPosition");
		ret = false;
		goto cleanup;
	}
	if (Denominator != origcount/2) {
		mapitest_print(mt, "* %-35s: unexpected filtered count (%i)\n", "QueryPosition", Denominator);
		ret = false;
		goto cleanup;
	}

	/* Resets the table */
	Reset(&(obj_test_folder));
	mapitest_print_retval(mt, "Reset");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Checks the results are as expected */
	context = mt->priv;
	QueryPosition(&(obj_test_folder), &Numerator, &Denominator);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "QueryPosition");
		ret = false;
		goto cleanup;
	}
	if (Denominator != origcount) {
		mapitest_print(mt, "* %-35s: unexpected reset count (%i)\n", "QueryPosition", Denominator);
		ret = false;
		goto cleanup;
	}

 cleanup:
	/* Release */
	mapi_object_release(&obj_htable);
	mapi_object_release(&(obj_test_folder));
	mapitest_common_cleanup(mt);

	return ret;
}

/**
   \details Test the QueryRows (0x15) operation

   This function:
   -# Opens the Inbox folder and gets the hierarchy table
   -# Set the required columns
   -# Calls QueryRows until the end of the table
   -# Open the test folder, and get its contents
   -# Calls QueryRows until the end of the test folder
   -# Checks the results are as expected.
   -# Cleans up

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_QueryRows(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_htable;
	mapi_object_t		obj_test_folder;
	struct SRowSet		SRowSet;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	lpProp;
	struct mt_common_tf_ctx	*context;
	uint32_t		idx = 0;
	uint32_t		count = 0;
	const char*		data;

	/* Step 1. Logon */
	if (! mapitest_common_setup(mt, &obj_htable, &count)) {
		return false;
	}

	/* Step 2. Set Table Columns */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "SetColumns");
		return false;
	}

	/* Step 3. QueryRows */
	do {
		retval = QueryRows(&obj_htable, 0x2, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet);
		if (SRowSet.cRows > 0) {
			idx += SRowSet.cRows;
			if (retval == MAPI_E_SUCCESS) {
				mapitest_print(mt, "* %-35s: %.2d/%.2d [PASSED]\n", 
					       "QueryRows", idx, count);
			} else {
				mapitest_print(mt, "* %-35s: %.2d/%.2d [FAILED]\n", 
					       "QueryRows", idx, count);
			}
		}
	} while (retval == MAPI_E_SUCCESS && SRowSet.cRows > 0);


	/* Step 4. Get the test folder */
	context = mt->priv;
	mapi_object_init(&(obj_test_folder));
	GetContentsTable(&(context->obj_test_folder), &(obj_test_folder), 0, &count);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "GetContentsTable");
		return false;
	}
	if (count != 10) {
		mapitest_print(mt, "* %-35s: unexpected count (%i)\n", "GetContentsTable", count);
		/* This isn't a hard error for this test though, because it might be from a 
		   previous test failure. Clean up and try again */
	}

	/* Step 5. Set Table Columns on the test folder */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_BODY, PR_MESSAGE_CLASS);
	retval = SetColumns(&obj_test_folder, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "SetColumns");
		return false;
	}

	/* Step 6. QueryRows on test folder contents */
	idx = 0;
	do {
		retval = QueryRows(&(obj_test_folder), 0x2, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet);
		if (SRowSet.cRows > 0) {
			idx += SRowSet.cRows;
			if (retval == MAPI_E_SUCCESS) {
			  	uint32_t	i;
				mapitest_print(mt, "* %-35s: %.2d/%.2d [PASSED]\n", 
					       "QueryRows", idx, count);
				for (i = 0; i < SRowSet.cRows; ++i) {
					lpProp = SRowSet.aRow[i].lpProps[0];
					if (lpProp.ulPropTag != PR_BODY) {
						mapitest_print(mt, "* %-35s: Bad proptag0 (0x%x)\n", 
							       "QueryRows", lpProp.ulPropTag);
						return false;
					}
					data = get_SPropValue_data(&lpProp);
					if (0 != strncmp(data, "Body of message", 15)) {
						mapitest_print(mt, "* %-35s: Bad propval0 (%s)\n", 
							       "QueryRows", data);
						return false;
					}
					lpProp = SRowSet.aRow[i].lpProps[1];
					if (lpProp.ulPropTag != PR_MESSAGE_CLASS) {
						mapitest_print(mt, "* %-35s: Bad proptag1 (0x%x)\n", 
							       "QueryRows", lpProp.ulPropTag);
						return false;
					}
					data = get_SPropValue_data(&lpProp);
					if (0 != strncmp(data, "IPM.Note", 8)) {
						mapitest_print(mt, "* %-35s: Bad propval1 (%s)\n", 
							       "QueryRows", data);
						return false;
					}
				}
			} else {
				mapitest_print(mt, "* %-35s: %.2d/%.2d [FAILED]\n", 
					       "QueryRows", idx, count);
			}
		}
	} while (retval == MAPI_E_SUCCESS && SRowSet.cRows > 0);

	/* Release */
	mapi_object_release(&obj_htable);
	mapi_object_release(&(obj_test_folder));
	mapitest_common_cleanup(mt);

	return true;
}

/**
   \details Test the GetStatus (0x16) operation

   This function:
   -# Opens the Inbox folder and gets the hierarchy table
   -# Call GetStatus
   -# Cleans up
 */
_PUBLIC_ bool mapitest_oxctable_GetStatus(struct mapitest *mt)
{
	bool			ret = true;
	mapi_object_t		obj_htable;
	uint8_t			TableStatus;

	/* Step 1. Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	/* Step 2. GetStatus */
	GetStatus(&obj_htable, &TableStatus);
	mapitest_print_retval(mt, "GetStatus");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
	} else {
		mapitest_print(mt, "* %-35s: TableStatus: %d\n", "GetStatus", TableStatus);
	}

	/* Step 3. Release */
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return ret;
}


/**
   \details Test the SeekRow (0x18) operation

   This function:
   -# Opens the Inbox folder and gets the hierarchy table
   -# SeekRow with BOOKMARK_BEGINNING
   -# SeekRow with BOOKMARK_END
   -# SeekRow with BOOKMARK_CURRENT
   -# Cleans up

   \param mt pointer on the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_SeekRow(struct mapitest *mt)
{
	mapi_object_t		obj_htable;
	uint32_t		count;

	/* Step 1. Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	/* Step 2. SeekRow */
	SeekRow(&obj_htable, BOOKMARK_BEGINNING, 0, &count);
	mapitest_print_retval_fmt(mt, "SeekRow", "(BOOKMARK_BEGINNING)");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	SeekRow(&obj_htable, BOOKMARK_END, 0, &count);
	mapitest_print_retval_fmt(mt, "SeekRow", "(BOOKMARK_END)");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	SeekRow(&obj_htable, BOOKMARK_CURRENT, 0, &count);
	mapitest_print_retval_fmt(mt, "SeekRow", "(BOOKMARK_CURRENT)");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return true;
}


/**
   \details Test the SeekRowApprox (0x1a) operation

   This function:
   -# Opens the Inbox folder and gets the hierarchy table
   -# SeekRowApprox with 0/1, 1/1 and 1/2 fractional values
   -# Cleans up

   \param mt pointer on the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_SeekRowApprox(struct mapitest *mt)
{
	mapi_object_t		obj_htable;

	/* Step 1. Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	/* Step 2. SeekRowApprox */
	SeekRowApprox(&obj_htable, 0, 1);
	mapitest_print_retval_fmt(mt, "SeekRowApprox", "0/1");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	SeekRowApprox(&obj_htable, 1, 1);
	mapitest_print_retval_fmt(mt, "SeekRowApprox", "1/1");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	SeekRowApprox(&obj_htable, 1, 2);
	mapitest_print_retval_fmt(mt, "SeekRowApprox", "1/2");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Release */
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return true;
}



/**
   \details Test the CreateBookmark (0x1b) operation

   This function:
   -# Opens the Inbox folder and gets the hierarchy table
   -# Customize the MAPI table view
   -# CreateBookmark for each table's row
   -# Free Bookmark for each created bookmark
   -# Cleans up

   \param mt pointer on the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_CreateBookmark(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_htable;
	uint32_t		*bkPosition;
	uint32_t		count;
	uint32_t		i;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;


	/* Step 1. Logon */
	if (! mapitest_common_setup(mt, &obj_htable, &count)) {
		return false;
	}

	/* Step 2. Customize the table view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create Bookmarks */
	bkPosition = talloc_array(mt->mem_ctx, uint32_t, 1);
	retval = QueryRows(&obj_htable, 100, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet);
	for (i = 0; i < SRowSet.cRows; i++) {
		bkPosition = talloc_realloc(mt->mem_ctx, bkPosition, uint32_t, i + 2);
		retval = CreateBookmark(&obj_htable, &(bkPosition[i]));
		mapitest_print_retval_fmt_clean(mt, "CreateBookmark", retval, "(%.2d)", i);
		if (retval != MAPI_E_SUCCESS) {
			return false;
		}
	}

	retval = mapi_object_bookmark_get_count(&obj_htable, &count);

	/* Step 4. Free Bookmarks */
	for (i = 0; i < count; i++) {
		retval = FreeBookmark(&obj_htable, bkPosition[i]);
		mapitest_print_retval_fmt_clean(mt, "FreeBookmark", retval, "(%.2d)", i);
		if (retval != MAPI_E_SUCCESS) {
			return false;
		}
	}

	/* Step 5. Release */
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);
	talloc_free(bkPosition);

	return true;
}



/**
   \details Test the SeekRowBookmark (0x19) operation

   This function:
   -# Open the Inbox folder and retrieve the hierarchy table
   -# Customize the MAPI table view
   -# SeekBookmark for each table's row
   -# Free Bookmark for each created bookmark

   \param mt pointer on the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_SeekRowBookmark(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_htable;
	uint32_t		*bkPosition;
	uint32_t		count;
	uint32_t		row;
	uint32_t		i;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;

	/* Step 1. Logon */
	if (! mapitest_common_setup(mt, &obj_htable, &count)) {
		return false;
	}

	/* Step 2. Customize the table view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create Bookmarks */
	bkPosition = talloc_array(mt->mem_ctx, uint32_t, 1);
	retval = QueryRows(&obj_htable, 100, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet);
	for (i = 0; i < SRowSet.cRows; i++) {
		bkPosition = talloc_realloc(mt->mem_ctx, bkPosition, uint32_t, i + 2);
		retval = CreateBookmark(&obj_htable, &(bkPosition[i]));
		mapitest_print_retval_fmt_clean(mt, "CreateBookmark", retval, "(%.2d)", i);
		if (retval != MAPI_E_SUCCESS) {
			return false;
		}
	}
	mapitest_print_retval(mt, "CreateBookmark");

	retval = mapi_object_bookmark_get_count(&obj_htable, &count);

	/* Step 4. SeekRowBookmark */
	for (i = 0; i < SRowSet.cRows; i++) {
		retval = SeekRowBookmark(&obj_htable, bkPosition[i], 0, &row);
		mapitest_print_retval_fmt_clean(mt, "SeekRowBookmark", retval, "(%.2d)", i);
	}


	/* Step 5. Free Bookmarks */
	for (i = 0; i < SRowSet.cRows; i++) {
		retval = FreeBookmark(&obj_htable, bkPosition[i]);
		mapitest_print_retval_fmt_clean(mt, "FreeBookmark", retval, "(%.2d)", i);
		if (retval != MAPI_E_SUCCESS) {
			return false;
		}
	}

	/* Step 6. Release */
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	talloc_free(bkPosition);

	return true;
}

/**
   \details Test the SortTable (0x13), ExpandRow (0x59), CollapseRow(0x5a),
   GetCollapseState(0x6b) and SetCollapseState (0x6c) operations

   This function:
   -# Opens the Inbox folder and creates some test content
   -# Checks that the content is OK
   -# Applies a sort and categorisation
   -# Checks the results are as expected.
   -# Save away the Row ID and Insatnce Number for the first header
   -# Collapse the first category
   -# Checks the results are as expected.
   -# Save the "collapse state"
   -# Expand the first category again
   -# Checks the results are as expected
   -# Restore the saved "collapse state"
   -# Checks the results are as expected
   -# Cleans up

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_Category(struct mapitest *mt)
{
	mapi_object_t		obj_htable;
	mapi_object_t		obj_test_folder;
	struct mt_common_tf_ctx	*context;
	uint32_t		count = 0;
	uint32_t		origcount = 0;
	bool			ret = true;
	struct SSortOrderSet	criteria;
	uint64_t		inst_id = 0;
	uint64_t		inst_num = 0;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	uint32_t                rowcount = 0;
	uint32_t		Numerator = 0;
	uint32_t		Denominator = 0;
	struct SBinary_short	collapseState;

	/* Step 1. Logon */
	if (! mapitest_common_setup(mt, &obj_htable, &count)) {
		return false;
	}

	/* Step 2. Get the test folder */
	context = mt->priv;
	mapi_object_init(&(obj_test_folder));
	GetContentsTable(&(context->obj_test_folder), &(obj_test_folder), 0, &origcount);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "GetContentsTable");
		ret = false;
		goto cleanup;
	}
	if (origcount != 10) {
		mapitest_print(mt, "* %-35s: unexpected count (%i)\n", "GetContentsTable", count);
		/* This isn't a hard error for this test though, because it might be from a 
		   previous test failure. Clean up and try again */
	}

	/* We need the header row InstanceId to fold/unfold the headers */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x6,
					  PR_SENDER_NAME,
					  PR_BODY,
					  PR_LAST_MODIFICATION_TIME,
					  PR_SUBJECT,
					  PR_INST_ID,
					  PR_INSTANCE_NUM);
	SetColumns(&(obj_test_folder), SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	mapitest_print_retval(mt, "SetColumns");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Apply a categorised sort */
	memset(&criteria, 0x0, sizeof (struct SSortOrderSet));
	criteria.cSorts = 1;
	criteria.cCategories = 1;
	criteria.cExpanded = 1;
	criteria.aSort = talloc_array(mt->mem_ctx, struct SSortOrder, criteria.cSorts);
	criteria.aSort[0].ulPropTag = PR_SENDER_NAME;
	criteria.aSort[0].ulOrder = TABLE_SORT_ASCEND;
	SortTable(&(obj_test_folder), &criteria);
	mapitest_print_retval(mt, "SortTable");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	rowcount =  2 * origcount;
	QueryRows(&(obj_test_folder), rowcount, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet);
	mapitest_print_retval(mt, "QueryRows");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Checks the results are as expected */
	QueryPosition(&(obj_test_folder), &Numerator, &Denominator);
	mapitest_print_retval(mt, "QueryPosition");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	/* the categories are expanded, and there are six unique senders, so there are six
	   extra rows - one for each header row */
	if (Denominator != origcount + 6) {
		mapitest_print(mt, "* %-35s: unexpected count (%i)\n", "QueryPosition", Numerator);
		ret = false;
		goto cleanup;
	}

	/* save away ID/instance values for first row header */
	inst_id = (*(const uint64_t *)get_SPropValue_data(&(SRowSet.aRow[0].lpProps[4])));
	inst_num = (*(const uint32_t *)get_SPropValue_data(&(SRowSet.aRow[0].lpProps[5])));

	/* Collapse a row header */
	CollapseRow(&(obj_test_folder), inst_id, &rowcount);
	mapitest_print_retval(mt, "CollapseRow");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Checks the results are as expected */
	QueryPosition(&(obj_test_folder), &Numerator, &Denominator);
	mapitest_print_retval(mt, "QueryPosition");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	/* there are still six unique headers, but half of the real entries are under the first
	   header (usually 10, unless we have some other rubbish hanging around), and when we
	   collapse the first header row, that half disappear */
	if (Denominator != origcount/2 + 6) {
		mapitest_print(mt, "* %-35s: unexpected count (%i)\n", "QueryPosition", Denominator);
		ret = false;
		goto cleanup;
	}

	/* Save the table collapse state */
	GetCollapseState(&(obj_test_folder), inst_id, inst_num, &collapseState);
	mapitest_print_retval(mt, "GetCollapseState");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}


	/* Expand a category */
	ExpandRow(&(obj_test_folder), inst_id, 20, &SRowSet, &rowcount);
	mapitest_print_retval(mt, "ExpandRow");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Checks the results are as expected */
	QueryPosition(&(obj_test_folder), &Numerator, &Denominator);
	mapitest_print_retval(mt, "QueryPosition");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	/* we've expanded the first header row, so we now get all the entries plus the 6 headers */
	if (Denominator != origcount + 6) {
		mapitest_print(mt, "* %-35s: unexpected count (%i)\n", "QueryPosition", Denominator);
		ret = false;
		goto cleanup;
	}

	/* Restore the collapse state  */
	SetCollapseState(&(obj_test_folder), &collapseState);
	mapitest_print_retval(mt, "SetCollapseState");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Checks the results are as expected */
	QueryPosition(&(obj_test_folder), &Numerator, &Denominator);
	mapitest_print_retval(mt, "QueryPosition");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	/* back to the situation with the first heading collapsed */
	if (Denominator != origcount/2 + 6) {
		mapitest_print(mt, "* %-35s: unexpected count (%i)\n", "QueryPosition", Denominator);
		ret = false;
		goto cleanup;
	}

 cleanup:
	/* Release */
	mapi_object_release(&obj_htable);
	mapi_object_release(&(obj_test_folder));
	mapitest_common_cleanup(mt);

	return ret;
}
