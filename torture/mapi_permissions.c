/*
   OpenChange MAPI torture suite implementation.

   Test MAPI Permissions

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

bool torture_rpc_mapi_permissions(struct torture_context *torture)
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
	const char		*operation = lp_parm_string(torture->lp_ctx, NULL, "mapi", "operation");
	const char		*role = lp_parm_string(torture->lp_ctx, NULL, "mapi", "role");
	uint32_t		permission;
	const char		*username = lp_parm_string(torture->lp_ctx, NULL, "mapi", "username");

	/* init torture */
	mem_ctx = talloc_named(NULL, 0, "torture_rpc_mapi_permissions");
	ntstatus = torture_rpc_connection(torture, &p, &ndr_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(ntstatus)) {
		talloc_free(mem_ctx);
		return false;
	}

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx, torture->lp_ctx)) == NULL) return false;

	mapi_object_init(&obj_store);
	retval = OpenMsgStore(session, &obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Retrieve the default inbox folder id */
	retval = GetDefaultFolder(&obj_store, &id_inbox, olFolderInbox);
	mapi_errstr("GetDefaultFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_init(&obj_inbox);
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	if (!strncasecmp(operation, "add", strlen(operation))) {
		permission = get_permission_from_name(role);
		retval = AddUserPermission(&obj_inbox, username, permission);
		mapi_errstr("AddUserPermission", GetLastError());
		if (retval != MAPI_E_SUCCESS) return false;
	}
	if (!strncasecmp(operation, "modify", strlen(operation))) {
		permission = get_permission_from_name(role);
		retval = ModifyUserPermission(&obj_inbox, username, permission);
		mapi_errstr("ModifyUserPermission", GetLastError());
		if (retval != MAPI_E_SUCCESS) return false;
	}
	if (!strncasecmp(operation, "remove", strlen(operation))) {
		retval = RemoveUserPermission(&obj_inbox, username);
		mapi_errstr("RemoveUserPermission", GetLastError());
		if (retval != MAPI_E_SUCCESS) return false;
	}
	if (!strncasecmp(operation, "list", strlen(operation))) {
		mapi_object_init(&obj_table);
		retval = GetTable(&obj_inbox, &obj_table);
		mapi_errstr("GetTable", GetLastError());
		if (retval != MAPI_E_SUCCESS) return false;

		proptags = set_SPropTagArray(mem_ctx, 4,
					     PR_MEMBER_ID,
					     PR_MEMBER_NAME,
					     PR_MEMBER_RIGHTS,
					     PR_ENTRYID);
		retval = SetColumns(&obj_table, proptags);
		mapi_errstr("SetColumns", GetLastError());
		MAPIFreeBuffer(proptags);
		if (retval != MAPI_E_SUCCESS) return false;
		
		retval = QueryRows(&obj_table, 0x32, TBL_ADVANCE, &SRowSet);
		mapi_errstr("QueryRows", GetLastError());
		if (retval != MAPI_E_SUCCESS) return false;
		
		for (i = 0; i < SRowSet.cRows; i++) {
			struct SPropValue *lpProp;
			uint32_t *rights;

			lpProp = get_SPropValue_SRow(&(SRowSet.aRow[i]), PR_MEMBER_NAME);
			printf("    %-25s: %s\n", "Username", lpProp && lpProp->value.lpszA ? lpProp->value.lpszA : "Default");
			lpProp = get_SPropValue_SRow(&(SRowSet.aRow[i]), PR_MEMBER_RIGHTS);
			rights = &(lpProp->value.l);
			ndr_print_debug((ndr_print_fn_t)ndr_print_ACLRIGHTS, "Rights", (void *)rights);
			printf("\n");
		}
		
		mapi_object_release(&obj_table);
	}
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);

	MAPIUninitialize();
	talloc_free(mem_ctx);
	return true;
}
