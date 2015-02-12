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

#include "libexchange2ical/libexchange2ical.h"
#include <ldb.h>

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

static const char *get_filename(const char *filename)
{
	const char *substr;

	if (!filename) return NULL;

	substr = rindex(filename, '/');
	if (substr) return substr;

	return filename;
}


void ical_property_ATTACH(struct exchange2ical *exchange2ical)
{
	mapi_object_t			obj_tb_attach;
	mapi_object_t			obj_attach;
	mapi_object_t			obj_stream;
	struct SRowSet			rowset_attach;
	struct SPropTagArray		*SPropTagArray = NULL;
	const uint32_t			*attach_num = NULL;
	struct SPropValue		*lpProps;
	enum MAPISTATUS			retval;
	unsigned int			i;
	uint32_t			count;
	struct SRow			aRow2;

	
	mapi_object_init(&obj_tb_attach);
	retval = GetAttachmentTable(&exchange2ical->obj_message, &obj_tb_attach);
	if (retval == MAPI_E_SUCCESS) {
		SPropTagArray = set_SPropTagArray(exchange2ical->mem_ctx, 0x1, PR_ATTACH_NUM);
		retval = SetColumns(&obj_tb_attach, SPropTagArray);
		MAPIFreeBuffer(SPropTagArray);
		retval = QueryRows(&obj_tb_attach, 0xa, TBL_ADVANCE, TBL_FORWARD_READ, &rowset_attach);
		
		for (i = 0; i < rowset_attach.cRows; i++) {
			attach_num = (const uint32_t *)find_SPropValue_data(&(rowset_attach.aRow[i]), PR_ATTACH_NUM);
			retval = OpenAttach(&exchange2ical->obj_message, *attach_num, &obj_attach);

			if (retval == MAPI_E_SUCCESS) {

				SPropTagArray = set_SPropTagArray(exchange2ical->mem_ctx, 0x7,
									  PR_ATTACH_FILENAME,
									  PR_ATTACH_LONG_FILENAME,
									  PR_ATTACH_METHOD,
									  PR_ATTACHMENT_FLAGS,
									  PR_ATTACHMENT_HIDDEN,
									  PR_ATTACH_MIME_TAG,
									  PR_ATTACH_DATA_BIN
									  );
									  
				lpProps = NULL;
				retval = GetProps(&obj_attach, MAPI_UNICODE, SPropTagArray, &lpProps, &count);
				MAPIFreeBuffer(SPropTagArray);
				if (retval == MAPI_E_SUCCESS) {

					uint32_t		*attachmentFlags = NULL;
					uint32_t		*attachMethod = NULL;
					uint8_t			*attachmentHidden = NULL;
					const char 		*data = NULL;
					const char		*fmttype = NULL;
					const char		*attach_filename = NULL;
					DATA_BLOB		body;
					icalattach		*icalattach = NULL;
					icalproperty 		*prop = NULL;
					icalparameter 		*param = NULL;
						
					
					aRow2.ulAdrEntryPad = 0;
					aRow2.cValues = count;
					aRow2.lpProps = lpProps;
					
					attachmentFlags	 = octool_get_propval(&aRow2, PR_ATTACHMENT_FLAGS);
					attachMethod	 = octool_get_propval(&aRow2, PR_ATTACH_METHOD);
					attachmentHidden = octool_get_propval(&aRow2, PR_ATTACHMENT_HIDDEN);

					if(attachmentFlags && !(*attachmentFlags & 0x00000007) 
						&& (*attachMethod == 0x00000001) 
						&& (!attachmentHidden || !(*attachmentHidden))) {

						/* Get data of attachment */
						retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 0, &obj_stream);
						retval = octool_get_stream(exchange2ical->mem_ctx, &obj_stream, &body);
						data=ldb_base64_encode(exchange2ical->mem_ctx, (const char *)body.data, body.length);
						
						/*Create a new icalattach from above data*/
#if HAVE_ICAL_0_46
						/* the function signature for icalattach_new_from_data() changed in 0.46, released 2010-08-30 */
						/* we can switch to just using the new signature after everyone has had a reasonable chance to update (say end of 2011) */
						icalattach = icalattach_new_from_data(data, 0, 0);
#else
						icalattach = icalattach_new_from_data((unsigned char *)data,0,0);
#endif
						/*Add attach property to vevent component*/
						prop = icalproperty_new_attach(icalattach);
						icalcomponent_add_property(exchange2ical->vevent, prop);

						/* Attachment filename for X-FILENAME parameter*/
						attach_filename = get_filename(octool_get_propval(&aRow2, PR_ATTACH_LONG_FILENAME));
						if (!attach_filename || (attach_filename && !strcmp(attach_filename, ""))) {
							attach_filename = get_filename(octool_get_propval(&aRow2, PR_ATTACH_FILENAME));
						}
						
						/* fmttype parameter */
						fmttype = (const char *) octool_get_propval(&aRow2, PR_ATTACH_MIME_TAG);
						if(fmttype) {
							param = icalparameter_new_fmttype(fmttype);
							icalproperty_add_parameter(prop, param);
						}
						
						/* ENCODING parameter */
						param =icalparameter_new_encoding(ICAL_ENCODING_BASE64);
						icalproperty_add_parameter(prop,param);
						
						/* VALUE parameter */
						param=icalparameter_new_value(ICAL_VALUE_BINARY);
						icalproperty_add_parameter(prop,param);
						
						/* X-FILENAME parameter */
						param = icalparameter_new_x(attach_filename);
						icalparameter_set_xname(param,"X-FILENAME");
						icalproperty_add_parameter(prop,param);
					}
					MAPIFreeBuffer(lpProps);
				}
			}
		}
	}
	mapi_object_release(&obj_tb_attach);
}


