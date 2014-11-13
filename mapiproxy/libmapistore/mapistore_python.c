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
#include <datetime.h>

#include <sys/types.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <err.h>

#include "mapistore.h"
#include "mapistore_errors.h"
#include "mapistore_private.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include <dlinklist.h>

/**
   \file mapistore_python.c

   \brief Implement a Python-C gateway providing backend implementors
   the ability to write mapistore backends using .py files
 */


static int mapistore_python_object_destructor(void *data)
{
	struct mapistore_python_object	*pyobj = (struct mapistore_python_object *) data;

	if (pyobj->private_object) {
		PyObject	*obj;

		obj = (PyObject *) pyobj->private_object;
		DEBUG(5, ("[INFO] mapistore_python_object_destructor: %s\n", obj->ob_type->tp_name));
		Py_DECREF(obj);
	}

	return 0;
}


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
   \details Convert Pyobject into C type and cast it to void

   \param mem_ctx pointer to the memory context
   \param proptag the property tag to lookup
   \param value the PyObject value to map to C type
   \param data pointer on pointer to the mapped value to return

   \return MAPISTORE_SUCCESS on success, otherwise
   MAPISTORE_ERR_NOT_FOUND
 */
static enum mapistore_error mapistore_data_from_pyobject(TALLOC_CTX *mem_ctx,
							 uint32_t proptag,
							 PyObject *value,
							 void **data)
{
	enum mapistore_error	retval = MAPISTORE_ERR_NOT_FOUND;
	PyObject		*item;
	uint32_t		count;
	bool			b;
	int			l;
	uint64_t		ll;
	double			d;
	struct Binary_r		*bin;
	struct StringArray_r	*MVszA;
	struct StringArrayW_r	*MVszW;
	struct LongArray_r	*MVl;
	NTTIME			nt;
	struct FILETIME		*ft;
	char			*str;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!proptag, MAPISTORE_ERR_NOT_FOUND, NULL);
	MAPISTORE_RETVAL_IF(!value, MAPISTORE_ERR_NOT_FOUND, NULL);
	MAPISTORE_RETVAL_IF(!data, MAPISTORE_ERR_NOT_FOUND, NULL);

	switch (proptag & 0xFFFF) {
		case PT_I2:
			DEBUG(5, ("[WARN][%s]: PT_I2 case not implemented\n", __location__));
			break;
		case PT_LONG:
			l = PyLong_AsLong(value);
			MAPISTORE_RETVAL_IF(l == -1, MAPISTORE_ERR_NOT_FOUND, NULL);
			*data = talloc_memdup(mem_ctx, &l, sizeof(l));
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_DOUBLE:
			MAPISTORE_RETVAL_IF(!PyFloat_Check(value), MAPISTORE_ERR_NOT_FOUND, NULL);
			d = PyFloat_AsDouble(value);
			*data = talloc_memdup(mem_ctx, &d, sizeof(d));
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_BOOLEAN:
			MAPISTORE_RETVAL_IF((PyBool_Check(value) == false), MAPISTORE_ERR_NOT_FOUND, NULL);
			b = (value == Py_True);
			*data = talloc_memdup(mem_ctx, &b, sizeof(b));
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_I8:
			if (PyInt_Check(value)) {
				ll = (unsigned long long)PyInt_AsLong(value);
			} else if (PyLong_Check(value)) {
				ll = PyLong_AsUnsignedLongLong(value);
			} else {
				PyErr_SetString(PyExc_TypeError, "PT_I8: a ulonglong is required");
				ll = -1;
			}
			MAPISTORE_RETVAL_IF(ll == -1, MAPISTORE_ERR_NOT_FOUND, NULL);
			*data = talloc_memdup(mem_ctx, &ll, sizeof(ll));
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_STRING8:
		case PT_UNICODE:
			MAPISTORE_RETVAL_IF(!PyString_Check(value) && !PyUnicode_Check(value),
					    MAPISTORE_ERR_NOT_FOUND, NULL);
			str = PyString_AsString(value);
			MAPISTORE_RETVAL_IF(!str, MAPISTORE_ERR_NOT_FOUND, NULL);

			*data = (void *)talloc_strdup(mem_ctx, str);
			MAPISTORE_RETVAL_IF(!*data, MAPISTORE_ERR_NOT_FOUND, NULL);
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_SYSTIME:
			MAPISTORE_RETVAL_IF(!PyFloat_Check(value), MAPISTORE_ERR_NOT_FOUND, NULL);
			unix_to_nt_time(&nt, PyFloat_AsDouble(value));
			ft = talloc_zero(mem_ctx, struct FILETIME);
			MAPISTORE_RETVAL_IF(!ft, MAPISTORE_ERR_NOT_FOUND, NULL);
			ft->dwLowDateTime = (nt << 32) >> 32;
			ft->dwHighDateTime = nt >> 32;
			*data = (void *) ft;
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_CLSID:
			DEBUG(5, ("[WARN][%s]: PT_CLSID case not implemented\n", __location__));
			break;
		case PT_SVREID:
		case PT_BINARY:
			MAPISTORE_RETVAL_IF(!PyByteArray_Check(value), MAPISTORE_ERR_NOT_FOUND, NULL);
			bin = talloc_zero(mem_ctx, struct Binary_r);
			MAPISTORE_RETVAL_IF(!bin, MAPISTORE_ERR_NOT_FOUND, NULL);
			bin->cb = PyByteArray_Size(value);
			bin->lpb = talloc_memdup(bin, PyByteArray_AsString(value), bin->cb);
			MAPISTORE_RETVAL_IF(!bin->lpb, MAPISTORE_ERR_NOT_FOUND, bin);
			*data = (void *) bin;
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_MV_SHORT:
			DEBUG(5, ("[WARN][%s]: PT_MV_I2 case not implemented\n", __location__));
			break;
		case PT_MV_LONG:
			MAPISTORE_RETVAL_IF(!PyList_Check(value), MAPISTORE_ERR_NOT_FOUND, NULL);
			MVl = talloc_zero(mem_ctx, struct LongArray_r);
			MAPISTORE_RETVAL_IF(!MVl, MAPISTORE_ERR_NOT_FOUND, NULL);
			MVl->cValues = PyList_Size(value);
			MVl->lpl = (uint32_t *) talloc_array(MVl, uint32_t, MVl->cValues + 1);
			for (count = 0; count < MVl->cValues; count++) {
				item = PyList_GetItem(value, count);
				MAPISTORE_RETVAL_IF(!item, MAPISTORE_ERR_INVALID_PARAMETER, MVl);
				l = PyLong_AsLong(item);
				MVl->lpl[count] = l;
			}
			*data = (void *) MVl;
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_MV_I8:
			DEBUG(5, ("[WARN][%s]: PT_MV_I8 case not implemented\n", __location__));
			break;
		case PT_MV_STRING8:
			MAPISTORE_RETVAL_IF(!PyList_Check(value), MAPISTORE_ERR_NOT_FOUND, NULL);
			MVszA = talloc_zero(mem_ctx, struct StringArray_r);
			MAPISTORE_RETVAL_IF(!MVszA, MAPISTORE_ERR_NOT_FOUND, NULL);
			MVszA->cValues = PyList_Size(value);
			MVszA->lppszA = (const char **) talloc_array(MVszA, char *, MVszA->cValues + 1);
			for (count = 0; count < MVszA->cValues; count++) {
				item = PyList_GetItem(value, count);
				MAPISTORE_RETVAL_IF(!item, MAPISTORE_ERR_INVALID_PARAMETER, MVszA);
				str = PyString_AsString(item);
				MAPISTORE_RETVAL_IF(!str, MAPISTORE_ERR_INVALID_PARAMETER, MVszA);
				MVszA->lppszA[count] = talloc_strdup(MVszA->lppszA, str);
			}
			*data = (void *) MVszA;
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_MV_UNICODE:
			MAPISTORE_RETVAL_IF(!PyList_Check(value), MAPISTORE_ERR_NOT_FOUND, NULL);
			MVszW = talloc_zero(mem_ctx, struct StringArrayW_r);
			MAPISTORE_RETVAL_IF(!MVszW, MAPISTORE_ERR_NOT_FOUND, NULL);
			MVszW->cValues = PyList_Size(value);
			MVszW->lppszW = (const char **) talloc_array(MVszW, char *, MVszW->cValues + 1);
			for (count = 0; count < MVszW->cValues; count++) {
				item = PyList_GetItem(value, count);
				MAPISTORE_RETVAL_IF(!item, MAPISTORE_ERR_INVALID_PARAMETER, MVszW);
				str = PyString_AsString(item);
				MAPISTORE_RETVAL_IF(!str, MAPISTORE_ERR_INVALID_PARAMETER, MVszW);
				MVszW->lppszW[count] = talloc_strdup(MVszW->lppszW, str);
			}
			*data = (void *) MVszW;
			retval = MAPISTORE_SUCCESS;
			break;
		case PT_MV_SYSTIME:
			DEBUG(5, ("[WARN][%s]: PT_MV_SYSTIME case not implemented\n", __location__));
			break;
		case PT_MV_CLSID:
			DEBUG(5, ("[WARN][%s]: PT_MV_CLSID case not implemented\n", __location__));
			break;
		case PT_MV_BINARY:
			DEBUG(5, ("[WARN][%s]: PT_MV_BINARY case not implemented\n", __location__));
			break;
		case PT_NULL:
			DEBUG(5, ("[WARN][%s]: PT_NULL case not implemented\n", __location__));
			break;
		case PT_OBJECT:
			DEBUG(5, ("[WARN][%s]: PT_OBJECT case not implemented\n", __location__));
			break;
		default:
			DEBUG(5, ("[WARN][%s]: 0x%x case not implemented\n", __location__,
				  (proptag & 0xFFFF)));
			break;
		}

	return retval;
}

