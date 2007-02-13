/* OpenChange MAPI implementation testsuite

   fetch mail from an Exchange server

   Copyright (C) Fabien Le Mentec 2007
   
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

#define CN_ROWS 0x100

/* FIXME: Should be part of Samba's data: */
NTSTATUS torture_rpc_connection(TALLOC_CTX*, struct dcerpc_pipe**, const struct dcerpc_interface_table *);


BOOL torture_rpc_mapi_deletemail(struct torture_context *torture)
{
	NTSTATUS		status;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	BOOL			ret = True;
	struct emsmdb_context	*emsmdb;
	const char		*organization = lp_parm_string(-1, "exchange", "organization");
	const char		*group = lp_parm_string(-1, "exchange", "ou");
	const char		*username;
	const char		*s_subject = lp_parm_string(-1, "mapi", "subject");
	int			len_subject;
	char			*mailbox_path;
	uint32_t		hdl_msgstore;
	uint32_t		hdl_inbox;
	uint32_t		hdl_table;
	uint64_t		id_inbox;
	uint64_t		id_outbox;
	uint64_t*		id_messages;
	unsigned long		cn_messages;
	struct SRowSet*		rowset;
	unsigned long		i_row;
	unsigned long		cn_rows;
	struct SPropTagArray	*SPropTagArray;


	mem_ctx = talloc_init("torture_rpc_mapi_deletemail");

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

	if (!organization) {
	  organization = cli_credentials_get_domain(cmdline_credentials);
	}
	username = cli_credentials_get_username(cmdline_credentials);
	if (!organization || !username) {
		DEBUG(1,("mapi_deletemail: username and domain required"));
		return NULL;
	}


	mailbox_path = talloc_asprintf(emsmdb->mem_ctx, MAILBOX_PATH, organization, group, username);
	OpenMsgStore(emsmdb, 0, &hdl_msgstore, &id_outbox, mailbox_path);

	GetReceiveFolder(emsmdb, 0, hdl_msgstore, &id_inbox);
	OpenFolder(emsmdb, 0, hdl_msgstore, id_inbox, &hdl_inbox);

	GetContentsTable(emsmdb, 0, hdl_inbox, &hdl_table);
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT);
	SetColumns(emsmdb, 0, SPropTagArray);
	rowset = talloc(mem_ctx, struct SRowSet);

	QueryRows(emsmdb, 0, hdl_table, CN_ROWS, &rowset);
	cn_rows = (*rowset).cRows;

	id_messages = talloc_array(mem_ctx, uint64_t, cn_rows);
	cn_messages = 0;

	/* default subject */
	if (s_subject == 0)
	  s_subject = "";
	len_subject = strlen(s_subject);

	for (i_row = 0; i_row < cn_rows; ++i_row) {
	  if (strncmp((*rowset).aRow[i_row].lpProps[4].value.lpszA, s_subject, len_subject) == 0) {
	    id_messages[cn_messages] = (*rowset).aRow[i_row].lpProps[1].value.d;
	    ++cn_messages;
	    printf("delete(%llx)\n", id_messages[cn_messages - 1]);
	  }
	}

	if (cn_messages) {
	  DeleteMessages(emsmdb, 0, hdl_inbox, id_messages, cn_messages);
	  printf("\n");
	}

	talloc_free(mem_ctx);
	
	return (ret);
}
