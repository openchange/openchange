/*
   Convert Exchange appointments and meetings to ICAL files

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

#include <utils/exchange2ical/exchange2ical.h>
#include <utils/openchange-tools.h>

struct RRULE_byday {
	uint16_t	DayOfWeek;
	const char	*DayName;
};

static const struct RRULE_byday RRULE_byday[] = {
	{ 0x0000,	"SU" },
	{ 0x0001,	"MO" },
	{ 0x0002,	"TU" },
	{ 0x0003,	"WE" },
	{ 0x0004,	"TH" },
	{ 0x0005,	"FR" },
	{ 0x0006,	"SA" },
	{ 0x0007,	NULL }
};


void ical_property_ATTENDEE(struct exchange2ical *exchange2ical)
{
	uint32_t	i;
	const char	*smtp;
	const char	*display_name;
	uint32_t	*RecipientFlags;
	uint32_t	*RecipientType;
	struct SRowSet	*SRowSet;

	/* Sanity check */
	if (!exchange2ical->apptStateFlags) return;
	if (!(*exchange2ical->apptStateFlags & 0x1)) return;

	SRowSet = &(exchange2ical->Recipients.SRowSet);

	/* Loop over the recipient table */
	for (i = 0; i < SRowSet->cRows; i++) {
		smtp = (const char *) octool_get_propval(&(SRowSet->aRow[i]), PR_SMTP_ADDRESS);
		display_name = (const char *) octool_get_propval(&(SRowSet->aRow[i]), PR_RECIPIENT_DISPLAY_NAME);
		RecipientFlags = (uint32_t *) octool_get_propval(&(SRowSet->aRow[i]), PR_RECIPIENTS_FLAGS);
		RecipientType = (uint32_t *) octool_get_propval(&(SRowSet->aRow[i]), PR_RECIPIENT_TYPE);

		if (RecipientFlags && !(*RecipientFlags & 0x20) && !(*RecipientFlags & 0x2) &&
		    (RecipientType && *RecipientType)) {
			DEBUG(0, ("ATTENDEE;"));

			if (*RecipientType == 0x3) {
				DEBUG(0, ("CUTYPE=RESOURCE;"));
			}

			switch (*RecipientType) {
			case 0x00000002:
				DEBUG(0, ("ROLE=OPT-PARTICIPANT;"));
				break;
			case 0x00000003:
				DEBUG(0, ("ROLE=NON-PARTICIPANT;"));
				break;
			}

			if (exchange2ical->partstat) {
				DEBUG(0, ("PARTSTAT=%s:", exchange2ical->partstat));
			}

			DEBUG(0, ("CN=%s;", display_name));
			
			if (exchange2ical->ResponseRequested) {
				DEBUG(0, ("RSVP=%s:", (*exchange2ical->ResponseRequested == true) ? "TRUE" : "FALSE"));
			}

			if (smtp) {
				DEBUG(0, ("mailto:%s", smtp));
			} else {
				DEBUG(0, ("invalid:nomail"));
			}

			DEBUG(0, ("\n"));
		}
	}
}


void ical_property_CATEGORIES(struct exchange2ical *exchange2ical)
{
	uint32_t	i;

	/* Sanity check */
	if (!exchange2ical->Keywords) return;
	if (!exchange2ical->Keywords->cValues) return;

	DEBUG(0, ("CATEGORIES:"));
	for (i = 0; i < exchange2ical->Keywords->cValues - 1; i++) {
		DEBUG(0, ("%s,", exchange2ical->Keywords->strings[i]->lppszA));
	}
	DEBUG(0, ("%s\n", exchange2ical->Keywords->strings[i]->lppszA));
}


void ical_property_CLASS(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->sensitivity) return;

	DEBUG(0, ("CLASS: %s\n", get_ical_class(*exchange2ical->sensitivity)));
}


void ical_property_CONTACT(struct exchange2ical *exchange2ical)
{
	uint32_t	i;

	/* Sanity check */
	if (!exchange2ical->Contacts) return;
	if (!exchange2ical->Contacts->cValues) return;

	for (i = 0; i < exchange2ical->Contacts->cValues; i++) {
		DEBUG(0, ("CONTACT:%s\n", exchange2ical->Contacts->strings[i]->lppszA));
	}
}


