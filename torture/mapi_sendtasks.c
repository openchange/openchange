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
#include <torture/mapi_torture.h>
#include <torture/torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>

#define	CN_PROPS 5

bool torture_rpc_mapi_sendtasks(struct torture_context *torture)
{
	NTSTATUS		nt_status;
	enum MAPISTATUS		retval;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	bool			ret = True;
	const char		*task = lp_parm_string(-1, "mapi", "task");
	uint32_t		priority = lp_parm_int(-1, "mapi", "priority", 0);
	uint32_t		status = lp_parm_int(-1, "mapi", "status", 0);
	struct mapi_session	*session;
	uint64_t		id_task;
	mapi_object_t		obj_store;
	mapi_object_t		obj_task;
	mapi_object_t		obj_table;
	mapi_object_t		obj_message;
	struct SPropValue	props[CN_PROPS];

	if (!task) return False;

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
	mapi_object_init(&obj_task);
	mapi_object_init(&obj_table);

	/* session::OpenMsgStore */
	retval = OpenMsgStore(&obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* Retrieve the task Folder ID */
	retval = GetDefaultFolder(&obj_store, &id_task, olFolderTasks);
	mapi_errstr("GetDefaultFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* We now open the task folder */
	retval = OpenFolder(&obj_store, id_task, &obj_task);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* Operations on the task folder */
	retval = CreateMessage(&obj_task, &obj_message);
	mapi_errstr("CreateMessage", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	set_SPropValue_proptag(&props[0], PR_CONTACT_CARD_NAME, (const void *) task);
	set_SPropValue_proptag(&props[1], PR_NORMALIZED_SUBJECT, (const void *) task);
	set_SPropValue_proptag(&props[2], PR_MESSAGE_CLASS, (const void *)"IPM.Task");
	set_SPropValue_proptag(&props[3], PR_PRIORITY, (const void *)&priority);
	set_SPropValue_proptag(&props[4], PR_Status, (const void *)&status);
	retval = SetProps(&obj_message, props, CN_PROPS);
	mapi_errstr("SetProps", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	retval = SaveChangesMessage(&obj_task, &obj_message);
	mapi_errstr("SaveChangesMessage", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;


	mapi_object_release(&obj_table);
	mapi_object_release(&obj_task);
	mapi_object_release(&obj_store);

	/* uninitialize mapi
	 */
	MAPIUninitialize();
	talloc_free(mem_ctx);
	
	return (ret);
}
