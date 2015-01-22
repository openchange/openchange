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

extern struct ldb_context *samdb_connect(TALLOC_CTX *, struct tevent_context *, struct loadparm_context *, struct auth_session_info *, int);

void initmapistore(void);

static PyMAPIStoreGlobals globals;

PyMAPIStoreGlobals *get_PyMAPIStoreGlobals()
{
	return &globals;
}

static enum mapistore_error sam_ldb_init(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx)
{
	struct tevent_context	*ev;
	struct ldb_result	*res;
	struct ldb_dn		*tmp_dn = NULL;
	static const char	*attrs[] = {
		"rootDomainNamingContext",
		"defaultNamingContext",
		NULL
	};
	int			retval;

	/* Sanity checks */
	if (globals.samdb_ctx) {
		return MAPISTORE_SUCCESS;
	}

	ev = tevent_context_init(talloc_autofree_context());
	MAPISTORE_RETVAL_IF(!ev, MAPISTORE_ERR_DATABASE_INIT, NULL);

	/* Step 1. Connect to the database */
	globals.samdb_ctx = samdb_connect(NULL, NULL, lp_ctx, system_session(lp_ctx), 0);
	MAPISTORE_RETVAL_IF(!globals.samdb_ctx, MAPISTORE_ERR_DATABASE_INIT, NULL);

	/* Step 2. Search for rootDSE record */
	retval = ldb_search(globals.samdb_ctx, mem_ctx, &res, ldb_dn_new(mem_ctx, globals.samdb_ctx, "@ROOTDSE"),
			 LDB_SCOPE_BASE, attrs, NULL);
	MAPISTORE_RETVAL_IF((retval != LDB_SUCCESS), MAPISTORE_ERR_DATABASE_INIT, NULL);
	MAPISTORE_RETVAL_IF((res->count != 1), MAPISTORE_ERR_DATABASE_INIT, NULL);

	/* Step 3. Set opaque naming */
	tmp_dn = ldb_msg_find_attr_as_dn(globals.samdb_ctx, globals.samdb_ctx,
					 res->msgs[0], "rootDomainNamingContext");
	ldb_set_opaque(globals.samdb_ctx, "rootDomainNamingContext", tmp_dn);
	
	tmp_dn = ldb_msg_find_attr_as_dn(globals.samdb_ctx, globals.samdb_ctx,
					 res->msgs[0], "defaultNamingContext");
	ldb_set_opaque(globals.samdb_ctx, "defaultNamingContext", tmp_dn);

	return MAPISTORE_SUCCESS;
}