void ical_property_CREATED(struct exchange2ical *exchange2ical)
{
	struct tm	*tm;
	char		outstr[200];

	/* Sanity check */
	if (!exchange2ical->created) return;

	tm = get_tm_from_FILETIME(exchange2ical->created);
	strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
	DEBUG(0, ("CREATED:%s\n", outstr));
}

void ical_property_DTSTART(struct exchange2ical *exchange2ical)
{
	struct tm	*tm;
	char		outstr[200];

	tm = get_tm_from_FILETIME(exchange2ical->apptStartWhole);

	/* If this is an all-day appointment */
	if (exchange2ical->apptSubType && (*exchange2ical->apptSubType == 0x1)) {
		strftime(outstr, sizeof (outstr), "%Y%m%d", tm);
		DEBUG(0, ("DTSTART;VALUE=DATE:%s\n", outstr));
	} else {
		/* If this is a recurring non all-day event */
		if (exchange2ical->Recurring && (*exchange2ical->Recurring == 0)) {
			strftime(outstr, sizeof (outstr), "%Y%m%dT%H%M%S", tm);
			DEBUG(0, ("DTSTART;TZID=\"%s\":%s\n",
				  exchange2ical->TimeZoneDesc, outstr));
		} else {
			/* If this is a non recurring non all-day event */
			strftime(outstr, sizeof (outstr), "%Y%m%dT%H%M%S", tm);
			DEBUG(0, ("DTSTART;TZID=\"%s\":%s\n",
				  exchange2ical->TimeZoneDesc, outstr));
		}
	}
}

void ical_property_DTEND(struct exchange2ical *exchange2ical)
{
	struct tm	*tm;
	char		outstr[200];

	tm = get_tm_from_FILETIME(exchange2ical->apptEndWhole);

	/* If this is an all-day appointment */
	if (exchange2ical->apptSubType && (*exchange2ical->apptSubType == 0x1)) {
		strftime(outstr, sizeof (outstr), "%Y%m%d", tm);
		DEBUG(0, ("DTEND;VALUE=DATE:%s\n", outstr));
	} else {
		/* If this is a recurring non all-day event */
		if (exchange2ical->Recurring && (*exchange2ical->Recurring == 0x1)) {
			strftime(outstr, sizeof (outstr), "%Y%m%dT%H%M%S", tm);
			DEBUG(0, ("DTEND;TZID=\"%s\":%s\n",
				  exchange2ical->TimeZoneDesc, outstr));
		} else {
			/* If this is a non recurring non all-day event */
			strftime(outstr, sizeof (outstr), "%Y%m%dT%H%M%S", tm);
			DEBUG(0, ("DTEND;TZID=\"%s\":%s\n",
				  exchange2ical->TimeZoneDesc, outstr));
		}
	}
}


void ical_property_DTSTAMP(struct exchange2ical *exchange2ical)
{
	struct tm	*tm;
	char		outstr[200];

	/* Sanity check */
	if (!exchange2ical->OwnerCriticalChange) return;

	/* FIXME: if the property doesn't exist, we should use system
	 * time instead 
	 */

	tm = get_tm_from_FILETIME(exchange2ical->OwnerCriticalChange);
	strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
	DEBUG(0, ("DTSTAMP:%s\n", outstr));
}


void ical_property_DESCRIPTION(struct exchange2ical *exchange2ical)
{
	if (exchange2ical->method && !strcmp(exchange2ical->method, "REPLY")) return;
	if (!exchange2ical->body) return;

	DEBUG(0, ("DESCRIPTION: %s\n", exchange2ical->body));
}


void ical_property_EXDATE(struct exchange2ical *exchange2ical)
{
	uint32_t	i;
	NTTIME		time;
	struct timeval	t;
	struct tm	*tm;
	char		outstr[200];

	/* Sanity check */
	if (exchange2ical->Recurring && (*exchange2ical->Recurring == 0x0)) return;
	if (!exchange2ical->RecurrencePattern) return;
	if (!exchange2ical->RecurrencePattern->DeletedInstanceCount) return;

	for (i = 0; i < exchange2ical->RecurrencePattern->DeletedInstanceCount; i++) {
		/* If this is an all-day appointment */
		if (exchange2ical->apptSubType && (*exchange2ical->apptSubType == 0x1)) {
			/* There is a bug in MS-OXOCAL
			 * documentation. For all-day appointment,
			 * DeletedInstanceDates is set to a value
			 * which can't be the number of minutes since
			 * blabla */
		} else {
			/* number of minutes between midnight of the specified
			 * day and midnight, January 1, 1601 */
			time = exchange2ical->RecurrencePattern->DeletedInstanceDates[i];
			time *= 60;
			time *= 10000000;
			nttime_to_timeval(&t, time);
			tm = localtime(&t.tv_sec);

			strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%S", tm);
			DEBUG(0, ("EXDATE;TZID=\"%s\":%s\n", exchange2ical->TimeZoneDesc, outstr));
		}
	}
}


