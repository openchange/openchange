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
#include "pyopenchange/mapistore/pymapistore.h"
#include "pyopenchange/pymapi.h"

#include <param.h>
#include <samba/session.h>

/* static PyTypeObject *SPropValue_Type; */

extern struct ldb_context *samdb_connect(TALLOC_CTX *, struct tevent_context *, struct loadparm_context *, struct auth_session_info *, int);

void initmapistore(void);

static PyMAPIStoreGlobals globals;

void PyErr_SetMAPIStoreError(uint32_t retval)
{
	PyErr_SetObject(PyExc_RuntimeError,
			Py_BuildValue("(i, s)", retval, mapistore_errstr(retval)));
}

PyMAPIStoreGlobals *get_PyMAPIStoreGlobals()
{
	return &globals;
}

static void sam_ldb_init(const char *syspath)
{
	TALLOC_CTX		*mem_ctx;
	/* char			*ldb_path; */
	struct loadparm_context *lp_ctx;
	struct tevent_context	*ev;
	int			ret;
	struct ldb_result	*res;
	struct ldb_dn		*tmp_dn = NULL;
	static const char	*attrs[] = {
		"rootDomainNamingContext",
		"defaultNamingContext",
		NULL
	};

	/* Sanity checks */
	if (globals.samdb_ctx) return;

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	ev = tevent_context_init(talloc_autofree_context());
	if (!ev) goto end;

	/* /\* Step 1. Retrieve a LDB context pointer on sam.ldb database *\/ */
	/* ldb_path = talloc_asprintf(mem_ctx, "%s/sam.ldb", syspath); */

	/* Step 2. Connect to the database */
	lp_ctx = loadparm_init_global(true);
	globals.samdb_ctx = samdb_connect(NULL, NULL, lp_ctx, system_session(lp_ctx), 0);
	if (!globals.samdb_ctx) goto end;

	/* Step 3. Search for rootDSE record */
	ret = ldb_search(globals.samdb_ctx, mem_ctx, &res, ldb_dn_new(mem_ctx, globals.samdb_ctx, "@ROOTDSE"),
			 LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS) goto end;
	if (res->count != 1) goto end;

	/* Step 4. Set opaque naming */
	tmp_dn = ldb_msg_find_attr_as_dn(globals.samdb_ctx, globals.samdb_ctx,
					 res->msgs[0], "rootDomainNamingContext");
	ldb_set_opaque(globals.samdb_ctx, "rootDomainNamingContext", tmp_dn);
	
	tmp_dn = ldb_msg_find_attr_as_dn(globals.samdb_ctx, globals.samdb_ctx,
					 res->msgs[0], "defaultNamingContext");
	ldb_set_opaque(globals.samdb_ctx, "defaultNamingContext", tmp_dn);

end:
	talloc_free(mem_ctx);
}

static void openchange_ldb_init(const char *syspath)
{
	TALLOC_CTX 		*mem_ctx;
	struct loadparm_context *lp_ctx;
	const char		*openchangedb_backend;

	if (globals.ocdb_ctx) return;

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	lp_ctx = loadparm_init_global(true);
	openchangedb_backend = lpcfg_parm_string(lp_ctx, NULL, "mapiproxy", "openchangedb");

	if (openchangedb_backend) {
		openchangedb_mysql_initialize(mem_ctx, lp_ctx, &globals.ocdb_ctx);
	} else {
		openchangedb_ldb_initialize(mem_ctx, syspath, &globals.ocdb_ctx);
	}

	if (!globals.ocdb_ctx) {
		PyErr_SetString(PyExc_SystemError, "Cannot initialize openchangedb ldb");
		goto end;
	}
	(void) talloc_reference(NULL, globals.ocdb_ctx);
end:
	talloc_free(mem_ctx);
}

