/*
   OpenChange MAPI implementation.

   Python interface to mapistore context

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

static void py_MAPIStoreContext_dealloc(PyObject *_self)
{
	PyMAPIStoreContextObject *self = (PyMAPIStoreContextObject *) _self;

	mapistore_del_context(self->mstore_ctx, self->context_id);
	Py_XDECREF(self->parent);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreContext_open(PyMAPIStoreContextObject *self, PyObject *args)
{
	PyMAPIStoreFolderObject		*folder;

	folder = PyObject_New(PyMAPIStoreFolderObject, &PyMAPIStoreFolder);
	
	folder->context = self;
	folder->folder_object = NULL;
	folder->fid = self->fid;
	
	Py_INCREF(self);
	return (PyObject *)folder;
}

static PyObject *py_MAPIStoreContext_register_subscription(PyMAPIStoreContextObject *self, PyObject *args)
{
	int				ret;
	struct mapistore_mgmt_notif	n;
	const char			*mapistoreURI;
	bool				WholeStore;
	uint16_t			NotificationFlags;
	uint64_t			FolderID;

	if (!PyArg_ParseTuple(args, "sbh", &mapistoreURI, &WholeStore, &NotificationFlags)) {
		return NULL;
	}

	n.WholeStore = WholeStore;
	n.NotificationFlags = NotificationFlags;

	if (WholeStore == true) {
		n.FolderID = 0;
		n.MessageID = 0;
		n.MAPIStoreURI = NULL;
	} else {
		/* Retrieve folderID from mapistoreURI in openchange.ldb */
		ret = openchangedb_get_fid(self->parent->ocdb_ctx, mapistoreURI, &FolderID);
		if (ret != MAPISTORE_SUCCESS) {
			/* Try to retrieve URI from user indexing.tdb */
		}

		n.FolderID = FolderID;
		n.MessageID = 0;
		n.MAPIStoreURI = mapistoreURI;
	}
	

	ret = mapistore_mgmt_interface_register_subscription(self->mstore_ctx->conn_info, &n);

	return PyBool_FromLong(!ret ? true : false);
}

static PyObject *py_MAPIStoreContext_unregister_subscription(PyMAPIStoreContextObject *self, PyObject *args)
{
	int				ret;
	struct mapistore_mgmt_notif	n;
	const char			*mapistoreURI;
	bool				WholeStore;
	uint16_t			NotificationFlags;
	uint64_t			FolderID;

	if (!PyArg_ParseTuple(args, "sbh", &mapistoreURI, &WholeStore, &NotificationFlags)) {
		return NULL;
	}

	n.WholeStore = WholeStore;
	n.NotificationFlags = NotificationFlags;

	if (WholeStore == true) {
		n.FolderID = 0;
		n.MessageID = 0;
		n.MAPIStoreURI = NULL;
	} else {
		/* Retrieve folderID from mapistoreURI in openchange.ldb */
		ret = openchangedb_get_fid(self->parent->ocdb_ctx, mapistoreURI, &FolderID);
		if (ret != MAPISTORE_SUCCESS) {
			/* Try to retrieve URI from user indexing.tdb */
		}

		n.FolderID = FolderID;
		n.MessageID = 0;
		n.MAPIStoreURI = mapistoreURI;
	}
	

	ret = mapistore_mgmt_interface_unregister_subscription(self->mstore_ctx->conn_info, &n);

	return PyBool_FromLong(!ret ? true : false);
}


static PyMethodDef mapistore_context_methods[] = {
	{ "open", (PyCFunction)py_MAPIStoreContext_open, METH_VARARGS },
	{ "add_subscription", (PyCFunction)py_MAPIStoreContext_register_subscription, METH_VARARGS },
	{ "delete_subscription", (PyCFunction)py_MAPIStoreContext_unregister_subscription, METH_VARARGS },
	{ NULL },
};

static PyGetSetDef mapistore_context_getsetters[] = {
	{ NULL }
};

PyTypeObject PyMAPIStoreContext = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapistore context",
	.tp_basicsize = sizeof (PyMAPIStoreContextObject),
	.tp_methods = mapistore_context_methods,
	.tp_getset = mapistore_context_getsetters,
	.tp_doc = "mapistore context object",
	.tp_dealloc = (destructor)py_MAPIStoreContext_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};
