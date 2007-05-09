/*
 *  OpenChange MAPI Implementation
 *
 *  Copyright (C) Julien Kerihuel 2007.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>

_PUBLIC_ void mapidump_SPropValue(struct SPropValue lpProp, const char *sep)
{
	const char	*proptag;
	void		*data;

	proptag = get_proptag_name(lpProp.ulPropTag);

	switch(lpProp.ulPropTag & 0xFFFF) {
	case PT_I8:
		data = get_SPropValue_data(&lpProp);
		printf("%s%s: %llx\n", sep?sep:"", proptag, (*(uint64_t *)data));
		break;
	case PT_STRING8:
	case PT_UNICODE:
		data = get_SPropValue_data(&lpProp);
		printf("%s%s: %s\n", sep?sep:"", proptag, (char *)data);
		break;
	case PT_ERROR:
		data = get_SPropValue_data(&lpProp);
		printf("%s%s: 0x%.8x\n", sep?sep:"", proptag, (*(uint32_t *)data));
		break;
	case PT_LONG:
		data = get_SPropValue_data(&lpProp);
		printf("%s%s: %d\n", sep?sep:"", proptag, (*(uint32_t *)data));
		break;
	default:
		break;
	}

}

_PUBLIC_ void mapidump_SPropTagArray(struct SPropTagArray *proptags)
{
	uint32_t	count;
	const char	*proptag;

	if (!proptags) return;

	for (count = 0; count != proptags->cValues; count++) {
		proptag = get_proptag_name(proptags->aulPropTag[count]);
		printf("%s\n", proptag);
	}
}

_PUBLIC_ void mapidump_SRow(struct SRow *aRow, const char *sep)
{
	uint32_t		i;

	for (i = 0; i < aRow->cValues; i++) {
		mapidump_SPropValue(aRow->lpProps[i], sep);
	}
}

_PUBLIC_ void mapidump_Recipients(const char **usernames, struct SRowSet *rowset, struct FlagList *flaglist)
{
	uint32_t		i;
	uint32_t		j;

	for (i = 0, j= 0; i < flaglist->cFlags; i++) {
		switch (flaglist->ulFlags[i]) {
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
		}
	}
}

_PUBLIC_ void mapidump_date(struct mapi_SPropValue_array *properties, uint32_t mapitag, const char *label)
{
	TALLOC_CTX		*mem_ctx;
	NTTIME			time;
	struct FILETIME		*filetime;
	const char		*date;

	mem_ctx = talloc_init("mapidump_date");

	filetime = (struct FILETIME *) find_mapi_SPropValue_data(properties, mapitag);
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

_PUBLIC_ void mapidump_message(struct mapi_SPropValue_array *properties)
{
	const char		*msgid;
	const char		*from;
	const char		*to;
	const char		*cc;
	const char		*bcc;
	const char		*subject;
	const char		*body;
	const char		*codepage;
	struct SBinary_short	*html = NULL;
	uint8_t			*has_attach;
	uint32_t       		*cp;

	msgid = (char *)find_mapi_SPropValue_data(properties, PR_INTERNET_MESSAGE_ID);
	subject = (char *) find_mapi_SPropValue_data(properties, PR_CONVERSATION_TOPIC);
	body = (char *) find_mapi_SPropValue_data(properties, PR_BODY);
	if (!body) {
		body = (char *) find_mapi_SPropValue_data(properties, PR_BODY_UNICODE);
		if (!body) {
			html = (struct SBinary_short *) find_mapi_SPropValue_data(properties, PR_HTML);
		}
	}
	from = (char *) find_mapi_SPropValue_data(properties, PR_SENT_REPRESENTING_NAME);
	to = (char *) find_mapi_SPropValue_data(properties, PR_DISPLAY_TO);
	cc = (char *) find_mapi_SPropValue_data(properties, PR_DISPLAY_CC);
	bcc = (char *) find_mapi_SPropValue_data(properties, PR_DISPLAY_BCC);

	has_attach = (uint8_t *)find_mapi_SPropValue_data(properties, PR_HASATTACH);

	cp = (uint32_t *)find_mapi_SPropValue_data(properties, PR_MESSAGE_CODEPAGE);
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
	printf("message id: %s\n", msgid ? msgid : "");
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

_PUBLIC_ void mapidump_appointment(struct mapi_SPropValue_array *properties)
{
	TALLOC_CTX	*mem_ctx;
	const char	*subject;
	const char	*location;

	mem_ctx = talloc_init("mapidump_appointment");

	subject = (char *)find_mapi_SPropValue_data(properties, PR_CONVERSATION_TOPIC);
	location = (char *)find_mapi_SPropValue_data(properties, PR_APPOINTMENT_LOCATION);

	printf("%s (%s)\n", subject, location);
	fflush(0);
	mapidump_date(properties, PR_START_DATE, "\tStart time");
	mapidump_date(properties, PR_END_DATE, "\tEnd time");

	talloc_free(mem_ctx);
}

_PUBLIC_ void mapidump_contact(struct mapi_SPropValue_array *properties)
{
	const char	*card_name =NULL;
	const char	*full_name = NULL;
	const char	*company = NULL;
	const char	*email = NULL;
	const char	*title = NULL;

	card_name = (char *)find_mapi_SPropValue_data(properties, PR_CONTACT_CARD_NAME);
	company = (char *)find_mapi_SPropValue_data(properties, PR_COMPANY_NAME);
	title = (char *)find_mapi_SPropValue_data(properties, PR_TITLE);
	full_name = (char *)find_mapi_SPropValue_data(properties, PR_DISPLAY_NAME);
	email = (char *)find_mapi_SPropValue_data(properties, PR_CONTACT_CARD_EMAIL_ADDRESS);

	printf("|== %s ==|\n", card_name?card_name:"<Unknown>");
	fflush(0);
	if (full_name) printf("Full Name: %s\n", full_name);
	fflush(0);
	if (title) printf("Job Title: %s\n", title);
	fflush(0);
	if (company) printf("Company: %s\n", company);
	fflush(0);
	if (email) printf("E-mail: %s\n", email);
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

_PUBLIC_ const char *get_priority(uint32_t priority)
{
	switch (priority) {
	case PRIORITY_LOW:
		return ("Low");
	case PRIORITY_NORMAL:
		return ("Normal");
	case PRIORITY_HIGH:
		return ("High");
	}
	return NULL;
}

_PUBLIC_ void mapidump_task(struct mapi_SPropValue_array *properties)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_SLPSTRArray	*keywords = NULL;
	struct mapi_SLPSTRArray	*contacts = NULL;
	const char		*subject = NULL;
	const char		*body = NULL;
	double			*complete = 0;
	uint32_t		*status;
	uint32_t		*priority;
	uint8_t			*private;
	int			i;

	mem_ctx = talloc_init("mapidump_task");

	keywords = (struct mapi_SLPSTRArray *)find_mapi_SPropValue_data(properties, PR_EMS_AB_MONITORING_CACHED_VIA_MAIL);
	contacts = (struct mapi_SLPSTRArray *)find_mapi_SPropValue_data(properties, PR_Contacts);
	subject = (char *)find_mapi_SPropValue_data(properties, PR_CONVERSATION_TOPIC);
	body = (char *)find_mapi_SPropValue_data(properties, PR_BODY);
	complete = (double *)find_mapi_SPropValue_data(properties, PR_PercentComplete);
	status = (uint32_t *)find_mapi_SPropValue_data(properties, PR_Status);
	priority = (uint32_t *)find_mapi_SPropValue_data(properties, PR_PRIORITY);
	private = (uint8_t *)find_mapi_SPropValue_data(properties, PR_Private);

	printf("|== %s ==|\n", subject?subject:"");
	fflush(0);

	printf("\tBody: %s\n", body?body:"none");
	fflush(0);

	if (keywords) {
		for (i = 0; i < keywords->cValues; i++) {
			printf("\tCategory: %s\n", keywords->strings[i].lppszA);
		}
	}
	
	if (complete) {
		printf("\tComplete: %d %c\n", (uint32_t)(*complete * 100), '%');
		fflush(0);
	}

	if (status) {
		printf("\tStatus: %s\n", get_task_status(*status));
		fflush(0);
		if (*status == olTaskComplete) {
			mapidump_date(properties, PR_DateCompleted, "\tDate Completed");
		}
	}

	printf("\tPriority: %s\n", get_priority(*priority));
	fflush(0);

	mapidump_date(properties, PR_CommonEnd,"\tDue Date");
	mapidump_date(properties, PR_CommonStart, "\tStart Date");

	if (private) {
		printf("\tPrivate: %s\n", (*private == True)?"True":"False");
		fflush(0);
	}

	if (contacts) {
		for (i = 0; i < contacts->cValues; i++) {
			printf("\tContact: %s\n", contacts->strings[i].lppszA);
			fflush(0);
		}
	}

	talloc_free(mem_ctx);
}
