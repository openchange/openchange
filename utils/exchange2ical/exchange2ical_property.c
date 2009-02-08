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

// TODO: check this - need an example
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
			icalproperty *prop;
			icalparameter *cn;
			icalparameter *participantType;

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

			cn = icalparameter_new_cn(display_name);
			icalproperty_add_parameter(prop, cn);

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

#if 0
			// TODO: fix this
			if (exchange2ical->partstat) {
				icalparameter *partstat;
				partstat = icalparameter_new_partstat(exchange2ical->partstat);
				icalproperty_add_parameter(prop, partstat);
			}
#endif
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
	char*		categoryList;
	icalproperty	*prop;

	/* Sanity check */
	if (!exchange2ical->Keywords) return;
	if (!exchange2ical->Keywords->cValues) return;

	categoryList = talloc_strdup(exchange2ical->mem_ctx, exchange2ical->Keywords->lppszA[0]);
	for (i = 1; i < exchange2ical->Keywords->cValues; ++i) {
		categoryList = talloc_strdup_append(categoryList, ",");
		categoryList = talloc_strdup_append(categoryList, exchange2ical->Keywords->lppszA[i]);
	}
	prop = icalproperty_new_categories(categoryList); 
	icalcomponent_add_property(exchange2ical->vevent, prop);
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

	icaltime = get_icaltime_from_FILETIME(exchange2ical->created);

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
		/* If this is a recurring non all-day event */
		if (exchange2ical->Recurring && (*exchange2ical->Recurring == 0)) {
			// TODO: we should handle recurrence here...
			icaltime = get_icaltime_from_FILETIME(exchange2ical->apptStartWhole);
			prop = icalproperty_new_dtstart(icaltime);
			icalcomponent_add_property(exchange2ical->vevent, prop);
			if (exchange2ical->TimeZoneDesc) {
				tzid = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
				icalproperty_add_parameter(prop, tzid);
			}
		} else {
			/* If this is a non recurring non all-day event */
			icaltime = get_icaltime_from_FILETIME(exchange2ical->apptStartWhole);
			prop = icalproperty_new_dtstart(icaltime);
			icalcomponent_add_property(exchange2ical->vevent, prop);
			if (exchange2ical->TimeZoneDesc) {
				tzid = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
				icalproperty_add_parameter(prop, tzid);
			}
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
		/* If this is a recurring non all-day event */
		if (exchange2ical->Recurring && (*exchange2ical->Recurring == 0)) {
			// TODO: we should handle recurrence here...
			icaltime = get_icaltime_from_FILETIME(exchange2ical->apptEndWhole);
			prop = icalproperty_new_dtend(icaltime);
			icalcomponent_add_property(exchange2ical->vevent, prop);
			if (exchange2ical->TimeZoneDesc) {
				tzid = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
				icalproperty_add_parameter(prop, tzid);
			}
		} else {
			/* If this is a non recurring non all-day event */
			icaltime = get_icaltime_from_FILETIME(exchange2ical->apptEndWhole);
			prop = icalproperty_new_dtend(icaltime);
			icalcomponent_add_property(exchange2ical->vevent, prop);
			if (exchange2ical->TimeZoneDesc) {
				tzid = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
				icalproperty_add_parameter(prop, tzid);
			}
		}
	}
}


void ical_property_DTSTAMP(struct exchange2ical *exchange2ical)
{
	icalproperty		*prop;
	struct icaltimetype	icaltime;

	/* Sanity check */
	if (!exchange2ical->OwnerCriticalChange) return;

	/* TODO: if the property doesn't exist, we should use system
	 * time instead 
	 */

	icaltime = get_icaltime_from_FILETIME(exchange2ical->OwnerCriticalChange);

	prop = icalproperty_new_dtstamp(icaltime);
	icalcomponent_add_property(exchange2ical->vevent, prop);
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
	NTTIME		time;
	struct timeval	t;
	struct tm	*tm;
	icalproperty	*prop;

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

			prop = icalproperty_new_exdate(get_icaltimetype_from_tm(tm));
			icalcomponent_add_property(exchange2ical->vevent, prop);
			if (exchange2ical->TimeZoneDesc) {
				icalparameter *tzid = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
				icalproperty_add_parameter(prop, tzid);
			}
		}
	}
}


