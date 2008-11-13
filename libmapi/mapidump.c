/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2008.

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
#include <libmapi/mapidump.h>
#include <libmapi/proto_private.h>
#include <libmapi/defs_private.h>
#include <time.h>

/**
   \file mapidump.c

   \brief Functions for displaying various data structures, mainly for debugging
 */

_PUBLIC_ void mapidump_SPropValue(struct SPropValue lpProp, const char *sep)
{
	const char			*proptag;
	const void			*data;
	TALLOC_CTX			*mem_ctx = NULL;
	const struct StringArray_r	*StringArray_r = NULL;
	uint32_t			i;

	proptag = get_proptag_name(lpProp.ulPropTag);
	if (!proptag) {
		mem_ctx = talloc_init("mapidump_SPropValue");
		proptag = talloc_asprintf(mem_ctx, "0x%.8x", lpProp.ulPropTag);
	}
	

	switch(lpProp.ulPropTag & 0xFFFF) {
	case PT_BOOLEAN:
		data = get_SPropValue_data(&lpProp);
		printf("%s%s: 0x%x\n", sep?sep:"", proptag, (*(const uint8_t *)data));
		break;
	case PT_I8:
		data = get_SPropValue_data(&lpProp);
		printf("%s%s: %"PRIx64"\n", sep?sep:"", proptag, (*(const uint64_t *)data));
		break;
	case PT_STRING8:
	case PT_UNICODE:
		data = get_SPropValue_data(&lpProp);
		printf("%s%s: %s\n", sep?sep:"", proptag, (const char *)data);
		break;
	case PT_SYSTIME:
		mapidump_date_SPropValue(lpProp, proptag);
		break;
	case PT_ERROR:
		data = get_SPropValue_data(&lpProp);
		printf("%s%s: 0x%.8x\n", sep?sep:"", proptag, (*(const uint32_t *)data));
		break;
	case PT_LONG:
		data = get_SPropValue_data(&lpProp);
		printf("%s%s: %d\n", sep?sep:"", proptag, (*(const uint32_t *)data));
		break;
	case PT_MV_STRING8:
		StringArray_r = (const struct StringArray_r *) get_SPropValue_data(&lpProp);
		printf("%s%s: ", sep?sep:"", proptag);
		for (i = 0; i < StringArray_r->cValues - 1; i++) {
			printf("%s, ", StringArray_r->lppszA[i]);
		}
		printf("%s\n", StringArray_r->lppszA[i]);
		break;
	default:
		break;
	}

	if (mem_ctx) {
		talloc_free(mem_ctx);
	}

}

_PUBLIC_ void mapidump_SPropTagArray(struct SPropTagArray *SPropTagArray)
{
	uint32_t	count;
	const char	*proptag;

	if (!SPropTagArray) return;
	if (!SPropTagArray->cValues) return;

	for (count = 0; count != SPropTagArray->cValues; count++) {
		proptag = get_proptag_name(SPropTagArray->aulPropTag[count]);
		if (proptag) {
			printf("%s\n", proptag);
		} else {
			printf("0x%.8x\n", SPropTagArray->aulPropTag[count]);
		}
	}
}

_PUBLIC_ void mapidump_SRowSet(struct SRowSet *SRowSet, const char *sep)
{
	uint32_t		i;

	/* Sanity checks */
	if (!SRowSet) return;
	if (!SRowSet->cRows) return;

	for (i = 0; i < SRowSet->cRows; i++) {
		mapidump_SRow(&(SRowSet->aRow[i]), sep);
	}
}

_PUBLIC_ void mapidump_SRow(struct SRow *aRow, const char *sep)
{
	uint32_t		i;

	for (i = 0; i < aRow->cValues; i++) {
		mapidump_SPropValue(aRow->lpProps[i], sep);
	}
}


_PUBLIC_ void mapidump_PAB_entry(struct SRow *aRow)
{
	const char	*addrtype;
	const char	*name;
	const char	*email;
	const char	*account;

	addrtype = (const char *)find_SPropValue_data(aRow, PR_ADDRTYPE_UNICODE);
	name = (const char *)find_SPropValue_data(aRow, PR_DISPLAY_NAME_UNICODE);
	email = (const char *)find_SPropValue_data(aRow, PR_EMAIL_ADDRESS_UNICODE);
	account = (const char *)find_SPropValue_data(aRow, PR_ACCOUNT_UNICODE);

	printf("[%s] %s:\n\tName: %-25s\n\tEmail: %-25s\n", 
	       addrtype, account, name, email);
	fflush(0);
}