void ical_property_LAST_MODIFIED(struct exchange2ical *exchange2ical)
{
	struct tm	*tm;
	char		outstr[200];

	/* Sanity check */
	if (!exchange2ical->LastModified) return;

	/* FIXME: if the property doesn't exist, we should use system
	 * time instead 
	 */

	tm = get_tm_from_FILETIME(exchange2ical->LastModified);
	strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
	DEBUG(0, ("LAST-MODIFIED:%s\n", outstr));	
}


void ical_property_LOCATION(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->Location) return;

	DEBUG(0, ("LOCATION:%s\n", exchange2ical->Location));
}


void ical_property_ORGANIZER(struct exchange2ical *exchange2ical)
{
	const char	*smtp;
	const char	*display_name;
	uint32_t	*RecipientFlags;
	uint32_t	*RecipientType;
	uint32_t	i;
	struct SRowSet	*SRowSet;

	/* Sanity check */
	if (!exchange2ical->apptStateFlags) return;
	if (!(*exchange2ical->apptStateFlags & 0x1)) return;

	SRowSet = &(exchange2ical->Recipients.SRowSet);

	/* Loop over the recipient table */
	for (i = 0; i < SRowSet->cRows; i++) {
		smtp = (const char *) octool_get_propval(&(SRowSet->aRow[i]), PR_SMTP_ADDRESS);
		display_name = (const char *) octool_get_propval(&(SRowSet->aRow[i]), PR_RECIPIENT_DISPLAY_NAME);
		RecipientFlags = (uint32_t *) octool_get_propval(&(SRowSet->aRow[i]), PR_RECIPIENTS_FLAGS);
		RecipientType = (uint32_t *) octool_get_propval(&(SRowSet->aRow[i]), PR_RECIPIENT_TYPE);

		if (RecipientFlags && !(*RecipientFlags & 0x20) &&
		    ((*RecipientFlags & 0x2) || (RecipientType && !*RecipientType))) {
			if (smtp) {
				DEBUG(0, ("ORGANIZER;CN=\"%s\":mailto:%s\n", display_name, smtp));
				break;
			} else {
				DEBUG(0, ("ORGANIZER;CN=\"%s\":invalid:nomail\n", display_name));
				break;
			}
		}
	}
}


void ical_property_PRIORITY(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->Importance) return;

	switch (*exchange2ical->Importance) {
	case 0x00000000:
		DEBUG(0, ("PRIORITY:9\n"));
		break;
	case 0x00000001:
		DEBUG(0, ("PRIORITY:5\n"));
		break;
	case 0x00000002:
		DEBUG(0, ("PRIORITY:1\n"));
		break;
	}
}


void ical_property_RDATE(struct exchange2ical *exchange2ical)
{
	uint32_t	i;
	NTTIME		time;
	struct timeval	t;
	struct tm	*tm;
	char		outstr[200];

	/* Sanity check */
	if (exchange2ical->Recurring && (*exchange2ical->Recurring == 0x0)) return;
	if (!exchange2ical->RecurrencePattern) return;
	if (!exchange2ical->RecurrencePattern->ModifiedInstanceCount) return;

	for (i = 0; i < exchange2ical->RecurrencePattern->ModifiedInstanceCount; i++) {
		/* If this is an all-day appointment */
		if (exchange2ical->apptSubType && (*exchange2ical->apptSubType == 0x1)) {
			/* There is a bug in MS-OXOCAL
			 * documentation. For all-day appointment,
			 * DeletedInstanceDates is set to a value
			 * which can't be the number of minutes since
			 * January 1st, 1601 */
		} else {
			/* number of minutes between midnight of the specified
			 * day and midnight, January 1st, 1601 */
			time = exchange2ical->RecurrencePattern->ModifiedInstanceDates[i];
			time *= 60;
			time *= 10000000;
			nttime_to_timeval(&t, time);
			tm = localtime(&t.tv_sec);

			strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%S", tm);
			DEBUG(0, ("RDATE;TZID=\"%s\":%s\n", exchange2ical->TimeZoneDesc, outstr));
		}
	}	
}


