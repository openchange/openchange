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

#include <mapistore/mapistore.h>

#include <gen_ndr/exchange.h>

#include "pymapistore.h"

struct PyMemberDef PyMAPIStoreFreeBusyProperties_members[] = {
	{ "timestamp", T_OBJECT_EX, offsetof(PyMAPIStoreFreeBusyPropertiesObject, timestamp), RO, "docstring of publish_start" },

	/* start date of the returned ranges */
	{ "publish_start", T_OBJECT_EX, offsetof(PyMAPIStoreFreeBusyPropertiesObject, publish_start), RO, "docstring of publish_start" },
	/* end date of the returned ranges */
	{ "publish_end", T_OBJECT_EX, offsetof(PyMAPIStoreFreeBusyPropertiesObject, publish_end), RO, "docstring of publish_end" },

	/* tuples of tuples of date ranges, based on event/participation status */
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
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

static PyObject *make_datetime_from_nttime(NTTIME nt_time)
{
	time_t		unix_time;

	unix_time = nt_time_to_unix(nt_time);

	return PyObject_CallMethod(datetime_datetime_class, "utcfromtimestamp", "i", unix_time);
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

static PyObject *make_range_tuple_from_range(struct mapistore_freebusy_properties *fb_props, uint16_t *minutes_range_start)
{
	PyObject *range_tuple, *datetime;
	uint32_t date_min;

	range_tuple = PyTuple_New(2);
	date_min = fb_props->publish_start + minutes_range_start[0];
	datetime = make_datetime_from_minutes(date_min);
	PyTuple_SET_ITEM(range_tuple, 0, datetime);

	date_min = fb_props->publish_start + minutes_range_start[1];
	datetime = make_datetime_from_minutes(date_min);
	PyTuple_SET_ITEM(range_tuple, 1, datetime);

	return range_tuple;
}

static PyObject *make_fb_tuple(struct mapistore_freebusy_properties *fb_props, struct Binary_r *ranges)
{
	int i, nbr_ranges;
	uint16_t *minutes_range_start;
	PyObject *tuple, *range_tuple;

	nbr_ranges = ranges->cb / (2 * sizeof(uint16_t));
	minutes_range_start = (uint16_t *) ranges->lpb;

	tuple = PyTuple_New(nbr_ranges);
	for (i = 0; i < nbr_ranges; i++) {
		range_tuple = make_range_tuple_from_range(fb_props, minutes_range_start);
		PyTuple_SET_ITEM(tuple, i, range_tuple);
		minutes_range_start += 2;
	}

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

void initmapistore_freebusy_properties(PyObject *parent_module)
{
	if (PyType_Ready(&PyMAPIStoreFreeBusyProperties) < 0) {
		return;
	}
}