_PUBLIC_ void mapidump_Recipients(const char **usernames, struct SRowSet *rowset, struct SPropTagArray *flaglist)
{
	uint32_t		i;
	uint32_t		j;

	for (i = 0, j= 0; i < flaglist->cValues; i++) {
		switch (flaglist->aulPropTag[i]) {
		case MAPI_UNRESOLVED:
			printf("\tUNRESOLVED (%s)\n", usernames[i]);
			break;
		case MAPI_AMBIGUOUS:
			printf("\tAMBIGUOUS (%s)\n", usernames[i]);
			break;
		case MAPI_RESOLVED:
			printf("\tRESOLVED (%s)\n", usernames[i]);
			mapidump_SRow(&rowset->aRow[j], "\t\t[+] ");
			j++;
			break;
		default:
			break;
		}
	}
}

_PUBLIC_ void mapidump_date(struct mapi_SPropValue_array *properties, uint32_t mapitag, const char *label)
{
	TALLOC_CTX		*mem_ctx;
	NTTIME			time;
	const struct FILETIME	*filetime;
	const char		*date;

	mem_ctx = talloc_init("mapidump_date");

	filetime = (const struct FILETIME *) find_mapi_SPropValue_data(properties, mapitag);
	if (filetime) {
		time = filetime->dwHighDateTime;
		time = time << 32;
		time |= filetime->dwLowDateTime;
		date = nt_time_string(mem_ctx, time);
		printf("\t%-15s:   %s\n", label, date);
		fflush(0);
	}

	talloc_free(mem_ctx);
}


_PUBLIC_ void mapidump_date_SPropValue(struct SPropValue lpProp, const char *label)
{
	TALLOC_CTX		*mem_ctx;
	NTTIME			time;
	const struct FILETIME		*filetime;
	const char		*date;

	mem_ctx = talloc_init("mapidump_date_SPropValue");

	filetime = (const struct FILETIME *) get_SPropValue_data(&lpProp);
	if (filetime) {
		time = filetime->dwHighDateTime;
		time = time << 32;
		time |= filetime->dwLowDateTime;
		date = nt_time_string(mem_ctx, time);
		printf("\t%s:   %s\n", label, date);
		fflush(0);
	}

	talloc_free(mem_ctx);
}

/**
   \details This function dumps the properties relating to a message to standard output

   The expected way to obtain the properties array is to use OpenMessage() to obtain the
   message object, then to use GetPropsAll() to obtain all the properties.

   \param properties array of message properties
   \param id identification to display for the message (can be NULL)

   \sa mapidump_appointment, mapidump_contact, mapidump_task, mapidump_note
*/
_PUBLIC_ void mapidump_message(struct mapi_SPropValue_array *properties, const char *id)
{
	const char			*msgid;
	const char			*from;
	const char			*to;
	const char			*cc;
	const char			*bcc;
	const char			*subject;
	const char			*body;
	const char			*codepage;
	const struct SBinary_short	*html = NULL;
	const uint8_t			*has_attach;
	const uint32_t       		*cp;

	msgid = (const char *)find_mapi_SPropValue_data(properties, PR_INTERNET_MESSAGE_ID);
	subject = (const char *) find_mapi_SPropValue_data(properties, PR_CONVERSATION_TOPIC);
	body = (const char *) find_mapi_SPropValue_data(properties, PR_BODY);
	if (!body) {
		body = (const char *) find_mapi_SPropValue_data(properties, PR_BODY_UNICODE);
		if (!body) {
			html = (const struct SBinary_short *) find_mapi_SPropValue_data(properties, PR_HTML);
		}
	}
	from = (const char *) find_mapi_SPropValue_data(properties, PR_SENT_REPRESENTING_NAME);
	to = (const char *) find_mapi_SPropValue_data(properties, PR_DISPLAY_TO);
	cc = (const char *) find_mapi_SPropValue_data(properties, PR_DISPLAY_CC);
	bcc = (const char *) find_mapi_SPropValue_data(properties, PR_DISPLAY_BCC);

	has_attach = (const uint8_t *)find_mapi_SPropValue_data(properties, PR_HASATTACH);

	cp = (const uint32_t *)find_mapi_SPropValue_data(properties, PR_MESSAGE_CODEPAGE);
	switch (cp ? *cp : 0) {
	case CP_USASCII:
		codepage = "CP_USASCII";
		break;
	case CP_UNICODE:
		codepage = "CP_UNICODE";
		break;
	case CP_JAUTODETECT:
		codepage = "CP_JAUTODETECT";
		break;
	case CP_KAUTODETECT:
		codepage = "CP_KAUTODETECT";
		break;
	case CP_ISO2022JPESC:
		codepage = "CP_ISO2022JPESC";
		break;
	case CP_ISO2022JPSIO:
		codepage = "CP_ISO2022JPSIO";
		break;
	default:
		codepage = "";
		break;
	}

	printf("+-------------------------------------+\n");
	printf("message id: %s %s\n", msgid ? msgid : "", id?id:"");
	printf("subject: %s\n", subject ? subject : "");
	printf("From: %s\n", from ? from : "");
	printf("To:  %s\n", to ? to : "");
	printf("Cc:  %s\n", cc ? cc : "");
	printf("Bcc: %s\n", bcc ? bcc : "");
	if (has_attach) {
		printf("Attachment: %s\n", *has_attach ? "True" : "False");
	}
	printf("Codepage: %s\n", codepage);
	printf("Body:\n");
	fflush(0);
	if (body) {
		printf("%s\n", body ? body : "");
	} else if (html) {
		write(1, html->lpb, html->cb);
		write(1, "\n", 1);
		fflush(0);
	}
}

