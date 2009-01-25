/*
   OpenChange MAPI torture suite implementation.

   Copy message from a folder to another

   Copyright (C) Julien Kerihuel 2007.

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
#include <torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>

bool torture_rpc_mapi_copymail(struct torture_context *torture)
{
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	struct mapi_session	*session;
	mapi_object_t		obj_store;
	mapi_object_t		obj_dir_src;
	mapi_object_t		obj_dir_dst;
	mapi_object_t		obj_table;
	mapi_id_t		id_src;
	mapi_id_t		id_dst;
	mapi_id_array_t		msg_id_array;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct SRowSet		rowset;
	int			i;

	/* init torture */
	mem_ctx = talloc_named(NULL, 0, "torture_rpc_mapi_copymail");
	status = torture_rpc_connection(torture, &p, &ndr_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return false;
	}
	
	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx, torture->lp_ctx)) == NULL) return false;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_dir_src);
	mapi_object_init(&obj_dir_dst);
	mapi_object_init(&obj_table);

	/* OpenMsgStore */
	retval = OpenMsgStore(session, &obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Get Inbox folder */
	retval = GetDefaultFolder(&obj_store, &id_src, olFolderInbox);
	mapi_errstr("GetDefaultFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	retval = OpenFolder(&obj_store, id_src, &obj_dir_src);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Get Deleted Items folder */
	retval = GetDefaultFolder(&obj_store, &id_dst, olFolderDrafts);
	mapi_errstr("GetDefaultFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	
	retval = OpenFolder(&obj_store, id_dst, &obj_dir_dst);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;


	retval = GetContentsTable(&obj_dir_src, &obj_table, 0, NULL);
	mapi_errstr("GetContentsTable", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

		SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	mapi_errstr("SetColumns", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_id_array_init(&msg_id_array);

	while ((retval = QueryRows(&obj_table, 0xa, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		for (i = 0; i < rowset.cRows; i++) {
			mapi_id_array_add_id(&msg_id_array, rowset.aRow[i].lpProps[1].value.d);
		}
		retval = MoveCopyMessages(&obj_dir_src, &obj_dir_dst, &msg_id_array, 1);
		mapi_errstr("MoveCopyMessages", GetLastError());
		if (retval != MAPI_E_SUCCESS) return false;
	}

	/* release mapi objects
	 */
	mapi_id_array_release(&msg_id_array);
	mapi_object_release(&obj_table);
	mapi_object_release(&obj_dir_src);
	mapi_object_release(&obj_dir_dst);
	mapi_object_release(&obj_store);

	/* uninitialize mapi
	 */
	MAPIUninitialize();
	talloc_free(mem_ctx);

	return true;
}
