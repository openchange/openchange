/*
   OpenChange MAPI torture suite implementation.

   Delete mail from an Exchange server

   Copyright (C) Fabien Le Mentec 2007
   
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
#include <libmapi/defs_private.h>
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>


#define CN_ROWS 0x100


bool torture_rpc_mapi_deletemail(struct torture_context *torture)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	bool			ret = true;
	const char		*s_subject = lp_parm_string(torture->lp_ctx, NULL, "mapi", "subject");
	int			len_subject;
	struct mapi_session	*session;
	mapi_object_t		obj_store;
	mapi_object_t		obj_inbox;
	mapi_object_t		obj_table;
	mapi_id_t		id_inbox;
	mapi_id_t		*id_messages;
	unsigned long		cn_messages;
	struct SRowSet		rowset;
	unsigned long		i_row;
	unsigned long		cn_rows;
	struct SPropTagArray	*SPropTagArray;


	/* init torture */
	mem_ctx = talloc_named(NULL, 0, "torture_rpc_mapi_deletemail");

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx, torture->lp_ctx)) == NULL) return false;

	/* init objets */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_inbox);
	mapi_object_init(&obj_table);

	/* session::OpenMsgStore() */
	retval = OpenMsgStore(session, &obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	mapi_object_debug(&obj_store);

	/* id_inbox = store->GetReceiveFolder */
	retval = GetReceiveFolder(&obj_store, &id_inbox, NULL);
	mapi_errstr("GetReceiveFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* inbox = store->OpenFolder()
	 */
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	mapi_object_debug(&obj_inbox);

	/* table = inbox->GetContentsTable() */
	retval = GetContentsTable(&obj_inbox, &obj_table, 0, NULL);
	mapi_errstr("GetContentsTable", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	mapi_object_debug(&obj_table);

	/* rowset = table->QueryRows() */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT);
	retval = SetColumns(&obj_table, SPropTagArray);
	mapi_errstr("SetColumns", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	while ((retval = QueryRows(&obj_table, CN_ROWS, TBL_ADVANCE, &rowset)) == MAPI_E_SUCCESS) {
		cn_rows = rowset.cRows;
		if (!cn_rows) break;
		id_messages = talloc_array(mem_ctx, uint64_t, cn_rows);
		cn_messages = 0;
		
		if (s_subject == 0)
			s_subject = "default_subject";
		len_subject = strlen(s_subject);
		
		for (i_row = 0; i_row < cn_rows; ++i_row) {
			if (strncmp(rowset.aRow[i_row].lpProps[4].value.lpszA, s_subject, len_subject) == 0) {
				id_messages[cn_messages] = rowset.aRow[i_row].lpProps[1].value.d;
				++cn_messages;
				DEBUG(0, ("delete(%"PRIx64")\n", id_messages[cn_messages - 1]));
			}
		}

		/* IMessage::DeleteMessages() */
		if (cn_messages) {
			retval = DeleteMessage(&obj_inbox, id_messages, cn_messages);
			if (retval != MAPI_E_SUCCESS) {
				mapi_errstr("DeleteMessages", GetLastError());
			}
		}
	}

	/* release objects
	 */
	mapi_object_release(&obj_store);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_table);

	/* uninitialize mapi
	 */
	MAPIUninitialize();
	talloc_free(mem_ctx);
	
	return (ret);
}