// TODO: check this - need an example
void ical_property_ATTENDEE(struct exchange2ical *exchange2ical)
{
	uint32_t	i;
	const char	*smtp;
	const char	*display_name;
	uint32_t	*RecipientFlags;
	uint32_t	*RecipientType;
	uint32_t	*TrackStatus;
	struct SRowSet	*SRowSet;

	/* Sanity check */
	if (!exchange2ical->apptStateFlags) return;
	if (!(*exchange2ical->apptStateFlags & 0x1)) return;
	SRowSet = &(exchange2ical->Recipients.SRowSet);

	/* Loop over the recipient table */
	for (i = 0; i < SRowSet->cRows; i++) {
		smtp = (const char *) octool_get_propval(&(SRowSet->aRow[i]), PR_SMTP_ADDRESS);
		display_name = (const char *) octool_get_propval(&(SRowSet->aRow[i]), PR_RECIPIENT_DISPLAY_NAME);
		RecipientFlags = (uint32_t *) octool_get_propval(&(SRowSet->aRow[i]), PR_RECIPIENT_FLAGS);
		RecipientType = (uint32_t *) octool_get_propval(&(SRowSet->aRow[i]), PR_RECIPIENT_TYPE);
		TrackStatus  = (uint32_t *) octool_get_propval(&(SRowSet->aRow[i]), PR_RECIPIENT_TRACKSTATUS);


		if (RecipientFlags && !(*RecipientFlags & 0x20) && !(*RecipientFlags & 0x2) &&
		    (RecipientType && *RecipientType)) {
			icalproperty *prop;
			icalparameter *cn;
			icalparameter *participantType;
			enum icalparameter_partstat	partstat = ICAL_PARTSTAT_NONE;

			if (smtp) {
				char *mailtoURL;
				mailtoURL = talloc_strdup(exchange2ical->mem_ctx, "mailto:");
				mailtoURL = talloc_strdup_append(mailtoURL, smtp);
				prop = icalproperty_new_attendee(mailtoURL);
				icalcomponent_add_property(exchange2ical->vevent, prop);
			} else {
				prop = icalproperty_new_attendee("invalid:nomail");
				icalcomponent_add_property(exchange2ical->vevent, prop);
			}

			if (display_name) {
				cn = icalparameter_new_cn(display_name);
				icalproperty_add_parameter(prop, cn);
			}

			if (*RecipientType == 0x3) {
				icalparameter *cutype = icalparameter_new_cutype(ICAL_CUTYPE_RESOURCE);
				icalproperty_add_parameter(prop, cutype);
			}

			switch (*RecipientType) {
			case 0x00000002:
				participantType = icalparameter_new_role(ICAL_ROLE_OPTPARTICIPANT);
				icalproperty_add_parameter(prop, participantType);
				break;
			case 0x00000003:
				participantType = icalparameter_new_role(ICAL_ROLE_NONPARTICIPANT);
				icalproperty_add_parameter(prop, participantType);
				break;
			}

		
			
			if((exchange2ical->method==ICAL_METHOD_REPLY) || (exchange2ical->method==ICAL_METHOD_COUNTER)){
				partstat = exchange2ical->partstat;	
			}else if(exchange2ical->method==ICAL_METHOD_PUBLISH){
				if(TrackStatus){
					partstat = get_ical_partstat_from_status(*TrackStatus);
				}else if(exchange2ical->ResponseStatus){
					partstat = get_ical_partstat_from_status(*exchange2ical->ResponseStatus);
				}
			}
			
			if (partstat != ICAL_PARTSTAT_NONE) {
				icalparameter *param;
				param = icalparameter_new_partstat(partstat);
				icalproperty_add_parameter(prop, param);
			}

			if (exchange2ical->ResponseRequested) {
				icalparameter *rsvp;
				if (*(exchange2ical->ResponseRequested)) {
					rsvp = icalparameter_new_rsvp(ICAL_RSVP_TRUE);
				} else {
					rsvp = icalparameter_new_rsvp(ICAL_RSVP_FALSE);
				}
				icalproperty_add_parameter(prop, rsvp);
			}
		}
	}
}


void ical_property_CATEGORIES(struct exchange2ical *exchange2ical)
{
	uint32_t	i;
	icalproperty	*prop;

	/* Sanity check */
	if (!exchange2ical->Keywords) return;
	if (!exchange2ical->Keywords->cValues) return;

	for (i = 0; i < exchange2ical->Keywords->cValues; i++) {
		prop = icalproperty_new_categories(exchange2ical->Keywords->lppszA[i]); 
		icalcomponent_add_property(exchange2ical->vevent, prop);
	}

}


void ical_property_CLASS(struct exchange2ical *exchange2ical)
{
	icalproperty	*prop;

	/* Sanity check */
	if (!exchange2ical->sensitivity) return;

	prop = icalproperty_new_class(get_ical_class(*exchange2ical->sensitivity));
	icalcomponent_add_property(exchange2ical->vevent, prop);
}


void ical_property_CONTACT(struct exchange2ical *exchange2ical)
{
	icalproperty	*prop;
	uint32_t	i;

	/* Sanity check */
	if (!exchange2ical->Contacts) return;
	if (!exchange2ical->Contacts->cValues) return;

	for (i = 0; i < exchange2ical->Contacts->cValues; i++) {
		prop = icalproperty_new_contact(exchange2ical->Contacts->lppszA[i]);
		icalcomponent_add_property(exchange2ical->vevent, prop);
	}
}


void ical_property_CREATED(struct exchange2ical *exchange2ical)
{
	icalproperty		*prop;
	struct icaltimetype	icaltime;

	/* Sanity check */
	if (!exchange2ical->created) return;

	icaltime = get_icaltime_from_FILETIME_UTC(exchange2ical->created);

	prop = icalproperty_new_created(icaltime);
	icalcomponent_add_property(exchange2ical->vevent, prop);
}

void ical_property_DTSTART(struct exchange2ical *exchange2ical)
{
	icalproperty		*prop;
	icalparameter		*tzid;
	struct icaltimetype	icaltime;

	/* Sanity check */
	if (!exchange2ical->apptStartWhole) return;

	/* If this is an all-day appointment */
	if (exchange2ical->apptSubType && (*exchange2ical->apptSubType == 0x1)) {
		icaltime = get_icaldate_from_FILETIME(exchange2ical->apptStartWhole);
		prop = icalproperty_new_dtstart(icaltime);
		icalcomponent_add_property(exchange2ical->vevent, prop);
	} else {
		if (exchange2ical->TimeZoneDesc) {
			icaltime = get_icaltime_from_FILETIME(exchange2ical->apptStartWhole);
			prop = icalproperty_new_dtstart(icaltime);
			icalcomponent_add_property(exchange2ical->vevent, prop);
			tzid = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
			icalproperty_add_parameter(prop, tzid);
		} else {
			icaltime = get_icaltime_from_FILETIME_UTC(exchange2ical->apptStartWhole);
			prop = icalproperty_new_dtstart(icaltime);
			icalcomponent_add_property(exchange2ical->vevent, prop);
		}
	
		
	}
}


