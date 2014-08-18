/*
   OpenChange Storage Abstraction Python Gateway

   OpenChange Project

   Copyright (C) Julien Kerihuel 2012-2014

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

#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <err.h>

#include "mapistore.h"
#include "mapistore_errors.h"
#include "mapistore_private.h"
#include <dlinklist.h>

/**
   \file mapistore_python.c

   \brief Implement a Python-C gateway providing backend implementors
   the ability to write mapistore backends using .py files
 */


/**
   \details Return the default installation path for mapistore python
   backends

   \return full path for mapistore python backends folder
 */
static const char *mapistore_python_get_installdir(void)
{
	return MAPISTORE_PYTHON_INSTALLDIR;
}

static enum mapistore_error mapistore_set_pypath(char *path)
{
	TALLOC_CTX	*mem_ctx;
	char		*env_path = NULL;

	mem_ctx = talloc_named(NULL, 0, "mapistore_set_pypath");
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	env_path = talloc_asprintf(mem_ctx, "%s:%s:%s/site-packages:%s:%s", path,
				   MAPISTORE_PYTHON_STDDIR,
				   MAPISTORE_PYTHON_STDDIR,
				   MAPISTORE_PYTHON_OTHERDIR,
				   Py_GetPath());
	MAPISTORE_RETVAL_IF(!env_path, MAPISTORE_ERR_NO_MEMORY, NULL);

	PySys_SetPath(env_path);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}

