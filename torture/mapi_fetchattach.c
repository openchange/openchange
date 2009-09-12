/*
   OpenChange MAPI torture suite implementation.

   Fetch attachments from an Exchange server

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
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>


static enum MAPISTATUS read_attach_stream(TALLOC_CTX *ctx_mem,
					  mapi_object_t *obj_attach,
					  mapi_object_t *obj_stream,
					  uint8_t** buf_data,
					  uint32_t* sz_data)
{
  uint16_t		cn_read;
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
  proptags = set_SPropTagArray(ctx_mem, 0x1, PR_ATTACH_SIZE);
  status = GetProps(obj_attach, proptags, &vals, &cn_vals);
  mapi_errstr("GetProps", GetLastError());
  if (status != MAPI_E_SUCCESS) return status;

  /* Alloc buffer
   */
  *sz_data = (uint32_t)vals[0].value.b;
  *buf_data = talloc_size(ctx_mem, *sz_data);
  if (*buf_data == 0)
    return MAPI_E_NOT_ENOUGH_MEMORY;

  /* Read attachment
   */
  while (done == 0) {
	  status = ReadStream(obj_stream,
			      (*buf_data) + off_data,
			      (*sz_data) - off_data,
			      &cn_read);
	  mapi_errstr("ReadStream", GetLastError());
	  if ((status != MAPI_E_SUCCESS) || (cn_read == 0)) {
		  done = 1;
	  }
	  else {
		  off_data += cn_read;
		  if (off_data >= *sz_data)
			  done = 1;
	  }
  }

  *sz_data = off_data;

  return status;
}

bool torture_rpc_mapi_fetchattach(struct torture_context *torture)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	bool			ret = true;
	struct mapi_session	*session;
	mapi_object_t		obj_store;
	mapi_object_t		obj_inbox;
	mapi_object_t		obj_message;
	mapi_object_t		obj_tb_contents;
	mapi_object_t		obj_tb_attach;
	mapi_object_t		obj_attach;
	mapi_object_t		obj_stream;
	mapi_id_t		id_inbox;
	mapi_id_t		id_folder;
	mapi_id_t		id_message;
	struct SPropTagArray	*proptags;
	struct SRowSet		rows_msgs;
	struct SRowSet		rows_attach;
	uint32_t		i_msg;
	uint32_t		i_row_attach;
	uint32_t		num_attach;
	uint8_t			*buf_attach;
	uint32_t		sz_attach;

	/* init torture */
	mem_ctx = talloc_named(NULL, 0, "torture_rpc_mapi_fetchattach");

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx, torture->lp_ctx)) == NULL) return false;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_inbox);
	mapi_object_init(&obj_message);
	mapi_object_init(&obj_tb_contents);
	mapi_object_init(&obj_tb_attach);
	mapi_object_init(&obj_attach);
	mapi_object_init(&obj_stream);

	/* session::OpenMsgStore() */
	retval = OpenMsgStore(session, &obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* id_inbox = store->GetReceiveFolder */
	retval = GetReceiveFolder(&obj_store, &id_inbox, NULL);
	mapi_errstr("GetReceiveFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* inbox = store->OpenFolder() */
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* table = inbox->GetContentsTable() */
	retval = GetContentsTable(&obj_inbox, &obj_tb_contents, 0, NULL);
	mapi_errstr("GetContentsTable", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	proptags = set_SPropTagArray(mem_ctx, 0x5,
				     PR_FID,
				     PR_MID,
				     PR_INST_ID,
				     PR_INSTANCE_NUM,
				     PR_SUBJECT);
	retval = SetColumns(&obj_tb_contents, proptags);
	mapi_errstr("SetColumns", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	retval = QueryRows(&obj_tb_contents, 0xa, TBL_ADVANCE, &rows_msgs);
	mapi_errstr("QueryRows", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* foreach message get attachment table
	   foreach attachment, get PR_NUM
	   foreach PR_NUM, open attachment
	 */
	
	for (i_msg = 0; i_msg < rows_msgs.cRows; i_msg++) {

		/* open message
		 */
		id_folder = rows_msgs.aRow[i_msg].lpProps[0].value.d;
		id_message = rows_msgs.aRow[i_msg].lpProps[1].value.d;
		retval = OpenMessage(&obj_store, 
				     id_folder, 
				     id_message, 
				     &obj_message, 0);
		mapi_errstr("OpenMessage", GetLastError());
		if (retval == MAPI_E_SUCCESS) {

			/* open attachment table */
			retval = GetAttachmentTable(&obj_message, &obj_tb_attach);
			mapi_errstr("GetAttachmentTable", GetLastError());

			/* foreach attachment, open by PR_ATTACH_NUM */
			if (retval == MAPI_E_SUCCESS) {
				proptags = set_SPropTagArray(mem_ctx, 0x1, PR_ATTACH_NUM);
				retval = SetColumns(&obj_tb_attach, proptags);
				mapi_errstr("SetColumns", GetLastError());
				if (retval != MAPI_E_SUCCESS) return false;

				retval = QueryRows(&obj_tb_attach, 0xa, TBL_ADVANCE, &rows_attach);
				mapi_errstr("QueryRows", GetLastError());
				if (retval != MAPI_E_SUCCESS) return false;

				/* get a stream on PR_ATTACH_DATA_BIN */
				for (i_row_attach = 0; i_row_attach < rows_attach.cRows; i_row_attach++) {
					num_attach = rows_attach.aRow[i_row_attach].lpProps[0].value.l;
					retval = OpenAttach(&obj_message, num_attach, &obj_attach);
					mapi_errstr("OpenAttach", GetLastError());
					if (retval == MAPI_E_SUCCESS) {
						retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, OpenStream_ReadOnly, &obj_stream);
						mapi_errstr("OpenStream", GetLastError());

						/* read stream content */
						if (retval == MAPI_E_SUCCESS) {
							read_attach_stream(mem_ctx,
									   &obj_attach, &obj_stream, &buf_attach,
									   &sz_attach);
						}
					}
				}
			}
		}
	}

	mapi_object_release(&obj_store);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_tb_contents);
	mapi_object_release(&obj_tb_attach);
	mapi_object_release(&obj_attach);
	mapi_object_release(&obj_stream);

	MAPIUninitialize();

	talloc_free(mem_ctx);
	return (ret);
}
