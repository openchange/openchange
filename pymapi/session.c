/*
   OpenChange MAPI implementation.

   Copyright (C) Jelmer Vernooij <jelmer@openchange.org> 2008.

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

#include "pymapi/pymapi.h"

typedef struct {
	PyObject_HEAD	
	struct mapi_session *session;
} PyMapiSessionObject;

static PyObject *py_session_create(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	enum MAPISTATUS retval;
	char *kwnames[] = { "profname", "password", "provider" };
	char *profname, *password;
	uint32_t provider = 0;
	struct mapi_session *session;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ss|i", kwnames, 
										&profname, &password, &provider))
		return NULL;

	retval = MapiLogonProvider(&session, profname, password, provider);
	if (!retval) {
		PyErr_SetMAPISTATUS(retval);
		return NULL;
	}
	return PyMapiSession_FromMapiSession(session);
}

PyObject *PyMapiSession_FromMapiSession(struct mapi_session *session)
{
	PyMapiSessionObject *ret;
	ret = PyObject_New(PyMapiSessionObject, &PyMapiSessionType);
	ret->session = session;
	return (PyObject *)ret;
}

static PyObject *py_session_login(PyObject *args, PyObject *kwargs)
{
	/* FIXME */
	return NULL;
}

static PyObject *py_open_msg_store(PyObject *self, PyObject *args)
{
	PyObject *py_mapi_object;
	PyMapiSessionObject *self_session = (PyMapiSessionObject *)self;
	mapi_object_t *obj;
	enum MAPISTATUS retval;

	if (!PyArg_ParseTuple(args, "O", &py_mapi_object))
		return NULL;

	obj = PyMapiObject_GetMapiObject(py_mapi_object);
	if (obj == NULL) {
		PyErr_SetString(PyExc_TypeError, "Expected MAPI Object");
		return NULL;
	}

	retval = OpenMsgStore(self_session->session, obj);
	if (!retval) {
		PyErr_SetMAPISTATUS(retval);
		return NULL;
	}

	return Py_None;
}

static PyObject *py_open_user_mailbox(PyObject *self, PyObject *args)
{
	PyObject *py_mapi_object;
	PyMapiSessionObject *self_session = (PyMapiSessionObject *)self;
	mapi_object_t *obj;
	enum MAPISTATUS retval;
	char *username;

	if (!PyArg_ParseTuple(args, "sO", &username, &py_mapi_object))
		return NULL;

	obj = PyMapiObject_GetMapiObject(py_mapi_object);
	if (obj == NULL) {
		PyErr_SetString(PyExc_TypeError, "Expected MAPI Object");
		return NULL;
	}

	retval = OpenUserMailbox(self_session->session, username, obj);
	if (!retval) {
		PyErr_SetMAPISTATUS(retval);
		return NULL;
	}

	return Py_None;
}

static PyObject *py_open_public_folder(PyObject *self, PyObject *args)
{
	PyObject *py_mapi_object;
	PyMapiSessionObject *self_session = (PyMapiSessionObject *)self;
	mapi_object_t *obj;
	enum MAPISTATUS retval;

	if (!PyArg_ParseTuple(args, "O", &py_mapi_object))
		return NULL;

	obj = PyMapiObject_GetMapiObject(py_mapi_object);
	if (obj == NULL) {
		PyErr_SetString(PyExc_TypeError, "Expected MAPI Object");
		return NULL;
	}

	retval = OpenPublicFolder(self_session->session, obj);
	if (!retval) {
		PyErr_SetMAPISTATUS(retval);
		return NULL;
	}

	return Py_None;
}

static PyObject *py_unsubscribe(PyMapiSessionObject *self, PyObject *args)
{
	uint32_t ulConnection;
	enum MAPISTATUS status;
	
	if (!PyArg_ParseTuple(args, "i", &ulConnection))
		return NULL;

	status = Unsubscribe(self->session, ulConnection);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);

	return Py_None;
}

static PyMethodDef session_methods[] = {
	{ "login", (PyCFunction) py_session_login, METH_VARARGS|METH_KEYWORDS },
	{ "open_msg_store", (PyCFunction) py_open_msg_store, METH_VARARGS,
		"S.open_msg_store(object)\n\n"
		"This function opens the main message store. This allows access to "
		"the normal user folders."
	},
	{ "open_user_mailbox", (PyCFunction) py_open_user_mailbox, METH_VARARGS,
		"S.open_user_mailbox(username, object)\n\n"
		"Open another users mailbox."
	},
	{ "open_public_folder", (PyCFunction) py_open_public_folder, METH_VARARGS,
		"S.open_public_folder(object)\n\n"
		"This function opens the public folder store. This allows access to "
		"the public folders."
	},
	{ "unsubscribe", (PyCFunction) py_unsubscribe, METH_VARARGS,
		"S.Unsubscribe(int) -> None"
	},
	{ NULL },
};

static PyGetSetDef session_getsetters[] = {
	{ "default_profile_path", NULL, NULL },
	{ "profile_name", NULL, NULL },
	{ "message_store", NULL, NULL },
	{ NULL }
};

PyTypeObject PyMapiSessionType = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "Session",
	.tp_basicsize = sizeof(PyMapiSessionObject),
	.tp_methods = session_methods,
	.tp_getset = session_getsetters,
	.tp_doc = "MAPI Session",
	.tp_new = py_session_create,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