static PyObject *py_MAPIStore_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX			*mem_ctx;
	struct loadparm_context		*lp_ctx;
	struct mapistore_context	*mstore_ctx;
	PyMAPIStoreObject		*msobj;
	char				*kwnames[] = { "syspath", "path", NULL };
	const char			*path = NULL;
	const char			*syspath = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|s", kwnames, &syspath, &path)) {
		return NULL;
	}

	/* Initialize ldb context on sam.ldb */
	sam_ldb_init(syspath);
	if (globals.samdb_ctx == NULL) {
		PyErr_SetString(PyExc_SystemError,
				"error in sam_ldb_init");
		return NULL;
	}

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	/* Initialize ldb context on openchange.ldb */
	openchange_ldb_init(syspath);
	if (globals.ocdb_ctx == NULL) {
		PyErr_SetString(PyExc_SystemError,
				"error in openchange_ldb_init");
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Initialize configuration */
	lp_ctx = loadparm_init(mem_ctx);
	lpcfg_load_default(lp_ctx);

	/* Initialize mapistore */
	mstore_ctx = mapistore_init(mem_ctx, lp_ctx, path);
	if (mstore_ctx == NULL) {
		PyErr_SetString(PyExc_SystemError,
				"error in mapistore_init");
		talloc_free(mem_ctx);
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

static PyObject *py_MAPIStore_new_mgmt(PyMAPIStoreObject *self, PyObject *args)
{
	PyMAPIStoreMGMTObject	*obj;

	obj = PyObject_New(PyMAPIStoreMGMTObject, &PyMAPIStoreMGMT);
	obj->mgmt_ctx = mapistore_mgmt_init(self->mstore_ctx);
	if (obj->mgmt_ctx == NULL) {
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_NOT_INITIALIZED);
		return NULL;
	}
	obj->mem_ctx = self->mem_ctx;
	obj->parent = self;
	Py_INCREF(obj->parent);

	return (PyObject *) obj;
}

static PyObject *py_MAPIStore_add_context(PyMAPIStoreObject *self, PyObject *args)
{
	int				ret;
	PyMAPIStoreContextObject	*context;
	uint32_t			context_id = 0;
	const char			*uri;
	const char			*username;
	void				*folder_object;
        int64_t				fid = 0;

	if (!PyArg_ParseTuple(args, "ss", &uri, &username)) {
		return NULL;
	}

	/* printf("Add context: %s\n", uri); */

	/* Initialize connection info */
	ret = mapistore_set_connection_info(self->mstore_ctx, globals.samdb_ctx, globals.ocdb_ctx, username);
	if (ret != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(ret);
		return NULL;
	}

	/* Get FID given mapistore_uri and username */
	ret = openchangedb_get_fid(globals.ocdb_ctx, uri, &fid);
	if (ret != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(ret);
		return NULL;
	}

	ret = mapistore_add_context(self->mstore_ctx, username, uri, fid, &context_id, &folder_object);
	if (ret != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(ret);
		return NULL;
	}

	context = PyObject_New(PyMAPIStoreContextObject, &PyMAPIStoreContext);
	context->mem_ctx = self->mem_ctx;
	context->mstore_ctx = self->mstore_ctx;
	context->fid = fid;
	context->folder_object = folder_object;
	context->context_id = context_id;
	context->parent = self;

	Py_INCREF(context->parent);

	return (PyObject *) context;
}

/* static PyObject *py_MAPIStore_delete_context(PyMAPIStoreObject *self, PyObject *args) */
/* { */
/* 	PyMAPIStoreContextObject	*context; */
/* 	int				ret = MAPISTORE_SUCCESS; */

/* 	if (!PyArg_ParseTuple(args, "O", &context)) { */
/* 		return NULL; */
/* 	} */

/* 	/\* mapistore_del_context(context->mstore_ctx, context->context_id); *\/ */
/* 	/\* Py_XDECREF(context); *\/ */

/* 	return PyInt_FromLong(ret); */
/* } */

/* static PyObject *py_MAPIStore_search_context_by_uri(PyMAPIStoreObject *self, PyObject *args) */
/* { */
/* 	int		ret; */
/* 	uint32_t	context_id = 0; */
/* 	const char	*uri; */
/* 	void		*backend_object; */

/* 	if (!PyArg_ParseTuple(args, "s", &uri)) { */
/* 		return NULL; */
/* 	} */

/* 	ret = mapistore_search_context_by_uri(self->mstore_ctx, uri, &context_id, &backend_object); */
/* 	if (ret != MAPISTORE_SUCCESS) { */
/* 		return NULL; */
/* 	} */

/* 	return PyInt_FromLong(context_id); */
/* } */

/* static PyObject *py_MAPIStore_add_context_ref_count(PyMAPIStoreObject *self, PyObject *args) */
/* { */
/* 	uint32_t	context_id = 0; */

/* 	if (!PyArg_ParseTuple(args, "k", &context_id)) { */
/* 		return NULL; */
/* 	} */

/* 	return PyInt_FromLong(mapistore_add_context_ref_count(self->mstore_ctx, context_id)); */
/* } */

/* static PyObject *py_MAPIStore_create_folder(PyMAPIStoreObject *self, PyObject *args) */
/* { */
/* 	uint32_t		context_id; */
/* 	uint64_t		parent_fid; */
/* 	uint64_t		fid; */
/* 	PyObject		*mod_mapi; */
/* 	PyObject		*pySPropValue; */
/* 	PySPropValueObject	*SPropValue; */
/* 	struct SRow		aRow; */

/* 	mod_mapi = PyImport_ImportModule("openchange.mapi"); */
/* 	if (mod_mapi == NULL) { */
/* 		printf("Can't load module\n"); */
/* 		return NULL; */
/* 	} */
/* 	SPropValue_Type = (PyTypeObject *)PyObject_GetAttrString(mod_mapi, "SPropValue"); */
/* 	if (SPropValue_Type == NULL) { */
/* 		return NULL; */
/* 	} */

/* 	if (!PyArg_ParseTuple(args, "kKKO", &context_id, &parent_fid, &fid, &pySPropValue)) { */
/* 		return NULL; */
/* 	} */

/* 	if (!PyObject_TypeCheck(pySPropValue, SPropValue_Type)) { */
/* 		PyErr_SetString(PyExc_TypeError, "Function require SPropValue object"); */
/* 		return NULL; */
/* 	} */

/* 	SPropValue = (PySPropValueObject *)pySPropValue; */
/* 	aRow.cValues = SPropValue->cValues; */
/* 	aRow.lpProps = SPropValue->SPropValue; */

/* 	return PyInt_FromLong(mapistore_folder_create_folder(self->mstore_ctx, context_id, parent_fid, fid, &aRow)); */
/* } */

/* static PyObject *py_MAPIStore_delete_folder(PyMAPIStoreObject *self, PyObject *args) */
/* { */
/* 	uint32_t	context_id; */
/* 	uint64_t	parent_fid; */
/* 	uint64_t	fid; */
/* 	uint8_t		flags; */

/* 	if (!PyArg_ParseTuple(args, "kKKH", &context_id, &parent_fid, &fid, &flags)) { */
/* 		return NULL; */
/* 	} */

/* 	return PyInt_FromLong(mapistore_folder_delete_folder(self->mstore_ctx, context_id, parent_fid, fid, flags)); */
/* } */

/* static PyObject *py_MAPIStore_setprops(PyMAPIStoreObject *self, PyObject *args) */
/* { */
/* 	uint32_t		context_id; */
/* 	uint64_t		fid; */
/* 	uint8_t			object_type; */
/* 	PyObject		*mod_mapi; */
/* 	PyObject		*pySPropValue; */
/* 	PySPropValueObject	*SPropValue; */
/* 	struct SRow		aRow; */

/* 	mod_mapi = PyImport_ImportModule("openchange.mapi"); */
/* 	if (mod_mapi == NULL) { */
/* 		printf("Can't load module\n"); */
/* 		return NULL; */
/* 	} */
/* 	SPropValue_Type = (PyTypeObject *)PyObject_GetAttrString(mod_mapi, "SPropValue"); */
/* 	if (SPropValue_Type == NULL) { */
/* 		return NULL; */
/* 	} */

/* 	if (!PyArg_ParseTuple(args, "kKbO", &context_id, &fid, &object_type, &pySPropValue)) { */
/* 		return NULL; */
/* 	} */

/* 	if (!PyObject_TypeCheck(pySPropValue, SPropValue_Type)) { */
/* 		PyErr_SetString(PyExc_TypeError, "Function require SPropValue object"); */
/* 		return NULL; */
/* 	} */

/* 	SPropValue = (PySPropValueObject *)pySPropValue; */
/* 	aRow.cValues = SPropValue->cValues; */
/* 	aRow.lpProps = SPropValue->SPropValue; */

/* 	return PyInt_FromLong(mapistore_setprops(self->mstore_ctx, context_id, fid, object_type, &aRow)); */
/* } */

/* static PyObject *py_MAPIStore_get_folder_count(PyMAPIStoreObject *self, PyObject *args) */
/* { */
/* 	uint32_t		context_id; */
/* 	uint64_t		fid; */
/* 	uint8_t			object_type; */
/* 	uint32_t		RowCount = 0; */

/* 	if (!PyArg_ParseTuple(args, "kKb", &context_id, &fid, &object_type)) { */
/* 		return NULL; */
/* 	} */

/* 	switch (object_type) { */
/* 	case MAPISTORE_FOLDER: */
/* 		mapistore_folder_get_folder_count(self->mstore_ctx, context_id,  */
/* 						  fid, &RowCount); */
/* 		break; */
/* 	case MAPISTORE_MESSAGE: */
/* 		mapistore_folder_get_message_count(self->mstore_ctx, context_id,  */
/* 						   fid, MAPISTORE_MESSAGE_TABLE, &RowCount); */
/* 		break; */
/* 	default: */
/* 		RowCount = 0; */
/* 		break; */
/* 	} */

/* 	return PyInt_FromLong(RowCount); */
/* } */

static PyMethodDef mapistore_methods[] = {
	{ "management", (PyCFunction)py_MAPIStore_new_mgmt, METH_VARARGS },
	{ "add_context", (PyCFunction)py_MAPIStore_add_context, METH_VARARGS },
	/* { "delete_context", (PyCFunction)py_MAPIStore_delete_context, METH_VARARGS }, */
	/* { "search_context_by_uri", (PyCFunction)py_MAPIStore_search_context_by_uri, METH_VARARGS }, */
	/* { "add_context_ref_count", (PyCFunction)py_MAPIStore_add_context_ref_count, METH_VARARGS }, */
	/* { "create_folder", (PyCFunction)py_MAPIStore_create_folder, METH_VARARGS }, */
	/* { "delete_folder", (PyCFunction)py_MAPIStore_delete_folder, METH_VARARGS }, */
	/* { "setprops", (PyCFunction)py_MAPIStore_setprops, METH_VARARGS }, */
	/* { "get_folder_count", (PyCFunction)py_MAPIStore_get_folder_count, METH_VARARGS }, */
	{ NULL },
};

PyTypeObject PyMAPIStore = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapistore.MAPIStore",
	.tp_basicsize = sizeof (PyMAPIStoreObject),
	.tp_doc = "mapistore object",
	.tp_methods = mapistore_methods,
	/* .tp_getset = mapistore_getsetters, */
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

static void load_modules(void)
{
	PyObject *datetime_dict;

	globals.datetime_module = PyImport_ImportModule("datetime");
	Py_INCREF(globals.datetime_module);
	if (globals.datetime_module) {
		datetime_dict = PyModule_GetDict(globals.datetime_module);
		globals.datetime_datetime_class = PyDict_GetItemString(datetime_dict, "datetime");
		if (PyType_Check(globals.datetime_datetime_class)) {
			Py_INCREF(globals.datetime_datetime_class);
		}
		else {
			fprintf (stderr, "failure loading datetime.datetime class\n");
		}
	}
	else {
		fprintf (stderr, "failure loading datetime module\n");
	}
}

void initmapistore(void)
{
	PyObject	*m;

	memset(&globals, 0, sizeof(PyMAPIStoreGlobals));

	load_modules();

	m = Py_InitModule3("mapistore", py_mapistore_global_methods,
			   "An interface to OpenChange MAPIStore");
	if (m == NULL) {
		return;
	}

	if (PyType_Ready(&PyMAPIStore) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStore);
	PyModule_AddObject(m, "MAPIStore", (PyObject *)&PyMAPIStore);

	initmapistore_mgmt(m);
	initmapistore_context(m);
	initmapistore_folder(m);
	initmapistore_freebusy_properties(m);
	initmapistore_errors(m);
	initmapistore_table(m);
}
