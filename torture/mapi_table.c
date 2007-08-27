/*
   OpenChange MAPI implementation testsuite

   Tables related operations torture

   Copyright (C) Julien Kerihuel 2007
   Copyright (C) Fabien Le Mentec 2007
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#include <libmapi/libmapi.h>
#include <gen_ndr/ndr_exchange.h>
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture/torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>
#include "oc_test.h"



static bool torture_table(mapi_object_t *obj_folder)
{
	uint32_t	count;
	struct SRowSet	rows;
	struct SPropTagArray cols;
	struct SPropTagArray *props;
	enum MAPISTATUS	status;
	TALLOC_CTX	*mem;
	int		ifolder;
	mapi_object_t	obj_subfolder;
	char		*name;
	int		error;
	mapi_object_t	obj_table;

	
	mem = talloc_init("local");


	/* fills in rows
	 */
	oc_test_describe("CreateFolder");
	error = 0;
	for (ifolder = 0; error == 0 && ifolder < 7; ifolder++) {
		name = talloc_asprintf(mem, "%d", ifolder);
		status = CreateFolder(obj_folder, name, "0", &obj_subfolder);
		if (status != MAPI_E_SUCCESS) error = -1;
	}
	oc_test_assert(error == 0);


	/* HierarchyTable
	 */
	oc_test_describe("GetHierarchyTable");
	mapi_object_init(&obj_table);
	status = GetHierarchyTable(obj_folder, &obj_table);
	oc_test_assert(status == MAPI_E_SUCCESS);


	/* GetRowCount()
	 */
	oc_test_describe("GetRowCount");
	status = GetRowCount(&obj_table, &count);
	oc_test_assert(status == MAPI_E_SUCCESS);
	oc_test_assert(count >= 3);


	/* QueryRows(TBL_NOADVANCE)
	 */
	oc_test_describe("QueryRows(TBL_NOADVANCE)");
	status = QueryRows(&obj_table, count, TBL_NOADVANCE, &rows);
	oc_test_assert(status == MAPI_E_SUCCESS);


	/* QueryRows(TBL_ADVANCE)
	 */
	oc_test_describe("QueryRows(TBL_ADVANCE)");
	count = rows.cRows;
	if (count) {
		status = QueryRows(&obj_table, count, TBL_ADVANCE, &rows);
		oc_test_assert(status == MAPI_E_SUCCESS);
		oc_test_assert(count == rows.cRows);
	}
	else {
		oc_test_skip(2);
	}


	/* SeekRow(BOOKMARK_BEGINNING)
	 */
	oc_test_describe("SeekRow(BOOKMARK_BEGINNING)");
	status = SeekRow(&obj_table, BOOKMARK_BEGINNING, 0, &count);
	oc_test_assert(status == MAPI_E_SUCCESS);

	status = QueryRows(&obj_table, 1, TBL_ADVANCE, &rows);
	oc_test_assert(status == MAPI_E_SUCCESS);

	oc_test_assert(rows.cRows == 1);


	/* SeekRow(BOOKMARK_END)
	 */
	oc_test_describe("SeekRow(BOOKMARK_END)");
	status = SeekRow(&obj_table, BOOKMARK_END, -2, &count);
	oc_test_assert(status == MAPI_E_SUCCESS);

	status = QueryRows(&obj_table, 0xa, TBL_ADVANCE, &rows);
	oc_test_assert(status == MAPI_E_SUCCESS);

	oc_test_assert(rows.cRows == 2);


	/* SeekRow(BOOKMARK_CURRENT)
	 */
	oc_test_describe("SeekRow(BOOKMARK_CURRENT)");
	status = SeekRow(&obj_table, BOOKMARK_END, -2, &count);
	oc_test_assert(status == MAPI_E_SUCCESS);

	status = SeekRow(&obj_table, BOOKMARK_CURRENT, -1, &count);
	status = QueryRows(&obj_table, 0xa, TBL_ADVANCE, &rows);
	oc_test_assert(status == MAPI_E_SUCCESS);

	oc_test_assert(rows.cRows == 3);


	/* QueryColumns()
	 */
	oc_test_describe("QueryColumns()");
	status = QueryColumns(&obj_table, &cols);
	oc_test_assert(status == MAPI_E_SUCCESS);


	/* SetColumns()
	 */
	oc_test_describe("SetColumns()");
	props = set_SPropTagArray(mem, 2, PR_DISPLAY_TO, PR_DISPLAY_CC);
	status = SetColumns(&obj_table, props);
	oc_test_assert(status == MAPI_E_SUCCESS);

	mapi_object_release(&obj_table);

	return True;
}


bool torture_rpc_mapi_table(struct torture_context *torture)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	bool			ret = True;
	mapi_object_t		obj_store;
	mapi_object_t		obj_inbox;
	mapi_id_t		id_inbox;
	struct mapi_session	*session;

	/* init torture */
	mem_ctx = talloc_init("torture_rpc_mapi_table");

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx)) == NULL) return False;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_inbox);

	/* session::OpenMsgStore() */
	retval = OpenMsgStore(&obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;
	mapi_object_debug(&obj_store);

	/* id_inbox = store->GeInboxFolder() */
	retval = GetReceiveFolder(&obj_store, &id_inbox);
	mapi_errstr("GetReceiveFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* inbox = store->OpenFolder(id_inbox) */
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;
	mapi_object_debug(&obj_inbox);


	/* test mapi table */
	oc_test_begin();
	ret = torture_table(&obj_inbox);
	oc_test_end();


	/* objects->Release()
	 */
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);

	/* uninitialize mapi
	 */
	MAPIUninitialize();
	talloc_free(mem_ctx);

	return ret;
}
