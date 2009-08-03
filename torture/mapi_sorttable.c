/*
   OpenChange MAPI torture suite implementation.

   Tables related operations torture

   Copyright (C) Julien Kerihuel 2008.

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
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>

bool torture_rpc_mapi_sorttable(struct torture_context *torture)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_ctable;
	mapi_id_t		id_folder;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	struct SSortOrderSet	criteria;
	struct mapi_session	*session;
	uint32_t		count;
	uint32_t		i;

	/* init torture test */
	mem_ctx = talloc_named(NULL, 0, "torture_rpc_mapi_sorttable");
	if ((session = torture_init_mapi(mem_ctx, torture->lp_ctx)) == NULL) return false;

	/* Open Message Store*/
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(session, &obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Retrieve Inbox folder ID */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderInbox);
	mapi_errstr("GetDefaultFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Open Inbox Folder */
	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Retrieve the Contents Table */
	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(&obj_folder, &obj_ctable, 0, &count);
	mapi_errstr("GetContentsTable", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Customize the contents table view */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2, PR_SUBJECT, PR_LAST_MODIFICATION_TIME);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	mapi_errstr("SetColumns", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Browse and print table content */
	printf("\nBefore SortTable ASCENDING:\n");
	while (((retval = QueryRows(&obj_ctable, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND) &&
	       SRowSet.cRows) {
		count -= SRowSet.cRows;
		
		for (i = 0; i < SRowSet.cRows; i++) {
			printf("\t[%d] %s\n", i, SRowSet.aRow[i].lpProps[0].value.lpszA);
		}
	}

	/* SortTable */
	memset(&criteria, 0x0, sizeof (struct SSortOrderSet));
	criteria.cSorts = 1;
	criteria.aSort = talloc_array(mem_ctx, struct SSortOrder, criteria.cSorts);
	criteria.aSort[0].ulPropTag = PR_LAST_MODIFICATION_TIME;
	criteria.aSort[0].ulOrder = TABLE_SORT_ASCEND;
	retval = SortTable(&obj_ctable, &criteria);
	mapi_errstr("SortTable", GetLastError());

	/* SeekRow at beginning */
	retval = SeekRow(&obj_ctable, BOOKMARK_BEGINNING, 0, &count);
	mapi_errstr("SeekRow", GetLastError());

	/* Browse and print table content */
	printf("\nAfter SortTable ASCENDING:\n");
	while (((retval = QueryRows(&obj_ctable, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND) &&
	       SRowSet.cRows) {
		count -= SRowSet.cRows;
		
		for (i = 0; i < SRowSet.cRows; i++) {
			printf("\t[%d] %s\n", i, SRowSet.aRow[i].lpProps[0].value.lpszA);
		}
	}


	mapi_object_release(&obj_ctable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}
