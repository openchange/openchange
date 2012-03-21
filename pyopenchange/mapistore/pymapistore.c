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

/* static PyTypeObject *SPropValue_Type; */

void initmapistore(void);

/* static struct ldb_context	*sam_ldb_ctx = NULL; */
static struct ldb_context	*openchange_ldb_ctx = NULL;

void PyErr_SetMAPIStoreError(uint32_t retval)
{
	PyErr_SetObject(PyExc_RuntimeError,
			Py_BuildValue("(i, s)", retval, mapistore_errstr(retval)));
}

/* static void *sam_ldb_init(TALLOC_CTX *mem_ctx, const char *syspath) */
/* { */
/* 	char			*ldb_path; */
/* 	struct tevent_context	*ev; */
/* 	int			ret; */
/* 	struct ldb_result	*res; */
/* 	struct ldb_dn		*tmp_dn = NULL; */
/* 	static const char	*attrs[] = { */
/* 		"rootDomainNamingContext", */
/* 		"defaultNamingContext", */
/* 		NULL */
/* 	}; */

/* 	/\* Sanity checks *\/ */
/* 	if (sam_ldb_ctx) return sam_ldb_ctx; */

/* 	ev = tevent_context_init(talloc_autofree_context()); */
/* 	if (!ev) return NULL; */

/* 	/\* Step 1. Retrieve a LDB context pointer on sam.ldb database *\/ */
/* 	ldb_path = talloc_asprintf(mem_ctx, "%s/sam.ldb", syspath); */
/* 	sam_ldb_ctx = ldb_init(mem_ctx, ev); */
/* 	if (!sam_ldb_ctx) return NULL; */

/* 	/\* Step 2. Connect to the database *\/ */
/* 	ret = ldb_connect(sam_ldb_ctx, ldb_path, 0, NULL); */
/* 	talloc_free(ldb_path); */
/* 	if (ret != LDB_SUCCESS) return NULL; */

/* 	/\* Step 3. Search for rootDSE record *\/ */
/* 	ret = ldb_search(sam_ldb_ctx, mem_ctx, &res, ldb_dn_new(mem_ctx, sam_ldb_ctx, "@ROOTDSE"), */
/* 			 LDB_SCOPE_BASE, attrs, NULL); */
/* 	if (ret != LDB_SUCCESS) return NULL; */
/* 	if (res->count != 1) return NULL; */

/* 	/\* Step 4. Set opaque naming *\/ */
/* 	tmp_dn = ldb_msg_find_attr_as_dn(sam_ldb_ctx, sam_ldb_ctx, */
/* 					 res->msgs[0], "rootDomainNamingContext"); */
/* 	ldb_set_opaque(sam_ldb_ctx, "rootDomainNamingContext", tmp_dn); */
	
/* 	tmp_dn = ldb_msg_find_attr_as_dn(sam_ldb_ctx, sam_ldb_ctx, */
/* 					 res->msgs[0], "defaultNamingContext"); */
/* 	ldb_set_opaque(sam_ldb_ctx, "defaultNamingContext", tmp_dn); */

/* 	return sam_ldb_ctx; */

/* } */

