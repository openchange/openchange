/*
   Common conversion routines for exchange2ical

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

static struct ical_method	ical_method[] = {
	{ ICAL_METHOD_PUBLISH,	ICAL_PARTSTAT_NONE,	"IPM.Appointment"		},
	{ ICAL_METHOD_REQUEST,	ICAL_PARTSTAT_NONE,	"IPM.Schedule.Meeting.Request"	},
	{ ICAL_METHOD_REPLY,	ICAL_PARTSTAT_ACCEPTED,	"IPM.Schedule.Meeting.Resp.Pos" },
	{ ICAL_METHOD_REPLY,	ICAL_PARTSTAT_TENTATIVE,"IPM.Schedule.Meeting.Resp.Tent"},
	{ ICAL_METHOD_REPLY,	ICAL_PARTSTAT_DECLINED,	"IPM.Schedule.Meeting.Resp.Neg"	},
	{ ICAL_METHOD_CANCEL,	ICAL_PARTSTAT_NONE,	"IPM.Schedule.Meeting.Canceled" },
	{ ICAL_METHOD_NONE,	ICAL_PARTSTAT_NONE,	NULL }
};

static struct ical_calendartype	ical_calendartype[] = {
	{ 0x0001,	"Gregorian"		},
	{ 0x0002,	"Gregorian_us"		},
	{ 0x0003,	"Japan"			},
	{ 0x0004,	"Taiwan"		},
	{ 0x0005,	"Korean"		},
	{ 0x0006,	"Hijri"			},
	{ 0x0007,	"Thai"			},
	{ 0x0008,	"Hebrew"		},
	{ 0x0009,	"GregorianMeFrench"	},
	{ 0x000A,	"GregorianArabic"	},
	{ 0x000B,	"GregorianXlitEnglish"	},
	{ 0x000C,	"GregorianXlitFrench"	},
	{ 0x000E,	"JapanLunar"		},
	{ 0x000F,	"ChineseLunar"		},
	{ 0x0010,	"Saka"			},
	{ 0x0011,	"LunarEtoChn"		},
	{ 0x0012,	"LunarEthoKor"		},
	{ 0x0013,	"LunarRockuyou"		},
	{ 0x0014,	"KoreanLunar"		},
	{ 0x0017,	"Umalqura"		},
	{ 0x0000,	NULL			}
};

static struct ical_class	ical_class[] = {
	{ 0x00000000,	ICAL_CLASS_PUBLIC	},
	{ 0x00000001,	ICAL_CLASS_X		},
	{ 0x00000002,	ICAL_CLASS_PRIVATE	},
	{ 0x00000003,	ICAL_CLASS_CONFIDENTIAL	},
	{ 0x00000000,	ICAL_CLASS_NONE		}
};

static struct ical_day	ical_day[] = {
	{ ICAL_SUNDAY_WEEKDAY,		FirstDOW_Sunday, 	Su },
	{ ICAL_MONDAY_WEEKDAY,		FirstDOW_Monday,	M  },
	{ ICAL_TUESDAY_WEEKDAY,		FirstDOW_Tuesday,	Tu },
	{ ICAL_WEDNESDAY_WEEKDAY,	FirstDOW_Wednesday, 	W  },
	{ ICAL_THURSDAY_WEEKDAY,	FirstDOW_Thursday,	Th },
	{ ICAL_FRIDAY_WEEKDAY,		FirstDOW_Friday,	F  },
	{ ICAL_SATURDAY_WEEKDAY,	FirstDOW_Saturday,	Sa },
	{ ICAL_NO_WEEKDAY,		0,			0 }

};

bool has_component_DAYLIGHT(struct exchange2ical *exchange2ical)
{
	if (!exchange2ical->TimeZoneStruct) return false;
	if (!exchange2ical->TimeZoneStruct->stStandardDate.wYear &&
	    !exchange2ical->TimeZoneStruct->stStandardDate.wMonth &&
	    !exchange2ical->TimeZoneStruct->stStandardDate.wDayOfWeek &&
	    !exchange2ical->TimeZoneStruct->stStandardDate.wDay &&
	    !exchange2ical->TimeZoneStruct->stStandardDate.wHour &&
	    !exchange2ical->TimeZoneStruct->stStandardDate.wMinute &&
	    !exchange2ical->TimeZoneStruct->stStandardDate.wSecond &&
	    !exchange2ical->TimeZoneStruct->stStandardDate.wMilliseconds) return false;

	return true;
}

struct FILETIME get_FILETIME_from_string(const char *time)
{
	icaltimetype icaltime;
	
	icaltime = icaltime_from_string(time);
	return get_FILETIME_from_icaltimetype(&icaltime);
}


char *get_ical_date(TALLOC_CTX *mem_ctx, struct SYSTEMTIME *time)
{
	char	*date;

	date = talloc_asprintf(mem_ctx, "%.4d%.2d%.2dT%.2d%.2d%.2d", time->wYear + 1601, time->wMonth,
			       time->wDay, time->wHour, time->wMinute, time->wSecond);
	return date;
}


enum icalproperty_method get_ical_method(const char *msgclass)
{
	uint32_t	i;

	/* Sanity check */
	if (!msgclass) return ICAL_METHOD_NONE;

	for (i = 0; ical_method[i].messageclass; i++) {
		if (!strcmp(msgclass, ical_method[i].messageclass)) {
			return ical_method[i].method;
		}
	}

	return ICAL_METHOD_NONE;
}

