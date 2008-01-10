/*
   Stand-alone MAPI testsuite

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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
#include <samba/popt.h>
#include <param.h>
#include <param/proto.h>

#include <utils/openchange-tools.h>
#include <utils/mapitest/mapitest.h>


static void mapitest_call_ModifyRecipients(struct mapitest *mt,
					   mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	mapi_id_t		id_outbox;
	mapi_object_t		obj_outbox;
	mapi_object_t		obj_message;
	char			**username = NULL;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct SPropValue	SPropValue;
	struct SRowSet		*SRowSet = NULL;
	struct FlagList		*flaglist = NULL;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "ModifyRecipients");
	mapitest_indent();

	/* Open the outbox folder */
	mapi_object_init(&obj_outbox);
	retval = GetDefaultFolder(obj_store, &id_outbox, olFolderOutbox);
	MT_ERRNO_IF(mt, retval, "ModifyRecipients:GetDefaultFolder");

	retval = OpenFolder(obj_store, id_outbox, &obj_outbox);
	MT_ERRNO_IF(mt, retval, "ModifyRecipients:OpenFolder");

	/* Create the message */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_outbox, &obj_message);
	MT_ERRNO_IF(mt, retval, "ModifyRecipients:CreateMessage");

	/* Resolve the recipients */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x6,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_7BIT_DISPLAY_NAME,
					  PR_DISPLAY_NAME,
					  PR_SMTP_ADDRESS,
					  PR_GIVEN_NAME);

	username = talloc_array(mt->mem_ctx, char *, 2);
	username[0] = mt->info.username;
	username[1] = NULL;

	retval = ResolveNames((const char **)username, SPropTagArray, 
			      &SRowSet, &flaglist, 0);
	MT_ERRNO_IF(mt, retval, "ModifyRecipients:ResolveNames");

	SPropValue.ulPropTag = PR_SEND_INTERNET_ENCODING;
	SPropValue.value.l = 0;
	SRowSet_propcpy(mt->mem_ctx, SRowSet, SPropValue);

	SetRecipientType(&(SRowSet->aRow[0]), MAPI_TO);
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapitest_print_subcall(mt, "ModifyRecipients: MAPI_TO", GetLastError());

	SetRecipientType(&(SRowSet->aRow[0]), MAPI_CC);
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapitest_print_subcall(mt, "ModifyRecipients: MAPI_CC", GetLastError());

	SetRecipientType(&(SRowSet->aRow[0]), MAPI_BCC);
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapitest_print_subcall(mt, "ModifyRecipients: MAPI_BCC", GetLastError());

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_outbox);
	mapitest_deindent();

	MAPIFreeBuffer(SRowSet);
	MAPIFreeBuffer(flaglist);
	MAPIFreeBuffer(SPropTagArray);
}


static void mapitest_call_SaveChangesMessage(struct mapitest *mt,
					     mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	mapi_id_t		id_outbox;
	mapi_object_t		obj_outbox;
	mapi_object_t		obj_message;
	mapi_id_t		id_msgs[1];

	/* Open the outbox folder */
	mapi_object_init(&obj_outbox);
	retval = GetDefaultFolder(obj_store, &id_outbox, olFolderOutbox);
	MT_ERRNO_IF(mt, retval, "SaveChangesMessage:GetDefaultFolder");

	retval = OpenFolder(obj_store, id_outbox, &obj_outbox);
	MT_ERRNO_IF(mt, retval, "SaveChangesMessage:OpenFolder");

	/* Create the message */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_outbox, &obj_message);
	MT_ERRNO_IF(mt, retval, "SaveChangesMessage:CreateMessage");	

	/* Save the message */
	retval = SaveChangesMessage(&obj_outbox, &obj_message);
	mapitest_print_call(mt, "SaveChangesMessage", GetLastError());

	/* Delete the saved message */
	id_msgs[0] = mapi_object_get_id(&obj_message);
	retval = DeleteMessage(&obj_outbox, id_msgs, 1);
	MT_ERRNO_IF(mt, retval, "SaveChangeMessages:DeleteMessage");

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_outbox);
}


