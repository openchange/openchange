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
   \details Test the SetColumns (0x12) operation

   This function:
   -# Opens the Inbox folder
   -# Retrieves the hierarchy table
   -# Calls the SetColumns operation

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_SetColumns(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	struct SPropTagArray	*SPropTagArray;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Top Information Store folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_folder, 
					  olFolderTopInformationStore);
	if (ret == false) {
		return false;
	}

	/* Step 3. GetHierarchyTable */
	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, NULL);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. SetColumns */
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

	/* Release */
	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the QueryColumns (0x37) operation

   This function:
	-# Opens the Inbox folder
	-# Retrieves the hierarchy table
	-# Calls the QueryColumn operation

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_QueryColumns(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	struct SPropTagArray	columns;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Top Information Store folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_folder, 
					  olFolderTopInformationStore);
	if (ret == false) {
		return false;
	}

	/* Step 3. GetHierarchyTable */
	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, NULL);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step4. QueryColumns */
	retval = QueryColumns(&obj_htable, &columns);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "QueryColumns", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);
	
	return true;
}


/**
   \details Test the QueryRows (0x15) operation

   This function:
   -# Open the Inbox folder
   -# Retrieve the hierarchy table
   -# Retrieve the number of rows
   -# Set the number of columns
   -# Call QueryRows until the end of the table

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_QueryRows(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	struct SRowSet		SRowSet;
	struct SPropTagArray	*SPropTagArray;
	uint32_t		idx = 0;
	uint32_t		count = 0;
	
	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Top Information Store folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_folder, 
					  olFolderTopInformationStore);
	if (ret == false) {
		return false;
	}

	/* Step 3. GetHierarchyTable */
	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, &count);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Set Table Columns */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SetColumns", GetLastError());

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


	/* Release */
	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}

/**
   \details Test the GetStatus (0x16) operation

   This function:
   -# Open the Top Information store folder
   -# Open the hiearchy tabke
   -# Call GetStatus
 */
_PUBLIC_ bool mapitest_oxctable_GetStatus(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	uint8_t			TableStatus;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Top Information Store folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_folder, 
					  olFolderTopInformationStore);
	if (ret == false) {
		return false;
	}

	/* Step 3. GetHierarchyTable */
	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, NULL);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. GetStatus */
	retval = GetStatus(&obj_htable, &TableStatus);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetStatus", GetLastError());
	mapitest_print(mt, "* %-35s: TableStatus: %d\n", "GetStatus", TableStatus);
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Release */
	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_htable);

	return ret;
}


