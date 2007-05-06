/* 
   OpenChange MAPI implementation testsuite

   Create a task on Exchange server

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

#define	CN_PROPS 5

BOOL torture_rpc_mapi_sendtasks(struct torture_context *torture)
{
	NTSTATUS		nt_status;
	enum MAPISTATUS		retval;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	BOOL			ret = True;
	const char		*profname;
	const char		*profdb;
	const char		*task = lp_parm_string(-1, "mapi", "task");
	uint32_t		priority = lp_parm_int(-1, "mapi", "priority", 0);
	uint32_t		status = lp_parm_int(-1, "mapi", "status", 0);
	struct mapi_session	*session;
	uint64_t		id_root;
	uint64_t		id_task;
	mapi_object_t		obj_store;
	mapi_object_t		obj_root;
	mapi_object_t		obj_htable;
	mapi_object_t		obj_task;
	mapi_object_t		obj_table;
	mapi_object_t		obj_message;
	struct SPropValue	props[CN_PROPS];
	struct SRowSet		SRowSet;
	struct SPropTagArray	*SPropTagArray;

	if (!task) return False;

	/* init torture */
	mem_ctx = talloc_init("torture_rpc_mapi_fetchmail");
	nt_status = torture_rpc_connection(mem_ctx, &p, &dcerpc_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return False;
	}

	/* init mapi */
	profdb = lp_parm_string(-1, "mapi", "profile_store");
	retval = MAPIInitialize(profdb);
	mapi_errstr("MAPIInitialize", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* profile name */
	profname = lp_parm_string(-1, "mapi", "profile");
	if (profname == 0) {
		DEBUG(0, ("Please specify a valid profile name\n"));
		return False;
	}

	retval = MapiLogonEx(&session, profname);
	mapi_errstr("MapiLogonEx", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_root);
	mapi_object_init(&obj_htable);
	mapi_object_init(&obj_task);
	mapi_object_init(&obj_table);

	/* session::OpenMsgStore */
	retval = OpenMsgStore(&obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	retval = GetOutboxFolder(&obj_store, &id_root);
	if (retval != MAPI_E_SUCCESS) return False;

	retval = OpenFolder(&obj_store, id_root, &obj_root);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	retval = GetHierarchyTable(&obj_root, &obj_htable);
	mapi_errstr("GetHierarchyTable", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2,
					  PR_FID,
					  PR_DISPLAY_NAME);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	mapi_errstr("SetColumns", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	retval = QueryRows(&obj_htable, 0x32, TBL_ADVANCE, &SRowSet);
	mapi_errstr("QueryRows", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* Retrieve the task Folder ID */
	id_task = GetTasksFID(SRowSet);

	/* We now open the task folder */
	retval = OpenFolder(&obj_store, id_task, &obj_task);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* Operations on the task folder */
	retval = CreateMessage(&obj_task, &obj_message);
	mapi_errstr("CreateMessage", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	set_SPropValue_proptag(&props[0], PR_CONTACT_CARD_NAME, (void *) task);
	set_SPropValue_proptag(&props[1], PR_NORMALIZED_SUBJECT, (void *) task);
	set_SPropValue_proptag(&props[2], PR_MESSAGE_CLASS, (void *)"IPM.Task");
	set_SPropValue_proptag(&props[3], PR_PRIORITY, (void *)&priority);
	set_SPropValue_proptag(&props[4], PR_Status, (void *)&status);
	retval = SetProps(&obj_message, props, CN_PROPS);
	mapi_errstr("SetProps", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	retval = SaveChangesMessage(&obj_task, &obj_message);
	mapi_errstr("SaveChangesMessage", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;


	mapi_object_release(&obj_table);
	mapi_object_release(&obj_task);
	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_root);
	mapi_object_release(&obj_store);

	/* uninitialize mapi
	 */
	MAPIUninitialize();
	talloc_free(mem_ctx);
	
	return (ret);
}
