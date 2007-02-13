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

#include <libmapi/libmapi.h>
#include <gen_ndr/ndr_exchange.h>
#include <param.h>
#include <credentials.h>
#include <torture/torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>

static enum MAPISTATUS read_attach_stream(struct emsmdb_context* ctx_emsmdb,
					  TALLOC_CTX* ctx_mem,
					  uint32_t hdl_attach,
					  uint32_t hdl_stream,
					  uint8_t** buf_data,
					  uint32_t* sz_data)
{
  uint32_t		cn_read;
  uint32_t		off_data;
  enum MAPISTATUS	status;
  int			done;
  struct SPropTagArray	*proptags;
  struct SPropValue	*vals;
  uint32_t		cn_vals;

  /* Reset 
   */
  *buf_data = 0;
  *sz_data = 0;
  off_data = 0;
  done = 0;

  /* Get Attachment size
   */
  proptags = set_SPropTagArray(ctx_emsmdb->mem_ctx, 0x1, PR_ATTACH_SIZE);
  status = GetProps(ctx_emsmdb, 0, hdl_attach, proptags, &vals, &cn_vals);
  if (status != MAPI_E_SUCCESS)
    return status;

  /* Alloc buffer
   */
  *sz_data = (uint32_t)vals[0].value.b;
  *buf_data = talloc_size(ctx_mem, *sz_data);
  if (*buf_data == 0)
    return -1;

  /* Read attachment
   */
  while (done == 0)
    {
      status = ReadStream(ctx_emsmdb, 0, hdl_stream,
			  (*buf_data) + off_data,
			  (*sz_data) - off_data,
			  &cn_read);
      if ((status != MAPI_E_SUCCESS) || (cn_read == 0))
	{
	  done = 1;
	}
      else
	{
	  off_data += cn_read;
	  if (off_data >= *sz_data)
	    done = 1;
	}
    }

  *sz_data = off_data;

  return status;
}

BOOL torture_rpc_mapi_fetchattach(struct torture_context *torture)
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
	uint32_t		hdl_msgstore;
	uint32_t		hdl_inbox;
	uint32_t		hdl_message;
	uint32_t		hdl_tb_contents;
	uint32_t		hdl_tb_attach;
	uint32_t		hdl_attach;
	uint32_t		hdl_stream;
	uint64_t		id_inbox;
	uint64_t		id_outbox;
	uint64_t		id_folder;
	uint64_t		id_message;
	struct SPropTagArray	*proptags;
	struct SRowSet		*rows_msgs;
	struct SRowSet		*rows_attach;
	uint32_t		i_msg;
	uint32_t		i_row_attach;
	uint32_t		num_attach;
	uint8_t			*buf_attach;
	uint32_t		sz_attach;

	mem_ctx = talloc_init("torture_rpc_mapi_fetchattach");

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
	  return NULL;
	}
	mailbox_path = talloc_asprintf(emsmdb->mem_ctx, MAILBOX_PATH, organization, group, username);

	/* get inbox content
	 */
	OpenMsgStore(emsmdb, 0, &hdl_msgstore, &id_outbox, mailbox_path);
	GetReceiveFolder(emsmdb, 0, hdl_msgstore, &id_inbox);
	OpenFolder(emsmdb, 0, hdl_msgstore, id_inbox, &hdl_inbox);

	/* query contents table rows
	 */
	GetContentsTable(emsmdb, 0, hdl_inbox, &hdl_tb_contents);
	proptags = set_SPropTagArray(emsmdb->mem_ctx, 0x5,
				     PR_FID,
				     PR_MID,
				     PR_INST_ID,
				     PR_INSTANCE_NUM,
				     PR_SUBJECT);
	SetColumns(emsmdb, 0, proptags);
	rows_msgs = talloc(mem_ctx, struct SRowSet);
	mapistatus = QueryRows(emsmdb, 0, hdl_tb_contents, 0xa, &rows_msgs);
	if (mapistatus != MAPI_E_SUCCESS) {
	  return False;
	}

	/* foreach message get attachment table
	   foreach attachment, get PR_NUM
	   foreach PR_NUM, open attachment
	 */
	for (i_msg = 0; i_msg < rows_msgs[0].cRows; i_msg++) {

		/* open message
		 */
		id_folder = rows_msgs[0].aRow[i_msg].lpProps[0].value.d;
		id_message = rows_msgs[0].aRow[i_msg].lpProps[1].value.d;
		mapistatus = OpenMessage(emsmdb, 0, hdl_msgstore, id_folder, id_message, &hdl_message);
		if (mapistatus == MAPI_E_SUCCESS) {
		  //			mapistatus = OpenStream(emsmdb, 0, PR_BODY, hdl_message, &hdl_tb_attach);
			mapistatus = GetAttachmentTable(emsmdb, hdl_message, &hdl_tb_attach);

			/* foreach attachment, open by PR_ATTACH_NUM */
			if (mapistatus == MAPI_E_SUCCESS) {
				proptags = set_SPropTagArray(emsmdb->mem_ctx, 0x1, PR_ATTACH_NUM);
				SetColumns(emsmdb, 0, proptags);
				rows_attach = talloc(mem_ctx, struct SRowSet);
				mapistatus = QueryRows(emsmdb, 0, hdl_tb_attach, 0xa, &rows_attach);
				if (mapistatus != MAPI_E_SUCCESS)
					return False;
				
				/* get a stream on PR_ATTACH_DATA_BIN */
				for (i_row_attach = 0; i_row_attach < rows_attach[0].cRows; i_row_attach++) {
					num_attach = rows_attach[0].aRow[i_row_attach].lpProps[0].value.l;
					mapistatus = OpenAttach(emsmdb, 0, hdl_message, num_attach, &hdl_attach);
					if (mapistatus == MAPI_E_SUCCESS) {
						mapistatus = OpenStream(emsmdb, 0, PR_ATTACH_DATA_BIN, hdl_attach, &hdl_stream);
						
						/* read stream content */
						if (mapistatus == MAPI_E_SUCCESS)
							read_attach_stream(emsmdb, mem_ctx, hdl_attach, hdl_stream, &buf_attach, &sz_attach);
					}
				}
			}
		}
	}
	
	talloc_free(mem_ctx);
	return (ret);
}