/**
   \details Initialize python backend

   \param module_name the name of the module

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_backend_init(const char *module_name)
{
	enum mapistore_error	retval;
	PyObject		*module;
	PyObject		*backend;
	PyObject		*pinst;
	PyObject		*pres;

	/* Import the module */
	module = PyImport_ImportModule(module_name);
	if (module == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to load python module: ",
			  module_name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_BACKEND_INIT;
	}

	/* Retrieve the backend object */
	backend = PyObject_GetAttrString(module, "BackendObject");
	if (backend == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to retrieve BackendObject\n",
			  module_name, __location__));
		Py_DECREF(module);
		return MAPISTORE_ERR_BACKEND_INIT;
	}

	/* Instantiate BackenObject and implicitly call __init__ */
	pinst = PyObject_CallFunction(backend, NULL);
	if (pinst == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: __init__ failed\n",
			  module_name, __location__));
		PyErr_Print();
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_BACKEND_INIT;
	}

	/* Call init function */
	pres = PyObject_CallMethod(pinst, "init", NULL);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: init failed\n",
			  module_name, __location__));
		PyErr_Print();
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_BACKEND_INIT;
	}

	retval = PyLong_AsLong(pres);
	if (retval == -1) {
		DEBUG(0, ("[ERR][%s][%s]: Overflow error\n",
			  module_name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_BACKEND_INIT;
	}

	Py_DECREF(pres);
	Py_DECREF(pinst);
	Py_DECREF(backend);
	Py_DECREF(module);
	return retval;
}


/**
   \details List available contexts (capabilities) for the backend

   \param mem_ctx pointer to the memory context
   \param module_name the name of the mapistore python backend
   \param username the name of the user
   \param ictx pointer to the indexing context
   \param capabilities pointer on pointer to the list of available
   mapistore contexts to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_backend_list_contexts(TALLOC_CTX *mem_ctx,
								   const char *module_name,
								   const char *username,
								   struct indexing_context *ictx,
								   struct mapistore_contexts_list **mclist)
{
	enum mapistore_error		retval;
	PyObject			*module;
	PyObject			*backend, *pres, *pinst;
	PyObject			*res;
	PyObject			*dict;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!module_name, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(!mclist, MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Import the module */
	module = PyImport_ImportModule(module_name);
	if (module == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to load python module: ",
			  module_name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve backend object */
	backend = PyObject_GetAttrString(module, "BackendObject");
	if (backend == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to retrieve BackendObject\n",
			  module_name, __location__));
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Instantiate BackendObject and implicitly call __init__ */
	pinst = PyObject_CallFunction(backend, NULL);
	if (pinst == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: __init__ failed\n", module_name, __location__));
		PyErr_Print();
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Call list_contexts function */
	pres = PyObject_CallMethod(pinst, "list_contexts", "s", username);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: list_contexts failed\n", module_name, __location__));
		PyErr_Print();
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Ensure a tuple was returned */
	if (PyTuple_Check(pres) == false) {
		DEBUG(0, ("[ERR][%s][%s]: Tuple expected to be returned in list_contexts\n",
			  module_name, __location__));
		Py_DECREF(pres);
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve return value (item 0 of the tuple) */
	res = PyTuple_GetItem(pres, 0);
	if (res == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed\n",
			  module_name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	retval = PyLong_AsLong(res);
	if (retval != MAPISTORE_SUCCESS) {
		if (retval == -1) {
			DEBUG(0, ("[ERR][%s][%s]: Overflow error\n", module_name, __location__));
		}
		Py_DECREF(res);
		Py_DECREF(pres);
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}
	Py_DECREF(res);

	/* Retrieve dictionary object (item 1 of the tuple) */
	dict = PyTuple_GetItem(pres, 1);
	if (dict == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed\n", module_name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* FIXME: Unpack the dictionary and map it to mapistore_contexts_list */

	return MAPISTORE_ERR_NOT_FOUND;

	return MAPISTORE_SUCCESS;
}


/**
   \details Create a context

   \param mem_ctx pointer to the memory context
   \param module_name the name of the mapistore python backend
   \param conn pointer to mapistore connection information structure
   \param ictx pointer to the indexing context
   \param uri point to the URI to create a context for
   \param context_obj pointer on pointer to the context object to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_backend_create_context(TALLOC_CTX *mem_ctx,
								    const char *module_name,
								    struct mapistore_connection_info *conn,
								    struct indexing_context *ictx,
								    const char *uri,
								    void **context_obj)
{
	enum mapistore_error		retval;
	struct mapistore_python_object	*pyobj;
	PyObject			*module;
	PyObject			*backend, *pres, *pinst;
	PyObject			*res, *robj;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!module_name, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(!context_obj, MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Import the module */
	module = PyImport_ImportModule(module_name);
	if (module == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to load python module: ",
			  module_name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve backend object */
	backend = PyObject_GetAttrString(module, "BackendObject");
	if (backend == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to retrieve BackendObject\n",
			  module_name, __location__));
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Instantiate BackendObject and implicitly call __init__ */
	pinst = PyObject_CallFunction(backend, NULL);
	if (pinst == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: __init__ failed\n", module_name, __location__));
		PyErr_Print();
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Call create_context function */
	pres = PyObject_CallMethod(pinst, "create_context", "s", uri);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: create_context failed\n",
			  module_name, __location__));
		PyErr_Print();
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Ensure a tuple was returned */
	if (PyTuple_Check(pres) == false) {
		DEBUG(0, ("[ERR][%s][%s]: Tuple expected to be returned in create_context\n",
			  module_name, __location__));
		Py_DECREF(pres);
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve return value (item 0 of the tuple) */
	res = PyTuple_GetItem(pres, 0);
	if (res == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed\n",
			  module_name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	retval = PyLong_AsLong(res);
	if (retval != MAPISTORE_SUCCESS) {
		if (retval == -1) {
			DEBUG(0, ("[ERR][%s][%s]: Overflow error\n",
				  module_name, __location__));
		}
		Py_DECREF(res);
		Py_DECREF(pres);
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}
	Py_DECREF(res);

	/* Retrieve private object (item 1 of the tuple) */
	robj = PyTuple_GetItem(pres, 1);
	if (robj == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed\n",
			  module_name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	} else if (strcmp("ContextObject", robj->ob_type->tp_name)) {
		DEBUG(0, ("[ERR][%s][%s]: Expected ContextObject and got '%s'\n",
			  module_name, __location__, robj->ob_type->tp_name));
		Py_DECREF(robj);
		Py_DECREF(pres);
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}
	Py_INCREF(robj);

	pyobj = talloc_zero(mem_ctx, struct mapistore_python_object);
	pyobj->name = talloc_strdup(pyobj, module_name);
	pyobj->conn = conn;
	pyobj->ictx = ictx;
	pyobj->module = module;
	pyobj->private_object = robj;
	*context_obj = pyobj;

	Py_DECREF(pres);
	Py_DECREF(pinst);
	Py_DECREF(backend);

	return MAPISTORE_SUCCESS;
}


/**
   \details Open a root folder

   \param mem_ctx pointer to the memory context
   \param backend_object pointer to the mapistore_python_object backend object
   \param fid the folder identifier of the root folder to open
   \param folder_object pointer on pointer to the folder object to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_context_get_root_folder(TALLOC_CTX *mem_ctx,
								     void *backend_object,
								     uint64_t fid,
								     void **folder_object)
{
	enum mapistore_error		retval;
	struct mapistore_python_object	*pyobj;
	struct mapistore_python_object	*fobj = NULL;
	PyObject			*context;
	PyObject			*pres;
	PyObject			*res;
	PyObject			*folder;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!backend_object, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(!folder_object, MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Retrieve backend object */
	pyobj = (struct mapistore_python_object *) backend_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Retrieve the context object */
	context = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!pyobj->private_object, MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* FIXME: Retrieve the indexing URI */
	/* FIXME: Replace "OK" with string */

	/* Call get_root_folder function */
	pres = PyObject_CallMethod(context, "get_root_folder", "K", fid);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		Py_DECREF(context);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Ensure a tuple was returned */
	if (PyTuple_Check(pres) == false) {
		DEBUG(0, ("[ERR][%s][%s]: Tuple expected to be returned in get_root_folder\n",
			  pyobj->name, __location__));
		Py_DECREF(pres);
		Py_DECREF(context);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve return value (item 0 of the tuple) */
	res = PyTuple_GetItem(pres, 0);
	if (res == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed: ", pyobj->name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		Py_DECREF(context);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	retval = PyLong_AsLong(res);
	Py_DECREF(res);
	if (retval != MAPISTORE_SUCCESS) {
		if (retval == -1) {
			DEBUG(0, ("[ERR][%s][%s]: Overflow error\n", pyobj->name, __location__));
			retval = MAPISTORE_ERR_CONTEXT_FAILED;
		}
		Py_DECREF(pres);
		Py_DECREF(context);
		return retval;
	}

	/* Retrieve private folder object (item 1 of the tuple) */
	folder = PyTuple_GetItem(pres, 1);
	if (folder == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed: ", pyobj->name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	} else if (strcmp(folder->ob_type->tp_name, "FolderObject")) {
		DEBUG(0, ("[ERR][%s][%s]: Expected FolderObject but got '%s'\n", pyobj->name,
			  __location__, folder->ob_type->tp_name));
		Py_DECREF(folder);
		Py_DECREF(pres);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	Py_DECREF(pres);

	fobj = talloc_zero(mem_ctx, struct mapistore_python_object);
	MAPISTORE_RETVAL_IF(!fobj, MAPISTORE_ERR_NO_MEMORY, NULL);

	fobj->name = talloc_strdup(fobj, pyobj->name);
	fobj->conn = pyobj->conn;
	fobj->ictx = pyobj->ictx;
	fobj->module = pyobj->module;
	fobj->private_object = folder;
	*folder_object = fobj;

	return retval;
}

/**
   \details Load specified mapistore python backend

   \param module_name the name of the mapistore python backend to load

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_load_backend(const char *module_name)
{
	struct mapistore_backend	backend;
	PyObject			*module;
	PyObject			*mod_backend;
	PyObject			*pname;
	PyObject			*pdesc;
	PyObject			*pns;

	/* Initialize backend with default settings */
	mapistore_backend_init_defaults(&backend);

	/* Import the module */
	module = PyImport_ImportModule(module_name);
	if (module == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to load python module: ",
			  module_name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_BACKEND_REGISTER;
	}

	/* Retrieve the backend object */
	mod_backend = PyObject_GetAttrString(module, "BackendObject");
	if (mod_backend == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to retrieve BackendObject\n",
			  module_name, __location__));
		Py_DECREF(module);
		return MAPISTORE_ERR_BACKEND_REGISTER;
	}

	/* Retrieve backend name attribute */
	pname = PyObject_GetAttrString(mod_backend, "name");
	if (pname == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to retrieve attribute name\n",
			  module_name, __location__));
		Py_DECREF(mod_backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_BACKEND_REGISTER;
	}
	backend.backend.name = PyString_AsString(pname);
	if (backend.backend.name == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to retrieve attribute name\n",
			  module_name, __location__));
		PyErr_Print();
		Py_DECREF(pname);
		Py_DECREF(mod_backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_BACKEND_REGISTER;
	}
	Py_DECREF(pname);

	/* Retrieve backend description attribute */
	pdesc = PyObject_GetAttrString(mod_backend, "description");
	if (pdesc == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to retrieve attribute description\n",
			  module_name, __location__));
		Py_DECREF(mod_backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_BACKEND_REGISTER;
	}
	backend.backend.description = PyString_AsString(pdesc);
	if (backend.backend.description == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to retrieve attribute description\n",
			  module_name, __location__));
		Py_DECREF(pdesc);
		Py_DECREF(mod_backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_BACKEND_REGISTER;
	}
	Py_DECREF(pdesc);

	/* Retrieve backend namespace attribute */
	pns = PyObject_GetAttrString(mod_backend, "namespace");
	if (pns == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to retrieve attribute namespace\n",
			  module_name, __location__));
		Py_DECREF(mod_backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_BACKEND_REGISTER;
	}
	backend.backend.namespace = PyString_AsString(pns);
	if (backend.backend.namespace == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to retrieve attribute namespace\n",
			  module_name, __location__));
		Py_DECREF(pns);
		Py_DECREF(mod_backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_BACKEND_REGISTER;
	}
	Py_DECREF(pns);

	/* backend */
	backend.backend.init = mapistore_python_backend_init;
	backend.backend.list_contexts = mapistore_python_backend_list_contexts;
	backend.backend.create_context = mapistore_python_backend_create_context;
