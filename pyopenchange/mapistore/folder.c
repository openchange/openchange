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

#include <Python.h>
#include "pyopenchange/mapistore/pymapistore.h"

static void py_MAPIStoreFolder_dealloc(PyObject *_self)
{
	PyMAPIStoreFolderObject *self = (PyMAPIStoreFolderObject *)_self;

	Py_DECREF(self->context);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreFolder_get_fid(PyMAPIStoreFolderObject *self, void *closure)
{
	return PyLong_FromLongLong(self->fid);
}

static PyObject *py_MAPIStoreFolder_get_folder_count(PyMAPIStoreFolderObject *self, void *closure)
{
	uint32_t	RowCount;
	int		retval;

	retval = mapistore_folder_get_folder_count(self->context->mstore_ctx, self->context->context_id,
						   (self->folder_object ? self->folder_object : 
						    self->context->folder_object), &RowCount);
	if (retval != MAPISTORE_SUCCESS) {
		return PyInt_FromLong(-1);
	}

	return PyInt_FromLong(RowCount);
}


static PyObject *py_MAPIStoreFolder_get_message_count(PyMAPIStoreFolderObject *self, void *closure)
{
	uint32_t	RowCount;
	int		retval;

	retval = mapistore_folder_get_message_count(self->context->mstore_ctx, self->context->context_id,
						   (self->folder_object ? self->folder_object : 
						    self->context->folder_object), 
						    MAPISTORE_MESSAGE_TABLE, &RowCount);
	if (retval != MAPISTORE_SUCCESS) {
		return PyInt_FromLong(-1);
	}

	return PyInt_FromLong(RowCount);
}


static PyObject *py_MAPIStoreFolder_get_fai_message_count(PyMAPIStoreFolderObject *self, void *closure)
{
	uint32_t	RowCount;
	int		retval;

	retval = mapistore_folder_get_message_count(self->context->mstore_ctx, self->context->context_id,
						   (self->folder_object ? self->folder_object : 
						    self->context->folder_object), 
						    MAPISTORE_FAI_TABLE, &RowCount);
	if (retval != MAPISTORE_SUCCESS) {
		return PyInt_FromLong(-1);
	}

	return PyInt_FromLong(RowCount);
}

static PyMethodDef mapistore_folder_methods[] = {
	{ NULL },
};

static PyGetSetDef mapistore_folder_getsetters[] = {
	{ (char *)"fid", (getter)py_MAPIStoreFolder_get_fid, NULL, NULL },
	{ (char *)"folder_count", (getter)py_MAPIStoreFolder_get_folder_count, NULL, NULL },
	{ (char *)"message_count", (getter)py_MAPIStoreFolder_get_message_count, NULL, NULL },
	{ (char *)"fai_message_count", (getter)py_MAPIStoreFolder_get_fai_message_count, NULL, NULL },
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
