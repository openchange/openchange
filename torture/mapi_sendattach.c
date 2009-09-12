/*
   OpenChange MAPI torture suite implementation.

   Send attach to an Exchange server

   Copyright (C) Julien Kerihuel 2007.
   Copyright (C) Fabien Le Mentec 2007.

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

#include <sys/stat.h>
#include <fcntl.h>


#define CN_MSG_PROPS 3

bool torture_rpc_mapi_sendattach(struct torture_context *torture)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	bool			ret = true;
	const char		*subject = lp_parm_string(torture->lp_ctx, NULL, "mapi", "subject");
	const char		*body = lp_parm_string(torture->lp_ctx, NULL, "mapi", "body");
	const char		*filename = lp_parm_string(torture->lp_ctx, NULL, "mapi", "attachment");
	const char		**usernames;
	const char		**usernames_to;
	const char		**usernames_cc;
	const char		**usernames_bcc;
	struct mapi_session	*session;
	mapi_object_t		obj_store;
	mapi_object_t		obj_outbox;
	mapi_object_t		obj_message;
	mapi_object_t		obj_attach;
	mapi_object_t		obj_stream;
	uint64_t		id_outbox;
	struct SRowSet		*SRowSet = NULL;
	struct SPropTagArray   	*flaglist = NULL;
	uint32_t		index = 0;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	SPropValue;
	struct SPropValue	props_attach[3];
	unsigned long		cn_props_attach;
	struct SPropValue	props[CN_MSG_PROPS];
	uint32_t		msgflag;
	DATA_BLOB		blob;

	/* get the attachment filename */
	if (!filename) {
		DEBUG(0, ("No filename specified with mapi:attachment\n"));
		return false;
	}

	/* init torture */
	mem_ctx = talloc_named(NULL, 0, "torture_rpc_mapi_sendmail");

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx, torture->lp_ctx)) == NULL) return false;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_outbox);
	mapi_object_init(&obj_message);
	mapi_object_init(&obj_attach);

	/* default if null */
	if (subject == 0) subject = "";
	if (body == 0) body = "";

	/* session::OpenMsgStore() */
	retval = OpenMsgStore(session, &obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* id_outbox = store->GeOutboxFolder() */
	retval = GetOutboxFolder(&obj_store, &id_outbox);
	mapi_errstr("GetOutboxFodler", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* outbox = store->OpenFolder(id_outbox) */
	retval = OpenFolder(&obj_store, id_outbox, &obj_outbox);
	if (retval != MAPI_E_SUCCESS) return false;

	/* message = outbox->CreateMessage() */
	retval = CreateMessage(&obj_outbox, &obj_message);
	mapi_errstr("CreateMessage", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x6,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_7BIT_DISPLAY_NAME,
					  PR_DISPLAY_NAME,
					  PR_SMTP_ADDRESS,
					  PR_GIVEN_NAME);

	usernames_to = get_cmdline_recipients(mem_ctx, "to");
	usernames_cc = get_cmdline_recipients(mem_ctx, "cc");
	usernames_bcc = get_cmdline_recipients(mem_ctx, "bcc");
	usernames = collapse_recipients(mem_ctx, usernames_to, usernames_cc, usernames_bcc);

	retval = ResolveNames(mapi_object_get_session(&obj_outbox), usernames, 
			      SPropTagArray, &SRowSet, &flaglist, 0);
	mapi_errstr("ResolveNames", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	if (!SRowSet) {
	  SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	}

	set_usernames_RecipientType(mem_ctx, &index, SRowSet, usernames_to, flaglist, MAPI_TO);
	set_usernames_RecipientType(mem_ctx, &index, SRowSet, usernames_cc, flaglist, MAPI_CC);
	set_usernames_RecipientType(mem_ctx, &index, SRowSet, usernames_bcc, flaglist, MAPI_BCC);


	SPropValue.ulPropTag = PR_SEND_INTERNET_ENCODING;
	SPropValue.value.l = 0;
	SRowSet_propcpy(mem_ctx, &SRowSet[0], SPropValue);

	/* message->ModifyRecipients() */
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapi_errstr("ModifyRecipients", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	
	retval = MAPIFreeBuffer(SRowSet);
	mapi_errstr("MAPIFreeBuffer: SRowSet", GetLastError());

	retval = MAPIFreeBuffer(flaglist);
	mapi_errstr("MAPIFreeBuffer: flaglist", GetLastError());

	/* message->SetProps()
	 */
	msgflag = MSGFLAG_UNSENT;
	set_SPropValue_proptag(&props[0], PR_SUBJECT, (const void *)subject);
	set_SPropValue_proptag(&props[1], PR_BODY, (const void *)body);
	set_SPropValue_proptag(&props[2], PR_MESSAGE_FLAGS, (const void *)&msgflag);
	retval = SetProps(&obj_message, props, CN_MSG_PROPS);
	mapi_errstr("SetProps", GetLastError());

	/* CreateAttach */
	retval = CreateAttach(&obj_message, &obj_attach);
	mapi_errstr("CreateAttach", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* send by value */
	props_attach[0].ulPropTag = PR_ATTACH_METHOD;
	props_attach[0].value.l = ATTACH_BY_VALUE;
	props_attach[1].ulPropTag = PR_RENDERING_POSITION;
	props_attach[1].value.l = 0;
	props_attach[2].ulPropTag = PR_ATTACH_FILENAME;
	props_attach[2].value.lpszA = get_filename(filename);
	cn_props_attach = 3;

	/* SetProps */
	retval = SetProps(&obj_attach, props_attach, cn_props_attach);
	mapi_errstr("SetProps", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* OpenStream on CreateAttach handle */
	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, OpenStream_Create, &obj_stream);
	mapi_errstr("OpenStream", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* WriteStream */
	{
		int		fd;
		struct stat	sb;
		ssize_t		read_size;
		uint16_t	buf_readsize;
		uint8_t		buf[0x7000];

		if ((fd = open(filename, O_RDONLY)) == -1) {
			DEBUG(0, ("Error while opening %s\n", filename));
			return false;
		}
		if (fstat(fd, &sb) != 0) return false;
	
		while (((read_size = read(fd, buf, 0x4000)) != -1) && read_size) {
			/* We reset errno due to read */
			blob.length = read_size;
			blob.data = talloc_size(mem_ctx, read_size);
			if (read_size > 0) {
				memcpy(blob.data, buf, read_size);
			}
			
			errno = 0;
			retval = WriteStream(&obj_stream, &blob, &buf_readsize);
			mapi_errstr("WriteStream", GetLastError());
			talloc_free(blob.data);
		}
		close(fd);
	}

	/* message->SaveChangesAttachment() */
	retval = SaveChangesAttachment(&obj_message, &obj_attach, KeepOpenReadOnly);
	mapi_errstr("SaveChangesAttachment", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* message->SubmitMessage() */
	retval = SubmitMessage(&obj_message);
	mapi_errstr("SubmitMessage", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* objects->Release()
	 */
	mapi_object_release(&obj_attach);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_outbox);
	mapi_object_release(&obj_store);
	
	/* session::Uninitialize()
	 */
	MAPIUninitialize();

	talloc_free(mem_ctx);
	return ret;
}
