/*
   OpenChange MAPI implementation.

   Python interface to mapistore folder

   Copyright (C) Julien Kerihuel 2011.

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

/* config.h defines the _GNU_SOURCE marco needed for the tm_gmtoff and
   tm_zone members of struct tm. */
#include "config.h"
#include <time.h>
#include <Python.h>
#include "pyopenchange/mapistore/pymapistore.h"
#include "gen_ndr/exchange.h"

static void py_MAPIStoreFolder_dealloc(PyObject *_self)
{
	PyMAPIStoreFolderObject *self = (PyMAPIStoreFolderObject *)_self;

	talloc_unlink(NULL, self->folder_object);

	Py_XDECREF(self->context);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreFolder_create_folder(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	PyMAPIStoreFolderObject	*folder;
	char			*kwnames[] = { "name", "description", "foldertype", "flags", NULL };
	const char		*name;
	const char		*desc = NULL;
	uint16_t		foldertype = FOLDER_GENERIC;
	uint16_t		flags = NONE;
	uint64_t		fid;
	int			retval;
	struct SRow 		*aRow;
	void			*folder_object;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|shh", kwnames, &name, &desc, &foldertype, &flags)) {
		return NULL;
	}

	/* Get FID for the new folder (backend should care about not duplicating folders) */
	retval = mapistore_indexing_get_new_folderID(self->context->mstore_ctx, &fid);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	aRow = talloc_zero(self->mem_ctx, struct SRow);
	aRow->lpProps = talloc_array(aRow, struct SPropValue, 3);
	aRow->cValues = 0;

	/* We assume the parameters passed by Python are UNICODE */
	aRow->lpProps = add_SPropValue(self->mem_ctx, aRow->lpProps, &(aRow->cValues),
				       PR_DISPLAY_NAME_UNICODE, (void *)name);
	aRow->lpProps = add_SPropValue(self->mem_ctx, aRow->lpProps, &(aRow->cValues),
				       PR_COMMENT_UNICODE, (void *)desc);

	aRow->lpProps = add_SPropValue(self->mem_ctx, aRow->lpProps, &(aRow->cValues),
				       PR_FOLDER_TYPE, (void *)&foldertype);

	retval = mapistore_folder_create_folder(self->context->mstore_ctx, self->context->context_id,
						self->folder_object, self->mem_ctx, fid, aRow, &folder_object);
	talloc_free(aRow);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	folder = PyObject_New(PyMAPIStoreFolderObject, &PyMAPIStoreFolder);

	folder->mem_ctx = self->mem_ctx;
	folder->context = self->context;
	Py_INCREF(folder->context);

	folder->folder_object = self->folder_object;
	(void) talloc_reference(NULL, folder->folder_object);
	folder->fid = fid;

	return (PyObject *)folder;
}

static PyObject *py_MAPIStoreFolder_get_fid(PyMAPIStoreFolderObject *self, void *closure)
{
	return PyLong_FromLongLong(self->fid);
}

static PyObject *py_MAPIStoreFolder_get_child_count(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	uint32_t			RowCount;
	enum mapistore_table_type	table_type;
	char				*kwnames[] = { "table_type", NULL };
	int				retval;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwnames, &table_type)) {
		return NULL;
	}

	retval = mapistore_folder_get_child_count(self->context->mstore_ctx, self->context->context_id,
						  self->folder_object, table_type, &RowCount);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	return PyInt_FromLong(RowCount);
}

