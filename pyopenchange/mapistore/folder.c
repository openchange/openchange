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
#include "gen_ndr/exchange.h"

static void py_MAPIStoreFolder_dealloc(PyObject *_self)
{
	PyMAPIStoreFolderObject *self = (PyMAPIStoreFolderObject *)_self;

	Py_DECREF(self->context);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreFolder_create_folder(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	int			ret;
	/* PyMAPIStoreFolderObject	*folder; */
	char			*kwnames[] = { "name", "description", "foldertype", "flags" };
	const char		*name;
	const char		*desc = NULL;
	uint16_t		foldertype = FOLDER_GENERIC;
	uint16_t		flags = NONE;
	uint64_t		fid;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|shh", kwnames, &name, &desc, &foldertype, &flags)) {
		return NULL;
	}

	/* Step 1. Check if the folder already exists */
	ret = mapistore_folder_get_child_fid_by_name(self->context->mstore_ctx,
						     self->context->context_id,
						     self->context->folder_object, 
						     name, &fid);
	if (ret == MAPISTORE_SUCCESS) {
		if (flags != OPEN_IF_EXISTS) {
			PyErr_MAPIStore_IS_ERR_RAISE(MAPISTORE_ERR_EXIST);
			Py_RETURN_NONE;
		}
	}
	
	/* TODO: Complete the implementation */

	return Py_None;
}

static PyObject *py_MAPIStoreFolder_get_fid(PyMAPIStoreFolderObject *self, void *closure)
{
	return PyLong_FromLongLong(self->fid);
}

static PyObject *py_MAPIStoreFolder_get_child_count(PyMAPIStoreFolderObject *self, PyObject *args, PyObject *kwargs)
{
	uint32_t			RowCount;
	enum mapistore_table_type	table_type;
	char				*kwnames[] = { "table_type" };
	int				retval;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwnames, &table_type)) {
		return NULL;
	}

	retval = mapistore_folder_get_child_count(self->context->mstore_ctx, self->context->context_id,
						  (self->folder_object ? self->folder_object : 
						   self->context->folder_object), table_type, &RowCount);
	if (retval != MAPISTORE_SUCCESS) {
		return PyInt_FromLong(-1);
	}

	return PyInt_FromLong(RowCount);
}

static PyMethodDef mapistore_folder_methods[] = {
	{ "create_folder", (PyCFunction)py_MAPIStoreFolder_create_folder, METH_VARARGS|METH_KEYWORDS },
	{ "get_child_count", (PyCFunction)py_MAPIStoreFolder_get_child_count, METH_VARARGS|METH_KEYWORDS },
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
