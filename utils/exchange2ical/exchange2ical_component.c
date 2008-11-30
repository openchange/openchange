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

void ical_component_VCALENDAR(struct exchange2ical *exchange2ical)
{
	DEBUG(0, ("BEGIN:VCALENDAR\n"));

	DEBUG(0, ("VERSION: %s\n", ICAL_VERSION));
	DEBUG(0, ("PRODID: %s\n", ICAL_PRODID));
	DEBUG(0, ("METHOD: %s\n", exchange2ical->method));

	if (exchange2ical->partstat) {
		DEBUG(0, ("PARTSTAT: %s\n", exchange2ical->partstat));
	}

	/* X-MICROSOFT-CALSCALE */
	if (exchange2ical->RecurrencePattern && exchange2ical->RecurrencePattern->CalendarType) {
		DEBUG(0, ("X-MICROSOFT-CALSCALE: %s\n", get_ical_calendartype(exchange2ical->RecurrencePattern->CalendarType)));
	}
	
	ical_component_VTIMEZONE(exchange2ical);
	ical_component_VEVENT(exchange2ical);

	DEBUG(0, ("END:VCALENDAR\n"));
}


void ical_component_VEVENT(struct exchange2ical *exchange2ical)
{
	DEBUG(0, ("BEGIN:VEVENT\n"));

	/* ATTACH property FIXME */
	ical_property_ATTENDEE(exchange2ical);
	ical_property_CATEGORIES(exchange2ical);
	ical_property_CLASS(exchange2ical);
	ical_property_CONTACT(exchange2ical);
	ical_property_CREATED(exchange2ical);
	ical_property_DTSTART(exchange2ical);
	ical_property_DTEND(exchange2ical);
	ical_property_DTSTAMP(exchange2ical);
	ical_property_DESCRIPTION(exchange2ical);
	ical_property_EXDATE(exchange2ical);
	ical_property_LAST_MODIFIED(exchange2ical);
	ical_property_LOCATION(exchange2ical);
	ical_property_ORGANIZER(exchange2ical);
	ical_property_PRIORITY(exchange2ical);
	ical_property_RDATE(exchange2ical);
	/* RECURRENCE-ID property:FIXME */
	ical_property_RECURRENCE_ID(exchange2ical);
	ical_property_RESOURCES(exchange2ical);
	ical_property_SEQUENCE(exchange2ical);
	ical_property_SUMMARY(exchange2ical);
	ical_property_TRANSP(exchange2ical);
	ical_property_UID(exchange2ical);

	/* X-ALT-DESC FIXME */
	ical_property_X_MICROSOFT_CDO_ATTENDEE_CRITICAL_CHANGE(exchange2ical);
	ical_property_X_MICROSOFT_CDO_BUSYSTATUS(exchange2ical);
	ical_property_X_MICROSOFT_CDO_INTENDEDSTATUS(exchange2ical);
	ical_property_X_MICROSOFT_CDO_OWNERAPPTID(exchange2ical);
	ical_property_X_MICROSOFT_CDO_OWNER_CRITICAL_CHANGE(exchange2ical);
	ical_property_X_MICROSOFT_CDO_REPLYTIME(exchange2ical);
	ical_property_X_MICROSOFT_DISALLOW_COUNTER(exchange2ical);
	ical_property_X_MS_OLK_ALLOWEXTERNCHECK(exchange2ical);
	ical_property_X_MS_OLK_APPTLASTSEQUENCE(exchange2ical);
	ical_property_X_MS_OLK_APPTSEQTIME(exchange2ical);
	ical_property_X_MS_OLK_AUTOFILLLOCATION(exchange2ical);
	ical_property_X_MS_OLK_AUTOSTARTCHECK(exchange2ical);
	ical_property_X_MS_OLK_COLLABORATEDOC(exchange2ical);
	ical_property_X_MS_OLK_CONFCHECK(exchange2ical);
	ical_property_X_MS_OLK_CONFTYPE(exchange2ical);
	ical_property_X_MS_OLK_DIRECTORY(exchange2ical);
	ical_property_X_MS_OLK_MWSURL(exchange2ical);
	ical_property_X_MS_OLK_NETSHOWURL(exchange2ical);
	ical_property_X_MS_OLK_ONLINEPASSWORD(exchange2ical);
	ical_property_X_MS_OLK_ORGALIAS(exchange2ical);
	ical_property_X_MS_OLK_SENDER(exchange2ical);

	ical_component_VALARM(exchange2ical);


	DEBUG(0, ("END:VEVENT\n"));
}


