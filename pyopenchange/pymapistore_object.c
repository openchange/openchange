/*
   OpenChange MAPI implementation.

   Python interface to mapistore

   Copyright (C) Julien Kerihuel 2011.

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

static PyObject *py_MAPIStore_add_context(PyMAPIStoreObject *self, PyObject *args, PyObject *kwargs)
{
	enum MAPISTORE_ERROR	retval;
	uint32_t		context_id = 0;
	const char * const	kwnames[] = { "username", "uri", NULL };
	const char		*uri;
	const char		*username;
	uint64_t		folderID;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ss", 
					 discard_const_p(char *, kwnames),
					 &username, &uri)) {
		return NULL;
	}

	/* Step 1. Add a mapistore context */
	retval = mapistore_add_context(self->mstore_ctx, username, username, uri, &context_id);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetString(PyExc_RuntimeError, "Failed to create mapistore context");
		return NULL;
	}

	/* Step 2. Retrieve the URI associated to the context URI */
	retval = mapistore_get_folder_identifier_from_uri(self->mstore_ctx, context_id, uri, &folderID);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetString(PyExc_KeyError, "Missing folder identifier");
		return NULL;
	}

	return Py_BuildValue("iK", context_id, folderID);
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

static PyObject *py_MAPIStore_get_mapistore_uri(PyMAPIStoreObject *self, PyObject *args)
{
	enum MAPISTORE_ERROR		retval;
	enum MAPISTORE_DFLT_FOLDERS	dflt_folder;
	uint32_t			index;
	const char			*username;
	const char			*ns;
	char				*uri;

	if (!PyArg_ParseTuple(args, "iss", &index, &username, &ns)) {
		return NULL;
	}

	dflt_folder = (enum MAPISTORE_DFLT_FOLDERS)index;
	retval = mapistore_create_uri(self->mstore_ctx, dflt_folder, ns, username, &uri);
	if (retval) {
		return NULL;
	}

	return PyString_FromString(uri);
}

static PyObject *py_MAPIStore_set_mapistore_uri(PyMAPIStoreObject *self, PyObject *args)
{
	uint32_t		context_id = 0;
	uint32_t		index;
	const char		*mapistore_uri;

	if (!PyArg_ParseTuple(args, "iis", &context_id, &index, &mapistore_uri)) {
		return NULL;
	}

	return PyInt_FromLong(mapistore_set_mapistore_uri(self->mstore_ctx, context_id, index, mapistore_uri));
}


static PyObject *py_MAPIStore_get_folder_identifier(PyMAPIStoreObject *self, PyObject *args, PyObject *kwargs)
{
	enum MAPISTORE_ERROR		retval;
	const char * const		kwnames[] = { "context_id", "index", "uri", NULL };
	char				*uri;
	enum MAPISTORE_DFLT_FOLDERS	index;
	uint32_t			_index;
	uint32_t			context_id;
	uint64_t			folderID;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "iiz",
					 discard_const_p(char *, kwnames),
					 &context_id, &_index, 
					 discard_const_p(char *, &uri))) {
		return NULL;
	}

	index = (enum MAPISTORE_DFLT_FOLDERS) _index;

	if (index < 1 && !uri) {
		PyErr_SetString(PyExc_TypeError, "Expected a valid MDB default folder index");
		return NULL;
	}

	if ((uri && index) || (!uri && !index)) {
		PyErr_SetString(PyExc_TypeError, "Expected either a MDB folder index or an uri");
		return NULL;
	}

	if (index) {
		retval = mapistore_create_context_uri(self->mstore_ctx, context_id, index, &uri);
		if (retval != MAPISTORE_SUCCESS) {
			PyErr_SetString(PyExc_TypeError, "Unable to retrieve URI for specified index");
			return NULL;
		}
	}

	retval = mapistore_get_folder_identifier_from_uri(self->mstore_ctx, context_id, uri, &folderID);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetString(PyExc_TypeError, "Unable to retrieve folder identifier for specified index/URI");
		return NULL;
	}

	return Py_BuildValue("K", folderID);
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

static PyObject *py_MAPIStore_opendir(PyMAPIStoreObject *self, PyObject *args, PyObject *kwargs)
{
	enum MAPISTORE_ERROR	retval;
	uint32_t		context_id;
	uint64_t		parent_fid;
	uint64_t		fid;
	char			*kwnames[] = { "context_id", "parent_fid", "fid", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "iKK", 
					 discard_const_p(char *, kwnames),
					 &context_id, &parent_fid, &fid)) {
		return NULL;
	}

	retval = mapistore_opendir(self->mstore_ctx, context_id, parent_fid, fid);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetString(PyExc_RuntimeError, "Failed to open folder");
		return NULL;
	}

	return PyInt_FromLong(retval);
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

static PyObject *py_MAPIStore_mkdir(PyMAPIStoreObject *self, PyObject *args, PyObject *kwargs)
{
	enum MAPISTORE_ERROR	retval;
	uint32_t		context_id;
	uint64_t		parent_fid;
	uint64_t		folder_fid = 0;
	const char		*folder_name = NULL;
	const char		*folder_desc = NULL;
	uint32_t		folder_type;
	char			*kwnames[] = { "context_id", "parent_fid", "name", "description", "type", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "iKszi", kwnames, &context_id, 
					 &parent_fid, &folder_name, &folder_desc, &folder_type)) {
		return NULL;
	}

	retval = mapistore_mkdir(self->mstore_ctx, context_id, parent_fid, folder_name, folder_desc, folder_type, &folder_fid);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetString(PyExc_TypeError, "Failed to create folder");
		return NULL;
	}

	return Py_BuildValue("K", folder_fid);
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

