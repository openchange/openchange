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

#include <utils/openchange-tools.h>
#include <utils/mapitest/mapitest.h>


static void mapitest_call_ModifyRecipients(struct mapitest *mt,
					   mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
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
	ret = mapitest_folder_open(mt, obj_store, &obj_outbox, 
				   olFolderOutbox, "ModifyRecipients");
	if (ret == false) goto end;

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

	MAPIFreeBuffer(SRowSet);
	MAPIFreeBuffer(flaglist);
	MAPIFreeBuffer(SPropTagArray);

end:
	mapitest_deindent();
}


static void mapitest_call_SaveChangesMessage(struct mapitest *mt,
					     mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
	mapi_object_t		obj_outbox;
	mapi_object_t		obj_message;
	mapi_id_t		id_msgs[1];

	/* Open the outbox folder */
	mapi_object_init(&obj_outbox);
	ret = mapitest_folder_open(mt, obj_store, &obj_outbox, 
				   olFolderOutbox, "SaveChangesMessage");
	if (ret == false) return;

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
	bool			ret = false;
	mapi_object_t		obj_outbox;
	mapi_object_t		obj_message;
	char			*str = NULL;
	int			i;

	const char		*subjects[] = {
		MT_MAIL_SUBJECT,
		MT_MAIL_SUBJECT_READFLAGS,
		NULL
	};

	mapitest_print(mt, MT_HDR_FMT_SECTION, "SubmitMessage");
	mapitest_indent();

	/* Open the outbox folder */
	mapi_object_init(&obj_outbox);
	ret = mapitest_folder_open(mt, obj_store, &obj_outbox, 
				   olFolderOutbox, "SubmitMessage");
	if (ret == false) goto end;
	
	for (i = 0; subjects[i] != NULL; i++) {
		mapi_object_init(&obj_message);
		ret = mapitest_message_create(mt, &obj_outbox, &obj_message, 
					      subjects[i], "SubmitMessage");
		if (ret == true) {
			/* Submit the message */
			retval = SubmitMessage(&obj_message);
			str = talloc_asprintf(mt->mem_ctx, "SubmitMessage: %s", subjects[i]);
			mapitest_print_subcall(mt, str, GetLastError());
			talloc_free(str);
			
			mapi_object_release(&obj_message);
		}
	}

end:
	mapitest_deindent();
	mapi_object_release(&obj_outbox);
}