uint16_t get_exchange_calendartype(const char *CalendarType)
{
	uint32_t	i;

	/* Sanity check */
	if (!CalendarType) return 0x0000;

	for (i = 0; ical_calendartype[i].type; i++) {
		if (strcmp(CalendarType, ical_calendartype[i].calendar)) {
			return ical_calendartype[i].type;
		}
	}
	return 0x0000;
}

const char *get_ical_calendartype(uint16_t CalendarType)
{
	uint32_t	i;

	/* Sanity check */
	if (!CalendarType) return NULL;

	for (i = 0; ical_calendartype[i].type; i++) {
		if (CalendarType == ical_calendartype[i].type) {
			return ical_calendartype[i].calendar;
		}
	}
	
	return NULL;
}
enum icalparameter_partstat get_ical_partstat_from_status(uint32_t status)
{
	enum icalparameter_partstat partstat;
	switch(status){
		case 0x00000003:
			partstat = ICAL_PARTSTAT_ACCEPTED;
			break;
		case 0x00000004:
			partstat = ICAL_PARTSTAT_DECLINED;
			break;
		case 0x00000002:
			partstat = ICAL_PARTSTAT_TENTATIVE;
			break;
		default:
			partstat = ICAL_PARTSTAT_NONE;
	}
	return partstat;
}

enum icalparameter_partstat get_ical_partstat(const char *msgclass)
{
	uint32_t	i;

	/* Sanity check */
	if (!msgclass) return ICAL_PARTSTAT_NONE;

	for (i = 0; ical_method[i].messageclass; i++) {
		if (!strcmp(msgclass, ical_method[i].messageclass)) {
			return ical_method[i].partstat;
		}
	}

	return ICAL_PARTSTAT_NONE;
}

enum icalproperty_class get_ical_class(uint32_t sensivity)
{
	uint32_t	i;
	
	for (i = 0; ical_class[i].classtype != ICAL_CLASS_NONE; ++i) {
		if (sensivity == ical_class[i].sensivity) {
			return ical_class[i].classtype;
		}
	}

	return ICAL_CLASS_NONE;
}

enum FirstDOW get_exchange_day_from_ical(enum icalrecurrencetype_weekday weekday)
{
	
	uint32_t	i;
	
	for (i = 0; ical_day[i].ical != ICAL_NO_WEEKDAY; ++i) {
		if (weekday == ical_day[i].ical) {
			return ical_day[i].exchange;
		}
	}