static void convert_datetime_to_tm(TALLOC_CTX *mem_ctx, PyObject *datetime, struct tm *tm)
{
	PyObject *value;

	value = PyObject_GetAttrString(datetime, "year");
	tm->tm_year = PyInt_AS_LONG(value) - 1900;
	value = PyObject_GetAttrString(datetime, "month");
	tm->tm_mon = PyInt_AS_LONG(value) - 1;
	value = PyObject_GetAttrString(datetime, "day");
	tm->tm_mday = PyInt_AS_LONG(value);
	value = PyObject_GetAttrString(datetime, "hour");
	tm->tm_hour = PyInt_AS_LONG(value);
	value = PyObject_GetAttrString(datetime, "minute");
	tm->tm_min = PyInt_AS_LONG(value);
	value = PyObject_GetAttrString(datetime, "second");
	tm->tm_sec = PyInt_AS_LONG(value);

	value = PyObject_CallMethod(datetime, "utcoffset", NULL);
	if (value && value != Py_None) {
		tm->tm_gmtoff = PyInt_AS_LONG(value);
	}
	else {
		tm->tm_gmtoff = 0;
	}
	value = PyObject_CallMethod(datetime, "tzname", NULL);
	if (value && value != Py_None) {
		tm->tm_zone = talloc_strdup(mem_ctx, PyString_AsString(value));
	}
	else {
		tm->tm_zone = NULL;
	}
}

static PyObject *py_MAPIStoreFolder_fetch_freebusy_properties(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX		*mem_ctx;
	char			*kwnames[] = { "startdate", "enddate", NULL };
	PyObject		*start = NULL, *end = NULL, *result = NULL;
	struct tm		*start_tm, *end_tm;
	enum mapistore_error	retval;
	struct mapistore_freebusy_properties *fb_props;
	PyMAPIStoreGlobals *globals;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|OO", kwnames, &start, &end)) {
		return NULL;
	}

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	globals = get_PyMAPIStoreGlobals();
	if (start) {
		if (!PyObject_IsInstance(start, globals->datetime_datetime_class)) {
			PyErr_SetString(PyExc_TypeError, "'start' must either be a datetime.datetime instance or None");
			goto end;
		}
		start_tm = talloc_zero(mem_ctx, struct tm);
		convert_datetime_to_tm(mem_ctx, start, start_tm);
	}
	else {
		start_tm = NULL;
	}

	if (end) {
		if (!PyObject_IsInstance(end, globals->datetime_datetime_class)) {
			PyErr_SetString(PyExc_TypeError, "'end' must either be a datetime.datetime instance or None");
			goto end;
		}
		end_tm = talloc_zero(mem_ctx, struct tm);
		convert_datetime_to_tm(mem_ctx, end, end_tm);
	}
	else {
		end_tm = NULL;
	}

	retval = mapistore_folder_fetch_freebusy_properties(self->context->mstore_ctx, self->context->context_id, self->folder_object, start_tm, end_tm, mem_ctx, &fb_props);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		goto end;
	}
	result = (PyObject *) instantiate_freebusy_properties(fb_props);

end:
	talloc_free(mem_ctx);

	return result;
}

static PyMethodDef mapistore_folder_methods[] = {
	{ "create_folder", (PyCFunction)py_MAPIStoreFolder_create_folder, METH_VARARGS|METH_KEYWORDS },
	{ "get_child_count", (PyCFunction)py_MAPIStoreFolder_get_child_count, METH_VARARGS|METH_KEYWORDS },
	{ "fetch_freebusy_properties", (PyCFunction)py_MAPIStoreFolder_fetch_freebusy_properties, METH_VARARGS|METH_KEYWORDS },
	{ NULL },
};

static PyGetSetDef mapistore_folder_getsetters[] = {
	{ (char *)"fid", (getter)py_MAPIStoreFolder_get_fid, NULL, NULL },
	{ NULL }
};

PyTypeObject PyMAPIStoreFolder = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapistore folder",
	.tp_basicsize = sizeof (PyMAPIStoreFolderObject),
	.tp_methods = mapistore_folder_methods,
	.tp_getset = mapistore_folder_getsetters,
	.tp_doc = "mapistore folder object",
	.tp_dealloc = (destructor)py_MAPIStoreFolder_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

void initmapistore_folder(PyObject *m)
{
	if (PyType_Ready(&PyMAPIStoreFolder) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStoreFolder);

	PyModule_AddObject(m, "FOLDER_GENERIC", PyInt_FromLong(0x1));
	PyModule_AddObject(m, "FOLDER_SEARCH", PyInt_FromLong(0x2));

	PyModule_AddObject(m, "NONE", PyInt_FromLong(0x0));
	PyModule_AddObject(m, "OPEN_IF_EXISTS", PyInt_FromLong(0x1));
}
