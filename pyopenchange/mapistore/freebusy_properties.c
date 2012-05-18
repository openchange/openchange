/*
   OpenChange MAPI implementation.

   Python interface to mapistore folder

   Copyright (C) Wolfgang Sourdeau 2012.

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

#include <Python.h>
#include <structmember.h>

#include "mapiproxy/libmapistore/mapistore.h"

#include <gen_ndr/exchange.h>

#include "pymapistore.h"

static void py_MAPIStoreFreeBusyProperties_dealloc(PyObject *_self);

struct PyMemberDef PyMAPIStoreFreeBusyProperties_members[] = {
	{ "timestamp", T_OBJECT_EX, offsetof(PyMAPIStoreFreeBusyPropertiesObject, timestamp), RO, "docstring of publish_start" },

	/* start date of the returned ranges */
	{ "publish_start", T_OBJECT_EX, offsetof(PyMAPIStoreFreeBusyPropertiesObject, publish_start), RO, "docstring of publish_start" },
	/* end date of the returned ranges */
	{ "publish_end", T_OBJECT_EX, offsetof(PyMAPIStoreFreeBusyPropertiesObject, publish_end), RO, "docstring of publish_end" },

	/* tuples of tuples of date ranges, based on event/participation status */
	{ "free", T_OBJECT_EX, offsetof(PyMAPIStoreFreeBusyPropertiesObject, free), RO, "docstring of free" },
	{ "tentative", T_OBJECT_EX, offsetof(PyMAPIStoreFreeBusyPropertiesObject, tentative), RO, "docstring of tentative" },
	{ "busy", T_OBJECT_EX, offsetof(PyMAPIStoreFreeBusyPropertiesObject, busy), RO, "docstring of busy" },
	{ "away", T_OBJECT_EX, offsetof(PyMAPIStoreFreeBusyPropertiesObject, away), RO, "docstring of away" },

	/* combination of the above tuples */
	{ "merged", T_OBJECT_EX, offsetof(PyMAPIStoreFreeBusyPropertiesObject, merged), RO, "docstring of merged" },

	{ NULL }
};

PyTypeObject PyMAPIStoreFreeBusyProperties = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "FreeBusyProperties",
	.tp_members = PyMAPIStoreFreeBusyProperties_members,
	.tp_basicsize = sizeof (PyMAPIStoreFreeBusyPropertiesObject),
	.tp_doc = "mapistore freebusy properties object",
	.tp_dealloc = (destructor)py_MAPIStoreFreeBusyProperties_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

static PyObject *make_datetime_from_nttime(NTTIME nt_time)
{
	time_t		unix_time;
	PyMAPIStoreGlobals *globals;

	unix_time = nt_time_to_unix(nt_time);
	globals = get_PyMAPIStoreGlobals();

	return PyObject_CallMethod(globals->datetime_datetime_class, "utcfromtimestamp", "i", unix_time);
}

static PyObject *make_datetime_from_filetime(struct FILETIME *filetime)
{
	NTTIME		nt_time;

	nt_time = ((NTTIME) filetime->dwHighDateTime << 32) | filetime->dwLowDateTime;

	return make_datetime_from_nttime(nt_time);
}

static PyObject *make_datetime_from_minutes(uint32_t minutes)
{
	NTTIME		nt_time;

	nt_time = (NTTIME) minutes * (60 * 10000000);

	return make_datetime_from_nttime(nt_time);
}

static PyObject *make_datetime_from_ymon_and_minutes(uint32_t ymon, uint16_t offset_mins)
{
	struct tm tm;
	int year, hours;
	time_t unix_time;
	PyMAPIStoreGlobals *globals;

	memset(&tm, 0, sizeof(struct tm));
	year = ymon >> 4;
	tm.tm_year = year - 1900;
	tm.tm_mon = (ymon & 0x000f) - 1;

	tm.tm_min = offset_mins % 60;
	hours = offset_mins / 60;
	tm.tm_mday = 1 + (hours / 24);
	tm.tm_hour = hours % 24;

	unix_time = mktime(&tm);

	globals = get_PyMAPIStoreGlobals();

	return PyObject_CallMethod(globals->datetime_datetime_class, "utcfromtimestamp", "i", unix_time);
}