/**
   \details This function dumps the properties relating to an appointment to standard output

   The expected way to obtain the properties array is to use OpenMessage() to obtain the
   appointment object, then to use GetPropsAll() to obtain all the properties.

   \param properties array of appointment properties
   \param id identification to display for the appointment (can be NULL)

   \sa mapidump_message, mapidump_contact, mapidump_task, mapidump_note
*/
_PUBLIC_ void mapidump_appointment(struct mapi_SPropValue_array *properties, const char *id)
{
	const struct mapi_SLPSTRArray	*contacts = NULL;
	const char		*subject = NULL;
	const char		*location= NULL;
	const char		*timezone = NULL;
	const uint32_t		*status;
	const uint8_t	       	*priv = NULL;
	uint32_t       		i;

	contacts = (const struct mapi_SLPSTRArray *)find_mapi_SPropValue_data(properties, PidLidContacts);
	subject = (const char *)find_mapi_SPropValue_data(properties, PR_CONVERSATION_TOPIC);
	timezone = (const char *)find_mapi_SPropValue_data(properties, PidLidTimeZoneDescription);
	location = (const char *)find_mapi_SPropValue_data(properties, PidLidLocation);
	status = (const uint32_t *)find_mapi_SPropValue_data(properties, PidLidBusyStatus);
	priv = (const uint8_t *)find_mapi_SPropValue_data(properties, PidLidPrivate);

	printf("|== %s ==| %s\n", subject?subject:"", id?id:"");
	fflush(0);

	if (location) {
		printf("\tLocation: %s\n", location);
		fflush(0);
	}

	mapidump_date(properties, PR_START_DATE, "Start time");
	mapidump_date(properties, PR_END_DATE, "End time");

	if (timezone) {
		printf("\tTimezone: %s\n", timezone);
		fflush(0);
	}

	printf("\tPrivate: %s\n", (priv && (*priv == true)) ? "True" : "False");
	fflush(0);

	if (status) {
		printf("\tStatus: %s\n", get_task_status(*status));
		fflush(0);
	}

	if (contacts) {
		printf("\tContacts:\n");
		fflush(0);
		for (i = 0; i < contacts->cValues; i++) {
			printf("\t\tContact: %s\n", contacts->strings[i].lppszA);
			fflush(0);
		}
	}	
}

