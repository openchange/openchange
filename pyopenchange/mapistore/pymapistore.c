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

void PyErr_SetMAPISTATUSError(enum MAPISTATUS retval)
{
	PyErr_SetObject(PyExc_RuntimeError,
			Py_BuildValue("(i, s)", retval, mapi_get_errstr(retval)));
}

PyMAPIStoreGlobals *get_PyMAPIStoreGlobals()
{
	return &globals;
}

static enum mapistore_error sam_ldb_init(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx, const char *syspath)
{
	struct tevent_context	*ev;
	int			retval;
	struct ldb_result	*res;
	struct ldb_dn		*tmp_dn = NULL;
	static const char	*attrs[] = {
		"rootDomainNamingContext",
		"defaultNamingContext",
		NULL
	};

	/* Sanity checks */
	if (globals.samdb_ctx) {
		return MAPISTORE_SUCCESS;
	}

	ev = tevent_context_init(talloc_autofree_context());
	if (!ev) goto end;

	/* Step 1. Connect to the database */
	globals.samdb_ctx = samdb_connect(NULL, NULL, lp_ctx, system_session(lp_ctx), 0);
	if (!globals.samdb_ctx) goto end;

	/* Step 2. Search for rootDSE record */
	retval = ldb_search(globals.samdb_ctx, mem_ctx, &res, ldb_dn_new(mem_ctx, globals.samdb_ctx, "@ROOTDSE"),
			 LDB_SCOPE_BASE, attrs, NULL);
	if (retval != LDB_SUCCESS) goto end;
	if (res->count != 1) goto end;

	/* Step 3. Set opaque naming */
	tmp_dn = ldb_msg_find_attr_as_dn(globals.samdb_ctx, globals.samdb_ctx,
					 res->msgs[0], "rootDomainNamingContext");
	ldb_set_opaque(globals.samdb_ctx, "rootDomainNamingContext", tmp_dn);
	
	tmp_dn = ldb_msg_find_attr_as_dn(globals.samdb_ctx, globals.samdb_ctx,
					 res->msgs[0], "defaultNamingContext");
	ldb_set_opaque(globals.samdb_ctx, "defaultNamingContext", tmp_dn);

	return MAPISTORE_SUCCESS;

end:
	return MAPISTORE_ERR_DATABASE_INIT;
}

