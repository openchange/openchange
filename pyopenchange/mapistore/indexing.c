/*
   OpenChange MAPI implementation.

   Python interface to mapistore indexing

   Copyright (C) Kamen Mazdrashki <kamenim@openchange.org> 2014

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
#include "mapiproxy/libmapistore/backends/indexing_tdb.h"
#include "mapiproxy/libmapistore/backends/indexing_mysql.h"


static PyObject *py_Indexing_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX			*mem_ctx;
	struct loadparm_context		*lp_ctx;
	struct indexing_context		*ictx;
	PyMAPIStoreIndexingObject	*pyidx;
	enum mapistore_error		ret;
	const char			*indexing_url;
	const char			*username = NULL;
	char				*kwnames[] = { "username", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwnames, &username)) {
		return NULL;
	}

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	/* Initialize configuration */
	lp_ctx = loadparm_init(mem_ctx);
	if (lp_ctx == NULL) {
		PyErr_SetString(PyExc_SystemError,
				"Error in loadparm_init");
		talloc_free(mem_ctx);
		return NULL;
	}

	if (!lpcfg_load_default(lp_ctx)) {
		PyErr_SetString(PyExc_SystemError,
				"lpcfg_load_defaul unable to load content from path");
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Initialize indexing layer */
	indexing_url = lpcfg_parm_string(lp_ctx, NULL, "mapistore", "indexing_backend");
	if (indexing_url && (strncmp(indexing_url, "mysql://", 8) == 0)) {
		ret = mapistore_indexing_mysql_init(mem_ctx, username, indexing_url, &ictx);
	} else {
		ret = mapistore_indexing_tdb_init(mem_ctx, username, &ictx);
	}
	if (ret != MAPISTORE_SUCCESS) {
		PyErr_Format(PyExc_SystemError,
			     "Failed to initialize mapistore_indexing: err=[%s], url=[%s]",
			     mapistore_errstr(ret),
			     indexing_url);
		talloc_free(mem_ctx);
		return NULL;
	}

	pyidx = PyObject_New(PyMAPIStoreIndexingObject, &PyMAPIStoreIndexing);
	pyidx->mem_ctx = mem_ctx;
	pyidx->lp_ctx = lp_ctx;
	pyidx->username = talloc_strdup(mem_ctx, username);
	pyidx->ictx = ictx;

	return (PyObject *)pyidx;
}

static void py_MAPIStoreIndexing_dealloc(PyObject *_self)
{
	PyMAPIStoreIndexingObject *self = (PyMAPIStoreIndexingObject *)_self;

	TALLOC_FREE(self->mem_ctx);

	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreIndexing_add_fmid(PyMAPIStoreIndexingObject *self, PyObject *args, PyObject *kwargs)
{
	enum mapistore_error	ret;
	uint64_t		fmid;
	const char		*mapistore_URI;
	char			*kwnames[] = { "fmid", "uri", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Ks", kwnames, &fmid, &mapistore_URI)) {
		return NULL;
	}

	ret = self->ictx->add_fmid(self->ictx, self->username, fmid, mapistore_URI);

	return PyBool_FromLong(ret == MAPISTORE_SUCCESS);
}

static PyObject *py_MAPIStoreIndexing_del_fmid(PyMAPIStoreIndexingObject *self, PyObject *args, PyObject *kwargs)
{
	enum mapistore_error	ret;
	uint64_t		fmid;
	uint8_t			flags;
	char			*kwnames[] = { "fmid", "flags", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "KB", kwnames, &fmid, &flags)) {
		return NULL;
	}

	ret = self->ictx->del_fmid(self->ictx, self->username, fmid, flags);

	return PyBool_FromLong(ret == MAPISTORE_SUCCESS);
}

