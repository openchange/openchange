/*
   OpenChange MAPI torture suite implementation.

   Fetch emails recipients from an Exchange server Inbox

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


bool torture_rpc_mapi_recipient(struct torture_context *torture)
{
	NTSTATUS		ntstatus;
	enum MAPISTATUS		retval;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	struct mapi_session	*session;
	mapi_object_t		obj_store;
	mapi_object_t		obj_inbox;
	mapi_object_t		obj_message;
	mapi_object_t		obj_table;
	mapi_id_t		id_inbox;
	struct SRowSet		SRowSet;
	struct SPropTagArray	*SPropTagArray;
	uint32_t		count = 0;
	uint32_t		i;
	uint32_t		j;

	/* init torture */
	mem_ctx = talloc_named(NULL, 0, "torture_rpc_mapi_recipient");
	ntstatus = torture_rpc_connection(torture, &p, &ndr_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(ntstatus)) {
		talloc_free(mem_ctx);
		return false;
	}

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx, torture->lp_ctx)) == NULL) return false;

	/* Open Message Store */
	mapi_object_init(&obj_store);
	retval  = OpenMsgStore(session, &obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Get Receive Folder */
	mapi_object_init(&obj_inbox);
	retval = GetReceiveFolder(&obj_store, &id_inbox, NULL);
	mapi_errstr("GetReceiveFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Get Contents Table */
	mapi_object_init(&obj_table);
	retval = GetContentsTable(&obj_inbox, &obj_table, 0, &count);
	mapi_errstr("GetContentsTable", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Customize table view */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x3,
					  PR_FID,
					  PR_MID,
					  PR_SUBJECT);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	mapi_errstr("SetColumns", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	while (((retval = QueryRows(&obj_table, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND) && SRowSet.cRows) {
		count -= SRowSet.cRows;
		for (i = 0; i < SRowSet.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(&obj_store,
					     SRowSet.aRow[i].lpProps[0].value.d,
					     SRowSet.aRow[i].lpProps[1].value.d,
					     &obj_message, 0);
			if (GetLastError() != MAPI_E_NOT_FOUND) {
				struct SRowSet		props;
				struct SPropTagArray	proptags;

				

				retval = GetRecipientTable(&obj_message, &props, &proptags);
				if (retval == MAPI_E_SUCCESS) {
					if (SRowSet.aRow[i].lpProps[2].value.lpszA) {
						printf("\n\nSubject: %s\n", SRowSet.aRow[i].lpProps[2].value.lpszA);
						fflush(0);
					}

					printf("\nSPropTagArray:\n");
					fflush(0);
					mapidump_SPropTagArray(&proptags);

					printf("\nSRowSet:\n");
					fflush(0);

					for (j = 0; j < props.cRows; j++) {
						printf("===\n");
						fflush(0);
						mapidump_SRow(&props.aRow[j], "SRow: ");
					}
					
					printf("\n\n");
					fflush(0);
				}
				mapi_object_release(&obj_message);
			}
		}
	}

	mapi_object_release(&obj_table);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);

	MAPIUninitialize();
	talloc_free(mem_ctx);

	return true;
}