void ical_property_LAST_MODIFIED(struct exchange2ical *exchange2ical)
{
	icalproperty		*prop;
	struct icaltimetype	icaltime;

	/* Sanity check */
	if (!exchange2ical->LastModified) return;

	/* TODO: if the property doesn't exist, we should use system
	 * time instead 
	 */

	icaltime = get_icaltime_from_FILETIME(exchange2ical->LastModified);

	prop = icalproperty_new_lastmodified(icaltime);
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
		RecipientFlags = (uint32_t *) octool_get_propval(&(SRowSet->aRow[i]), PR_RECIPIENTS_FLAGS);
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
			cn = icalparameter_new_cn(display_name);
			icalproperty_add_parameter(prop, cn);
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


void ical_property_RDATE(struct exchange2ical *exchange2ical)
{
	uint32_t	i;
	NTTIME		time;
	struct timeval	t;
	struct tm	*tm;
	icalproperty	*prop;

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

			prop = icalproperty_new_rdate(get_icaldatetimeperiodtype_from_tm(tm));
			icalcomponent_add_property(exchange2ical->vevent, prop);
			if (exchange2ical->TimeZoneDesc) {
				icalparameter *tzid = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
				icalproperty_add_parameter(prop, tzid);
			}
		}
	}	
}


#if 0
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
#endif


