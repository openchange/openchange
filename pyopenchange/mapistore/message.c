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
	PyObject			*py_ret;
	enum mapistore_error 		retval;
	char				*uri;
	bool				soft_deleted;

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
		goto end;
	}

	if (soft_deleted == true) {
		PyErr_SetString(PyExc_SystemError, "Soft-deleted message");
		goto end;
	}

	/* Return the URI */
	py_ret = PyString_FromString(uri);

	talloc_free(mem_ctx);
	return py_ret;
end:
	talloc_free(mem_ctx);
	return NULL;
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
			goto end;
		}
	} else {
		/* Check the input argument */
		if (PyList_Check(list) == false) {
			PyErr_SetString(PyExc_TypeError, "Input argument must be a list");
			goto end;
		}

		/* Build the SPropTagArray structure */
		count = PyList_Size(list);

		properties = talloc_zero(mem_ctx, struct SPropTagArray);
		if (properties == NULL) {
			PyErr_NoMemory();
			goto end;
		}

		properties->aulPropTag = talloc_zero(properties, void);
		if (properties->aulPropTag == NULL) {
			PyErr_NoMemory();
			goto end;
		}

		for (i = 0; i < count; i++) {
			py_key = PyList_GetItem(list, i);
			if (PyString_Check(py_key)) {
				tag = get_proptag_value(PyString_AsString(py_key));

				if (tag == 0xFFFFFFFF) {
					DEBUG(0, ("[WARN][%s]: Unsupported property tag '%s' \n",
							__location__, PyString_AsString(py_key)));
					PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_DATA);
					goto end;
				}
			} else if (PyInt_Check(py_key)) {
				tag = PyInt_AsUnsignedLongMask(py_key);
			} else {
				PyErr_SetString(PyExc_TypeError,
						"Invalid type in list: only strings and integers accepted");
				goto end;
			}

			ret = SPropTagArray_add(mem_ctx, properties, tag);
			if (ret != MAPI_E_SUCCESS) {
				PyErr_SetMAPISTATUSError(ret);
				goto end;
			}
		}
	}

	/* Get the available values */
	prop_data = talloc_zero_array(mem_ctx, struct mapistore_property_data, properties->cValues);
	if (prop_data == NULL) {
		PyErr_NoMemory();
		goto end;
	}

	retval = mapistore_properties_get_properties(self->context->mstore_ctx,
			self->context->context_id, self->message_object, mem_ctx,
			properties->cValues, properties->aulPropTag, prop_data);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		goto end;
	}

	/* Build a Python dictionary object with the tags and the property values */
	py_ret = pymapistore_python_dict_from_properties(properties->aulPropTag, prop_data, properties->cValues);
	if (py_ret == NULL) {
		PyErr_SetString(PyExc_SystemError, "Error building the dictionary");
		goto end;
	}

	talloc_free(mem_ctx);
	return py_ret;

end:
	talloc_free(mem_ctx);
	return NULL;
}

