/*
   OpenChange MAPI implementation.

   Python interface to mapistore message

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
#include "gen_ndr/exchange.h"

static void py_MAPIStoreMessage_dealloc(PyObject *_self)
{
	PyMAPIStoreMessageObject *self = (PyMAPIStoreMessageObject *)_self;

	Py_XDECREF(self->context);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreMessage_get_uri(PyMAPIStoreMessageObject *self)
{
	TALLOC_CTX			*mem_ctx;
	enum mapistore_error 		retval;
	char				*uri;
	bool				soft_deleted;
	PyObject			*py_ret;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	/* Retrieve the URI from the indexing */
	retval = mapistore_indexing_record_get_uri(self->context->mstore_ctx, self->context->parent->username,
			self->mem_ctx, self->mid, &uri, &soft_deleted);

	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		talloc_free(mem_ctx);
		return NULL;
	}

	if (soft_deleted == true) {
		PyErr_SetString(PyExc_SystemError, "Soft-deleted message");
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Return the URI */
	py_ret = PyString_FromString(uri);
	talloc_free(mem_ctx);

	return py_ret;
}

static PyObject *py_MAPIStoreMessage_get_properties(PyMAPIStoreMessageObject *self, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX			*mem_ctx;
	char				*kwnames[] = { "list", NULL };
	PyObject			*list = NULL, *py_key, *py_ret = NULL;
	enum mapistore_error		retval;
	enum MAPISTATUS			ret;
	enum MAPITAGS			tag;
	struct SPropTagArray		*properties;
	struct mapistore_property_data  *prop_data;
	Py_ssize_t			i, count;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O", kwnames, &list)) {
		return NULL;
	}

	/* Get the available properties */
	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	if (list == NULL) {
		/* If no list of needed properties is provided, return all */
		retval = mapistore_properties_get_available_properties(self->context->mstore_ctx,
				self->context->context_id, self->message_object, mem_ctx, &properties);
		if (retval != MAPISTORE_SUCCESS) {
			PyErr_SetMAPIStoreError(retval);
			talloc_free(mem_ctx);
			return NULL;
		}
	} else {
		/* Check the input argument */
		if (PyList_Check(list) == false) {
			PyErr_SetString(PyExc_TypeError, "Input argument must be a list");
			talloc_free(mem_ctx);
			return NULL;
		}

		/* Build the SPropTagArray structure */
		count = PyList_Size(list);

		properties = talloc_zero(mem_ctx, struct SPropTagArray);
		properties->aulPropTag = talloc_zero(properties, void);
		for (i = 0; i < count; i++) {
			py_key = PyList_GetItem(list, i);
			if (PyString_Check(py_key)) {
				tag = openchangedb_property_get_tag(PyString_AsString(py_key));
				if (tag == 0xFFFFFFFF) {
					DEBUG(0, ("[WARN][%s]: Unsupported property tag '%s' \n",
							__location__, PyString_AsString(py_key)));
					PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_DATA);
					talloc_free(mem_ctx);
					return NULL;
				}
			} else if (PyInt_Check(py_key)) {
				tag = PyInt_AsUnsignedLongMask(py_key);
			} else {
				PyErr_SetString(PyExc_TypeError,
						"Invalid type in list: only strings and integers accepted");
				talloc_free(mem_ctx);
				return NULL;
			}

			ret = SPropTagArray_add(mem_ctx, properties, tag);
			if (ret != MAPI_E_SUCCESS) {
				PyErr_SetMAPISTATUSError(ret);
				talloc_free(mem_ctx);
				return NULL;
			}
		}
	}

	/* Get the available values */
	prop_data = talloc_array(mem_ctx, struct mapistore_property_data, properties->cValues);
	if (prop_data == NULL) {
		PyErr_NoMemory();
		talloc_free(mem_ctx);
		return NULL;
	}

	memset(prop_data, 0, sizeof(struct mapistore_property_data) * properties->cValues);

	retval = mapistore_properties_get_properties(self->context->mstore_ctx,
			self->context->context_id, self->message_object, mem_ctx,
			properties->cValues, properties->aulPropTag, prop_data);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Build a Python dictionary object with the tags and the property values */
	py_ret = pymapistore_python_dict_from_properties(properties->aulPropTag, prop_data, properties->cValues);
	if (py_ret == NULL) {
		PyErr_SetString(PyExc_SystemError, "Error building the dictionary");
		talloc_free(mem_ctx);
		return NULL;
	}
	talloc_free(mem_ctx);
	return py_ret;
}