void ical_property_DTEND(struct exchange2ical *exchange2ical)
{
	icalproperty		*prop;
	icalparameter		*tzid;
	struct icaltimetype	icaltime;

	/* Sanity check */
	if (!exchange2ical->apptEndWhole) return;

	/* If this is an all-day appointment */
	if (exchange2ical->apptSubType && (*exchange2ical->apptSubType == 0x1)) {
		icaltime = get_icaldate_from_FILETIME(exchange2ical->apptEndWhole);
		prop = icalproperty_new_dtend(icaltime);
		icalcomponent_add_property(exchange2ical->vevent, prop);
	} else {
		if (exchange2ical->TimeZoneDesc) {
			icaltime = get_icaltime_from_FILETIME(exchange2ical->apptEndWhole);
			prop = icalproperty_new_dtend(icaltime);
			icalcomponent_add_property(exchange2ical->vevent, prop);
			tzid = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
			icalproperty_add_parameter(prop, tzid);
		} else {
			icaltime = get_icaltime_from_FILETIME_UTC(exchange2ical->apptEndWhole);
			prop = icalproperty_new_dtend(icaltime);
			icalcomponent_add_property(exchange2ical->vevent, prop);
		}
		
	}
}


void ical_property_DTSTAMP(struct exchange2ical *exchange2ical)
{
	icalproperty		*prop;
	struct icaltimetype	icaltime;
	struct tm		*tm;
	icalparameter		*tzid;
	time_t			t;

	/* Sanity check */
	/*If OwnerCriticalChange field is null, get time system time*/
	if (!exchange2ical->OwnerCriticalChange) {
		t=time(NULL);
		tm = gmtime(&t);
		icaltime = get_icaltimetype_from_tm_UTC(tm);
		prop = icalproperty_new_dtstamp(icaltime);
		icalcomponent_add_property(exchange2ical->vevent, prop);
		return;
	} else {
	      icaltime = get_icaltime_from_FILETIME_UTC(exchange2ical->OwnerCriticalChange);
	      prop = icalproperty_new_dtstamp(icaltime);
	      icalcomponent_add_property(exchange2ical->vevent, prop);
	      if (exchange2ical->TimeZoneDesc) {
			tzid = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
			icalproperty_add_parameter(prop, tzid);
		}
	}
}


void ical_property_DESCRIPTION(struct exchange2ical *exchange2ical)
{
	icalproperty	*prop;

	if (exchange2ical->method == ICAL_METHOD_REPLY) return;
	if (!exchange2ical->body) return;

	prop = icalproperty_new_description(exchange2ical->body);
	icalcomponent_add_property(exchange2ical->vevent, prop);
}


void ical_property_EXDATE(struct exchange2ical *exchange2ical)
{
	uint32_t	i;
	uint32_t	j;
	uint8_t		modified;
	struct icaltimetype	icaltime;
	struct icaltimetype	dttime;

	icalproperty	*prop;
	struct RecurrencePattern *pat = exchange2ical->RecurrencePattern;

	/* Sanity check */
	if (exchange2ical->Recurring && (*exchange2ical->Recurring == 0x0)) return;
	if (!pat) return;
	if (!pat->DeletedInstanceCount) return;

	for (i = 0; i < pat->DeletedInstanceCount; i++) {
		modified=0;
		if(pat->ModifiedInstanceDates && pat->ModifiedInstanceCount){
			for(j=0; j < pat->ModifiedInstanceCount; j++){
				if(pat->ModifiedInstanceDates[j]==pat->DeletedInstanceDates[i]){
					modified=1;
					break;
				}
			}
		}
		/* If this is an all-day appointment */
		if (!modified && exchange2ical->apptSubType && (*exchange2ical->apptSubType == 0x1)) {
			struct tm	tm;
			tm = get_tm_from_minutes_UTC(pat->DeletedInstanceDates[i]);
			icaltime= get_icaldate_from_tm(&tm);
			prop = icalproperty_new_exdate(icaltime);
			icalcomponent_add_property(exchange2ical->vevent, prop);
		} else if (!modified){
			/* number of minutes between midnight of the specified
			* day and midnight, January 1, 1601 */
			struct tm	tm;
			tm = get_tm_from_minutes_UTC(pat->DeletedInstanceDates[i]);
			icaltime= get_icaltimetype_from_tm(&tm);

			if (exchange2ical->TimeZoneDesc) {
				/*Get time from dtstart*/
				if (exchange2ical->apptEndWhole){
					dttime = get_icaltime_from_FILETIME(exchange2ical->apptStartWhole);
					icaltime.hour   = dttime.hour;
					icaltime.minute = dttime.minute;
				}

				prop = icalproperty_new_exdate(icaltime);
				icalcomponent_add_property(exchange2ical->vevent, prop);
				icalparameter *tzid = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
				icalproperty_add_parameter(prop, tzid);
			} else {
				/*Get time from dtstart*/
				icaltime.is_utc = 1;
				if (exchange2ical->apptEndWhole){
					dttime = get_icaltime_from_FILETIME_UTC(exchange2ical->apptStartWhole);
					icaltime.hour   = dttime.hour;
					icaltime.minute = dttime.minute;
				}
				prop = icalproperty_new_exdate(icaltime);
				icalcomponent_add_property(exchange2ical->vevent, prop);
			}
		}		
		
	}
}


void ical_property_LAST_MODIFIED(struct exchange2ical *exchange2ical)
{
	icalproperty		*prop;
	struct icaltimetype	icaltime;
	icalparameter		*tzid;


	/* Sanity check */
	if (!exchange2ical->LastModified) return;
	if (exchange2ical->TimeZoneDesc) {
		icaltime=get_icaltime_from_FILETIME(exchange2ical->LastModified);
		prop = icalproperty_new_lastmodified(icaltime);
		tzid = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
		icalproperty_add_parameter(prop, tzid);
	} else {
		icaltime=get_icaltime_from_FILETIME_UTC(exchange2ical->LastModified);
		prop = icalproperty_new_lastmodified(icaltime);
	}
	icalcomponent_add_property(exchange2ical->vevent, prop);
}