	return 0x00000000;
}

uint32_t get_exchange_rdfDays_from_ical(enum icalrecurrencetype_weekday weekday){
	
	uint32_t	i;
	
	for (i = 0; ical_day[i].ical != ICAL_NO_WEEKDAY; ++i) {
		if (weekday == ical_day[i].ical) {
			return ical_day[i].rdfDays;
		}
	}

	return 0;
}

struct icaltimetype get_icaltimetype_from_tm(struct tm *tm)
{
	struct icaltimetype tt;

	tt.year   = tm->tm_year+1900;
	tt.month  = tm->tm_mon+1;
	tt.day    = tm->tm_mday;
	tt.hour   = tm->tm_hour;
	tt.minute = tm->tm_min;
	tt.second = tm->tm_sec;

	tt.is_date     = 0;
	tt.is_daylight = 0;
	tt.zone        = 0;

	return tt;
}

struct icaltimetype get_icaltimetype_from_tm_UTC(struct tm *tm)
{
	struct icaltimetype tt;
	
	tt = get_icaltimetype_from_tm(tm);
	
	return tt;
}

struct icaltimetype get_icaldate_from_tm(struct tm *tm)
{
	struct icaltimetype tt;

	tt.year   = tm->tm_year+1900;
	tt.month  = tm->tm_mon+1;
	tt.day    = tm->tm_mday;
	tt.hour   = 0;
	tt.minute = 0;
	tt.second = 0;

	tt.is_date     = 1;
	tt.is_daylight = 0;
	tt.zone        = NULL;

	return tt;
}

struct tm *get_tm_from_FILETIME(const struct FILETIME *ft)
{
	NTTIME		time;
	struct timeval	t;
	struct tm	*tm;

	/* Sanity checks */
	if (ft == NULL) return NULL;
	
	time = FILETIME_to_NTTIME(*ft);
	nttime_to_timeval(&t, time);
	tm = localtime(&t.tv_sec);

	return tm;
}

struct icaltimetype get_icaltime_from_FILETIME(const struct FILETIME *ft)
{
	struct icaltimetype	tt;
	NTTIME			nttime;
	struct timeval		temp_timeval;
	struct tm		*tm;

	nttime = FILETIME_to_NTTIME(*ft);
	nttime_to_timeval(&temp_timeval, nttime);
	
	tm = localtime(&temp_timeval.tv_sec);
	
	tt.year   = tm->tm_year + 1900;                                        
	tt.month  = tm->tm_mon + 1;                                            
	tt.day    = tm->tm_mday;                                               
	tt.hour   = tm->tm_hour;                                               
	tt.minute = tm->tm_min;                                                
	tt.second = tm->tm_sec;
	tt.is_date = 0;
	tt.is_daylight = 0;
	tt.zone = NULL;

	return tt;
}

struct icaltimetype get_icaltime_from_FILETIME_UTC(const struct FILETIME *ft)
{
	struct icaltimetype	tt;
	NTTIME			nttime;
	struct timeval		temp_timeval;
	struct tm		*tm;

	nttime = FILETIME_to_NTTIME(*ft);
	nttime_to_timeval(&temp_timeval, nttime);

	tm = gmtime(&temp_timeval.tv_sec);
	
	tt.year   = tm->tm_year + 1900;                                        
	tt.month  = tm->tm_mon + 1;                                            
	tt.day    = tm->tm_mday;                                               
	tt.hour   = tm->tm_hour;                                               
	tt.minute = tm->tm_min;                                                
	tt.second = tm->tm_sec;
	tt.is_date = 0;
	tt.is_daylight = 0;
	tt.zone = NULL;

	return tt;
}

struct icaltimetype get_icaldate_from_FILETIME(const struct FILETIME *ft)
{
	struct icaltimetype	tt;
	NTTIME			nttime;
	struct timeval		temp_timeval;
	struct tm		*tm;

