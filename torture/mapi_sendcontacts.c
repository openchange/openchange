/*
   OpenChange MAPI torture suite implementation.

   Send contacts to an Exchange server

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

bool torture_rpc_mapi_sendcontacts(struct torture_context *torture)
{
	NTSTATUS		nt_status;
	enum MAPISTATUS		retval;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	bool			ret = true;
	const char		*cardname = lp_parm_string(torture->lp_ctx, NULL, "mapi", "cardname");
	const char		*fullname = lp_parm_string(torture->lp_ctx, NULL, "mapi", "fullname");
	struct mapi_session	*session;
	uint64_t		id_contacts;
	mapi_object_t		obj_store;
	mapi_object_t		obj_contacts;
	mapi_object_t		obj_table;
	mapi_object_t		obj_message;
	struct SPropValue	props[5];
	struct mapi_nameid	*nameid;
	struct SPropTagArray	*SPropTagArray;

	if (!cardname) return false;

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
	retval = CreateMessage(&obj_contacts, &obj_message);
	mapi_errstr("CreateMessage", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Build the list of named properties we want to set */
	nameid = mapi_nameid_new(mem_ctx);
	mapi_nameid_OOM_add(nameid, "FileUnder", PSETID_Address);
	mapi_nameid_string_add(nameid, "urn:schemas:contacts:fileas", PS_PUBLIC_STRINGS);


	/* GetIDsFromNames and map property types */
	SPropTagArray = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = GetIDsFromNames(&obj_contacts, nameid->count,
				 nameid->nameid, 0, &SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;
	mapi_nameid_SPropTagArray(nameid, SPropTagArray);
	MAPIFreeBuffer(nameid);

	set_SPropValue_proptag(&props[0], SPropTagArray->aulPropTag[0], (const void *) cardname);
	set_SPropValue_proptag(&props[1], PR_DISPLAY_NAME, (const void *) fullname);
	set_SPropValue_proptag(&props[2], PR_MESSAGE_CLASS, (const void *)"IPM.Contact");
	set_SPropValue_proptag(&props[3], PR_NORMALIZED_SUBJECT, (const void *) cardname);
	set_SPropValue_proptag(&props[4], SPropTagArray->aulPropTag[1], (const void *) cardname);
	retval = SetProps(&obj_message, props, 5);
	mapi_errstr("SetProps", GetLastError());
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = SaveChangesMessage(&obj_contacts, &obj_message, KeepOpenReadOnly);
	mapi_errstr("SaveChangesMessage", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;


	mapi_object_release(&obj_table);
	mapi_object_release(&obj_contacts);
	mapi_object_release(&obj_store);

	/* uninitialize mapi
	 */
	MAPIUninitialize();
	talloc_free(mem_ctx);
	
	return (ret);
}
