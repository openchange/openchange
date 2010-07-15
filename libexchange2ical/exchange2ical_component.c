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
#include <libical/icalderivedproperty.h>

/*
   VCALENDAR component
 */
void ical_component_VCALENDAR(struct exchange2ical *exchange2ical)
{
	icalproperty* prop;
	
	exchange2ical->vcalendar = icalcomponent_new_vcalendar();
	if (!(exchange2ical->vcalendar)) {
		return;
	}

	prop = icalproperty_new_version(OPENCHANGE_ICAL_VERSION);
	icalcomponent_add_property(exchange2ical->vcalendar, prop);

	prop = icalproperty_new_prodid(OPENCHANGE_ICAL_PRODID);
	icalcomponent_add_property(exchange2ical->vcalendar, prop);

	prop = icalproperty_new_method(exchange2ical->method);
	icalcomponent_add_property(exchange2ical->vcalendar, prop);


	if (exchange2ical->RecurrencePattern && exchange2ical->RecurrencePattern->CalendarType) {
		prop = icalproperty_new_x(get_ical_calendartype(exchange2ical->RecurrencePattern->CalendarType));
		icalproperty_set_x_name(prop, "X-MICROSOFT-CALSCALE");
		icalcomponent_add_property(exchange2ical->vcalendar, prop);
	}

	ical_component_VTIMEZONE(exchange2ical);
}

/*
   VEVENT component
 */
void ical_component_VEVENT(struct exchange2ical *exchange2ical)
{
	exchange2ical->vevent = icalcomponent_new_vevent();
	if ( ! (exchange2ical->vevent) ) {
		return;
	}
	
	icalcomponent_add_component(exchange2ical->vcalendar, exchange2ical->vevent);
	ical_property_ATTACH(exchange2ical);
	ical_property_ATTENDEE(exchange2ical);
	ical_property_CATEGORIES(exchange2ical);
	ical_property_CLASS(exchange2ical);
	ical_property_CONTACT(exchange2ical);
	ical_property_CREATED(exchange2ical);
	ical_property_DESCRIPTION(exchange2ical);
	ical_property_DTEND(exchange2ical);
	ical_property_DTSTAMP(exchange2ical);
	ical_property_DTSTART(exchange2ical);
	ical_property_RECURRENCE_ID(exchange2ical);
	ical_property_EXDATE(exchange2ical);
	ical_property_LAST_MODIFIED(exchange2ical);
	ical_property_LOCATION(exchange2ical);
	ical_property_ORGANIZER(exchange2ical);
	ical_property_PRIORITY(exchange2ical);
	/*All possible RDATE properties are now exported as separate vevents.
	No longer a need for it*/
	//ical_property_RDATE(exchange2ical);
	ical_property_RRULE(exchange2ical);
	ical_property_RESOURCES(exchange2ical);
	ical_property_SEQUENCE(exchange2ical);
	ical_property_SUMMARY(exchange2ical);
	ical_property_TRANSP(exchange2ical);
	ical_property_UID(exchange2ical);
	ical_property_X_ALT_DESC(exchange2ical);
	ical_property_X_MICROSOFT_CDO_ATTENDEE_CRITICAL_CHANGE(exchange2ical);
	ical_property_X_MICROSOFT_CDO_BUSYSTATUS(exchange2ical);
	/* Looks like this is no longer supposed to be used, and is ignored by most Outlook versions */
	/* ical_property_X_MICROSOFT_MSNCALENDAR_IMPORTANCE(exchange2ical); */
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
}

/*
   VTIMEZONE component
 */
void ical_component_VTIMEZONE(struct exchange2ical *exchange2ical)
{
	exchange2ical->vtimezone = icalcomponent_new_vtimezone();

	/* TZID property */
	if (exchange2ical->TimeZoneDesc) {
		icalproperty *tzid = icalproperty_new_tzid(exchange2ical->TimeZoneDesc);
		icalcomponent_add_property(exchange2ical->vtimezone, tzid);
	}
	/* STANDARD sub-component */
	if (exchange2ical->TimeZoneStruct) {
		icalcomponent_add_component(exchange2ical->vcalendar, exchange2ical->vtimezone);
		ical_component_STANDARD(exchange2ical);

		if (has_component_DAYLIGHT(exchange2ical)) {
			ical_component_DAYLIGHT(exchange2ical);
		}
	} else {
		icalcomponent_free(exchange2ical->vtimezone);
	}
}