static PyObject *mapistore_pyobject_from_data(struct SPropValue *lpProps)
{
	uint32_t		i;
	const void		*data;
	PyObject		*val;
	PyObject		*item;
	struct Binary_r		*bin;
	struct StringArrayW_r	*MVszW;
	struct StringArray_r	*MVszA;
	struct LongArray_r	*MVl;
	NTTIME			nt;
	struct FILETIME		*ft;
	struct timeval		t;

	/* Sanity checks */
	if (!lpProps) return NULL;


	/* Retrieve SPropValue data */
	data = get_SPropValue_data(lpProps);
	if (data == NULL) {
		return NULL;
	}

	val = NULL;
	switch (lpProps->ulPropTag & 0xFFFF) {
	case PT_I2:
		DEBUG(5, ("[WARN][%s]: PT_I2 case not implemented\n", __location__));
		break;
	case PT_LONG:
		val = PyLong_FromLong(*((uint32_t *)data));
		break;
	case PT_DOUBLE:
		val = PyFloat_FromDouble(*((double *)data));
		break;
	case PT_BOOLEAN:
		val = PyBool_FromLong(*((uint32_t *)data));
		break;
	case PT_I8:
		val = PyLong_FromUnsignedLongLong(*((uint64_t *)data));
		break;
	case PT_STRING8:
	case PT_UNICODE:
		val = PyString_FromString((const char *)data);
		break;
	case PT_SYSTIME:
		ft = (struct FILETIME *) data;
		nt = ft->dwHighDateTime;
		nt = nt << 32;
		nt |= ft->dwLowDateTime;
		nttime_to_timeval(&t, nt);
		val = PyFloat_FromString(PyString_FromFormat("%ld.%ld", t.tv_sec, t.tv_usec), NULL);
		break;
	case PT_CLSID:
		DEBUG(5, ("[WARN][%s]: PT_CLSID case not implemented\n", __location__));
		break;
	case PT_SVREID:
	case PT_BINARY:
		bin = (struct Binary_r *)data;
		val = PyByteArray_FromStringAndSize((const char *)bin->lpb, bin->cb);
		break;
	case PT_MV_SHORT:
		DEBUG(5, ("[WARN][%s]: PT_MV_I2 case not implemented\n", __location__));
		break;
	case PT_MV_LONG:
		MVl = (struct LongArray_r *)data;
		val = PyList_New(MVl->cValues);
		if (val == NULL) {
			DEBUG(0, ("[ERR][%s]: Unable to initialize Python List\n", __location__));
			return NULL;
		}
		for (i = 0; i < MVl->cValues; i++) {
			item = PyLong_FromLong(MVl->lpl[i]);
			if (PyList_SetItem(val, i, item) == -1) {
				DEBUG(0, ("[ERR][%s]: Unable to append entry to Python list\n", __location__));
				return NULL;
			}
		}
		break;
	case PT_MV_I8:
		DEBUG(5, ("[WARN][%s]: PT_MV_I8 case not implemented\n", __location__));
		break;
	case PT_MV_STRING8:
		MVszA = (struct StringArray_r *)data;
		val = PyList_New(MVszA->cValues);
		if (val == NULL) {
			DEBUG(0, ("[ERR][%s]: Unable to initialize Python List\n", __location__));
			return NULL;
		}
		for (i = 0; i < MVszA->cValues; i++) {
			item = PyString_FromString(MVszA->lppszA[i]);
			if (PyList_SetItem(val, i, item) == -1) {
				DEBUG(0, ("[ERR][%s]: Unable to append entry to Python list\n", __location__));
				return NULL;
			}
		}
		break;
	case PT_MV_UNICODE:
		MVszW = (struct StringArrayW_r *)data;
		val = PyList_New(MVszW->cValues);
		if (val == NULL) {
			DEBUG(0, ("[ERR][%s]: Unable to initialize Python List\n", __location__));
			return NULL;
		}

		for (i = 0; i < MVszW->cValues; i++) {
			item = PyString_FromString(MVszW->lppszW[i]);
			if (PyList_SetItem(val, i, item) == -1) {
				DEBUG(0, ("[ERR][%s]: Unable to append entry to Python list\n", __location__));
				return NULL;
			}
		}
		break;
	case PT_MV_SYSTIME:
		DEBUG(5, ("[WARN][%s]: PT_MV_SYSTIME case not implemented\n", __location__));
		break;
	case PT_MV_CLSID:
		DEBUG(5, ("[WARN][%s]: PT_MV_CLSID case not implemented\n", __location__));
		break;
	case PT_MV_BINARY:
		DEBUG(5, ("[WARN][%s]: PT_MV_BINARY case not implemented\n", __location__));
		break;
	case PT_NULL:
		DEBUG(5, ("[WARN][%s]: PT_NULL case not implemented\n", __location__));
		break;
	case PT_OBJECT:
		DEBUG(5, ("[WARN][%s]: PT_OBJECT case not implemented\n", __location__));
		break;
	default:
		DEBUG(5, ("[WARN][%s]: 0x%x case not implemented\n", __location__, (lpProps->ulPropTag & 0xFFFF)));
		break;
	}

	return val;
}


static PyObject *mapistore_python_pyobject_from_proptag(uint32_t proptag)
{
	const char	*sproptag = NULL;
	PyObject	*item = NULL;

	if (((proptag >> 16) & 0xFFFF) > 0x8000) {
		sproptag = openchangedb_named_properties_get_attribute(proptag);
	} else {
		sproptag = openchangedb_property_get_attribute(proptag);
	}
	if (sproptag == NULL) {
		item = PyString_FromFormat("0x%x", proptag);
	} else {
		item = PyString_FromString(sproptag);
	}

	return item;
}


static PyObject	*mapistore_python_dict_from_SRow(struct SRow *aRow)
{
	uint32_t		count;
	PyObject		*pydict;
	PyObject		*key;
	PyObject		*val;

	/* Sanity checks */
	if (!aRow) return NULL;

	/* Initialize Dictionary */
	pydict = PyDict_New();
	if (pydict == NULL) {
		DEBUG(0, ("[ERR][%s]: Unable to initialize Python Dictionary\n", __location__));
		return NULL;
	}

	for (count = 0; count < aRow->cValues; count++) {

		/* Set the key of the dictionary entry */
		key = mapistore_python_pyobject_from_proptag(aRow->lpProps[count].ulPropTag);

		/* Retrieve SPropValue data */
		val = mapistore_pyobject_from_data(&(aRow->lpProps[count]));
		if (val) {
			if (PyDict_SetItem(pydict, key, val) == -1) {
				DEBUG(0, ("[ERR][%s]: Unable to add entry to Python dictionary\n",
					  __location__));
				return NULL;
			}
		}
	}

	return pydict;
}