static PyObject *py_MAPIStore_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX			*mem_ctx;
	struct loadparm_context		*lp_ctx;
	PyMAPIStoreObject		*msobj;
	bool				ret;
	enum mapistore_error		retval;
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

	/* Initialize ldb context on sam.ldb */
	retval = sam_ldb_init(mem_ctx, lp_ctx, syspath);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		talloc_free(mem_ctx);
		return NULL;
	}

	if ((syspath != NULL) && (file_exist(syspath) == true)) {
		ret = lpcfg_load(lp_ctx, syspath);
	} else {
		ret = lpcfg_load_default(lp_ctx);
	}

	if (ret == false) {
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
	enum MAPISTATUS			ret;
	enum mapistore_error		retval;
	const char			*path = NULL;
	const char			*username = NULL;

	if (!PyArg_ParseTuple(args, "s|s", &username, &path)) {
		return NULL;
	}

	/* Initialize ldb context on openchange.ldb */
	ret = openchangedb_initialize(self->mem_ctx, self->lp_ctx, &ocdb_ctx);
	if (ret != MAPI_E_SUCCESS) {
		PyErr_SetMAPISTATUSError(ret);
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
	retval = mapistore_set_connection_info(mstore_ctx, globals.samdb_ctx, globals.ocdb_ctx, username);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
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
	enum mapistore_error		retval;
	TALLOC_CTX 			*mem_ctx;
	PyObject			*py_ret = NULL;
	const char			**backend_names;
	int 				i, list_size;

	DEBUG(0, ("List backends for user: %s\n", self->username));

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	/* list backends */
	retval = mapistore_list_backends_for_user(mem_ctx, &list_size, &backend_names);
	if (retval != MAPISTORE_SUCCESS) {
		talloc_free(mem_ctx);
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	/* Build the list */
	py_ret = PyList_New(list_size);

	for (i = 0; i < list_size; i++) {
		PyList_SetItem(py_ret, i, Py_BuildValue("s", backend_names[i]));
	}

	talloc_free(mem_ctx);
	return (PyObject *) py_ret;
}

static PyObject *py_MAPIStore_list_contexts_for_user(PyMAPIStoreObject *self)
{
	enum mapistore_error		retval;
	TALLOC_CTX 			*mem_ctx;
	PyObject			*py_ret = NULL;
	PyObject			*py_dict;
	struct mapistore_contexts_list 	*contexts_list;

	DEBUG(0, ("List contexts for user %s\n", self->username));

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	/* list contexts */
	retval = mapistore_list_contexts_for_user(self->mstore_ctx, self->username, mem_ctx, &contexts_list);
	if (retval != MAPISTORE_SUCCESS) {
		talloc_free(mem_ctx);
		PyErr_SetMAPIStoreError(retval);
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
	enum mapistore_error		retval;
	enum MAPISTATUS			ret;
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
	ret = openchangedb_get_fid(globals.ocdb_ctx, uri, &fid);
	if (ret != MAPI_E_SUCCESS) {
		PyErr_SetMAPISTATUSError(ret);
		return NULL;
	}

	retval = mapistore_add_context(self->mstore_ctx, self->username, uri, fid, &context_id, &folder_object);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
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
	const char	*errstr;

	if (!PyArg_ParseTuple(args, "k", &ret)) {
		return NULL;
	}

	errstr = mapistore_errstr(ret);
	if (errstr != NULL) {
		return PyString_FromString(errstr);
	}
	Py_RETURN_NONE;
}

static PyObject *py_mapistatus_errstr(PyObject *mod, PyObject *args)
{
	int		ret;
	const char	*errstr;

	if (!PyArg_ParseTuple(args, "k", &ret)) {
		return NULL;
	}

	errstr = mapi_get_errstr(ret);
	if (errstr != NULL) {
		return PyString_FromString(errstr);
	}
	Py_RETURN_NONE;
}

static PyMethodDef py_mapistore_global_methods[] = {
	{ "set_mapping_path", (PyCFunction)py_mapistore_set_mapping_path, METH_VARARGS },
	{ "errstr", (PyCFunction)py_mapistore_errstr, METH_VARARGS,
		"Returns mapistore_error string presentation\n\n"
		":param status: mapistore_error code\n"
		":return string: String" },
	{ "mapistatus_errstr", (PyCFunction)py_mapistatus_errstr, METH_VARARGS,
		"Returns MAPISTATUS string presentation\n\n"
		":param status: MAPISTATUS code\n"
		":return string: String or None if MAPISTATUS is unknown" },
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

	PyModule_AddObject(m, "ROLE_MAIL", PyInt_FromLong(MAPISTORE_MAIL_ROLE));
	PyModule_AddObject(m, "ROLE_DRAFTS", PyInt_FromLong(MAPISTORE_DRAFTS_ROLE));
	PyModule_AddObject(m, "ROLE_SENTITEMS", PyInt_FromLong(MAPISTORE_SENTITEMS_ROLE));
	PyModule_AddObject(m, "ROLE_OUTBOX", PyInt_FromLong(MAPISTORE_OUTBOX_ROLE));
	PyModule_AddObject(m, "ROLE_DELETEDITEMS", PyInt_FromLong(MAPISTORE_DELETEDITEMS_ROLE));
	PyModule_AddObject(m, "ROLE_CALENDAR", PyInt_FromLong(MAPISTORE_CALENDAR_ROLE));
	PyModule_AddObject(m, "ROLE_CONTACTS", PyInt_FromLong(MAPISTORE_CONTACTS_ROLE));
	PyModule_AddObject(m, "ROLE_TASKS", PyInt_FromLong(MAPISTORE_TASKS_ROLE));
	PyModule_AddObject(m, "ROLE_NOTES", PyInt_FromLong(MAPISTORE_NOTES_ROLE));
	PyModule_AddObject(m, "ROLE_JOURNAL", PyInt_FromLong(MAPISTORE_JOURNAL_ROLE));
	PyModule_AddObject(m, "ROLE_FALLBACK", PyInt_FromLong(MAPISTORE_FALLBACK_ROLE));
	PyModule_AddObject(m, "ROLE_MAX_ROLES", PyInt_FromLong(MAPISTORE_MAX_ROLES));

	initmapistore_mgmt(m);
	initmapistore_context(m);
	initmapistore_folder(m);
	initmapistore_freebusy_properties(m);
	initmapistore_errors(m);
	initmapistore_table(m);
	initmapistore_indexing(m);
}