/*
   STANDARD sub-component
 */
void ical_component_STANDARD(struct exchange2ical *exchange2ical)
{
	char		*dtstart = NULL;
	int32_t		tzoffsetfrom;
	int32_t		tzoffsetto;
	icalcomponent	*standard;
	icalproperty	*prop;

	standard = icalcomponent_new_xstandard();
	icalcomponent_add_component(exchange2ical->vtimezone, standard);

	/* DTSTART property */
	dtstart = get_ical_date(exchange2ical->mem_ctx, &(exchange2ical->TimeZoneStruct->stStandardDate));
	if (dtstart) {
		prop = icalproperty_new_dtstart(icaltime_from_string(dtstart));
		icalcomponent_add_property(standard, prop);
		talloc_free(dtstart);
	}

	/* RRULE Property */
	if (has_component_DAYLIGHT(exchange2ical)){
		ical_property_RRULE_daylight_standard(standard,exchange2ical->TimeZoneStruct->stStandardDate);
	}
	
	/* TZOFFSETFROM property */
	tzoffsetfrom = (-60 * (exchange2ical->TimeZoneStruct->lBias + exchange2ical->TimeZoneStruct->lDaylightBias));
	prop = icalproperty_new_tzoffsetfrom(tzoffsetfrom);
	icalcomponent_add_property(standard, prop);

	/* TZOFFSETTO property */
	tzoffsetto = (-60 * (exchange2ical->TimeZoneStruct->lBias + exchange2ical->TimeZoneStruct->lStandardBias));
	prop = icalproperty_new_tzoffsetto(tzoffsetto);
	icalcomponent_add_property(standard, prop);
}


/*
   DAYLIGHT sub-component
 */
void ical_component_DAYLIGHT(struct exchange2ical *exchange2ical)
{
	char		*dtstart = NULL;
	int32_t		tzoffsetfrom;
	int32_t		tzoffsetto;
	icalcomponent	*daylight;
	icalproperty	*prop;

	daylight = icalcomponent_new_xdaylight();
	icalcomponent_add_component(exchange2ical->vtimezone, daylight);

	/* DTSTART property */
	dtstart = get_ical_date(exchange2ical->mem_ctx, &exchange2ical->TimeZoneStruct->stDaylightDate);
	if (dtstart) {
		prop = icalproperty_new_dtstart(icaltime_from_string(dtstart));
		icalcomponent_add_property(daylight, prop);
		talloc_free(dtstart);
	}
	
	/* RRULE property */
	ical_property_RRULE_daylight_standard(daylight,exchange2ical->TimeZoneStruct->stDaylightDate);
	
	/* TZOFFSETFROM property */
	tzoffsetfrom = (-60 * (exchange2ical->TimeZoneStruct->lBias + exchange2ical->TimeZoneStruct->lStandardBias));
	prop = icalproperty_new_tzoffsetfrom(tzoffsetfrom);
	icalcomponent_add_property(daylight, prop);

	/* TZOFFSETTO property */
	tzoffsetto = (-60 * (exchange2ical->TimeZoneStruct->lBias + exchange2ical->TimeZoneStruct->lDaylightBias));
	prop = icalproperty_new_tzoffsetto(tzoffsetto);
	icalcomponent_add_property(daylight, prop);
}


/*
   VALARM component

   [MS-OXCICAL], Section 2.2.1.20.61 
 */
void ical_component_VALARM(struct exchange2ical *exchange2ical)
{
	icalproperty *action;
	icalproperty *description;

	/* Sanity check */
	if (!exchange2ical->vevent) return;
	if (!exchange2ical->ReminderSet) return;
	if (*exchange2ical->ReminderSet == false) return;

	exchange2ical->valarm = icalcomponent_new_valarm();
	if (!(exchange2ical->valarm)) {
		printf("could not create new valarm\n");
		return;
	}
	icalcomponent_add_component(exchange2ical->vevent, exchange2ical->valarm);
	ical_property_TRIGGER(exchange2ical);
	action = icalproperty_new_action(ICAL_ACTION_DISPLAY);
	icalcomponent_add_property(exchange2ical->valarm, action);
	description = icalproperty_new_description("Reminder");
	icalcomponent_add_property(exchange2ical->valarm, description);
}