	nttime = FILETIME_to_NTTIME(*ft);
	nttime_to_timeval(&temp_timeval, nttime);

	tm = gmtime(&temp_timeval.tv_sec);
                                                                 
	tt.year   = tm->tm_year + 1900;                                        
	tt.month  = tm->tm_mon + 1;                                            
	tt.day    = tm->tm_mday;
	if (tm->tm_hour >= 12) {
		/* If the time is greater than or equal to 12, and we're representing midnight, then
		the actual day is one less (e.g. if we're at UTC+11 timezone, then local
		midnight is 1300 UTC, on the day before). So we add a day. */
		tt.day++;
	}
	tt.hour   = 0;                                               
	tt.minute = 0;                                                
	tt.second = 0;
	tt.is_date = 1;
	tt.is_daylight = 0;
	tt.zone = NULL;

	return tt;
}

struct icaltimetype get_icaldate_from_GlobalObjectId(struct GlobalObjectId *GlobalObjectId)
{
	struct icaltimetype	tt;
	tt.year   = GlobalObjectId->YH;
	tt.year   = tt.year <<8;
	tt.year   |= GlobalObjectId->YL;
	tt.month  = GlobalObjectId->Month;
	tt.day    = GlobalObjectId->D;
	tt.hour   = 0;
	tt.minute = 0;
	tt.second = 0;

	tt.is_date     = 1;
	tt.is_daylight = 0;
	tt.zone        = NULL;

	return tt;
}

struct tm get_tm_from_minutes(uint32_t mins)
{
	struct timeval	t;
	struct tm	tm;
	NTTIME		nt;
	nt = mins;
	nt *= 10000000;
	nt *= 60;
	nttime_to_timeval(&t, nt);
	tm= *localtime(&t.tv_sec);
	return tm;
}

struct tm get_tm_from_minutes_UTC(uint32_t mins)
{
	struct timeval	t;
	struct tm	tm;
	NTTIME		nt;
	nt = mins;
	nt *= 10000000;
	nt *= 60;
	nttime_to_timeval(&t, nt);
	tm= *gmtime(&t.tv_sec);
	return tm;
}

bool compareGlobalObjectIds(struct GlobalObjectId *glb1, struct GlobalObjectId *glb2)
{
	int i;
	time_t creationTime1;
	time_t creationTime2;
	
	/* Sanity checks */
	if (!&glb1->CreationTime || !&glb2->CreationTime) return false;
	
	creationTime1=mktime(get_tm_from_FILETIME(&glb1->CreationTime));
	creationTime2=mktime(get_tm_from_FILETIME(&glb2->CreationTime));
	
	if(difftime(creationTime1, creationTime2)!=0) return false;
	
	for(i=0; i<16; i++){
		if(glb1->ByteArrayID[i]!=glb2->ByteArrayID[i]) return false;
	}
	if(glb1->YH!=glb2->YH) return false;
	if(glb1->YL!=glb2->YL) return false;
	if(glb1->Month!=glb2->Month) return false;
	if(glb1->D!=glb2->D) return false;
	
	for(i=0; i<8; i++){
		if(glb1->X[i]!=glb2->X[i]) return false;
	}
	if(glb1->Size!=glb2->Size) return false;
	for(i=0; i<glb1->Size; i++){
		if(glb1->Data[i]!=glb2->Data[i]) return false;
	}
	return true;
}