static void mapitest_call_GetMessageStatus(struct mapitest *mt, 
					   mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_ctable;
	uint32_t		i;
	uint32_t		count;
	uint32_t		status;

	/* Open Inbox folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_folder_open(mt, obj_store, &obj_folder, 
				   olFolderInbox, "GetMessageStatus");
	if (ret == false) return;

	/* Retrieve the contents table */
	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(&obj_folder, &obj_ctable);
	MT_ERRNO_IF_CALL(mt, retval, "GetMessageStatus", "GetContentsTable");

	/* Customize the contents table view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, 
					  PR_MID,
					  PR_MSG_STATUS);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MT_ERRNO_IF_CALL(mt, retval, "GetMessageStatus", "SetColumns");

	
	/* Get the total number of rows */
	retval = GetRowCount(&obj_ctable, &count);
	MT_ERRNO_IF_CALL(mt, retval, "GetMessageStatus", "GetRowCount");
	if (count == 0) goto end;

	/* Loop through the table and call GetMessageStatus */
	while (((retval = QueryRows(&obj_ctable, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND) &&
	       SRowSet.cRows) {
		count -= SRowSet.cRows;
		for (i = 0; i < SRowSet.cRows; i++) {
			retval = GetMessageStatus(&obj_folder, SRowSet.aRow[i].lpProps[0].value.d, &status);
			mapitest_print_call(mt, "GetMessageStatus", GetLastError());
		}
	}

end:
	mapi_object_release(&obj_ctable);
	mapi_object_release(&obj_folder);
}


struct msgstatus {
	uint32_t	status;
	const char	*name;
};

static struct msgstatus msgstatus[] = {
	{MSGSTATUS_HIDDEN,		"MSGSTATUS_HIDDEN"},
	{MSGSTATUS_HIGHLIGHTED,		"MSGSTATUS_HIGHLIGHTED"},
	{MSGSTATUS_TAGGED,		"MSGSTATUS_TAGGED"},
	{MSGSTATUS_REMOTE_DOWNLOAD,	"MSGSTATUS_REMOTE_DOWNLOAD"},
	{MSGSTATUS_REMOTE_DELETE,	"MSGSTATUS_REMOTE_DELETE"},
	{MSGSTATUS_DELMARKED,		"MSGSTATUS_DELMARKED"},
	{0,				NULL}
};


static void mapitest_call_SetMessageStatus(struct mapitest *mt,
					   mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	bool			ret;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_ctable;
	uint32_t		i;
	uint32_t		count;
	uint32_t		ulOldStatus;
	uint32_t		ulOldStatus2;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "SetMessageStatus");
	mapitest_indent();

	/* Open Inbox folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_folder_open(mt, obj_store, &obj_folder, 
				   olFolderInbox, "SetMessageStatus");
	if (ret == false) {
		mapitest_deindent();
		return;
	}

	/* Retrieve the contents table */
	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(&obj_folder, &obj_ctable);
	MT_ERRNO_IF_CALL(mt, retval, "SetMessageStatus", "GetContentsTable");

	/* Customize the contents table view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2,
					  PR_MID,
					  PR_MSG_STATUS);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MT_ERRNO_IF_CALL(mt, retval, "SetMessageStatus", "SetColumns");

	/* Get the total number of rows */
	retval = GetRowCount(&obj_ctable, &count);
	MT_ERRNO_IF_CALL(mt, retval, "SetMessageStatus", "GetRowCount");
	if (count == 0) goto end;

	/* Fetch the first email */
	retval = QueryRows(&obj_ctable, 1, TBL_NOADVANCE, &SRowSet);
	MT_ERRNO_IF_CALL(mt, retval, "SetMessageStatus", "QueryRows");


	/* Test MSGSTATUS_HIDDEN */
	for (i = 0; msgstatus[i].name; i++) {
		retval = SetMessageStatus(&obj_folder, SRowSet.aRow[0].lpProps[0].value.d,
					  msgstatus[i].status, msgstatus[i].status, &ulOldStatus2);
		mapitest_print_subcall(mt, "SetMessageStatus", GetLastError());

		retval = GetMessageStatus(&obj_folder, SRowSet.aRow[0].lpProps[0].value.d, &ulOldStatus);
		MT_ERRNO_IF_CALL(mt, retval, "SetMessageStatus", "GetMessageStatus");

		if ((ulOldStatus != ulOldStatus2) && (ulOldStatus & msgstatus[i].status)) {
			errno = 0;
			mapitest_print_subcall(mt, msgstatus[i].name, GetLastError());
		}
	}
	

end:
	mapi_object_release(&obj_ctable);
	mapi_object_release(&obj_folder);
	mapitest_deindent();
}


static void mapitest_call_GetRowCount(struct mapitest *mt, 
				      mapi_object_t *obj_table)
{
	enum MAPISTATUS	retval;
	uint32_t	count;

	retval = GetRowCount(obj_table, &count);
	mapitest_print_call(mt, "GetRowCount", GetLastError());
}


static void mapitest_call_SetColumns(struct mapitest *mt,
				    mapi_object_t *obj_table)
{
	enum MAPISTATUS		retval;
	struct SPropTagArray	*SPropTagArray;

	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3, 
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(obj_table, SPropTagArray);
	mapitest_print_call(mt, "SetColumns", GetLastError());
	MAPIFreeBuffer(SPropTagArray);
}


static void mapitest_call_QueryColumns(struct mapitest *mt,
				       mapi_object_t *obj_table)
{
	enum MAPISTATUS		retval;
	struct SPropTagArray	columns;

	retval = QueryColumns(obj_table, &columns);
	mapitest_print_call(mt, "QueryColumns", GetLastError());
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


static void mapitest_call_SeekRowApprox(struct mapitest *mt,
					mapi_object_t *obj_table)
{
	enum MAPISTATUS		retval;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "SeekRowApprox");
	mapitest_indent();

	retval = SeekRowApprox(obj_table, 0, 1);
	mapitest_print_subcall(mt, "SeekRowApprox 0/1", GetLastError());

	retval = SeekRowApprox(obj_table, 1, 1);
	mapitest_print_subcall(mt, "SeekRowApprox 1/1", GetLastError());

	retval = SeekRowApprox(obj_table, 1, 2);
	mapitest_print_subcall(mt, "SeekRowApprox 1/2", GetLastError());

	mapitest_deindent();
}


static void mapitest_call_SortTable(struct mapitest *mt,
				    mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_ctable;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	struct SSortOrderSet	lpSortCriteria;
	uint32_t		count;
	const char     		*msgid;
	uint32_t		attempt = 0;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "SortTable");
	mapitest_indent();

	/* Open Inbox folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_folder_open(mt, obj_store, &obj_folder, olFolderInbox, "SortTable");
	if (ret == false) return;

	/* Get Contents Table */
	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(&obj_folder, &obj_ctable);
	MT_ERRNO_IF_CALL(mt, retval, "SortTable", "GetContentsTable");

	/* Customize table view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, 
					  PR_INTERNET_MESSAGE_ID,
					  PR_LAST_MODIFICATION_TIME);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MT_ERRNO_IF_CALL(mt, retval, "SortTable", "SetColumns");

	/* Get the total number of rows */
	retval = GetRowCount(&obj_ctable, &count);
	if (count >= 2) {
		errno = 0;
		mapitest_print_subcall(mt, "Enough messages to process", GetLastError());
	}

	/* Retrieve the first message: We retry 5 times with sleep 1
	 * in case submitted message has not yet been dispatched into
	 * Inbox.  It is a bit messy and this should be cleaned when
	 * notifications are implemented.
	 */
retry:
	retval = QueryRows(&obj_ctable, 1, TBL_NOADVANCE, &SRowSet);
	MT_ERRNO_IF_CALL(mt, retval, "SortTable", "QueryRows");
	if ((SRowSet.cRows == 0 || !SRowSet.aRow[0].lpProps[0].value.lpszA) && attempt < 5) {
		sleep(1);
		attempt++;
		goto retry;
	}
	if (SRowSet.cRows == 0 || !SRowSet.aRow[0].lpProps[0].value.lpszA) {
		errno = -1;
		mapitest_print_subcall(mt, "No messages available", GetLastError());
		errno = 0;
		goto end;
	}
	msgid = SRowSet.aRow[0].lpProps[0].value.lpszA;

	/* Sort Table: ascending PR_LAST_MODIFICATION_TIME */
	lpSortCriteria.cSorts = 1;
	lpSortCriteria.cCategories = 0;
	lpSortCriteria.cExpanded = 0;
	lpSortCriteria.aSort = talloc_array(mt->mem_ctx, struct SSortOrder, 1);
	lpSortCriteria.aSort[0].ulPropTag = PR_LAST_MODIFICATION_TIME;
	lpSortCriteria.aSort[0].ulOrder = TABLE_SORT_ASCEND;
	
	retval = SortTable(&obj_ctable, &lpSortCriteria);
	mapitest_print_subcall(mt, "SortTable", GetLastError());

	/* Retrieve the "New" first msgid */
	retval = QueryRows(&obj_ctable, 1, TBL_NOADVANCE, &SRowSet);
	MT_ERRNO_IF_CALL(mt, retval, "SortTable", "QueryRows");

	if (SRowSet.aRow[0].lpProps[0].value.lpszA && msgid &&
	    strcmp(msgid, SRowSet.aRow[0].lpProps[0].value.lpszA)) {
		errno = 0;
		mapitest_print_subcall(mt, "msgid has changed", GetLastError());
	} else {
		errno = -1;
		mapitest_print_subcall(mt, "msgid has not changed", GetLastError());
	}

end:
	mapi_object_release(&obj_ctable);
	mapi_object_release(&obj_folder);

	mapitest_deindent();
}


