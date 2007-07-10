/* 
   OpenChange MAPI implementation testsuite

   Test MAPI Permissions

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
#include <torture/torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>

BOOL torture_rpc_mapi_permissions(struct torture_context *torture)
{
	NTSTATUS		ntstatus;
	enum MAPISTATUS		retval;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	struct mapi_session	*session;
	mapi_object_t		obj_store;
	mapi_object_t		obj_inbox;
	mapi_object_t		obj_table;
	mapi_id_t		id_inbox;
	struct SRowSet		SRowSet;
	struct SPropTagArray	*proptags;
	uint32_t		i;
	const char		*operation = lp_parm_string(-1, "mapi", "operation");
	const char		*role = lp_parm_string(-1, "mapi", "role");
	uint32_t		permission;
	const char		*username = lp_parm_string(-1, "mapi", "username");

	/* init torture */
	mem_ctx = talloc_init("torture_rpc_mapi_permissions");
	ntstatus = torture_rpc_connection(mem_ctx, &p, &dcerpc_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(ntstatus)) {
		talloc_free(mem_ctx);
		return False;
	}

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx)) == NULL) return False;

	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* Retrieve the default inbox folder id */
	retval = GetDefaultFolder(&obj_store, &id_inbox, olFolderInbox);
	mapi_errstr("GetDefaultFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	mapi_object_init(&obj_inbox);
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	if (!strncasecmp(operation, "add", strlen(operation))) {
		permission = get_permission_from_name(role);
		retval = AddUserPermission(&obj_inbox, username, permission);
		mapi_errstr("AddUserPermission", GetLastError());
		if (retval != MAPI_E_SUCCESS) return False;
	}
	if (!strncasecmp(operation, "modify", strlen(operation))) {
		permission = get_permission_from_name(role);
		retval = ModifyUserPermission(&obj_inbox, username, permission);
		mapi_errstr("ModifyUserPermission", GetLastError());
		if (retval != MAPI_E_SUCCESS) return False;
	}
	if (!strncasecmp(operation, "remove", strlen(operation))) {
		retval = RemoveUserPermission(&obj_inbox, username);
		mapi_errstr("RemoveUserPermission", GetLastError());
		if (retval != MAPI_E_SUCCESS) return False;
	}
	if (!strncasecmp(operation, "list", strlen(operation))) {
		mapi_object_init(&obj_table);
		retval = GetTable(&obj_inbox, &obj_table);
		mapi_errstr("GetTable", GetLastError());
		if (retval != MAPI_E_SUCCESS) return False;

		proptags = set_SPropTagArray(mem_ctx, 4,
					     PR_MEMBER_ID,
					     PR_MEMBER_NAME,
					     PR_MEMBER_RIGHTS,
					     PR_ENTRYID);
		retval = SetColumns(&obj_table, proptags);
		mapi_errstr("SetColumns", GetLastError());
		MAPIFreeBuffer(proptags);
		if (retval != MAPI_E_SUCCESS) return False;
		
		retval = QueryRows(&obj_table, 0x32, TBL_ADVANCE, &SRowSet);
		mapi_errstr("QueryRows", GetLastError());
		if (retval != MAPI_E_SUCCESS) return False;
		
		for (i = 0; i < SRowSet.cRows; i++) {
			struct SPropValue *lpProp;
			
			lpProp = get_SPropValue_SRow(&(SRowSet.aRow[i]), PR_MEMBER_NAME);
			printf("    %-25s: %s\n", "Username", lpProp->value.lpszA ? lpProp->value.lpszA : "Default");
			lpProp = get_SPropValue_SRow(&(SRowSet.aRow[i]), PR_MEMBER_RIGHTS);
			ndr_print_debug((ndr_print_fn_t)ndr_print_ACLRIGHTS, "Rights", (void *)lpProp->value.l);
			printf("\n");
		}
		
		mapi_object_release(&obj_table);
	}
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);

	MAPIUninitialize();
	talloc_free(mem_ctx);
	return True;
}
