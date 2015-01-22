/*
   OpenChange MAPI implementation.

   Python interface to mapistore context

   Copyright (C) Julien Kerihuel 2010-2011.

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
#include <inttypes.h>
#include "pyopenchange/mapistore/pymapistore.h"

static void py_MAPIStoreContext_dealloc(PyObject *_self)
{
	PyMAPIStoreContextObject *self = (PyMAPIStoreContextObject *) _self;

	mapistore_del_context(self->mstore_ctx, self->context_id);
	Py_XDECREF(self->parent);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreContext_open(PyMAPIStoreContextObject *self)
{
	PyMAPIStoreFolderObject		*folder;

	folder = PyObject_New(PyMAPIStoreFolderObject, &PyMAPIStoreFolder);
	if (folder == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	folder->mem_ctx = self->mem_ctx;
	folder->context = self;
	Py_INCREF(folder->context);

	folder->folder_object = self->folder_object;
	folder->fid = self->fid;
	
	return (PyObject *)folder;
}


static PyMethodDef mapistore_context_methods[] = {
	{ "open", (PyCFunction)py_MAPIStoreContext_open, METH_NOARGS },
	{ NULL },
};

static PyGetSetDef mapistore_context_getsetters[] = {
	{ NULL }
};

PyTypeObject PyMAPIStoreContext = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapistore context",
	.tp_basicsize = sizeof (PyMAPIStoreContextObject),
	.tp_methods = mapistore_context_methods,
	.tp_getset = mapistore_context_getsetters,
	.tp_doc = "mapistore context object",
	.tp_dealloc = (destructor)py_MAPIStoreContext_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

void initmapistore_context(PyObject *m)
{
	if (PyType_Ready(&PyMAPIStoreContext) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStoreContext);
}