static void mapitest_call_Bookmark(struct mapitest *mt,
				  mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_ctable;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	uint32_t		count;
	uint32_t		*bkPosition;
	uint32_t		i;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "Bookmark");
	mapitest_indent();

	/* Open Inbox folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_folder_open(mt, obj_store, &obj_folder, olFolderInbox, "Bookmark");
	if (ret == false) return;

	/* Get Contents Table */
	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(&obj_folder, &obj_ctable);
	MT_ERRNO_IF_CALL(mt, retval, "Bookmark", "GetContentsTable");

	/* Customize table view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2,
					  PR_INTERNET_MESSAGE_ID,
					  PR_LAST_MODIFICATION_TIME);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MT_ERRNO_IF_CALL(mt, retval, "Bookmark", "SetColumns");

	/* Get the total number of rows */
	retval = GetRowCount(&obj_ctable, &count);
	MT_ERRNO_IF_CALL(mt, retval, "Bookmark", "GetRowCount");

	bkPosition = talloc_array(mt->mem_ctx, uint32_t, 1);
	while (((retval = QueryRows(&obj_ctable, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND) &&
	       SRowSet.cRows) {
		count -= SRowSet.cRows;
		for (i = 0; i < SRowSet.cRows; i++) {
			bkPosition = talloc_realloc(mt->mem_ctx, bkPosition, uint32_t, i + 2);
			retval = CreateBookmark(&obj_ctable, &(bkPosition[i]));
			mapitest_print_subcall(mt, "CreateBookmark", GetLastError());
		}
	}

	retval = mapi_object_bookmark_get_count(&obj_ctable, &count);

	for (i = 0; i < count; i++) {
		retval = FreeBookmark(&obj_ctable, bkPosition[i]);
		mapitest_print_subcall(mt, "FreeBookmark", GetLastError());
	}

	mapi_object_release(&obj_ctable);
	mapi_object_release(&obj_folder);
	mapitest_deindent();
}


static void mapitest_call_SeekRowBookmark(struct mapitest *mt,
					  mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_ctable;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	uint32_t		row;
	uint32_t		count;
	uint32_t		*bkPosition;
	uint32_t		i;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "SeekRowBookmark");
	mapitest_indent();

	/* Open Inbox folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_folder_open(mt, obj_store, &obj_folder, olFolderInbox, "SeekRowBookmark");
	if (ret == false) return;

	/* Get Contents Table */
	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(&obj_folder, &obj_ctable);
	MT_ERRNO_IF_CALL(mt, retval, "SeekRowBookmark", "GetContentsTable");

	/* Customize table view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2,
					  PR_INTERNET_MESSAGE_ID,
					  PR_LAST_MODIFICATION_TIME);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MT_ERRNO_IF_CALL(mt, retval, "SeekRowBookmark", "SetColumns");

	/* Get the total number of rows */
	retval = GetRowCount(&obj_ctable, &count);
	MT_ERRNO_IF_CALL(mt, retval, "SeekRowBookmark", "GetRowCount");

	bkPosition = talloc_array(mt->mem_ctx, uint32_t, 1);
	while (((retval = QueryRows(&obj_ctable, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND) &&
	       SRowSet.cRows) {
		count -= SRowSet.cRows;
		for (i = 0; i < SRowSet.cRows; i++) {
			bkPosition = talloc_realloc(mt->mem_ctx, bkPosition, uint32_t, i + 2);
			retval = CreateBookmark(&obj_ctable, &(bkPosition[i]));
			MT_ERRNO_IF_CALL(mt, retval, "SeekRowBookmark", "CreateBookmark");
		}
	}

	retval = mapi_object_bookmark_get_count(&obj_ctable, &count);

	for (i = 0; i < count; i++) {
		retval = SeekRowBookmark(&obj_ctable, bkPosition[i], 0, &row);
		mapitest_print_subcall(mt, "SeekRowBookmark", GetLastError());
	}

	for (i = 0; i < count; i++) {
		retval = FreeBookmark(&obj_ctable, bkPosition[i]);
		MT_ERRNO_IF_CALL(mt, retval, "SeekRowBookmark", "FreeBookmark");
	}

	mapi_object_release(&obj_ctable);
	mapi_object_release(&obj_folder);
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


static void mapitest_call_CreateAttach(struct mapitest *mt, 
				       mapi_object_t *obj_folder)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
	mapi_object_t		obj_message;
	mapi_object_t		obj_attach;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "CreateAttach");
	mapitest_indent();

	mapi_object_init(&obj_message);
	ret = mapitest_message_create(mt, obj_folder, &obj_message, 
				      MT_MAIL_SUBJECT_ATTACH, "CreateAttach");
	if (ret == false) goto end;

	/* Create the attachment */
	mapi_object_init(&obj_attach);
	retval = CreateAttach(&obj_message, &obj_attach);
	mapitest_print_subcall(mt, "CreateAttach", GetLastError());

	mapi_object_release(&obj_attach);
	mapi_object_release(&obj_message);

end:
	mapitest_deindent();
}


static void mapitest_call_SaveChanges(struct mapitest *mt,
				      mapi_object_t *obj_folder)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
	mapi_object_t		obj_message;
	mapi_object_t		obj_attach;
	struct SPropValue	lpProps[4];

	mapitest_print(mt, MT_HDR_FMT_SECTION, "SaveChanges");
	mapitest_indent();

	mapi_object_init(&obj_message);
	ret = mapitest_message_create(mt, obj_folder, &obj_message,
				      MT_MAIL_SUBJECT_ATTACH, "SaveChanges");
	if (ret == false) goto end;

	/* Create the attachment */
	mapi_object_init(&obj_attach);
	retval = CreateAttach(&obj_message, &obj_attach);
	MT_ERRNO_IF_CALL(mt, retval, "SaveChanges", "CreateAttach");

	/* Set the attachment */
	lpProps[0].ulPropTag= PR_ATTACH_METHOD;
	lpProps[0].value.l = ATTACH_BY_VALUE;
	lpProps[1].ulPropTag = PR_RENDERING_POSITION;
	lpProps[1].value.l = -1;
	lpProps[2].ulPropTag = PR_ATTACH_DATA_BIN;
	lpProps[2].value.bin.lpb = (uint8_t *) MT_MAIL_ATTACH_BLOB;
	lpProps[2].value.bin.cb = strlen(MT_MAIL_ATTACH_BLOB) + 1;
	lpProps[3].ulPropTag = PR_DISPLAY_NAME;
	lpProps[3].value.lpszA = MT_MAIL_ATTACH_NAME;

	retval = SetProps(&obj_attach, lpProps, 4);
	MT_ERRNO_IF_CALL(mt, retval, "SaveChanges", "SetProps");

	/* Test the call we were looking for */
	retval = SaveChanges(&obj_message, &obj_attach, KEEP_OPEN_READONLY);
	mapitest_print_subcall(mt, "SaveChanges", GetLastError());
	mapi_object_release(&obj_attach);

	/* Finally submit the message */
	retval = SubmitMessage(&obj_message);
	MT_ERRNO_IF_CALL(mt, retval, "SaveChanges", "SubmitMessage");


	mapi_object_release(&obj_message);

end:
	mapitest_deindent();
}


