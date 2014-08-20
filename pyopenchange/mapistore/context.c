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
#include <inttypes.h>
#include "pyopenchange/mapistore/pymapistore.h"

static void py_MAPIStoreContext_dealloc(PyObject *_self)
{
	PyMAPIStoreContextObject *self = (PyMAPIStoreContextObject *) _self;

	mapistore_del_context(self->mstore_ctx, self->context_id);
	Py_XDECREF(self->parent);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreContext_open(PyMAPIStoreContextObject *self)
{
	PyMAPIStoreFolderObject		*folder;

	folder = PyObject_New(PyMAPIStoreFolderObject, &PyMAPIStoreFolder);
	
	folder->context = self;
	Py_INCREF(folder->context);

	folder->folder_object = self->folder_object;
	(void) talloc_reference(NULL, folder->folder_object);
	folder->fid = self->fid;
	
	return (PyObject *)folder;
}

/* FIXME: fix subscription code before reenabling this */
#if 0
static PyObject *py_MAPIStoreContext_register_subscription(PyMAPIStoreContextObject *self, PyObject *args)
{
	int						ret;
	struct mapistore_mgmt_notif			n;
	const char					*mapistoreURI;
	bool						WholeStore;
	uint16_t					NotificationFlags;
	uint64_t					FolderID;
	bool						softdeleted;
	struct mapistore_subscription_list		*subscription_list;
	struct mapistore_subscription			*subscription;
	struct mapistore_object_subscription_parameters	subscription_params;
	uint32_t					random_int;
	PyMAPIStoreGlobals				*globals;

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

		globals = get_PyMAPIStoreGlobals();
		ret = openchangedb_get_fid(globals->ocdb_ctx, mapistoreURI, &FolderID);
		if (ret != MAPISTORE_SUCCESS) {
			/* Try to retrieve URI from user indexing.tdb */
			ret = mapistore_indexing_record_get_fmid(self->mstore_ctx, 
								 self->mstore_ctx->conn_info->username,
								 mapistoreURI, false, &FolderID, &softdeleted);
			if (ret != MAPISTORE_SUCCESS || softdeleted == true) {
				return PyBool_FromLong(false);
			}
		}

		n.FolderID = FolderID;
		n.MessageID = 0;
		n.MAPIStoreURI = mapistoreURI;
	}
	

#if 0
	ret = mapistore_mgmt_interface_register_subscription(self->mstore_ctx->conn_info, &n);
#endif

	/* Upon success attach subscription to session object using
	 * existing mapistore_notification.c implementation */
	if (ret == MAPISTORE_SUCCESS) {
		subscription_list = talloc_zero(self->mstore_ctx, struct mapistore_subscription_list);
		DLIST_ADD(self->mstore_ctx->subscriptions, subscription_list);

		subscription_params.folder_id = n.FolderID;
		subscription_params.object_id = n.MessageID;
		subscription_params.whole_store = n.WholeStore;

		/* In OpenChange server, we use handle_id of the
		 * object, just use rand for now in bindings */
		random_int = rand();

		subscription = mapistore_new_subscription(subscription_list, 
							  self->mstore_ctx,
							  self->mstore_ctx->conn_info->username,
							  random_int, n.NotificationFlags, 
							  &subscription_params);
		subscription_list->subscription = subscription;
	}

	return PyInt_FromLong(!ret ? random_int : -1);
}

static PyObject *py_MAPIStoreContext_unregister_subscription(PyMAPIStoreContextObject *self, PyObject *args)
{
	int					ret;
	struct mapistore_mgmt_notif		n;
	const char				*mapistoreURI;
	bool					WholeStore;
	uint16_t				NotificationFlags;
	uint64_t				FolderID;
	uint32_t				identifier;
	PyMAPIStoreGlobals *globals;

	if (!PyArg_ParseTuple(args, "sbhi", &mapistoreURI, &WholeStore, &NotificationFlags, &identifier)) {
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
		globals = get_PyMAPIStoreGlobals();
		ret = openchangedb_get_fid(globals->ocdb_ctx, mapistoreURI, &FolderID);
		if (ret != MAPISTORE_SUCCESS) {
			/* Try to retrieve URI from user indexing.tdb */
		}

		n.FolderID = FolderID;
		n.MessageID = 0;
		n.MAPIStoreURI = mapistoreURI;
	}
	
#if 0
	ret = mapistore_mgmt_interface_unregister_subscription(self->mstore_ctx->conn_info, &n);
#endif

	/* Remove matching notifications from mapistore_notification system */
	if (ret == MAPISTORE_SUCCESS) {
		ret = mapistore_delete_subscription(self->mstore_ctx, identifier, NotificationFlags);
	}

	return PyBool_FromLong(!ret ? true : false);
}

static PyObject *py_MAPIStoreContext_get_notifications(PyMAPIStoreContextObject *self, PyObject *args)
{
	int					ret;
	struct mapistore_subscription_list	*sel;
	struct mapistore_notification_list	*nlist;

	for (sel = self->mstore_ctx->subscriptions; sel; sel = sel->next) {
		ret = mapistore_get_queued_notifications(self->mstore_ctx, sel->subscription, &nlist);
		if (ret == MAPISTORE_SUCCESS) {
			while (nlist) {
				printf("notification FolderID: 0x%"PRIx64"\n", 
				       nlist->notification->parameters.object_parameters.folder_id);
				printf("notification MessageID: 0x%"PRIx64"\n", 
				       nlist->notification->parameters.object_parameters.object_id);
				nlist = nlist->next;
			}
			talloc_free(nlist);
		}
	}

	if (ret != MAPISTORE_SUCCESS) {
		return Py_None;
	}
	return Py_None;
}
#endif /* disabled notifications */

static PyMethodDef mapistore_context_methods[] = {
	{ "open", (PyCFunction)py_MAPIStoreContext_open, METH_NOARGS },
#if 0
	{ "add_subscription", (PyCFunction)py_MAPIStoreContext_register_subscription, METH_VARARGS },
	{ "delete_subscription", (PyCFunction)py_MAPIStoreContext_unregister_subscription, METH_VARARGS },
	{ "get_notifications", (PyCFunction)py_MAPIStoreContext_get_notifications, METH_VARARGS },
#endif /* disabled notifications */
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

void initmapistore_context(PyObject *m)
{
	if (PyType_Ready(&PyMAPIStoreContext) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStoreContext);
}
