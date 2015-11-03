/*
   Stand-alone MAPI testsuite

   OpenChange Project - mapidump function tests

   Copyright (C) Brad Hards 2009

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

#include "utils/mapitest/mapitest.h"
#include "utils/mapitest/proto.h"

/**
   \file module_mapidump.c

   \brief mapidump function tests

   \note These tests do not show how to use libmapi properly, and should
  not be used as a programming reference.
*/

/**
   \details Test dump using mapidump_SPropValue

   This function:
   -# Tests the mapidump_SPropValue() function 

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_spropvalue(struct mapitest *mt)
{
	struct SPropValue propvalue;
	struct FILETIME ft;
	struct Binary_r bin;
	struct StringArray_r mvstr;
	uint32_t i;

	propvalue.ulPropTag = PR_GENDER; /* enum MAPITAGS */
	propvalue.dwAlignPad = 0;
	propvalue.value.i = 3; /* union SPropValue_CTR */
	mapidump_SPropValue(propvalue, "[sep]");

	propvalue.ulPropTag = PR_SENSITIVITY; /* enum MAPITAGS */
	propvalue.dwAlignPad = 0;
	propvalue.value.l = 68000; /* union SPropValue_CTR */
	mapidump_SPropValue(propvalue, "[sep]");

	propvalue.ulPropTag = PR_REPLY_REQUESTED; /* enum MAPITAGS */
	propvalue.dwAlignPad = 0;
	propvalue.value.b = 1; /* union SPropValue_CTR */
	mapidump_SPropValue(propvalue, "[sep]");

	propvalue.ulPropTag = PidTagMemberId; /* enum MAPITAGS */
	propvalue.dwAlignPad = 0;
	propvalue.value.d = 0x3DEADBEEFCAFE124LL; /* union SPropValue_CTR */
	mapidump_SPropValue(propvalue, "[sep]");

	propvalue.ulPropTag = PR_SENDER_NAME; /* enum MAPITAGS */
	propvalue.dwAlignPad = 0;
	propvalue.value.lpszA = (uint8_t *) "Mr. The Sender"; /* union SPropValue_CTR */
	mapidump_SPropValue(propvalue, "[sep]");

	propvalue.ulPropTag = PR_REPORT_TAG; /* enum MAPITAGS */
	propvalue.dwAlignPad = 0;
	bin.cb = 12;
	bin.lpb = talloc_array(mt->mem_ctx, uint8_t, bin.cb);
	for (i = 0; i < bin.cb; ++i) {
		bin.lpb[i] = 0xF0 + i;
	}
	propvalue.value.bin = bin; /* union SPropValue_CTR */
	mapidump_SPropValue(propvalue, "[sep]");

	propvalue.ulPropTag = PidTagOriginalDeliveryTime ; /* enum MAPITAGS */
	propvalue.dwAlignPad = 0;
	ft.dwLowDateTime = 0x12345678;
	ft.dwHighDateTime = 0x01CA6AE4;
	propvalue.value.ft = ft; /* union SPropValue_CTR */
	mapidump_SPropValue(propvalue, "[sep]");

	propvalue.ulPropTag = PR_REPLY_TIME_ERROR; /* enum MAPITAGS */
	propvalue.dwAlignPad = 0;
	propvalue.value.err = MAPI_E_UNKNOWN_CPID; /* union SPropValue_CTR */
	mapidump_SPropValue(propvalue, "[sep]");

	propvalue.ulPropTag = PidTagScheduleInfoDelegateNames;
	propvalue.dwAlignPad = 0;
	mvstr.cValues = 3;
	mvstr.lppszA = talloc_array(mt->mem_ctx, uint8_t *, mvstr.cValues);
	mvstr.lppszA[0] = (uint8_t *) talloc_strdup(mt->mem_ctx, "Foo");
	mvstr.lppszA[1] = (uint8_t *) talloc_strdup(mt->mem_ctx, "A longer string");
	mvstr.lppszA[2] = (uint8_t *) talloc_strdup(mt->mem_ctx, "All strung out on bugs");

	propvalue.value.MVszA = mvstr;
	mapidump_SPropValue(propvalue, "[sep]");

#if 0
	/* Types TODO */
	// int64_t dbl;/* [case(0x0005)] */
	// const char *lpszW;/* [unique,charset(UTF16),case(0x001f)] */
	// struct FlatUID_r *lpguid;/* [unique,case(0x0048)] */
	// struct ShortArray_r MVi;/* [case(0x1002)] */
	// struct LongArray_r MVl;/* [case(0x1003)] */
	// struct BinaryArray_r MVbin;/* [case(0x1102)] */
	// struct FlatUIDArray_r MVguid;/* [case(0x1048)] */
	// struct StringArrayW_r MVszW;/* [case(0x101f)] */
	// struct DateTimeArray_r MVft;/* [case(0x1040)] */
	// uint32_t object;/* [case(0x000d)] */