static PyObject *py_MAPIStore_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX			*mem_ctx;
	char				*kwnames[] = { "syspath", NULL };
	PyMAPIStoreObject		*msobj;
	struct loadparm_context		*lp_ctx;
	const char			*syspath = NULL;
	bool				ret;

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
		DEBUG(0, ("[ERR][%s]: Error initialising loadparm context\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERROR);
		talloc_free(mem_ctx);
		return NULL;
	}

	if ((syspath != NULL) && (file_exist(syspath) == true)) {
		ret = lpcfg_load(lp_ctx, syspath);
	} else {
		ret = lpcfg_load_default(lp_ctx);
	}

	if (ret == false) {
		PySys_WriteStderr("Unable to load content from path\n");
	}

	msobj = PyObject_New(PyMAPIStoreObject, &PyMAPIStore);
	if (msobj == NULL) {
		PyErr_NoMemory();
		talloc_free(mem_ctx);
		return NULL;
	}

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
	const char			*path = NULL;
	const char			*username = NULL;
	enum MAPISTATUS			ret;
	enum mapistore_error		retval;

	if (!PyArg_ParseTuple(args, "s|s", &username, &path)) {
		return NULL;
	}

	/* Initialise ldb context on sam.ldb */
	retval = sam_ldb_init(self->mem_ctx, self->lp_ctx);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	/* Initialise ldb context on openchange.ldb */
	ret = openchangedb_initialize(self->mem_ctx, self->lp_ctx, &ocdb_ctx);
	if (ret != MAPI_E_SUCCESS) {
		PyErr_SetMAPISTATUSError(ret);
		return NULL;
	}
	globals.ocdb_ctx = ocdb_ctx;

	/* Initialize mapistore */
	mstore_ctx = mapistore_init(self->mem_ctx, self->lp_ctx, path);
	if (mstore_ctx == NULL) {
		DEBUG(0, ("[ERR][%s]: MAPIStore initialization failed\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERROR);
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
	const char			*option = NULL;
	const char			*value = NULL;
	bool				set_success;

	if (!PyArg_ParseTuple(args, "ss", &option, &value)) {
		return NULL;
	}

	/* Set the value in the specified parameter */
	set_success = lpcfg_set_cmdline(self->lp_ctx, option, value);
	if (set_success == false) {
		DEBUG(0, ("[ERR][%s]: Error setting the parameter\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERROR);
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject *py_MAPIStore_dump(PyMAPIStoreObject *self)
{
	bool 				show_defaults = false;

	if (self->lp_ctx == NULL) {
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_NOT_INITIALIZED);
		return NULL;
	}

	lpcfg_dump(self->lp_ctx, stdout, show_defaults, lpcfg_numservices(self->lp_ctx));

	Py_RETURN_NONE;
}

static PyObject *py_MAPIStore_list_backends_for_user(PyMAPIStoreObject *self)
{
	TALLOC_CTX 			*mem_ctx;
	PyObject			*py_ret = NULL;
	const char			**backend_names;
	enum mapistore_error		retval;
	int 				i, list_size, ret;

	DEBUG(0, ("List backends for user: %s\n", self->username));

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	/* list backends */
	retval = mapistore_list_backends_for_user(mem_ctx, &list_size, &backend_names);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		goto end;
	}

	/* Build the list */
	py_ret = PyList_New(list_size);
	if (py_ret == NULL) {
		PyErr_NoMemory();
		goto end;
	}

	for (i = 0; i < list_size; i++) {
		ret = PyList_SetItem(py_ret, i, Py_BuildValue("s", backend_names[i]));
		if (ret != 0) {
			DEBUG(0,("[ERR][%s]: Unable to set item list\n", __location__));
			PyErr_SetMAPIStoreError(MAPISTORE_ERROR);
			Py_DECREF(py_ret);
			goto end;
		}
	}

	talloc_free(mem_ctx);
	return py_ret;
end:
	talloc_free(mem_ctx);
	return NULL;
}

static PyObject *py_MAPIStore_list_contexts_for_user(PyMAPIStoreObject *self)
{
	TALLOC_CTX 			*mem_ctx;
	PyObject			*py_ret = NULL, *py_dict;
	struct mapistore_contexts_list 	*contexts_list;
	enum mapistore_error		retval;

	if ((self->mstore_ctx == NULL) || (self->username == NULL)) {
		DEBUG(0,("[ERR][%s]: Can't list capabilities before initialising MAPIStore\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_NOT_INITIALIZED);
		return NULL;
	}

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	/* list contexts */
	retval = mapistore_list_contexts_for_user(self->mstore_ctx, self->username, mem_ctx, &contexts_list);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		talloc_free(mem_ctx);
		return NULL;
	}

	py_ret = Py_BuildValue("[]");

	while (contexts_list) {
		py_dict = Py_BuildValue("{s:s, s:s, s:i, s:i}",
				"name", contexts_list->name,
				"url", contexts_list->url,
				"role", contexts_list->role,
				"main_folder", contexts_list->main_folder);
		if (py_dict) {
			PyList_Append(py_ret, py_dict);
		}
		contexts_list = contexts_list->next;
	}

	talloc_free(mem_ctx);
	return (PyObject *) py_ret;
}

static PyObject *py_MAPIStore_new_mgmt(PyMAPIStoreObject *self, PyObject *args)
{
	PyMAPIStoreMGMTObject	*obj;

	obj = PyObject_New(PyMAPIStoreMGMTObject, &PyMAPIStoreMGMT);
	if (obj == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

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
	PyMAPIStoreContextObject	*context;
	void				*folder_object;
	const char			*uri;
	enum mapistore_error		retval;
	enum MAPISTATUS			ret;
        uint64_t			fid = 0;
	uint32_t			context_id = 0;

	if (!PyArg_ParseTuple(args, "s", &uri)) {
		return NULL;
	}

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
	if (context == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	context->mem_ctx = self->mem_ctx;
	context->mstore_ctx = self->mstore_ctx;
	context->fid = fid;
	context->folder_object = folder_object;
	context->context_id = context_id;
	context->parent = self;

	Py_INCREF(context->parent);

	return (PyObject *) context;
}

static PyMethodDef mapistore_methods[] = {
	{ "initialize", (PyCFunction)py_MAPIStore_initialize, METH_VARARGS },
	{ "set_parm", (PyCFunction)py_MAPIStore_set_parm, METH_VARARGS },
	{ "dump", (PyCFunction)py_MAPIStore_dump, METH_NOARGS },
	{ "list_backends", (PyCFunction)py_MAPIStore_list_backends_for_user, METH_NOARGS },
	{ "capabilities", (PyCFunction)py_MAPIStore_list_contexts_for_user, METH_NOARGS },
	{ "management", (PyCFunction)py_MAPIStore_new_mgmt, METH_VARARGS },
	{ "add_context", (PyCFunction)py_MAPIStore_add_context, METH_VARARGS },
	{ NULL },
};

static int py_mapistore_set_debuglevel(PyMAPIStoreObject *self, PyObject *value, void *closure)
{
	char	*debuglevel = NULL;
	bool    ret;

	if (value == NULL) {
		DEBUG(0, ("[ERR][%s]: Cannot delete the 'debug' attribute\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return -1;
	}

	if (!PyInt_Check(value)) {
		DEBUG(0, ("[ERR][%s]: The debuglevel attribute value must be an integer\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return -1;
	}

	debuglevel = talloc_asprintf(self->mem_ctx, "%ld", PyInt_AsLong(value));
	if (!debuglevel) {
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_NO_MEMORY);
		return -1;
	}

	ret = lpcfg_set_cmdline(self->lp_ctx, "log level", debuglevel);
	talloc_free(debuglevel);

	return (ret == true) ? 0 : -1;
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


static PyObject *py_mapistore_isPtypServerId(PyMAPIStoreObject *self, PyObject *args)
{
	char		*sproptag = NULL;
	uint32_t	proptag = 0;
	bool		retval = false;

	if (!PyArg_ParseTuple(args, "s", &sproptag)) {
		return NULL;
	}

	if (strcasestr(sproptag, "0x")) {
		proptag = strtoul(sproptag, NULL, 16);
	} else {
		proptag = openchangedb_property_get_tag(sproptag);
		if (proptag == 0xFFFFFFFF) {
			proptag = openchangedb_named_properties_get_tag(sproptag);
			if (proptag == 0xFFFFFFFF) {
				proptag = strtoul(sproptag, NULL, 16);
			}
		}
	}


	if ((proptag & 0xFFFF) == PT_SVREID) {
		retval = true;
	}

	return PyBool_FromLong((uint32_t)retval);
}



static PyObject *py_mapistore_isPtypBinary(PyMAPIStoreObject *self, PyObject *args)
{
	char		*sproptag = NULL;
	uint32_t	proptag = 0;
	bool		retval = false;

	if (!PyArg_ParseTuple(args, "s", &sproptag)) {
		return NULL;
	}

	if (strcasestr(sproptag, "0x")) {
		proptag = strtoul(sproptag, NULL, 16);
	} else {
		proptag = openchangedb_property_get_tag(sproptag);
		if (proptag == 0xFFFFFFFF) {
			proptag = openchangedb_named_properties_get_tag(sproptag);
			if (proptag == 0xFFFFFFFF) {
				proptag = strtoul(sproptag, NULL, 16);
			}
		}
	}


	if ((proptag & 0xFFFF) == PT_BINARY) {
		retval = true;
	}

	return PyBool_FromLong((uint32_t)retval);
}


static PyObject *py_mapistore_isPtypMVBinary(PyMAPIStoreObject *self, PyObject *args)
{
	char		*sproptag = NULL;
	uint32_t	proptag = 0;
	bool		retval = false;

	if (!PyArg_ParseTuple(args, "s", &sproptag)) {
		return NULL;
	}

	if (strcasestr(sproptag, "0x")) {
		proptag = strtoul(sproptag, NULL, 16);
	} else {
		proptag = openchangedb_property_get_tag(sproptag);
		if (proptag == 0xFFFFFFFF) {
			proptag = openchangedb_named_properties_get_tag(sproptag);
			if (proptag == 0xFFFFFFFF) {
				proptag = strtoul(sproptag, NULL, 16);
			}
		}
	}


	if ((proptag & 0xFFFF) == PT_MV_BINARY) {
		retval = true;
	}

	return PyBool_FromLong((uint32_t)retval);
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
	{ "isPtypServerId", (PyCFunction)py_mapistore_isPtypServerId, METH_VARARGS,
		"Check if the property is of type PT_SRVEID\n\n"
		":param paroperty: Property tag to lookup\n"
		":return boolean: Boolean" },
	{ "isPtypBinary", (PyCFunction)py_mapistore_isPtypBinary, METH_VARARGS,
		"Check if the property is of type PT_BINARY\n\n"
		":param paroperty: Property tag to lookup\n"
		":return boolean: Boolean" },
	{ "isPtypMVBinary", (PyCFunction)py_mapistore_isPtypMVBinary, METH_VARARGS,
		"Check if the property is of type PT_BINARY\n\n"
		":param paroperty: Property tag to lookup\n"
		":return boolean: Boolean" },
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
	initmapistore_attachment(m);
	initmapistore_message(m);
	initmapistore_freebusy_properties(m);
	initmapistore_errors(m);
	initmapistore_table(m);
	initmapistore_indexing(m);
}