static PyObject *py_MAPIStoreMessage_set_properties(PyMAPIStoreMessageObject *self, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX		*mem_ctx;
	char			*kwnames[] = { "dict", NULL };
	PyObject		*dict = NULL, *py_key, *py_value;
	struct SRow		*aRow;
	struct SPropValue	newValue;
	void			*data;
	enum MAPITAGS		tag;
	enum mapistore_error	retval;
	enum MAPISTATUS		ret;
	Py_ssize_t		pos = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwnames, &dict)) {
		return NULL;
	}

	/* Check the input argument */
	if (PyDict_Check(dict) == false) {
		PyErr_SetString(PyExc_TypeError, "Input argument must be a dictionary");
		return NULL;
	}

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	aRow = talloc_zero(mem_ctx, struct SRow);
	if (aRow == NULL) {
		PyErr_NoMemory();
		goto end;
	}

	while (PyDict_Next(dict, &pos, &py_key, &py_value)) {
		/* Transform the key into a property tag */
		if (PyString_Check(py_key)) {
			tag = get_proptag_value(PyString_AsString(py_key));

			if (tag == 0xFFFFFFFF) {
				DEBUG(0, ("[ERR][%s]: Unsupported property tag '%s' \n",
						__location__, PyString_AsString(py_key)));
				PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_DATA);
				goto end;
			}
		} else if (PyInt_Check(py_key)) {
			tag = PyInt_AsUnsignedLongMask(py_key);
		} else {
			PyErr_SetString(PyExc_TypeError,
					"Invalid property type: only strings and integers accepted");
			goto end;
		}

		/* Transform the input value into proper C type */
		retval = pymapistore_data_from_pyobject(mem_ctx,tag, py_value, &data);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("[WARN][%s]: Unsupported value for property '%s' \n",
					__location__, PyString_AsString(py_key)));
			continue;
		}

		/* Update aRow */
		if (set_SPropValue_proptag(&newValue, tag, data) == false) {
			PyErr_SetString(PyExc_SystemError, "Can't set property");
			goto end;
		}

		ret = SRow_addprop(aRow, newValue);
		if (ret != MAPI_E_SUCCESS) {
			PyErr_SetMAPISTATUSError(ret);
			goto end;
		}
	}

	/* Set the properties from aRow */
	retval = mapistore_properties_set_properties(self->context->mstore_ctx,self->context->context_id,
			self->message_object, aRow);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		goto end;
	}

	talloc_free(mem_ctx);
	Py_RETURN_NONE;
end:
	talloc_free(mem_ctx);
	return NULL;
}