void ical_property_LOCATION(struct exchange2ical *exchange2ical)
{
	icalproperty *prop;
	/* Sanity check */
	if (!exchange2ical->Location) return;

	prop = icalproperty_new_location(exchange2ical->Location);
	icalcomponent_add_property(exchange2ical->vevent, prop);
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
		RecipientFlags = (uint32_t *) octool_get_propval(&(SRowSet->aRow[i]), PR_RECIPIENT_FLAGS);
		RecipientType = (uint32_t *) octool_get_propval(&(SRowSet->aRow[i]), PR_RECIPIENT_TYPE);

		if (RecipientFlags && !(*RecipientFlags & 0x20) &&
		    ((*RecipientFlags & 0x2) || (RecipientType && !*RecipientType))) {
			icalproperty *prop;
			icalparameter *cn;

			if (smtp) {
				char *mailtoURL;
				mailtoURL = talloc_strdup(exchange2ical->mem_ctx, "mailto:");
				mailtoURL = talloc_strdup_append(mailtoURL, smtp);
				prop = icalproperty_new_organizer(mailtoURL);
				icalcomponent_add_property(exchange2ical->vevent, prop);
				talloc_free(mailtoURL);
			} else {
				prop = icalproperty_new_organizer("invalid:nomail");
				icalcomponent_add_property(exchange2ical->vevent, prop);
			}

			if (display_name) {
				cn = icalparameter_new_cn(display_name);
				icalproperty_add_parameter(prop, cn);
			}
		}
	}
}


void ical_property_PRIORITY(struct exchange2ical *exchange2ical)
{
	icalproperty *prop;
	/* Sanity check */
	if (!exchange2ical->Importance) return;

	switch (*exchange2ical->Importance) {
	case 0x00000000:
		prop = icalproperty_new_priority(9);
		break;
	case 0x00000001:
		prop = icalproperty_new_priority(5);
		break;
	case 0x00000002:
		prop = icalproperty_new_priority(1);
		break;
	default:
		prop = icalproperty_new_priority(5);
	}
	icalcomponent_add_property(exchange2ical->vevent, prop);
}


void ical_property_RRULE_Daily(struct exchange2ical *exchange2ical)
{
	struct icalrecurrencetype recurrence;
	icalproperty *prop;
	struct RecurrencePattern *pat = exchange2ical->RecurrencePattern;


	icalrecurrencetype_clear(&recurrence);
	recurrence.freq = ICAL_DAILY_RECURRENCE;
	recurrence.interval = (pat->Period / 1440);

	if (pat->EndType == END_AFTER_N_OCCURRENCES || pat->EndType == END_AFTER_DATE) {
		recurrence.count = pat->OccurrenceCount;
	}
	prop = icalproperty_new_rrule(recurrence);
	icalcomponent_add_property(exchange2ical->vevent, prop);
}


void ical_property_RRULE_Weekly(struct exchange2ical *exchange2ical)
{
	struct icalrecurrencetype recurrence;
	icalproperty *prop;
	struct RecurrencePattern *pat = exchange2ical->RecurrencePattern;
	uint32_t rdfDaysBitmask = pat->PatternTypeSpecific.WeekRecurrencePattern;
	short idx = 0;

	icalrecurrencetype_clear(&recurrence);
	recurrence.freq = ICAL_WEEKLY_RECURRENCE;
	recurrence.interval = pat->Period;
	
	if(pat->Period > 1){
		switch(pat->FirstDOW){
			case FirstDOW_Sunday:
				recurrence.week_start=ICAL_SUNDAY_WEEKDAY;
				break;
			case FirstDOW_Monday:
				recurrence.week_start=ICAL_MONDAY_WEEKDAY;
				break;
			case FirstDOW_Tuesday:
				recurrence.week_start=ICAL_TUESDAY_WEEKDAY;
				break;
			case FirstDOW_Wednesday:
				recurrence.week_start=ICAL_WEDNESDAY_WEEKDAY;
				break;
			case FirstDOW_Thursday:
				recurrence.week_start=ICAL_THURSDAY_WEEKDAY;
				break;
			case FirstDOW_Friday:
				recurrence.week_start=ICAL_FRIDAY_WEEKDAY;
				break;
			case FirstDOW_Saturday:
				recurrence.week_start=ICAL_SATURDAY_WEEKDAY;
				break;
		}
	}
	
	if (rdfDaysBitmask & Su) {
		recurrence.by_day[idx] = ICAL_SUNDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & M) {
		recurrence.by_day[idx] = ICAL_MONDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & Tu) {
		recurrence.by_day[idx] = ICAL_TUESDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & W) {
		recurrence.by_day[idx] = ICAL_WEDNESDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & Th) {
		recurrence.by_day[idx] = ICAL_THURSDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & F) {
		recurrence.by_day[idx] = ICAL_FRIDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & Sa) {
		recurrence.by_day[idx] = ICAL_FRIDAY_WEEKDAY;
		++idx;
	}
	recurrence.by_day[idx] = ICAL_RECURRENCE_ARRAY_MAX;

	if (pat->EndType == END_AFTER_N_OCCURRENCES || pat->EndType == END_AFTER_DATE) {
		recurrence.count = pat->OccurrenceCount;
	}

	prop = icalproperty_new_rrule(recurrence);
	icalcomponent_add_property(exchange2ical->vevent, prop);
}


void ical_property_RRULE_Monthly(struct exchange2ical *exchange2ical)
{
	struct icalrecurrencetype recurrence;
	icalproperty *prop;
	struct RecurrencePattern *pat = exchange2ical->RecurrencePattern;
	uint32_t day = pat->PatternTypeSpecific.Day;

	icalrecurrencetype_clear(&recurrence);
	recurrence.freq = ICAL_MONTHLY_RECURRENCE;
	recurrence.interval = pat->Period;

	if (day == 0x0000001F) {
		recurrence.by_month_day[0] = -1;
	} else {
		recurrence.by_month_day[0] = day;
	}
	recurrence.by_month_day[1] = ICAL_RECURRENCE_ARRAY_MAX;

	if (pat->EndType == END_AFTER_N_OCCURRENCES || pat->EndType == END_AFTER_DATE) {
		recurrence.count = pat->OccurrenceCount;
	}

	prop = icalproperty_new_rrule(recurrence);
	icalcomponent_add_property(exchange2ical->vevent, prop);
}


