/*
   OpenChange MAPI implementation.

   Python interface to mapistore management

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
#include "pyopenchange/pymapistore_mgmt.h"

void initmapistore_mgmt(void);

static PyObject *py_MAPIStoreMGMT_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX			*mem_ctx;
	struct mapistore_mgmt_context	*mgmt_ctx;
	PyMAPIStoreMGMTObject		*mgmtobj;
	char				*kwnames[] = { "path", NULL };
	const char			*path = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|s", kwnames, &path)) {
		return NULL;
	}

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	mgmt_ctx = mapistore_mgmt_init(mem_ctx, path);

	mgmtobj = PyObject_New(PyMAPIStoreMGMTObject, &PyMAPIStoreMGMT);
	mgmtobj->mem_ctx = mem_ctx;
	mgmtobj->mgmt_ctx = mgmt_ctx;

	return (PyObject *) mgmtobj;
}

static PyMethodDef mapistore_mgmt_methods[] = {
	{ NULL },
}

static PyGetSetDef mapistore_mgmt_getsetters[] = {
	{ NULL }
};

PyTypeObject PyMAPIStoreMGMT = {
	PyObject_HEAD_INIT(NULL, 0),
	.tp_name = "mapistore_mgmt",
	.tp_basicsize = sizeof (PyMAPIStoreMGMTObject),
	.tp_methods = mapistore_mgmt_methods,
	.tp_getset = mapistore_mgmt_getsetters,
	.tp_doc = "mapistore management object",
	.tp_new = py_MAPIStoreMGMT_new,
	.tp_dealloc = (destructor)py_MAPIStoreMGMT_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

static PyObject *py_mapistore_mgmt_set_mapping_path(PyObject *mod, PyObject *args)
{
	const char *mapping_path;

	if (!PyArg_ParseTuple(args, "s", &mapping_path)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_set_mapping_path(mapping_path));
}

static PyObject *py_mapistore_mgmt_errstr(PyObject *mod, PyObject *args)
{
	int		ret;

	if (!PyArg_ParseTuple(args, "k", &ret)) {
		return NULL;
	}

	return PyString_FromString(mapistore_errstr(ret));
}

static PyMethodDef py_mapistore_mgmt_global_methods[] = {
	{ "set_mapping_path", (PyCFunction)py_mapistore_mgmt_set_mapping_path, METH_VARARGS },
	{ "errstr", (PyCFunction)py_mapistore_mgmt_errstr, METH_VARARGS },
	{ NULL }
};

void initmapistore_mgmt(void)
{
	PyObject	*m;

	if (PyType_Ready(&PyMAPIStore) < 0) {
		return;
	}

	m = Py_InitModule3("mapistore_mgmt", py_mapistore_mgmt_global_methods,
			   "An interface to OpenChange MAPIStore Management");
	if (m == NULL) {
		return;
	}

	Py_INCREF(&PyMAPIStoreMGMT);

	PyModule_AddObject(m, "mapistore_mgmt", (PyObject *)&PyMAPIStore);
}
