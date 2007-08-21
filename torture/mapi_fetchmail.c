/* 
   OpenChange MAPI implementation testsuite

   Fetch mail from an Exchange server

   Copyright (C) Julien Kerihuel 2007
   
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


BOOL torture_rpc_mapi_fetchmail(struct torture_context *torture)
{
	NTSTATUS		nt_status;
	enum MAPISTATUS		retval;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	BOOL			ret = True;
	struct mapi_session	*session;
	mapi_object_t		obj_store;
	mapi_object_t		obj_inbox;
	mapi_object_t		obj_message;
	mapi_object_t		obj_table;
	uint64_t		id_inbox;
	struct SPropTagArray	*SPropTagArray;
	struct mapi_SPropValue_array	properties_array;
	struct SRowSet		rowset;
	uint32_t		i;
	uint32_t		count;
	uint32_t		unread;
	uint32_t		total;


	/* init torture */
	mem_ctx = talloc_init("torture_rpc_mapi_fetchmail");
	nt_status = torture_rpc_connection(mem_ctx, &p, &ndr_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return False;
	}

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx)) == NULL) return False;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_inbox);
	mapi_object_init(&obj_message);
	mapi_object_init(&obj_table);

	/* session::OpenMsgStore()
	 */
	retval = OpenMsgStore(&obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* id_inbox = store->GetReceiveFolder */
	retval = GetReceiveFolder(&obj_store, &id_inbox);
	mapi_errstr("GetReceiveFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* Open Inbox folder */
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* Retrieve message count summary from the folder */
	retval = GetFolderItemsCount(&obj_inbox, &unread, &total);
	mapi_errstr("GetFolderItemsCount", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;
		
	/* table = inbox->GetContentsTable()
	 */
	retval = GetContentsTable(&obj_inbox, &obj_table);
	mapi_errstr("GetContentsTable", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT);
	retval = SetColumns(&obj_table, SPropTagArray);
	talloc_free(SPropTagArray);
	mapi_errstr("SetColumns", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* Iterate through messages
	 */
	retval = GetRowCount(&obj_table, &count);
	mapi_errstr("GetRowCount", GetLastError());

	printf("Inbox: Total(%d) Unread(%d)\n", total, unread);

	while ((retval = QueryRows(&obj_table, 0xa, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		for (i = 0; i < rowset.cRows; i++) {
			retval = OpenMessage(&obj_store,
					     rowset.aRow[i].lpProps[0].value.d,
					     rowset.aRow[i].lpProps[1].value.d,
					     &obj_message);

			if (GetLastError() != MAPI_E_NOT_FOUND) {
			  retval = GetPropsAll(&obj_message, &properties_array);
			  mapidump_message(&properties_array);
			  mapi_object_release(&obj_message);
			}
		}
	}

	/* release mapi objects
	 */
	mapi_object_release(&obj_store);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_table);

	/* uninitialize mapi
	 */
	MAPIUninitialize();
	talloc_free(mem_ctx);
	
	return (ret);
}