#endif

	propvalue.ulPropTag = 0xDDDD << 16 | PT_LONG; /* This isn't a real tag, just a test case */
	propvalue.dwAlignPad = 0;
	propvalue.value.l = 68020;
	mapidump_SPropValue(propvalue, "[sep]");

	return true;
}

/**
   \details Test dump using mapidump_SPropTagArray

   This function:
   -# Tests the mapidump_SPropTagArray() function 

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_sproptagarray(struct mapitest *mt)
{
	struct SPropTagArray *tagarray;

	tagarray = set_SPropTagArray(mt->mem_ctx, 5, PR_SENDER_NAME,
						     PR_BODY,
						     PR_PERSONAL_HOME_PAGE,
						     PR_OTHER_ADDRESS_CITY,
						     (0xDDDD << 16 | PT_LONG));

	mapidump_SPropTagArray(tagarray);

	return true;
}

/**
   \details Test dump using mapidump_SRowSet

   This function:
   -# Tests the mapidump_SRowSet() function 
   -# Indirectly tests the mapidump_SRow() function

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_srowset(struct mapitest *mt)
{
	struct SRowSet 		srowset;

	struct SPropValue       SPropValue;

	srowset.cRows = 2;
	srowset.aRow = talloc_array(mt->mem_ctx, struct SRow, srowset.cRows);

	srowset.aRow[0].cValues = 0;
	srowset.aRow[0].lpProps = talloc_zero(mt->mem_ctx, struct SPropValue);

	SPropValue.ulPropTag = PR_OBJECT_TYPE;
	SPropValue.value.l = MAPI_MAILUSER;
	SRow_addprop(&(srowset.aRow[0]), SPropValue);

	SPropValue.ulPropTag = PR_DISPLAY_TYPE;
	SPropValue.value.l = 0;
	SRow_addprop(&(srowset.aRow[0]), SPropValue);

	SPropValue.ulPropTag = PR_GIVEN_NAME;
	SPropValue.value.lpszA = (uint8_t *) "gname";
	SRow_addprop(&(srowset.aRow[0]), SPropValue);

	srowset.aRow[1].cValues = 0;
	srowset.aRow[1].lpProps = talloc_zero(mt->mem_ctx, struct SPropValue);
	SPropValue.ulPropTag = PR_GIVEN_NAME;
	SPropValue.value.lpszA = (uint8_t *) "kname";
	SRow_addprop(&(srowset.aRow[1]), SPropValue);

	SPropValue.ulPropTag = PR_7BIT_DISPLAY_NAME;
	SPropValue.value.lpszA = (uint8_t *) "lname";
	SRow_addprop(&(srowset.aRow[1]), SPropValue);

	mapidump_SRowSet(&srowset, "[sep]");

	return true;
}

/**
   \details Test dump using mapidump_pabentry

   This function:
   -# Tests the mapidump_PAB_entry() function 

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_pabentry(struct mapitest *mt)
{
	struct PropertyRow_r	pabentry;
	struct PropertyValue_r	value;

	pabentry.cValues = 0;
	pabentry.lpProps = talloc_zero(mt->mem_ctx, struct PropertyValue_r);

	value.ulPropTag = PR_ADDRTYPE_UNICODE;
	value.value.lpszA = (uint8_t *) "dummy addrtype";
	PropertyRow_addprop(&(pabentry), value);

	value.ulPropTag = PR_DISPLAY_NAME_UNICODE;
	value.value.lpszA = (uint8_t *) "dummy display name";
	PropertyRow_addprop(&(pabentry), value);

	value.ulPropTag = PR_EMAIL_ADDRESS_UNICODE;
	value.value.lpszA = (uint8_t *) "dummy@example.com";
	PropertyRow_addprop(&(pabentry), value);

	value.ulPropTag = PR_ACCOUNT_UNICODE;
	value.value.lpszA = (uint8_t *) "dummy account";
	PropertyRow_addprop(&(pabentry), value);

	mapidump_PAB_entry(&pabentry);

	return true;
}

/**
   \details Test dump using mapidump_note

   This function:
   -# Tests the mapidump_note() function on a plain text message
   -# Tests the mapidump_note() function on a HTML message

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_note(struct mapitest *mt)
{
	struct mapi_SPropValue_array	props;

	props.cValues = 3;
	props.lpProps = talloc_array(mt->mem_ctx, struct mapi_SPropValue, props.cValues);

	props.lpProps[0].ulPropTag = PR_CONVERSATION_TOPIC;
	props.lpProps[0].value.lpszA = (char *) "Topic of the Note";

	props.lpProps[1].ulPropTag = PR_BODY;
	props.lpProps[1].value.lpszA = (char *) "This is the body of the note. It has two sentences.";

	props.lpProps[2].ulPropTag = PR_CLIENT_SUBMIT_TIME;
	props.lpProps[2].value.ft.dwLowDateTime = 0x12345678;
	props.lpProps[2].value.ft.dwHighDateTime = 0x01CA6BE4;

	mapidump_note(&props, "[dummy ID]");

	props.lpProps[1].ulPropTag = PR_BODY_HTML;
	props.lpProps[1].value.lpszA = (char *) "<h1>Heading for the Note</h1>\n<p>This is the body of the note. It has two sentences.</p>";

	mapidump_note(&props, "[dummy ID]");

	
	return true;
}

/**
   \details Test dump using mapidump_task

   This function:
   -# Tests the mapidump_task() function
   -# modifies the task to be completed
   -# Tests the associated get_importance() function
   -# Tests the associated get_task_status() function

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_task(struct mapitest *mt)
{
	struct mapi_SPropValue_array	props;

	props.cValues = 9;
	props.lpProps = talloc_array(mt->mem_ctx, struct mapi_SPropValue, props.cValues);

	props.lpProps[0].ulPropTag = PR_CONVERSATION_TOPIC;
	props.lpProps[0].value.lpszA = (char *) "Topic of the Task";

	props.lpProps[1].ulPropTag = PR_BODY;
	props.lpProps[1].value.lpszA = (char *) "This is the body of the task. It has two sentences.";

	props.lpProps[2].ulPropTag = PidLidTaskDueDate;
	props.lpProps[2].value.ft.dwLowDateTime = 0x12345678;
	props.lpProps[2].value.ft.dwHighDateTime = 0x01CA6CE4;

	props.lpProps[3].ulPropTag = PidLidPrivate;
	props.lpProps[3].value.b = 0;

	props.lpProps[4].ulPropTag = PR_IMPORTANCE;
	props.lpProps[4].value.l = IMPORTANCE_HIGH;

	props.lpProps[5].ulPropTag = PidLidPercentComplete;
	props.lpProps[5].value.dbl = 0.78;

	props.lpProps[6].ulPropTag = PidLidTaskStartDate;
	props.lpProps[6].value.ft.dwLowDateTime = 0x09876543;
	props.lpProps[6].value.ft.dwHighDateTime = 0x01CA6AE4;

	props.lpProps[7].ulPropTag = PidLidTaskStatus;
	props.lpProps[7].value.l = olTaskWaiting;

	props.lpProps[8].ulPropTag = PidLidContacts;
	props.lpProps[8].value.MVszA.cValues = 2;
	props.lpProps[8].value.MVszA.strings = talloc_array(mt->mem_ctx, struct mapi_LPSTR, 2);
	props.lpProps[8].value.MVszA.strings[0].lppszA = "Contact One";
	props.lpProps[8].value.MVszA.strings[1].lppszA = "Contact Two Jr.";

	mapidump_task(&props, "[dummy ID]");

	props.lpProps[7].ulPropTag = PidLidTaskStatus;
	props.lpProps[7].value.l = olTaskComplete;

	props.lpProps[3].ulPropTag = PidLidTaskDateCompleted;
	props.lpProps[3].value.ft.dwLowDateTime = 0x22345678;
	props.lpProps[3].value.ft.dwHighDateTime = 0x01CA6CB4;

	mapidump_task(&props, "[dummy ID]");

	if (strcmp("Low", get_importance(IMPORTANCE_LOW)) != 0) {
		mapitest_print(mt, "* %-40s: bad result IMPORTANCE_LOW\n", "mapidump_task");
		return false;
	}
	if (strcmp("Normal", get_importance(IMPORTANCE_NORMAL)) != 0) {
		mapitest_print(mt, "* %-40s: bad result IMPORTANCE_NORMAL\n", "mapidump_task");
		return false;
	}
	if (strcmp("High", get_importance(IMPORTANCE_HIGH)) != 0) {
		mapitest_print(mt, "* %-40s: bad result IMPORTANCE_HIGH\n", "mapidump_task");
		return false;
	}
	if (get_importance(IMPORTANCE_HIGH+1) != 0) {
		mapitest_print(mt, "* %-40s: bad result OUT_OF_RANGE\n", "mapidump_task");
		return false;
	}

	if (strcmp("Not Started", get_task_status(olTaskNotStarted)) != 0) {
		mapitest_print(mt, "* %-40s: bad result olTaskNotStarted\n", "mapidump_task");
		return false;
	}
	if (strcmp("In Progress", get_task_status(olTaskInProgress)) != 0) {
		mapitest_print(mt, "* %-40s: bad result olTaskInProgress\n", "mapidump_task");
		return false;
	}
	if (strcmp("Completed", get_task_status(olTaskComplete)) != 0) {
		mapitest_print(mt, "* %-40s: bad result olTaskCompleted\n", "mapidump_task");
		return false;
	}
	if (strcmp("Waiting on someone else", get_task_status(olTaskWaiting)) != 0) {
		mapitest_print(mt, "* %-40s: bad result olTaskWaiting\n", "mapidump_task");
		return false;
	}
	if (strcmp("Deferred", get_task_status(olTaskDeferred)) != 0) {
		mapitest_print(mt, "* %-40s: bad result olTaskDeferred\n", "mapidump_task");
		return false;
	}
	if (get_task_status(olTaskDeferred+1) != 0) {
		mapitest_print(mt, "* %-40s: bad result OUT_OF_RANGE\n", "mapidump_task");
		return false;
	}
	return true;
}

/**
   \details Test dump using mapidump_contact

   This function:
   -# Tests the mapidump_contact() function

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_contact(struct mapitest *mt)
{
	struct mapi_SPropValue_array	props;

	props.cValues = 8;
	props.lpProps = talloc_array(mt->mem_ctx, struct mapi_SPropValue, props.cValues);

	props.lpProps[0].ulPropTag = PR_POSTAL_ADDRESS;
	props.lpProps[0].value.lpszA = (char *) "P.O. Box 543, KTown";

	props.lpProps[1].ulPropTag = PR_STREET_ADDRESS;
	props.lpProps[1].value.lpszA = (char *) "1 My Street, KTown";

	props.lpProps[2].ulPropTag = PR_COMPANY_NAME;
	props.lpProps[2].value.lpszA = (char *) "Dummy Company Pty Ltd";

	props.lpProps[3].ulPropTag = PR_TITLE;
	props.lpProps[3].value.lpszA = (char *) "Ms.";

	props.lpProps[4].ulPropTag = PR_COUNTRY;
	props.lpProps[4].value.lpszA = (char *) "Australia";

	props.lpProps[5].ulPropTag = PR_GIVEN_NAME;
	props.lpProps[5].value.lpszA = (char *) "Konq.";

	props.lpProps[6].ulPropTag = PR_SURNAME;
	props.lpProps[6].value.lpszA = (char *) "Dragoon";

	props.lpProps[7].ulPropTag = PR_DEPARTMENT_NAME;
	props.lpProps[7].value.lpszA = (char *) "Research and Marketing";

	mapidump_contact(&props, "[dummy ID]");

	props.lpProps[1].ulPropTag = PR_CONVERSATION_TOPIC;
	props.lpProps[1].value.lpszA = (char *) "Contact Topic";

	mapidump_contact(&props, "[dummy ID]");

	props.lpProps[0].ulPropTag = PidLidFileUnder;
	props.lpProps[0].value.lpszA = (char *) "Card Label";

	props.lpProps[4].ulPropTag = PR_DISPLAY_NAME;
	props.lpProps[4].value.lpszA = (char *) "Konqi Dragon";

	mapidump_contact(&props, "[dummy ID]");

	return true;
}

/**
   \details Test message dump using mapidump_appointment

   This function:
   -# Tests the mapidump_appointment() function

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_appointment(struct mapitest *mt)
{
	struct mapi_SPropValue_array	props;

	props.cValues = 8;
	props.lpProps = talloc_array(mt->mem_ctx, struct mapi_SPropValue, props.cValues);

	props.lpProps[0].ulPropTag = PidLidContacts;
	props.lpProps[0].value.MVszA.cValues = 2;
	props.lpProps[0].value.MVszA.strings = talloc_array(mt->mem_ctx, struct mapi_LPSTR, 2);
	props.lpProps[0].value.MVszA.strings[0].lppszA = "Contact One";
	props.lpProps[0].value.MVszA.strings[1].lppszA = "Contact Two Jr.";

	props.lpProps[1].ulPropTag = PR_CONVERSATION_TOPIC;
	props.lpProps[1].value.lpszA = (char *) "Dummy Appointment topic";

	props.lpProps[2].ulPropTag = PidLidTimeZoneDescription;
	props.lpProps[2].value.lpszA = (char *) "AEDT";

	props.lpProps[3].ulPropTag = PidLidLocation;
	props.lpProps[3].value.lpszA = (char *) "OpenChange Conference Room #3";

	props.lpProps[4].ulPropTag = PidLidBusyStatus;
	props.lpProps[4].value.l = olTaskNotStarted;

	props.lpProps[5].ulPropTag = PidLidPrivate;
	props.lpProps[5].ulPropTag = true;

	props.lpProps[6].ulPropTag = PR_END_DATE;
	props.lpProps[6].value.ft.dwLowDateTime = 0x12345678;
	props.lpProps[6].value.ft.dwHighDateTime = 0x01CA6CE4;

	props.lpProps[7].ulPropTag = PR_START_DATE;
	props.lpProps[7].value.ft.dwLowDateTime = 0x09876543;
	props.lpProps[7].value.ft.dwHighDateTime = 0x01CA6AE4;

	mapidump_appointment(&props, "[dummy ID]");

	return true;
}

/**
   \details Test dump of an email message using mapidump_message

   This function:
   -# Builds an indicative email using mapi_SPropValues (sets no codepage)
   -# Calls the mapidump_appointment() function

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_message(struct mapitest *mt)
{
	struct mapi_SPropValue_array	props;

	props.cValues = 9;
	props.lpProps = talloc_array(mt->mem_ctx, struct mapi_SPropValue, props.cValues);

	props.lpProps[0].ulPropTag = PR_INTERNET_MESSAGE_ID;
	props.lpProps[0].value.lpszA = (char *) "dummy-3535395fds@example.com";
	
	props.lpProps[1].ulPropTag = PR_CONVERSATION_TOPIC;
	props.lpProps[1].value.lpszA = (char *) "Dummy Email Subject";

	props.lpProps[2].ulPropTag = PR_BODY;
	props.lpProps[2].value.lpszA = (char *) "This is the body of the email. It has two sentences.\n";

	props.lpProps[3].ulPropTag = PR_SENT_REPRESENTING_NAME;
	props.lpProps[3].value.lpszA = (char *) "The Sender <sender@example.com>";

	props.lpProps[4].ulPropTag = PR_DISPLAY_TO;
	props.lpProps[4].value.lpszA = (char *) "The Recipient <to@example.com>";

	props.lpProps[5].ulPropTag = PR_DISPLAY_CC;
	props.lpProps[5].value.lpszA = (char *) "Other Recipient <cc@example.com>";

	props.lpProps[6].ulPropTag = PR_DISPLAY_BCC;
	props.lpProps[6].value.lpszA = (char *) "Ms. Anonymous <bcc@example.com>";

	props.lpProps[7].ulPropTag = PidTagPriority;
	props.lpProps[7].value.l = 0;

	props.lpProps[8].ulPropTag = PR_HASATTACH;
	props.lpProps[8].value.b = false;

	mapidump_message(&props, "[dummy ID]", NULL);

	props.lpProps[7].ulPropTag = PR_MESSAGE_CODEPAGE;
	props.lpProps[7].value.l = CP_USASCII;

	mapidump_message(&props, "[dummy ID]", NULL);

	props.lpProps[7].ulPropTag = PR_MESSAGE_CODEPAGE;
	props.lpProps[7].value.l = CP_UNICODE;

	mapidump_message(&props, "[dummy ID]", NULL);

	props.lpProps[7].ulPropTag = PR_MESSAGE_CODEPAGE;
	props.lpProps[7].value.l = CP_JAUTODETECT;

	mapidump_message(&props, "[dummy ID]", NULL);

	props.lpProps[7].ulPropTag = PR_MESSAGE_CODEPAGE;
	props.lpProps[7].value.l = CP_KAUTODETECT;

	mapidump_message(&props, "[dummy ID]", NULL);

	props.lpProps[7].ulPropTag = PR_MESSAGE_CODEPAGE;
	props.lpProps[7].value.l = CP_ISO2022JPESC;

	mapidump_message(&props, "[dummy ID]", NULL);

	props.lpProps[7].ulPropTag = PR_MESSAGE_CODEPAGE;
	props.lpProps[7].value.l = CP_ISO2022JPSIO;

	mapidump_message(&props, "[dummy ID]", NULL);

	return true;
}

/**
   \details Test dump of an new mail notification

   This function:
   -# Tests the mapidump_msgflags function with no bits set
   -# Tests the mapidump_msgflags function with one bit set
   -# Tests the mapidump_msgflags function with all bits set
   -# Builds an indicative new mail notification
   -# Calls the mapidump_newmail function to test dumping of that new mail notification
  
   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_newmail(struct mapitest *mt)
{
	struct NewMailNotification notif;

	mapidump_msgflags(0x0, "[sep]");

	mapidump_msgflags(0x100, "[sep]");

	mapidump_msgflags(0x1|0x2|0x4|0x8|0x10|0x20|0x40|0x80|0x100|0x200, "[sep]");

	notif.FID = 0xFEDCBA9876543210LL;
	notif.MID = 0xBADCAFEBEEF87625LL;
	notif.MessageFlags = 0x20|0x2;
	notif.UnicodeFlag = false;
	notif.MessageClass.lpszA = (char *) "Dummy class";

	mapidump_newmail(&notif, "[sep]");

	return true;
}

/**
   \details Test dump of a free/busy event

   This function:
   -# builds a freebusy binary event
   -# tests dumping it using mapidump_freebusy_event()
   -# modifies the event, and dumps it again
   -# modifies the event, and dumps it again
   -# tests dumping a date using mapidump_freebusy_date()
   -# tests each months for mapidump_freebusy_month()
   
   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_freebusy(struct mapitest *mt)
{
	struct Binary_r bin;
	
	bin.cb = 4;
	bin.lpb = talloc_array(mt->mem_ctx, uint8_t, bin.cb);
	/* this example from MS-OXOPFFB Section 4.4.3 */
	bin.lpb[0] = 0x50;
	bin.lpb[1] = 0x0A;
	bin.lpb[2] = 0xC8;
	bin.lpb[3] = 0x0A;
	
	mapidump_freebusy_event(&bin, 2009*16+11, 2008, "[sep]");

	/* this example adapted from MS-OXOPFFB Section 4.4.3 */
	bin.lpb[0] = 0x50;
	bin.lpb[1] = 0x0A;
	bin.lpb[2] = 0xCA;
	bin.lpb[3] = 0x0A;
	
	mapidump_freebusy_event(&bin, 2009*16+11, 2009, "[sep]");

	/* this example adapted from MS-OXOPFFB Section 4.4.3 */
	bin.lpb[0] = 0x50;
	bin.lpb[1] = 0x0A;
	bin.lpb[2] = 0x80;
	bin.lpb[3] = 0x0A;

	mapidump_freebusy_event(&bin, 2009*16+11, 2009, "[sep]");

	mapidump_freebusy_date(0x0CD18345, "[sep]");

	if (strcmp("January", mapidump_freebusy_month(2009*16+1, 2009)) != 0) {
		mapitest_print(mt, "* %-40s: bad result January\n", "mapidump_freebusy");
		return false;
	}
	if (strcmp("February", mapidump_freebusy_month(2009*16+2, 2009)) != 0) {
		mapitest_print(mt, "* %-40s: bad result February\n", "mapidump_freebusy");
		return false;
	}
	if (strcmp("March", mapidump_freebusy_month(2009*16+3, 2009)) != 0) {
		mapitest_print(mt, "* %-40s: bad result March\n", "mapidump_freebusy");
		return false;
	}
	if (strcmp("April", mapidump_freebusy_month(2009*16+4, 2009)) != 0) {
		mapitest_print(mt, "* %-40s: bad result April\n", "mapidump_freebusy");
		return false;
	}
	if (strcmp("May", mapidump_freebusy_month(2009*16+5, 2009)) != 0) {
		mapitest_print(mt, "* %-40s: bad result May\n", "mapidump_freebusy");
		return false;
	}
	if (strcmp("June", mapidump_freebusy_month(2009*16+6, 2009)) != 0) {
		mapitest_print(mt, "* %-40s: bad result June\n", "mapidump_freebusy");
		return false;
	}
	if (strcmp("July", mapidump_freebusy_month(2009*16+7, 2009)) != 0) {
		mapitest_print(mt, "* %-40s: bad result July\n", "mapidump_freebusy");
		return false;
	}
	if (strcmp("August", mapidump_freebusy_month(2009*16+8, 2009)) != 0) {
		mapitest_print(mt, "* %-40s: bad result August\n", "mapidump_freebusy");
		return false;
	}
	if (strcmp("September", mapidump_freebusy_month(2009*16+9, 2009)) != 0) {
		mapitest_print(mt, "* %-40s: bad result September\n", "mapidump_freebusy");
		return false;
	}
	if (strcmp("October", mapidump_freebusy_month(2009*16+10, 2009)) != 0) {
		mapitest_print(mt, "* %-40s: bad result October\n", "mapidump_freebusy");
		return false;
	}
	if (strcmp("November", mapidump_freebusy_month(2009*16+11, 2009)) != 0) {
		mapitest_print(mt, "* %-40s: bad result November\n", "mapidump_freebusy");
		return false;
	}
	if (strcmp("December", mapidump_freebusy_month(2009*16+12, 2009)) != 0) {
		mapitest_print(mt, "* %-40s: bad result December\n", "mapidump_freebusy");
		return false;
	}
	if (mapidump_freebusy_month(2009*16+0, 2009) != 0) {
		mapitest_print(mt, "* %-40s: bad result underrange\n", "mapidump_freebusy");
		return false;
	}
	if (mapidump_freebusy_month(2009*16+13, 2009) != 0) {
		mapitest_print(mt, "* %-40s: bad result overrange\n", "mapidump_freebusy");
		return false;
	}
	return true;
}