void ical_property_RRULE_NthMonthly(struct exchange2ical *exchange2ical)
{
	struct icalrecurrencetype recurrence;
	icalproperty *prop;
	struct RecurrencePattern *pat = exchange2ical->RecurrencePattern;
	uint32_t rdfDaysBitmask = pat->PatternTypeSpecific.MonthRecurrencePattern.WeekRecurrencePattern;
	short idx = 0;
	enum RecurrenceN setpos = pat->PatternTypeSpecific.MonthRecurrencePattern.N;

	icalrecurrencetype_clear(&recurrence);
	recurrence.freq = ICAL_MONTHLY_RECURRENCE;
	recurrence.interval = pat->Period;

	if (rdfDaysBitmask & Su) {
		recurrence.by_day[idx] = ICAL_SUNDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & M) {
		recurrence.by_day[idx] = ICAL_MONDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & Tu) {
		recurrence.by_day[idx] = ICAL_TUESDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & W) {
		recurrence.by_day[idx] = ICAL_WEDNESDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & Th) {
		recurrence.by_day[idx] = ICAL_THURSDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & F) {
		recurrence.by_day[idx] = ICAL_FRIDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & Sa) {
		recurrence.by_day[idx] = ICAL_FRIDAY_WEEKDAY;
		++idx;
	}
	recurrence.by_day[idx] = ICAL_RECURRENCE_ARRAY_MAX;

	if (setpos == RecurrenceN_First) {
		recurrence.by_set_pos[0] = 1;
		recurrence.by_set_pos[1] = ICAL_RECURRENCE_ARRAY_MAX;
	} else if (setpos == RecurrenceN_Second) {
		recurrence.by_set_pos[0] = 2;
		recurrence.by_set_pos[1] = ICAL_RECURRENCE_ARRAY_MAX;
	} else if (setpos == RecurrenceN_Third) {
		recurrence.by_set_pos[0] = 3;
		recurrence.by_set_pos[1] = ICAL_RECURRENCE_ARRAY_MAX;
	} else if (setpos == RecurrenceN_Fourth) {
		recurrence.by_set_pos[0] = 4;
		recurrence.by_set_pos[1] = ICAL_RECURRENCE_ARRAY_MAX;
	} else if (setpos == RecurrenceN_Last) {
		recurrence.by_set_pos[0] = -1;
		recurrence.by_set_pos[1] = ICAL_RECURRENCE_ARRAY_MAX;
	}

	if (pat->EndType == END_AFTER_N_OCCURRENCES || pat->EndType == END_AFTER_DATE) {
		recurrence.count = pat->OccurrenceCount;
	}

	prop = icalproperty_new_rrule(recurrence);
	icalcomponent_add_property(exchange2ical->vevent, prop);
}


void ical_property_RRULE_Yearly(struct exchange2ical *exchange2ical)
{
	struct icalrecurrencetype recurrence;
	icalproperty *prop;
	struct RecurrencePattern *pat = exchange2ical->RecurrencePattern;
	uint32_t day = pat->PatternTypeSpecific.Day;
	struct icaltimetype icaltime;

	icalrecurrencetype_clear(&recurrence);
	recurrence.freq = ICAL_YEARLY_RECURRENCE;
	recurrence.interval = (pat->Period / 12);

	if (day == 0x0000001F) {
		recurrence.by_month_day[0] = -1;
	} else {
		recurrence.by_month_day[0] = day;
	}
	recurrence.by_month_day[1] = ICAL_RECURRENCE_ARRAY_MAX;

	icaltime = get_icaltime_from_FILETIME_UTC(exchange2ical->apptStartWhole);
	recurrence.by_month[0] = icaltime.month;
	recurrence.by_month[1] = ICAL_RECURRENCE_ARRAY_MAX;


	
	if (pat->EndType == END_AFTER_N_OCCURRENCES || pat->EndType == END_AFTER_DATE) {
		recurrence.count = pat->OccurrenceCount;
	} 
	
	prop = icalproperty_new_rrule(recurrence);
	icalcomponent_add_property(exchange2ical->vevent, prop);
}

void ical_property_RRULE_NthYearly(struct exchange2ical *exchange2ical)
{
	struct icalrecurrencetype recurrence;
	icalproperty *prop;
	struct RecurrencePattern *pat = exchange2ical->RecurrencePattern;
	uint32_t rdfDaysBitmask = pat->PatternTypeSpecific.MonthRecurrencePattern.WeekRecurrencePattern;
	short idx = 0;
	enum RecurrenceN setpos = pat->PatternTypeSpecific.MonthRecurrencePattern.N;

	struct icaltimetype icaltime;

	icalrecurrencetype_clear(&recurrence);
	recurrence.freq = ICAL_YEARLY_RECURRENCE;
	recurrence.interval = (pat->Period / 12);

	if (rdfDaysBitmask & Su) {
		recurrence.by_day[idx] = ICAL_SUNDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & M) {
		recurrence.by_day[idx] = ICAL_MONDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & Tu) {
		recurrence.by_day[idx] = ICAL_TUESDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & W) {
		recurrence.by_day[idx] = ICAL_WEDNESDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & Th) {
		recurrence.by_day[idx] = ICAL_THURSDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & F) {
		recurrence.by_day[idx] = ICAL_FRIDAY_WEEKDAY;
		++idx;
	}
	if (rdfDaysBitmask & Sa) {
		recurrence.by_day[idx] = ICAL_FRIDAY_WEEKDAY;
		++idx;
	}
	recurrence.by_day[idx] = ICAL_RECURRENCE_ARRAY_MAX;

	if (setpos == RecurrenceN_First) {
		recurrence.by_set_pos[0] = 1;
		recurrence.by_set_pos[1] = ICAL_RECURRENCE_ARRAY_MAX;
	} else if (setpos == RecurrenceN_Second) {
		recurrence.by_set_pos[0] = 2;
		recurrence.by_set_pos[1] = ICAL_RECURRENCE_ARRAY_MAX;
	} else if (setpos == RecurrenceN_Third) {
		recurrence.by_set_pos[0] = 3;
		recurrence.by_set_pos[1] = ICAL_RECURRENCE_ARRAY_MAX;
	} else if (setpos == RecurrenceN_Fourth) {
		recurrence.by_set_pos[0] = 4;
		recurrence.by_set_pos[1] = ICAL_RECURRENCE_ARRAY_MAX;
	} else if (setpos == RecurrenceN_Last) {
		recurrence.by_set_pos[0] = -1;
		recurrence.by_set_pos[1] = ICAL_RECURRENCE_ARRAY_MAX;
	}

	icaltime = get_icaltime_from_FILETIME(exchange2ical->apptStartWhole);
	recurrence.by_month[0] = icaltime.month;
	recurrence.by_month[1] = ICAL_RECURRENCE_ARRAY_MAX;
	
	if (pat->EndType == END_AFTER_N_OCCURRENCES || pat->EndType == END_AFTER_DATE) {
		recurrence.count = pat->OccurrenceCount;
	}

	prop = icalproperty_new_rrule(recurrence);
	icalcomponent_add_property(exchange2ical->vevent, prop);
}