static void mapitest_call_SubmitMessage(struct mapitest *mt,
					mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	mapi_id_t		id_outbox;
	mapi_object_t		obj_outbox;
	mapi_object_t		obj_message;
	uint32_t		msgflag;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct SRowSet		*SRowSet = NULL;
	struct FlagList		*flaglist = NULL;
	struct SPropValue	SPropValue;
	struct SPropValue	props[2];
	char			**username = NULL;
	char			*str = NULL;
	int			i;

	const char		*subjects[] = {
		MT_MAIL_SUBJECT,
		MT_MAIL_SUBJECT_ATTACH,
		MT_MAIL_SUBJECT_READFLAGS,
		NULL
	};

	mapitest_print(mt, MT_HDR_FMT_SECTION, "SubmitMessage");
	mapitest_indent();

	/* Open the outbox folder */
	mapi_object_init(&obj_outbox);
	retval = GetDefaultFolder(obj_store, &id_outbox, olFolderOutbox);
	MT_ERRNO_IF(mt, retval, "SubmitMessage:GetDefaultFolder");

	retval = OpenFolder(obj_store, id_outbox, &obj_outbox);
	MT_ERRNO_IF(mt, retval, "SubmitMessage:OpenFolder");


	for (i = 0; subjects[i] != NULL; i++) {
		/* Create the message */
		mapi_object_init(&obj_message);
		retval = CreateMessage(&obj_outbox, &obj_message);
		MT_ERRNO_IF(mt, retval, "SubmitMessage:CreateMessage");	

		/* Resolve the recipients */
		SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x6,
						  PR_OBJECT_TYPE,
						  PR_DISPLAY_TYPE,
						  PR_7BIT_DISPLAY_NAME,
						  PR_DISPLAY_NAME,
						  PR_SMTP_ADDRESS,
						  PR_GIVEN_NAME);

		username = talloc_array(mt->mem_ctx, char *, 2);
		username[0] = mt->info.username;
		username[1] = NULL;
		
		retval = ResolveNames((const char **)username, SPropTagArray, 
				      &SRowSet, &flaglist, 0);
		MAPIFreeBuffer(SPropTagArray);
		MT_ERRNO_IF(mt, retval, "SubmitMessage:ResolveNames");
		
		SPropValue.ulPropTag = PR_SEND_INTERNET_ENCODING;
		SPropValue.value.l = 0;
		SRowSet_propcpy(mt->mem_ctx, SRowSet, SPropValue);
		
		SetRecipientType(&(SRowSet->aRow[0]), MAPI_TO);
		retval = ModifyRecipients(&obj_message, SRowSet);
		MAPIFreeBuffer(SRowSet);
		MAPIFreeBuffer(flaglist);
		MT_ERRNO_IF(mt, retval, "SubmitMessage:ModifyRecipients");
		
		/* Set the message properties */
		msgflag = MSGFLAG_SUBMIT;
		set_SPropValue_proptag(&props[0], PR_SUBJECT, 
				       (const void *)subjects[i]);
		set_SPropValue_proptag(&props[1], PR_MESSAGE_FLAGS, 
				       (const void *)&msgflag);
		retval = SetProps(&obj_message, props, 2);
		MT_ERRNO_IF(mt, retval, "SubmitMessage:SetProps");
		
		/* Submit the message */
		retval = SubmitMessage(&obj_message);
		str = talloc_asprintf(mt->mem_ctx, "SubmitMessage: %s", subjects[i]);
		mapitest_print_subcall(mt, str, GetLastError());
		talloc_free(str);

		mapi_object_release(&obj_message);
	}

	mapitest_deindent();
	mapi_object_release(&obj_outbox);
}


static void mapitest_call_GetRowCount(struct mapitest *mt, 
				      mapi_object_t *obj_table)
{
	enum MAPISTATUS	retval;
	uint32_t	count;

	retval = GetRowCount(obj_table, &count);
	mapitest_print_call(mt, "GetRowCount", GetLastError());
}


static void mapitest_call_SetColumn(struct mapitest *mt,
				    mapi_object_t *obj_table)
{
	enum MAPISTATUS		retval;
	struct SPropTagArray	*SPropTagArray;

	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3, 
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(obj_table, SPropTagArray);
	mapitest_print_call(mt, "SetColumn", GetLastError());
	MAPIFreeBuffer(SPropTagArray);
}


static void mapitest_call_QueryRows(struct mapitest *mt,
				    mapi_object_t *obj_table)
{
	enum MAPISTATUS		retval;
	struct SRowSet		rowset;
	struct SPropTagArray	*SPropTagArray;
	uint32_t		idx = 0;
	uint32_t		count = 0;

	retval = GetRowCount(obj_table, &count);

	SPropTagArray =set_SPropTagArray(mt->mem_ctx, 0x3,
					 PR_DISPLAY_NAME,
					 PR_FID,
					 PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);

	mapitest_print(mt, MT_HDR_FMT_SECTION, "QueryRows");
	mapitest_indent();

	do {
		retval = QueryRows(obj_table, 0x2, TBL_ADVANCE, &rowset);
		if (rowset.cRows > 0) {
			idx += rowset.cRows;
			if (retval == MAPI_E_SUCCESS) {
				mapitest_print(mt, "%-41s: %.2d/%.2d [PASSED]\n", 
					       "QueryRows", idx, count);
			} else {
				mapitest_print(mt, "%-41s: %.2d/%.2d [FAILED]\n", 
					       "QueryRows", idx, count);
			}
		}
	} while (retval == MAPI_E_SUCCESS && rowset.cRows > 0);

	mapitest_deindent();
}


