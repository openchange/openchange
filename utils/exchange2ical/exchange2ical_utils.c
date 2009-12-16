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

#include <utils/exchange2ical/exchange2ical.h>

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

char *get_ical_date(TALLOC_CTX *mem_ctx, struct SYSTEMTIME *time)
{
	char	*date;

	date = talloc_asprintf(mem_ctx, "%.4d%.2d%.2dT%.2d%.2d%.2d", time->wYear + 1601, time->wMonth,
			       time->wDay, time->wHour, time->wMinute, time->wSecond);
	return date;
}


static struct ical_method	ical_method[] = {
	{ ICAL_METHOD_PUBLISH,	ICAL_PARTSTAT_NONE,	"IPM.Appointment"		},
	{ ICAL_METHOD_REQUEST,	ICAL_PARTSTAT_NONE,	"IPM.Schedule.Meeting.Request"	},
	{ ICAL_METHOD_REPLY,	ICAL_PARTSTAT_ACCEPTED,	"IPM.Schedule.Meeting.Resp.Pos" },
	{ ICAL_METHOD_REPLY,	ICAL_PARTSTAT_TENTATIVE,"IPM.Schedule.Meeting.Resp.Tent"},
	{ ICAL_METHOD_REPLY,	ICAL_PARTSTAT_DECLINED,	"IPM.Schedule.Meeting.Resp.Neg"	},
	{ ICAL_METHOD_CANCEL,	ICAL_PARTSTAT_NONE,	"IPM.Schedule.Meeting.Canceled" },
	{ ICAL_METHOD_NONE,	ICAL_PARTSTAT_NONE,	NULL }
};

enum icalproperty_method get_ical_method(const char *msgclass)
{
	uint32_t	i;

	/* Sanity check */
	if (!msgclass) return ICAL_METHOD_NONE;

	for (i = 0; ical_method[i].PidTagMessageClass; i++) {
		if (!strcmp(msgclass, ical_method[i].PidTagMessageClass)) {
			return ical_method[i].method;
		}
	}

	return ICAL_METHOD_NONE;
}


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

enum icalparameter_partstat get_ical_partstat(const char *msgclass)
{
	uint32_t	i;

	/* Sanity check */
	if (!msgclass) return ICAL_PARTSTAT_NONE;

	for (i = 0; ical_method[i].PidTagMessageClass; i++) {
		if (!strcmp(msgclass, ical_method[i].PidTagMessageClass)) {
			return ical_method[i].partstat;
		}
	}

	return ICAL_PARTSTAT_NONE;
}


static struct ical_class	ical_class[] = {
	{ 0x00000000,	ICAL_CLASS_PUBLIC	},
	{ 0x00000001,	ICAL_CLASS_X		},
	{ 0x00000002,	ICAL_CLASS_PRIVATE	},
	{ 0x00000003,	ICAL_CLASS_CONFIDENTIAL	},
	{ 0x00000000,	ICAL_CLASS_NONE		}
};

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

struct icaltimetype get_icaltimetype_from_tm(struct tm *tm)
{
	icaltimetype tt;

	tt.year   = tm->tm_year;
	tt.month  = tm->tm_mon;
	tt.day    = tm->tm_mday;
	tt.hour   = tm->tm_hour;
	tt.minute = tm->tm_min;
	tt.second = tm->tm_sec;

	tt.is_utc      = 0;
	tt.is_date     = 0;
	tt.is_daylight = 0;
	tt.zone        = 0;

	return tt;
}

struct icaldatetimeperiodtype get_icaldatetimeperiodtype_from_tm(struct tm *tm)
{
	struct icaldatetimeperiodtype tt;

	// TODO: build tt here!
	memset(&tt, 0, sizeof (struct icaldatetimeperiodtype));

	return tt;
}

struct tm *get_tm_from_FILETIME(const struct FILETIME *ft)
{
	NTTIME		time;
	struct timeval	t;
	struct tm	*tm;

	time = ft->dwHighDateTime;
	time = time << 32;
	time |= ft->dwLowDateTime;
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

	nttime = ft->dwHighDateTime;
	nttime = nttime << 32;
	nttime |= ft->dwLowDateTime;
	nttime_to_timeval(&temp_timeval, nttime);
	tm = gmtime(&temp_timeval.tv_sec);

	tt.year   = tm->tm_year + 1900;
	tt.month  = tm->tm_mon + 1;
	tt.day    = tm->tm_mday;
	tt.hour   = tm->tm_hour;
	tt.minute = tm->tm_min;
	tt.second = tm->tm_sec;
	tt.is_date = 0;
	tt.is_utc = 1;
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

	nttime = ft->dwHighDateTime;
	nttime = nttime << 32;
	nttime |= ft->dwLowDateTime;
	nttime_to_timeval(&temp_timeval, nttime);
	tm = gmtime(&temp_timeval.tv_sec);

	tt.year   = tm->tm_year + 1900;
	tt.month  = tm->tm_mon + 1;
	tt.day    = tm->tm_mday;
	tt.hour   = 0;
	tt.minute = 0;
	tt.second = 0;
	tt.is_date = 1;
	tt.is_utc = 1;
	tt.is_daylight = 0;
	tt.zone = NULL;

	return tt;
}