/**
   VTIMEZONE component
 */
void ical_component_VTIMEZONE(struct exchange2ical *exchange2ical)
{
	DEBUG(0, ("BEGIN:VTIMEZONE\n"));

	/* TZID property */
	DEBUG(0, ("TZID: %s\n", exchange2ical->TimeZoneDesc));

	/* STANDARD sub-component */
	ical_component_STANDARD(exchange2ical);
	
	/* DAYLIGH component */
	ical_component_DAYLIGHT(exchange2ical);

	DEBUG(0, ("END:VTIMEZONE\n"));
}


/**
   STANDARD sub-component
 */
void ical_component_STANDARD(struct exchange2ical *exchange2ical)
{
	char		*dtstart = NULL;
	int32_t		tzoffsetfrom;
	int32_t		tzoffsetto;

	/* Sanity Check */
	if (!exchange2ical->TimeZoneStruct) return;

	DEBUG(0, ("BEGIN:STANDARD\n"));

	/* DTSTART property */
	dtstart = get_ical_date(exchange2ical->mem_ctx, &(exchange2ical->TimeZoneStruct->stStandardDate));
	if (dtstart) {
		DEBUG(0, ("DTSTART: %s\n", dtstart));
		talloc_free(dtstart);
	}

	/* RRULE property */
	ical_property_RRULE(exchange2ical, exchange2ical->TimeZoneStruct->stStandardDate);

	/* TZOFFSETFROM property */
	tzoffsetfrom = -1 * (exchange2ical->TimeZoneStruct->lBias + exchange2ical->TimeZoneStruct->lDaylightBias);
	DEBUG(0, ("TZOFFSETFROM: %s%.4d\n", (tzoffsetfrom > 0) ? "+" : "", tzoffsetfrom));

	/* TZOFFSETTO property */
	tzoffsetto = -1 * (exchange2ical->TimeZoneStruct->lBias + exchange2ical->TimeZoneStruct->lStandardBias);
	DEBUG(0, ("TZOFFSETTO: %s%.4d\n", (tzoffsetto > 0) ? "+" : "", tzoffsetto));

	DEBUG(0, ("END:STANDARD\n"));
}


/**
   DAYLIGHT sub-component
 */
void ical_component_DAYLIGHT(struct exchange2ical *exchange2ical)
{
	char		*dtstart = NULL;
	int32_t		tzoffsetfrom;
	int32_t		tzoffsetto;

	/* Sanity check */
	if (has_component_DAYLIGHT(exchange2ical) == false) return;

	DEBUG(0, ("BEGIN:DAYLIGHT\n"));

	/* DTSTART property */
	dtstart = get_ical_date(exchange2ical->mem_ctx, &exchange2ical->TimeZoneStruct->stDaylightDate);
	DEBUG(0, ("DTSTART: %s\n", dtstart));
	talloc_free(dtstart);

	/* RRULE property */
	ical_property_RRULE(exchange2ical, exchange2ical->TimeZoneStruct->stDaylightDate);
	
	/* TZOFFSETFROM property */
	tzoffsetfrom = -1 * (exchange2ical->TimeZoneStruct->lBias + exchange2ical->TimeZoneStruct->lStandardBias);
	DEBUG(0, ("TZOFFSETFROM: %s%.4d\n", (tzoffsetfrom > 0) ? "+" : "", tzoffsetfrom));

	/* TZOFFSETTO property */
	tzoffsetto = -1 * (exchange2ical->TimeZoneStruct->lBias + exchange2ical->TimeZoneStruct->lDaylightBias);
	DEBUG(0, ("TZOFFSETTO: %s%.4d\n", (tzoffsetto > 0) ? "+" : "", tzoffsetto));

	DEBUG(0, ("END:DAYLIGHT\n"));
}


/**
   VALARM component
 */
void ical_component_VALARM(struct exchange2ical *exchange2ical)
{
	/* Sanity check */
	if (!exchange2ical->ReminderSet) return;
	if (*exchange2ical->ReminderSet == false) return;

	DEBUG(0, ("BEGIN:VALARM\n"));

	ical_property_TRIGGER(exchange2ical);
	DEBUG(0, ("ACTION:DISPLAY\n"));
	DEBUG(0, ("DESCRIPTION:Reminder\n"));

	DEBUG(0, ("END:VALARM\n"));
}
