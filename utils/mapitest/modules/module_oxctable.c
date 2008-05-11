/*
   Stand-alone MAPI testsuite

   OpenChange Project - TABLE OBJECT PROTOCOL operations

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
   \file module_oxctable.c

   \brief Table Object Protocol test suite
*/

/**
   Context for OXCTABL unit tests
*/
struct mt_oxctabl_ctx
{
	mapi_object_t	obj_store;
	mapi_object_t	obj_top_folder;
	mapi_object_t	obj_test_folder;
	mapi_object_t	obj_test_msg[10];
};


_PUBLIC_ bool mapitest_create_filled_test_folder(struct mapitest *mt)
{
	struct mt_oxctabl_ctx	*context;
	enum MAPISTATUS		retval;
	const char		*from = NULL;
	const char		*subject = NULL;
	const char		*body = NULL;
	struct SPropValue	lpProp[2];
	int 			i;

	context = mt->priv;

	/* Create test folder */
	mapi_object_init(&(context->obj_test_folder));
        retval = CreateFolder(&(context->obj_top_folder), FOLDER_GENERIC,
			      MT_DIRNAME_TEST, NULL,
                              OPEN_IF_EXISTS, &(context->obj_test_folder));
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "Create the test folder", GetLastError());
		return false;
	}

	/* Create 5 test messages in the test folder with the same subject */
	for (i = 0; i < 5; ++i) {
		mapi_object_init(&(context->obj_test_msg[i]));
		retval = mapitest_common_message_create(mt, &(context->obj_test_folder),
							&(context->obj_test_msg[i]), MT_MAIL_SUBJECT);
		if (GetLastError() != MAPI_E_SUCCESS) {
			mapitest_print(mt, "* %-35s: 0x%.8x\n", "Create test message", GetLastError());
			return false;
		}

		from = talloc_asprintf(mt->mem_ctx, "[MT] Dummy%i", i);
		set_SPropValue_proptag(&lpProp[0], PR_SENDER_NAME, (const void *)from);
		body = talloc_asprintf(mt->mem_ctx, "Body of message %i", i);
		set_SPropValue_proptag(&lpProp[1], PR_BODY, (const void *)body);
		retval = SetProps(&(context->obj_test_msg[i]), lpProp, 2);
		MAPIFreeBuffer((void *)from);
		MAPIFreeBuffer((void *)body);
		if (retval != MAPI_E_SUCCESS) {
			mapitest_print(mt, "* %-35s: 0x%.8x\n", "Set props on message", GetLastError());
			return false;
		}
		retval = SaveChangesMessage(&(context->obj_test_folder), &(context->obj_test_msg[i]));
		if (retval != MAPI_E_SUCCESS) {
			return false;
		}
	}

	/* Create 5 test messages in the test folder with the same sender */
	for (i = 5; i < 10; ++i) {
		mapi_object_init(&(context->obj_test_msg[i]));
		subject = talloc_asprintf(mt->mem_ctx, "[MT] Subject%i", i);
		retval = mapitest_common_message_create(mt, &(context->obj_test_folder),
							&(context->obj_test_msg[i]), subject);
		if (GetLastError() != MAPI_E_SUCCESS) {
			mapitest_print(mt, "* %-35s: 0x%.8x\n", "Create test message", GetLastError());
			return false;
		}

		from = talloc_asprintf(mt->mem_ctx, "[MT] Dummy From");
		set_SPropValue_proptag(&lpProp[0], PR_SENDER_NAME, (const void *)from);
		body = talloc_asprintf(mt->mem_ctx, "Body of message %i", i);
		set_SPropValue_proptag(&lpProp[1], PR_BODY, (const void *)body);
		retval = SetProps(&(context->obj_test_msg[i]), lpProp, 2);
		MAPIFreeBuffer((void *)from);
		MAPIFreeBuffer((void *)body);
		if (retval != MAPI_E_SUCCESS) {
			mapitest_print(mt, "* %-35s: 0x%.8x\n", "Set props on message", GetLastError());
			return false;
		}
		retval = SaveChangesMessage(&(context->obj_test_folder), &(context->obj_test_msg[i]));
		if (retval != MAPI_E_SUCCESS) {
			return false;
		}
	}

	return true;
};

/**
   Convenience function to login to the server

   This functions logs into the server, gets the top level store, and
   gets the hierachy table for the top level store (which is returned as
   obj_htable). It also creates a test folder with 10 test messages.

   \param mt pointer to the top-level mapitest structure
   \param obj_htable the hierachy table for the top level store
   \param count the number of rows in the top level hierarchy table

   \return true on success, otherwise false
*/
_PUBLIC_ bool mapitest_oxctable_setup(struct mapitest *mt, mapi_object_t *obj_htable, uint32_t *count)
{
	bool			ret = false;
	struct mt_oxctabl_ctx	*context;

	context = talloc(mt->mem_ctx, struct mt_oxctabl_ctx);
	mt->priv = context;

	mapi_object_init(&(context->obj_store));
	OpenMsgStore(&(context->obj_store));
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&(context->obj_top_folder));
	ret = mapitest_common_folder_open(mt, &(context->obj_store), &(context->obj_top_folder), 
					  olFolderTopInformationStore);
	if (ret == false) {
		return false;
	}

	/* We do this before getting the hierachy table, because otherwise the new
	   test folder will be omitted, and the count will be wrong */
	ret = mapitest_create_filled_test_folder(mt);
	if (ret == false) {
		return false;
	}

	mapi_object_init(obj_htable);
	GetHierarchyTable(&(context->obj_top_folder), obj_htable, 0, count);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	return true;
}

