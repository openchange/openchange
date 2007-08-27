/*
   OpenChange MAPI implementation testsuite

   Message related operations torture

   Copyright (C) Julien Kerihuel 2007
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
#include <torture/mapi_torture.h>
#include <torture/torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>
#include <string.h>
#include <unistd.h>
#include "oc_test.h"


#define CN_MSG_PROPS	3
#define MSG_SUBJECT	"torture_subject"
#define MSG_BODY	"torture_body"
#define CN_ATTACH_PROPS	4
#define MSG_ATTACH	"torture_attach"


static enum MAPISTATUS read_attach_stream(TALLOC_CTX *ctx_mem,
					  mapi_object_t *obj_attach,
					  mapi_object_t *obj_stream,
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
  proptags = set_SPropTagArray(ctx_mem, 0x1, PR_ATTACH_SIZE);
  status = GetProps(obj_attach, proptags, &vals, &cn_vals);
  mapi_errstr("GetProps", GetLastError());
  if (status != MAPI_E_SUCCESS) return status;

  /* Alloc buffer
   */
  *sz_data = (uint32_t)vals[0].value.b;
  *buf_data = talloc_size(ctx_mem, *sz_data);
  if (*buf_data == 0)
    return -1;

  /* Read attachment
   */
  while (done == 0) {
	  status = ReadStream(obj_stream,
			      (*buf_data) + off_data,
			      (*sz_data) - off_data,
			      &cn_read);
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


static bool torture_message(mapi_object_t *obj_store,
			    mapi_object_t *obj_inbox,
			    mapi_object_t *obj_outbox)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	mapi_object_t		obj_message;
	mapi_object_t		obj_table;
	mapi_object_t		obj_attach_table;
	mapi_object_t		obj_attach;
	mapi_object_t		obj_stream;
	struct SPropTagArray	*proptags;
	const char		*recipients[2];
	char			**attrs = NULL;
	struct SRowSet		*rows = NULL;
	struct FlagList		*flaglist = NULL;
	struct SRowSet		msgrows;
	struct SRowSet		attachrows;
	struct SPropValue	value;
	struct SPropValue	props[CN_MSG_PROPS];
	struct SPropValue	*values;
	uint32_t		nvalues;
	uint32_t		msgflag;
	uint32_t		nrows;
	struct SPropValue	attachprops[CN_ATTACH_PROPS];
	uint32_t		nattachprops;
	uint32_t		nattachs;
	uint8_t			*buf_attach;
	uint32_t		sz_attach;
	unsigned int		count = 0;
	uint32_t		index = 0;

	mem_ctx = talloc_init("local");

	/* EmptyFolder
	 */
	oc_test_describe("empty_inbox");
	retval = EmptyFolder(obj_inbox);
	oc_test_assert(retval == MAPI_E_SUCCESS);


	/* CreateMessage
	 */
	oc_test_describe("CreateMessage");
	mapi_object_init(&obj_message);
	retval = CreateMessage(obj_outbox, &obj_message);
	oc_test_assert(retval == MAPI_E_SUCCESS);


	/* ModifyRecipients
	 */
	oc_test_describe("ModifyRecipients");
	proptags = set_SPropTagArray(mem_ctx, 0x6,
				     PR_OBJECT_TYPE,
				     PR_DISPLAY_TYPE,
				     PR_7BIT_DISPLAY_NAME,
				     PR_DISPLAY_NAME,
				     PR_SMTP_ADDRESS,
				     PR_GIVEN_NAME);
	GetProfileAttr(global_mapi_ctx->session->profile, "username", 
		       &count, &attrs);
	recipients[0] = attrs[0];
	recipients[1] = 0;
	retval = ResolveNames(recipients, proptags, &rows, &flaglist, 0);
	mapi_errstr("ResolveNames", GetLastError());

	oc_test_assert(rows != 0);

	value.ulPropTag = PR_SEND_INTERNET_ENCODING;
	value.value.l = 0;
	SRowSet_propcpy(mem_ctx, rows, value);

	set_usernames_RecipientType(mem_ctx, &index, rows, (const char **)recipients, flaglist, MAPI_TO);

	retval = ModifyRecipients(&obj_message, rows);
	mapi_errstr("ModifyRecipients", GetLastError());
	oc_test_assert(retval == MAPI_E_SUCCESS);

	retval = MAPIFreeBuffer(attrs);
	mapi_errstr("MAPIFreeBuffer", GetLastError());

	retval = MAPIFreeBuffer(rows);
	mapi_errstr("MAPIFreeBuffer", GetLastError());

	retval = MAPIFreeBuffer(flaglist);
	mapi_errstr("MAPIFreeBuffer", GetLastError());

	/* SetProps
	 */
	oc_test_describe("SetProps");
	msgflag = MSGFLAG_UNSENT;
	set_SPropValue_proptag(&props[0], PR_SUBJECT, (const void *)MSG_SUBJECT);
	set_SPropValue_proptag(&props[1], PR_BODY, (const void *)MSG_BODY);
	set_SPropValue_proptag(&props[2], PR_MESSAGE_FLAGS, (const void *)&msgflag);
	retval = SetProps(&obj_message, props, CN_MSG_PROPS);
	oc_test_assert(retval == MAPI_E_SUCCESS);

	
	/* CreateAttach
	 */
	oc_test_describe("CreateAttach");
	mapi_object_init(&obj_attach);	
	retval = CreateAttach(&obj_message, &obj_attach);
	oc_test_assert(retval == MAPI_E_SUCCESS);

	attachprops[0].ulPropTag = PR_ATTACH_METHOD;
	attachprops[0].value.l = ATTACH_BY_VALUE;
	attachprops[1].ulPropTag = PR_RENDERING_POSITION;
	attachprops[1].value.l = -1;
	attachprops[2].ulPropTag = PR_ATTACH_DATA_BIN;
	attachprops[2].value.bin.lpb = (uint8_t *)MSG_ATTACH;
	attachprops[2].value.bin.cb = strlen(MSG_ATTACH) + 1;
	attachprops[3].ulPropTag = PR_DISPLAY_NAME;
	attachprops[3].value.lpszA = "attach.txt";
	nattachprops = 4;

	retval = SetProps(&obj_attach, attachprops, nattachprops);
	oc_test_assert(retval == MAPI_E_SUCCESS);

	SaveChanges(&obj_message, &obj_attach);

	mapi_object_release(&obj_attach);


	/* SubmitMessage
	 */
	oc_test_describe("SubmitMessage");
	retval = SubmitMessage(&obj_message);
	oc_test_assert(retval == MAPI_E_SUCCESS);
	mapi_object_release(&obj_message);

	/* time for the message to be available? */
	sleep(2);


	/* Check message content
	 */
	oc_test_describe("check_mail");

	mapi_object_init(&obj_table);
	retval = GetContentsTable(obj_inbox, &obj_table);
	oc_test_assert(retval == MAPI_E_SUCCESS);

	proptags = set_SPropTagArray(mem_ctx, 0x5,
				     PR_FID,
				     PR_MID,
				     PR_INST_ID,
				     PR_INSTANCE_NUM,
				     PR_SUBJECT);
	retval = SetColumns(&obj_table, proptags);
	oc_test_assert(retval == MAPI_E_SUCCESS);

	retval = GetRowCount(&obj_table, &nrows);
	oc_test_assert(nrows >= 1);

	retval = QueryRows(&obj_table, 10, TBL_ADVANCE, &msgrows);
	oc_test_assert(retval == MAPI_E_SUCCESS);
	oc_test_assert(msgrows.cRows == 1);

	mapi_object_init(&obj_message);
	retval = OpenMessage(obj_store,
			     msgrows.aRow[0].lpProps[0].value.d,
			     msgrows.aRow[0].lpProps[1].value.d,
			     &obj_message);
	oc_test_assert(retval == MAPI_E_SUCCESS);

	proptags = set_SPropTagArray(mem_ctx, 0x2,
				     PR_SUBJECT,
				     PR_BODY);
	retval = GetProps(&obj_message, proptags, &values, &nvalues);
	oc_test_assert(retval == MAPI_E_SUCCESS);
	oc_test_assert(nvalues == 2);

	oc_test_assert(strcmp(values[0].value.lpszA, MSG_SUBJECT) == 0);
	oc_test_assert(strcmp(values[1].value.lpszA, MSG_BODY) == 0);

	
	/* GetAttachmentTable
	 */
	oc_test_describe("GetAttachmentTable");

	mapi_object_init(&obj_attach_table);
	retval = GetAttachmentTable(&obj_message, &obj_attach_table);
	oc_test_assert(retval == MAPI_E_SUCCESS);

	retval = GetRowCount(&obj_attach_table, &nattachs);
	oc_test_assert(retval == MAPI_E_SUCCESS);
	oc_test_assert(nattachs == 1);


	/* OpenAttach
	 */
	oc_test_describe("OpenAttach");

	proptags = set_SPropTagArray(mem_ctx, 0x1, PR_ATTACH_NUM);
	retval = SetColumns(&obj_attach_table, proptags);
	oc_test_assert(retval == MAPI_E_SUCCESS);

	retval = QueryRows(&obj_attach_table, 0x1, TBL_ADVANCE, &attachrows);
	oc_test_assert(retval == MAPI_E_SUCCESS);
	oc_test_assert(attachrows.cRows == 1);

	mapi_object_init(&obj_attach);
	retval = OpenAttach(&obj_message, attachrows.aRow[0].lpProps[0].value.l, &obj_attach);
	mapi_errstr("OpenAttach", GetLastError());
	oc_test_assert(retval == MAPI_E_SUCCESS);


	/* ReadAttach
	 */
	oc_test_describe("ReadAttach");

	mapi_object_init(&obj_stream);
	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 0, &obj_stream);
	mapi_errstr("OpenStream", GetLastError());
	oc_test_assert(retval == MAPI_E_SUCCESS);
	retval = read_attach_stream(mem_ctx, &obj_attach, &obj_stream, &buf_attach, &sz_attach);
	oc_test_assert(retval == MAPI_E_SUCCESS);
	oc_test_assert(!strncmp((const char *)buf_attach, MSG_ATTACH, sz_attach));


	mapi_object_release(&obj_stream);
	mapi_object_release(&obj_attach);
	mapi_object_release(&obj_attach_table);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_table);


	/* EmptyFolder
	 */
	retval = EmptyFolder(obj_inbox);
	mapi_errstr("EmptyFolder", GetLastError());

	return True;
}