static void mapitest_call_SeekRow(struct mapitest *mt,
				  mapi_object_t *obj_table)
{
	enum MAPISTATUS		retval;
	char			*str;
	uint32_t		count;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "SeekRow");
	mapitest_indent();

	retval = SeekRow(obj_table, BOOKMARK_BEGINNING, 0, &count);
	str = talloc_asprintf(mt->mem_ctx, "SeekRow: BOOKMARK_BEGINNING");
	mapitest_print_subcall(mt, str, GetLastError());
	talloc_free(str);

	retval = SeekRow(obj_table, BOOKMARK_END, 0, &count);
	str = talloc_asprintf(mt->mem_ctx, "SeekRow: BOOKMARK_END");
	mapitest_print_subcall(mt, str, GetLastError());
	talloc_free(str);


	retval = SeekRow(obj_table, BOOKMARK_CURRENT, 0, &count);
	str = talloc_asprintf(mt->mem_ctx, "SeekRow: BOOKMARK_CURRENT");
	mapitest_print_subcall(mt, str, GetLastError());
	talloc_free(str);


	mapitest_deindent();
}


static void mapitest_call_OpenMessage(struct mapitest *mt, 
				      mapi_object_t *obj_folder,
				      const char *folder_name)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_ctable;
	mapi_object_t		obj_message;
	mapi_id_t		id_msgs[1];
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	uint32_t		count;
	uint32_t		i;
	const char		*subject = NULL;
	char			*str = NULL;
	uint32_t		attempt = 0;

	/* Retrieve the contents table */
	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(obj_folder, &obj_ctable);
	MT_ERRNO_IF_CALL(mt, retval, "OpenMessage", "GetContentsTable");

	/* Customize the content view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_FID,
					  PR_MID,
					  PR_SUBJECT);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MT_ERRNO_IF_CALL(mt, retval, "OpenMessage", "SetColumns");

	/* Count the total number of rows */
	retval = GetRowCount(&obj_ctable, &count);
	MT_ERRNO_IF_CALL(mt, retval, "OpenMessage", "GetRowCount");

	/* Browse the table and search for open messages */
	while (((retval = QueryRows(&obj_ctable, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND) &&
	       SRowSet.cRows) {
		count -= SRowSet.cRows;
		for (i = 0; i < SRowSet.cRows; i++) {
		retry:
			errno = 0;
			mapi_object_init(&obj_message);
			retval = OpenMessage(obj_folder, SRowSet.aRow[i].lpProps[0].value.d,
					     SRowSet.aRow[i].lpProps[1].value.d,
					     &obj_message, MAPI_MODIFY);
			if (GetLastError() != MAPI_E_SUCCESS && attempt < 3) {
				sleep(1);
				attempt++;
				goto retry;
			}
			attempt = 0;

			/* Delete message matching MT_MAIL_SUBJECT */
			if (retval == MAPI_E_SUCCESS) {
				subject = SRowSet.aRow[i].lpProps[2].value.lpszA;
				if (subject && !strncmp(MT_MAIL_SUBJECT, subject, strlen(subject))) {
					str = talloc_asprintf(mt->mem_ctx, "OpenMessage: %-10s: %s", folder_name, subject);
					mapitest_print_call(mt, str, GetLastError());
					talloc_free(str);
					id_msgs[0] = SRowSet.aRow[i].lpProps[1].value.d;
					retval = DeleteMessage(obj_folder, id_msgs, 1);
					MT_ERRNO_IF_CALL(mt, retval, "OpenMessage", "DeleteMessage");
				}
			}
			mapi_object_release(&obj_message);
		}
	}
	mapi_object_release(&obj_ctable);
}


static void mapitest_call_SetReadFlags(struct mapitest *mt,
				       mapi_object_t *obj_folder)
{
	enum MAPISTATUS		retval;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		cValues;
	mapi_object_t		obj_message;
	uint32_t		count;
	bool			bret = false;
	mapi_id_t		id_msgs[1];
	uint32_t		status = 0;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "SetReadFlags");
	mapitest_indent();
	
	count = 0;
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, 
					  PR_MID, 
					  PR_MESSAGE_FLAGS);
	do {
		mapi_object_init(&obj_message);
		bret = mapitest_find_message_subject(mt, obj_folder, &obj_message,
						     MAPI_MODIFY, MT_MAIL_SUBJECT_READFLAGS,
						     "SetReadFlags",
						     &count);
		if (bret == true) {
			/* Retrieve and Save original PR_MESSAGE_FLAGS value */
			retval = GetProps(&obj_message, SPropTagArray, &lpProps, &cValues);
			MT_ERRNO_IF_CALL(mt, GetLastError(), "SetReadFlags", "GetProps");

			if (cValues > 1) {
				status = lpProps[1].value.l;
			}
			MAPIFreeBuffer(lpProps);

			/* Set message flags as read */
			retval = SetReadFlags(obj_folder, &obj_message, MSGFLAG_READ);
			mapitest_print_subcall(mt, "SetReadFlags: MSGFLAG_READ", GetLastError());

			/* Check if the operation was successful */
			retval = GetProps(&obj_message, SPropTagArray, &lpProps, &cValues);
			MT_ERRNO_IF_CALL(mt, GetLastError(), "SetReadFlags", "GetProps");
			if (cValues > 1 && status != lpProps[1].value.l) {
				mapitest_print_subcall(mt, "SetReadFlags: PR_MESSAGE_FLAGS changed", MAPI_E_SUCCESS);
				status = lpProps[1].value.l;
			} else {
				mapitest_print_subcall(mt, "SetReadFlags: PR_MESSAGE_FLAGS failed", MAPI_E_RESERVED);
			}
			MAPIFreeBuffer(lpProps);

			/* Set the message flags as submitted */
			retval = SetReadFlags(obj_folder, &obj_message, MSGFLAG_SUBMIT);
			mapitest_print_subcall(mt, "SetReadFlags: MSGFLAG_SUBMIT", GetLastError());
			
			/* Check if the operation was successful */
			retval = GetProps(&obj_message, SPropTagArray, &lpProps, &cValues);
			MT_ERRNO_IF_CALL(mt, GetLastError(), "SetReadFlags", "GetProps");
			if (cValues > 1 && status != lpProps[1].value.l) {
				mapitest_print_subcall(mt, "SetReadFlags: PR_MESSAGE_FLAGS changed", MAPI_E_SUCCESS);
				status = lpProps[1].value.l;
			} else {
				mapitest_print_subcall(mt, "SetReadFlags: PR_MESSAGE_FLAGS failed", MAPI_E_RESERVED);
			}

			/* Finally delete the message */
			if (cValues) {
				id_msgs[0] = lpProps[0].value.d;
				retval = DeleteMessage(obj_folder, id_msgs, 1);
				MT_ERRNO_IF_CALL(mt, GetLastError(), "SetReadFlags", "DeleteMessage");
				mapi_object_release(&obj_message);
			}
			MAPIFreeBuffer(lpProps);
		}
	} while (bret == true);
	MAPIFreeBuffer(SPropTagArray);

	mapitest_deindent();
}


