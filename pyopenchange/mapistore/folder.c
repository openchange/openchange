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
#include <string.h>

static void py_MAPIStoreFolder_dealloc(PyObject *_self)
{
	PyMAPIStoreFolderObject *self = (PyMAPIStoreFolderObject *)_self;

	Py_XDECREF(self->context);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreFolder_create_folder(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX		*mem_ctx;
	char			*kwnames[] = { "name", "description", "foldertype", NULL };
	PyMAPIStoreFolderObject	*folder;
	struct SRow 		*aRow;
	void			*folder_object;
	const char		*name;
	const char		*desc = NULL;
	enum mapistore_error	retval;
	uint64_t		fid;
	uint16_t		foldertype = FOLDER_GENERIC;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|sH", kwnames, &name, &desc, &foldertype)) {
		return NULL;
	}

	/* Check 'foldertype' range */
	if ((foldertype < FOLDER_GENERIC) || (foldertype > FOLDER_SEARCH)) {
		DEBUG(0, ("[ERR][%s]: 'foldertype' argument out of range\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
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
	if (aRow == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	aRow->lpProps = talloc_array(aRow, struct SPropValue, 3);
	if (aRow->lpProps == NULL) {
		PyErr_NoMemory();
		goto end;
	}

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

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		goto end;
	}

	folder = PyObject_New(PyMAPIStoreFolderObject, &PyMAPIStoreFolder);
	if (folder == NULL) {
		PyErr_NoMemory();
		goto end;
	}

	folder->mem_ctx = self->mem_ctx;
	folder->context = self->context;
	Py_INCREF(folder->context);

	folder->folder_object = folder_object;
	folder->fid = fid;

	return (PyObject *)folder;
end:
	talloc_free(mem_ctx);
	return NULL;
}

static PyObject *py_MAPIStoreFolder_open_folder(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	char			*kwnames[] = { "uri", NULL };
	PyMAPIStoreFolderObject	*folder;
	void			*folder_object;
	const char		*uri;
	enum mapistore_error	retval;
	uint64_t		fid;
	bool			soft_deleted, partial;

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
		DEBUG(0, ("[ERR][%s]: Soft-deleted folder\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_DATA);
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
	if (folder == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	folder->mem_ctx = self->mem_ctx;
	folder->context = self->context;
	Py_INCREF(folder->context);

	folder->folder_object = folder_object;
	folder->fid = fid;

	return (PyObject *)folder;
}

static PyObject *py_MAPIStoreFolder_delete(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	char				*kwnames[] = { "flags", NULL };
	enum mapistore_error		retval;
	uint16_t			flags = 0x0;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|H", kwnames, &flags)) {
		return NULL;
	}

	/* Check 'flags' range */
	if ((flags < 1) && (flags > 5)) {
		DEBUG(0, ("[ERR][%s]: Argument 'flags' out of range\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	/* Delete the folder (soft or hard deletion depending on the flags) */
	retval = mapistore_folder_delete(self->context->mstore_ctx, self->context->context_id, self->folder_object, flags);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject *py_MAPIStoreFolder_copy_folder(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	char				*kwnames[] = { "target_folder", "new_name", "recursive", NULL };
	PyMAPIStoreFolderObject		*target_folder;
	const char			*new_name;
	enum mapistore_error		retval;
	uint16_t			recursive = 0x1;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Os|H", kwnames, &target_folder, &new_name, &recursive)) {
		return NULL;
	}

	/* Check target_folder type */
	if (strcmp("mapistorefolder", target_folder->ob_type->tp_name) != 0) {
		DEBUG(0,("[ERR][%s]: Target folder must be a PyMAPIStoreFolder object\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	/* Check 'recursive' range */
	if ((recursive < 0) || (recursive > 1)) {
		DEBUG(0, ("[ERR][%s]: Argument 'recursive' out of range\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	retval = mapistore_folder_copy_folder(self->context->mstore_ctx, self->context->context_id,
			self->folder_object, target_folder->folder_object, self->mem_ctx,
			(bool) recursive, new_name);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject *py_MAPIStoreFolder_move_folder(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	char				*kwnames[] = { "target_folder", "new_name", NULL };
	PyMAPIStoreFolderObject		*target_folder;
	const char			*new_name;
	enum mapistore_error		retval;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Os", kwnames, &target_folder, &new_name)) {
		return NULL;
	}

	/* Check target_folder type */
	if (strcmp("mapistorefolder", target_folder->ob_type->tp_name) != 0) {
		DEBUG(0, ("[ERR][%s]: Target folder must be a PyMAPIStoreFolder object\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	retval = mapistore_folder_move_folder(self->context->mstore_ctx, self->context->context_id,
			self->folder_object, target_folder->folder_object, target_folder->mem_ctx, new_name);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	Py_RETURN_NONE;
}
static PyObject *py_MAPIStoreFolder_get_child_count(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	char				*kwnames[] = { "table_type", NULL };
	uint32_t			RowCount;
	uint16_t			table_type;
	int				retval;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "H", kwnames, &table_type)) {
		return NULL;
	}

	/* Check 'table_type' range */
	if ((table_type < MAPISTORE_FOLDER_TABLE) || (table_type > MAPISTORE_PERMISSIONS_TABLE)) {
		DEBUG(0, ("[ERR][%s]: 'table_type' argument out of range\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	retval = mapistore_folder_get_child_count(self->context->mstore_ctx, self->context->context_id,
						  self->folder_object, (enum mapistore_table_type) table_type,
						  &RowCount);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	return PyInt_FromLong(RowCount);
}

static PyObject *py_MAPIStoreFolder_get_properties(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	char				*kwnames[] = { "list", NULL };
	PyObject			*list = NULL, *py_ret = NULL;
	enum mapistore_error		retval;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O", kwnames, &list)) {
		return NULL;
	}

	retval = pymapistore_get_properties(list, self->context->mstore_ctx,
			self->context->context_id, self->folder_object, &py_ret);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	return py_ret;
}

static PyObject *py_MAPIStoreFolder_set_properties(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	char			*kwnames[] = { "dict", NULL };
	PyObject		*dict = NULL;
	enum mapistore_error	retval;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwnames, &dict)) {
		return NULL;
	}

	retval = pymapistore_set_properties(dict, self->context->mstore_ctx, self->context->context_id, self->folder_object);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	Py_RETURN_NONE;
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

static PyObject *py_MAPIStoreFolder_create_message(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	PyMAPIStoreMessageObject	*message;
	char				*kwnames[] = { "associated", NULL };
	void				*message_object;
	enum mapistore_error		retval;
	uint64_t			mid;
	uint16_t			associated = 0x0;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|H", kwnames, &associated)) {
		return NULL;
	}

	/* Check 'associated' range */
	if ((associated < 0) || (associated > 1)) {
		DEBUG(0,("[ERR][%s]: 'associated' argument out of range\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	/* Get the MID for the new message */
	retval = mapistore_indexing_reserve_fmid_range(self->context->mstore_ctx, 1, &mid);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	/* Create message */
	retval = mapistore_folder_create_message(self->context->mstore_ctx, self->context->context_id,
						self->folder_object, self->mem_ctx, mid, (uint8_t) associated,
						&message_object);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	/* Return the message object */
	message = PyObject_New(PyMAPIStoreMessageObject, &PyMAPIStoreMessage);
	if (message == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	message->mem_ctx = self->mem_ctx;
	message->context = self->context;
	Py_INCREF(message->context);

	message->message_object = message_object;
	message->mid = mid;

	return (PyObject *)message;
}

static PyObject *py_MAPIStoreFolder_open_message(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	PyMAPIStoreMessageObject	*message;
	char				*kwnames[] = { "uri", "read_write", NULL };
	void				*message_object;
	const char			*uri;
	enum mapistore_error		retval;
	uint64_t			mid;
	uint16_t			read_write = 0x0;
	bool				soft_deleted, partial;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|H", kwnames, &uri, &read_write)) {
		return NULL;
	}

	/* Check 'read_write' range */
	if ((read_write < 0) || (read_write > 1)) {
		DEBUG(0, ("[ERR][%s]: 'read_write' argument out of range\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	/* Get the MID from the URI */
	partial = false;	// A full URI is needed
	retval = mapistore_indexing_record_get_fmid(self->context->mstore_ctx, self->context->parent->username,
			uri, partial, &mid, &soft_deleted);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	if (soft_deleted == true) {
		DEBUG(0, ("[ERR][%s]: Soft-deleted message\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_DATA);
		return NULL;
	}

	/* Open the message (read-only by default) */
	retval = mapistore_folder_open_message(self->context->mstore_ctx, self->context->context_id,
			self->folder_object, self->mem_ctx, mid, (bool) read_write, &message_object);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	/* Return the message object */
	message = PyObject_New(PyMAPIStoreMessageObject, &PyMAPIStoreMessage);
	if (message == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	message->mem_ctx = self->mem_ctx;
	message->context = self->context;
	Py_INCREF(message->context);

	message->message_object = message_object;
	message->mid = mid;

	return (PyObject *)message;
}

static PyObject *py_MAPIStoreFolder_delete_message(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	char				*kwnames[] = { "uri", "flags", NULL };
	const char			*uri;
	enum mapistore_error		retval;
	uint64_t			mid;
	uint16_t			flags = MAPISTORE_PERMANENT_DELETE;
	bool				partial, soft_deleted;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|H", kwnames, &uri, &flags)) {
		return NULL;
	}

	/* Check 'flags' range */
	if ((flags < MAPISTORE_SOFT_DELETE) || (flags > MAPISTORE_PERMANENT_DELETE)) {
		DEBUG(0, ("[ERR][%s]: 'table_type' argument out of range\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	/* Get the MID from the URI */
	partial = false;	// A full URI is needed
	retval = mapistore_indexing_record_get_fmid(self->context->mstore_ctx, self->context->parent->username,
			uri, partial, &mid, &soft_deleted);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	if ((soft_deleted == true) && (flags == MAPISTORE_SOFT_DELETE)) {
		DEBUG(0, ("[ERR][%s]: Already soft-deleted\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_DATA);
		return NULL;
	}

	/* Delete the message (soft/hard delete depending on the flags)*/
	retval = mapistore_folder_delete_message(self->context->mstore_ctx, self->context->context_id,
						self->folder_object, mid, (uint8_t) flags);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject *py_MAPIStoreFolder_copy_messages(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX			*mem_ctx;
	char				*kwnames[] = { "uri_list", "target_folder", NULL };
	PyMAPIStoreFolderObject 	*target_folder;
	PyObject			*uri_list, *item;
	char				*uri;
	enum mapistore_error		retval;
	uint64_t			*source_mids, *target_mids, mid;
	Py_ssize_t			count, i;
	bool				partial = false, soft_deleted; // We use full URIs

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO", kwnames, &uri_list, &target_folder)) {
		return NULL;
	}

	/* Check the arguments' types */
	if (PyList_Check(uri_list) == false) {
		DEBUG(0, ("[ERR][%s]: 'message_list' must be a list\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	if (strcmp("mapistorefolder", target_folder->ob_type->tp_name) != 0) {
		DEBUG(0, ("[ERR][%s]: Target folder must be a PyMAPIStoreFolder object\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	/* Build source MID list */
	count = PyList_Size(uri_list);

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	source_mids = talloc_array(mem_ctx, uint64_t,count);
	if (source_mids == NULL) {
		PyErr_NoMemory();
		goto end;
	}

	for (i = 0; i < count; i++) {
		item = PyList_GetItem(uri_list, i);

		if (PyString_Check(item) == false) {
			DEBUG(0, ("[ERR][%s]: Argument 'uri_list' must only contain strings\n", __location__));
			PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
			goto end;
		}

		uri = PyString_AsString(item);

		retval = mapistore_indexing_record_get_fmid(self->context->mstore_ctx, self->context->parent->username,
				uri, partial, &mid, &soft_deleted);
		if (retval != MAPISTORE_SUCCESS) {
			PyErr_SetMAPIStoreError(retval);
			goto end;
		}

		if (soft_deleted == true) {
			DEBUG(0, ("[ERR][%s]: Soft-deleted message\n", __location__));
			PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_DATA);
			goto end;
		}

		source_mids[i] = mid;
	}

	/* Build target MID list */
	target_mids = talloc_array(mem_ctx, uint64_t, count);
	if (target_mids == NULL) {
		PyErr_NoMemory();
		goto end;
	}

	retval = mapistore_indexing_reserve_fmid_range(self->context->mstore_ctx, count, target_mids);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		goto end;
	}

	/* Move/copy messages */
	retval = mapistore_folder_move_copy_messages(self->context->mstore_ctx, self->context->context_id,
			target_folder->folder_object, self->folder_object, target_folder->mem_ctx, count,
			source_mids, target_mids, NULL, true);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		goto end;
	}

	talloc_free(mem_ctx);
	Py_RETURN_NONE;
end:
	talloc_free(mem_ctx);
	return NULL;
}

static PyObject *py_MAPIStoreFolder_move_messages(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX			*mem_ctx;
	char				*kwnames[] = { "uri_list", "target_folder", NULL };
	PyMAPIStoreFolderObject 	*target_folder;
	PyObject			*uri_list, *item;
	char				*uri;
	enum mapistore_error		retval;
	uint64_t			*source_mids, *target_mids, mid;
	Py_ssize_t			count, i;
	bool				partial = false, soft_deleted; // We use full URIs

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO", kwnames, &uri_list, &target_folder)) {
		return NULL;
	}

	/* Check the arguments' types */
	if (PyList_Check(uri_list) == false) {
		DEBUG(0, ("[ERR][%s]: 'message_list' must be a list\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	if (strcmp("mapistorefolder", target_folder->ob_type->tp_name) != 0) {
		DEBUG(0, ("[ERR][%s]: Target folder must be a PyMAPIStoreFolder object\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	/* Build source MID list */
	count = PyList_Size(uri_list);

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	source_mids = talloc_array(mem_ctx, uint64_t,count);
	if (source_mids == NULL) {
		PyErr_NoMemory();
		goto end;
	}

	for (i = 0; i < count; i++) {
		item = PyList_GetItem(uri_list, i);

		if (PyString_Check(item) == false) {
			DEBUG(0, ("[ERR][%s]: Argument 'uri_list' must only contain strings\n", __location__));
			PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
			goto end;
		}

		uri = PyString_AsString(item);

		retval = mapistore_indexing_record_get_fmid(self->context->mstore_ctx, self->context->parent->username,
				uri, partial, &mid, &soft_deleted);
		if (retval != MAPISTORE_SUCCESS) {
			PyErr_SetMAPIStoreError(retval);
			goto end;
		}

		if (soft_deleted == true) {
			DEBUG(0,("[ERR][%s]: Soft-deleted message: %s\n", __location__, uri));
			PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_DATA);
			goto end;
		}

		source_mids[i] = mid;
	}

	/* Build target MID list */
	target_mids = talloc_array(mem_ctx, uint64_t, count);
	if (target_mids == NULL) {
		PyErr_NoMemory();
		goto end;
	}

	retval = mapistore_indexing_reserve_fmid_range(self->context->mstore_ctx, count, target_mids);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		goto end;
	}

	/* Move/copy messages */
	retval = mapistore_folder_move_copy_messages(self->context->mstore_ctx, self->context->context_id,
			target_folder->folder_object, self->folder_object, target_folder->mem_ctx, count,
			source_mids, target_mids, NULL, false);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		goto end;
	}

	talloc_free(mem_ctx);
	Py_RETURN_NONE;
end:
	talloc_free(mem_ctx);
	return NULL;
}

static PyObject *py_MAPIStoreFolder_open_table(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	char				*kwnames[] = { "table_type", NULL };
	PyMAPIStoreTableObject		*table;
	void				*table_object;
	struct SPropTagArray		*columns;
	uint16_t			table_type;
	enum mapistore_error		retval;
	uint32_t			count;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "H", kwnames, &table_type)) {
		return NULL;
	}

	/* Check 'table_type' range */
	if ((table_type < MAPISTORE_FOLDER_TABLE) || (table_type > MAPISTORE_PERMISSIONS_TABLE)) {
		DEBUG(0, ("[ERR][%s]: 'table_type' argument out of range\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	/* Open the table */
	retval = mapistore_folder_open_table(self->context->mstore_ctx, self->context->context_id,
						self->folder_object, self->mem_ctx,
						(enum mapistore_table_type) table_type, 0,
						&table_object, &count);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	/* Initialise the SPropTagArray */
	columns = talloc_zero(self->mem_ctx, struct SPropTagArray);
	if (columns == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	columns->aulPropTag = talloc_zero(columns, void);
	if (columns->aulPropTag == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	/* Return the table object */
	table = PyObject_New(PyMAPIStoreTableObject, &PyMAPIStoreTable);
	if (table == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	table->mem_ctx = self->mem_ctx;
	table->context = self->context;
	Py_INCREF(table->context);

	table->table_object = table_object;
	table->columns = columns;

	return (PyObject *)table;
}
static PyMethodDef mapistore_folder_methods[] = {
	{ "create_folder", (PyCFunction)py_MAPIStoreFolder_create_folder, METH_VARARGS|METH_KEYWORDS },
	{ "open_folder", (PyCFunction)py_MAPIStoreFolder_open_folder, METH_VARARGS|METH_KEYWORDS },
	{ "delete", (PyCFunction)py_MAPIStoreFolder_delete, METH_VARARGS|METH_KEYWORDS },
	{ "copy_folder", (PyCFunction)py_MAPIStoreFolder_copy_folder, METH_VARARGS|METH_KEYWORDS },
	{ "move_folder", (PyCFunction)py_MAPIStoreFolder_move_folder, METH_VARARGS|METH_KEYWORDS },
	{ "get_child_count", (PyCFunction)py_MAPIStoreFolder_get_child_count, METH_VARARGS|METH_KEYWORDS },
	{ "get_properties", (PyCFunction)py_MAPIStoreFolder_get_properties, METH_VARARGS|METH_KEYWORDS},
	{ "set_properties", (PyCFunction)py_MAPIStoreFolder_set_properties, METH_VARARGS|METH_KEYWORDS},
	{ "fetch_freebusy_properties", (PyCFunction)py_MAPIStoreFolder_fetch_freebusy_properties, METH_VARARGS|METH_KEYWORDS },
	{ "create_message", (PyCFunction)py_MAPIStoreFolder_create_message, METH_VARARGS|METH_KEYWORDS },
	{ "open_message", (PyCFunction)py_MAPIStoreFolder_open_message, METH_VARARGS|METH_KEYWORDS },
	{ "delete_message", (PyCFunction)py_MAPIStoreFolder_delete_message, METH_VARARGS|METH_KEYWORDS },
	{ "copy_messages", (PyCFunction)py_MAPIStoreFolder_copy_messages, METH_VARARGS|METH_KEYWORDS },
	{ "move_messages", (PyCFunction)py_MAPIStoreFolder_move_messages, METH_VARARGS|METH_KEYWORDS },
	{ "open_table", (PyCFunction)py_MAPIStoreFolder_open_table, METH_VARARGS|METH_KEYWORDS },
	{ NULL },
};

static PyObject *py_MAPIStoreFolder_get_uri(PyMAPIStoreFolderObject *self, void *closure)
{
	PyObject			*py_uri;
	enum mapistore_error 		retval;

	retval = pymapistore_get_uri(self->context->mstore_ctx, self->context->parent->username,
			self->fid, &py_uri);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	return py_uri;
}

static PyObject *py_MAPIStoreFolder_get_fid(PyMAPIStoreFolderObject *self, void *closure)
{
	return PyLong_FromUnsignedLongLong(self->fid);
}

static PyObject *py_MAPIStoreFolder_get_child_folders(PyMAPIStoreFolderObject *self, void *closure)
{
	TALLOC_CTX			*mem_ctx;
	PyMAPIStoreFoldersObject	*folder_list;
	enum mapistore_error		retval;
	uint64_t			*fid_list;
	uint32_t			list_size;

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
		goto end;
	}

	/* Return the PyMAPIStoreFolders object*/
	folder_list = PyObject_New(PyMAPIStoreFoldersObject, &PyMAPIStoreFolders);
	if (folder_list == NULL) {
		PyErr_NoMemory();
		goto end;
	}

	talloc_reference(self->mem_ctx, fid_list);
	folder_list->mem_ctx = self->mem_ctx;
	folder_list->folder = self;
	Py_INCREF(folder_list->folder);

	folder_list->fids = fid_list;
	folder_list->count = list_size;
	folder_list->curr_index = 0;

	talloc_free(mem_ctx);
	return (PyObject *) folder_list;
end:
	talloc_free(mem_ctx);
	return NULL;
}

static PyObject *py_MAPIStoreFolder_get_child_messages(PyMAPIStoreFolderObject *self, void *closure)
{
	TALLOC_CTX			*mem_ctx;
	PyMAPIStoreMessagesObject	*message_list;
	enum mapistore_error		retval;
	uint64_t			*mid_list;
	uint32_t			list_size;

	/* Get the child folders' MIDs */
	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	retval = mapistore_folder_get_child_fmids(self->context->mstore_ctx, self->context->context_id,
			self->folder_object, MAPISTORE_MESSAGE_TABLE, mem_ctx, &mid_list, &list_size);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		goto end;
	}

	/* Return the PyMAPIStoreMessages object*/
	message_list = PyObject_New(PyMAPIStoreMessagesObject, &PyMAPIStoreMessages);
	if (message_list == NULL) {
		PyErr_NoMemory();
		goto end;
	}

	message_list->mem_ctx = self->mem_ctx;
	talloc_reference(message_list->mem_ctx, mid_list);
	message_list->folder = self;
	Py_INCREF(message_list->folder);

	message_list->mids = mid_list;
	message_list->count = list_size;
	message_list->curr_index = 0;

	talloc_free(mem_ctx);
	return (PyObject *) message_list;
end:
	talloc_free(mem_ctx);
	return NULL;
}

static PyGetSetDef mapistore_folder_getsetters[] = {
	{ (char *)"uri", (getter)py_MAPIStoreFolder_get_uri, NULL, NULL },
	{ (char *)"fid", (getter)py_MAPIStoreFolder_get_fid, NULL, NULL },
	{ (char *)"subfolders", (getter)py_MAPIStoreFolder_get_child_folders, NULL, NULL },
	{ (char *)"messages", (getter)py_MAPIStoreFolder_get_child_messages, NULL, NULL },
	{ NULL }
};

PyTypeObject PyMAPIStoreFolder = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapistorefolder",
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

static PyObject *py_MAPIStoreFolders_iter(PyObject *self)
{
	Py_INCREF(self);
	return self;
}

static PyObject *py_MAPIStoreFolders_next(PyObject *_self)
{
	PyMAPIStoreFoldersObject 	*self;
	PyMAPIStoreFolderObject		*folder;
	void 				*folder_object;
	enum mapistore_error		retval;
	uint64_t			fid;

	self = (PyMAPIStoreFoldersObject *)_self;
	if (!self) {
		DEBUG(0,("[ERR][%s]: Expected object of type MAPIStoreFolders\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	/* Check if there are remaining folders */
	if(self->curr_index < self->count) {
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
		if (folder == NULL) {
			PyErr_NoMemory();
			return NULL;
		}

		folder->mem_ctx = self->mem_ctx;
		folder->context = self->folder->context;
		Py_INCREF(folder->context);

		folder->folder_object = folder_object;
		folder->fid = fid;

		return (PyObject *)folder;
	} else {
	    PyErr_SetNone(PyExc_StopIteration);
	    return NULL;
	}
}

PyTypeObject PyMAPIStoreFolders = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "childfolders",
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
	PyModule_AddObject(m, "FAI_TABLE", PyInt_FromLong(0x3));
	PyModule_AddObject(m, "RULE_TABLE", PyInt_FromLong(0x4));
	PyModule_AddObject(m, "ATTACHMENT_TABLE", PyInt_FromLong(0x5));
	PyModule_AddObject(m, "PERMISSIONS_TABLE", PyInt_FromLong(0x6));

	/* Copy flags */
	PyModule_AddObject(m, "NON_RECURSIVE", PyInt_FromLong(0x0));
	PyModule_AddObject(m, "RECURSIVE", PyInt_FromLong(0x1));

	/* Deletion flags */
	PyModule_AddObject(m, "DEL_MESSAGES", PyInt_FromLong(0x1));
	PyModule_AddObject(m, "DEL_FOLDERS", PyInt_FromLong(0x4));
	PyModule_AddObject(m, "DEL_ALL", PyInt_FromLong(0x5));
	PyModule_AddObject(m, "SOFT_DELETE", PyInt_FromLong(0x1));
	PyModule_AddObject(m, "PERMANENT_DELETE", PyInt_FromLong(0x2));

	/* Open message flags */
	PyModule_AddObject(m, "OPEN_READ", PyInt_FromLong(0x0));
	PyModule_AddObject(m, "OPEN_WRITE", PyInt_FromLong(0x1));

	/* Create message flags */
	PyModule_AddObject(m, "MESSAGE_GENERIC", PyInt_FromLong(0x0));
	PyModule_AddObject(m, "MESSAGE_FAI", PyInt_FromLong(0x1));
}