bool checkEvent(struct exchange2ical *exchange2ical, struct exchange2ical_check *exchange2ical_check, struct tm *aptStart)
{
	time_t beginTime;
	time_t endTime;
	time_t dtstart;
	struct GlobalObjectId	*GlbObjId;
	enum exchange2ical_flags eFlags= exchange2ical_check->eFlags;

	if(eFlags & EntireFlag) return true;
		
	if(eFlags & RangeFlag){
		dtstart = mktime(aptStart);
		if(dtstart==-1) return false;
		if(exchange2ical_check->begin){
			beginTime=mktime(exchange2ical_check->begin);
			if(beginTime!=-1){
				if(difftime(dtstart, beginTime)<0){
					return false;
				}
			}
		}
		if(exchange2ical_check->end){
			endTime=mktime(exchange2ical_check->end);
			if(endTime!=-1){
				if(difftime(endTime, dtstart)<0){
					return false;
				}
			}
		}
		return true;
	}
	
	if(eFlags & EventFlag){
		if(!exchange2ical->Sequence) return false;
		GlbObjId = get_GlobalObjectId(exchange2ical->mem_ctx, exchange2ical->GlobalObjectId);
		if(!GlbObjId || !exchange2ical_check->GlobalObjectId) return false;
		if (!compareGlobalObjectIds(GlbObjId, exchange2ical_check->GlobalObjectId)) return false;
		if(*exchange2ical->Sequence != exchange2ical_check->Sequence) return false;
		talloc_free(GlbObjId);
		return true;
	}
	
	if(eFlags & EventsFlag){
		GlbObjId = get_GlobalObjectId(exchange2ical->mem_ctx, exchange2ical->GlobalObjectId);
		if(!GlbObjId || !exchange2ical_check->GlobalObjectId) return false;
		if (!compareGlobalObjectIds(GlbObjId, exchange2ical_check->GlobalObjectId)) return false;
		talloc_free(GlbObjId);
		return true;
	}
	
	return false;
}



#define WIN32_UNIX_EPOCH_DIFF 194074560

uint32_t get_minutes_from_icaltimetype(icaltimetype icaltime)
{
	struct tm		tm;
	time_t			time;
	NTTIME			nttime;
	struct timeval 		t;
	
	tm.tm_year	= icaltime.year - 1900;
	tm.tm_mon 	= icaltime.month - 1;
	tm.tm_mday	= icaltime.day;
	tm.tm_hour	= icaltime.hour;
	tm.tm_min	= icaltime.minute;
	tm.tm_sec	= icaltime.second;
	tm.tm_isdst	= 0;

	/* TODO: fix this properly when we implement ical2exchange */
#if !defined(__FreeBSD__)
 	if(timezone!=0){
 		tm.tm_hour-=timezone/3600;
 	}
#endif
	time = mktime(&tm);
	unix_to_nt_time(&nttime, time);
	nttime_to_timeval(&t, nttime);
	
	return t.tv_sec/60+WIN32_UNIX_EPOCH_DIFF;
	
}


struct FILETIME get_FILETIME_from_icaltimetype(icaltimetype *tt)
{
	struct FILETIME		ft;
	struct tm		tm;
	time_t			time;
	NTTIME			nttime;
	
	tm.tm_year	= tt->year - 1900;
	tm.tm_mon 	= tt->month - 1;
	tm.tm_mday	= tt->day;
	tm.tm_hour	= tt->hour;
	tm.tm_min	= tt->minute;
	tm.tm_sec	= tt->second;
	tm.tm_isdst	= 0;

	/* TODO: fix this properly when we implement ical2exchange */
#if !defined(__FreeBSD__)
 	if(timezone!=0){
 		tm.tm_hour-=timezone/3600;
 	}
#endif
	time = mktime(&tm);
	unix_to_nt_time(&nttime, time);
	
	ft.dwHighDateTime=(uint32_t) (nttime>>32);
	ft.dwLowDateTime=(uint32_t) (nttime);
	
	return ft;
}


NTTIME FILETIME_to_NTTIME(struct FILETIME ft)
{
	NTTIME nt;
	nt = ft.dwHighDateTime;
	nt = nt << 32;
	nt |= ft.dwLowDateTime;
	return nt;
}

int compare_minutes(const void *min1, const void *min2)
{
	if( *(uint32_t *) min1 > * (uint32_t *) min2) return -1;
	return 1;
}
