/*
   OpenChange MAPI implementation.

   Python interface to mapistore

   Copyright (C) Julien Kerihuel 2010.

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

static PyTypeObject *SPropValue_Type;

void initmapistore(void);

static PyObject *py_MAPIStore_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX			*mem_ctx;
	struct mapistore_context	*mstore_ctx;
	PyMAPIStoreObject		*msobj;
	char				*kwnames[] = { "path", NULL };
	const char			*path = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|s", kwnames, &path)) {
		return NULL;
	}

	mem_ctx = talloc_named(NULL, 0, "py_MAPIStore_new");
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	mstore_ctx = mapistore_init(mem_ctx, path);
	if (mstore_ctx == NULL) {
		return NULL;
	}

	msobj = PyObject_New(PyMAPIStoreObject, &PyMAPIStore);
	msobj->mem_ctx = mem_ctx;
	msobj->mstore_ctx = mstore_ctx;

	return (PyObject *) msobj;
}

static void py_MAPIStore_dealloc(PyObject *_self)
{
	PyMAPIStoreObject *self = (PyMAPIStoreObject *)_self;

	mapistore_release(self->mstore_ctx);
	talloc_free(self->mem_ctx);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStore_add_context(PyMAPIStoreObject *self, PyObject *args)
{
	int		ret;
	uint32_t	context_id = 0;
	const char	*uri;
	const char	*username;

	if (!PyArg_ParseTuple(args, "ss", &username, &uri)) {
		return NULL;
	}

	ret = mapistore_add_context(self->mstore_ctx, username, username, uri, &context_id);
	if (ret != MAPISTORE_SUCCESS) {
		return NULL;
	}

	return PyInt_FromLong(context_id);
}

static PyObject *py_MAPIStore_del_context(PyMAPIStoreObject *self, PyObject *args)
{
	uint32_t	context_id = 0;

	if (!PyArg_ParseTuple(args, "i", &context_id)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_del_context(self->mstore_ctx, context_id));
}

static PyObject *py_MAPIStore_root_mkdir(PyObject *module, PyObject *args, PyObject *kwargs)
{
	enum MAPISTORE_ERROR	retval;
	PyMAPIStoreObject	*self = (PyMAPIStoreObject *) module;
	const char * const	kwnames[] = { "context_id", "parent_index",
					      "index", "folder_name", NULL };
	uint32_t		context_id;
	uint32_t		index;
	uint32_t		parent_index;
	const char		*folder_name;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "iii|s", 
					 discard_const_p(char *, kwnames),
					 &context_id, &parent_index, 
					 &index, &folder_name)) {
		return NULL;
	}

	if (folder_name && folder_name[1] == '\0') {
		folder_name = NULL;
	}

	retval = mapistore_create_root_folder(self->mstore_ctx, context_id, 
					      parent_index, index, folder_name);
	return PyInt_FromLong(retval);
}

static PyObject *py_MAPIStore_add_context_indexing(PyMAPIStoreObject *self, PyObject *args)
{
	uint32_t	context_id;
	const char	*username;

	if (!PyArg_ParseTuple(args, "si", &username, &context_id)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_add_context_indexing(self->mstore_ctx, username, context_id));
}

static PyObject *py_MAPIStore_search_context_by_uri(PyMAPIStoreObject *self, PyObject *args)
{
	int		ret;
	uint32_t	context_id = 0;
	const char	*uri;

	if (!PyArg_ParseTuple(args, "s", &uri)) {
		return NULL;
	}

	ret = mapistore_search_context_by_uri(self->mstore_ctx, uri, &context_id);
	if (ret != MAPISTORE_SUCCESS) {
		return NULL;
	}

	return PyInt_FromLong(context_id);
}

static PyObject *py_MAPIStore_add_context_ref_count(PyMAPIStoreObject *self, PyObject *args)
{
	uint32_t	context_id = 0;

	if (!PyArg_ParseTuple(args, "k", &context_id)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_add_context_ref_count(self->mstore_ctx, context_id));
}

static PyObject *py_MAPIStore_opendir(PyMAPIStoreObject *self, PyObject *args)
{
	uint32_t	context_id;
	uint64_t	parent_fid;
	uint64_t	fid;

	if (!PyArg_ParseTuple(args, "iKK", &context_id, &parent_fid, &fid)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_opendir(self->mstore_ctx, context_id, parent_fid, fid));
}

static PyObject *py_MAPIStore_closedir(PyMAPIStoreObject *self, PyObject *args)
{
	uint32_t	context_id;
	uint64_t	fid;

	if (!PyArg_ParseTuple(args, "iK", &context_id, &fid)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_closedir(self->mstore_ctx, context_id, fid));
}

static PyObject *py_MAPIStore_mkdir(PyMAPIStoreObject *self, PyObject *args)
{
	uint32_t		context_id;
	uint64_t		parent_fid;
	uint64_t		fid;
	PyObject		*mod_mapi;
	PyObject		*pySPropValue;
	PySPropValueObject	*SPropValue;
	struct SRow		aRow;

	mod_mapi = PyImport_ImportModule("openchange.mapi");
	if (mod_mapi == NULL) {
		printf("Can't load module\n");
		return NULL;
	}
	SPropValue_Type = (PyTypeObject *)PyObject_GetAttrString(mod_mapi, "SPropValue");
	if (SPropValue_Type == NULL) {
		return NULL;
	}

	if (!PyArg_ParseTuple(args, "iKKO", &context_id, &parent_fid, &fid, &pySPropValue)) {
		return NULL;
	}

	if (!PyObject_TypeCheck(pySPropValue, SPropValue_Type)) {
		PyErr_SetString(PyExc_TypeError, "Function require SPropValue object");
		return NULL;
	}

	SPropValue = (PySPropValueObject *)pySPropValue;
	aRow.cValues = SPropValue->cValues;
	aRow.lpProps = SPropValue->SPropValue;

	return PyInt_FromLong(mapistore_mkdir(self->mstore_ctx, context_id, parent_fid, fid, &aRow));
}

static PyObject *py_MAPIStore_rmdir(PyMAPIStoreObject *self, PyObject *args)
{
	uint32_t	context_id;
	uint64_t	parent_fid;
	uint64_t	fid;
	uint8_t		flags;

	if (!PyArg_ParseTuple(args, "iKKH", &context_id, &parent_fid, &fid, &flags)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_rmdir(self->mstore_ctx, context_id, parent_fid, fid, flags));
}

static PyObject *py_MAPIStore_setprops(PyMAPIStoreObject *self, PyObject *args)
{
	uint32_t		context_id;
	uint64_t		fid;
	uint8_t			object_type;
	PyObject		*mod_mapi;
	PyObject		*pySPropValue;
	PySPropValueObject	*SPropValue;
	struct SRow		aRow;

	mod_mapi = PyImport_ImportModule("openchange.mapi");
	if (mod_mapi == NULL) {
		printf("Can't load module\n");
		return NULL;
	}
	SPropValue_Type = (PyTypeObject *)PyObject_GetAttrString(mod_mapi, "SPropValue");
	if (SPropValue_Type == NULL) {
		return NULL;
	}

	if (!PyArg_ParseTuple(args, "iKbO", &context_id, &fid, &object_type, &pySPropValue)) {
		return NULL;
	}

	if (!PyObject_TypeCheck(pySPropValue, SPropValue_Type)) {
		PyErr_SetString(PyExc_TypeError, "Function require SPropValue object");
		return NULL;
	}

	SPropValue = (PySPropValueObject *)pySPropValue;
	aRow.cValues = SPropValue->cValues;
	aRow.lpProps = SPropValue->SPropValue;

	return PyInt_FromLong(mapistore_setprops(self->mstore_ctx, context_id, fid, object_type, &aRow));
}

static PyMethodDef mapistore_methods[] = {
	{ "add_context", (PyCFunction)py_MAPIStore_add_context, METH_VARARGS },
	{ "del_context", (PyCFunction)py_MAPIStore_del_context, METH_VARARGS },
	{ "root_mkdir", (PyCFunction)py_MAPIStore_root_mkdir, METH_KEYWORDS },
	{ "add_context_idexing", (PyCFunction)py_MAPIStore_add_context_indexing, METH_VARARGS },
	{ "search_context_by_uri", (PyCFunction)py_MAPIStore_search_context_by_uri, METH_VARARGS },
	{ "add_context_ref_count", (PyCFunction)py_MAPIStore_add_context_ref_count, METH_VARARGS },
	{ "opendir", (PyCFunction)py_MAPIStore_opendir, METH_VARARGS },
	{ "closedir", (PyCFunction)py_MAPIStore_closedir, METH_VARARGS },
	{ "mkdir", (PyCFunction)py_MAPIStore_mkdir, METH_VARARGS },
	{ "rmdir", (PyCFunction)py_MAPIStore_rmdir, METH_VARARGS },
	{ "setprops", (PyCFunction)py_MAPIStore_setprops, METH_VARARGS },
	{ NULL },
};

static PyGetSetDef mapistore_getsetters[] = {
	{ NULL }
};

PyTypeObject PyMAPIStore = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapistore",
	.tp_basicsize = sizeof (PyMAPIStoreObject),
	.tp_methods = mapistore_methods,
	.tp_getset = mapistore_getsetters,
	.tp_doc = "mapistore object",
	.tp_new = py_MAPIStore_new,
	.tp_dealloc = (destructor)py_MAPIStore_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

static PyObject *py_mapistore_set_mapping_path(PyObject *mod, PyObject *args)
{
	const char	*mapping_path;
	
	if (!PyArg_ParseTuple(args, "s", &mapping_path)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_set_mapping_path(mapping_path));
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

	Py_INCREF(&PyMAPIStore);
	PyModule_AddObject(m, "mapistore", (PyObject *)&PyMAPIStore);
}
