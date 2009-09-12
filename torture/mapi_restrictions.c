/*
   OpenChange MAPI torture suite implementation.

   Test Restrictions

   Copyright (C) Julien Kerihuel 2007 - 2008.

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
#include <gen_ndr/ndr_exchange.h>
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>

#define	SAME_SUBJECT		"Same subject"
#define	SAME_SUBJECT_BODY	"Same subject and body"
#define	UNIQUE_BODY		"The secret word is OpenChange and is hidden"

bool torture_create_environment(struct loadparm_context *lp_ctx,
				TALLOC_CTX *mem_ctx, 
				mapi_object_t *parent,
				mapi_object_t *child)
{
	enum MAPISTATUS	retval;
	char		*subject = NULL;
	char		*body = NULL;
	uint32_t	i;

	/* Create the test directory */
	mapi_object_init(child);
	retval = CreateFolder(parent, FOLDER_GENERIC, 
			      "torture_restrictions", 
			      "MAPI restrictions torture test", 
			      OPEN_IF_EXISTS, child);
	if (retval != MAPI_E_SUCCESS) return false;
	DEBUG(0, ("[+] torture restrictions directory created\n"));
	
	retval = EmptyFolder(child);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Send 5 mails with MSGFLAG_READ set */
	for (i = 0; i < 5; i++) {
		subject = talloc_asprintf(mem_ctx, "Subject: %s %d", 
					  "MSGFLAG_READ: Sample mail", i);
		retval = torture_simplemail_fromme(lp_ctx, child, subject, 
						   "This is sample content", 
						   MSGFLAG_READ|MSGFLAG_SUBMIT);
		talloc_free(subject);
		if (retval != MAPI_E_SUCCESS) return false;
	}
	DEBUG(0, ("[+] 5 mails created with MSGFLAG_READ set\n"));

	/* Send 5 mails with MSGFLAG_UNREAD set */
	for (i = 0; i < 5; i++) {
		subject = talloc_asprintf(mem_ctx, "Subject: %s %d", 
					  "Sample mail", i);
		retval = torture_simplemail_fromme(lp_ctx, child, subject, 
						   "This is sample content", 
						   MSGFLAG_SUBMIT);
		talloc_free(subject);
		if (retval != MAPI_E_SUCCESS) return false;
	}
	DEBUG(0, ("[+] 5 unread mails created\n"));

	/* Create 2 mails with the same subject */
	for (i = 0; i < 2; i++) {
		retval = torture_simplemail_fromme(lp_ctx, child, SAME_SUBJECT,
						   "Different content",
						   MSGFLAG_SUBMIT);
		if (retval != MAPI_E_SUCCESS) return false;
	}
	DEBUG(0, ("[+] 2 mails unread with same subject but different body\n"));


	/* Create 3 mails with the same subject and same body */
	for (i = 0; i < 3; i++) {
		retval = torture_simplemail_fromme(lp_ctx, child, SAME_SUBJECT_BODY,
						   SAME_SUBJECT_BODY,
						   MSGFLAG_SUBMIT);
		if (retval != MAPI_E_SUCCESS) return false;
	}
	DEBUG(0, ("[+] 3 mails unread with same subject and body\n"));


	/* Create 1 mail with a long body */
	body = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
	retval = torture_simplemail_fromme(lp_ctx, child, "Long body", body, MSGFLAG_SUBMIT);
	if (retval != MAPI_E_SUCCESS) return false;
	DEBUG(0, ("[+] 1 mail with  body > 39 chars\n"));

	/* Create 1 mail with a unique body content */
	retval = torture_simplemail_fromme(lp_ctx, child, "Unique content", UNIQUE_BODY, MSGFLAG_SUBMIT);
	if (retval != MAPI_E_SUCCESS) return false;
	DEBUG(0, ("[+] 1 mail with  unique body: %s\n", UNIQUE_BODY));
	

	return true;
}

