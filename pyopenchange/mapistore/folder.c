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
	TALLOC_CTX		*mem_ctx;
	PyMAPIStoreFolderObject	*folder;
	char			*kwnames[] = { "name", "description", "foldertype", NULL };
	const char		*name;
	const char		*desc = NULL;
	uint16_t		foldertype = FOLDER_GENERIC;
	uint64_t		fid;
	int			retval;
	struct SRow 		*aRow;
	void			*folder_object;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|sh", kwnames, &name, &desc, &foldertype)) {
		return NULL;
	}

	/* Get FID for the new folder (backend should care about not duplicating folders) */
	retval = mapistore_indexing_get_new_folderID(self->context->mstore_ctx, &fid);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	/* Set the name and properties of the folder */
	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	aRow = talloc_zero(mem_ctx, struct SRow);
	aRow->lpProps = talloc_array(aRow, struct SPropValue, 3);
	aRow->cValues = 0;

	/* We assume the parameters passed by Python are UNICODE */
	aRow->lpProps = add_SPropValue(mem_ctx, aRow->lpProps, &(aRow->cValues),
				       PR_DISPLAY_NAME_UNICODE, (void *)name);
	aRow->lpProps = add_SPropValue(mem_ctx, aRow->lpProps, &(aRow->cValues),
				       PR_COMMENT_UNICODE, (void *)desc);

	aRow->lpProps = add_SPropValue(mem_ctx, aRow->lpProps, &(aRow->cValues),
				       PR_FOLDER_TYPE, (void *)&foldertype);

	retval = mapistore_folder_create_folder(self->context->mstore_ctx, self->context->context_id,
						self->folder_object, self->mem_ctx, fid, aRow, &folder_object);
	talloc_free(mem_ctx);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	folder = PyObject_New(PyMAPIStoreFolderObject, &PyMAPIStoreFolder);

	folder->mem_ctx = self->mem_ctx;
	folder->context = self->context;
	Py_INCREF(folder->context);

	folder->folder_object = folder_object;
	(void) talloc_reference(NULL, folder->folder_object);
	folder->fid = fid;

	return (PyObject *)folder;
}

static PyObject *py_MAPIStoreFolder_open_folder(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	PyMAPIStoreFolderObject	*folder;
	char			*kwnames[] = { "uri", NULL };
	const char		*uri;
	uint64_t		fid;
	bool			soft_deleted, partial;
	int			retval;
	void			*folder_object;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwnames, &uri)) {
		return NULL;
	}

	/* Get the FID from the URI */
	partial = false;	// A full URI is needed
	retval = mapistore_indexing_record_get_fmid(self->context->mstore_ctx, self->context->parent->username,
			uri, partial, &fid, &soft_deleted);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	if (soft_deleted == true) {
		PyErr_SetString(PyExc_SystemError,
				"Soft-deleted folder.");
		return NULL;
	}

	/* Open the folder */
	retval = mapistore_folder_open_folder(self->context->mstore_ctx, self->context->context_id,
						self->folder_object, self->mem_ctx, fid, &folder_object);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	/* Return the folder object */
	folder = PyObject_New(PyMAPIStoreFolderObject, &PyMAPIStoreFolder);

	folder->mem_ctx = self->mem_ctx;
	folder->context = self->context;
	Py_INCREF(folder->context);

	folder->folder_object = folder_object;
	(void) talloc_reference(NULL, folder->folder_object);
	folder->fid = fid;

	return (PyObject *)folder;
}

static PyObject *py_MAPIStoreFolder_delete(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	char				*kwnames[] = { "flags", NULL };
	uint8_t				flags;
	enum mapistore_error		retval;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "H", kwnames, &flags)) {
		return NULL;
	}

	/* Delete the folder (soft or hard deletion depending on the flags) */
	retval = mapistore_folder_delete(self->context->mstore_ctx, self->context->context_id, self->folder_object, flags);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	/* TODO: Handle the Python object */
	Py_RETURN_NONE;
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

static PyObject *py_MAPIStoreFolder_get_child_folders(PyMAPIStoreFolderObject *self)
{
	TALLOC_CTX			*mem_ctx;
	enum mapistore_error		retval;
	uint64_t			*fid_list;
	uint32_t			list_size;
	PyMAPIStoreFoldersObject	*folder_list;

	/* Get the child folders' FIDs */
	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	retval = mapistore_folder_get_child_fmids(self->context->mstore_ctx, self->context->context_id,
			self->folder_object, MAPISTORE_FOLDER_TABLE, mem_ctx, &fid_list, &list_size);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Return the PyMAPIStoreFolders object*/
	folder_list = PyObject_New(PyMAPIStoreFoldersObject, &PyMAPIStoreFolders);

	talloc_reference(self->mem_ctx, fid_list);
	folder_list->mem_ctx = self->mem_ctx;
	folder_list->folder = self;
	Py_INCREF(folder_list->folder);

	folder_list->fids = fid_list;
	folder_list->count = list_size;
	folder_list->curr_index = 0;

	talloc_free(mem_ctx);
	return (PyObject *) folder_list;
}