static PyObject *make_range_tuple_from_range(uint32_t ymon, uint16_t *minutes_range_start)
{
	PyObject *range_tuple, *datetime;

	range_tuple = PyTuple_New(2);
	datetime = make_datetime_from_ymon_and_minutes(ymon, minutes_range_start[0]);
	PyTuple_SET_ITEM(range_tuple, 0, datetime);
	datetime = make_datetime_from_ymon_and_minutes(ymon, minutes_range_start[1]);
	PyTuple_SET_ITEM(range_tuple, 1, datetime);

	return range_tuple;
}

static PyObject *make_fb_tuple(struct mapistore_freebusy_properties *fb_props, struct Binary_r *ranges)
{
	int i, j, range_nbr, nbr_ranges, nbr_minute_ranges;
	struct Binary_r *current_ranges;
	uint16_t *minutes_range_start;
	PyObject *tuple, *range_tuple;
	char *tz;

	nbr_ranges = 0;
	for (i = 0; i < fb_props->nbr_months; i++) {
		current_ranges = ranges + i;
		nbr_ranges += (current_ranges->cb / (2 * sizeof(uint16_t)));
	}

	tz = getenv("TZ");
	setenv("TZ", "", 1);
	tzset();

	tuple = PyTuple_New(nbr_ranges);
	range_nbr = 0;
	for (i = 0; i < fb_props->nbr_months; i++) {
		current_ranges = ranges + i;
		minutes_range_start = (uint16_t *) current_ranges->lpb;
		nbr_minute_ranges = (current_ranges->cb / (2 * sizeof(uint16_t)));
		for (j = 0; j < nbr_minute_ranges; j++) {
 			range_tuple = make_range_tuple_from_range(fb_props->months_ranges[i], minutes_range_start);
			PyTuple_SET_ITEM(tuple, range_nbr, range_tuple);
			minutes_range_start += 2;
			range_nbr++;
		}
	}

	if (tz) {
		setenv("TZ", tz, 1);
	}
	else {
		unsetenv("TZ");
	}
	tzset();

	return tuple;
}

PyMAPIStoreFreeBusyPropertiesObject* instantiate_freebusy_properties(struct mapistore_freebusy_properties *fb_props)
{
	PyMAPIStoreFreeBusyPropertiesObject	*fb_props_object;
	PyObject				 *value;

	fb_props_object = PyObject_New(PyMAPIStoreFreeBusyPropertiesObject, &PyMAPIStoreFreeBusyProperties);

	value = make_datetime_from_filetime(&fb_props->timestamp);
	fb_props_object->timestamp = value;
	Py_INCREF(value);

	value = make_datetime_from_minutes(fb_props->publish_start);
	fb_props_object->publish_start = value;
	Py_INCREF(value);
	value = make_datetime_from_minutes(fb_props->publish_end);
	fb_props_object->publish_end = value;
	Py_INCREF(value);

	value = make_fb_tuple(fb_props, fb_props->freebusy_free);
	fb_props_object->free = value;
	Py_INCREF(value);
	value = make_fb_tuple(fb_props, fb_props->freebusy_tentative);
	fb_props_object->tentative = value;
	Py_INCREF(value);
	value = make_fb_tuple(fb_props, fb_props->freebusy_busy);
	fb_props_object->busy = value;
	Py_INCREF(value);
	value = make_fb_tuple(fb_props, fb_props->freebusy_away);
	fb_props_object->away = value;
	Py_INCREF(value);
	value = make_fb_tuple(fb_props, fb_props->freebusy_merged);
	fb_props_object->merged = value;
	Py_INCREF(value);

	return fb_props_object;
}

static void py_MAPIStoreFreeBusyProperties_dealloc(PyObject *_self)
{
	PyMAPIStoreFreeBusyPropertiesObject *self = (PyMAPIStoreFreeBusyPropertiesObject *)_self;

	Py_XDECREF(self->timestamp);
	Py_XDECREF(self->publish_start);
	Py_XDECREF(self->publish_end);
	Py_XDECREF(self->free);
	Py_XDECREF(self->tentative);
	Py_XDECREF(self->busy);
	Py_XDECREF(self->away);
	Py_XDECREF(self->merged);

	PyObject_Del(_self);
}

void initmapistore_freebusy_properties(PyObject *parent_module)
{
	if (PyType_Ready(&PyMAPIStoreFreeBusyProperties) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStoreFreeBusyProperties);
}
