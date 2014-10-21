/*
   OpenChange MAPI implementation.

   Python interface to mapistore attachment

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

static void py_MAPIStoreAttachment_dealloc(PyObject *_self)
{
	PyMAPIStoreAttachmentObject *self = (PyMAPIStoreAttachmentObject *)_self;

	Py_XDECREF(self->context);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreAttachment_get_properties(PyMAPIStoreAttachmentObject *self, PyObject *args, PyObject *kwargs)
{
	char				*kwnames[] = { "list", NULL };
	PyObject			*list = NULL, *py_ret = NULL;
	enum mapistore_error		retval;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O", kwnames, &list)) {
		return NULL;
	}

	retval = pymapistore_get_properties(list, self->context->mstore_ctx,
			self->context->context_id, self->attachment_object, &py_ret);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	return py_ret;
}

static PyObject *py_MAPIStoreAttachment_set_properties(PyMAPIStoreAttachmentObject *self, PyObject *args, PyObject *kwargs)
{
	char			*kwnames[] = { "dict", NULL };
	PyObject		*dict = NULL;
	enum mapistore_error	retval;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwnames, &dict)) {
		return NULL;
	}

	retval = pymapistore_set_properties(dict, self->context->mstore_ctx, self->context->context_id, self->attachment_object);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyMethodDef mapistore_attachment_methods[] = {
	{ "get_properties", (PyCFunction)py_MAPIStoreAttachment_get_properties, METH_VARARGS|METH_KEYWORDS },
	{ "set_properties", (PyCFunction)py_MAPIStoreAttachment_set_properties, METH_VARARGS|METH_KEYWORDS },
	{ NULL },
};

static PyObject *py_MAPIStoreAttachment_get_aid(PyMAPIStoreAttachmentObject *self, void *closure)
{
	return PyLong_FromUnsignedLong(self->aid);
}

static PyGetSetDef mapistore_attachment_getsetters[] = {
	{ (char *)"aid", (getter)py_MAPIStoreAttachment_get_aid, NULL, NULL },
	{ NULL }
};

PyTypeObject PyMAPIStoreAttachment = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapistoreattachment",
	.tp_basicsize = sizeof (PyMAPIStoreAttachmentObject),
	.tp_methods = mapistore_attachment_methods,
	.tp_getset = mapistore_attachment_getsetters,
	.tp_doc = "mapistore attachment object",
	.tp_dealloc = (destructor)py_MAPIStoreAttachment_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

static void py_MAPIStoreAttachments_dealloc(PyObject *_self)
{
	PyMAPIStoreAttachmentsObject *self = (PyMAPIStoreAttachmentsObject *)_self;

	Py_XDECREF(self->message);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreAttachments_iter(PyObject *self)
{
	Py_INCREF(self);
	return self;
}

static PyObject *py_MAPIStoreAttachments_next(PyObject *_self)
{
	PyMAPIStoreAttachmentsObject	*self;
	PyMAPIStoreAttachmentObject	*attachment;
	void				*attachment_object;
	enum mapistore_error		retval;

	self = (PyMAPIStoreAttachmentsObject *)_self;
	if (!self) {
		DEBUG(0, ("[ERR][%s]: Expected object of type 'MAPIStoreAttachments'\n", __location__));
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_PARAMETER);
		return NULL;
	}

	/* Check if there are remaining attachments */
	if(self->curr_index < self->count) {
		/* Retrieve the attachment */
		retval = mapistore_message_open_attachment(self->message->context->mstore_ctx,
				self->message->context->context_id, self->message->message_object,
				self->mem_ctx, self->curr_index, &attachment_object);
		if (retval != MAPISTORE_SUCCESS) {
			PyErr_SetMAPIStoreError(retval);
			return NULL;
		}

		/* Increment the index */
		self->curr_index += 1;

		/* Return the MAPIStoreAttachment object */
		attachment = PyObject_New(PyMAPIStoreAttachmentObject, &PyMAPIStoreAttachment);
		if (attachment == NULL) {
			PyErr_NoMemory();
			return NULL;
		}

		attachment->mem_ctx = self->mem_ctx;
		attachment->context = self->message->context;
		Py_INCREF(attachment->context);

		attachment->attachment_object = attachment_object;

		return (PyObject *)attachment;
	} else {
	    PyErr_SetNone(PyExc_StopIteration);
	    return NULL;
	}
}

PyTypeObject PyMAPIStoreAttachments = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapistoreattachments",
	.tp_basicsize = sizeof (PyMAPIStoreAttachmentsObject),
	.tp_iter = py_MAPIStoreAttachments_iter,
	.tp_iternext = py_MAPIStoreAttachments_next,
	.tp_doc = "iterator over folder MAPIStore Attachments",
	.tp_dealloc = (destructor)py_MAPIStoreAttachments_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER,
};

void initmapistore_attachment(PyObject *m)
{
	if (PyType_Ready(&PyMAPIStoreAttachment) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStoreAttachment);

	if (PyType_Ready(&PyMAPIStoreAttachments) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStoreAttachments);
}