static PyObject *py_MAPIStore_get_folder_count(PyMAPIStoreObject *self, PyObject *args)
{
	uint32_t		context_id;
	uint64_t		fid;
	uint32_t		folder_count;
	enum MAPISTORE_ERROR	retval;

	if (!PyArg_ParseTuple(args, "iK", &context_id, &fid)) {
		return NULL;
	}

	retval = mapistore_get_folder_count(self->mstore_ctx, context_id, fid, &folder_count);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetString(PyExc_RuntimeError, mapistore_errstr(retval));
		return NULL;
	}
	
	return PyInt_FromLong(folder_count);
}

static PyObject *py_MAPIStore_get_message_count(PyMAPIStoreObject *self, PyObject *args)
{
	uint32_t		context_id;
	uint64_t		fid;
	uint32_t		message_count;
	enum MAPISTORE_ERROR	retval;

	if (!PyArg_ParseTuple(args, "iK", &context_id, &fid)) {
		return NULL;
	}

	retval = mapistore_get_message_count(self->mstore_ctx, context_id, fid, &message_count);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetString(PyExc_RuntimeError, mapistore_errstr(retval));
		return NULL;
	}
	
	return PyInt_FromLong(message_count);
}

static PyObject *py_MAPIStore_errstr(PyMAPIStoreObject *self, PyObject *args)
{
	enum MAPISTORE_ERROR	retval;
	int			ret;

	if (!PyArg_ParseTuple(args, "i", &ret)) {
		return NULL;
	}

	retval = (enum MAPISTORE_ERROR) ret;
	return PyString_FromString(mapistore_errstr(retval));
}

static PyObject *py_MAPIStore_get_debuglevel(PyMAPIStoreObject *self, void *py_data)
{
	enum MAPISTORE_ERROR	retval;
	uint32_t		value = 0;

	retval = mapistore_get_debuglevel(self->mstore_ctx, &value);
	if (retval) {
		return PyInt_FromLong(0);
	}

	return PyInt_FromLong(value);
}

static int py_MAPIStore_set_debuglevel(PyMAPIStoreObject *self, PyObject *value, void *py_data)
{
	enum MAPISTORE_ERROR	retval;
	uint32_t		debuglevel;

	if (value == NULL) {
		PyErr_SetString(PyExc_TypeError, "Cannot set debuglevel");
		return -1;
	}
	
	if (!PyInt_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "The attribute must be an integer");
		return -1;
	}

	debuglevel = PyInt_AsLong(value);
	retval = mapistore_set_debuglevel(self->mstore_ctx, debuglevel);
	if (retval) return -1;

	return 0;
}

static PyMethodDef mapistore_methods[] = {
	{ "add_context", (PyCFunction)py_MAPIStore_add_context, METH_KEYWORDS },
	{ "del_context", (PyCFunction)py_MAPIStore_del_context, METH_VARARGS },
	{ "root_mkdir", (PyCFunction)py_MAPIStore_root_mkdir, METH_KEYWORDS },
	{ "get_mapistore_uri", (PyCFunction)py_MAPIStore_get_mapistore_uri, METH_VARARGS },
	{ "set_mapistore_uri", (PyCFunction)py_MAPIStore_set_mapistore_uri, METH_VARARGS },
	{ "get_folder_identifier", (PyCFunction)py_MAPIStore_get_folder_identifier, METH_KEYWORDS },
	{ "add_context_idexing", (PyCFunction)py_MAPIStore_add_context_indexing, METH_VARARGS },
	{ "search_context_by_uri", (PyCFunction)py_MAPIStore_search_context_by_uri, METH_VARARGS },
	{ "add_context_ref_count", (PyCFunction)py_MAPIStore_add_context_ref_count, METH_VARARGS },
	{ "opendir", (PyCFunction)py_MAPIStore_opendir, METH_KEYWORDS },
	{ "closedir", (PyCFunction)py_MAPIStore_closedir, METH_VARARGS },
	{ "mkdir", (PyCFunction)py_MAPIStore_mkdir, METH_KEYWORDS },
	{ "rmdir", (PyCFunction)py_MAPIStore_rmdir, METH_VARARGS },
	{ "setprops", (PyCFunction)py_MAPIStore_setprops, METH_VARARGS },
	{ "get_folder_count", (PyCFunction)py_MAPIStore_get_folder_count, METH_VARARGS },
	{ "get_message_count", (PyCFunction)py_MAPIStore_get_message_count, METH_VARARGS },
	{ "errstr", (PyCFunction)py_MAPIStore_errstr, METH_VARARGS },
	{ NULL },
};

static PyGetSetDef mapistore_getsetters[] = {
	{ "debuglevel", (getter)py_MAPIStore_get_debuglevel, (setter)py_MAPIStore_set_debuglevel, "Debug Level", NULL },
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