bool torture_rpc_mapi_restrictions(struct torture_context *torture)
{
	NTSTATUS			nt_status;
	enum MAPISTATUS			retval;
	struct dcerpc_pipe		*p;
	TALLOC_CTX			*mem_ctx;
	bool				ret = true;
	struct mapi_session		*session;
	mapi_object_t			obj_store;
	mapi_object_t			obj_inbox;
	mapi_object_t			obj_table;
	mapi_object_t			obj_testdir;
	mapi_id_t			id_inbox;
	struct SPropTagArray		*SPropTagArray;
	struct SRowSet			SRowSet;
	struct SRowSet			SRowSet_row;
	struct mapi_SRestriction	res;
	uint32_t			total;
	uint32_t			Numerator;
	uint32_t			Denominator;
	uint32_t			row_idx;
	enum BOOKMARK			bookmark;


	/* init torture */
	mem_ctx = talloc_named(NULL, 0, "torture_rpc_mapi_restrictions");
	nt_status = torture_rpc_connection(torture, &p, &ndr_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return false;
	}

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx, torture->lp_ctx)) == NULL) return false;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_inbox);
	mapi_object_init(&obj_table);

	/* Open Message Store */
	retval = OpenMsgStore(session, &obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	retval = GetDefaultFolder(&obj_store, &id_inbox, olFolderInbox);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Open Inbox folder */
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Create test environment */
	if (torture_create_environment(torture->lp_ctx, mem_ctx, &obj_inbox, &obj_testdir) != true) {
		return false;
	}

	/* Get Contents Table */
	retval = GetContentsTable(&obj_testdir, &obj_table, 0, &total);
	if (retval != MAPI_E_SUCCESS) return false;

	DEBUG(0, ("Total number of mails = %d\n", total));

	/* Filter contents table on subject */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x6,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT,
					  PR_MESSAGE_FLAGS);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;

	DEBUG(0, ("\nStep 1. Test Restrict MAPI call\n"));
	DEBUG(0, ("===============================\n"));

	/* RES_PROPERTY test */
	res.rt = RES_PROPERTY;
	res.res.resProperty.relop = RES_PROPERTY;
	res.res.resProperty.ulPropTag = PR_SUBJECT;
	res.res.resProperty.lpProp.ulPropTag = PR_SUBJECT;
	res.res.resProperty.lpProp.value.lpszA = "Same subject";

	retval = Restrict(&obj_table, &res, NULL);
	if (retval != MAPI_E_SUCCESS) return false;
	
	retval = QueryPosition(&obj_table, &Numerator, &Denominator);
	if (retval != MAPI_E_SUCCESS) return false;

	DEBUG(0, ("\no Restriction: RES_PROPERTY\n"));
	DEBUG(0, ("  -------------------------\n"));
	DEBUG(0, ("\tFilter on PR_SUBJECT\n"));
	DEBUG(0, (("\tCheck for subject eq \"%s\"\n"), SAME_SUBJECT));
	DEBUG(0, ("\tCursor is at %d / %d\n", Numerator, Denominator));

	/* RES_BITMASK test */
	res.rt = RES_BITMASK;
	res.res.resBitmask.relMBR = BMR_NEZ;
	res.res.resBitmask.ulPropTag = PR_MESSAGE_FLAGS;
	res.res.resBitmask.ulMask = MSGFLAG_READ;

	retval = Restrict(&obj_table, &res, NULL);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = QueryPosition(&obj_table, &Numerator, &Denominator);
	if (retval != MAPI_E_SUCCESS) return false;

	DEBUG(0, ("\no Restriction: RES_BITMASK\n"));
	DEBUG(0, ("  --------------------------\n"));
	DEBUG(0, ("\tFilter on PR_MESSAGE_FLAG bitmask\n"));
	DEBUG(0, ("\tCheck for all emails with MSGFLAG_READ set\n"));
	DEBUG(0, ("\tCursor is at %d / %d\n", Numerator, Denominator));

	/* RES_SIZE test */
	res.rt = RES_SIZE;
	res.res.resSize.relop = RELOP_GT;
	res.res.resSize.ulPropTag = PR_BODY;
	res.res.resSize.size = 30;

	retval = Restrict(&obj_table, &res, NULL);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = QueryPosition(&obj_table, &Numerator, &Denominator);
	if (retval != MAPI_E_SUCCESS) return false;

	DEBUG(0, ("\no Restriction: RES_SIZE\n"));
	DEBUG(0, ("  --------------------------\n"));
	DEBUG(0, ("\tFilter on property size\n"));
	DEBUG(0, ("\tCheck for all emails with PR_BODY size > 30 chars\n"));
	DEBUG(0, ("\tCursor is at %d / %d\n", Numerator, Denominator));

	/* RES_EXIST test */
	res.rt = RES_EXIST;
	res.res.resExist.ulPropTag = PR_HTML;

	retval = Restrict(&obj_table, &res, NULL);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = QueryPosition(&obj_table, &Numerator, &Denominator);
	if (retval != MAPI_E_SUCCESS) return false;

	DEBUG(0, ("\no Restriction: RES_EXIST\n"));
	DEBUG(0, ("  --------------------------\n"));
	DEBUG(0, ("\tFilter on an existing property\n"));
	DEBUG(0, ("\tCheck for all emails with PR_HTML\n"));
	DEBUG(0, ("\tCursor is at %d / %d\n", Numerator, Denominator));


	/* RES_COMPAREPROPS */
	res.rt = RES_COMPAREPROPS;
	res.res.resCompareProps.relop = RELOP_EQ;
	res.res.resCompareProps.ulPropTag1 = PR_BODY;
	res.res.resCompareProps.ulPropTag2 = PR_SUBJECT;
	retval = Restrict(&obj_table, &res, NULL);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = QueryPosition(&obj_table, &Numerator, &Denominator);
	if (retval != MAPI_E_SUCCESS) return false;

	DEBUG(0, ("\no Restriction: RES_COMPAREPROPS\n"));
	DEBUG(0, ("  --------------------------\n"));
	DEBUG(0, ("\tFilter on properties comparison\n"));
	DEBUG(0, ("\tCheck for all emails with PR_SUBJECT == PR_BODY\n"));
	DEBUG(0, ("\tCursor is at %d / %d\n", Numerator, Denominator));

	/* RES_CONTENT */
	res.rt = RES_CONTENT;
	res.res.resContent.fuzzy = FL_SUBSTRING|FL_LOOSE;
	res.res.resContent.ulPropTag = PR_BODY;
	res.res.resContent.lpProp.ulPropTag = PR_BODY;
	res.res.resContent.lpProp.value.lpszA = "openchange";
	retval = Restrict(&obj_table, &res, NULL);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = QueryPosition(&obj_table, &Numerator, &Denominator);
	if (retval != MAPI_E_SUCCESS) return false;

	DEBUG(0, ("\no Restriction: RES_CONTENT\n"));
	DEBUG(0, ("  --------------------------\n"));
	DEBUG(0, ("\tFilter on insensitive substring within content\n"));
	DEBUG(0, ("\tCheck for all emails with PR_SUBJECT contained \"openchange\"\n"));
	DEBUG(0, ("\tCursor is at %d / %d\n", Numerator, Denominator));

	/* We now test the FindRow MAPI call */

	DEBUG(0, ("\nStep 2. Test FindRow MAPI call\n"));
	DEBUG(0, ("==============================\n"));

	/* Approximatively position at half the contents table */
	retval = SeekRowApprox(&obj_table, 1, 2);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Create a bookmark */
	retval = CreateBookmark(&obj_table, &bookmark);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Fetch row data */
	retval = QueryRows(&obj_table, 1, TBL_NOADVANCE, &SRowSet_row);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Position table cursor to the beginning of the table */
	retval = SeekRow(&obj_table, BOOKMARK_BEGINNING, 0, &row_idx);
	if (retval != MAPI_E_SUCCESS) return false;

	res.rt = RES_CONTENT;
	res.res.resContent.fuzzy = FL_SUBSTRING|FL_LOOSE;
	res.res.resContent.ulPropTag = PR_BODY;
	res.res.resContent.lpProp.ulPropTag = PR_BODY;
	res.res.resContent.lpProp.value.lpszA = "openchange";
	retval = FindRow(&obj_table, &res, BOOKMARK_BEGINNING, DIR_FORWARD, &SRowSet);
	if (retval != MAPI_E_SUCCESS) return false;

	DEBUG(0, ("\no FindRow: RES_CONTENT\n"));
	DEBUG(0, ("----------------------\n"));
	mapidump_SRowSet(&SRowSet, "\t[+] ");
	MAPIFreeBuffer(SRowSet.aRow);

	DEBUG(0, ("\n"));
	mapi_object_bookmark_debug(&obj_table);

	retval = FindRow(&obj_table, &res, bookmark, DIR_FORWARD, &SRowSet);
	if (retval != MAPI_E_SUCCESS) return false;
	DEBUG(0, ("\no FindRow: RES_CONTENT BOOKMARK_USER (%.2d)\n", bookmark));
	DEBUG(0, ("--------------------------------------------\n"));
	mapidump_SRowSet(&SRowSet, "\t[+] ");
	DEBUG(0, ("=============\n"));
	mapidump_SRowSet(&SRowSet_row, "\t[+] ");
	
	if (SRowSet.aRow[0].lpProps[1].value.d == SRowSet_row.aRow[0].lpProps[1].value.d) {
		DEBUG(0, ("PR_MID matches\n"));
	}
	MAPIFreeBuffer(SRowSet.aRow);

	/* Clean up test environment */
	retval = EmptyFolder(&obj_testdir);
	if (retval != MAPI_E_SUCCESS) return false;
	DEBUG(0, ("\n[+] Removing messages from testdir\n"));

	retval = DeleteFolder(&obj_inbox, mapi_object_get_id(&obj_testdir), 
			      DEL_MESSAGES|DEL_FOLDERS|DELETE_HARD_DELETE, NULL);
	if (retval != MAPI_E_SUCCESS) return false;
	DEBUG(0, ("[+] Deleting testdir folder\n"));

	/* release mapi objects */

	mapi_object_release(&obj_table);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);

	/* uninitialize mapi
	 */
	MAPIUninitialize();
	talloc_free(mem_ctx);
	
	return (ret);
}