static void *openchange_ldb_init(TALLOC_CTX *mem_ctx, const char *syspath)
{
	char			*ldb_path;
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
	if (openchange_ldb_ctx) return openchange_ldb_ctx;

	ev = tevent_context_init(talloc_autofree_context());
	if (!ev) return NULL;

	/* Step 1. Retrieve a LDB context pointer on openchange.ldb database */
	ldb_path = talloc_asprintf(mem_ctx, "%s/openchange.ldb", syspath);
	openchange_ldb_ctx = ldb_init(mem_ctx, ev);
	if (!openchange_ldb_ctx) return NULL;

	/* Step 2. Connect to the database */
	ret = ldb_connect(openchange_ldb_ctx, ldb_path, 0, NULL);
	talloc_free(ldb_path);
	if (ret != LDB_SUCCESS) return NULL;

	/* Step 3. Search for rootDSE record */
	ret = ldb_search(openchange_ldb_ctx, mem_ctx, &res, ldb_dn_new(mem_ctx, openchange_ldb_ctx, "@ROOTDSE"),
			 LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS) return NULL;
	if (res->count != 1) return NULL;

	/* Step 4. Set opaque naming */
	tmp_dn = ldb_msg_find_attr_as_dn(openchange_ldb_ctx, openchange_ldb_ctx, 
					 res->msgs[0], "rootDomainNamingContext");
	ldb_set_opaque(openchange_ldb_ctx, "rootDomainNamingContext", tmp_dn);
	
	tmp_dn = ldb_msg_find_attr_as_dn(openchange_ldb_ctx, openchange_ldb_ctx,
					 res->msgs[0], "defaultNamingContext");
	ldb_set_opaque(openchange_ldb_ctx, "defaultNamingContext", tmp_dn);

	return openchange_ldb_ctx;

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
	struct ldb_context		*samdb_ctx = NULL;
	struct ldb_context		*ocdb_ctx = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|s", kwnames, &syspath, &path)) {
		return NULL;
	}

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	/* Initialize ldb context on sam.ldb */
/*	samdb_ctx = sam_ldb_init(mem_ctx, syspath);
	if (samdb_ctx == NULL) {
		printf("Error in sam_ldb_init\n");
		talloc_free(mem_ctx);
		return NULL;
	}
*/
	/* Initialize ldb context on openchange.ldb */
	ocdb_ctx = openchange_ldb_init(mem_ctx, syspath);
	if (ocdb_ctx == NULL) {
		printf("Error in openchange_ldb_init\n");
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Initialize configuration */
	lp_ctx = loadparm_init(mem_ctx);
	lpcfg_load_default(lp_ctx);

	/* Initialize mapistore */
	mstore_ctx = mapistore_init(mem_ctx, lp_ctx, path);
	if (mstore_ctx == NULL) {
		printf("Error in mapistore_init\n");
		talloc_free(mem_ctx);
		return NULL;
	}

	msobj = PyObject_New(PyMAPIStoreObject, &PyMAPIStore);
	msobj->mem_ctx = mem_ctx;
	msobj->mstore_ctx = mstore_ctx;
	msobj->samdb_ctx = samdb_ctx;
	msobj->ocdb_ctx = ocdb_ctx;

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
		PyErr_MAPIStore_IS_ERR_RAISE(MAPISTORE_ERR_NOT_INITIALIZED);
		return NULL;
	}
	obj->mem_ctx = self->mem_ctx;
	obj->parent = self;

	Py_INCREF(self);

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
        uint64_t			fid = 0;

	if (!PyArg_ParseTuple(args, "ss", &uri, &username)) {
		return NULL;
	}

	printf("Add context: %s\n", uri);

	/* Initialize connection info */
	ret = mapistore_set_connection_info(self->mstore_ctx, self->samdb_ctx, self->ocdb_ctx, username);
	if (ret != MAPISTORE_SUCCESS) {
		PyErr_MAPIStore_IS_ERR_RAISE(ret)
		return NULL;
	}

	/* Get FID given mapistore_uri and username */
	ret = openchangedb_get_fid(self->ocdb_ctx, uri, &fid);
	if (ret != MAPISTORE_SUCCESS) {
		PyErr_MAPIStore_IS_ERR_RAISE(ret)
		return NULL;
	}

	ret = mapistore_add_context(self->mstore_ctx, username, uri, fid, &context_id, &folder_object);
	if (ret != MAPISTORE_SUCCESS) {
		PyErr_MAPIStore_IS_ERR_RAISE(ret)
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

static PyObject *py_MAPIStore_delete_context(PyMAPIStoreObject *self, PyObject *args)
{
	PyMAPIStoreContextObject	*context;
	int				ret = MAPISTORE_SUCCESS;

	if (!PyArg_ParseTuple(args, "O", &context)) {
		return NULL;
	}

	mapistore_del_context(context->mstore_ctx, context->context_id);
	Py_CLEAR(context);
	return PyInt_FromLong(ret);
}

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
	{ "delete_context", (PyCFunction)py_MAPIStore_delete_context, METH_VARARGS },
	/* { "search_context_by_uri", (PyCFunction)py_MAPIStore_search_context_by_uri, METH_VARARGS }, */
	/* { "add_context_ref_count", (PyCFunction)py_MAPIStore_add_context_ref_count, METH_VARARGS }, */
	/* { "create_folder", (PyCFunction)py_MAPIStore_create_folder, METH_VARARGS }, */
	/* { "delete_folder", (PyCFunction)py_MAPIStore_delete_folder, METH_VARARGS }, */
	/* { "setprops", (PyCFunction)py_MAPIStore_setprops, METH_VARARGS }, */
	/* { "get_folder_count", (PyCFunction)py_MAPIStore_get_folder_count, METH_VARARGS }, */
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

	if (PyType_Ready(&PyMAPIStoreMGMT) < 0) {
		return;
	}

	if (PyType_Ready(&PyMAPIStoreContext) < 0) {
		return;
	}

	if (PyType_Ready(&PyMAPIStoreFolder) < 0) {
		return;
	}

	if (PyType_Ready(&PyMAPIStoreTable) < 0) {
		return;
	}

	m = Py_InitModule3("mapistore", py_mapistore_global_methods,
			   "An interface to OpenChange MAPIStore");
	if (m == NULL) {
		return;
	}

	PyModule_AddObject(m, "FOLDER_GENERIC", PyInt_FromLong(0x1));
	PyModule_AddObject(m, "FOLDER_SEARCH", PyInt_FromLong(0x2));

	PyModule_AddObject(m, "NONE", PyInt_FromLong(0x0));
	PyModule_AddObject(m, "OPEN_IF_EXISTS", PyInt_FromLong(0x1));

	Py_INCREF(&PyMAPIStore);

	PyModule_AddObject(m, "mapistore", (PyObject *)&PyMAPIStore);
}
