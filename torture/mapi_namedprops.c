/*
   OpenChange MAPI torture suite implementation.

   Test Named properties and IMAPIProp associated functions

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

#define	NAMEDPROP_NAME	"torture_namedprops"
#define	NAMEDPROP_VALUE	"Can you see me?"

bool torture_rpc_mapi_namedprops(struct torture_context *torture)
{
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	struct dcerpc_pipe		*p;
	TALLOC_CTX			*mem_ctx;
	bool				ret = true;
	struct mapi_session		*session;
	mapi_id_t			id_folder;
	mapi_object_t			obj_store;
	mapi_object_t			obj_folder;
	mapi_object_t			obj_table;
	mapi_object_t			obj_message;
	struct SRowSet			SRowSet;
	struct SPropTagArray		*SPropTagArray;
	struct SPropValue		*propvals;
	struct mapi_SPropValue_array	props_array;
	uint32_t			i;
	uint32_t			propID;
	uint16_t			count;
	struct MAPINAMEID		*nameid;
	uint16_t			*propIDs;
	uint32_t			cn_propvals;

	/* init torture */
	mem_ctx = talloc_named(NULL, 0, "torture_rpc_mapi_namedprops");
	status = torture_rpc_connection(torture, &p, &ndr_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return false;
	}
	
	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx, torture->lp_ctx)) == NULL) return false;

	/* OpenMsgStore */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(session, &obj_store);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Retrieve the specified folder ID */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderInbox);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Open the folder */
	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Retrieve the folder contents */
	mapi_object_init(&obj_table);
	retval = GetContentsTable(&obj_folder, &obj_table, 0, NULL);
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

	/* We just need to open the first message for this test */
	if (SRowSet.cRows == 0) {
		printf("No messages in Mailbox\n");
		talloc_free(mem_ctx);
		return false;
	}

	mapi_object_init(&obj_message);
	retval = OpenMessage(&obj_folder,
			     SRowSet.aRow[0].lpProps[0].value.d,
			     SRowSet.aRow[0].lpProps[1].value.d,
			     &obj_message, Create);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = GetPropsAll(&obj_message, &props_array);
	if (retval != MAPI_E_SUCCESS) return false;

	/* loop through properties, search for named properties
	 * (0x8000-0xFFFE range) and call GetNamesFromIDs 
	 */
	printf("\n\n1. GetNamesFromIDs\n");
	for (i = 0; i < props_array.cValues; i++) {
		propID = props_array.lpProps[i].ulPropTag >> 16;
		if (propID >= 0x8000 && propID <= 0xFFFE) {
			propID = props_array.lpProps[i].ulPropTag;
			propID = (propID & 0xFFFF0000) | PT_NULL;
			nameid = talloc_zero(mem_ctx, struct MAPINAMEID);
			retval = GetNamesFromIDs(&obj_message, propID, &count, &nameid);
			if (retval != MAPI_E_SUCCESS) return false;
			switch (nameid->ulKind) {
			case MNID_ID:
				printf("\t0x%.8x mapped to 0x%.4x\n", 
				       propID | (props_array.lpProps[i].ulPropTag & 0xFFFF), nameid->kind.lid);
				break;
			case MNID_STRING:
				printf("\t0x%.8x mapped to %s\n",
				       propID, nameid->kind.lpwstr.Name);
				break;
			}
			talloc_free(nameid);
		}
	}

	/*
	 * Retrieve all the named properties for the current object
	 * This function seems to be the only one accepting 0 for
	 * input ulPropTag and returning the whole set of named properties
	 */
	printf("\n\n2. QueryNamedProperties\n");
	nameid = talloc_zero(mem_ctx, struct MAPINAMEID);
	propIDs = talloc_zero(mem_ctx, uint16_t);
	retval = QueryNamedProperties(&obj_message, 0, NULL, &count, &propIDs, &nameid);
	mapi_errstr("QueryNamedProperties", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	for (i = 0; i < count; i++) {
		char	*guid;

		printf("0x%.4x:\n", propIDs[i]);

		guid = GUID_string(mem_ctx, &nameid[i].lpguid);
		printf("\tguid: %s\n", guid);
		talloc_free(guid);

		switch (nameid[i].ulKind) {
		case MNID_ID:
			printf("\tmapped to 0x%.4x\n", nameid[i].kind.lid);
			break;
		case MNID_STRING:
			printf("\tmapped to %s\n", nameid[i].kind.lpwstr.Name);
			break;
		}		
	}
	talloc_free(propIDs);

	/*
	 * finally call GetIDsFromNames with the Names retrieved in
	 * the previous call
	 */
	printf("\n\n3. GetIDsFromNames\n");
	for (i = 0; i < count; i++) {
		SPropTagArray = talloc_zero(mem_ctx, struct SPropTagArray);
		retval = GetIDsFromNames(&obj_folder, 1, &nameid[i], 0, &SPropTagArray);
		switch (nameid[i].ulKind) {
		case MNID_ID:
			printf("0x%.4x mapped to ", nameid[i].kind.lid);
			break;
		case MNID_STRING:
			printf("%s mapped to ", nameid[i].kind.lpwstr.Name);
			break;
		}	
		mapidump_SPropTagArray(SPropTagArray);
		talloc_free(SPropTagArray);
	}

	talloc_free(nameid);

	/*
	 * Try to create a named property
	 */
	{
	  struct GUID guid;

	  printf("\n\n4. GetIDsFromNames (Create named property)\n");
	  GUID_from_string(PS_INTERNET_HEADERS, &guid);
	  nameid = talloc_zero(mem_ctx, struct MAPINAMEID);
	  SPropTagArray = talloc_zero(mem_ctx, struct SPropTagArray);

	  nameid[0].lpguid = guid;
	  nameid[0].ulKind = MNID_STRING;
	  nameid[0].kind.lpwstr.Name = NAMEDPROP_NAME;
	  nameid[0].kind.lpwstr.NameSize = strlen(NAMEDPROP_NAME) * 2 + 2;
	  retval = GetIDsFromNames(&obj_folder, 1, &nameid[0], MAPI_CREATE, &SPropTagArray);
	  if (retval != MAPI_E_SUCCESS) return false;
	  mapi_errstr("GetIDsFromNames", GetLastError());

	  printf("%s mapped to 0x%.8x\n", NAMEDPROP_NAME, SPropTagArray->aulPropTag[0]);
	  propID = SPropTagArray->aulPropTag[0] | PT_STRING8;

	  talloc_free(nameid);
	  talloc_free(SPropTagArray);
	}

	/*
	 * Assign its value with SetProps and save changes
	 */
	{
		struct SPropValue	props[1];
		const char		*testval = NAMEDPROP_VALUE;

		printf("\n\n5. Assigning %s to %s\n", NAMEDPROP_VALUE, NAMEDPROP_NAME);
		
		set_SPropValue_proptag(&props[0], propID, (const void *)testval);
		retval = SetProps(&obj_message, props, 1);
		if (retval != MAPI_E_SUCCESS) return false;
		mapi_errstr("SetProps", GetLastError());
		
		retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
		mapi_errstr("SaveChangesMessage", GetLastError());
		if (retval != MAPI_E_SUCCESS) return false;
	}

	printf("\n\n6. GetNamesFromIDs (Fetch torture_namedprops property)\n");
	propID = (propID & 0xFFFF0000)| PT_NULL;
	retval = GetNamesFromIDs(&obj_message, propID, &count, &nameid);
	mapi_errstr("GetNamesFromIDs", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	switch (nameid->ulKind) {
	case MNID_ID:
		printf("\t0x%.8x mapped to 0x%.4x\n", 
		       propID | (props_array.lpProps[i].ulPropTag & 0xFFFF), nameid->kind.lid);
		break;
	case MNID_STRING:
		printf("\t0x%.8x mapped to %s\n",
		       propID, nameid->kind.lpwstr.Name);
		break;
	}
	talloc_free(nameid);

	printf("\n\n7. GetProps (torture_namedprops property)\n");
	propID = (propID & 0xFFFF0000) | PT_STRING8;
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, propID);
	retval = GetProps(&obj_message, SPropTagArray, &propvals, &cn_propvals);
	MAPIFreeBuffer(SPropTagArray);
	mapi_errstr("GetProps", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	mapidump_SPropValue(propvals[0], "\t");
	MAPIFreeBuffer(propvals);

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	/* Uninitialize MAPI */
	MAPIUninitialize();
	talloc_free(mem_ctx);

	return ret;
}