/**
   Convenience function to clean up after logging into the server

   This functions cleans up after a mapitest_oxctable_setup() call

   \param mt pointer to the top-level mapitest structure
*/
_PUBLIC_ void mapitest_oxctable_cleanup(struct mapitest *mt)
{
	struct mt_oxctabl_ctx	*context;
	int			i;

	context = mt->priv;

	for (i = 0; i<10; ++i) {
		mapi_object_release(&(context->obj_test_msg[i]));
	}

	EmptyFolder(&(context->obj_test_folder));
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "Empty test folder", GetLastError());
	}

	DeleteFolder(&(context->obj_top_folder), mapi_object_get_id(&(context->obj_test_folder)));
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "Delete test folder", GetLastError());
	}

	mapi_object_release(&(context->obj_test_folder));
	mapi_object_release(&(context->obj_top_folder));
	mapi_object_release(&(context->obj_store));

	talloc_free(mt->priv);
}

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
	enum MAPISTATUS		retval;
	mapi_object_t		obj_htable;
	struct SPropTagArray	*SPropTagArray;

	/* Step 1. Logon */
	if (! mapitest_oxctable_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	/* Step 2. SetColumns */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SetColumns", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Release */
	mapi_object_release(&obj_htable);
	mapitest_oxctable_cleanup(mt);

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
	enum MAPISTATUS		retval;
	mapi_object_t		obj_htable;
	mapi_object_t		obj_test_folder;
	struct SPropTagArray	columns;
	struct SPropTagArray	*SPropTagArray;
	struct mt_oxctabl_ctx	*context;
	uint32_t		count;

	/* Step 1. Logon */
	if (! mapitest_oxctable_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	/* Step 2. QueryColumns */
	retval = QueryColumns(&obj_htable, &columns);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "QueryColumns", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Get the test folder */
	context = mt->priv;
	mapi_object_init(&(obj_test_folder));
	GetContentsTable(&(context->obj_test_folder), &(obj_test_folder), 0, &count);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetContentsTable", GetLastError());
		return false;
	}
	if (count != 10) {
		mapitest_print(mt, "* %-35s: unexpected count (%i)\n", "GetContentsTable", count);
		/* This isn't a hard error for this test though, because it might be from a 
		   previous test failure. Clean up and try again */
	}


	/* Step 4. SetColumns */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&(obj_test_folder), SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "SetColumns", GetLastError());
		return false;
	}

	/* Step 5. QueryColumns on a contents folder */
	retval = QueryColumns(&(obj_test_folder), &columns);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "QueryColumns", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	/* TODO: check the return count against something */
	mapitest_print(mt, "column count: %i\n", columns.cValues);

	/* Step 6. Release */
	mapi_object_release(&obj_htable);
	mapi_object_release(&(obj_test_folder));
	mapitest_oxctable_cleanup(mt);

	return true;
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
	struct mt_oxctabl_ctx	*context;
	uint32_t		idx = 0;
	uint32_t		count = 0;
	const char*		data;
	int			i;

	/* Step 1. Logon */
	if (! mapitest_oxctable_setup(mt, &obj_htable, &count)) {
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
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "SetColumns2", GetLastError());
		return false;
	}

	/* Step 3. QueryRows */
	do {
		retval = QueryRows(&obj_htable, 0x2, TBL_ADVANCE, &SRowSet);
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
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetContentsTable", GetLastError());
		return false;
	}
	if (count != 10) {
		mapitest_print(mt, "* %-35s: unexpected count (%i)\n", "GetContentsTable", count);
		/* This isn't a hard error for this test though, because it might be from a 
		   previous test failure. Clean up and try again */
	}

	/* Step 5. Set Table Columns on the test folder */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_BODY, PR_BODY_HTML);
	retval = SetColumns(&obj_test_folder, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "SetColumns5", GetLastError());
		return false;
	}

	/* Step 6. QueryRows on test folder contents */
	idx = 0;
	do {
		retval = QueryRows(&(obj_test_folder), 0x2, TBL_ADVANCE, &SRowSet);
		if (SRowSet.cRows > 0) {
			idx += SRowSet.cRows;
			if (retval == MAPI_E_SUCCESS) {
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
					if (lpProp.ulPropTag != PR_BODY_HTML) {
						mapitest_print(mt, "* %-35s: Bad proptag1 (0x%x)\n", 
							       "QueryRows", lpProp.ulPropTag);
						return false;
					}
					data = get_SPropValue_data(&lpProp);
					if (0 != strncmp(data, "<!DOCTYPE HTML PUBLIC", 21)) {
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
	mapitest_oxctable_cleanup(mt);

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
	enum MAPISTATUS		retval;
	bool			ret = true;
	mapi_object_t		obj_htable;
	uint8_t			TableStatus;

	/* Step 1. Logon */
	if (! mapitest_oxctable_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	/* Step 2. GetStatus */
	retval = GetStatus(&obj_htable, &TableStatus);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetStatus", GetLastError());
	mapitest_print(mt, "* %-35s: TableStatus: %d\n", "GetStatus", TableStatus);
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 3. Release */
	mapi_object_release(&obj_htable);
	mapitest_oxctable_cleanup(mt);

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
	enum MAPISTATUS		retval;
	mapi_object_t		obj_htable;
	uint32_t		count;

	/* Step 1. Logon */
	if (! mapitest_oxctable_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	/* Step 2. SeekRow */
	retval = SeekRow(&obj_htable, BOOKMARK_BEGINNING, 0, &count);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SeekRow (BOOKMARK_BEGINNING)", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	retval = SeekRow(&obj_htable, BOOKMARK_END, 0, &count);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SeekRow (BOOKMARK_END)", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	retval = SeekRow(&obj_htable, BOOKMARK_CURRENT, 0, &count);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SeekRow (BOOKMARK_CURRENT)", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_htable);
	mapitest_oxctable_cleanup(mt);

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
	enum MAPISTATUS		retval;
	mapi_object_t		obj_htable;

	/* Step 1. Logon */
	if (! mapitest_oxctable_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	/* Step 2. SeekRowApprox */
	retval = SeekRowApprox(&obj_htable, 0, 1);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SeekRowApprox 0/1", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	retval = SeekRowApprox(&obj_htable, 1, 1);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SeekRowApprox 1/1", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	retval = SeekRowApprox(&obj_htable, 1, 2);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SeekRowApprox 1/2", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Release */
	mapi_object_release(&obj_htable);
	mapitest_oxctable_cleanup(mt);

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
	if (! mapitest_oxctable_setup(mt, &obj_htable, &count)) {
		return false;
	}

	/* Step 2. Customize the table view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create Bookmarks */
	bkPosition = talloc_array(mt->mem_ctx, uint32_t, 1);
	while (((retval = QueryRows(&obj_htable, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND) &&
	       SRowSet.cRows) {
		count -= SRowSet.cRows;
		for (i = 0; i < SRowSet.cRows; i++) {
			bkPosition = talloc_realloc(mt->mem_ctx, bkPosition, uint32_t, i + 2);
			retval = CreateBookmark(&obj_htable, &(bkPosition[i]));
			mapitest_print(mt, "* %-30s (%.2d): 0x%.8x\n", "CreateBookmark", i, GetLastError());
			if (GetLastError() != MAPI_E_SUCCESS) {
				return false;
			}
		}
	}

	retval = mapi_object_bookmark_get_count(&obj_htable, &count);

	/* Step 4. Free Bookmarks */
	for (i = 0; i < count; i++) {
		retval = FreeBookmark(&obj_htable, bkPosition[i]);
		mapitest_print(mt, "* %-30s (%.2d): 0x%.8x\n", "FreeBookmark", i, GetLastError());
		if (GetLastError() != MAPI_E_SUCCESS) {
			return false;
		}
	}

	/* Step 5. Release */
	mapi_object_release(&obj_htable);
	mapitest_oxctable_cleanup(mt);
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
	if (! mapitest_oxctable_setup(mt, &obj_htable, &count)) {
		return false;
	}

	/* Step 2. Customize the table view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create Bookmarks */
	bkPosition = talloc_array(mt->mem_ctx, uint32_t, 1);
	while (((retval = QueryRows(&obj_htable, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND) &&
	       SRowSet.cRows) {
		count -= SRowSet.cRows;
		for (i = 0; i < SRowSet.cRows; i++) {
			bkPosition = talloc_realloc(mt->mem_ctx, bkPosition, uint32_t, i + 2);
			retval = CreateBookmark(&obj_htable, &(bkPosition[i]));
			if (GetLastError() != MAPI_E_SUCCESS) {
				return false;
			}
		}
	}
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "CreateBookmark", 0);

	retval = mapi_object_bookmark_get_count(&obj_htable, &count);

	/* Step 4. SeekRowBookmark */
	for (i = 0; i < count; i++) {
		retval = SeekRowBookmark(&obj_htable, bkPosition[i], 0, &row);
		mapitest_print(mt, "* %-30s (%.2d): 0x%.8x\n", "SeekRowBookmark", i, GetLastError());
	}


	/* Step 5. Free Bookmarks */
	for (i = 0; i < count; i++) {
		retval = FreeBookmark(&obj_htable, bkPosition[i]);
		if (GetLastError() != MAPI_E_SUCCESS) {
			return false;
		}
	}
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "FreeBookmark", 0);

	/* Step 6. Release */
	mapi_object_release(&obj_htable);
	mapitest_oxctable_cleanup(mt);

	talloc_free(bkPosition);

	return true;
}