static const char *get_RRULE_byday(uint16_t DayOfWeek)
{
	uint16_t	i;

	for (i = 0; RRULE_byday[i].DayName; i++) {
		if (DayOfWeek == RRULE_byday[i].DayOfWeek) {
			return RRULE_byday[i].DayName;
		}
	}
	
	return NULL;
}

void ical_property_RRULE(struct exchange2ical *exchange2ical, struct SYSTEMTIME date)
{
	/* Sanity check */
	if (has_component_DAYLIGHT(exchange2ical) == false) return;

	if (exchange2ical->TimeZoneStruct->stStandardDate.wDayOfWeek) {
		DEBUG(0, ("RRULE:FREQ=YEARLY;BYDAY=%d%s;BYMONTH=%d\n",
			  date.wDay, get_RRULE_byday(date.wDayOfWeek), date.wMonth));
	} else {
		DEBUG(0, ("RRULE:FREQ=YEARLY;BYMONTHDAY=%d;BYMONTH=%d\n",
			  date.wDay, date.wMonth));
	}
}



void ical_property_RECURRENCE_ID(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->ExceptionReplaceTime) return;

	/* FIXME: I'm too lazy to implement this today */
	DEBUG(0, ("RECURRENCE-ID:\n"));
}


void ical_property_RESOURCES(struct exchange2ical *exchange2ical)
{
	char	*NonSendableBcc = NULL;

	/* Sanity check */
	if (!exchange2ical->NonSendableBcc) return;

	NonSendableBcc = talloc_strdup(exchange2ical->mem_ctx, exchange2ical->NonSendableBcc);
	all_string_sub(NonSendableBcc, ";", ",", 0);
	DEBUG(0, ("RESOURCES:%s\n", NonSendableBcc));
	talloc_free(NonSendableBcc);
}


void ical_property_SEQUENCE(struct exchange2ical *exchange2ical)
{
	if (!exchange2ical->Sequence) {
		DEBUG(0, ("SEQUENCE:0\n"));
	} else {
		DEBUG(0, ("SEQUENCE:%d\n", *exchange2ical->Sequence));
	}
}


void ical_property_SUMMARY(struct exchange2ical *exchange2ical)
{
	DEBUG(0, ("SUMMARY:%s\n", exchange2ical->Subject ? exchange2ical->Subject : ""));
}


void ical_property_TRANSP(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->BusyStatus) return;

	switch (*exchange2ical->BusyStatus) {
	case 0x00000000:
		DEBUG(0, ("TRANSP:TRANSPARENT\n"));
		break;
	case 0x00000001:
	case 0x00000002:
	case 0x00000003:
		DEBUG(0, ("TRANSP:OPAQUE\n"));
		break;
	}
}


void ical_property_TRIGGER(struct exchange2ical *exchange2ical)
{
	if (!exchange2ical->ReminderDelta) return;

	if (*exchange2ical->ReminderDelta == 0x5AE980E1) {
		DEBUG(0, ("TRIGGER:-PT15M\n"));
	} else {
		DEBUG(0, ("TRIGGER:-PT%dM\n", *exchange2ical->ReminderDelta));
	}
}


#define	GLOBAL_OBJECT_ID_DATA_START	"\x76\x43\x61\x6c\x2d\x55\x69\x64\x01\x00\x00\x00"

