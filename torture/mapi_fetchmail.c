/* 
   OpenChange MAPI implementation testsuite

   fetch mail from an Exchange server

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

#include "openchange.h"
#include <torture/torture.h>
#include "ndr_exchange.h"
#include "libmapi/mapicode.h"
#include "libmapi/include/emsmdb.h"
#include "libmapi/include/mapi_proto.h"

/* FIXME: Should be part of Samba's data: */
NTSTATUS torture_rpc_connection(TALLOC_CTX *parent_ctx, 
				struct dcerpc_pipe **p, 
				const struct dcerpc_interface_table *table);

BOOL torture_rpc_mapi_fetchmail(struct torture_context *torture)
{
	NTSTATUS		status;
	MAPISTATUS		mapistatus;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	BOOL			ret = True;
	struct emsmdb_context	*emsmdb;
	const char		*organization = lp_parm_string(-1, "exchange", "organization");
	const char		*group = lp_parm_string(-1, "exchange", "ou");
	const char		*username;
	char			*mailbox_path;
	uint32_t		hid_msgstore, hid_folder1, hid_msg;
	uint32_t		table_id;
	uint64_t		folder_id;
	struct SPropTagArray	*SPropTagArray, *props;	
	struct SRowSet		*SRowSet;
	uint32_t		i;

	mem_ctx = talloc_init("torture_rpc_mapi_fetchmail");

	status = torture_rpc_connection(mem_ctx, &p, &dcerpc_table_exchange_emsmdb);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);

		return False;
	}

	emsmdb = emsmdb_connect(mem_ctx, p, cmdline_credentials);
	if (!emsmdb) {
		return False;
	}
	
	emsmdb->mem_ctx = mem_ctx;

	DEBUG(0, ("[STEP 01] mapi call: OpenMsgStore\n"));
	if (!organization) {
	  organization = cli_credentials_get_domain(cmdline_credentials);
	}
	username = cli_credentials_get_username(cmdline_credentials);
	if (!organization || !username) {
		DEBUG(1,("mapi_fetchmail: username and domain required"));
		return NULL;
	}
	mailbox_path = talloc_asprintf(emsmdb->mem_ctx, "/o=%s/ou=%s/cn=Recipients/cn=%s", organization, group, username);
	OpenMsgStore(emsmdb, 0, &hid_msgstore, mailbox_path);

	DEBUG(0, ("[STEP 02] mapi call: GetReceiveFolder\n"));
	GetReceiveFolder(emsmdb, 0, hid_msgstore, &folder_id);

	DEBUG(0, ("[STEP 03] mapi call: OpenFolder\n"));
	OpenFolder(emsmdb, 0, hid_msgstore, folder_id, &hid_folder1);

	DEBUG(0, ("[STEP 05] mapi call: GetContentsTable\n"));
	GetContentsTable(emsmdb, 0, hid_folder1, &table_id);

	SPropTagArray = set_SPropTagArray(emsmdb->mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT);
	SetColumns(emsmdb, 0, SPropTagArray);

	DEBUG(0, ("[STEP 06] mapi call: GetContentsTable\n"));
	printf("#### handle_id = 0x%x\n", table_id);
	SRowSet = talloc(mem_ctx, struct SRowSet);
	QueryRows(emsmdb, 0, table_id, 0xa, &SRowSet);

	for (i = 0; i < SRowSet[0].cRows; i++) {
		printf("[STEP 07-%i] mapi call: OpenMessage\n", i);
		mapistatus = OpenMessage(emsmdb, 0, hid_msgstore,
			    SRowSet[0].aRow[i].lpProps[0].value.d,
			    SRowSet[0].aRow[i].lpProps[1].value.d,
			    &hid_msg);
		
		if (mapistatus == MAPI_E_SUCCESS) {
			printf("[STEP 08-%i] mapi call: GetProps\n", i);
			props = set_SPropTagArray(emsmdb->mem_ctx, 0x4,
						  PR_SUBJECT,
						  PR_BODY,
						  PR_SENDER_NAME,
						  PR_SENDER_EMAIL_ADDRESS);
		
			GetProps(emsmdb, 0, hid_msg, props);
			talloc_free(props);
		}
	}

	talloc_free(mem_ctx);
	
	return (ret);
}
