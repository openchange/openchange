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

static PyMethodDef mapistore_message_methods[] = {
	{ "get_uri", (PyCFunction)py_MAPIStoreMessage_get_uri, METH_NOARGS},
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
	.tp_name = "mapistore message",
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
	.tp_name = "child messages",
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