/**
   \details Test dump of a set of recipients

   This function:
   -# builds a recipient list
   -# dumps out the recipient list using mapidump_Recipients()

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_recipients(struct mapitest *mt)
{
	const char 			**userlist;
	struct SRowSet 			resolved;
	struct PropertyTagArray_r	flaglist;
	struct SPropValue		SPropValue;

	userlist = talloc_array(mt->mem_ctx, const char*, 3);
	userlist[0] = "Mr. Unresolved";
	userlist[1] = "Mr/Ms. Ambiguous";
	userlist[2] = "Mrs. Resolved";

	resolved.cRows = 1;
	resolved.aRow = talloc_array(mt->mem_ctx, struct SRow, resolved.cRows);
	resolved.aRow[0].cValues = 0;
	resolved.aRow[0].lpProps = talloc_zero(mt->mem_ctx, struct SPropValue);
	SPropValue.ulPropTag = PR_OBJECT_TYPE;
	SPropValue.value.l = MAPI_MAILUSER;
	SRow_addprop(&(resolved.aRow[0]), SPropValue);

	SPropValue.ulPropTag = PR_GIVEN_NAME;
	SPropValue.value.lpszA = (uint8_t *) "gname";
	SRow_addprop(&(resolved.aRow[0]), SPropValue);

	flaglist.cValues = 3;
	flaglist.aulPropTag = talloc_zero_array(mt->mem_ctx, uint32_t, flaglist.cValues);
	flaglist.aulPropTag[0] = MAPI_UNRESOLVED;
	flaglist.aulPropTag[1] = MAPI_AMBIGUOUS;
	flaglist.aulPropTag[2] = MAPI_RESOLVED;
	
	mapidump_Recipients(userlist, &resolved, &flaglist);

	talloc_free(flaglist.aulPropTag);

	return true;
}

/**
   \details Test dump of a Folder deletion notification

   This function:
   -# Creates a FolderDeletedNotification structure
   -# Dumps that structure out using mapidump_folderdeleted()
   -# Tests mapidump_folderdeleted() with a null argument

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_folderdeleted(struct mapitest *mt)
{
	struct FolderDeletedNotification folderdeletednotification;

	folderdeletednotification.ParentFID = 0x9876CAFE432LL;
	folderdeletednotification.FID = 0x1234ABCDLL;
	mapidump_folderdeleted(&folderdeletednotification, "[sep]");
	
	mapidump_folderdeleted(0, "[sep]");

	return true;
}

/**
   \details Test dump of a folder move notification

   This function:
   -# Creates a FolderMoveCopyNotification structure
   -# Dumps that structure out using mapidump_foldermoved()
   -# Tests mapidump_foldermoved() with a null argument

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_foldermoved(struct mapitest *mt)
{
	struct FolderMoveCopyNotification foldermovecopynotification;

	foldermovecopynotification.ParentFID = 0x9876CAFE432LL;
	foldermovecopynotification.FID = 0x1234ABCDLL;
	foldermovecopynotification.OldParentFID = 0x9876CAFE43201DLL;
	foldermovecopynotification.OldFID = 0x1234ABCD01DLL;
	mapidump_foldermoved(&foldermovecopynotification, "[sep]");
	
	mapidump_foldermoved(0, "[sep]");

	return true;
}

/**
   \details Test dump of a folder copy notification

   This function:
   -# Creates a FolderMoveCopyNotification structure
   -# Dumps that structure out using mapidump_foldercopy()
   -# Tests mapidump_foldercopy() with a null argument

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_foldercopied(struct mapitest *mt)
{
	struct FolderMoveCopyNotification foldermovecopynotification;

	foldermovecopynotification.ParentFID = 0x9876CAFE432LL;
	foldermovecopynotification.FID = 0x1234ABCDLL;
	foldermovecopynotification.OldParentFID = 0x9876CAFE43201DLL;
	foldermovecopynotification.OldFID = 0x1234ABCD01DLL;
	mapidump_foldercopied(&foldermovecopynotification, "[sep]");
	
	mapidump_foldercopied(0, "[sep]");

	return true;
}

/**
   \details Test dump of a Folder creation notification

   This function:
   -# Creates a FolderCreatedNotification structure with a null tag set
   -# Dumps that structure out using mapidump_foldercreated()
   -# Adds a set of tags to the FolderCreatedNotification
   -# Dumps the modified FolderCreatedNotification structure using mapidump_foldercreated()
   -# Tests mapidump_foldercreated() with a null argument

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_foldercreated(struct mapitest *mt)
{
	struct FolderCreatedNotification foldercreatednotification;

	foldercreatednotification.ParentFID = 0x9876CAFE432LL;
	foldercreatednotification.FID = 0x1234ABCDLL;
	foldercreatednotification.NotificationTags.Tags = 0;
	foldercreatednotification.TagCount = 0;
	mapidump_foldercreated(&foldercreatednotification, "[sep]");

	foldercreatednotification.TagCount = 3;
	foldercreatednotification.NotificationTags.Tags = talloc_array(mt->mem_ctx, enum MAPITAGS,
                                                           foldercreatednotification.TagCount);
	foldercreatednotification.NotificationTags.Tags[0] = PidTagTemplateData;
	foldercreatednotification.NotificationTags.Tags[1] = PidTagUrlCompName;
	foldercreatednotification.NotificationTags.Tags[2] = EndAttach;

	mapidump_foldercreated(&foldercreatednotification, "[sep]");

	mapidump_foldercreated(0, "[sep]");

	return true;
}

/**
   \details Test dump of a Message deletion notification

   This function:
   -# Creates a MessageDeletedNotification structure
   -# Dumps that structure out using mapidump_messagedeleted()
   -# Tests mapidump_messagedeleted() with a null argument

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_messagedeleted(struct mapitest *mt)
{
	struct MessageDeletedNotification messagedeletednotification;

	messagedeletednotification.FID = 0x1234ABCDLL;
	messagedeletednotification.MID = 0x9876FEALL;
	mapidump_messagedeleted(&messagedeletednotification, "[sep]");
	
	mapidump_messagedeleted(0, "[sep]");

	return true;
}

/**
   \details Test dump of a Message creation notification

   This function:
   -# Creates a MessageCreatedNotification structure
   -# Dumps that structure out using mapidump_messagecreated()
   -# Adds tags to the MessageCreatedNotification structure
   -# Dumps the structure again.
   -# Tests mapidump_messagecreated() with a null argument

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_messagecreated(struct mapitest *mt)
{
	struct MessageCreatedNotification messagecreatednotification;

	messagecreatednotification.FID = 0x1234ABCDLL;
	messagecreatednotification.MID = 0x9876FEALL;
	messagecreatednotification.NotificationTags.Tags = 0;
	messagecreatednotification.TagCount = 0;
	mapidump_messagecreated(&messagecreatednotification, "[sep]");

	messagecreatednotification.TagCount = 3;
	messagecreatednotification.NotificationTags.Tags = talloc_array(mt->mem_ctx, enum MAPITAGS,
						       messagecreatednotification.TagCount);
	messagecreatednotification.NotificationTags.Tags[0] = PR_DISPLAY_NAME;
	messagecreatednotification.NotificationTags.Tags[1] = PR_DISPLAY_NAME_UNICODE;
	messagecreatednotification.NotificationTags.Tags[2] = PR_COMPANY_NAME;

	mapidump_messagecreated(0, "[sep]");

	return true;
}

/**
   \details Test dump of a Message moved notification

   This function:
   -# Creates a MessageMovedNotification structure
   -# Dumps that structure out using mapidump_messagemoved()
   -# Tests mapidump_messagemoved() with a null argument

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_messagemoved(struct mapitest *mt)
{
	struct MessageMoveCopyNotification messagemovednotification;

	messagemovednotification.FID = 0x1234ABCDLL;
	messagemovednotification.MID = 0x9876FEALL;
	messagemovednotification.OldFID = 0x1234ABCD01dLL;
	messagemovednotification.OldMID = 0x9876FEA01dLL;
	mapidump_messagemoved(&messagemovednotification, "[sep]");
	
	mapidump_messagemoved(0, "[sep]");

	return true;
}


/**
   \details Test dump of a Message copied notification

   This function:
   -# Creates a MessageCopiedNotification structure
   -# Dumps that structure out using mapidump_messagecopied()
   -# Tests mapidump_messagecopied() with a null argument

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_messagecopied(struct mapitest *mt)
{
	struct MessageMoveCopyNotification messagecopiednotification;

	messagecopiednotification.FID = 0x1234ABCDLL;
	messagecopiednotification.MID = 0x9876FEALL;
	messagecopiednotification.OldFID = 0x1234ABCD01dLL;
	messagecopiednotification.OldMID = 0x9876FEA01dLL;
	mapidump_messagecopied(&messagecopiednotification, "[sep]");
	
	mapidump_messagecopied(0, "[sep]");

	return true;
}

/**
   \details Test dump of a Message modification notification

   This function:
   -# Creates a MessageModifiedNotification structure
   -# Dumps that structure out using mapidump_messagemodified()
   -# Adds tags to the MessageModifiedNotification structure
   -# Dumps the structure again.
   -# Tests mapidump_messagemodified() with a null argument

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
   
   \note This currently doesn't check the results are sane, so manual inspection is required
*/ 
_PUBLIC_ bool mapitest_mapidump_messagemodified(struct mapitest *mt)
{
	struct MessageModifiedNotification messagemodifiednotification;

	messagemodifiednotification.FID = 0x1234ABCDLL;
	messagemodifiednotification.MID = 0x9876FEALL;
	messagemodifiednotification.NotificationTags.Tags = 0;
	messagemodifiednotification.TagCount = 0;
	mapidump_messagemodified(&messagemodifiednotification, "[sep]");

	messagemodifiednotification.TagCount = 3;
	messagemodifiednotification.NotificationTags.Tags = talloc_array(mt->mem_ctx, enum MAPITAGS,
						        messagemodifiednotification.TagCount);
	messagemodifiednotification.NotificationTags.Tags[0] = PR_DISPLAY_NAME;
	messagemodifiednotification.NotificationTags.Tags[1] = PR_DISPLAY_NAME_UNICODE;
	messagemodifiednotification.NotificationTags.Tags[2] = PR_COMPANY_NAME;

	mapidump_messagemodified(0, "[sep]");

	return true;
}
