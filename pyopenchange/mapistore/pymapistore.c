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

/* static PyTypeObject *SPropValue_Type; */

void initmapistore(void);

static PyMAPIStoreGlobals globals;

void PyErr_SetMAPIStoreError(uint32_t retval)
{
	PyErr_SetObject(PyExc_RuntimeError,
			Py_BuildValue("(i, s)", retval, mapistore_errstr(retval)));
}

void PyErr_SetMAPISTATUSError(enum MAPISTATUS retval)
{
	PyErr_SetObject(PyExc_RuntimeError,
			Py_BuildValue("(i, s)", retval, mapi_get_errstr(retval)));
}

PyMAPIStoreGlobals *get_PyMAPIStoreGlobals()
{
	return &globals;
}

static PyObject *py_MAPIStore_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX			*mem_ctx;
	struct loadparm_context		*lp_ctx;
	PyMAPIStoreObject		*msobj;
	bool				ret_lp;
	char				*kwnames[] = { "syspath", NULL };
	const char			*syspath = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|s", kwnames, &syspath)) {
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

	if ((syspath != NULL) && (file_exist(syspath) == true)) {
		ret_lp = lpcfg_load(lp_ctx, syspath);
	} else {
		ret_lp = lpcfg_load_default(lp_ctx);
	}

	if (ret_lp == false) {
		PySys_WriteStderr("lpcfg_load unable to load content from path\n");
	}

	msobj = PyObject_New(PyMAPIStoreObject, &PyMAPIStore);
	msobj->mem_ctx = mem_ctx;
	msobj->lp_ctx = lp_ctx;
	msobj->mstore_ctx = NULL;
	msobj->username = NULL;

	return (PyObject *) msobj;
}