static PyObject *py_MAPIStoreIndexing_uri_for_fmid(PyMAPIStoreIndexingObject *self, PyObject *args, PyObject *kwargs)
{
	enum mapistore_error	ret;
	uint64_t		fmid;
	char			*uri = NULL;
	PyObject		*py_uri;
	bool			deleted;
	char			*kwnames[] = { "fmid", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "K", kwnames, &fmid)) {
		return NULL;
	}

	ret = self->ictx->get_uri(self->ictx, self->username, self->mem_ctx, fmid, &uri, &deleted);
	if (ret != MAPISTORE_SUCCESS) {
		Py_RETURN_NONE;
	}

	py_uri = PyString_FromString(uri);
	talloc_free(uri);

	return py_uri;
}

static PyObject *py_MAPIStoreIndexing_fmid_for_uri(PyMAPIStoreIndexingObject *self, PyObject *args, PyObject *kwargs)
{
	enum mapistore_error	ret;
	uint64_t		fmid;
	char			*uri = NULL;
	bool			deleted;
	char			*kwnames[] = { "uri", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwnames, &uri)) {
		return NULL;
	}

	ret = self->ictx->get_fmid(self->ictx, self->username, uri, false, &fmid, &deleted);
	if (ret != MAPISTORE_SUCCESS) {
		Py_RETURN_NONE;
	}

	return PyLong_FromLongLong(fmid);
}

static PyObject *py_MAPIStoreIndexing_allocate_fmid(PyMAPIStoreIndexingObject *self)
{
	enum mapistore_error	ret;
	uint64_t		fmid;

	ret = self->ictx->allocate_fmid(self->ictx, self->username, &fmid);
	if (ret != MAPISTORE_SUCCESS) {
		PyErr_Format(PyExc_SystemError,
			     "Failed to allocate new fmid: err=[%s]",
			     mapistore_errstr(ret));
		return NULL;
	}

	return PyLong_FromLongLong(fmid);
}

static PyObject *obj_get_username(PyMAPIStoreIndexingObject *self, void *closure)
{
	return PyString_FromString(self->username);
}


static PyMethodDef mapistore_indexing_methods[] = {
	{ "add_fmid", (PyCFunction)py_MAPIStoreIndexing_add_fmid, METH_VARARGS|METH_KEYWORDS },
	{ "del_fmid", (PyCFunction)py_MAPIStoreIndexing_del_fmid, METH_VARARGS|METH_KEYWORDS },
	{ "uri_for_fmid", (PyCFunction)py_MAPIStoreIndexing_uri_for_fmid, METH_VARARGS|METH_KEYWORDS },
	{ "fmid_for_uri", (PyCFunction)py_MAPIStoreIndexing_fmid_for_uri, METH_VARARGS|METH_KEYWORDS },
	{ "allocate_fmid", (PyCFunction)py_MAPIStoreIndexing_allocate_fmid, METH_NOARGS },
	{ NULL },
};

static PyGetSetDef mapistore_indexing_getsetters[] = {
	{ "username", (getter)obj_get_username, (setter)NULL,
		"Get current username for Indexing" },
	{ NULL }
};

PyTypeObject PyMAPIStoreIndexing = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapistore.Indexing",
	.tp_basicsize = sizeof (PyMAPIStoreIndexingObject),
	.tp_methods = mapistore_indexing_methods,
	.tp_getset = mapistore_indexing_getsetters,
	.tp_doc = "Intefrace to access mapistore indexing impelementation",
	.tp_new = py_Indexing_new,
	.tp_dealloc = (destructor)py_MAPIStoreIndexing_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

void initmapistore_indexing(PyObject *m)
{
	if (PyType_Ready(&PyMAPIStoreIndexing) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStoreIndexing);

	PyDict_SetItemString(PyMAPIStoreIndexing.tp_dict, "SOFT_DELETE", PyInt_FromLong(MAPISTORE_SOFT_DELETE));
	PyDict_SetItemString(PyMAPIStoreIndexing.tp_dict, "PERMANENT_DELETE", PyInt_FromLong(MAPISTORE_PERMANENT_DELETE));

	PyModule_AddObject(m, "Indexing", (PyObject *)&PyMAPIStoreIndexing);
}