#if 0
	backend.backend.create_root_folder = mapistore_python_backend_create_root_folder;
	/* context */
	backend.context.get_path = mapistore_python_context_get_path;
#endif
	backend.context.get_root_folder = mapistore_python_context_get_root_folder;
#if 0
	/* folder */
	backend.folder.open_folder = mapistore_python_folder_open_folder;
	backend.folder.create_folder = mapistore_python_folder_create_folder;
	backend.folder.delete = mapistore_python_folder_delete;
	backend.folder.open_message = mapistore_python_folder_open_message;
	backend.folder.create_message = mapistore_python_folder_create_message;
	backend.folder.delete_message = mapistore_python_folder_delete_message;
	backend.folder.move_copy_messages = mapistore_python_folder_move_copy_messages;
	backend.folder.get_deleted_fmids = mapistore_python_folder_get_deleted_fmids;
	backend.folder.get_child_count = mapistore_python_folder_get_child_count;
	backend.folder.open_table = mapistore_python_folder_open_table;
	backend.folder.modify_permissions = mapistore_python_folder_modify_permissions;

	/* message */
	backend.message.get_message_data = mapistore_python_message_get_message_data;
	backend.message.modify_recipients = mapistore_python_message_modify_recipients;
	backend.message.set_read_flag = mapistore_python_message_set_read_flag;
	backend.message.save = mapistore_python_message_save;
	backend.message.submit = mapistore_python_message_submit;
	backend.message.open_attachment = mapistore_python_message_open_attachment;
	backend.message.create_attachment = mapistore_python_message_create_attachment;
	backend.message.open_embedded_message = mapistore_python_message_open_embedded_message;

	/* table */
	backend.table.get_available_properties = mapistore_python_table_get_available_properties;
	backend.table.set_columns = mapistore_python_table_set_columns;
	backend.table.set_restrictions = mapistore_python_table_set_restrictions;
	backend.table.set_sort_order = mapistore_python_table_set_sort_order;
	backend.table.get_row = mapistore_python_table_get_row;
	backend.table.get_row_count = mapistore_python_table_get_row_count;
	backend.table.handle_destructor = mapistore_python_table_handle_destructor;

	/* properties */
	backend.properties.get_available_properties = mapistore_python_properties_get_available_properties;
	backend.properties.get_properties = mapistore_python_properties_get_properties;
	backend.properties.set_properties = mapistore_python_properties_set_properties;

	/* management */
	backend.manager.generate_uri = mapistore_python_manager_generate_uri;
