/*
   OpenChange MAPI torture suite implementation.

   Fetch contacts from an Exchange server

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

bool torture_rpc_mapi_fetchcontacts(struct torture_context *torture)
{
	NTSTATUS		nt_status;
	enum MAPISTATUS		retval;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	bool			ret = true;
	struct mapi_session	*session;
	uint64_t		id_contacts;
	mapi_object_t		obj_store;
	mapi_object_t		obj_contacts;
	mapi_object_t		obj_table;
	struct SRowSet		SRowSet;
	struct SPropTagArray	*SPropTagArray;

	/* init torture */
	mem_ctx = talloc_named(NULL, 0, "torture_rpc_mapi_fetchmail");
	nt_status = torture_rpc_connection(torture, &p, &ndr_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return false;
	}

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx, torture->lp_ctx)) == NULL) return false;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_contacts);
	mapi_object_init(&obj_table);

	/* session::OpenMsgStore */
	retval = OpenMsgStore(session, &obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Retrieve the contacts Folder ID */
	retval = GetDefaultFolder(&obj_store, &id_contacts, olFolderContacts);
	mapi_errstr("GetDefaultFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* We now open the contacts folder */
	retval = OpenFolder(&obj_store, id_contacts, &obj_contacts);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Operations on the contacts folder */
	retval = GetContentsTable(&obj_contacts, &obj_table, 0, NULL);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x8,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT,
					  PR_MESSAGE_CLASS,
					  PR_RULE_MSG_PROVIDER,
					  PR_RULE_MSG_NAME);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = QueryRows(&obj_table, 0x32, TBL_ADVANCE, &SRowSet);
	if (retval != MAPI_E_SUCCESS) return false;

	{
		int i;
		mapi_object_t obj_message;
		struct mapi_SPropValue_array properties_array;

		for (i = 0; i < SRowSet.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(&obj_contacts, 
					     SRowSet.aRow[i].lpProps[0].value.d,
					     SRowSet.aRow[i].lpProps[1].value.d,
					     &obj_message, 0);
			if (retval != MAPI_E_NOT_FOUND) {
				retval = GetPropsAll(&obj_message, &properties_array);
				mapidump_contact(&properties_array, NULL);
				mapi_object_release(&obj_message);
			}
		}
	}

	mapi_object_release(&obj_table);
	mapi_object_release(&obj_contacts);
	mapi_object_release(&obj_store);

	/* uninitialize mapi
	 */
	MAPIUninitialize();
	talloc_free(mem_ctx);
	
	return (ret);
}