/**
   \details Test the SeekRow (0x18) operation

   This function:
   -# Open the Inbox folder
   -# Retrieve the hierarchy table
   -# SeekRow with BOOKMARK_BEGINNING
   -# SeekRow with BOOKMARK_END
   -# SeekRow with BOOKMARK_CURRENT

   \param mt pointer on the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_SeekRow(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	uint32_t		count;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Top Information Store folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_folder, 
					  olFolderTopInformationStore);
	if (ret == false) {
		return false;
	}

	/* Step 3. GetHierarchyTable */
	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, NULL);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. SeekRow */
	retval = SeekRow(&obj_htable, BOOKMARK_BEGINNING, 0, &count);
	mapitest_print(mt, "* %-35s: BOOKMARK_BEGINNING 0x%.8x\n", "SeekRow", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	retval = SeekRow(&obj_htable, BOOKMARK_END, 0, &count);
	mapitest_print(mt, "* %-35s: BOOKMARK_END 0x%.8x\n", "SeekRow", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	retval = SeekRow(&obj_htable, BOOKMARK_CURRENT, 0, &count);
	mapitest_print(mt, "* %-35s: BOOKMARK_CURRENT 0x%.8x\n", "SeekRow", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the SeekRowApprox (0x1a) operation

   This function:
   -# Open the Inbox folder
   -# Retrieve the hierarchy table
   -# SeekRowApprox with 0/1, 1/1 and 1/2 fractional values

   \param mt pointer on the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_SeekRowApprox(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Top Information Store folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_folder, 
					  olFolderTopInformationStore);
	if (ret == false) {
		return false;
	}

	/* Step 3. GetHierarchyTable */
	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, NULL);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. SeekRowApprox */
	retval = SeekRowApprox(&obj_htable, 0, 1);
	mapitest_print(mt, "* %-35s 0/1: 0x%.8x\n", "SeekRowApprox", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	retval = SeekRowApprox(&obj_htable, 1, 1);
	mapitest_print(mt, "* %-35s 1/1: 0x%.8x\n", "SeekRowApprox", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	retval = SeekRowApprox(&obj_htable, 1, 2);
	mapitest_print(mt, "* %-35s 1/2: 0x%.8x\n", "SeekRowApprox", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}



/**
   \details Test the CreateBookmark (0x1b) operation

   This function:
   -# Open the Inbox folder
   -# Retrieve the hierarchy table
   -# Customize the MAPI table view
   -# CreateBookmark for each table's row
   -# Free Bookmark for each created bookmark

   \param mt pointer on the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_CreateBookmark(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	uint32_t		*bkPosition;
	uint32_t		count;
	uint32_t		i;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Top Information Store folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_folder, 
					  olFolderTopInformationStore);
	if (ret == false) {
		return false;
	}

	/* Step 3. GetHierarchyTable */
	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, &count);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Customize the table view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 5. Create Bookmarks */
	bkPosition = talloc_array(mt->mem_ctx, uint32_t, 1);
	while (((retval = QueryRows(&obj_htable, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND) &&
	       SRowSet.cRows) {
		count -= SRowSet.cRows;
		for (i = 0; i < SRowSet.cRows; i++) {
			bkPosition = talloc_realloc(mt->mem_ctx, bkPosition, uint32_t, i + 2);
			retval = CreateBookmark(&obj_htable, &(bkPosition[i]));
			mapitest_print(mt, "* %-35s (%.2d): 0x%.8x\n", "CreateBookmark", i, GetLastError());
			if (GetLastError() != MAPI_E_SUCCESS) {
				return false;
			}
		}
	}

	retval = mapi_object_bookmark_get_count(&obj_htable, &count);

	/* Step 6. Free Bookmarks */
	for (i = 0; i < count; i++) {
		retval = FreeBookmark(&obj_htable, bkPosition[i]);
		mapitest_print(mt, "* %-35s (%.2d): 0x%.8x\n", "FreeBookmark", i, GetLastError());
		if (GetLastError() != MAPI_E_SUCCESS) {
			return false;
		}
	}

	/* Release */
	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	talloc_free(bkPosition);

	return true;
}



/**
   \details Test the SeekRowBookmark (0x19) operation

   This function:
   -# Open the Inbox folder
   -# Retrieve the hierarchy table
   -# Customize the MAPI table view
   -# SeekBookmark for each table's row
   -# Free Bookmark for each created bookmark

   \param mt pointer on the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxctable_SeekRowBookmark(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	uint32_t		*bkPosition;
	uint32_t		count;
	uint32_t		row;
	uint32_t		i;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Top Information Store folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_folder, 
					  olFolderTopInformationStore);
	if (ret == false) {
		return false;
	}

	/* Step 3. GetHierarchyTable */
	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, &count);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Customize the table view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 5. Create Bookmarks */
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

	/* Step 6. SeekRowBookmark */
	for (i = 0; i < count; i++) {
		retval = SeekRowBookmark(&obj_htable, bkPosition[i], 0, &row);
		mapitest_print(mt, "* %-35s (%.2d): 0x%.8x\n", "SeekRowBookmark", i, GetLastError());
	}


	/* Step 8. Free Bookmarks */
	for (i = 0; i < count; i++) {
		retval = FreeBookmark(&obj_htable, bkPosition[i]);
		if (GetLastError() != MAPI_E_SUCCESS) {
			return false;
		}
	}
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "FreeBookmark", 0);

	/* Release */
	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	talloc_free(bkPosition);

	return true;
}