static PyObject *py_MAPIStoreFolder_get_uri(PyMAPIStoreFolderObject *self)
{
	TALLOC_CTX			*mem_ctx;
	enum mapistore_error 		retval;
	char				*uri;
	bool				soft_deleted;
	PyObject			*py_ret;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	/* Retrieve the URI from the indexing */
	retval = mapistore_indexing_record_get_uri(self->context->mstore_ctx, self->context->parent->username,
			self->mem_ctx, self->fid, &uri, &soft_deleted);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		talloc_free(mem_ctx);
		return NULL;
	}

	if (soft_deleted == true) {
		PyErr_SetString(PyExc_SystemError,
				"Soft-deleted folder.");
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Return the URI */
	py_ret = PyString_FromString(uri);
	talloc_free(mem_ctx);

	return py_ret;
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
	{ "open_folder", (PyCFunction)py_MAPIStoreFolder_open_folder, METH_VARARGS|METH_KEYWORDS },
	{ "delete", (PyCFunction)py_MAPIStoreFolder_delete, METH_VARARGS|METH_KEYWORDS },
	{ "get_child_count", (PyCFunction)py_MAPIStoreFolder_get_child_count, METH_VARARGS|METH_KEYWORDS },
	{ "get_child_folders", (PyCFunction)py_MAPIStoreFolder_get_child_folders, METH_NOARGS },
	{ "get_uri", (PyCFunction)py_MAPIStoreFolder_get_uri, METH_NOARGS},
	{ "fetch_freebusy_properties", (PyCFunction)py_MAPIStoreFolder_fetch_freebusy_properties, METH_VARARGS|METH_KEYWORDS },
	{ NULL },
};

static PyObject *py_MAPIStoreFolder_get_fid(PyMAPIStoreFolderObject *self, void *closure)
{
	return PyLong_FromLongLong(self->fid);
}

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

static void py_MAPIStoreFolders_dealloc(PyObject *_self)
{
	PyMAPIStoreFoldersObject *self = (PyMAPIStoreFoldersObject *)_self;

	talloc_free(self->fids);
	Py_XDECREF(self->folder);

	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreFolders_iter(PyMAPIStoreFoldersObject *self)
{
	return (PyObject *)self;
}

static PyObject *py_MAPIStoreFolders_next(PyMAPIStoreFoldersObject *self)
{
	uint64_t			fid;
	enum mapistore_error		retval;
	void 				*folder_object;
	PyMAPIStoreFolderObject		*folder;

	/* Check if there are remaining folders */
	if(self->curr_index >= self->count) {
		    PyErr_SetNone(PyExc_StopIteration);
		    return NULL;
	}

	/* Retrieve FID and increment curr_index*/
	fid = self->fids[self->curr_index];
	self->curr_index += 1;

	/* Use FID to open folder */
	retval = mapistore_folder_open_folder(self->folder->context->mstore_ctx,
						self->folder->context->context_id,
						self->folder->folder_object,
						self->mem_ctx, fid, &folder_object);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	/* Return the MAPIStoreFolder object */
	folder = PyObject_New(PyMAPIStoreFolderObject, &PyMAPIStoreFolder);

	folder->mem_ctx = self->mem_ctx;
	folder->context = self->folder->context;
	Py_INCREF(folder->context);

	folder->folder_object = folder_object;
	(void) talloc_reference(NULL, folder->folder_object);
	folder->fid = fid;

	return (PyObject *)folder;
}

PyTypeObject PyMAPIStoreFolders = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "child folders",
	.tp_basicsize = sizeof (PyMAPIStoreFoldersObject),
	.tp_iter = py_MAPIStoreFolders_iter,
	.tp_iternext = py_MAPIStoreFolders_next,
	.tp_doc = "iterator over folder child folders",
	.tp_dealloc = (destructor)py_MAPIStoreFolders_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER,
};

void initmapistore_folder(PyObject *m)
{
	if (PyType_Ready(&PyMAPIStoreFolder) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStoreFolder);

	if (PyType_Ready(&PyMAPIStoreFolders) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStoreFolders);

	/* Folder types */
	PyModule_AddObject(m, "FOLDER_GENERIC", PyInt_FromLong(0x1));
	PyModule_AddObject(m, "FOLDER_SEARCH", PyInt_FromLong(0x2));

	/* Table types */
	PyModule_AddObject(m, "FOLDER_TABLE", PyInt_FromLong(0x1));
	PyModule_AddObject(m, "MESSAGE_TABLE", PyInt_FromLong(0x2));

	/* Deletion flags */
	PyModule_AddObject(m, "SOFT_DELETE", PyInt_FromLong(0x1));
	PyModule_AddObject(m, "PERMANENT_DELETE", PyInt_FromLong(0x2));
}