void mapitest_calls_ring4(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	mapi_id_t		id_folder;
	uint32_t		attempt = 0;

	mapitest_print_interface_start(mt, "Fourth Ring");

retry:
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS && attempt < 3) {
		attempt++;
		goto retry;
	}

	mapitest_call_ModifyRecipients(mt, &obj_store);
	mapitest_call_SaveChangesMessage(mt, &obj_store);
	mapitest_call_SubmitMessage(mt, &obj_store);
	
	mapi_object_init(&obj_folder);
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);

	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable);

	mapitest_call_GetRowCount(mt, &obj_htable);
	mapitest_call_SetColumn(mt, &obj_htable);
	mapitest_call_QueryRows(mt, &obj_htable);
	mapitest_call_SeekRow(mt, &obj_htable);

	/* Open outbox folder where we have stored emails in previous tests */
	mapi_object_release(&obj_folder);
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderOutbox);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);

	mapitest_call_OpenMessage(mt, &obj_folder, "Outbox");

	mapi_object_release(&obj_folder);

	/* Open inbox folder so we can remove submitted messages */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderInbox);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);

	mapitest_call_OpenMessage(mt, &obj_folder, "Inbox");
	mapitest_call_SetReadFlags(mt, &obj_folder);

	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);
	mapitest_print_interface_end(mt);
}
