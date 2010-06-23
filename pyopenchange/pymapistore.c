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

	mem_ctx = talloc_new(NULL);
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

	if (!PyArg_ParseTuple(args, "s", &uri)) {
		return NULL;
	}

	ret = mapistore_add_context(self->mstore_ctx, uri, &context_id);
	if (ret != MAPISTORE_SUCCESS) {
		return NULL;
	}

	return PyInt_FromLong(context_id);
}

static PyObject *py_MAPIStore_del_context(PyMAPIStoreObject *self, PyObject *args)
{
	uint32_t	context_id = 0;

	if (!PyArg_ParseTuple(args, "k", &context_id)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_del_context(self->mstore_ctx, context_id));
}

static PyObject *py_MAPIStore_add_context_indexing(PyMAPIStoreObject *self, PyObject *args)
{
	uint32_t	context_id;
	const char	*username;

	if (!PyArg_ParseTuple(args, "sk", &username, &context_id)) {
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

	if (!PyArg_ParseTuple(args, "kKK", &context_id, &parent_fid, &fid)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_opendir(self->mstore_ctx, context_id, parent_fid, fid));
}

static PyObject *py_MAPIStore_closedir(PyMAPIStoreObject *self, PyObject *args)
{
	uint32_t	context_id;
	uint64_t	fid;

	if (!PyArg_ParseTuple(args, "kK", &context_id, &fid)) {
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

	if (!PyArg_ParseTuple(args, "kKKO", &context_id, &parent_fid, &fid, &pySPropValue)) {
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

	if (!PyArg_ParseTuple(args, "kKKH", &context_id, &parent_fid, &fid, &flags)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_rmdir(self->mstore_ctx, context_id, parent_fid, fid, flags));
}

static PyMethodDef mapistore_methods[] = {
	{ "add_context", (PyCFunction)py_MAPIStore_add_context, METH_VARARGS },
	{ "del_context", (PyCFunction)py_MAPIStore_del_context, METH_VARARGS },
	{ "add_context_idexing", (PyCFunction)py_MAPIStore_add_context_indexing, METH_VARARGS },
	{ "search_context_by_uri", (PyCFunction)py_MAPIStore_search_context_by_uri, METH_VARARGS },
	{ "add_context_ref_count", (PyCFunction)py_MAPIStore_add_context_ref_count, METH_VARARGS },
	{ "opendir", (PyCFunction)py_MAPIStore_opendir, METH_VARARGS },
	{ "closedir", (PyCFunction)py_MAPIStore_closedir, METH_VARARGS },
	{ "mkdir", (PyCFunction)py_MAPIStore_mkdir, METH_VARARGS },
	{ "rmdir", (PyCFunction)py_MAPIStore_rmdir, METH_VARARGS },
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

	PyModule_AddObject(m, "DEL_MESSAGES", PyInt_FromLong(0x1));
	PyModule_AddObject(m, "DEL_FOLDERS", PyInt_FromLong(0x4));
	PyModule_AddObject(m, "DELETE_HARD_DELETE", PyInt_FromLong(0x10));

	Py_INCREF(&PyMAPIStore);

	PyModule_AddObject(m, "mapistore", (PyObject *)&PyMAPIStore);
}
