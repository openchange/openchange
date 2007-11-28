/*
   OpenChange MAPI torture suite implementation.

   Folder related operations torture

   Copyright (C) Julien Kerihuel 2007.
   Copyright (C) Fabien Le Mentec 2007.

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
#include <gen_ndr/ndr_exchange.h>
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture/torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>
#include "oc_test.h"


static bool torture_folder(mapi_object_t *obj_parent)
{
	enum MAPISTATUS	status;
	TALLOC_CTX	*mem;
	mapi_object_t	obj_child;
	mapi_object_t	obj_sub;
	mapi_object_t	obj_table;
	uint32_t	cn_rows;

	
	mem = talloc_init("local");

	/* CreateFolder
	 */
	oc_test_describe("CreateFolder(parent)");
	status = CreateFolder(obj_parent, FOLDER_GENERIC, "torture_folder_name", "torture_folder_comment", 0, &obj_child);
	oc_test_assert(status == MAPI_E_SUCCESS);

	mapi_object_debug(&obj_child);


	/* Create sub Folder
	 */
	oc_test_describe("CreateFolder(child)");
	status = CreateFolder(&obj_child, FOLDER_GENERIC, "torture_subfolder_name", "torture_subfolder_comment", 0, &obj_sub);
	oc_test_assert(status == MAPI_E_SUCCESS);


	/* EmptyFolder
	 */
	oc_test_describe("EmptyFolder");
	status = EmptyFolder(&obj_child);
	oc_test_assert(status == MAPI_E_SUCCESS);
	
	/* look for remaining entries */
	mapi_object_init(&obj_table);
	oc_test_describe("GetHierarchyTable");
	status = GetHierarchyTable(&obj_child, &obj_table);
	oc_test_assert(status == MAPI_E_SUCCESS);

	oc_test_describe("GetRowCount");
	status = GetRowCount(&obj_table, &cn_rows);
	oc_test_assert(status == MAPI_E_SUCCESS);

	/* remaining entries, EmptyFolder didn't work */
	oc_test_describe("EmptyFolder");
	oc_test_assert(cn_rows == 0);


	/* DeleteFolder
	 */
	oc_test_describe("DeleteFolder");
	status = DeleteFolder(obj_parent, mapi_object_get_id(&obj_child));
	oc_test_assert(status == MAPI_E_SUCCESS);


	/* Release
	 */
	mapi_object_release(&obj_table);
	mapi_object_release(&obj_sub);
	mapi_object_release(&obj_child);
	
	oc_test_describe("Release");
	oc_test_step();

	return true;
}


bool torture_rpc_mapi_folder(struct torture_context *torture)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_inbox;
	mapi_id_t		id_inbox;
	struct mapi_session	*session;


	/* init torture */
	mem_ctx = talloc_init("torture_rpc_mapi_folder");

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx)) == NULL) return false;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_inbox);

	/* session::OpenMsgStore() */
	retval = OpenMsgStore(&obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	mapi_object_debug(&obj_store);

	/* id_inbox = store->GeInboxFolder() */
	retval = GetReceiveFolder(&obj_store, &id_inbox);
	mapi_errstr("GetReceiveFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* inbox = store->OpenFolder(id_inbox) */
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	mapi_object_debug(&obj_inbox);

	oc_test_begin();
	ret = torture_folder(&obj_inbox);
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
