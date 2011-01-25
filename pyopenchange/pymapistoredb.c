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


static PyObject *py_MAPIStoreDB_get_mapistore_uri(PyMAPIStoreDBObject *_self, PyObject *args)
{
	PyMAPIStoreDBObject		*self = (PyMAPIStoreDBObject *) _self;
	enum MAPISTORE_ERROR		retval;
	enum MAPISTORE_DFLT_FOLDERS	dflt_folder;
	const char			*username;
	const char			*ns;
	char				*uri;

	if (!PyArg_ParseTuple(args, "kss", &dflt_folder, &username, &ns)) {
		return NULL;
	}

	retval = mapistoredb_get_mapistore_uri(self->mdb_ctx, dflt_folder, ns, username, &uri);
	if (retval == MAPISTORE_SUCCESS) {
		return PyString_FromString(uri);
	}

	return NULL;
}


static PyObject *py_MAPIStoreDB_get_new_fid(PyMAPIStoreDBObject *_self, PyObject *args)
{
	PyMAPIStoreDBObject		*self = (PyMAPIStoreDBObject *) _self;
	enum MAPISTORE_ERROR		retval;
	uint64_t			fmid = 0;
	char				*username;

	if (!PyArg_ParseTuple(args, "s", &username)) {
		return NULL;
	}

	retval = mapistoredb_get_new_fmid(self->mdb_ctx, (const char *)username, &fmid);
	if (retval == MAPISTORE_SUCCESS) {
		return PyLong_FromUnsignedLongLong(fmid);
	}

	return NULL;
}

static PyObject *py_MAPIStoreDB_get_new_allocation_range(PyMAPIStoreDBObject *_self, PyObject *args)
{
	PyMAPIStoreDBObject		*self = (PyMAPIStoreDBObject *) _self;
	enum MAPISTORE_ERROR		retval;
	char				*username;
	uint64_t			range;
	uint64_t			range_start = 0;
	uint64_t			range_end = 0;

	if (!PyArg_ParseTuple(args, "sK", &username, &range)) {
		return NULL;
	}

	retval = mapistoredb_get_new_allocation_range(self->mdb_ctx, (const char *)username, range, &range_start, &range_end);
	if (retval == MAPISTORE_SUCCESS) {
		return Py_BuildValue("kKK", retval, range_start, range_end);
	}

	return Py_BuildValue("kKK", retval, range_start, range_start);
}

static PyObject *py_MAPIStoreDB_new_mailbox(PyMAPIStoreDBObject *_self, PyObject *args)
{
	PyMAPIStoreDBObject		*self = (PyMAPIStoreDBObject *) _self;
	enum MAPISTORE_ERROR		retval;
	char				*username;
	char				*mapistore_uri;

	if (!PyArg_ParseTuple(args, "ss", &username, &mapistore_uri)) {
		return NULL;
	}

	retval = mapistoredb_register_new_mailbox(self->mdb_ctx, username, mapistore_uri);
	return PyInt_FromLong(retval);
}

