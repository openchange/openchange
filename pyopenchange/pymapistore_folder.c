/*
   OpenChange MAPI implementation.

   Python interface to mapistore folders

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
#include "pyopenchange/pymapistore.h"
#include "pyopenchange/pymapi.h"

PyObject *pymapistore_folder_new(TALLOC_CTX *mem_ctx,
				 struct mapistore_context *mstore_ctx,
				 PyObject *parent_object,
				 uint32_t context_id,
				 uint64_t fid)
{
	PyMAPIStoreFolderObject	*ret;

	/* Sanity checks */
	if (!mstore_ctx || !context_id) {
		PyErr_SetString(PyExc_ValueError, "Invalid parameter");
		return NULL;
	}

	ret = PyObject_New(PyMAPIStoreFolderObject, &PyMAPIStoreFolder);
	if (!ret) {
		PyErr_SetString(PyExc_MemoryError, "No more memory");
		return NULL;
	}

	ret->mem_ctx = mem_ctx;
	ret->mstore_ctx = mstore_ctx;
	ret->parent_object = parent_object;
	ret->context_id = context_id;
	ret->fid = fid;

	Py_INCREF(ret->parent_object);
	Py_INCREF(ret);

	return (PyObject *) ret;
}

static PyObject *py_MAPIStoreFolder_open(PyMAPIStoreFolderObject *self,
					 PyObject *args,
					 PyObject *kwargs)
{
	return NULL;
}

static PyObject *py_MAPIStoreFolder_close(PyMAPIStoreFolderObject *self,
					  PyObject *args,
					  PyObject *kwargs)
{
	return NULL;
}

static PyObject *py_MAPIStoreFolder_mkdir(PyMAPIStoreFolderObject *self,
					  PyObject *args,
					  PyObject *kwargs)
{
	return NULL;
}

static PyObject *py_MAPIStoreFolder_rmdir(PyMAPIStoreFolderObject *self,
					  PyObject *args,
					  PyObject *kwargs)
{
	return NULL;
}

static PyObject *py_MAPIStoreFolder_move(PyMAPIStoreFolderObject *self,
					 PyObject *args,
					 PyObject *kwargs)
{
	return NULL;
}

static PyObject *py_MAPIStoreFolder_copy(PyMAPIStoreFolderObject *self,
					 PyObject *args,
					 PyObject *kwargs)
{
	return NULL;
}

static PyObject *py_MAPIStoreFolder_empty(PyMAPIStoreFolderObject *self,
					  PyObject *args,
					  PyObject *kwargs)
{
	return NULL;
}

static PyObject *py_MAPIStoreFolder_setprops(PyMAPIStoreFolderObject *self,
					     PyObject *args,
					     PyObject *kwargs)
{
	return NULL;
}

static PyObject *py_MAPIStoreFolder_get_hierarchy_table(PyMAPIStoreFolderObject *self,
							PyObject *args,
							PyObject *kwargs)
{
	return NULL;
}

static PyObject *py_MAPIStoreFolder_get_contents_table(PyMAPIStoreFolderObject *self,
						       PyObject *args,
						       PyObject *kwargs)
{
	return NULL;
}

static void pymapistore_folder_dealloc(PyObject *_self)
{
	PyMAPIStoreFolderObject *self = (PyMAPIStoreFolderObject *)_self;

	Py_DECREF(self->parent_object);
	PyObject_Del(_self);
}

static PyMethodDef pymapistore_folder_methods[] = {
	/* Folder semantics */
	{ "open", (PyCFunction)py_MAPIStoreFolder_open, METH_KEYWORDS },
	{ "close", (PyCFunction)py_MAPIStoreFolder_close, METH_KEYWORDS },
	{ "mkdir", (PyCFunction)py_MAPIStoreFolder_mkdir, METH_KEYWORDS },
	{ "rmdir", (PyCFunction)py_MAPIStoreFolder_rmdir, METH_KEYWORDS },
	{ "move", (PyCFunction)py_MAPIStoreFolder_move, METH_KEYWORDS },
	{ "copy", (PyCFunction)py_MAPIStoreFolder_copy, METH_KEYWORDS },
	{ "empty", (PyCFunction)py_MAPIStoreFolder_empty, METH_KEYWORDS },
	/* Properties semantics */
	{ "setprops", (PyCFunction)py_MAPIStoreFolder_setprops, METH_KEYWORDS },
	/* Table semantics */
	{ "hierarchy_table", (PyCFunction)py_MAPIStoreFolder_get_hierarchy_table, METH_KEYWORDS },
	{ "contents_table", (PyCFunction)py_MAPIStoreFolder_get_contents_table, METH_KEYWORDS },
	{ NULL }
};

static PyObject *pymapistore_get_folder_identifier(PyMAPIStoreFolderObject *self, void *py_data)
{
	return Py_BuildValue("K", self->fid);
}

static PyGetSetDef pymapistore_folder_getsetters[] = {
	{ "id", (getter)pymapistore_get_folder_identifier, NULL, "Folder identifier", NULL },
	{ NULL }
};

PyTypeObject PyMAPIStoreFolder = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "Folder",
	.tp_basicsize = sizeof(PyMAPIStoreFolderObject),
	.tp_methods = pymapistore_folder_methods,
	.tp_getset = pymapistore_folder_getsetters,
	.tp_doc = "MAPIStore Folder",
	.tp_dealloc = pymapistore_folder_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT
};
