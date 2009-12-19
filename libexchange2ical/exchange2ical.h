/*
   Convert Exchange appointments to ICAL

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

#ifndef	__EXCHANGE2ICAL_H_
#define	__EXCHANGE2ICAL_H_

#include <libmapi/libmapi.h>
#include <gen_ndr/ndr_property.h>
#include <utils/openchange-tools.h>

#include <libical/ical.h>

#include <time.h>

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS		extern "C" {
#define __END_DECLS		}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif


struct message_recipients {
	struct SRowSet			SRowSet;
	struct SPropTagArray		SPropTagArray;
};

enum exchange2ical_flags{
	EntireFlag = 	0x00001,
	RangeFlag =  	0x00010,
	VcalFlag =	0x00100,
	EventFlag = 	0x01000,
	EventsFlag = 	0x10000
};

struct exchange2ical_check {
	enum exchange2ical_flags eFlags;
	struct tm *begin;
	struct tm *end;
	struct GlobalObjectId *GlobalObjectId;
	uint32_t Sequence;
};

struct exchange2ical {
	TALLOC_CTX				*mem_ctx;
	struct message_recipients		Recipients;
	enum icalproperty_method		method;
	enum icalparameter_partstat		partstat;
	uint32_t				*ResponseStatus;
	uint8_t					*Recurring;
	struct RecurrencePattern		*RecurrencePattern;
	struct TimeZoneStruct			*TimeZoneStruct;
	const char				*TimeZoneDesc;
	const struct StringArray_r     		*Keywords;
	const struct StringArray_r		*Contacts;
	uint32_t				*apptStateFlags;
	uint32_t				*sensitivity;
	uint32_t				*Importance;
	const struct FILETIME  			*created;
	const char				*body;
	const struct FILETIME			*apptStartWhole;
	const struct FILETIME			*apptEndWhole;
	const struct FILETIME			*OwnerCriticalChange;
	const struct FILETIME			*LastModified;
	const struct FILETIME			*ExceptionReplaceTime;
	uint8_t					*apptSubType;
	const char				*Location;       
	uint8_t					*ResponseRequested;
	const char				*NonSendableBcc;
	uint32_t				*Sequence;
	const char				*Subject;
	uint32_t				*MessageLocaleId;
	uint32_t				*BusyStatus;
	uint32_t				*IntendedBusyStatus;
	struct Binary_r				*GlobalObjectId;
	const struct FILETIME			*AttendeeCriticalChange;
	uint32_t				*OwnerApptId;
	const struct FILETIME			*apptReplyTime;
	uint8_t					*NotAllowPropose;
	uint8_t					*AllowExternCheck;
	uint32_t				*apptLastSequence;
	const struct FILETIME			*apptSeqTime;
	uint8_t					*AutoFillLocation;
	uint8_t					*AutoStartCheck;
	const char				*CollaborateDoc;
	uint8_t					*ConfCheck;
	uint32_t				*ConfType;
	const char				*Directory;
	const char				*MWSURL;
	const char				*NetShowURL;
	const char				*OnlinePassword;
	const char				*OrgAlias;
	const char				*SenderName;
	const char				*SenderEmailAddress;
	uint8_t					*ReminderSet;
	uint32_t				*ReminderDelta;	
	icalcomponent				*vcalendar;
	icalcomponent				*vevent;
	icalcomponent				*vtimezone;
	icalcomponent				*valarm;
	mapi_object_t				obj_message;
	const char				*bodyHTML;
	uint32_t				idx;
	struct	AppointmentRecurrencePattern	*AppointmentRecurrencePattern;
};


struct	ical_method {
	enum icalproperty_method	method;
	enum icalparameter_partstat	partstat;
	const char			*PidTagMessageClass;
};

struct ical_calendartype {
	uint16_t	type;
	const char	*calendar;
};

struct ical_day {
	enum icalrecurrencetype_weekday ical;
	enum FirstDOW exchange;
	uint32_t rdfDays;

};

struct ical_class {
	uint32_t		sensivity;
	enum icalproperty_class	classtype;
};


#define	OPENCHANGE_ICAL_PRODID	"-//OpenChange Project/exchange2ical MIMEDIR//EN"
#define	OPENCHANGE_ICAL_VERSION	"2.0"

__BEGIN_DECLS

/* definitions from exchang2ical.c */
icalcomponent * _Exchange2Ical(mapi_object_t *obj_folder, struct exchange2ical_check *exchange2ical_check);