bool torture_rpc_mapi_message(struct torture_context *torture)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	bool			ret = True;
	mapi_object_t		obj_store;
	mapi_object_t		obj_outbox;
	mapi_object_t		obj_inbox;
	mapi_id_t		id_outbox;
	mapi_id_t		id_inbox;
	struct mapi_session	*session;


	/* init torture */
	mem_ctx = talloc_init("torture_rpc_mapi_message");

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx)) == NULL) return False;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_outbox);
	mapi_object_init(&obj_inbox);

	/* session::OpenMsgStore() */
	retval = OpenMsgStore(&obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;
	mapi_object_debug(&obj_store);

	/* id_outbox = store->GetOutboxFolder() */
	retval = GetOutboxFolder(&obj_store, &id_outbox);
	mapi_errstr("GetOutboxFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* id_inbox = store->GetReceiveFolder() */
	retval = GetReceiveFolder(&obj_store, &id_inbox);
	mapi_errstr("GetReceiveFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* inbox = store->OpenFolder(id_inbox) */
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;
	mapi_object_debug(&obj_inbox);

	/* outbox = store->OpenFolder(id_outbox)
	 */
	retval = OpenFolder(&obj_store, id_outbox, &obj_outbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;
	mapi_object_debug(&obj_outbox);

	/* message->torture() */
	oc_test_begin();
	ret = torture_message(&obj_store, &obj_inbox, &obj_outbox);
	oc_test_end();

	/* objects->Release()
	 */
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_outbox);
	mapi_object_release(&obj_store);

	/* uninitialize mapi
	 */
	MAPIUninitialize();
	talloc_free(mem_ctx);

	return ret;
}