/**
   \details This function dumps the properties relating to a contact (address book entry)
   to standard output

   The expected way to obtain the properties array is to use OpenMessage() to obtain the
   contact object, then to use GetPropsAll() to obtain all the properties.

   \param properties array of contact properties
   \param id identification to display for the contact (can be NULL)

   \sa mapidump_message, mapidump_appointment, mapidump_task, mapidump_note
*/
_PUBLIC_ void mapidump_contact(struct mapi_SPropValue_array *properties, const char *id)
{
	const char	*card_name =NULL;
	const char	*topic =NULL;
	const char	*full_name = NULL;
	const char	*given_name = NULL;
	const char	*surname = NULL;
	const char	*company = NULL;
	const char	*email = NULL;
	const char	*title = NULL;
	const char      *office_phone = NULL;
	const char      *home_phone = NULL;
	const char      *mobile_phone = NULL;
	const char      *postal_address = NULL;
	const char      *street_address = NULL;
	const char      *locality = NULL;
	const char      *state = NULL;
	const char      *country = NULL;
	const char      *department = NULL;
	const char      *business_fax = NULL;
	const char      *business_home_page = NULL;

	card_name = (const char *)find_mapi_SPropValue_data(properties, PidLidFileUnder);
	topic = (const char *)find_mapi_SPropValue_data(properties, PR_CONVERSATION_TOPIC);
	company = (const char *)find_mapi_SPropValue_data(properties, PR_COMPANY_NAME);
	title = (const char *)find_mapi_SPropValue_data(properties, PR_TITLE);
	full_name = (const char *)find_mapi_SPropValue_data(properties, PR_DISPLAY_NAME);
	given_name = (const char *)find_mapi_SPropValue_data(properties, PR_GIVEN_NAME);
	surname = (const char *)find_mapi_SPropValue_data(properties, PR_SURNAME);
	department = (const char *)find_mapi_SPropValue_data(properties, PR_DEPARTMENT_NAME);
	email = (const char *)find_mapi_SPropValue_data(properties, PidLidEmail1OriginalDisplayName);
	office_phone = (const char *)find_mapi_SPropValue_data(properties, PR_OFFICE_TELEPHONE_NUMBER);
	home_phone = (const char *)find_mapi_SPropValue_data(properties, PR_HOME_TELEPHONE_NUMBER);
	mobile_phone = (const char *)find_mapi_SPropValue_data(properties, PR_MOBILE_TELEPHONE_NUMBER);
	business_fax = (const char *)find_mapi_SPropValue_data(properties, PR_BUSINESS_FAX_NUMBER);
	business_home_page = (const char *)find_mapi_SPropValue_data(properties, PR_BUSINESS_HOME_PAGE);
	postal_address = (const char*)find_mapi_SPropValue_data(properties, PR_POSTAL_ADDRESS);
	street_address = (const char*)find_mapi_SPropValue_data(properties, PR_STREET_ADDRESS);
	locality = (const char*)find_mapi_SPropValue_data(properties, PR_LOCALITY);
	state = (const char*)find_mapi_SPropValue_data(properties, PR_STATE_OR_PROVINCE);
	country = (const char*)find_mapi_SPropValue_data(properties, PR_COUNTRY);

	if (card_name) 
		printf("|== %s ==| %s\n", card_name, id?id:"");
	else if (topic)
		printf("|== %s ==| %s\n", topic, id?id:"");
	else 
	  printf("|== <Unknown> ==| %s\n", id?id:"");
	fflush(0);
	if (topic) printf("Topic: %s\n", topic);
	fflush(0);
	if (full_name)
		printf("Full Name: %s\n", full_name);
	else if (given_name && surname)
		printf("Full Name: %s %s\n", given_name, surname); // initials? l10n?
	fflush(0);
	if (title) printf("Job Title: %s\n", title);
	fflush(0);
	if (department) printf("Department: %s\n", department);
	fflush(0);
	if (company) printf("Company: %s\n", company);
	fflush(0);
	if (email) printf("E-mail: %s\n", email);
	fflush(0);
	if (office_phone) printf("Office phone number: %s\n", office_phone);
	fflush(0);
	if (home_phone) printf("Work phone number: %s\n", home_phone);
	fflush(0);
	if (mobile_phone) printf("Mobile phone number: %s\n", mobile_phone);
	fflush(0);
	if (business_fax) printf("Business fax number: %s\n", business_fax);
	fflush(0);
	if (business_home_page) printf("Business home page: %s\n", business_home_page);
	fflush(0);
	if (postal_address) printf("Postal address: %s\n", postal_address);
	fflush(0);
	if (street_address) printf("Street address: %s\n", street_address);
	fflush(0);
	if (locality) printf("Locality: %s\n", locality);
	fflush(0);
	if (state) printf("State / Province: %s\n", state);
	fflush(0);
	if (country) printf("Country: %s\n", country);
	fflush(0);

	printf("\n");
}