#endif
	mapistore_backend_register((const void *)&backend);

	Py_DECREF(mod_backend);
	Py_DECREF(module);

	return MAPISTORE_SUCCESS;
}

/**
   \details Lookup mapistore python backend directory and load all
   backends found.

   \param mem_ctx pointer to the memory context
   \param path pointer to the mapistore python backends to lookup

   \return MAPISTORE_SUCCESS on success, other MAPISTORE error
 */
static enum mapistore_error mapistore_python_load_backends(TALLOC_CTX *mem_ctx,
							   const char *path)
{
	enum mapistore_error	retval = MAPISTORE_SUCCESS;
	DIR			*dir;
	struct dirent		*entry;
	char			*filename;

	dir = opendir(path);
	if (dir == NULL) {
		DEBUG(0, ("[ERR][%s]: Unable to open mapistore python backend directory: ",
			  __location__));
		err(1, "%s\n", path);
		return MAPISTORE_ERR_BACKEND_INIT;
	}

	while ((entry = readdir(dir))) {
		if (ISDOT(entry->d_name) || ISDOTDOT(entry->d_name)) {
			continue;
		}

		if ((strlen(entry->d_name) > 3) &&
		    !strcmp(&entry->d_name[strlen(entry->d_name) - 3], ".py")) {
			filename = talloc_strndup(mem_ctx, entry->d_name, strlen(entry->d_name) - 3);
			DEBUG(0, ("[INFO][%s]: Found a python backend: %s\n", __location__, filename));
			retval = mapistore_python_load_backend(filename);
			talloc_free(filename);
			if (retval != MAPISTORE_SUCCESS) {
				goto end;
			}
		}
	}

end:
	closedir(dir);
	return retval;
}


/**
   \details Load and initialize mapistore python backends located in
   default or specified path.

   \param mem_ctx pointer to the memory context
   \param path custom path where mapistore python backends are located
   if different from default.

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_python_load_and_run(TALLOC_CTX *mem_ctx,
							    const char *path)
{
	enum mapistore_error	retval;

	if (!path) {
		path = mapistore_python_get_installdir();
	}

	Py_Initialize();
	DEBUG(0, ("[INFO][%s]: Loading from '%s'\n", __location__, path));

	retval = mapistore_set_pypath((char *)path);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	return mapistore_python_load_backends(mem_ctx, path);
}