void ical_property_UID(struct exchange2ical *exchange2ical)
{
	uint32_t		i;
	const char		*uid;
	struct GlobalObjectId	*GlbObjId;

	/* Sanity check */
	if (!exchange2ical->GlobalObjectId) return;
	
	GlbObjId = get_GlobalObjectId(exchange2ical->mem_ctx, exchange2ical->GlobalObjectId);
	if (!GlbObjId) return;

	DEBUG(0, ("UID:"));

	if (GlbObjId->Size >= 12 && !memcmp(GlbObjId->Data, GLOBAL_OBJECT_ID_DATA_START, 12)) {
		for (i = 0; i < 52; i++) {
			DEBUG(0, ("%.2X", exchange2ical->GlobalObjectId->lpb[i]));
		}

		uid = (const char *)&(GlbObjId->Data[13]);
		DEBUG(0, ("%s", uid));

	} else {
		for (i = 0; i < 16; i++) {
			DEBUG(0, ("%.2X", exchange2ical->GlobalObjectId->lpb[i]));
		}
		/* YH, YL, Month and D must be set to 0 */
		DEBUG(0, ("00000000"));

		for (i = 20; i < exchange2ical->GlobalObjectId->cb; i++) {
			DEBUG(0, ("%.2X", exchange2ical->GlobalObjectId->lpb[i]));
		}
	}

	DEBUG(0, ("\n"));
	talloc_free(GlbObjId);
}


void ical_property_X_MICROSOFT_CDO_ATTENDEE_CRITICAL_CHANGE(struct exchange2ical *exchange2ical)
{
	struct tm	*tm;
	char		outstr[200];

	/* Sanity check */
	if (!exchange2ical->AttendeeCriticalChange) return;

	tm = get_tm_from_FILETIME(exchange2ical->AttendeeCriticalChange);
	strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
	DEBUG(0, ("X-MICROSOFT-CDO-ATTENDEE-CRITICAL-CHANGE:%s\n", outstr));
}


void ical_property_X_MICROSOFT_CDO_BUSYSTATUS(struct exchange2ical *exchange2ical)
{
	/* Sanity Check */
	if (!exchange2ical->BusyStatus) return;

	switch (*exchange2ical->BusyStatus) {
	case 0x00000000:
		DEBUG(0, ("X-MICROSOFT-CDO-BUSYSTATUS:FREE\n"));
		break;
	case 0x00000001:
		DEBUG(0, ("X-MICROSOFT-CDO-BUSYSTATUS:TENTATIVE\n"));
		break;
	case 0x00000002:
		DEBUG(0, ("X-MICROSOFT-CDO-BUSYSTATUS:BUSY\n"));
		break;
	case 0x00000003:
		DEBUG(0, ("X-MICROSOFT-CDO-BUSYSTATUS:OOF\n"));
		break;
	}
}


void ical_property_X_MICROSOFT_CDO_INTENDEDSTATUS(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->IntendedBusyStatus) return;

	switch (*exchange2ical->IntendedBusyStatus) {
	case 0x00000000:
		DEBUG(0, ("X-MICROSOFT-CDO-INTENDEDSTATUS:FREE\n"));
		break;
	case 0x00000001:
		DEBUG(0, ("X-MICROSOFT-CDO-INTENDEDSTATUS:TENTATIVE\n"));
		break;
	case 0x00000002:
		DEBUG(0, ("X-MICROSOFT-CDO-INTENDEDSTATUS:BUSY\n"));
		break;
	case 0x00000003:
		DEBUG(0, ("X-MICROSOFT-CDO-INTENDEDSTATUS:OOF\n"));
		break;
	}
}


void ical_property_X_MICROSOFT_CDO_OWNERAPPTID(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->OwnerApptId) return;

	DEBUG(0, ("X-MICROSOFT-CDO-OWNERAPPTID:%d\n", *exchange2ical->OwnerApptId));
}


void ical_property_X_MICROSOFT_CDO_OWNER_CRITICAL_CHANGE(struct exchange2ical *exchange2ical)
{
	struct tm	*tm;
	char		outstr[200];

	/* Sanity check */
	if (!exchange2ical->OwnerCriticalChange) return;

	/* FIXME: if the property doesn't exist, we should use system
	 * time instead 
	 */

	tm = get_tm_from_FILETIME(exchange2ical->OwnerCriticalChange);
	strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
	DEBUG(0, ("X-MICROSOFT-CDO-OWNER-CRITICAL-CHANGE:%s\n", outstr));
}


void ical_property_X_MICROSOFT_CDO_REPLYTIME(struct exchange2ical *exchange2ical)
{
	struct tm	*tm;
	char		outstr[200];

	/* Sanity check */
	if (!exchange2ical->apptReplyTime) return;

	tm = get_tm_from_FILETIME(exchange2ical->apptReplyTime);
	strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
	DEBUG(0, ("X-MICROSOFT-CDO-REPLYTIME:%s\n", outstr));
}