void ical_property_RRULE(struct exchange2ical *exchange2ical)
{
	struct RecurrencePattern *pat;

	/* Sanity check */
	if (!(exchange2ical->RecurrencePattern)) return;

	pat = exchange2ical->RecurrencePattern;

	switch(pat->PatternType) {
	case PatternType_Day:
		ical_property_RRULE_Daily(exchange2ical);
		break;
	case PatternType_Week:
		ical_property_RRULE_Weekly(exchange2ical);
		break;
	case PatternType_Month:
		if ((pat->Period % 12 ) == 0) {
			ical_property_RRULE_Yearly(exchange2ical);
		} else {
			ical_property_RRULE_Monthly(exchange2ical);
		}
		break;
	case PatternType_MonthNth:
		if ((pat->Period % 12 ) == 0) {
			ical_property_RRULE_NthYearly(exchange2ical);
		} else {
			ical_property_RRULE_NthMonthly(exchange2ical);
		}
		break;
	default:
		printf("RRULE pattern type not implemented yet!:0x%x\n", pat->PatternType);
	}
}

void ical_property_RRULE_daylight_standard(icalcomponent* component , struct SYSTEMTIME st)
{
	struct icalrecurrencetype recurrence;
	icalproperty *prop;
	
	icalrecurrencetype_clear(&recurrence);
	recurrence.freq = ICAL_YEARLY_RECURRENCE;

	if(st.wYear ==0x0000){
		recurrence.by_month[0]=st.wMonth;
		/* Microsoft day of week = libIcal day of week +1; */
		/* Day encode = day + occurrence*8 */
		if (st.wDay==5){
			/* Last occurrence of day in the month*/
			recurrence.by_day[0] = -1 * (st.wDayOfWeek + 9);
		}else{
			/* st.wDay occurrence of day in the month */
			recurrence.by_day[0] = (st.wDayOfWeek + 1 + st.wDay*8);
		}
		
	}else{
		recurrence.by_month_day[0]=st.wDay;
		recurrence.by_month[0]=st.wMonth;
	}

	
	prop = icalproperty_new_rrule(recurrence);
	icalcomponent_add_property(component, prop);
}


void ical_property_RECURRENCE_ID(struct exchange2ical *exchange2ical)
{	
	struct icaltimetype	icaltime;
	icalproperty 		*prop;
	icalparameter		*tzid;
	struct GlobalObjectId	*GlbObjId;

	
	if (exchange2ical->ExceptionReplaceTime){
		/*if parent has an all day event*/
 		if (exchange2ical->apptSubType && (*exchange2ical->apptSubType == 0x1)) {
			icaltime = get_icaldate_from_FILETIME(exchange2ical->ExceptionReplaceTime);
			prop = icalproperty_new_recurrenceid(icaltime);
			icalcomponent_add_property(exchange2ical->vevent, prop);
		} else {
			if (exchange2ical->TimeZoneDesc) {
				icaltime=get_icaltime_from_FILETIME(exchange2ical->ExceptionReplaceTime);
				prop = icalproperty_new_recurrenceid(icaltime);
				icalcomponent_add_property(exchange2ical->vevent, prop);
				tzid = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
				icalproperty_add_parameter(prop, tzid);
			} else {
				icaltime = get_icaltime_from_FILETIME_UTC(exchange2ical->ExceptionReplaceTime);
				prop = icalproperty_new_recurrenceid(icaltime);
				icalcomponent_add_property(exchange2ical->vevent, prop);
			}
		}
	} else if (exchange2ical->GlobalObjectId){
		GlbObjId = get_GlobalObjectId(exchange2ical->mem_ctx, exchange2ical->GlobalObjectId);
		if(GlbObjId){
			icaltime=get_icaldate_from_GlobalObjectId(GlbObjId);
			prop = icalproperty_new_recurrenceid(icaltime);
			icalcomponent_add_property(exchange2ical->vevent, prop);
			talloc_free(GlbObjId);

		}
	}
	
}


void ical_property_RESOURCES(struct exchange2ical *exchange2ical)
{
	char		*NonSendableBcc = NULL;
	icalproperty 	*prop;

	/* Sanity check */
	if (!exchange2ical->NonSendableBcc) return;
	
	NonSendableBcc = talloc_strdup(exchange2ical->mem_ctx, exchange2ical->NonSendableBcc);
	all_string_sub(NonSendableBcc, ";", ",", 0);
	prop = icalproperty_new_resources(NonSendableBcc);
	icalcomponent_add_property(exchange2ical->vevent, prop);
	talloc_free(NonSendableBcc);
}


void ical_property_SEQUENCE(struct exchange2ical *exchange2ical)
{
	icalproperty *prop;
	if (!exchange2ical->Sequence) {
		prop = icalproperty_new_sequence(0);
	} else {
		prop = icalproperty_new_sequence(*(exchange2ical->Sequence));
	}
	icalcomponent_add_property(exchange2ical->vevent, prop);
}


void ical_property_SUMMARY(struct exchange2ical *exchange2ical)
{
	icalproperty *prop;
	icalparameter *language;

	if (exchange2ical->Subject) {
		prop = icalproperty_new_summary(exchange2ical->Subject);
	} else {
		prop = icalproperty_new_summary("");
	}

	if (exchange2ical->MessageLocaleId) {
		const char *langtag = mapi_get_locale_from_lcid( *(exchange2ical->MessageLocaleId) ); 
		language = icalparameter_new_language( langtag );
		icalproperty_add_parameter(prop, language);
	}

	icalcomponent_add_property(exchange2ical->vevent, prop);
}


void ical_property_TRANSP(struct exchange2ical *exchange2ical)
{
	icalproperty *prop;

	/* Sanity check */
	if (!exchange2ical->BusyStatus) return;

	switch (*exchange2ical->BusyStatus) {
	case 0x00000000:
		prop = icalproperty_new_transp(ICAL_TRANSP_TRANSPARENT);
		break;
	case 0x00000001:
	case 0x00000002:
	case 0x00000003:
		prop = icalproperty_new_transp(ICAL_TRANSP_OPAQUE);
		break;
	default:
		prop = icalproperty_new_transp(ICAL_TRANSP_NONE);
	}
	icalcomponent_add_property(exchange2ical->vevent, prop);
}