static void mapitest_call_GetAttachmentTable(struct mapitest *mt,
					     mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_object_t		obj_atable;
	uint32_t		count = 0;

	/* Open Inbox folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_folder_open(mt, obj_store, &obj_folder, olFolderInbox, "GetAttachmentTable");
	if (ret == false) return;
	
	/* Find message */
	mapi_object_init(&obj_message);
	ret = mapitest_message_find_subject(mt, &obj_folder, &obj_message, 0,
					    MT_MAIL_SUBJECT_ATTACH, "GetAttachmentTable", &count);

	if (ret == false) return;
	/* Retrieve attachment table */
	mapi_object_init(&obj_atable);
	retval = GetAttachmentTable(&obj_message, &obj_atable);
	mapitest_print_call(mt, "GetAttachmentTable", GetLastError());

	mapi_object_release(&obj_atable);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
}


static void mapitest_call_DeleteAttach(struct mapitest *mt,
				       mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	bool			ret = false;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_object_t		obj_atable;
	uint32_t		count = 0;
	uint32_t		count2 = 0;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "DeleteAttach");
	mapitest_indent();

	/* Open Inbox folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_folder_open(mt, obj_store, &obj_folder, olFolderInbox, "DeleteAttach");
	if (ret == false) goto end;

	/* Find Message */
	mapi_object_init(&obj_message);
	ret = mapitest_message_find_subject(mt, &obj_folder, &obj_message, MAPI_MODIFY, 
					    MT_MAIL_SUBJECT_ATTACH, "DeleteAttach", &count);
	if (ret == false) goto end;

	mapi_object_init(&obj_atable);
	retval = GetAttachmentTable(&obj_message, &obj_atable);
	MT_ERRNO_IF_CALL(mt, retval, "DeleteAttach", "GetAttachmentTable");
	
	count = 0;
	retval = GetRowCount(&obj_atable, &count);
	MT_ERRNO_IF_CALL(mt, retval, "DeleteAttach", "GetRowCount");

	if (count > 0) {
		retval = DeleteAttach(&obj_message, 0);
		mapitest_print_subcall(mt, "DeleteAttach", GetLastError());

		retval = SaveChangesMessage(&obj_folder, &obj_message);
		MT_ERRNO_IF_CALL(mt, retval, "DeleteAttach", "SaveChangesMessage");
	}

	count2 = 0;
	retval = GetRowCount(&obj_atable, &count2);
	MT_ERRNO_IF_CALL(mt, retval, "DeleteAttach", "GetRowCount");

	errno = (count2 < count) ? 0 : -1;
	mapitest_print_subcall(mt, "DeleteAttach Effectiveness", GetLastError());
	errno = 0;
	
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
end:
	mapitest_deindent();
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
		bret = mapitest_message_find_subject(mt, obj_folder, &obj_message,
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
	mapitest_call_SetColumns(mt, &obj_htable);
	mapitest_call_QueryColumns(mt, &obj_htable);
	mapitest_call_QueryRows(mt, &obj_htable);
	mapitest_call_SeekRow(mt, &obj_htable);
	mapitest_call_SeekRowApprox(mt, &obj_htable);
	mapitest_call_SortTable(mt, &obj_store);

	mapitest_call_Bookmark(mt, &obj_store);
	mapitest_call_SeekRowBookmark(mt, &obj_store);

	mapitest_call_GetMessageStatus(mt, &obj_store);
	mapitest_call_SetMessageStatus(mt, &obj_store);

	/* Open outbox folder where we have stored emails in previous tests */
	mapi_object_release(&obj_folder);
	mapi_object_init(&obj_folder);
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderOutbox);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);

	mapitest_call_OpenMessage(mt, &obj_folder, "Outbox");
	mapitest_call_CreateAttach(mt, &obj_folder);
	mapitest_call_SaveChanges(mt, &obj_folder);
	mapitest_call_GetAttachmentTable(mt, &obj_store);
	mapitest_call_DeleteAttach(mt, &obj_store);

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
