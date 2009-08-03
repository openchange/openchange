/*
   OpenChange MAPI torture suite implementation.

   Send emails to an Exchange server

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


#define CN_MSG_PROPS 3

bool torture_rpc_mapi_sendmail(struct torture_context *torture)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	bool			ret = true;
	const char		*subject = lp_parm_string(torture->lp_ctx, NULL, "mapi", "subject");
	const char		*body = lp_parm_string(torture->lp_ctx, NULL, "mapi", "body");
	const char		**usernames;
	const char		**usernames_to;
	const char		**usernames_cc;
	const char		**usernames_bcc;
	uint32_t		index = 0;
	mapi_object_t		obj_message;
	mapi_object_t		obj_store;
	mapi_object_t		obj_outbox;
	mapi_id_t		id_outbox;
	struct mapi_session	*session;
	struct SRowSet		*SRowSet = NULL;
	struct SPropTagArray   	*flaglist = NULL;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	SPropValue;
	struct SPropValue	props[CN_MSG_PROPS];
	uint32_t		msgflag;


	/* init torture */
	mem_ctx = talloc_named(NULL, 0, "torture_rpc_mapi_sendmail");

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx, torture->lp_ctx)) == NULL) return false;

	/* default if null */
	if (subject == NULL) subject = "";
	if (body == NULL) body = "";

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_outbox);
	mapi_object_init(&obj_message);

	/* session::OpenMsgStore() */
	retval = OpenMsgStore(session, &obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Retrieve the outbox folder id */
	retval = GetDefaultFolder(&obj_store, &id_outbox, olFolderTopInformationStore);
	mapi_errstr("GetDefaultFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* outbox = store->OpenFolder(id_outbox) */
	retval = OpenFolder(&obj_store, id_outbox, &obj_outbox);
	mapi_errstr("OpenFolder", GetLastError());
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

	/* ResolveNames */
	retval = ResolveNames(mapi_object_get_session(&obj_store), usernames, 
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
	SRowSet_propcpy(mem_ctx, SRowSet, SPropValue);
	
	/* ModifyRecipients */
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapi_errstr("ModifyRecipients", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	retval = MAPIFreeBuffer(SRowSet);
	mapi_errstr("MAPIFreeBuffer: SRowSet", GetLastError());

	retval = MAPIFreeBuffer(flaglist);
	mapi_errstr("MAPIFreeBuffer: flaglist", GetLastError());

	/* message->SetProps() */
	msgflag = MSGFLAG_UNSENT;
	set_SPropValue_proptag(&props[0], PR_SUBJECT, (const void *)subject);
	set_SPropValue_proptag(&props[1], PR_BODY, (const void *)body);
	set_SPropValue_proptag(&props[2], PR_MESSAGE_FLAGS, (const void *)&msgflag);

	retval = SetProps(&obj_message, props, CN_MSG_PROPS);
	mapi_errstr("SetProps", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* message->SubmitMessage()
	 */
	retval = SubmitMessage(&obj_message);
	mapi_errstr("SubmitMessage", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* objects->Release()
	 */
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_outbox);
	mapi_object_release(&obj_store);

	/* uninitialize mapi
	 */
	MAPIUninitialize();
	talloc_free(mem_ctx);

	return ret;
}