static PyObject *py_MAPIStoreDB_set_mailbox_allocation_range(PyMAPIStoreDBObject *_self, PyObject *args)
{
	PyMAPIStoreDBObject		*self = (PyMAPIStoreDBObject *) _self;
	enum MAPISTORE_ERROR		retval;
	uint64_t			rstart;
	uint64_t			rend;
	char				*username;

	if (!PyArg_ParseTuple(args, "sKK", &username, &rstart, &rend)) {
		return NULL;
	}

	retval = mapistoredb_register_new_mailbox_allocation_range(self->mdb_ctx, username, rstart, rend);
	return PyInt_FromLong(retval);
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
	{ "get_mapistore_uri", (PyCFunction)py_MAPIStoreDB_get_mapistore_uri, METH_VARARGS },
	{ "get_new_fid", (PyCFunction)py_MAPIStoreDB_get_new_fid, METH_VARARGS },
	{ "get_new_allocation_range", (PyCFunction)py_MAPIStoreDB_get_new_allocation_range, METH_VARARGS },
	{ "new_mailbox", (PyCFunction)py_MAPIStoreDB_new_mailbox, METH_VARARGS },
	{ "set_mailbox_allocation_range", (PyCFunction)py_MAPIStoreDB_set_mailbox_allocation_range, METH_VARARGS },
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

	PyModule_AddObject(m, "MDB_ROOT_FOLDER", PyInt_FromLong((int)MDB_ROOT_FOLDER));
	PyModule_AddObject(m, "MDB_DEFERRED_ACTIONS", PyInt_FromLong((int)MDB_DEFERRED_ACTIONS));
	PyModule_AddObject(m, "MDB_SPOOLER_QUEUE", PyInt_FromLong((int)MDB_SPOOLER_QUEUE));
	PyModule_AddObject(m, "MDB_TODO_SEARCH", PyInt_FromLong((int)MDB_TODO_SEARCH));
	PyModule_AddObject(m, "MDB_IPM_SUBTREE", PyInt_FromLong((int)MDB_IPM_SUBTREE));
	PyModule_AddObject(m, "MDB_INBOX", PyInt_FromLong((int)MDB_INBOX));
	PyModule_AddObject(m, "MDB_OUTBOX", PyInt_FromLong((int)MDB_OUTBOX));
	PyModule_AddObject(m, "MDB_SENT_ITEMS", PyInt_FromLong((int)MDB_SENT_ITEMS));
	PyModule_AddObject(m, "MDB_DELETED_ITEMS", PyInt_FromLong((int)MDB_DELETED_ITEMS));
	PyModule_AddObject(m, "MDB_COMMON_VIEWS", PyInt_FromLong((int)MDB_COMMON_VIEWS));
	PyModule_AddObject(m, "MDB_SCHEDULE", PyInt_FromLong((int)MDB_SCHEDULE));
	PyModule_AddObject(m, "MDB_SEARCH", PyInt_FromLong((int)MDB_SEARCH));
	PyModule_AddObject(m, "MDB_VIEWS", PyInt_FromLong((int)MDB_VIEWS));
	PyModule_AddObject(m, "MDB_SHORTCUTS", PyInt_FromLong((int)MDB_SHORTCUTS));
	PyModule_AddObject(m, "MDB_REMINDERS", PyInt_FromLong((int)MDB_REMINDERS));
	PyModule_AddObject(m, "MDB_CALENDAR", PyInt_FromLong((int)MDB_CALENDAR));
	PyModule_AddObject(m, "MDB_CONTACTS", PyInt_FromLong((int)MDB_CONTACTS));
	PyModule_AddObject(m, "MDB_JOURNAL", PyInt_FromLong((int)MDB_JOURNAL));
	PyModule_AddObject(m, "MDB_NOTES", PyInt_FromLong((int)MDB_NOTES));
	PyModule_AddObject(m, "MDB_TASKS", PyInt_FromLong((int)MDB_TASKS));
	PyModule_AddObject(m, "MDB_DRAFTS", PyInt_FromLong((int)MDB_DRAFTS));
	PyModule_AddObject(m, "MDB_TRACKED_MAIL", PyInt_FromLong((int)MDB_TRACKED_MAIL));
	PyModule_AddObject(m, "MDB_SYNC_ISSUES", PyInt_FromLong((int)MDB_SYNC_ISSUES));
	PyModule_AddObject(m, "MDB_CONFLICTS", PyInt_FromLong((int)MDB_CONFLICTS));
	PyModule_AddObject(m, "MDB_LOCAL_FAILURES", PyInt_FromLong((int)MDB_LOCAL_FAILURES));
	PyModule_AddObject(m, "MDB_SERVER_FAILURES", PyInt_FromLong((int)MDB_SERVER_FAILURES));
	PyModule_AddObject(m, "MDB_JUNK_EMAIL", PyInt_FromLong((int)MDB_JUNK_EMAIL));
	PyModule_AddObject(m, "MDB_RSS_FEEDS", PyInt_FromLong((int)MDB_RSS_FEEDS));
	PyModule_AddObject(m, "MDB_CONVERSATION_ACT", PyInt_FromLong((int)MDB_CONVERSATION_ACT));
	PyModule_AddObject(m, "MDB_LAST_SPECIALFOLDER", PyInt_FromLong((int)MDB_LAST_SPECIALFOLDER));
	PyModule_AddObject(m, "MDB_CUSTOM", PyInt_FromLong((int)MDB_CUSTOM));
	

	PyModule_AddObject(m, "mapistoredb", (PyObject *)&PyMAPIStoreDB);
}
