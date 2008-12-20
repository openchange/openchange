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

static PyObject *PyMapiObject_FromMapiObject(mapi_object_t *obj)
{
	return NULL; /* FIXME */
}	

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

static PyObject *py_folder_get_items_count(PyMapiObjectObject *self)
{
	enum MAPISTATUS status;
	uint32_t unread, total;

	status = GetFolderItemsCount(self->object, &unread, &total);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);

	return Py_BuildValue("(ii)", unread, total);
}

static PyObject *py_folder_remove_user_permissions(PyMapiObjectObject *self, PyObject *args)
{
	char *username;
	enum MAPISTATUS status;
	if (!PyArg_ParseTuple(args, "s", &username))
		return NULL;

	status = RemoveUserPermission(self->object, username);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);

	return Py_None;
}

static PyObject *py_folder_create(PyMapiObjectObject *self, PyObject *args)
{
	int foldertype;
	char *name, *comment;
	uint32_t flags;
	enum MAPISTATUS status;
	mapi_object_t child;

	if (!PyArg_ParseTuple(args, "issI", &foldertype, &name, &comment, &flags))
		return NULL;

	status = CreateFolder(self->object, foldertype, name, comment, flags, &child);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);

	return PyMapiObject_FromMapiObject(&child);
}

static PyObject *py_folder_delete(PyMapiObjectObject *self, PyObject *args)
{
	mapi_id_t folderid; 
	int flags;
	enum MAPISTATUS status;
	bool partial;
	if (!PyArg_ParseTuple(args, "ii", &folderid, &flags))
		return NULL;

	status = DeleteFolder(self->object, folderid, flags, &partial);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);

	return PyBool_FromLong(partial);
}

static PyObject *py_folder_empty(PyMapiObjectObject *self)
{
	enum MAPISTATUS status = EmptyFolder(self->object);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);
	return Py_None;
}

static PyMethodDef object_methods[] = {
	{ "get_folder_items_count", (PyCFunction)py_folder_get_items_count, METH_NOARGS, 
		"S.get_folder_items_count() -> (unread, total)" },
	{ "remove_user_permission", (PyCFunction)py_folder_remove_user_permissions, METH_VARARGS,
		"S.remove_user_permissions(user) -> None" },
	{ "create_folder", (PyCFunction)py_folder_create, METH_VARARGS,
		"S.create_folder(type, name, comment, flags) -> None" },
	{ "empty_folder", (PyCFunction)py_folder_empty, METH_NOARGS, 
		"S.empty_folder() -> None" },
	{ "delete_folder", (PyCFunction)py_folder_delete, METH_VARARGS,
		"S.delete_folder(folderid, flags) -> None" },
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