_PUBLIC_ const char *get_task_status(uint32_t status)
{
	switch (status) {
	case olTaskNotStarted:
		return ("Not Started");
	case olTaskInProgress:
		return ("In Progress");
	case olTaskComplete:
		return ("Completed");
	case olTaskWaiting:
		return ("Waiting on someone else");
	case olTaskDeferred:
		return ("Deferred");
	}

	return NULL;
}

_PUBLIC_ const char *get_importance(uint32_t importance)
{
	switch (importance) {
	case IMPORTANCE_LOW:
		return ("Low");
	case IMPORTANCE_NORMAL:
		return ("Normal");
	case IMPORTANCE_HIGH:
		return ("High");
	}
	return NULL;
}

/**
   \details This function dumps the properties relating to a task (to-do list entry)
   to standard output

   The expected way to obtain the properties array is to use OpenMessage() to obtain the
   task object, then to use GetPropsAll() to obtain all the properties.

   \param properties array of task properties
   \param id identification to display for the task (can be NULL)

   \sa mapidump_message, mapidump_appointment, mapidump_contact, mapidump_note
*/
_PUBLIC_ void mapidump_task(struct mapi_SPropValue_array *properties, const char *id)
{
	const struct mapi_SLPSTRArray	*contacts = NULL;
	const char			*subject = NULL;
	const char			*body = NULL;
	const double			*complete = 0;
	const uint32_t			*status;
	const uint32_t			*importance;
	const uint8_t			*private;
	uint32_t       			i;

	contacts = (const struct mapi_SLPSTRArray *)find_mapi_SPropValue_data(properties, PidLidContacts);
	subject = (const char *)find_mapi_SPropValue_data(properties, PR_CONVERSATION_TOPIC);
	body = (const char *)find_mapi_SPropValue_data(properties, PR_BODY);
	complete = (const double *)find_mapi_SPropValue_data(properties, PidLidPercentComplete);
	status = (const uint32_t *)find_mapi_SPropValue_data(properties, PidLidTaskStatus);
	importance = (const uint32_t *)find_mapi_SPropValue_data(properties, PR_IMPORTANCE);
	private = (const uint8_t *)find_mapi_SPropValue_data(properties, PidLidPrivate);

	printf("|== %s ==| %s\n", subject?subject:"", id?id:"");
	fflush(0);

	printf("\tBody: %s\n", body?body:"none");
	fflush(0);

	if (complete) {
		printf("\tComplete: %d %c\n", (uint32_t)(*complete * 100), '%');
		fflush(0);
	}

	if (status) {
		printf("\tStatus: %s\n", get_task_status(*status));
		fflush(0);
		if (*status == olTaskComplete) {
			mapidump_date(properties, PidLidTaskDateCompleted, "Date Completed");
		}
	}

	if (importance) {
		printf("\tImportance: %s\n", get_importance(*importance));
		fflush(0);
	}

	mapidump_date(properties, PidLidTaskDueDate,"Due Date");
	mapidump_date(properties, PidLidTaskStartDate, "Start Date");

	if (private) {
		printf("\tPrivate: %s\n", (*private == true)?"True":"False");
		fflush(0);
	} else {
		printf("\tPrivate: false\n");
		fflush(0);
	}

	if (contacts) {
		for (i = 0; i < contacts->cValues; i++) {
			printf("\tContact: %s\n", contacts->strings[i].lppszA);
			fflush(0);
		}
	}
}

/**
   \details This function dumps the properties relating to a note to standard output

   The expected way to obtain the properties array is to use OpenMessage() to obtain the
   note object, then to use GetPropsAll() to obtain all the properties.

   \param properties array of note properties
   \param id identification to display for the note (can be NULL)

   \sa mapidump_message, mapidump_appointment, mapidump_contact, mapidump_task
*/
_PUBLIC_ void mapidump_note(struct mapi_SPropValue_array *properties, const char *id)
{
	const char		*subject = NULL;
	const char		*body = NULL;

	subject = (const char *)find_mapi_SPropValue_data(properties, PR_CONVERSATION_TOPIC);
	body = (const char *)find_mapi_SPropValue_data(properties, PR_BODY);

	printf("|== %s ==| %s\n", subject?subject:"", id?id:"");
	fflush(0);
	
	mapidump_date(properties, PR_CLIENT_SUBMIT_TIME, "Submit Time");

	if (body) {
		printf("Content:\n");
		printf("%s\n", body);
		fflush(0);
	} else {
		body = (const char *)find_mapi_SPropValue_data(properties, PR_BODY_HTML);
		if (body) {
			printf("Content HTML:\n");
			printf("%s\n", body);
			fflush(0);
		}
	}
}