void ical_property_TRIGGER(struct exchange2ical *exchange2ical)
{
	struct icaltriggertype duration;
	icalproperty *prop;
	if (!exchange2ical->ReminderDelta) return;
	if (*exchange2ical->ReminderDelta == 0x5AE980E1) {
		duration = icaltriggertype_from_int(-15 * 60);
		prop = icalproperty_new_trigger(duration);
		icalcomponent_add_property(exchange2ical->valarm, prop);
	} else {
		duration = icaltriggertype_from_int(*(exchange2ical->ReminderDelta) * -1 * 60);
		prop = icalproperty_new_trigger(duration);
		icalcomponent_add_property(exchange2ical->valarm, prop);
	}
}


#define	GLOBAL_OBJECT_ID_DATA_START	"\x76\x43\x61\x6c\x2d\x55\x69\x64\x01\x00\x00\x00"

void ical_property_UID(struct exchange2ical *exchange2ical)
{
	uint32_t		i;
	const char		*uid;
	char*			outstr;
	struct GlobalObjectId	*GlbObjId;
	icalproperty		*prop;

	outstr = talloc_strdup(exchange2ical->mem_ctx, "");
	
	if(exchange2ical->GlobalObjectId){
		GlbObjId = get_GlobalObjectId(exchange2ical->mem_ctx, exchange2ical->GlobalObjectId);
	}
	
      
	if (exchange2ical->GlobalObjectId && (exchange2ical->GlobalObjectId->cb >= 36) && GlbObjId) {
		if (GlbObjId->Size >= 16 && (0 == memcmp(GlbObjId->Data, GLOBAL_OBJECT_ID_DATA_START, 12))) {
			fflush(0);
			for (i = 12; i < exchange2ical->GlobalObjectId->cb; i++) {
				char objID[6];
				snprintf(objID, 6, "%.2X", exchange2ical->GlobalObjectId->lpb[i]);
				outstr = talloc_strdup_append(outstr, objID);
			}

			uid = (const char *)&(GlbObjId->Data[13]);
			outstr = talloc_strdup_append(outstr, uid);
		} else {
			fflush(0);
			for (i = 0; i < 16; i++) {
				char objID[6];
				snprintf(objID, 6, "%.2X", exchange2ical->GlobalObjectId->lpb[i]);
				outstr = talloc_strdup_append(outstr, objID);
			}
			/* YH, YL, Month and D must be set to 0 */
			outstr = talloc_strdup_append(outstr, "00000000");

			for (i = 20; i < exchange2ical->GlobalObjectId->cb; i++) {
				char objID[6];
				snprintf(objID, 6, "%.2X", exchange2ical->GlobalObjectId->lpb[i]);
				outstr = talloc_strdup_append(outstr, objID);
			}
		}
		talloc_free(GlbObjId);
	} else {
		char objID[32];
		snprintf(objID, 32, "%i", exchange2ical->idx);
		outstr = talloc_strdup(outstr, objID);
	}
	
	prop = icalproperty_new_uid(outstr);
	icalcomponent_add_property(exchange2ical->vevent, prop);
	talloc_free(outstr);
}


static icalproperty * ical_property_add_x_property_value(icalcomponent *parent, const char *propname, const char *value)
{
	icalproperty *prop;
	icalvalue *icalText;

	/* Sanity checks */
	if (!parent) return NULL;
	if (!propname) return NULL;
	if (!value) return NULL;

	icalText = icalvalue_new_text(value);
	prop = icalproperty_new_x(icalvalue_as_ical_string(icalText));
	icalvalue_free(icalText);
	icalproperty_set_x_name(prop, propname);
	icalcomponent_add_property(parent, prop);
	return prop;
}

void ical_property_X_ALT_DESC(struct exchange2ical *exchange2ical)
{
	icalproperty *prop;
	icalparameter *param;
	
	/*sanity check */
	if(!exchange2ical->bodyHTML) return;
	prop = ical_property_add_x_property_value(exchange2ical->vevent, "X-ALT-DESC",exchange2ical->bodyHTML);
	param = icalparameter_new_fmttype("text/html");
	icalproperty_add_parameter(prop, param);	
}

void ical_property_X_MICROSOFT_CDO_ATTENDEE_CRITICAL_CHANGE(struct exchange2ical *exchange2ical)
{
	struct tm	*tm;
	char		outstr[200];

	/* Sanity check */
	if (!exchange2ical->AttendeeCriticalChange) return;

	tm = get_tm_from_FILETIME(exchange2ical->AttendeeCriticalChange);
	if (tm) {
		strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-ATTENDEE-CRITICAL-CHANGE", outstr);
	}
}


void ical_property_X_MICROSOFT_CDO_BUSYSTATUS(struct exchange2ical *exchange2ical)
{
	/*sanity check*/
	if(!exchange2ical->BusyStatus) return;
	
	switch (*exchange2ical->BusyStatus) {
	case 0x00000000:
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-BUSYSTATUS", "FREE");
		break;
	case 0x00000001:
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-BUSYSTATUS", "TENTATIVE");
		break;
	case 0x00000002:
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-BUSYSTATUS", "BUSY");
		break;
	case 0x00000003:
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-BUSYSTATUS", "OOF");
		break;
	}
}
void ical_property_X_MICROSOFT_MSNCALENDAR_IMPORTANCE(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->Importance) return;

	switch (*exchange2ical->Importance) {
	case 0x00000000:
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-MSNCALENDAR-IMPORTANCE", "0");
		break;
	case 0x00000001:
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-MSNCALENDAR-IMPORTANCE", "1");
		break;
	case 0x00000002:
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-MSNCALENDAR-IMPORTANCE", "2");
		break;
	}
}

void ical_property_X_MICROSOFT_CDO_INTENDEDSTATUS(struct exchange2ical *exchange2ical)
{
	if (!exchange2ical->IntendedBusyStatus) return;

	switch (*exchange2ical->IntendedBusyStatus) {
	case 0x00000000:
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-INTENDEDSTATUS", "FREE");
		break;
	case 0x00000001:
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-INTENDEDSTATUS", "TENTATIVE");
		break;
	case 0x00000002:
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-INTENDEDSTATUS", "BUSY");
		break;
	case 0x00000003:
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-INTENDEDSTATUS", "OOF");
		break;
	}
}


