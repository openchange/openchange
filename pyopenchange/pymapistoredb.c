/*
   OpenChange MAPI implementation.

   Python interface to mapistore database

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
#include "pyopenchange/pymapistore.h"

void initmapistoredb(void);

static PyObject *py_MAPIStoreDB_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX			*mem_ctx;
	struct mapistoredb_context	*mdb_ctx;
	PyMAPIStoreDBObject		*mdbobj;
	char				*kwnames[] = { "path", NULL };
	const char			*path = NULL;

	/* Path is optional */
	PyArg_ParseTupleAndKeywords(args, kwargs, "|s", kwnames, &path);

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	mdb_ctx = mapistoredb_init(mem_ctx, path);
	if (mdb_ctx == NULL) {
		DEBUG(0, ("mapistoredb_init returned NULL\n"));
		return NULL;
	}

	mdbobj = PyObject_New(PyMAPIStoreDBObject, &PyMAPIStoreDB);
	mdbobj->mem_ctx = mem_ctx;
	mdbobj->mdb_ctx = mdb_ctx;

	return (PyObject *) mdbobj;
}

static void py_MAPIStoreDB_dealloc(PyObject *_self)
{
	PyMAPIStoreDBObject	*self = (PyMAPIStoreDBObject *)_self;

	talloc_free(self->mem_ctx);
	PyObject_Del(_self);
}


static PyObject *py_MAPIStoreDB_dump_configuration(PyMAPIStoreDBObject *self, PyObject *args)
{
	mapistoredb_dump_conf(self->mdb_ctx);
	return PyInt_FromLong(0);
}

static PyObject *py_MAPIStoreDB_provision(PyMAPIStoreDBObject *self, PyObject *args)
{
	return PyInt_FromLong(mapistoredb_provision(self->mdb_ctx));
}


static int PyMAPIStoreDB_setParameter(PyObject *_self, PyObject *value, void *data)
{
	PyMAPIStoreDBObject	*self = (PyMAPIStoreDBObject *) _self;
	const char		*attr = (const char *) data;
	const char		*str;

	if (!self->mdb_ctx) return -1;

	if (!PyArg_Parse(value, "s", &str)) {
		return -1;
	}

 	if (!strcmp(attr, "netbiosname")) {
		return mapistoredb_set_netbiosname(self->mdb_ctx, str);
	} else if (!strcmp(attr, "firstorg")) {
		return mapistoredb_set_firstorg(self->mdb_ctx, str);
	} else if (!strcmp(attr, "firstou")) {
		return mapistoredb_set_firstou(self->mdb_ctx, str);
	}

	return 0;
}

static PyObject *PyMAPIStoreDB_getParameter(PyObject *_self, void *data)
{
	PyMAPIStoreDBObject	*self = (PyMAPIStoreDBObject *) _self;
	const char		*attr = (const char *) data;

	if (!strcmp(attr, "netbiosname")) {
		return PyString_FromString(mapistoredb_get_netbiosname(self->mdb_ctx));
	} else if (!strcmp(attr, "firstorg")) {
		return PyString_FromString(mapistoredb_get_firstorg(self->mdb_ctx));
	} else if (!strcmp(attr, "firstou")) {
		return PyString_FromString(mapistoredb_get_firstou(self->mdb_ctx));
	}

	return NULL;
}

static PyMethodDef mapistoredb_methods[] = {
	{ "dump_configuration", (PyCFunction)py_MAPIStoreDB_dump_configuration, METH_VARARGS },
	{ "provision", (PyCFunction)py_MAPIStoreDB_provision, METH_VARARGS },
	{ NULL },
};

static PyGetSetDef mapistoredb_getsetters[] = {
	{ "netbiosname", (getter)PyMAPIStoreDB_getParameter,
	  (setter)PyMAPIStoreDB_setParameter, "netbiosname", "netbiosname"},
	{ "firstorg", (getter)PyMAPIStoreDB_getParameter,
	  (setter)PyMAPIStoreDB_setParameter, "firstorg", "firstorg"},
	{ "firstou", (getter)PyMAPIStoreDB_getParameter,
	  (setter)PyMAPIStoreDB_setParameter, "firstou", "firstou"},
	{ NULL },
};

PyTypeObject PyMAPIStoreDB = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapistoredb",
	.tp_basicsize = sizeof (PyMAPIStoreDBObject),
	.tp_methods = mapistoredb_methods,
	.tp_getset = mapistoredb_getsetters,
	.tp_doc = "mapistore database object",
	.tp_new = py_MAPIStoreDB_new,
	.tp_dealloc = (destructor) py_MAPIStoreDB_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

static PyMethodDef py_mapistoredb_global_methods[] = {
	{ NULL },
};

void initmapistoredb(void)
{
	PyObject	*m;

	if (PyType_Ready(&PyMAPIStoreDB) < 0) {
		return;
	}

	m = Py_InitModule3("mapistoredb", py_mapistoredb_global_methods,
			   "An interface to MAPIStore database");

	if (m == NULL) {
		return;
	}

	PyModule_AddObject(m, "mapistoredb", (PyObject *)&PyMAPIStoreDB);
}