_PUBLIC_ void mapidump_msgflags(uint32_t MsgFlags, const char *sep)
{
	uint32_t	i;
	
	for (i = 0; mdump_msgflags[i].flag; i++) {
		if (MsgFlags & mdump_msgflags[i].flag) {
			printf("%s\t%s (0x%x)\n", sep?sep:"", 
			       mdump_msgflags[i].value, mdump_msgflags[i].flag);
			fflush(0);
		}
	}

}


_PUBLIC_ void mapidump_newmail(struct NewMailNotification *newmail, const char *sep)
{
	printf("%sParent Entry ID: 0x%"PRIx64"\n", sep?sep:"", newmail->FID);
	fflush(0);
	printf("%sMessage Entry ID: 0x%"PRIx64"\n", sep?sep:"", newmail->MID);
	fflush(0);
	printf("%sMessage flags:\n", sep?sep:"");
	fflush(0);
	mapidump_msgflags(newmail->MessageFlags, sep);
	if (newmail->UnicodeFlag == 0x0) {
		printf("%sMessage Class: %s\n", sep?sep:"", newmail->MessageClass.lpszA);
	} else {
		printf("%sMessage Class: %s\n", sep?sep:"", newmail->MessageClass.lpszW);
	}
	fflush(0);
}


_PUBLIC_ const char *mapidump_freebusy_month(uint32_t month, uint32_t year)
{
	uint32_t	realmonth;

	realmonth = month - (year * 16);

	switch (realmonth) {
	case 0x1:
		return "January";
	case 0x2:
		return "February";
	case 0x3:
		return "March";
	case 0x4:
		return "April";
	case 0x5:
		return "May";
	case 0x6:
		return "June";
	case 0x7:
		return "July";
	case 0x8:
		return "August";
	case 0x9:
		return "September";
	case 0xa:
		return "October";
	case 0xb:
		return "November";
	case 0xc:
		return "December";
	}
	return NULL;
}


_PUBLIC_ void mapidump_freebusy_date(uint32_t t, const char *sep)
{
	TALLOC_CTX	*mem_ctx;
	NTTIME		time;
	const char	*date;

	mem_ctx = talloc_init("mapidump_freebusy_date");

	time = t;
	time *= 60;
	time *= 10000000;

	date = nt_time_string(mem_ctx, time);
	DEBUG(0, ("%s %-30s\n", sep, date));
	talloc_free((char *)date);
	talloc_free(mem_ctx);
}


_PUBLIC_ void mapidump_freebusy_event(struct Binary_r *bin, uint32_t month, uint32_t year, const char *sep)
{
	uint16_t	event_start;
	uint16_t	event_end;
	uint32_t	i;
	uint32_t       	hour;
	uint32_t       	hours;
	uint32_t	day;
	const char	*month_name;
	uint32_t	last;
	uint32_t	minutes;

	if (!bin) return;
	/* bin.cb must be a multiple of 4 */
	if (bin->cb % 4) return;

	month_name = mapidump_freebusy_month(month, year);

	for (i = 0; i < bin->cb; i+= 4) {
		event_start = (bin->lpb[i + 1] << 8) | bin->lpb[i];
		event_end = (bin->lpb[i + 3] << 8) | bin->lpb[i + 2];

		for (hour = 0; hour < 24; hour++) {
			if (!((event_start - (60 * hour)) % 1440)) {
				day = ((event_start - (60 * hour)) / 1440) + 1;
				last = event_end - event_start;
				DEBUG(0, ("%s %d %s %d at %d hours and lasts ", sep ? sep : "", day, month_name, year, hour + daylight));
				if (last < 60) {
					DEBUG(0, ("%d minutes\n", last));
				} else {
					hours = last / 60;
					minutes = last - hours * 60;
					if (minutes > 0) {
						DEBUG(0, ("%d hours and %d minutes\n", hours, minutes));
					} else {
						DEBUG(0, ("%d hours\n", hours));
					}
				}
			}
		}
		
	}	
}