void ical_property_X_MICROSOFT_CDO_OWNERAPPTID(struct exchange2ical *exchange2ical)
{
	char outstr[200];
	/* Sanity check */
	if (!exchange2ical->OwnerApptId) return;
	snprintf(outstr, 200, "%d", *(exchange2ical->OwnerApptId));
	ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-OWNERAPPTID", outstr);
}


void ical_property_X_MICROSOFT_CDO_OWNER_CRITICAL_CHANGE(struct exchange2ical *exchange2ical)
{
	struct tm	*tm;
	char		outstr[200];
	
	/* Sanity check */
	if (!exchange2ical->OwnerCriticalChange) return;


	tm = get_tm_from_FILETIME(exchange2ical->OwnerCriticalChange);
	if (tm) {
		strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-OWNER-CRITICAL-CHANGE", outstr);
	}
}


void ical_property_X_MICROSOFT_CDO_REPLYTIME(struct exchange2ical *exchange2ical)
{
	struct tm	*tm;
	char		outstr[200];

	/* Sanity check */
	if (!exchange2ical->apptReplyTime) return;

	tm = get_tm_from_FILETIME(exchange2ical->apptReplyTime);
	if (tm) {
		strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-REPLYTIME", outstr);
	}
}


void ical_property_X_MICROSOFT_DISALLOW_COUNTER(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->NotAllowPropose) return;

	if (*(exchange2ical->NotAllowPropose) == true) {
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-DISALLOW-COUNTER", "TRUE");
	} else {
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-DISALLOW-COUNTER", "FALSE");
	}
}


void ical_property_X_MS_OLK_ALLOWEXTERNCHECK(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->AllowExternCheck) return;

	if (*(exchange2ical->AllowExternCheck) == true) {
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-ALLOWEXTERNCHECK", "TRUE");
	} else {
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-ALLOWEXTERNCHECK", "FALSE");
	}
}


void ical_property_X_MS_OLK_APPTLASTSEQUENCE(struct exchange2ical *exchange2ical)
{
	char outstr[20];
	/* Sanity check */
	if (!exchange2ical->apptLastSequence) return;

	snprintf(outstr, 20, "%d", *exchange2ical->apptLastSequence);
	ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-APPTLASTSEQUENCE", outstr);
}


void ical_property_X_MS_OLK_APPTSEQTIME(struct exchange2ical *exchange2ical)
{
	struct tm	*tm = NULL;
	char		outstr[200];

	/* Sanity check */
	if (!exchange2ical->apptSeqTime) return;

	tm = get_tm_from_FILETIME(exchange2ical->apptSeqTime);
	if (tm) {
		strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-APPTSEQTIME", outstr);
	}
}


void ical_property_X_MS_OLK_AUTOFILLLOCATION(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->AutoFillLocation) return;

	if (*(exchange2ical->AutoFillLocation) == true) {
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-AUTOFILLLOCATION", "TRUE");
	} else {
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-AUTOFILLLOCATION", "FALSE");
	}
}


void ical_property_X_MS_OLK_AUTOSTARTCHECK(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->AutoStartCheck) return;
	if (*(exchange2ical->AutoStartCheck) == true) {
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-AUTOSTARTCHECK", "TRUE");
	} else {
		ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-AUTOSTARTCHECK", "FALSE");
	}
}


void ical_property_X_MS_OLK_COLLABORATEDOC(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->CollaborateDoc) return;
	if(!(*exchange2ical->CollaborateDoc)) return;

	ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-COLLABORATEDOC", exchange2ical->CollaborateDoc);
}


void ical_property_X_MS_OLK_CONFCHECK(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->ConfCheck) return;
	
	ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-CONFCHECK", (*exchange2ical->ConfCheck == true) ? "TRUE" : "FALSE");
}


void ical_property_X_MS_OLK_CONFTYPE(struct exchange2ical *exchange2ical)
{
	char outstr[20];
	/* Sanity check */
	if (!exchange2ical->ConfType) return;

	snprintf(outstr, 20, "%d", *exchange2ical->ConfType);
	ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-CONFTYPE", outstr);
}


void ical_property_X_MS_OLK_DIRECTORY(struct exchange2ical *exchange2ical)
{
	/*Sanity Check*/
	if(!exchange2ical->Directory) return;
	if(!(*exchange2ical->Directory)) return;

	ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-DIRECTORY", exchange2ical->Directory);
}


void ical_property_X_MS_OLK_MWSURL(struct exchange2ical *exchange2ical)
{
	/*Sanity Check*/
	if(!exchange2ical->MWSURL) return;

	ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-MWSURL", exchange2ical->MWSURL);
}


void ical_property_X_MS_OLK_NETSHOWURL(struct exchange2ical *exchange2ical)
{
	/*Sanity Check*/
	if(!exchange2ical->NetShowURL) return;
	if(!(*exchange2ical->NetShowURL)) return;

	ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-NETSHOWURL", exchange2ical->NetShowURL);
}

void ical_property_X_MS_OLK_ONLINEPASSWORD(struct exchange2ical *exchange2ical)
{
	/*Sanity Check*/
	if(!exchange2ical->OnlinePassword) return;
	if(!(*exchange2ical->OnlinePassword)) return;

	ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-ONLINEPASSWORD", exchange2ical->OnlinePassword);
}

void ical_property_X_MS_OLK_ORGALIAS(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if(!exchange2ical->OrgAlias) return;
	if(!(*exchange2ical->OrgAlias)) return;

	ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-ORGALIAS", exchange2ical->OrgAlias);
}


// TODO: double check this - need an example.
void ical_property_X_MS_OLK_SENDER(struct exchange2ical *exchange2ical)
{
	icalproperty *prop;
	icalparameter *param;
	char outstr[200];

	/* Sanity check */
	if (!exchange2ical->apptStateFlags) return;
	if (!(*exchange2ical->apptStateFlags & 0x1)) return;
	if (!exchange2ical->SenderName) return;

	if (exchange2ical->SenderEmailAddress) {
		snprintf(outstr, 200, "mailto:%s",exchange2ical->SenderEmailAddress);
		prop = icalproperty_new_x(outstr);
	} else {
		prop = icalproperty_new_x("invalid:nomail");
	}

	icalproperty_set_x_name(prop, "X-MS-OLK-SENDER");
	icalcomponent_add_property(exchange2ical->vevent, prop);

	param = icalparameter_new_cn(exchange2ical->SenderName);
	icalproperty_add_parameter(prop, param);
}