/* definitions from exchange2ical_utils.c */
struct icaltimetype get_icaltime_from_FILETIME(const struct FILETIME *);
struct icaltimetype get_icaltime_from_FILETIME_UTC(const struct FILETIME *);
struct icaltimetype get_icaldate_from_FILETIME(const struct FILETIME *);
struct tm *get_tm_from_FILETIME(const struct FILETIME *);
struct icaltimetype get_icaldate_from_tm(struct tm *);
struct icaltimetype get_icaltimetype_from_tm_UTC(struct tm *tm);
struct icaltimetype get_icaltimetype_from_tm(struct tm *tm);
struct FILETIME get_FILETIME_from_string(const char *);
struct FILETIME get_FILETIME_from_icaltimetype(icaltimetype *);
struct tm get_tm_from_minutes(uint32_t mins);
struct tm get_tm_from_minutes_UTC(uint32_t mins);
struct icaltimetype get_icaldate_from_GlobalObjectId(struct GlobalObjectId *);
NTTIME FILETIME_to_NTTIME(struct FILETIME);
enum icalproperty_method get_ical_method(const char *);
enum icalparameter_partstat get_ical_partstat(const char *);
enum icalproperty_class get_ical_class(uint32_t);
enum icalparameter_partstat get_ical_partstat_from_status(uint32_t status);
enum FirstDOW get_exchange_day_from_ical(enum icalrecurrencetype_weekday weekday);
uint8_t set_exception_from_ExceptionInfo(struct exchange2ical *, struct exchange2ical_check *);
uint8_t set_exception_from_EmbeddedObj(struct exchange2ical *, struct exchange2ical_check *);
bool checkEvent(struct exchange2ical *exchange2ical, struct exchange2ical_check *exchange2ical_check, struct tm *aptStart);
bool compareGlobalObjectIds(struct GlobalObjectId *glb1, struct GlobalObjectId *glb2);
bool has_component_DAYLIGHT(struct exchange2ical *);
uint16_t get_exchange_calendartype(const char *);
uint32_t get_minutes_from_icaltimetype(icaltimetype);
uint32_t get_exchange_rdfDays_from_ical(enum icalrecurrencetype_weekday weekday);
const char *get_ical_calendartype(uint16_t);
char *get_ical_date(TALLOC_CTX *, struct SYSTEMTIME *);
int compare_minutes(const void *min1, const void *min2);

/* definitions from exchange2ical_component.c */
void ical_component_VCALENDAR(struct exchange2ical *);
void ical_component_VEVENT(struct exchange2ical *);
void ical_component_VTIMEZONE(struct exchange2ical *);
void ical_component_STANDARD(struct exchange2ical *);
void ical_component_DAYLIGHT(struct exchange2ical *);
void ical_component_VALARM(struct exchange2ical *);


