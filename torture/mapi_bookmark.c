/*
   OpenChange MAPI torture suite implementation.

   Bookmark related operations torture

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
#include <libmapi/defs_private.h>
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>


bool torture_rpc_mapi_bookmark(struct torture_context *torture)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_ctable;
	mapi_id_t		id_folder;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	struct mapi_session	*session;
	uint32_t		bookmark;
	const char		*subject = NULL;
	uint64_t		mid = 0;
	uint32_t		row;
	uint32_t		count;
	uint32_t		i = 0;
	uint32_t		halfcount;

	/* init torture test */
	mem_ctx = talloc_named(NULL, 0, "torture_rpc_mapi_bookmark");
	if ((session = torture_init_mapi(mem_ctx, torture->lp_ctx)) == NULL) return false;

	/* Open Message Store */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(session, &obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Retrieve Inbox folder ID */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderInbox);
	mapi_errstr("GetDefaultFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Open Inbox folder */
	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Retrieve the Contents Table */
	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(&obj_folder, &obj_ctable, 0, &count);
	mapi_errstr("GetContentsTable", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Customize the table view */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2, PR_MID, PR_SUBJECT);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	mapi_errstr("SetColumns", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	halfcount = count / 2;

	/* Position the cursor at the desired position */
	retval = SeekRow(&obj_ctable, BOOKMARK_BEGINNING, halfcount, &row);
	mapi_errstr("SeekRow: BOOKMARK_BEGINNING + offset", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Fetch the associated row */
	retval = QueryRows(&obj_ctable, 1, TBL_NOADVANCE, &SRowSet);
	mapi_errstr("QueryRows", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Create the Bookmark */
	mid = SRowSet.aRow[i].lpProps[0].value.d;
	subject = SRowSet.aRow[i].lpProps[1].value.lpszA;

	retval = CreateBookmark(&obj_ctable, &bookmark);
	mapi_errstr("CreateBookmark", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Position the cursor at the beginning of the table */
	retval = SeekRow(&obj_ctable, BOOKMARK_BEGINNING, 0, &row);
	mapi_errstr("SeekRow: BOOKMARK_BEGINNING", GetLastError());

	/* SeekRowBookmark */
	retval = SeekRowBookmark(&obj_ctable, bookmark, 0, &row);
	mapi_errstr("SeekRowBookmark", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	
	/* QueryRows a single column */
	retval = QueryRows(&obj_ctable, 1, TBL_NOADVANCE, &SRowSet);
	mapi_errstr("QueryRows", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	
	if (SRowSet.cRows == 0) {
		DEBUG(0, ("QueryRows returned no rows\n"));
		return false;
	}
	
	DEBUG(0, ("[1] mid: %"PRIx64", subject = %s\n", mid, subject));
	DEBUG(0, ("[2] mid: %"PRIx64", subject = %s\n", SRowSet.aRow[0].lpProps[0].value.d,
		  SRowSet.aRow[0].lpProps[1].value.lpszA));

	if (mid == SRowSet.aRow[0].lpProps[0].value.d) {
		DEBUG(0, ("[SUCCESS] Message ID are the same\n"));
	} else {
		DEBUG(0, ("[FAILURE] Message ID are different\n"));
		return false;
	}

	/* Free the bookmark */
	retval = FreeBookmark(&obj_ctable, bookmark);
	mapi_errstr("FreeBookmark", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_release(&obj_ctable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);
	return true;
}
