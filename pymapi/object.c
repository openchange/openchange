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

static PyObject *object_create(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	/* FIXME */
	return (PyObject *)PyObject_New(PyMapiObjectObject, type);
}

static void object_dealloc(PyObject *_self)
{
	PyMapiObjectObject *self = (PyMapiObjectObject *)_self;
	mapi_object_release(self->object);
	PyObject_Del(_self);
}

mapi_object_t *PyMapiObject_GetMapiObject(PyObject *obj)
{
	PyMapiObjectObject *self = (PyMapiObjectObject *)obj;
	if (!PyMapiObject_Check(obj))
		return NULL;

	return self->object;
}

static PyMethodDef object_methods[] = {
	{ NULL },
};

static PyObject *object_get_session(PyObject *_self, void *closure)
{
	PyMapiObjectObject *self = (PyMapiObjectObject *)_self;
	struct mapi_session *session;

	session = mapi_object_get_session(self->object);

	return PyMapiSession_FromMapiSession(session);
}

static PyObject *object_get_id(PyObject *_self, void *closure)
{
	PyMapiObjectObject *self = (PyMapiObjectObject *)_self;
	mapi_id_t id;

	id = mapi_object_get_id(self->object);

	return PyLong_FromLong(id);
}

static PyGetSetDef object_getsetters[] = {
	{ "session", object_get_session, NULL, "The MAPI session" },
	{ "id", object_get_id, NULL, "MAPI ID" },
	{ NULL }
};

PyTypeObject PyMapiObjectType = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "Object",
	.tp_basicsize = sizeof(PyMapiObjectObject),
	.tp_methods = object_methods,
	.tp_getset = object_getsetters,
	.tp_doc = "MAPI Object",
	.tp_new = object_create,
	.tp_dealloc = object_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