/* definitions from exchange2ical_property.c */
void ical_property_ATTACH(struct exchange2ical *);
void ical_property_ATTENDEE(struct exchange2ical *);
void ical_property_CATEGORIES(struct exchange2ical *);
void ical_property_CLASS(struct exchange2ical *);
void ical_property_CONTACT(struct exchange2ical *);
void ical_property_CREATED(struct exchange2ical *);
void ical_property_DTEND(struct exchange2ical *);
void ical_property_DTSTAMP(struct exchange2ical *);
void ical_property_DTSTART(struct exchange2ical *);
void ical_property_DESCRIPTION(struct exchange2ical *);
void ical_property_EXDATE(struct exchange2ical *);
void ical_property_LAST_MODIFIED(struct exchange2ical *);
void ical_property_LOCATION(struct exchange2ical *);
void ical_property_ORGANIZER(struct exchange2ical *);
void ical_property_PRIORITY(struct exchange2ical *);
void ical_property_RDATE(struct exchange2ical *);
void ical_property_RRULE_Daily(struct exchange2ical *);
void ical_property_RRULE_Weekly(struct exchange2ical *);
void ical_property_RRULE_Monthly(struct exchange2ical *);
void ical_property_RRULE_NthMonthly(struct exchange2ical *);
void ical_property_RRULE_Yearly(struct exchange2ical *);
void ical_property_RRULE_NthYearly(struct exchange2ical *);
void ical_property_RRULE(struct exchange2ical *);
void ical_property_RRULE_daylight_standard(icalcomponent* component , struct SYSTEMTIME st);
void ical_property_RECURRENCE_ID(struct exchange2ical *);
void ical_property_RESOURCES(struct exchange2ical *);
void ical_property_SEQUENCE(struct exchange2ical *);
void ical_property_SUMMARY(struct exchange2ical *);
void ical_property_TRANSP(struct exchange2ical *);
void ical_property_TRIGGER(struct exchange2ical *);
void ical_property_UID(struct exchange2ical *);
void ical_property_X_ALT_DESC(struct exchange2ical *);
void ical_property_X_MICROSOFT_CDO_ATTENDEE_CRITICAL_CHANGE(struct exchange2ical *);
void ical_property_X_MICROSOFT_CDO_BUSYSTATUS(struct exchange2ical *);
void ical_property_X_MICROSOFT_CDO_INTENDEDSTATUS(struct exchange2ical *);
void ical_property_X_MICROSOFT_CDO_OWNERAPPTID(struct exchange2ical *);
void ical_property_X_MICROSOFT_CDO_OWNER_CRITICAL_CHANGE(struct exchange2ical *);
void ical_property_X_MICROSOFT_CDO_REPLYTIME(struct exchange2ical *);
void ical_property_X_MICROSOFT_DISALLOW_COUNTER(struct exchange2ical *);
void ical_property_X_MS_OLK_ALLOWEXTERNCHECK(struct exchange2ical *);
void ical_property_X_MS_OLK_APPTLASTSEQUENCE(struct exchange2ical *);
void ical_property_X_MS_OLK_APPTSEQTIME(struct exchange2ical *);
void ical_property_X_MS_OLK_AUTOFILLLOCATION(struct exchange2ical *);
void ical_property_X_MS_OLK_AUTOSTARTCHECK(struct exchange2ical *);
void ical_property_X_MS_OLK_COLLABORATEDOC(struct exchange2ical *);
void ical_property_X_MS_OLK_CONFCHECK(struct exchange2ical *);
void ical_property_X_MS_OLK_CONFTYPE(struct exchange2ical *);
void ical_property_X_MS_OLK_DIRECTORY(struct exchange2ical *);
void ical_property_X_MS_OLK_MWSURL(struct exchange2ical *);
void ical_property_X_MS_OLK_NETSHOWURL(struct exchange2ical *);
void ical_property_X_MS_OLK_ONLINEPASSWORD(struct exchange2ical *);
void ical_property_X_MS_OLK_ORGALIAS(struct exchange2ical *);
void ical_property_X_MS_OLK_SENDER(struct exchange2ical *);
void ical_property_X_MICROSOFT_MSNCALENDAR_IMPORTANCE(struct exchange2ical *);


struct ical2exchange{
	TALLOC_CTX				*mem_ctx;
	enum icalproperty_method	method;
	icalproperty 			*classProp;
	icalproperty			*commentProp;
	icalproperty 			*descriptionProp;
	icalproperty 			*dtendProp;
	icalproperty 			*dtstampProp;
	icalproperty 			*dtstartProp;
	icalproperty 			*durationProp;
	icalproperty 			*locationProp;
	icalproperty 			*organizerProp;
	icalproperty 			*priorityProp;
	icalproperty 			*recurrenceidProp;
	icalproperty 			*rruleProp;
	icalproperty			*sequenceProp;
	icalproperty			*statusProp;
	icalproperty			*summaryProp;
	icalproperty 			*transpProp;
	icalproperty 			*uidProp;
	
	uint32_t			rdateCount;
	uint32_t			exdateCount;
	
