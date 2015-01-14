/*
   OpenChange MAPI implementation.

   Python interface to mapistore management

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
#include "pyopenchange/mapistore/pymapistore.h"

static void py_MAPIStoreMGMT_dealloc(PyObject *_self)
{
	PyMAPIStoreMGMTObject *self = (PyMAPIStoreMGMTObject *)_self;

	printf("deallocate MGMT object\n");
	mapistore_mgmt_release(self->mgmt_ctx);

	Py_XDECREF(self->parent);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreMGMT_registered_backend(PyMAPIStoreMGMTObject *self, PyObject *args)
{
	int		ret;
	const char	*bname;

	if (!PyArg_ParseTuple(args, "s", &bname)) {
		return NULL;
	}

	ret = mapistore_mgmt_registered_backend(self->mgmt_ctx, bname);
	return PyBool_FromLong(ret == MAPISTORE_SUCCESS ? true : false);

}

static PyObject *py_MAPIStoreMGMT_registered_users(PyMAPIStoreMGMTObject *self, PyObject *args)
{
	PyObject				*dict;
	PyObject				*userlist;
	const char				*backend;
	const char				*vuser;
	struct mapistore_mgmt_users_list	*ulist;
	int					i;

	if (!PyArg_ParseTuple(args, "ss", &backend, &vuser)) {
		return NULL;
	}

	dict = PyDict_New();
	PyDict_SetItemString(dict, "backend", PyString_FromString(backend));
	PyDict_SetItemString(dict, "user", PyString_FromString(vuser));
	
	ulist = mapistore_mgmt_registered_users(self->mgmt_ctx, backend, vuser);
	userlist = PyList_New(0);

	if (ulist && ulist->count != 0) {
		PyDict_SetItem(dict, PyString_FromString("count"), PyLong_FromLong(ulist->count));
		for (i = 0; i < ulist->count; i++) {
			PyList_Append(userlist, PyString_FromString(ulist->user[i]));
		}
	} else {
		PyDict_SetItem(dict, PyString_FromString("count"), PyLong_FromLong(0));
	}
	PyDict_SetItem(dict, PyString_FromString("usernames"), userlist);

	if (ulist) {
		talloc_free(ulist);
	}

	return (PyObject *)dict;
}

static PyObject *py_MAPIStoreMGMT_registered_message(PyMAPIStoreMGMTObject *self, PyObject *args)
{
	const char	*backend;
	const char	*sysuser;
	const char	*vuser;
	const char	*folder;
	const char	*message;
	const char	*rootURI;
	int		ret;

	if (!PyArg_ParseTuple(args, "ssszzs", &backend, &sysuser, &vuser, &folder, &rootURI, &message)) {
		return NULL;
	}

	ret = mapistore_mgmt_registered_message(self->mgmt_ctx, backend, sysuser, vuser, folder, rootURI, message);

	return PyBool_FromLong(ret);
}

static PyObject *py_MAPIStoreMGMT_register_message(PyMAPIStoreMGMTObject *self, PyObject *args)
{
	const char	*backend;
	const char	*user;
	const char	*uri;
	const char	*messageID;
	char		*registered_uri;
	PyObject	*retlist;
	uint64_t	mid;
	int		ret;

	if (!PyArg_ParseTuple(args, "ssss", &backend, &user, &uri, &messageID)) {
		return NULL;
	}

	retlist = PyList_New(0);

	/* Gets a new message ID */
	ret = mapistore_indexing_get_new_folderID(self->mgmt_ctx->mstore_ctx, &mid);
	if (ret) return (PyObject *)retlist;

	/* Register the message within specified user indexing database */
	ret = mapistore_mgmt_register_message(self->mgmt_ctx, backend, user, mid, uri, messageID, &registered_uri);
	if (ret) return (PyObject *)retlist;

	PyList_Append(retlist, PyLong_FromLongLong(mid));
	PyList_Append(retlist, PyString_FromString(registered_uri));
	
	talloc_free(registered_uri);
	return (PyObject *) retlist;
}

static PyObject *py_MAPIStoreMGMT_existing_users(PyMAPIStoreMGMTObject *self, PyObject *args)
{
	PyObject	*dict;
	PyObject	*userlist;
	PyObject	*item;
	char		**MAPIStoreURI;
	char		**users;
	uint32_t	count;
	char		*uri;
	const char	*backend;
	const char	*vuser;
	const char	*folder;
	int		ret;
	int		i;
	PyMAPIStoreGlobals *globals;

	if (!PyArg_ParseTuple(args, "sss", &backend, &vuser, &folder)) {
		return NULL;
	}	

	dict = PyDict_New();
	userlist = PyList_New(0);
	PyDict_SetItemString(dict, "backend", PyString_FromString(backend));
	PyDict_SetItemString(dict, "user", PyString_FromString(vuser));
	PyDict_SetItemString(dict, "count", PyLong_FromLong(0));
	PyDict_SetItem(dict, PyString_FromString("infos"), userlist);

	ret = mapistore_mgmt_generate_uri(self->mgmt_ctx, backend, vuser, folder, NULL, NULL, &uri);
	if (ret != MAPISTORE_SUCCESS) return (PyObject *)dict;
	printf("uri: %s\n", uri);

	globals = get_PyMAPIStoreGlobals();
	ret = openchangedb_get_users_from_partial_uri(self->mgmt_ctx, globals->ocdb_ctx, uri, 
						      &count, &MAPIStoreURI, &users);
	if (ret != MAPISTORE_SUCCESS) return (PyObject *)dict;

	PyDict_SetItemString(dict, "count", PyLong_FromLong(count));
	for (i = 0; i != count; i++) {
		item = PyDict_New();
		PyDict_SetItemString(item, "username", PyString_FromString(users[i]));
		PyDict_SetItemString(item, "mapistoreURI", PyString_FromString(MAPIStoreURI[i]));
		PyList_Append(userlist, item);
	}
	PyDict_SetItem(dict, PyString_FromString("infos"), userlist);

	return (PyObject *)dict;
}