static void py_MAPIStore_dealloc(PyObject *_self)
{

	PyMAPIStoreObject *self = (PyMAPIStoreObject *)_self;

	mapistore_release(self->mstore_ctx);
	talloc_free(self->mem_ctx);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStore_initialize(PyMAPIStoreObject *self, PyObject *args)
{
	struct openchangedb_context 	*ocdb_ctx;
	struct mapistore_context	*mstore_ctx;
	enum MAPISTATUS			ret_ocdb;
	enum mapistore_error		ret;
	const char			*path = NULL;
	const char			*username = NULL;

	if (!PyArg_ParseTuple(args, "s|s", &username, &path)) {
		return NULL;
	}

	/* Initialize ldb context on openchange.ldb */
	ret_ocdb = openchangedb_initialize(self->mem_ctx, self->lp_ctx, &ocdb_ctx);
	if (ret_ocdb != MAPI_E_SUCCESS) {
		PyErr_SetMAPISTATUSError(ret_ocdb);
		return NULL;
	}
	globals.ocdb_ctx = ocdb_ctx;

	/* Initialize mapistore */
	mstore_ctx = mapistore_init(self->mem_ctx, self->lp_ctx, path);
	if (mstore_ctx == NULL) {
		PyErr_SetString(PyExc_SystemError,
				"Error in mapistore_init");
		return NULL;
	}

	/* set connection info */
	ret = mapistore_set_connection_info(mstore_ctx, globals.ocdb_ctx, username);
	if (ret != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(ret);
		return NULL;
	}

	self->mstore_ctx = mstore_ctx;
	self->username = talloc_strdup(self->mem_ctx, username);

	Py_RETURN_NONE;
}

static PyObject *py_MAPIStore_set_parm(PyMAPIStoreObject *self, PyObject *args)
{
	bool				set_success;
	const char			*option = NULL;
	const char			*value = NULL;

	if (!PyArg_ParseTuple(args, "ss", &option, &value)) {
		return NULL;
	}

	/* Set the value in the specified parameter */
	set_success = lpcfg_set_cmdline(self->lp_ctx, option, value);
	if (set_success == false) {
		PyErr_SetString(PyExc_SystemError,
				"Error in lpcfg_set_cmdline");
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject *py_MAPIStore_dump(PyMAPIStoreObject *self)
{
	bool 				show_defaults = false;

	if (self->lp_ctx == NULL) {
		PyErr_SetString(PyExc_SystemError,
				"Parameters not initialized");
		return NULL;
	}

	lpcfg_dump(self->lp_ctx, stdout, show_defaults, lpcfg_numservices(self->lp_ctx));

	Py_RETURN_NONE;
}

static PyObject *py_MAPIStore_list_backends_for_user(PyMAPIStoreObject *self)
{
	enum mapistore_error		ret;
	TALLOC_CTX 			*mem_ctx;
	PyObject			*py_ret = NULL;
	char				**backend_names;
	int 				i, list_size;

	DEBUG(0, ("List backends for user: %s\n", self->username));

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	/* list backends */
	ret = mapistore_list_backends_for_user(self->mstore_ctx, self->username, mem_ctx, &backend_names);
	if (ret != MAPISTORE_SUCCESS) {
		talloc_free(mem_ctx);
		PyErr_SetMAPIStoreError(ret);
		return NULL;
	}

	/* Build the list */
	list_size = talloc_array_length(backend_names);
	py_ret = PyList_New(list_size);

	for (i = 0; i < list_size; i++) {
		PyList_SetItem(py_ret, i, Py_BuildValue("s", backend_names[i]));
	}

	talloc_free(mem_ctx);
	return (PyObject *) py_ret;
}

static PyObject *py_MAPIStore_list_contexts_for_user(PyMAPIStoreObject *self)
{
	enum mapistore_error		ret;
	TALLOC_CTX 			*mem_ctx;
	PyObject			*py_ret = NULL;
	PyObject			*py_dict;
	struct mapistore_contexts_list 	*contexts_list;

	DEBUG(0, ("List contexts for user: %s\n", self->username));

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	/* list contexts */
	ret = mapistore_list_contexts_for_user(self->mstore_ctx, self->username, mem_ctx, &contexts_list);
	if (ret != MAPISTORE_SUCCESS) {
		talloc_free(mem_ctx);
		PyErr_SetMAPIStoreError(ret);
		return NULL;
	}

	py_ret = Py_BuildValue("[]");

	while (contexts_list) {
		py_dict = Py_BuildValue("{s:s, s:s, s:i, s:i}",
				"name", contexts_list->name,
				"url", contexts_list->url,
				"role", contexts_list->role,
				"main_folder", contexts_list->main_folder);
		PyList_Append(py_ret, py_dict);
		contexts_list = contexts_list->next;
	}

	talloc_free(mem_ctx);
	return (PyObject *) py_ret;
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
	enum mapistore_error		ret_mstore;
	enum MAPISTATUS			ret_mstatus;
	PyMAPIStoreContextObject	*context;
	uint32_t			context_id = 0;
	const char			*uri;
	void				*folder_object;
        uint64_t			fid = 0;

	if (!PyArg_ParseTuple(args, "s", &uri)) {
		return NULL;
	}

	/* printf("Add context: %s\n", uri); */

	/* Get FID given mapistore_uri and username */
	ret_mstatus = openchangedb_get_fid(globals.ocdb_ctx, uri, &fid);
	if (ret_mstatus != MAPI_E_SUCCESS) {
		PyErr_SetMAPISTATUSError(ret_mstatus);
		return NULL;
	}

	ret_mstore = mapistore_add_context(self->mstore_ctx, self->username, uri, fid, &context_id, &folder_object);
	if (ret_mstore != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(ret_mstore);
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
	{ "initialize", (PyCFunction)py_MAPIStore_initialize, METH_VARARGS },
	{ "set_parm", (PyCFunction)py_MAPIStore_set_parm, METH_VARARGS },
	{ "dump", (PyCFunction)py_MAPIStore_dump, METH_NOARGS },
	{ "list_backends", (PyCFunction)py_MAPIStore_list_backends_for_user, METH_NOARGS },
	{ "capabilities", (PyCFunction)py_MAPIStore_list_contexts_for_user, METH_NOARGS },
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

static int py_mapistore_set_debuglevel(PyMAPIStoreObject *self, PyObject *value, void *closure)
{
	char	*debuglevel = NULL;

	if (value == NULL) {
		PyErr_SetString(PyExc_TypeError, "Cannot delete the last attribute");
		return -1;
	}

	if (!PyInt_Check(value)) {
		PyErr_SetString(PyExc_TypeError,
				"The debuglevel attribute value must be an integer");
		return -1;
	}

	debuglevel = talloc_asprintf(self->mem_ctx, "%ld", PyInt_AsLong(value));
	if (!debuglevel) {
		PyErr_SetString(PyExc_MemoryError, "Out of memory");
		return -1;
	}
	lpcfg_set_cmdline(self->lp_ctx, "log level", debuglevel);
	talloc_free(debuglevel);

	return 0;
}

static PyGetSetDef mapistore_getsetters[] = {
	{ "debuglevel", NULL, (setter)py_mapistore_set_debuglevel, "The level of debug."},
	{ NULL }
};

PyTypeObject PyMAPIStore = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapistore.MAPIStore",
	.tp_basicsize = sizeof (PyMAPIStoreObject),
	.tp_doc = "mapistore object",
	.tp_methods = mapistore_methods,
	.tp_getset = mapistore_getsetters,
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

static PyObject *py_mapistatus_errstr(PyObject *mod, PyObject *args)
{
	int		ret;

	if (!PyArg_ParseTuple(args, "k", &ret)) {
		return NULL;
	}

	return PyString_FromString(mapi_get_errstr(ret));
}

static PyMethodDef py_mapistore_global_methods[] = {
	{ "set_mapping_path", (PyCFunction)py_mapistore_set_mapping_path, METH_VARARGS },
	{ "errstr", (PyCFunction)py_mapistore_errstr, METH_VARARGS },
	{ "mapistatus_errstr", (PyCFunction)py_mapistatus_errstr, METH_VARARGS },
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