/**
   \details Initialize python backend

   \param module_name the name of the module

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_backend_init(const char *module_name)
{
	enum mapistore_error	retval = MAPISTORE_SUCCESS;
	long			l;
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

	l = PyLong_AsLong(pres);
	if (l == -1) {
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
	struct mapistore_contexts_list	*clist = NULL;
	struct mapistore_contexts_list	*entry;
	PyObject			*module;
	PyObject			*backend;
	PyObject			*pylist;
	PyObject			*dict;
	PyObject			*pinst;
	PyObject			*key;
	PyObject			*item;
	uint32_t			i;
	uint32_t			count;

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
	pylist = PyObject_CallMethod(pinst, "list_contexts", "s", username);
	if (pylist == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: list_contexts failed\n", module_name, __location__));
		PyErr_Print();
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Ensure a list was returned */
	if (PyList_Check(pylist) == false) {
		DEBUG(0, ("[ERR][%s][%s]: Tuple expected to be returned in list_contexts\n",
			  module_name, __location__));
		Py_DECREF(pylist);
		Py_DECREF(pinst);
		Py_DECREF(backend);
		Py_DECREF(module);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	count = PyList_Size(pylist);
	for (i = 0; i < count; i++) {
		dict = PyList_GetItem(pylist, i);
		if (dict == NULL) {
			DEBUG(0, ("[ERR][%s][%s]: PyList_GetItem failed ",
				  module_name, __location__));
			PyErr_Print();
			Py_DECREF(pylist);
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}

		if (PyDict_Check(dict) != true) {
		DEBUG(0, ("[ERR][%s][%s]: dict expected to be returned but got '%s'\n",
			  module_name, __location__, dict->ob_type->tp_name));
		Py_DECREF(pylist);
		return MAPISTORE_ERR_INVALID_PARAMETER;
		}

		entry = talloc_zero(mem_ctx, struct mapistore_contexts_list);
		MAPISTORE_RETVAL_IF(!entry, MAPISTORE_ERR_NO_MEMORY, clist);

		/* Retrieve url */
		key = PyString_FromString("url");
		if (key == NULL) {
			PyErr_Print();
			Py_DECREF(pylist);
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}
		if (PyDict_Contains(dict, key) == -1) {
			DEBUG(0, ("[ERR][%s][%s]: Missing url key for entry %d\n",
				  module_name, __location__, i));
			Py_DECREF(pylist);
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}
		item = PyDict_GetItem(dict, key);
		Py_DECREF(key);
		if (PyString_Check(item) != true) {
			DEBUG(0, ("[ERR][%s][%s]: String expect but got '%s'\n",
				  module_name, __location__, item->ob_type->tp_name));
			Py_DECREF(pylist);
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}
		entry->url = PyString_AsString(item);

		/* name */
		key = PyString_FromString("name");
		if (key == NULL) {
			PyErr_Print();
			Py_DECREF(pylist);
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}
		if (PyDict_Contains(dict, key) == -1) {
			DEBUG(0, ("[ERR][%s][%s]: Missing name key for entry %d\n",
				  module_name, __location__, i));
			Py_DECREF(pylist);
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}
		item = PyDict_GetItem(dict, key);
		Py_DECREF(key);
		if (PyString_Check(item) != true) {
			DEBUG(0, ("[ERR][%s][%s]: String expect but got '%s'\n",
				  module_name, __location__, item->ob_type->tp_name));
			Py_DECREF(pylist);
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}
		entry->name = PyString_AsString(item);

		/* main_folder */
		key = PyString_FromString("main_folder");
		if (key == NULL) {
			PyErr_Print();
			Py_DECREF(pylist);
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}
		if (PyDict_Contains(dict, key) == -1) {
			DEBUG(0, ("[ERR][%s][%s]: Missing main_folder key for entry %d\n",
				  module_name, __location__, i));
			Py_DECREF(pylist);
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}
		item = PyDict_GetItem(dict, key);
		Py_DECREF(key);
		/* FIXME: Check bool value */
		entry->main_folder = (bool)PyInt_AsLong(item);

		/* role */
		key = PyString_FromString("role");
		if (key == NULL) {
			PyErr_Print();
			Py_DECREF(pylist);
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}
		if (PyDict_Contains(dict, key) == -1) {
			DEBUG(0, ("[ERR][%s][%s]: Missing role key for entry %d\n",
				  module_name, __location__, i));
			Py_DECREF(pylist);
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}
		item = PyDict_GetItem(dict, key);
		Py_DECREF(key);
		/* FIXME: Check int type */
		entry->role = PyInt_AsLong(item);

		/* tag */
		key = PyString_FromString("tag");
		if (key == NULL) {
			PyErr_Print();
			Py_DECREF(pylist);
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}
		if (PyDict_Contains(dict, key) == -1) {
			DEBUG(0, ("[ERR][%s][%s]: Missing tag key for entry %d\n",
				  module_name, __location__, i));
			Py_DECREF(pylist);
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}
		item = PyDict_GetItem(dict, key);
		Py_DECREF(key);
		if (PyString_Check(item) != true) {
			DEBUG(0, ("[ERR][%s][%s]: String expect but got '%s'\n",
				  module_name, __location__, item->ob_type->tp_name));
			Py_DECREF(pylist);
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}
		entry->tag = PyString_AsString(item);

		DLIST_ADD_END(clist, entry, void);
	}
	*mclist = clist;

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
	struct mapistore_python_object	*pyobj;
	PyObject			*module;
	PyObject			*backend, *pres, *pinst;
	PyObject			*res, *robj;
	long				l;

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
	pres = PyObject_CallMethod(pinst, "create_context", "ss", uri, conn->username);
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

	l = PyLong_AsLong(res);
	if (l == -1) {
	  DEBUG(0, ("[ERR][%s][%s]: Overflow error\n",
		    module_name, __location__));
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

	pyobj = talloc_zero(mem_ctx, struct mapistore_python_object);
	MAPISTORE_RETVAL_IF(!pyobj, MAPISTORE_ERR_NO_MEMORY, NULL);

	pyobj->obj_type = MAPISTORE_PYTHON_OBJECT_CONTEXT;
	pyobj->name = talloc_strdup(pyobj, module_name);
	MAPISTORE_RETVAL_IF(!pyobj->name, MAPISTORE_ERR_NO_MEMORY, NULL);
	pyobj->conn = conn;
	pyobj->ictx = ictx;
	pyobj->module = module;
	pyobj->private_object = robj;
	*context_obj = pyobj;

	talloc_set_destructor((void *)pyobj, (int (*)(void *))mapistore_python_object_destructor);

	Py_INCREF(robj);

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
	enum mapistore_error		retval = MAPISTORE_SUCCESS;
	struct mapistore_python_object	*pyobj;
	struct mapistore_python_object	*fobj = NULL;
	PyObject			*context;
	PyObject			*pres;
	PyObject			*res;
	PyObject			*folder;
	long				l;

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
	MAPISTORE_RETVAL_IF(strcmp("ContextObject", context->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* FIXME: Retrieve the indexing URI */

	/* Call get_root_folder function */
	pres = PyObject_CallMethod(context, "get_root_folder", "K", fid);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Ensure a tuple was returned */
	if (PyTuple_Check(pres) == false) {
		DEBUG(0, ("[ERR][%s][%s]: Tuple expected to be returned in get_root_folder\n",
			  pyobj->name, __location__));
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve return value (item 0 of the tuple) */
	res = PyTuple_GetItem(pres, 0);
	if (res == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed: ", pyobj->name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	l = PyLong_AsLong(res);
	Py_DECREF(res);
	if (l == -1) {
		DEBUG(0, ("[ERR][%s][%s]: Overflow error\n", pyobj->name, __location__));
		retval = MAPISTORE_ERR_CONTEXT_FAILED;
		Py_DECREF(pres);
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
		Py_DECREF(pres);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	fobj = talloc_zero(mem_ctx, struct mapistore_python_object);
	MAPISTORE_RETVAL_IF(!fobj, MAPISTORE_ERR_NO_MEMORY, NULL);

	fobj->obj_type = MAPISTORE_PYTHON_OBJECT_FOLDER;
	fobj->name = talloc_strdup(fobj, pyobj->name);
	MAPISTORE_RETVAL_IF(!fobj->name, MAPISTORE_ERR_NO_MEMORY, fobj);
	fobj->conn = pyobj->conn;
	fobj->ictx = pyobj->ictx;
	fobj->module = pyobj->module;
	fobj->private_object = folder;
	*folder_object = fobj;

	Py_INCREF(folder);
	Py_DECREF(pres);

	return retval;
}

/**
   \details Retrieve the MAPIStore URI associated to given temporary
   Folder ID/Message ID

   \param mem_ctx pointer to the memory context
   \param context_object pointer to the mapistore_python_object
   backend object
   \param fmid the folderID/messageID associated to the mapistore URI
   to return
   \param mapistore_uri pointer on pointer to the MAPIStore URI to
   return

   \note It is the responsibility of the caller to free the string
   returned in mapistore_uri upon success

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_context_get_path(TALLOC_CTX *mem_ctx,
							      void *backend_object,
							      uint64_t fmid,
							      char **mapistore_uri)
{
	struct mapistore_python_object	*pyobj;
	PyObject			*context;
	PyObject			*pres;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!backend_object, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(!mapistore_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the backend object */
	pyobj = (struct mapistore_python_object *) backend_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_CONTEXT),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Retrieve the context object */
	context = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!context, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("ContextObject", context->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Call get_path function */
	pres = PyObject_CallMethod(context, "get_path", "K", fmid);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_NOT_FOUND;
	}

	if (PyString_Check(pres) == false) {
		DEBUG(0, ("[WARN][%s][%s]: Expected string but got '%s'\n", pyobj->name,
			  __location__, pres->ob_type->tp_name));
		Py_DECREF(pres);
		return MAPISTORE_ERR_NOT_FOUND;
	}

	*mapistore_uri = talloc_strdup(mem_ctx, PyString_AsString(pres));
	Py_DECREF(pres);
	MAPISTORE_RETVAL_IF(!*mapistore_uri, MAPISTORE_ERR_NO_MEMORY, NULL);

	return MAPISTORE_SUCCESS;
}

/**
   \details Open a folder

   \param mem_ctx pointer to the memory context
   \param folder_object the mapistore python parent object
   \param fid the folder identifier to lookup
   \param child_object pointer on pointer to the folder object to
   return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static enum mapistore_error mapistore_python_folder_open_folder(TALLOC_CTX *mem_ctx,
								void *folder_object,
								uint64_t fid,
								void **child_object)
{
	struct mapistore_python_object	*pyobj;
	struct mapistore_python_object	*pyfold;
	PyObject			*folder;
	PyObject			*pres;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!folder_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!child_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the folder object */
	pyobj = (struct mapistore_python_object *) folder_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_FOLDER),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	folder = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("FolderObject", folder->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Call open_folder function */
	pres = PyObject_CallMethod(folder, "open_folder", "K", fid);
	MAPISTORE_RETVAL_IF(!pres, MAPISTORE_ERR_NOT_FOUND, NULL);

	if (pres == Py_None) {
		Py_DECREF(pres);
		return MAPISTORE_ERR_NOT_FOUND;
	}

	if (strcmp("FolderObject", pres->ob_type->tp_name)) {
		DEBUG(0, ("[ERR][%s][%s]: Expected FolderObject but got '%s'\n", pyobj->name,
			  __location__, pres->ob_type->tp_name));
		Py_DECREF(pres);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	pyfold = talloc_zero(pyobj, struct mapistore_python_object);
	MAPISTORE_RETVAL_IF(!pyfold, MAPISTORE_ERR_NO_MEMORY, NULL);

	pyfold->obj_type = MAPISTORE_PYTHON_OBJECT_FOLDER;
	pyfold->name = talloc_strdup(pyfold, pyobj->name);
	MAPISTORE_RETVAL_IF(!pyfold->name, MAPISTORE_ERR_NO_MEMORY, NULL);
	pyfold->conn = pyobj->conn;
	pyfold->ictx = pyobj->ictx;
	pyfold->module = pyobj->module;
	pyfold->private_object = pres;
	*child_object = pyfold;

	talloc_set_destructor((void *)pyfold, (int (*)(void *))mapistore_python_object_destructor);

	Py_INCREF(pres);

	return MAPISTORE_SUCCESS;
}


/**
   \details Create a folder

   \param mem_ctx pointer to the memory context
   \param folder_object the mapistore python parent object
   \param fid the folder identifier to associate to the folder to
   create
   \param aRow pointer to a SRow structure holding folder properties
   to associate to the folder to create
   \param new_folder pointer on pointer to the new folder object to
   return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static enum mapistore_error mapistore_python_folder_create_folder(TALLOC_CTX *mem_ctx,
								  void *folder_object,
								  uint64_t fid,
								  struct SRow *aRow,
								  void **new_folder)
{
	struct mapistore_python_object	*pyobj;
	struct mapistore_python_object	*pynobj;
	PyObject			*folder;
	PyObject			*pydict;
	PyObject			*pynew;
	PyObject			*pres;
	PyObject			*res;
	long				l;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!folder_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!new_folder, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!aRow, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the folder object */
	pyobj = (struct mapistore_python_object *) folder_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_FOLDER),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	folder = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("FolderObject", folder->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Build PyDict from aRow */
	pydict = mapistore_python_dict_from_SRow(aRow);
	MAPISTORE_RETVAL_IF(!pydict, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Call create_folder function */
	pres = PyObject_CallMethod(folder, "create_folder", "OK", pydict, fid);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		Py_DECREF(pydict);
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}
	Py_DECREF(pydict);

	/* Ensure a tuple was returned */
	if (PyTuple_Check(pres) == false) {
		DEBUG(0, ("[ERR][%s][%s]: Tuple expected to be returned in create_folder\n",
			  pyobj->name, __location__));
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve return value (item 0 of the tuple) */
	res = PyTuple_GetItem(pres, 0);
	if (res == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed ",
			  pyobj->name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	l = PyLong_AsLong(res);
	if (l == -1) {
		DEBUG(0, ("[ERR][%s][%s]: Overflow error\n", pyobj->name, __location__));
		Py_DECREF(pres);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}
	Py_DECREF(res);

	pynew = PyTuple_GetItem(pres, 1);
	if (pynew == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed\n", pyobj->name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	} else if (strcmp("FolderObject", pynew->ob_type->tp_name)) {
		DEBUG(0, ("[ERR][%s][%s]: Expected FolderObject and got '%s'\n",
			  pyobj->name, __location__, pynew->ob_type->tp_name));
		Py_DECREF(pres);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	pynobj = talloc_zero(pyobj, struct mapistore_python_object);
	MAPISTORE_RETVAL_IF(!pynobj, MAPISTORE_ERR_NO_MEMORY, NULL);

	pynobj->obj_type = MAPISTORE_PYTHON_OBJECT_FOLDER;
	pynobj->name = talloc_strdup(pynobj, pyobj->name);
	MAPISTORE_RETVAL_IF(!pynobj->name, MAPISTORE_ERR_NO_MEMORY, NULL);
	pynobj->conn = pyobj->conn;
	pynobj->ictx = pyobj->ictx;
	pynobj->module = pyobj->module;
	pynobj->private_object = pynew;
	*new_folder = pynobj;

	talloc_set_destructor((void *)pynobj, (int (*)(void *))mapistore_python_object_destructor);

	Py_INCREF(pynew);

	return MAPISTORE_SUCCESS;
}


/**
   \details Delete a folder

   \param folder_object pointer to the mapistore python object to
   delete

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static enum mapistore_error mapistore_python_folder_delete(void *folder_object)
{
	enum mapistore_error		retval;
	struct mapistore_python_object	*pyobj;
	PyObject			*folder;
	PyObject			*pres;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!folder_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the folder object */
	pyobj = (struct mapistore_python_object *) folder_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_FOLDER),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	folder = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("FolderObject", folder->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	pres = PyObject_CallMethod(folder, "delete", NULL);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	retval = PyLong_AsLong(pres);
	if (retval != MAPISTORE_SUCCESS) {
		Py_DECREF(pres);
		return retval;
	}

	talloc_free(pyobj);

	return MAPISTORE_SUCCESS;
}


/**
   \details Open a message

   \param mem_ctx pointer to the memory context
   \param folder_object pointer to the parent folder object
   \param mid the message identifier
   \param read_write boolean value to control opening capabilities
   \param message_object pointer on pointer to the message object to
   return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_folder_open_message(TALLOC_CTX *mem_ctx,
								 void *folder_object,
								 uint64_t mid,
								 bool read_write,
								 void **message_object)
{
	struct mapistore_python_object	*pyobj;
	struct mapistore_python_object	*pymsg;
	PyObject			*folder;
	PyObject			*msg;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!folder_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!message_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the folder object */
	pyobj = (struct mapistore_python_object *) folder_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_FOLDER),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	folder = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("FolderObject", folder->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Call open_message function */
	msg = PyObject_CallMethod(folder, "open_message", "Kb", mid, read_write);
	if (!msg) {
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	if (msg == Py_None) {
		Py_DECREF(msg);
		return MAPISTORE_ERR_NOT_FOUND;
	}

	if (strcmp("MessageObject", msg->ob_type->tp_name)) {
		DEBUG(0, ("[ERR][%s][%s]: Expected MessageObject but got '%s'\n", pyobj->name,
			  __location__, msg->ob_type->tp_name));
		Py_DECREF(msg);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	pymsg = talloc_zero(pyobj, struct mapistore_python_object);
	MAPISTORE_RETVAL_IF(!pymsg, MAPISTORE_ERR_NO_MEMORY, NULL);

	pymsg->obj_type = MAPISTORE_PYTHON_OBJECT_MESSAGE;
	pymsg->name = talloc_strdup(pymsg, pyobj->name);
	MAPISTORE_RETVAL_IF(!pymsg->name, MAPISTORE_ERR_NO_MEMORY, NULL);
	pymsg->conn = pyobj->conn;
	pymsg->ictx = pyobj->ictx;
	pymsg->module = pyobj->module;
	pymsg->private_object = msg;
	*message_object = pymsg;

	talloc_set_destructor((void *)pymsg, (int (*)(void *))mapistore_python_object_destructor);

	Py_INCREF(msg);
	return MAPISTORE_SUCCESS;
}


/**
   \details Create a message

   \param mem_ctx pointer to the memory context
   \param folder_object pointer to the folder object
   \param mid the message identifier
   \param associated specify is it is a FAI message or not
   \param message_object pointer on pointer to the message object to
   return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_folder_create_message(TALLOC_CTX *mem_ctx,
								   void *folder_object,
								   uint64_t mid,
								   uint8_t associated,
								   void **message_object)
{
	struct mapistore_python_object	*pyobj;
	struct mapistore_python_object	*pymsg;
	PyObject			*folder;
	PyObject			*msg;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!folder_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!message_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the folder object */
	pyobj = (struct mapistore_python_object *) folder_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_FOLDER),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	folder = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("FolderObject", folder->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Call the create_message function */
	msg = PyObject_CallMethod(folder, "create_message", "Kb", mid, associated);
	MAPISTORE_RETVAL_IF(!msg, MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	if (msg == Py_None) {
		Py_DECREF(msg);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	if (strcmp("MessageObject", msg->ob_type->tp_name)) {
		DEBUG(0, ("[ERR][%s][%s]: Expected MessageObject but got '%s'\n", pyobj->name,
			  __location__, msg->ob_type->tp_name));
		Py_DECREF(msg);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	pymsg = talloc_zero(pyobj, struct mapistore_python_object);
	MAPISTORE_RETVAL_IF(!pymsg, MAPISTORE_ERR_NO_MEMORY, NULL);

	pymsg->obj_type = MAPISTORE_PYTHON_OBJECT_MESSAGE;
	pymsg->name = talloc_strdup(pymsg, pyobj->name);
	MAPISTORE_RETVAL_IF(!pymsg->name, MAPISTORE_ERR_NO_MEMORY, NULL);
	pymsg->conn = pyobj->conn;
	pymsg->ictx = pyobj->ictx;
	pymsg->module = pyobj->module;
	pymsg->private_object = msg;
	*message_object = pymsg;

	talloc_set_destructor((void *)pymsg, (int (*)(void *))mapistore_python_object_destructor);

	Py_INCREF(msg);
	return MAPISTORE_SUCCESS;
}

/*
\param folder_object pointer to the folder object
\param mid the message identifier
\param flags specifies partial or temporary deletion

\return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
*/
static enum mapistore_error mapistore_python_folder_delete_message(void *folder_object,
								   uint64_t mid,
								   uint8_t flags)
{
	enum mapistore_error		retval;
	struct mapistore_python_object	*pyobj;
	PyObject			*folder;
	PyObject			*pres;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!folder_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the folder object */
	pyobj = (struct mapistore_python_object *) folder_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_FOLDER),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	folder = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("FolderObject", folder->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Call the "delete_folder" method */
	pres = PyObject_CallMethod(folder, "delete_message", "K", mid);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	retval = PyLong_AsLong(pres);

	Py_DECREF(pres);
	return retval;
}

/**
   \details Retrieve the number of children object of a given type
   within a folder

   \param folder_object the mapistore python object
   \param table_type the type of children object to lookup
   \param row_count pointer on the number of children objects to
   return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_folder_get_child_count(void *folder_object,
								    enum mapistore_table_type table_type,
								    uint32_t *row_count)
{
	struct mapistore_python_object	*pyobj;
	PyObject			*folder;
	PyObject			*pres;
	uint32_t			count = 0;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!folder_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!row_count, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the folder object */
	pyobj = (struct mapistore_python_object *) folder_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_FOLDER),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	folder = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("FolderObject", folder->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Call open_table function */
	pres = PyObject_CallMethod(folder, "get_child_count", "H", table_type);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	count = PyLong_AsLong(pres);
	*row_count = count;

	Py_DECREF(pres);
	return MAPISTORE_SUCCESS;
}

/**
   \details Create a table object

   \param mem_ctx pointer to the memory context
   \param folder_object the mapistore python parent object
   \param table_type the type of mapistore table to create
   \param handle_id the handle of the table *deprecated*
   \param table_object pointer on pointer to the table object to create
   \param row_count pointer on the number of elements in the table
   created

   \note handle_id is only used by SOGo backend to uniquely identify
   tables

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_folder_open_table(TALLOC_CTX *mem_ctx,
							       void *folder_object,
							       enum mapistore_table_type table_type,
							       uint32_t handle_id,
							       void **table_object,
							       uint32_t *row_count)
{
	struct mapistore_python_object		*pyobj;
	struct mapistore_python_object		*pytable;
	PyObject				*folder;
	PyObject				*table;
	PyObject				*pres;
	PyObject				*res;
	uint32_t				count = 0;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!folder_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!table_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!row_count, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the folder object */
	pyobj = (struct mapistore_python_object *) folder_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_FOLDER),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	folder = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("FolderObject", folder->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Call open_table function */
	pres = PyObject_CallMethod(folder, "open_table", "H", table_type);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Ensure a tuple was returned */
	if (PyTuple_Check(pres) == false) {
		DEBUG(0, ("[ERR][%s][%s]: Tuple expected to be returned in open_table\n",
			  pyobj->name, __location__));
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve table object (item 0 of the tuple) */
	table = PyTuple_GetItem(pres, 0);
	if (table == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed\n", pyobj->name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	} else if (strcmp("TableObject", table->ob_type->tp_name)) {
		DEBUG(0, ("[ERR][%s][%s]: Expected TableObject and got '%s'\n",
			  pyobj->name, __location__, table->ob_type->tp_name));
		Py_DECREF(pres);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	/* Retrieve table count (item 1 of the tuple) */
	res = PyTuple_GetItem(pres, 1);
	if (res == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed ", pyobj->name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}
	count = PyLong_AsLong(res);
	Py_DECREF(res);

	pytable = talloc_zero(pyobj, struct mapistore_python_object);
	MAPISTORE_RETVAL_IF(!pytable, MAPISTORE_ERR_NO_MEMORY, NULL);

	pytable->obj_type = MAPISTORE_PYTHON_OBJECT_TABLE;
	pytable->name = talloc_strdup(pytable, pyobj->name);
	MAPISTORE_RETVAL_IF(!pytable->name, MAPISTORE_ERR_NO_MEMORY, NULL);
	pytable->conn = pyobj->conn;
	pytable->ictx = pyobj->ictx;
	pytable->module = pyobj->module;
	pytable->private_object = table;

	talloc_set_destructor((void *)pytable, (int (*)(void *))mapistore_python_object_destructor);

	*table_object = pytable;
	*row_count = count;

	Py_INCREF(table);

	return MAPISTORE_SUCCESS;
}


/**
   \details Retrieve message general data and recipients

   \param mem_ctx pointer to the memory context
   \param message_object pointer to the message object
   \param message_data pointer on pointer to the message data to return

   \note the function is responsible for allocating message_data

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_message_get_message_data(TALLOC_CTX *mem_ctx,
								      void *message_object,
								      struct mapistore_message **message_data)
{
	enum mapistore_error		retval;
	struct mapistore_python_object	*pyobj;
	struct mapistore_message	*msgdata;
	PyObject			*message;
	PyObject			*pres;
	PyObject			*rlist;
	PyObject			*rdict;
	PyObject			*dict;
	PyObject			*ret;
	PyObject			*key;
	PyObject			*item;
	uint32_t			i;
	uint32_t			count;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!message_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!message_data, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the message object */
	pyobj = (struct mapistore_python_object *) message_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_MESSAGE),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	message = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!message, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("MessageObject", message->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Call get_message_data function */
	pres = PyObject_CallMethod(message, "get_message_data", NULL);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Ensure a tuple was returned */
	if (PyTuple_Check(pres) == false) {
		DEBUG(0, ("[ERR][%s][%s]: Tuple expected to be returned in get_message_data\n",
			  pyobj->name, __location__));
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve list of recipients (item 0 of the tuple) */
	rlist = PyTuple_GetItem(pres, 0);
	if (rlist == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed ",
			  pyobj->name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	if (PyList_Check(rlist) != true) {
		DEBUG(0, ("[ERR][%s][%s]: list expected to be returned but got '%s'\n",
			  pyobj->name, __location__, rlist->ob_type->tp_name));
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve message properties (item 1 of the tuple) */
	rdict = PyTuple_GetItem(pres, 1);
	if (rdict == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed ",
			  pyobj->name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	if (PyDict_Check(rdict) != true) {
		DEBUG(0, ("[ERR][%s][%s]: dict expected to be returned but got '%s'\n",
			  pyobj->name, __location__, rdict->ob_type->tp_name));
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Process message data */
	msgdata = talloc_zero(mem_ctx, struct mapistore_message);
	MAPISTORE_RETVAL_IF(!msgdata, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Message data */
	ret = PyDict_GetItemString(rdict, "PidTagSubjectPrefix");
	if (ret != NULL) {
		if ((PyString_Check(ret) == false) && (PyUnicode_Check(ret) == false)) {
			DEBUG(0, ("[ERR][%s][%s]: string expected to be returned but got '%s'\n",
				  pyobj->name, __location__, ret->ob_type->tp_name));
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		msgdata->subject_prefix = talloc_strdup(mem_ctx, PyString_AsString(ret));
		MAPISTORE_RETVAL_IF(!msgdata->subject_prefix, MAPISTORE_ERR_NO_MEMORY, msgdata);
	} else {
		msgdata->subject_prefix = NULL;
	}

	ret = PyDict_GetItemString(rdict, "PidTagNormalizedSubject");
	if (ret != NULL) {
		if ((PyString_Check(ret) != true) && (PyUnicode_Check(ret) != true)) {
			DEBUG(0, ("[ERR][%s][%s]: string expected to be returned but got '%s'\n",
				  pyobj->name, __location__, ret->ob_type->tp_name));
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		msgdata->normalized_subject = talloc_strdup(mem_ctx, PyString_AsString(ret));
		MAPISTORE_RETVAL_IF(!msgdata->normalized_subject, MAPISTORE_ERR_NO_MEMORY, msgdata);
	} else {
		msgdata->normalized_subject = NULL;
	}

	/* Message recipients */
	msgdata->columns = NULL;
	msgdata->recipients = NULL;

	msgdata->recipients_count = PyList_Size(rlist);
	if (msgdata->recipients_count) {
		msgdata->columns = set_SPropTagArray(msgdata, 0x8,
						     PidTagObjectType,
						     PidTagDisplayType,
						     PidTagSmtpAddress,
						     PidTagSendInternetEncoding,
						     PidTagRecipientDisplayName,
						     PidTagRecipientFlags,
						     PidTagRecipientEntryId,
						     PidTagRecipientTrackStatus);
		msgdata->recipients = talloc_array(msgdata, struct mapistore_message_recipient,
						   msgdata->recipients_count);
	} else {
		msgdata->columns = NULL;
		msgdata->recipients = NULL;
	}

	for (i = 0; i < msgdata->recipients_count; i++) {
		msgdata->recipients[i].username = NULL;

		dict = PyList_GetItem(rlist, i);
		if (dict == NULL) {
			DEBUG(0, ("[ERR][%s][%s]: PyList_GetItem failed ",
				  pyobj->name, __location__));
			PyErr_Print();
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}

		if (PyDict_Check(dict) != true) {
			DEBUG(0, ("[ERR][%s][%s]: dict expected to be returned but got '%s'\n",
				  pyobj->name, __location__, dict->ob_type->tp_name));
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}

		/* Retrieve ulRecipClass - PidTagRecipientType */
		key = PyString_FromString("PidTagRecipientType");
		if (key == NULL) {
			PyErr_Print();
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		if (PyDict_Contains(dict, key) == -1) {
			DEBUG(0, ("[ERR][%s][%s]: Missing PidTagRecipientType property for recipient %d\n",
				  pyobj->name, __location__, i));
			Py_DECREF(pres);
			Py_DECREF(key);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		item = PyDict_GetItem(dict, key);
		Py_DECREF(key);
		if (item == NULL) {
			DEBUG(0, ("[ERR][%s][%s]: PyDict_GetItem failed\n", pyobj->name,
				  __location__));
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		msgdata->recipients[i].type = PyLong_AsLong(item);
		if ((msgdata->recipients[i].type < MAPI_ORIG) &&
		    (msgdata->recipients[i].type > MAPI_BCC)) {
			DEBUG(0, ("[ERR][%s][%s]: Overflow error: %d\n",
				  pyobj->name, __location__, msgdata->recipients[i].type));
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}

		/* Recipient properties */
		msgdata->recipients[i].data = talloc_array(msgdata, void *, msgdata->columns->cValues);
		memset(msgdata->recipients[i].data, 0, msgdata->columns->cValues * sizeof (void *));
		count = 0;

		/* PidTagObjectType */
		key = PyString_FromString("PidTagObjectType");
		if (key == NULL) {
			PyErr_Print();
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		item = PyDict_GetItem(dict,key);
		Py_DECREF(key);
		if (item == NULL) {
			msgdata->recipients[i].data[count] = talloc_zero(msgdata->recipients[i].data, uint32_t);
			*((uint32_t *)(msgdata->recipients[i].data[count])) = MAPI_MAILUSER;
		} else {
			retval = mapistore_data_from_pyobject(msgdata->recipients[i].data,
							      PidTagObjectType, item,
							      &msgdata->recipients[i].data[count]);
			if (retval != MAPISTORE_SUCCESS) {
				DEBUG(0, ("[ERR][%s][%s]: Failed to retrieve PidTagObjectType\n",
					  pyobj->name, __location__));
				Py_DECREF(pres);
				return MAPISTORE_ERR_CONTEXT_FAILED;
			}
		}
		count++;

		/* PidTagDisplayType */
		key = PyString_FromString("PidTagDisplayType");
		if (key == NULL) {
			PyErr_Print();
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		item = PyDict_GetItem(dict,key);
		Py_DECREF(key);
		if (item == NULL) {
			msgdata->recipients[i].data[count] = talloc_zero(msgdata->recipients[i].data, uint32_t);
			*((uint32_t *)(msgdata->recipients[i].data[count])) = 0;
		} else {
			retval = mapistore_data_from_pyobject(msgdata->recipients[i].data,
							      PidTagDisplayType, item,
							      &msgdata->recipients[i].data[count]);
			if (retval != MAPISTORE_SUCCESS) {
				DEBUG(0, ("[ERR][%s][%s]: Failed to retrieve PidTagDisplayType\n",
					  pyobj->name, __location__));
				Py_DECREF(pres);
				return MAPISTORE_ERR_CONTEXT_FAILED;
			}
		}
		count++;

		/* PidTagSmtpAddress */
		key = PyString_FromString("PidTagSmtpAddress");
		if (key == NULL) {
			PyErr_Print();
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		item = PyDict_GetItem(dict,key);
		Py_DECREF(key);
		if (item == NULL) {
			msgdata->recipients[i].data[count] = NULL;
		} else {
			retval = mapistore_data_from_pyobject(msgdata->recipients[i].data,
							      PidTagSmtpAddress, item,
							      &msgdata->recipients[i].data[count]);
			if (retval != MAPISTORE_SUCCESS) {
				DEBUG(0, ("[ERR][%s][%s]: Failed to retrieve PidTagSmtpAddress\n",
					  pyobj->name, __location__));
				Py_DECREF(pres);
				return MAPISTORE_ERR_CONTEXT_FAILED;
			}
		}
		count++;

		/* PidTagSendInternetEncoding */
		key = PyString_FromString("PidTagSendInternetEncoding");
		if (key == NULL) {
			PyErr_Print();
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		item = PyDict_GetItem(dict,key);
		Py_DECREF(key);
		if (item == NULL) {
			msgdata->recipients[i].data[count] = talloc_zero(msgdata->recipients[i].data, uint32_t);
			*((uint32_t *)(msgdata->recipients[i].data[count])) = 0x00060000;
		} else {
			retval = mapistore_data_from_pyobject(msgdata->recipients[i].data,
							      PidTagSendInternetEncoding, item,
							      &msgdata->recipients[i].data[count]);
			if (retval != MAPISTORE_SUCCESS) {
				DEBUG(0, ("[ERR][%s][%s]: Failed to retrieve PidTagSendInternetEncoding\n",
					  pyobj->name, __location__));
				Py_DECREF(pres);
				return MAPISTORE_ERR_CONTEXT_FAILED;
			}
		}
		count++;

		/* PidTagRecipientDisplayName */
		key = PyString_FromString("PidTagRecipientDisplayName");
		if (key == NULL) {
			PyErr_Print();
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		item = PyDict_GetItem(dict,key);
		Py_DECREF(key);
		if (item == NULL) {
			msgdata->recipients[i].data[count] = NULL;
		} else {
			retval = mapistore_data_from_pyobject(msgdata->recipients[i].data,
							      PidTagRecipientDisplayName, item,
							      &msgdata->recipients[i].data[count]);
			if (retval != MAPISTORE_SUCCESS) {
				DEBUG(0, ("[ERR][%s][%s]: Failed to retrieve PidTagRecipientDisplayName\n",
					  pyobj->name, __location__));
				Py_DECREF(pres);
				return MAPISTORE_ERR_CONTEXT_FAILED;
			}
		}
		count++;

		/* PidTagRecipientFlags */
		key = PyString_FromString("PidTagRecipientFlags");
		if (key == NULL) {
			PyErr_Print();
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		item = PyDict_GetItem(dict,key);
		Py_DECREF(key);
		if (item == NULL) {
			msgdata->recipients[i].data[count] = talloc_zero(msgdata->recipients[i].data, uint32_t);
			*((uint32_t *)(msgdata->recipients[i].data[count])) = 0x01;
		} else {
			retval = mapistore_data_from_pyobject(msgdata->recipients[i].data,
							      PidTagRecipientFlags, item,
							      &msgdata->recipients[i].data[count]);
			if (retval != MAPISTORE_SUCCESS) {
				DEBUG(0, ("[ERR][%s][%s]: Failed to retrieve PidTagRecipientFlags\n",
					  pyobj->name, __location__));
				Py_DECREF(pres);
				return MAPISTORE_ERR_CONTEXT_FAILED;
			}
		}
		count++;

		/* PidTagRecipientEntryId */
		key = PyString_FromString("PidTagRecipientEntryId");
		if (key == NULL) {
			PyErr_Print();
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		item = PyDict_GetItem(dict,key);
		Py_DECREF(key);
		if (item == NULL) {
			msgdata->recipients[i].data[count] = NULL;
		} else {
			retval = mapistore_data_from_pyobject(msgdata->recipients[i].data,
							      PidTagRecipientEntryId, item,
							      &msgdata->recipients[i].data[count]);
			if (retval != MAPISTORE_SUCCESS) {
				DEBUG(0, ("[ERR][%s][%s]: Failed to retrieve PidTagRecipientEntryId\n",
					  pyobj->name, __location__));
				Py_DECREF(pres);
				return MAPISTORE_ERR_CONTEXT_FAILED;
			}
		}
		count++;

		/* PidTagRecipientTrackStatus */
		key = PyString_FromString("PidTagRecipientTrackStatus");
		if (key == NULL) {
			PyErr_Print();
			Py_DECREF(pres);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		item = PyDict_GetItem(dict,key);
		Py_DECREF(key);
		if (item == NULL) {
			msgdata->recipients[i].data[count] = talloc_zero(msgdata->recipients[i].data, uint32_t);
			*((uint32_t *)(msgdata->recipients[i].data[count])) = 0;
		} else {
			retval = mapistore_data_from_pyobject(msgdata->recipients[i].data,
							      PidTagRecipientTrackStatus, item,
							      &msgdata->recipients[i].data[count]);
			if (retval != MAPISTORE_SUCCESS) {
				DEBUG(0, ("[ERR][%s][%s]: Failed to retrieve PidTagRecipientTrackStatus\n",
					  pyobj->name, __location__));
				Py_DECREF(pres);
				return MAPISTORE_ERR_CONTEXT_FAILED;
			}
		}
		count++;
	}
	Py_DECREF(pres);

	*message_data = msgdata;

	return MAPISTORE_SUCCESS;
}


/**
   \details Modify Recipients on a message

   \param message_object pointer to the message object
   \param columns array of properties set on each mapistore_message_recipient
   \param count number of recipients
   \param recipients pointer to the list of recipients

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_message_modify_recipients(void *message_object,
								       struct SPropTagArray *columns,
								       uint16_t count,
								       struct mapistore_message_recipient *recipients)
{
	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	return MAPISTORE_SUCCESS;
}



/**
   \details Save message

   \param mem_ctx pointer to the memory context
   \param message_object pointer to the message object to save

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_message_save(TALLOC_CTX *mem_ctx,
							  void *message_object)
{
	enum mapistore_error		retval;
	struct mapistore_python_object	*pyobj;
	PyObject			*message;
	PyObject			*pres;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!message_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the message object */
	pyobj = (struct mapistore_python_object *) message_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_MESSAGE),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	message = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!message, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("MessageObject", message->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Call save function */
	pres = PyObject_CallMethod(message, "save", NULL);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	retval = PyLong_AsLong(pres);
	Py_DECREF(pres);

	return retval;
}


/**
   \details Open an attachment object

   \param mem_ctx pointer to the memory context
   \param message_object pointer to the message object
   \param attach_id the id of the attachment to retrieve
   \param attachment_object pointer on pointer to the attachment
   object to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_message_open_attachment(TALLOC_CTX *mem_ctx,
								     void *message_object,
								     uint32_t attach_id,
								     void **attachment_object)
{
	struct mapistore_python_object	*pyobj;
	struct mapistore_python_object	*pyattach;
	PyObject			*message;
	PyObject			*attach;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!message_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!attachment_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the message object */
	pyobj = (struct mapistore_python_object *) message_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_MESSAGE),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	message = (PyObject *) pyobj->private_object;
	MAPISTORE_RETVAL_IF(!message, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("MessageObject", message->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Call the open_attachment function */
	attach = PyObject_CallMethod(message, "open_attachment", "i", attach_id);
	if (attach == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	} else if (strcmp("AttachmentObject", attach->ob_type->tp_name)) {
		DEBUG(0, ("[ERR][%s][%s]: Expected AttachmentObject and got '%s'\n",
			  pyobj->name, __location__, attach->ob_type->tp_name));
		Py_DECREF(attach);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	pyattach = talloc_zero(pyobj, struct mapistore_python_object);
	MAPISTORE_RETVAL_IF(!pyattach, MAPISTORE_ERR_NO_MEMORY, NULL);

	pyattach->obj_type = MAPISTORE_PYTHON_OBJECT_ATTACHMENT;
	pyattach->name = talloc_strdup(pyattach, pyobj->name);
	MAPISTORE_RETVAL_IF(!pyattach->name, MAPISTORE_ERR_NO_MEMORY, pyattach);
	pyattach->conn = pyobj->conn;
	pyattach->ictx = pyobj->ictx;
	pyattach->module = pyobj->module;
	pyattach->private_object = attach;

	talloc_set_destructor((void *)pyattach, (int (*)(void *))mapistore_python_object_destructor);
	*attachment_object = pyattach;
	Py_INCREF(attach);

	return MAPISTORE_SUCCESS;
}

/**
   \details Delete an attachment object

   \param message_object pointer to the message object
   \param attach_id the id of the attachment to retrieve

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_message_delete_attachment(void *message_object,
								       uint32_t attach_id)
{
	struct mapistore_python_object	*pyobj;
	PyObject			*message;
	PyObject			*pres;
	enum mapistore_error		retval;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!message_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the message object */
	pyobj = (struct mapistore_python_object *) message_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_MESSAGE),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	message = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!message, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("MessageObject", message->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Call delete_attachment function */
	pres = PyObject_CallMethod(message, "delete_attachment", "i", attach_id);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	retval = PyLong_AsLong(pres);
	Py_DECREF(pres);

	return retval;
}

/**
   \details Open the attachment table

   \param mem_ctx pointer to the memory context
   \param message_object pointer to the message object
   \param table_object pointer to the table object to return
   \param row_count pointer to the number of elements in the table created

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_message_get_attachment_table(TALLOC_CTX *mem_ctx,
									  void *message_object,
									  void **table_object,
									  uint32_t *row_count)
{
	struct mapistore_python_object		*pyobj;
	struct mapistore_python_object		*pytable;
	PyObject				*message;
	PyObject				*table;
	PyObject				*pres;
	PyObject				*res;
	uint32_t				count = 0;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!message_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!table_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!row_count, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the message object */
	pyobj = (struct mapistore_python_object *) message_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_MESSAGE),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	message = (PyObject *) pyobj->private_object;
	MAPISTORE_RETVAL_IF(!message, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("MessageObject", message->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Call the get_attachment_table function */
	pres = PyObject_CallMethod(message, "get_attachment_table", NULL);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Ensure a tuple was returned */
	if (PyTuple_Check(pres) == false) {
		DEBUG(0, ("[ERR][%s][%s]: Tuple expected to be returned in open_table\n",
			  pyobj->name, __location__));
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve table object (item 0 of the tuple) */
	table = PyTuple_GetItem(pres, 0);
	if (table == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTiple_GetItem failed\n", pyobj->name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	} else if (strcmp("TableObject", table->ob_type->tp_name)) {
		DEBUG(0, ("[ERR][%s][%s]: Expected TableObject and got '%s'\n",
			  pyobj->name, __location__, table->ob_type->tp_name));
		Py_DECREF(pres);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	/* Retrieve table count (item 1 of the tuple) */
	res = PyTuple_GetItem(pres, 1);
	if (res == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed ", pyobj->name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}
	count = PyLong_AsLong(res);
	Py_DECREF(res);

	pytable = talloc_zero(pyobj, struct mapistore_python_object);
	MAPISTORE_RETVAL_IF(!pytable, MAPISTORE_ERR_NO_MEMORY, NULL);

	pytable->obj_type = MAPISTORE_PYTHON_OBJECT_TABLE;
	pytable->name = talloc_strdup(pytable, pyobj->name);
	MAPISTORE_RETVAL_IF(!pytable->name, MAPISTORE_ERR_NO_MEMORY, pytable);
	pytable->conn = pyobj->conn;
	pytable->ictx = pyobj->ictx;
	pytable->module = pyobj->module;
	pytable->private_object = table;

	talloc_set_destructor((void *)pytable, (int (*)(void *))mapistore_python_object_destructor);

	*table_object = pytable;
	*row_count = count;

	Py_INCREF(table);

	return MAPISTORE_SUCCESS;
}


/**
   \details Set columns on specified table

   \param table_object the table object to set properties on
   \param count the number of properties in the list
   \param properties the list of properties to add to the table

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_table_set_columns(void *table_object,
							       uint16_t count,
							       enum MAPITAGS *properties)
{
	enum mapistore_error		retval = MAPISTORE_SUCCESS;
	struct mapistore_python_object	*pyobj;
	PyObject			*table;
	PyObject			*pres;
	PyObject			*proplist;
	PyObject			*item;
	uint16_t			i;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!table_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!count, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!properties, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the table object */
	pyobj = (struct mapistore_python_object *) table_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_TABLE),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	table = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!table, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("TableObject", table->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Build a PyList from properties array */
	proplist = PyList_New(count);
	if (proplist == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to initialize Python List\n",
			  pyobj->name, __location__));
		return MAPISTORE_ERR_NO_MEMORY;
	}

	for (i = 0; i < count; i++) {
		item = mapistore_python_pyobject_from_proptag(properties[i]);
		if (!item || PyList_SetItem(proplist, i, item) == -1) {
			DEBUG(0, ("[ERR][%s][%s]: Unable to append entry to Python list\n",
				  pyobj->name, __location__));
			return MAPISTORE_ERR_NO_MEMORY;
		}
	}

	/* Call set_columns function */
	pres = PyObject_CallMethod(table, "set_columns", "O", proplist);
	Py_DECREF(proplist);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	retval = PyLong_AsLong(pres);
	Py_DECREF(pres);

	return retval;
}


/**
   \details Recursively builds a python object for MAPI restrictions

   \param res Pointer to the MAPI restriction to map

   \return Python object on success, otherwise NULL

 */
static PyObject *mapistore_python_add_restriction(struct mapi_SRestriction *res)
{
	TALLOC_CTX			*mem_ctx;
	struct mapi_SRestriction	*_res;
	struct SPropValue		*lpProp;
	PyObject			*pyobj = NULL;
	PyObject			*val = NULL;
	PyObject			*item = NULL;
	uint16_t			i;
	uint16_t			idx = 0;

	/* Initialize the dictionary */
	pyobj = PyDict_New();
	if (pyobj == NULL) {
		DEBUG(0, ("[ERR][%s]: Unable to initialize Python dictionary\n", __location__));
		return NULL;
	}

	switch (res->rt) {
	case RES_AND:
		PyDict_SetItemString(pyobj, "type", PyString_FromString("and"));
		val = PyList_New(res->res.resAnd.cRes);
		if (val == NULL) {
			DEBUG(0, ("[ERR][%s]: Unable to initialize Python list for RES_AND\n", __location__));
			return NULL;
		}
		for (i = 0, idx = 0; i < res->res.resAnd.cRes; i++) {
			_res = (struct mapi_SRestriction *) &(res->res.resAnd.res[i]);
			item = mapistore_python_add_restriction(_res);
			if (item) {
				if (PyList_SetItem(val, idx, item) == -1) {
					DEBUG(0, ("[ERR][%s]: Unable to append entry to Python list\n", __location__));
				} else {
					idx++;
				}
			}
		}
		PyDict_SetItemString(pyobj, "value", val);
		break;
	case RES_OR:
		PyDict_SetItemString(pyobj, "type", PyString_FromString("or"));
		val = PyList_New(res->res.resOr.cRes);
		if (val == NULL) {
			DEBUG(0, ("[ERR][%s]: Unable to initialize Python list for RES_OR\n", __location__));
			return NULL;
		}
		for (i = 0, idx = 0; i < res->res.resOr.cRes; i++) {
			_res = (struct mapi_SRestriction *) &(res->res.resOr.res[i]);
			item = mapistore_python_add_restriction(_res);
			if (item) {
				if (PyList_SetItem(val, idx, item) == -1) {
					DEBUG(0, ("[ERR][%s]: Unable to append entry to Python list\n", __location__));
				} else {
					idx++;
				}
			}
		}
		PyDict_SetItemString(pyobj, "value", val);
		break;
	case RES_NOT:
		break;
	case RES_CONTENT:
		PyDict_SetItemString(pyobj, "type", PyString_FromString("content"));
		val = PyList_New(0);
		switch (res->res.resContent.fuzzy & 0xFFFF) {
		case FL_FULLSTRING:
			item = PyString_FromString("FULLSTRING");
			break;
		case FL_SUBSTRING:
			item = PyString_FromString("SUBSTRING");
			break;
		case FL_PREFIX:
			item = PyString_FromString("PREFIX");
			break;
		}
		if (PyList_Append(val, item) == -1) {
			DEBUG(0, ("[ERR][%s]: Unable to append entry to Python list\n", __location__));
		}

		if (res->res.resContent.fuzzy & FL_IGNORECASE) {
			PyList_Append(val, PyString_FromString("IGNORECASE"));
		}
		if (res->res.resContent.fuzzy & FL_IGNORENONSPACE) {
			PyList_Append(val, PyString_FromString("IGNORENONSPACE"));
		}
		if (res->res.resContent.fuzzy & FL_LOOSE) {
			PyList_Append(val, PyString_FromString("LOOSE"));
		}
		PyDict_SetItemString(pyobj, "fuzzyLevel", val);

		item = mapistore_python_pyobject_from_proptag(res->res.resContent.ulPropTag);
		PyDict_SetItemString(pyobj, "property", item);

		mem_ctx = talloc_zero(NULL, TALLOC_CTX);
		lpProp = talloc_zero(mem_ctx, struct SPropValue);
		cast_SPropValue(mem_ctx, &res->res.resProperty.lpProp, lpProp);
		item = mapistore_pyobject_from_data(lpProp);
		talloc_free(mem_ctx);
		PyDict_SetItemString(pyobj, "value", item);
		break;
	case RES_PROPERTY:
		PyDict_SetItemString(pyobj, "type", PyString_FromString("property"));
		val = PyDict_New();
		if (val == NULL) {
			DEBUG(0, ("[ERR][%s]: Unable to initialize Python dictionary for RES_PROPERTY\n", __location__));
			return NULL;
		}
		PyDict_SetItemString(pyobj, "operator", PyLong_FromLong(res->res.resProperty.relop));

		item = mapistore_python_pyobject_from_proptag(res->res.resProperty.ulPropTag);
		PyDict_SetItemString(pyobj, "property", item);

		mem_ctx = talloc_zero(NULL, TALLOC_CTX);
		lpProp = talloc_zero(mem_ctx, struct SPropValue);
		cast_SPropValue(mem_ctx, &res->res.resProperty.lpProp, lpProp);
		item = mapistore_pyobject_from_data(lpProp);
		talloc_free(mem_ctx);
		PyDict_SetItemString(pyobj, "value", item);
		break;
	case RES_COMPAREPROPS:
		break;
	case RES_BITMASK:
		break;
	case RES_SIZE:
		break;
	case RES_EXIST:
		break;
	case RES_SUBRESTRICTION:
		break;
	case RES_COMMENT:
		break;
	}

	return pyobj;
}

/**
   \details Compile MAPI restrictions into a Python object

   \param table_object the table object to set restrictions for
   \param res Pointer to the restrictions to apply to the table
   \param table_status pointer to the status of the table to return

   \return MAPISTORE_SUCCESS on success,otherwise MAPISTORE error.
 */
static enum mapistore_error mapistore_python_table_set_restrictions(void *table_object,
								    struct mapi_SRestriction *res,
								    uint8_t *table_status)
{
	enum mapistore_error		retval;
	struct mapistore_python_object	*pyobj;
	PyObject			*table;
	PyObject			*pyres;
	PyObject			*pres;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!table_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!table_status, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the table object */
	pyobj = (struct mapistore_python_object *) table_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_TABLE),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	table = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("TableObject", table->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Special case where res is NULL - RopResetTable */
	if (res == NULL) {
		pyres = Py_None;
	} else {
		pyres = mapistore_python_add_restriction(res);
		if (pyres == NULL) {
			DEBUG(0, ("[ERR][%s][%s]: Unable to process restrictions\n", pyobj->name, __location__));
			*table_status = 0x0;
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}
	}

	/* Call set_restrictions function */
	pres = PyObject_CallMethod(table, "set_restrictions", "O", pyres);
	Py_DECREF(pyres);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ", pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	retval = PyLong_AsLong(pres);
	Py_DECREF(pres);

	*table_status = 0x0;
	return retval;
}


/**
   \details Set sort order for rows in a table

   \param table_object the table object to apply sort to
   \param sort_order Pointer to the sort order criterias to apply
   \param table_status pointer to the status of the table to return
 */
static enum mapistore_error mapistore_python_table_set_sort_order(void *table_object,
								  struct SSortOrderSet *sort_order,
								  uint8_t *table_status)
{
	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	*table_status = 0x0;
	return MAPISTORE_SUCCESS;
}


/**
   \details Retrieve a particular row from a table

   \param mem_ctx pointer to the memory context
   \param table_object the table object to retrieve row from
   \param query_type the query type
   \param row_id the index of the row to return
   \param data pointer on pointer to the data to return

   \note It is the responsibility of the caller to free
   mapistore_property_data

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_table_get_row(TALLOC_CTX *mem_ctx,
							   void *table_object,
							   enum mapistore_query_type query_type,
							   uint32_t row_id,
							   struct mapistore_property_data **data)
{
	struct mapistore_python_object	*pyobj;
	struct mapistore_property_data	*propdata;
	PyObject			*table;
	PyObject			*pres;
	PyObject			*pydict;
	PyObject			*pylist;
	PyObject			*item;
	PyObject			*value;
	char				*sproptag;
	int				proptag = 0;
	uint32_t			count = 0;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!table_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!data, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the table object */
	pyobj = (struct mapistore_python_object *) table_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_TABLE),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	table = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!table, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("TableObject", table->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Call get_row method */
	pres = PyObject_CallMethod(table, "get_row", "kH", row_id, query_type);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Ensure a tuple was returned */
	if (PyTuple_Check(pres) == false) {
		DEBUG(0, ("[ERR][%s][%s]: Tuple expected to be returned but fot '%s'\n",
			  pyobj->name, __location__, pres->ob_type->tp_name));
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve properties list (item 0 of the tuple) */
	pylist = PyTuple_GetItem(pres, 0);
	if (pylist == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed\n",
			  pyobj->name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	if (PyList_Check(pylist) != true) {
		DEBUG(0, ("[ERR][%s][%s]: list expected to be returned but got '%s'\n",
			  pyobj->name, __location__, pylist->ob_type->tp_name));
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve dictionary of properties (item 1 of the tuple) */
	pydict = PyTuple_GetItem(pres, 1);
	if (pydict == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyTuple_GetItem failed\n",
			  pyobj->name, __location__));
		PyErr_Print();
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	if (pydict == Py_None) {
		Py_DECREF(pres);
		return MAPISTORE_ERR_NOT_FOUND;
	}

	if (PyDict_Check(pydict) != true) {
		DEBUG(0, ("[ERR][%s][%s]: dict expected to be returned but got '%s'\n",
			  pyobj->name, __location__, pydict->ob_type->tp_name));
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Map row data */
	propdata = talloc_array(mem_ctx, struct mapistore_property_data, PyList_Size(pylist));
	if (!propdata) {
		Py_DECREF(pres);
		return MAPISTORE_ERR_NO_MEMORY;
	}
	for (count = 0; count < PyList_Size(pylist); count++) {
		item = PyList_GetItem(pylist, count);
		if (PyString_Check(item)) {
			sproptag = PyString_AsString(item);
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
		} else if (PyLong_Check(item)) {
			proptag = PyLong_AsLong(item);
			if (proptag == -1) {
				DEBUG(0, ("[ERR][%s][%s]: dict key is neither a string or int\n",
					  pyobj->name, __location__));
				Py_DECREF(pres);
				talloc_free(propdata);
				return MAPISTORE_ERR_CONTEXT_FAILED;
			}
		} else {
			DEBUG(0, ("[ERR][%s][%s]: dict key is neither a string or int\n",
				  pyobj->name, __location__));
			Py_DECREF(pres);
			talloc_free(propdata);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}

		value = PyDict_GetItem(pydict, item);
		if (value == NULL) {
			propdata[count].error = MAPISTORE_ERR_NOT_FOUND;
			propdata[count].data = NULL;
		} else {
			/* Map dict data to void * */
			propdata[count].error = mapistore_data_from_pyobject(mem_ctx, proptag,
									     value, &(propdata[count].data));
			if (propdata[count].error != MAPISTORE_SUCCESS) {
				propdata[count].data = NULL;
			}
		}
	}

	*data = propdata;

	Py_DECREF(pres);
	return MAPISTORE_SUCCESS;
}


/**
   \details Retrieve the number of rows in this table view

   \param table_object pointer to the mapistore table object
   \param query_type the type of query
   \param row_count pointer on the number of row to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_table_get_row_count(void *table_object,
							   enum mapistore_query_type query_type,
								 uint32_t *row_count)
{
	struct mapistore_python_object	*pyobj;
	PyObject			*table;
	PyObject			*pres;
	int				count = 0;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!table_object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!row_count, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the table object */
	pyobj = (struct mapistore_python_object *) table_object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_TABLE),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	table = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!table, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("TableObject", table->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Call get_row method */
	pres = PyObject_CallMethod(table, "get_row_count", "H", query_type);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	count = PyLong_AsLong(pres);
	Py_DECREF(pres);
	if (count == -1) {
		DEBUG(0, ("[ERR][%s][%s]: Overflow error\n",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	*row_count = count;
	return MAPISTORE_SUCCESS;
}


/**
   \details Retrieve all available properties of a given object

   \param mem_ctx pointer to the memory context
   \param object pointer to the object to retrieve properties from
   \param propertiesp pointer on pointer to the set of available properties to return

   \note properties is assumed to be allocated by the caller

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_properties_get_available_properties(TALLOC_CTX *mem_ctx,
										 void *object,
										 struct SPropTagArray **propertiesp)
{
	struct mapistore_python_object	*pyobj;
	struct SPropTagArray		*props;
	PyObject			*obj;
	PyObject			*proplist;
	PyObject			*pres;
	PyObject			*key;
	PyObject			*value;
	Py_ssize_t			cnt;
	const char			*sproptag;
	enum MAPITAGS			proptag;
	enum MAPISTATUS			retval;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!propertiesp, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the object */
	pyobj = (struct mapistore_python_object *) object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_FOLDER) &&
			    (pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_MESSAGE) &&
			    (pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_ATTACHMENT),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	obj = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!obj, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("FolderObject", obj->ob_type->tp_name) &&
			    strcmp("MessageObject", obj->ob_type->tp_name) &&
			    strcmp("AttachmentObject", obj->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Build empty list of properties */
	proplist = PyList_New(0);
	if (proplist == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to initialize Python list\n",
			  pyobj->name, __location__));
		return MAPISTORE_ERR_NO_MEMORY;
	}

	/* Call get_properties function */
	pres = PyObject_CallMethod(obj, "get_properties", "O", proplist);
	Py_DECREF(proplist);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: \n",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve dictionary of properties */
	if (PyDict_Check(pres) != true) {
		DEBUG(0, ("[ERR][%s][%s]: dict expected to be returned but got '%s'\n",
			  pyobj->name, __location__, pres->ob_type->tp_name));
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	props = talloc_zero(mem_ctx, struct SPropTagArray);
	if (props == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to allocate memory for SPropTagArray\n",
			  pyobj->name, __location__));
		Py_DECREF(pres);
		return MAPISTORE_ERR_NO_MEMORY;
	}

	props->aulPropTag = talloc_zero(props, void);
	if (props->aulPropTag == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to allocate memory for SPropTagArray->aulPropTag\n",
			  pyobj->name, __location__));
		Py_DECREF(pres);
		return MAPISTORE_ERR_NO_MEMORY;
	}

	cnt = 0;
	while (PyDict_Next(pres, &cnt, &key, &value)) {
		sproptag = PyString_AsString(key);
		if (sproptag) {
			proptag = openchangedb_named_properties_get_tag((char *)sproptag);
			if (proptag == 0xFFFFFFFF) {
				proptag = openchangedb_property_get_tag((char *)sproptag);
				if (proptag == 0xFFFFFFFF) {
					proptag = strtol(sproptag, NULL, 16);
				}
			}
			retval = SPropTagArray_add(props, props, proptag);
			if (retval != MAPI_E_SUCCESS) {
				DEBUG(0, ("[ERR][%s][%s]: Error getting the available properties\n",
					  pyobj->name, __location__));
				Py_DECREF(pres);
				return MAPISTORE_ERR_CONTEXT_FAILED;
			}
		}
	}
	Py_DECREF(pres);
	*propertiesp = props;

	return MAPISTORE_SUCCESS;
}


/**
   \details Retrieve set of properties on a given object

   \param mem_ctx pointer to the memory context
   \param object pointer to the object to retreive properties from
   \param count number of elements in properties array
   \param properties array of property tags
   \param data pointer on properties data to be returned

   \note data is assumed to be allocated by the caller

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_properties_get_properties(TALLOC_CTX *mem_ctx,
								       void *object,
								       uint16_t count,
								       enum MAPITAGS *properties,
								       struct mapistore_property_data *data)
{
	struct mapistore_python_object	*pyobj;
	PyObject			*obj;
	PyObject			*proplist;
	PyObject			*value;
	PyObject			*item;
	PyObject			*pres;
	uint16_t			i;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!data, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the object */
	pyobj = (struct mapistore_python_object *) object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_FOLDER) &&
			    (pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_MESSAGE) &&
			    (pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_ATTACHMENT),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	obj = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!obj, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("FolderObject", obj->ob_type->tp_name) &&
			    strcmp("MessageObject", obj->ob_type->tp_name) &&
			    strcmp("AttachmentObject", obj->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Build a PyList from properties array */
	proplist = PyList_New(count);
	if (proplist == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: Unable to initialize Python list\n",
			  pyobj->name, __location__));
		return MAPISTORE_ERR_NO_MEMORY;
	}

	for (i = 0; i < count; i++) {
		item = mapistore_python_pyobject_from_proptag(properties[i]);
		if (PyList_SetItem(proplist, i, item) == -1) {
			DEBUG(0, ("[ERR][%s][%s]: Unable to append entry to Python list\n",
				  pyobj->name, __location__));
			Py_DECREF(proplist);
			return MAPISTORE_ERR_NO_MEMORY;
		}
	}

	/* Call get_properties function */
	pres = PyObject_CallMethod(obj, "get_properties", "O", proplist);
	Py_DECREF(proplist);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: \n",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Retrieve dictionary of properties */
	if (PyDict_Check(pres) != true) {
		DEBUG(0, ("[ERR][%s][%s]: dict expected to be returned but got '%s'\n",
			  pyobj->name, __location__, pres->ob_type->tp_name));
		Py_DECREF(pres);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	/* Map data */
	for (i = 0; i < count; i++) {
		item = mapistore_python_pyobject_from_proptag(properties[i]);
		value = PyDict_GetItem(pres, item);
		if (value == NULL) {
			data[i].error = MAPISTORE_ERR_NOT_FOUND;
			data[i].data = NULL;
		} else {
			/* Map dict data to void */
			data[i].error = mapistore_data_from_pyobject(mem_ctx, properties[i],
								     value, &data[i].data);
			if (data[i].error != MAPISTORE_SUCCESS) {
				data[i].data = NULL;
			}
		}
		Py_DECREF(item);
	}

	Py_DECREF(pres);

	return MAPISTORE_SUCCESS;
}


/**
   \details Set properties on a given object

   \param object pointer to the object to set properties on
   \param aRow pointer to the SRow structure with properties to set

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_python_properties_set_properties(void *object,
								       struct SRow *aRow)
{
	enum mapistore_error		retval;
	struct mapistore_python_object	*pyobj;
	PyObject			*obj;
	PyObject			*pydict;
	PyObject			*pres;

	DEBUG(5, ("[INFO] %s\n", __FUNCTION__));

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!object, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!aRow, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the object */
	pyobj = (struct mapistore_python_object *) object;
	MAPISTORE_RETVAL_IF(!pyobj->module, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF((pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_FOLDER) &&
			    (pyobj->obj_type != MAPISTORE_PYTHON_OBJECT_MESSAGE),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	obj = (PyObject *)pyobj->private_object;
	MAPISTORE_RETVAL_IF(!obj, MAPISTORE_ERR_CONTEXT_FAILED, NULL);
	MAPISTORE_RETVAL_IF(strcmp("FolderObject", obj->ob_type->tp_name) &&
			    strcmp("MessageObject", obj->ob_type->tp_name),
			    MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	/* Build dictionary of properties */
	pydict = mapistore_python_dict_from_SRow(aRow);
	MAPISTORE_RETVAL_IF(!pydict, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Call set_properties function */
	pres = PyObject_CallMethod(obj, "set_properties", "O", pydict);
	Py_DECREF(pydict);
	if (pres == NULL) {
		DEBUG(0, ("[ERR][%s][%s]: PyObject_CallMethod failed: ",
			  pyobj->name, __location__));
		PyErr_Print();
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	retval = PyLong_AsLong(pres);
	Py_DECREF(pres);

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

	void *h = dlopen(PYTHON_DLOPEN, RTLD_NOW | RTLD_GLOBAL);
	DEBUG(0, ("[INFO] Preloading %s: %p\n", PYTHON_DLOPEN, h));

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
	/* backend.backend.create_root_folder = mapistore_python_backend_create_root_folder; */

	/* context */
	backend.context.get_path = mapistore_python_context_get_path;
	backend.context.get_root_folder = mapistore_python_context_get_root_folder;

	/* folder */
	backend.folder.open_folder = mapistore_python_folder_open_folder;
	backend.folder.create_folder = mapistore_python_folder_create_folder;
	backend.folder.delete = mapistore_python_folder_delete;
	backend.folder.open_message = mapistore_python_folder_open_message;
	backend.folder.create_message = mapistore_python_folder_create_message;
	backend.folder.delete_message = mapistore_python_folder_delete_message;
	/* backend.folder.move_copy_messages = mapistore_python_folder_move_copy_messages; */
	/* backend.folder.get_deleted_fmids = mapistore_python_folder_get_deleted_fmids; */
	backend.folder.get_child_count = mapistore_python_folder_get_child_count;
	backend.folder.open_table = mapistore_python_folder_open_table;
	/* backend.folder.modify_permissions = mapistore_python_folder_modify_permissions; */

	/* message */
	backend.message.get_message_data = mapistore_python_message_get_message_data;
	backend.message.modify_recipients = mapistore_python_message_modify_recipients;
	/* backend.message.set_read_flag = mapistore_python_message_set_read_flag; */
	backend.message.save = mapistore_python_message_save;
	/* backend.message.submit = mapistore_python_message_submit; */
	backend.message.open_attachment = mapistore_python_message_open_attachment;
	/* backend.message.create_attachment = mapistore_python_message_create_attachment; */
	backend.message.delete_attachment = mapistore_python_message_delete_attachment;
	backend.message.get_attachment_table = mapistore_python_message_get_attachment_table;
	/* backend.message.open_embedded_message = mapistore_python_message_open_embedded_message; */

	/* table */
	/* backend.table.get_available_properties = mapistore_python_table_get_available_properties; */
	backend.table.set_columns = mapistore_python_table_set_columns;
	backend.table.set_restrictions = mapistore_python_table_set_restrictions;
	backend.table.set_sort_order = mapistore_python_table_set_sort_order;
	backend.table.get_row = mapistore_python_table_get_row;
	backend.table.get_row_count = mapistore_python_table_get_row_count;
	/* backend.table.handle_destructor = mapistore_python_table_handle_destructor; */

	/* properties */
	backend.properties.get_available_properties = mapistore_python_properties_get_available_properties;
	backend.properties.get_properties = mapistore_python_properties_get_properties;
	backend.properties.set_properties = mapistore_python_properties_set_properties;

	/* management */
	/* backend.manager.generate_uri = mapistore_python_manager_generate_uri; */

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

	PyDateTime_IMPORT;

	retval = mapistore_set_pypath((char *)path);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	return mapistore_python_load_backends(mem_ctx, path);
}