static PyObject *py_MAPIStoreMGMT_registered_subscription(PyMAPIStoreMGMTObject *self, PyObject *args)
{
	int		ret;
	const char	*username;
	const char	*uri;
	uint16_t	type;
	uint16_t	NotificationFlags;

	if (!PyArg_ParseTuple(args, "sshh", &username, &uri, &type, &NotificationFlags)) {
		return NULL;
	}

	switch (type) {
	case MAPISTORE_FOLDER:
		ret = mapistore_mgmt_registered_folder_subscription(self->mgmt_ctx, username, uri, 
								    NotificationFlags);
		return PyBool_FromLong((ret == MAPISTORE_SUCCESS) ? true : false);
		break;
	case MAPISTORE_MESSAGE:
		DEBUG(0, ("[%s:%d]: Unsupported subscription type\n", __FUNCTION__, __LINE__));
		return PyBool_FromLong(false);
		break;
	}

	return PyBool_FromLong(false);
}

static PyObject *py_MAPIStoreMGMT_send_newmail(PyMAPIStoreMGMTObject *self, PyObject *args)
{
	int		ret;
	const char	*username;
	const char	*storeuser;
	const char	*FolderURI;
	const char	*MessageURI;
	uint64_t	FolderID;
	uint64_t	MessageID;
	bool		softdeleted;
	PyMAPIStoreGlobals *globals;

	if (!PyArg_ParseTuple(args, "ssss", &username, &storeuser, &FolderURI, &MessageURI)) {
		return NULL;
	}

	/* Turn MessageURI into MessageID from indexing database */
	ret = mapistore_indexing_record_get_fmid(self->mgmt_ctx->mstore_ctx, storeuser, MessageURI, 
						 false, &MessageID, &softdeleted);
	if (ret != MAPISTORE_SUCCESS || softdeleted == true) {
		return PyBool_FromLong(false);
	}

	/* Turn FolderURI into FolderID from openchangedb or indexing database */
	globals = get_PyMAPIStoreGlobals();
	ret = openchangedb_get_fid(globals->ocdb_ctx, FolderURI, &FolderID);
	if (ret != MAPI_E_SUCCESS) {
		ret = mapistore_indexing_record_get_fmid(self->mgmt_ctx->mstore_ctx, username, FolderURI, false,
							 &FolderID, &softdeleted);
		if (ret != MAPISTORE_SUCCESS || softdeleted == true) {
			return PyBool_FromLong(false);
		}
	}

	/* Send notification on user queue */
	ret = mapistore_mgmt_send_newmail_notification(self->mgmt_ctx, username, FolderID, 
						       MessageID, MessageURI);

	return PyBool_FromLong((ret == MAPISTORE_SUCCESS) ? true : false);
}

static PyObject *obj_get_verbose(PyMAPIStoreMGMTObject *self, void *closure)
{
	return PyBool_FromLong(self->mgmt_ctx->verbose);
}

static int obj_set_verbose(PyMAPIStoreMGMTObject *self, PyObject *verbose, void *closure)
{
	if (!PyBool_Check(verbose))
		return -1;
	if (mapistore_mgmt_set_verbosity(self->mgmt_ctx, PyLong_AsLong(verbose)) != MAPISTORE_SUCCESS) return -1;
	return 0;
}

static PyMethodDef mapistore_mgmt_methods[] = {
	{ "registered_backend", (PyCFunction)py_MAPIStoreMGMT_registered_backend, METH_VARARGS },
	{ "registered_users", (PyCFunction)py_MAPIStoreMGMT_registered_users, METH_VARARGS },
	{ "registered_message", (PyCFunction)py_MAPIStoreMGMT_registered_message, METH_VARARGS },
	{ "register_message", (PyCFunction)py_MAPIStoreMGMT_register_message, METH_VARARGS },
	{ "registered_subscription", (PyCFunction)py_MAPIStoreMGMT_registered_subscription, METH_VARARGS },
	{ "existing_users", (PyCFunction)py_MAPIStoreMGMT_existing_users, METH_VARARGS },
	{ "send_newmail", (PyCFunction)py_MAPIStoreMGMT_send_newmail, METH_VARARGS },
	{ NULL },
};

static PyGetSetDef mapistore_mgmt_getsetters[] = {
	{ (char *)"verbose", (getter)obj_get_verbose, (setter)obj_set_verbose, 
	  "Enable/Disable verbosity for management object" },
	{ NULL }
};

PyTypeObject PyMAPIStoreMGMT = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapistore_mgmt",
	.tp_basicsize = sizeof (PyMAPIStoreMGMTObject),
	.tp_methods = mapistore_mgmt_methods,
	.tp_getset = mapistore_mgmt_getsetters,
	.tp_doc = "mapistore management object",
	.tp_dealloc = (destructor)py_MAPIStoreMGMT_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

void initmapistore_mgmt(PyObject *m)
{
	if (PyType_Ready(&PyMAPIStoreMGMT) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStoreMGMT);
}
