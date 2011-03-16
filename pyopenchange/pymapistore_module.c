/*
   OpenChange MAPI implementation.

   Python interface to mapistore

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
#include "pyopenchange/pymapi.h"

void initmapistore(void);

static PyObject *py_mapistore_set_mapping_path(PyObject *mod, PyObject *args)
{
	const char	*mapping_path;
	
	if (!PyArg_ParseTuple(args, "s", &mapping_path)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_set_mapping_path(mapping_path));
}

static PyObject *py_mapistore_set_database_path(PyObject *mod, PyObject *args)
{
	const char	*db_path;
	
	if (!PyArg_ParseTuple(args, "s", &db_path)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_set_database_path(db_path));
}

static PyObject *py_mapistore_set_named_properties_database_path(PyObject *mod, PyObject *args)
{
	const char	*db_path;
	
	if (!PyArg_ParseTuple(args, "s", &db_path)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_set_named_properties_database_path(db_path));
}

static PyObject *py_mapistore_errstr(PyObject *mod, PyObject *args)
{
	int		ret;

	if (!PyArg_ParseTuple(args, "k", &ret)) {
		return NULL;
	}

	return PyString_FromString(mapistore_errstr(ret));
}

static PyMethodDef py_mapistore_global_methods[] = {
	{ "set_mapping_path", (PyCFunction)py_mapistore_set_mapping_path, METH_VARARGS },
	{ "set_database_path", (PyCFunction)py_mapistore_set_database_path, METH_VARARGS },
	{ "set_named_properties_database_path", (PyCFunction)py_mapistore_set_named_properties_database_path, METH_VARARGS },
	{ "errstr", (PyCFunction)py_mapistore_errstr, METH_VARARGS },
	{ NULL },
};

void initmapistore(void)
{
	PyObject	*m;

	if (PyType_Ready(&PyMAPIStore) < 0) {
		return;
	}

	m = Py_InitModule3("mapistore", py_mapistore_global_methods,
			   "An interface to OpenChange MAPIStore");
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

	PyModule_AddObject(m, "DEL_MESSAGES", PyInt_FromLong(0x1));
	PyModule_AddObject(m, "DEL_FOLDERS", PyInt_FromLong(0x4));
	PyModule_AddObject(m, "DELETE_HARD_DELETE", PyInt_FromLong(0x10));

	PyModule_AddObject(m, "MAPISTORE_FOLDER", PyInt_FromLong(MAPISTORE_FOLDER));
	PyModule_AddObject(m, "MAPISTORE_MESSAGE", PyInt_FromLong(MAPISTORE_MESSAGE));

	PyModule_AddObject(m, "FOLDER_GENERIC", PyInt_FromLong(FOLDER_GENERIC));
	PyModule_AddObject(m, "FOLDER_SEARCH", PyInt_FromLong(FOLDER_SEARCH));

	Py_INCREF(&PyMAPIStore);
	PyModule_AddObject(m, "mapistore", (PyObject *)&PyMAPIStore);
}