void ical_property_RRULE_Daily(struct exchange2ical *exchange2ical)
{
	struct icalrecurrencetype recurrence;
	icalproperty *prop;
	struct RecurrencePattern *pat = exchange2ical->RecurrencePattern;

	icalrecurrencetype_clear(&recurrence);
	recurrence.freq = ICAL_DAILY_RECURRENCE;
	recurrence.interval = (pat->Period / 1440);

	// TODO: need answers from Microsoft on what is happening here
	printf("endType:0x%x\n", pat->EndType);
	printf("seeing %i occurences\n", pat->OccurrenceCount);
	if (pat->EndType == END_AFTER_N_OCCURRENCES) {
		printf("seeing %i occurences\n", pat->OccurrenceCount);
		recurrence.count = pat->OccurrenceCount;
	} else if (pat->EndType == END_AFTER_DATE) {
	      // TODO: set recurrence.until = pat->EndDate;
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

	recurrence.count = pat->OccurrenceCount;

	// TODO: need answers from Microsoft on what is happening here
	printf("endType:0x%x\n", pat->EndType);
	printf("seeing %i occurences\n", pat->OccurrenceCount);
	if (pat->EndType == END_AFTER_N_OCCURRENCES) {
		printf("seeing %i occurences\n", pat->OccurrenceCount);
		recurrence.count = pat->OccurrenceCount;
	} else if (pat->EndType == END_AFTER_DATE) {
	      // TODO: set recurrence.until = pat->EndDate;
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

	recurrence.count = pat->OccurrenceCount;

	// TODO: need answers from Microsoft on what is happening here
	printf("endType:0x%x\n", pat->EndType);
	printf("seeing %i occurences\n", pat->OccurrenceCount);
	if (pat->EndType == END_AFTER_N_OCCURRENCES) {
		printf("seeing %i occurences\n", pat->OccurrenceCount);
		recurrence.count = pat->OccurrenceCount;
	} else if (pat->EndType == END_AFTER_DATE) {
	      // TODO: set recurrence.until = pat->EndDate;
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

	recurrence.count = pat->OccurrenceCount;

	// TODO: need answers from Microsoft on what is happening here
	printf("endType:0x%x\n", pat->EndType);
	printf("seeing %i occurences\n", pat->OccurrenceCount);
	if (pat->EndType == END_AFTER_N_OCCURRENCES) {
		printf("seeing %i occurences\n", pat->OccurrenceCount);
		recurrence.count = pat->OccurrenceCount;
	} else if (pat->EndType == END_AFTER_DATE) {
	      // TODO: set recurrence.until = pat->EndDate;
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

	icaltime = get_icaltime_from_FILETIME(exchange2ical->apptStartWhole);
	recurrence.by_month[0] = icaltime.month;
	recurrence.by_month[1] = ICAL_RECURRENCE_ARRAY_MAX;

	recurrence.count = pat->OccurrenceCount;

	// TODO: need answers from Microsoft on what is happening here
	printf("endType:0x%x\n", pat->EndType);
	printf("seeing %i occurences\n", pat->OccurrenceCount);
	if (pat->EndType == END_AFTER_N_OCCURRENCES) {
		printf("seeing %i occurences\n", pat->OccurrenceCount);
		recurrence.count = pat->OccurrenceCount;
	} else if (pat->EndType == END_AFTER_DATE) {
	      // TODO: set recurrence.until = pat->EndDate;
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

	// TODO: need answers from Microsoft on what is happening here
	printf("endType:0x%x\n", pat->EndType);
	printf("seeing %i occurences\n", pat->OccurrenceCount);
	if (pat->EndType == END_AFTER_N_OCCURRENCES) {
		printf("seeing %i occurences\n", pat->OccurrenceCount);
		recurrence.count = pat->OccurrenceCount;
	} else if (pat->EndType == END_AFTER_DATE) {
	      // TODO: set recurrence.until = pat->EndDate;
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


void ical_property_RECURRENCE_ID(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->ExceptionReplaceTime) return;

	/* TODO: I'm too lazy to implement this today */
	DEBUG(0, ("RECURRENCE-ID:\n"));
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

	// TODO: convert exchange2ical->MessageLocaleId to an RFC1766 language tag

	language = icalparameter_new_language("en-au");
	icalproperty_add_parameter(prop, language);

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

	/* Sanity check */
	if (!exchange2ical->GlobalObjectId) {
		// printf("GlobalObjectId not found\n");
		return;
	}
	
	GlbObjId = get_GlobalObjectId(exchange2ical->mem_ctx, exchange2ical->GlobalObjectId);
	if (!GlbObjId) {
		// printf("could not get GlbObjId\n");
		return;
	}

	outstr=talloc_init("uid");
	if (GlbObjId->Size >= 12 && (0 == memcmp(GlbObjId->Data, GLOBAL_OBJECT_ID_DATA_START, 12))) {
		// TODO: could this code overrun GlobalObjectId->lpb?
		// TODO: I think we should start at 12, not at zero...
		for (i = 0; i < 52; i++) {
			char objID[6];
			snprintf(objID, 6, "%.2X", exchange2ical->GlobalObjectId->lpb[i]);
			outstr = talloc_strdup_append(outstr, objID);
		}

		uid = (const char *)&(GlbObjId->Data[13]);
		outstr = talloc_strdup_append(outstr, uid);

	} else {
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
	prop = icalproperty_new_uid(outstr);
	icalcomponent_add_property(exchange2ical->vevent, prop);
	talloc_free(outstr);
	talloc_free(GlbObjId);
}


static void ical_property_add_x_property_value(icalcomponent *parent, const char* propname, const char* value)
{
	icalproperty *prop;

	/* Sanity checks */
	if (!parent) return;
	if (!propname) return;
	if (!value) return;

	prop = icalproperty_new_x(value);
	icalproperty_set_x_name(prop, propname);
	icalcomponent_add_property(parent, prop);
}


void ical_property_X_MICROSOFT_CDO_ATTENDEE_CRITICAL_CHANGE(struct exchange2ical *exchange2ical)
{
	struct tm	*tm;
	char		outstr[200];

	/* Sanity check */
	if (!exchange2ical->AttendeeCriticalChange) return;

	tm = get_tm_from_FILETIME(exchange2ical->AttendeeCriticalChange);
	strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
	ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-ATTENDEE-CRITICAL-CHANGE", outstr);
}


void ical_property_X_MICROSOFT_CDO_BUSYSTATUS(struct exchange2ical *exchange2ical)
{
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

	/* FIXME: if the property doesn't exist, we should use system
	 * time instead 
	 */

	tm = get_tm_from_FILETIME(exchange2ical->OwnerCriticalChange);
	strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
	ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-OWNER-CRITICAL-CHANGE", outstr);
}


void ical_property_X_MICROSOFT_CDO_REPLYTIME(struct exchange2ical *exchange2ical)
{
	struct tm	*tm;
	char		outstr[200];

	/* Sanity check */
	if (!exchange2ical->apptReplyTime) return;

	tm = get_tm_from_FILETIME(exchange2ical->apptReplyTime);
	strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
	ical_property_add_x_property_value(exchange2ical->vevent, "X-MICROSOFT-CDO-REPLYTIME", outstr);
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
	strftime(outstr, sizeof(outstr), "%Y%m%dT%H%M%SZ", tm);
	ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-APPTSEQTIME", outstr);
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
	ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-DIRECTORY", exchange2ical->Directory);
}


void ical_property_X_MS_OLK_MWSURL(struct exchange2ical *exchange2ical)
{
	ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-MWSURL", exchange2ical->MWSURL);
}


void ical_property_X_MS_OLK_NETSHOWURL(struct exchange2ical *exchange2ical)
{
	ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-NETSHOWURL", exchange2ical->NetShowURL);
}

void ical_property_X_MS_OLK_ONLINEPASSWORD(struct exchange2ical *exchange2ical)
{
	ical_property_add_x_property_value(exchange2ical->vevent, "X-MS-OLK-ONLINEPASSWORD", exchange2ical->OnlinePassword);
}

void ical_property_X_MS_OLK_ORGALIAS(struct exchange2ical *exchange2ical)
{
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