static PyObject *py_MAPIStoreMessage_get_message_data(PyMAPIStoreMessageObject *self)
{
	TALLOC_CTX			*mem_ctx;
	PyObject			*py_ret = NULL, *py_val, *py_user_dict, *py_user_val;
	struct mapistore_message	*msg_data;
	struct mapistore_property_data  *prop_data;
	const char			*proptag;
	enum mapistore_error		retval;
	uint32_t			tag_count = 0, recipients_count, i, j;
	int				ret;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	/* Retrieve the message data */
	retval = mapistore_message_get_message_data(self->context->mstore_ctx, self->context->context_id,
						    self->message_object, mem_ctx,
						    &msg_data);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Build the dictionary */
	py_ret = PyDict_New();
	if (py_ret == NULL) {
		PyErr_NoMemory();
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Build a dictionary with the message properties */
	/* "Subject prefix" */
	if (msg_data->subject_prefix != NULL) {
		py_val = PyString_FromString(msg_data->subject_prefix);
	} else {
		py_val = PyString_FromString("Nan");
	}

	ret = PyDict_SetItem(py_ret, PyString_FromString("Subject prefix"), py_val);
	if (ret != 0) {
		Py_DECREF(py_val);
		goto end;
	}
	Py_DECREF(py_val);

	/* "Normalized subject" */
	if (msg_data->normalized_subject != NULL) {
		py_val = PyString_FromString(msg_data->normalized_subject);
	} else {
		py_val = PyString_FromString("Nan");
	}

	ret = PyDict_SetItem(py_ret, PyString_FromString("Normalized subject"), py_val);
	if (ret != 0) {
		Py_DECREF(py_val);
		goto end;
	}
	Py_DECREF(py_val);

	/* "Recipient count" */
	recipients_count = msg_data->recipients_count;
	py_val = PyInt_FromLong(recipients_count);
	ret = PyDict_SetItem(py_ret, PyString_FromString("Recipient count"), py_val);
	if (ret != 0) {
		Py_DECREF(py_val);
		goto end;
	}
	Py_DECREF(py_val);

	/* "Recipient columns" */
	if (msg_data->columns != NULL) {
		tag_count = msg_data->columns->cValues;
		py_val = PyList_New(tag_count);
		if (py_val == NULL) {
			PyErr_NoMemory();
			goto end;
		}

		for (i = 0; i < tag_count; i++) {
			proptag = get_proptag_name(msg_data->columns->aulPropTag[i]);
			if (proptag) {
				ret = PyList_SetItem(py_val, i, PyString_FromString(proptag));
			} else {
				ret = PyList_SetItem(py_val, i, PyString_FromFormat("0x%x",
						msg_data->columns->aulPropTag[i]));
			}
			if (ret != 0) {
				PyErr_SetString(PyExc_SystemError, "Unable to set item in column list");
				Py_DECREF(py_val);
				goto end;
			}
		}
	} else {
		py_val = PyString_FromString("Nan");
	}

	ret = PyDict_SetItem(py_ret, PyString_FromString("Recipient columns"), py_val);
	if (ret != 0) {
		Py_DECREF(py_val);
		goto end;
	}

	Py_DECREF(py_val);

	/* "Recipient data" */
	py_val = PyList_New(recipients_count);
	if (py_val == NULL) {
		PyErr_NoMemory();
		goto end;
	}

	for (i = 0; i < recipients_count; i++){
		/* Build the mapistore_property_data structure */
		prop_data = talloc_zero_array(mem_ctx, struct mapistore_property_data, tag_count);
		if (prop_data == NULL) {
			PyErr_NoMemory();
			Py_DECREF(py_val);
			goto end;
		}

		for (j = 0; j < tag_count; j++){
			if (msg_data->recipients[i].data[j] != NULL) {
				prop_data[j].data = msg_data->recipients[i].data[j];
				prop_data[j].error = MAPISTORE_SUCCESS;
			} else {
				prop_data[j].error = MAPISTORE_ERR_NOT_FOUND;
			}
		}

		/* Get a dictionary with the user properties */
		py_user_dict = pymapistore_python_dict_from_properties(msg_data->columns->aulPropTag, prop_data, tag_count);
		if (py_ret == NULL) {
			PyErr_SetString(PyExc_SystemError, "Error building the recipient data dictionary");
			Py_DECREF(py_val);
			goto end;
		}

		if (msg_data->recipients[i].username != NULL) {
			py_user_val = PyString_FromString(msg_data->recipients[i].username);
		} else {
			py_user_val = PyString_FromString("Nan");
		}

		ret = PyDict_SetItem(py_user_dict, PyString_FromString("Username"), py_user_val);
		if (ret != 0) {
			Py_DECREF(py_val);
			Py_DECREF(py_user_dict);
			Py_DECREF(py_user_val);
			goto end;
		}

		Py_DECREF(py_user_val);

		ret = PyDict_SetItem(py_user_dict, PyString_FromString("Type"),
				PyLong_FromLong(msg_data->recipients[i].type));
		if (ret != 0) {
			Py_DECREF(py_val);
			Py_DECREF(py_user_dict);
			goto end;
		}

		ret = PyList_SetItem(py_val, i, py_user_dict);
		if (ret != 0) {
			Py_DECREF(py_val);
			goto end;
		}
	}

	ret = PyDict_SetItem(py_ret, PyString_FromString("Recipient data"), py_val);
	if (ret != 0) {
		Py_DECREF(py_val);
		goto end;
	}

	Py_DECREF(py_val);

	talloc_free(mem_ctx);
	return py_ret;

end:
	Py_DECREF(py_ret);
	talloc_free(mem_ctx);
	return NULL;
}

static PyObject *py_MAPIStoreMessage_save(PyMAPIStoreMessageObject *self)
{
	TALLOC_CTX		*mem_ctx;
	enum mapistore_error	retval;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	retval = mapistore_message_save(self->context->mstore_ctx, self->context->context_id,
			self->message_object, mem_ctx);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		talloc_free(mem_ctx);
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject *py_MAPIStoreMessage_create_attachment(PyMAPIStoreMessageObject *self)
{
	PyMAPIStoreAttachmentObject	*attachment;
	void				*attachment_object;
	enum mapistore_error		retval;
	uint32_t			aid;

	/* Create attachment */
	retval = mapistore_message_create_attachment(self->context->mstore_ctx,self->context->context_id,
			self->message_object, self->mem_ctx, &attachment_object, &aid);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	/* Return the attachment object */
	attachment = PyObject_New(PyMAPIStoreAttachmentObject, &PyMAPIStoreAttachment);
	if (attachment == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	attachment->mem_ctx = self->mem_ctx;
	attachment->context = self->context;
	Py_INCREF(attachment->context);

	attachment->attachment_object = attachment_object;
	printf("\n***AID: %d\n", aid);

	return (PyObject *)attachment;
}

static PyObject *py_MAPIStoreMessage_open_attachment_table(PyMAPIStoreMessageObject *self)
{
	PyMAPIStoreTableObject		*table;
	void				*table_object;
	struct SPropTagArray		*columns;
	enum mapistore_error		retval;
	uint32_t			count = 0;

	retval = mapistore_message_get_attachment_table(self->context->mstore_ctx, self->context->context_id,
			self->message_object, self->mem_ctx, &table_object, &count);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	/* Initialise the SPropTagArray */
	columns = talloc_zero(self->mem_ctx, struct SPropTagArray);
	if (columns == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	columns->aulPropTag = talloc_zero(columns, void);
	if (columns->aulPropTag == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	/* Return the table object */
	table = PyObject_New(PyMAPIStoreTableObject, &PyMAPIStoreTable);
	if (table == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	table->mem_ctx = self->mem_ctx;
	table->context = self->context;
	Py_INCREF(table->context);

	table->columns = columns;
	table->table_object = table_object;

	return (PyObject *)table;
}

static PyMethodDef mapistore_message_methods[] = {
	{ "get_uri", (PyCFunction)py_MAPIStoreMessage_get_uri, METH_NOARGS},
	{ "get_properties", (PyCFunction)py_MAPIStoreMessage_get_properties, METH_VARARGS|METH_KEYWORDS},
	{ "set_properties", (PyCFunction)py_MAPIStoreMessage_set_properties, METH_VARARGS|METH_KEYWORDS},
	{ "save", (PyCFunction)py_MAPIStoreMessage_save, METH_NOARGS},
	{ "get_data", (PyCFunction)py_MAPIStoreMessage_get_message_data, METH_NOARGS},
	{ "create_attachment", (PyCFunction)py_MAPIStoreMessage_create_attachment, METH_NOARGS},
	{ "open_attachment_table", (PyCFunction)py_MAPIStoreMessage_open_attachment_table, METH_NOARGS},
	{ NULL },
};

static PyObject *py_MAPIStoreMessage_get_mid(PyMAPIStoreMessageObject *self, void *closure)
{
	return PyLong_FromUnsignedLongLong(self->mid);
}

static PyObject *py_MAPIStoreMessage_get_attachment_count(PyMAPIStoreMessageObject *self, void *closure)
{
	TALLOC_CTX			*mem_ctx;
	void				*table_object;
	enum mapistore_error		retval;
	uint32_t			count;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	retval = mapistore_message_get_attachment_table(self->context->mstore_ctx, self->context->context_id,
			self->message_object, mem_ctx, &table_object, &count);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		talloc_free(mem_ctx);
		return NULL;
	}

	talloc_free(mem_ctx);
	return PyLong_FromUnsignedLong(count);
}

static PyGetSetDef mapistore_message_getsetters[] = {
	{ (char *)"mid", (getter)py_MAPIStoreMessage_get_mid, NULL, NULL },
	{ (char *)"attachment_count", (getter)py_MAPIStoreMessage_get_attachment_count, NULL, NULL },
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
	PyMAPIStoreMessagesObject 	*self;
	PyMAPIStoreMessageObject	*message;
	void 				*message_object;
	enum mapistore_error		retval;
	uint64_t			mid;

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