static PyObject *py_MAPIStoreMessage_set_properties(PyMAPIStoreMessageObject *self, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX		*mem_ctx;
	char			*kwnames[] = { "dict", NULL };
	PyObject		*dict = NULL, *list, *py_key, *py_value;
	struct SRow		*aRow;
	struct SPropValue	newValue;
	void			*data;
	enum MAPITAGS		tag;
	enum mapistore_error	retval;
	enum MAPISTATUS		ret;
	size_t			i, count;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwnames, &dict)) {
		return NULL;
	}

	/* Check the input argument */
	if (PyDict_Check(dict) == false) {
		PyErr_SetString(PyExc_TypeError, "Input argument must be a dictionary");
		return NULL;
	}

	/* Get a tuple list with the keys */
	list = PyDict_Keys(dict);
	if (list == NULL) {
		PyErr_NoMemory();
		Py_DECREF(list);
		return NULL;
	}

	count = PyList_Size(list);

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		Py_DECREF(list);
		return NULL;
	}
	aRow = talloc_zero(mem_ctx, struct SRow);

	for (i = 0; i < count; i++) {
		/* Transform the key into a property tag */
		py_key = PyList_GetItem(list, i);

		if (PyString_Check(py_key)) {
			tag = openchangedb_property_get_tag(PyString_AsString(py_key));
			if (tag == 0xFFFFFFFF) {
				DEBUG(0, ("[ERR][%s]: Unsupported property tag '%s' \n",
						__location__, PyString_AsString(py_key)));
				PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_DATA);
				talloc_free(mem_ctx);
				Py_DECREF(list);
				return NULL;
			}
		} else if (PyInt_Check(py_key)) {
			tag = PyInt_AsUnsignedLongMask(py_key);
		} else {
			PyErr_SetString(PyExc_TypeError,
					"Invalid property type: only strings and integers accepted");
			talloc_free(mem_ctx);
			Py_DECREF(list);
			return NULL;
		}

		/* Transform the input value into proper C type */
		py_value = PyDict_GetItem(dict, py_key);

		retval = pymapistore_data_from_pyobject(mem_ctx,tag, py_value, &data);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("[WARN][%s]: Unsupported value for property '%s' \n",
					__location__, PyString_AsString(py_key)));
			continue;
		}

		/* Update aRow */
		if (set_SPropValue_proptag(&newValue, tag, data) == false) {
			PyErr_SetString(PyExc_SystemError, "Can't set property");
			talloc_free(mem_ctx);
			Py_DECREF(list);
			return NULL;
		}

		ret = SRow_addprop(aRow, newValue);
		if (ret != MAPI_E_SUCCESS) {
			PyErr_SetMAPISTATUSError(ret);
			talloc_free(mem_ctx);
			Py_DECREF(list);
			return NULL;
		}
	}

	/* Set the properties from aRow */
	retval = mapistore_properties_set_properties(self->context->mstore_ctx,self->context->context_id,
			self->message_object, aRow);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		talloc_free(mem_ctx);
		Py_DECREF(list);
		return NULL;
	}

	talloc_free(mem_ctx);
	Py_DECREF(list);
	Py_RETURN_NONE;
}

static PyMethodDef mapistore_message_methods[] = {
	{ "get_uri", (PyCFunction)py_MAPIStoreMessage_get_uri, METH_NOARGS},
	{ "get_properties", (PyCFunction)py_MAPIStoreMessage_get_properties, METH_VARARGS|METH_KEYWORDS},
	{ "set_properties", (PyCFunction)py_MAPIStoreMessage_set_properties, METH_VARARGS|METH_KEYWORDS},
	{ NULL },
};

static PyObject *py_MAPIStoreMessage_get_mid(PyMAPIStoreMessageObject *self, void *closure)
{
	return PyLong_FromLongLong(self->mid);
}

static PyGetSetDef mapistore_message_getsetters[] = {
	{ (char *)"mid", (getter)py_MAPIStoreMessage_get_mid, NULL, NULL },
	{ NULL }
};

PyTypeObject PyMAPIStoreMessage = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapistoremessage",
	.tp_basicsize = sizeof (PyMAPIStoreMessageObject),
	.tp_methods = mapistore_message_methods,
	.tp_getset = mapistore_message_getsetters,
	.tp_doc = "mapistore message object",
	.tp_dealloc = (destructor)py_MAPIStoreMessage_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

static void py_MAPIStoreMessages_dealloc(PyObject *_self)
{
	PyMAPIStoreMessagesObject *self = (PyMAPIStoreMessagesObject *)_self;

	talloc_free(self->mids);
	Py_XDECREF(self->folder);

	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreMessages_iter(PyObject *self)
{
	Py_INCREF(self);
	return self;
}

static PyObject *py_MAPIStoreMessages_next(PyObject *_self)
{
	uint64_t			mid;
	enum mapistore_error		retval;
	void 				*message_object;
	PyMAPIStoreMessageObject	*message;
	PyMAPIStoreMessagesObject 	*self;

	self = (PyMAPIStoreMessagesObject *)_self;
	if (!self) {
		PyErr_SetString(PyExc_TypeError, "Expected object of type MAPIStoreMessages");
		return NULL;
	}

	/* Check if there are remaining folders */
	if (self->curr_index < self->count) {
		/* Retrieve MID and increment curr_index*/
		mid = self->mids[self->curr_index];
		self->curr_index += 1;

		/* Use MID to open folder (read-only) */
		retval = mapistore_folder_open_message(self->folder->context->mstore_ctx,
							self->folder->context->context_id,
							self->folder->folder_object,
							self->mem_ctx, mid, false, &message_object);

		if (retval != MAPISTORE_SUCCESS) {
			PyErr_SetMAPIStoreError(retval);
			return NULL;
		}

		/* Return the MAPIStoreFolder object */
		message = PyObject_New(PyMAPIStoreMessageObject, &PyMAPIStoreMessage);
		if (message == NULL) {
			PyErr_NoMemory();
			return NULL;
		}

		message->mem_ctx = self->mem_ctx;
		message->context = self->folder->context;
		Py_INCREF(message->context);

		message->message_object = message_object;
		message->mid = mid;

		return (PyObject *)message;

	} else {
	    PyErr_SetNone(PyExc_StopIteration);
	    return NULL;
	}
}

PyTypeObject PyMAPIStoreMessages = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "childmessages",
	.tp_basicsize = sizeof (PyMAPIStoreMessagesObject),
	.tp_iter = py_MAPIStoreMessages_iter,
	.tp_iternext = py_MAPIStoreMessages_next,
	.tp_doc = "iterator over child messages of a folder",
	.tp_dealloc = (destructor)py_MAPIStoreMessages_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER,
};

void initmapistore_message(PyObject *m)
{
	if (PyType_Ready(&PyMAPIStoreMessage) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStoreMessage);

	if (PyType_Ready(&PyMAPIStoreMessages) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStoreMessages);
}