	/*		x properties				*/
	icalproperty			*x_busystatusProp;
	icalproperty			*x_sequenceProp;
	icalproperty			*x_importanceProp;
	icalproperty			*x_intendedProp;
	icalproperty			*x_ownerapptidProp;
	icalproperty			*x_attendeecriticalchangeProp;
	icalproperty			*x_replytimeProp;
	icalproperty			*x_disallowcounterProp;
	icalproperty			*x_isdraftProp;
	icalproperty			*x_allowexterncheckProp;
	icalproperty			*x_apptlastsequenceProp;
	icalproperty			*x_apptseqtimeProp;
	icalproperty			*x_autofilllocationProp;
	icalproperty			*x_autostartcheckProp;
	icalproperty			*x_confcheckProp;
	icalproperty			*x_collaborateddocProp;
	icalproperty			*x_conftypeProp;
	icalproperty			*x_mwsurlProp;
	icalproperty			*x_netshowurlProp;
	icalproperty			*x_onlinepasswordProp;
	icalproperty			*x_originalstartProp;
	icalproperty			*x_originalendProp;
	icalproperty			*x_orgaliasProp;
	icalproperty			*x_ownercriticalchangeProp;
				
	
	/*		Events					*/
	icalcomponent			*attachEvent;
	icalcomponent			*attendeeEvent;
	icalcomponent			*categoriesEvent;
	icalcomponent			*contactEvent;
	icalcomponent			*exdateEvent;
	icalcomponent			*rdateEvent;
	icalcomponent			*resourcesEvent;
	icalcomponent			*valarmEvent;
	
	mapi_object_t			*obj_message;
	struct SPropValue		*lpProps;
	uint32_t			cValues;

};


/*ical2exchange file*/
void _IcalEvent2Exchange(mapi_object_t *obj_folder, icalcomponent *);


/*ical2exchange_property*/
void ical2exchange_property_ATTACH(struct ical2exchange *);
//TODO ATTENDEE, ORGANIZER, x_ms_ok_sender
void ical2exchange_property_CATEGORIES(struct ical2exchange *);
void ical2exchange_property_CLASS(struct ical2exchange *);
void ical2exchange_property_COMMENT(struct ical2exchange *);
void ical2exchange_property_CONTACT(struct ical2exchange *);
void ical2exchange_property_DESCRIPTION(struct ical2exchange *);
void ical2exchange_property_DTSTAMP(struct ical2exchange *);
void ical2exchange_property_DTSTART_DTEND(struct ical2exchange *);
void ical2exchange_property_LOCATION(struct ical2exchange *);
void ical2exchange_property_PRIORITY(struct ical2exchange *);
void ical2exchange_property_RRULE_EXDATE_RDATE(struct ical2exchange *);
void ical2exchange_property_SEQUENCE(struct ical2exchange *);
void ical2exchange_property_STATUS(struct ical2exchange *);
void ical2exchange_property_SUMMARY(struct ical2exchange *);
void ical2exchange_property_VALARM(struct ical2exchange *);
void ical2exchange_property_X_ALLOWEXTERNCHECK(struct ical2exchange *);
void ical2exchange_property_X_APPTSEQTIME(struct ical2exchange *);
void ical2exchange_property_X_APPTLASTSEQUENCE(struct ical2exchange *);
void ical2exchange_property_X_ATTENDEE_CRITICAL_CHANGE(struct ical2exchange *);
void ical2exchange_property_X_AUTOFILLLOCATION(struct ical2exchange *);
void ical2exchange_property_X_AUTOSTARTCHECK(struct ical2exchange *);
void ical2exchange_property_X_COLLABORATEDDOC(struct ical2exchange *);
void ical2exchange_property_X_CONFCHECK(struct ical2exchange *);
void ical2exchange_property_X_CONFTYPE(struct ical2exchange *);
void ical2exchange_property_X_DISALLOW_COUNTER(struct ical2exchange *);
void ical2exchange_property_X_INTENDEDSTATUS(struct ical2exchange *);
void ical2exchange_property_X_ISDRAFT(struct ical2exchange *);
void ical2exchange_property_X_MWSURL(struct ical2exchange *);
void ical2exchange_property_X_NETSHOWURL(struct ical2exchange *);
void ical2exchange_property_X_ONLINEPASSWORD(struct ical2exchange *);
void ical2exchange_property_X_ORGALIAS(struct ical2exchange *);
void ical2exchange_property_X_ORIGINALEND_ORIGINALSTART(struct ical2exchange *);
void ical2exchange_property_X_OWNER_CRITICAL_CHANGE(struct ical2exchange *);
void ical2exchange_property_X_OWNERAPPTID(struct ical2exchange *);
void ical2exchange_property_X_REPLYTIME(struct ical2exchange *);


__END_DECLS

#endif /* __EXCHANGE2ICAL_H_ */