void ical_property_X_MICROSOFT_DISALLOW_COUNTER(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->NotAllowPropose) return;

	DEBUG(0, ("X-MICROSOFT-DISALLOW-COUNTER:%s\n", (*exchange2ical->NotAllowPropose == true) ? "TRUE" : "FALSE"));
}


void ical_property_X_MS_OLK_ALLOWEXTERNCHECK(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->AllowExternCheck) return;

	DEBUG(0, ("X-MS-OLK-ALLOWEXTERNCHECK:%s\n", (*exchange2ical->AllowExternCheck == true) ? "TRUE" : "FALSE"));
}


void ical_property_X_MS_OLK_APPTLASTSEQUENCE(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->apptLastSequence) return;

	DEBUG(0, ("X-MS-OLK-APPTLASTSEQUENCE:%d\n", *exchange2ical->apptLastSequence));
}


void ical_property_X_MS_OLK_APPTSEQTIME(struct exchange2ical *exchange2ical)
{
	struct tm	*tm = NULL;
	char		outstr[200];

	/* Sanity check */
	if (!exchange2ical->apptSeqTime) return;

	tm = get_tm_from_FILETIME(exchange2ical->apptSeqTime);
	strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
	DEBUG(0, ("X-MS-OLK-APPTSEQTIME:%s\n", outstr));

}


void ical_property_X_MS_OLK_AUTOFILLLOCATION(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->AutoFillLocation) return;

	DEBUG(0, ("X-MS-OLK-AUTOFILLLOCATION:%s\n", (*exchange2ical->AutoFillLocation == true) ? "TRUE" : "FALSE"));
}


void ical_property_X_MS_OLK_AUTOSTARTCHECK(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->AutoStartCheck) return;

	DEBUG(0, ("X-MS-OLK-AUTOSTARTCHECK:%s\n", (*exchange2ical->AutoStartCheck == true) ? "TRUE" : "FALSE"));
}


void ical_property_X_MS_OLK_COLLABORATEDOC(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->CollaborateDoc) return;

	DEBUG(0, ("X-MS-OLK-COLLABORATEDOC:%s\n", exchange2ical->CollaborateDoc));
}

void ical_property_X_MS_OLK_CONFCHECK(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->ConfCheck) return;

	DEBUG(0, ("X-MS-OLK-CONFCHECK:%s\n", (*exchange2ical->ConfCheck == true) ? "TRUE" : "FALSE"));
}


void ical_property_X_MS_OLK_CONFTYPE(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->ConfType) return;

	DEBUG(0, ("X-MS-OLK-CONFTYPE:%d\n", *exchange2ical->ConfType));
}


void ical_property_X_MS_OLK_DIRECTORY(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->Directory) return;

	DEBUG(0, ("X-MS-OLK-DIRECTORY:%s\n", exchange2ical->Directory));
}


void ical_property_X_MS_OLK_MWSURL(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->MWSURL) return;

	DEBUG(0, ("X-MS-OLK-MWSURL:%s\n", exchange2ical->MWSURL));
}


void ical_property_X_MS_OLK_NETSHOWURL(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->NetShowURL) return;

	DEBUG(0, ("X-MS-OLK-NETSHOWURL:%s\n", exchange2ical->NetShowURL));
}


void ical_property_X_MS_OLK_ONLINEPASSWORD(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->OnlinePassword) return;

	DEBUG(0, ("X-MS-OLK-ONLINEPASSWORD:%s\n", exchange2ical->OnlinePassword));
}


void ical_property_X_MS_OLK_ORGALIAS(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->OrgAlias) return;

	DEBUG(0, ("X-MS-OLK-ORGALIAS:%s\n", exchange2ical->OrgAlias));
}


void ical_property_X_MS_OLK_SENDER(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->apptStateFlags) return;
	if (!(*exchange2ical->apptStateFlags & 0x1)) return;
	if (!exchange2ical->SenderName) return;

	if (exchange2ical->SenderEmailAddress) {
		DEBUG(0, ("X-MS-OLK-SENDER;CN=\"%s\":mailto:%s\n", 
			  exchange2ical->SenderName, exchange2ical->SenderEmailAddress));
	} else {
		DEBUG(0, ("X-MS-OLK-SENDER;CN=\"%s\":invalid:nomail\n", exchange2ical->SenderName));
	}
}
