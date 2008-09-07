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

struct exchange2ical {
	TALLOC_CTX			*mem_ctx;
	struct message_recipients	Recipients;
	const char			*method;
	const char			*partstat;
	uint8_t				*Recurring;
	struct RecurrencePattern	*RecurrencePattern;
	struct TimeZoneStruct		*TimeZoneStruct;
	const char			*TimeZoneDesc;
	const struct StringArray_r     	*Keywords;
	const struct StringArray_r	*Contacts;
	uint32_t			*apptStateFlags;
	uint32_t			*sensitivity;
	uint32_t			*Importance;
	const struct FILETIME  		*created;
	const char			*body;
	const struct FILETIME		*apptStartWhole;
	const struct FILETIME		*apptEndWhole;
	const struct FILETIME		*OwnerCriticalChange;
	const struct FILETIME		*LastModified;
	const struct FILETIME		*ExceptionReplaceTime;
	uint8_t				*apptSubType;
	const char			*Location;       
	uint8_t				*ResponseRequested;
	const char			*NonSendableBcc;
	uint32_t			*Sequence;
	const char			*Subject;
	uint32_t			*BusyStatus;
	uint32_t			*IntendedBusyStatus;
	struct SBinary  		*GlobalObjectId;
	const struct FILETIME		*AttendeeCriticalChange;
	uint32_t			*OwnerApptId;
	const struct FILETIME		*apptReplyTime;
	uint8_t				*NotAllowPropose;
	uint8_t				*AllowExternCheck;
	uint32_t			*apptLastSequence;
	const struct FILETIME		*apptSeqTime;
	uint8_t				*AutoFillLocation;
	uint8_t				*AutoStartCheck;
	const char			*CollaborateDoc;
	uint8_t				*ConfCheck;
	uint32_t			*ConfType;
	const char			*Directory;
	const char			*MWSURL;
	const char			*NetShowURL;
	const char			*OnlinePassword;
	const char			*OrgAlias;
	const char			*SenderName;
	const char			*SenderEmailAddress;
	uint8_t				*ReminderSet;
	uint32_t			*ReminderDelta;
};

struct	ical_method {
	const char	*method;
	const char	*partstat;
	const char	*PidTagMessageClass;
};

struct ical_calendartype {
	uint16_t	type;
	const char	*calendar;
};

struct ical_class {
	uint32_t	sensivity;
	const char	*classname;
};

#define	ICAL_PRODID	"-//OpenChange Project/exchange2ical MIMEDIR//EN"
#define	ICAL_VERSION	"2.0"

__BEGIN_DECLS

/* definitions from exchange2ical_utils.c */
struct tm *get_tm_from_FILETIME(const struct FILETIME *);

bool has_component_DAYLIGHT(struct exchange2ical *);

char *get_ical_date(TALLOC_CTX *, struct SYSTEMTIME *);
const char *get_ical_method(const char *);
const char *get_ical_partstat(const char *);
const char *get_ical_calendartype(uint16_t);
const char *get_ical_class(uint32_t);

/* definitions from exchange2ical_component.c */
void ical_component_VCALENDAR(struct exchange2ical *);
void ical_component_VEVENT(struct exchange2ical *);
void ical_component_VTIMEZONE(struct exchange2ical *);
void ical_component_STANDARD(struct exchange2ical *);
void ical_component_DAYLIGHT(struct exchange2ical *);
void ical_component_VALARM(struct exchange2ical *);

/* definitions from exchange2ical_property.c */
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
void ical_property_RRULE(struct exchange2ical *, struct SYSTEMTIME);
void ical_property_RECURRENCE_ID(struct exchange2ical *);
void ical_property_RESOURCES(struct exchange2ical *);
void ical_property_SEQUENCE(struct exchange2ical *);
void ical_property_SUMMARY(struct exchange2ical *);
void ical_property_TRANSP(struct exchange2ical *);
void ical_property_TRIGGER(struct exchange2ical *);
void ical_property_UID(struct exchange2ical *);
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

__END_DECLS

#endif /* __EXCHANGE2ICAL_H_ */
