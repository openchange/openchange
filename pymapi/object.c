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

static PyObject *py_folder_create_message(PyMapiObjectObject *self)
{
	mapi_object_t msg;
	enum MAPISTATUS status = CreateMessage(self->object, &msg);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);
	return PyMapiObject_FromMapiObject(&msg);
}

static PyObject *py_folder_delete_messages(PyMapiObjectObject *self, PyObject *args)
{
	PyObject *py_ids;
	uint32_t cn_messages;
	mapi_id_t *ids;
	enum MAPISTATUS status;
	int i;

	if (!PyArg_ParseTuple(args, "O", &py_ids))
		return NULL;

	if (!PySequence_Check(py_ids)) {
		PyErr_SetString(PyExc_TypeError, "ids should be a list of ids");
		return NULL;
	}

	cn_messages = PySequence_Size(py_ids);
	ids = talloc_array(NULL, mapi_id_t, cn_messages);
	if (ids == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	for (i = 0; i < cn_messages; i++) {
		PyObject *item;
		item = PySequence_GetItem(py_ids, i);
		ids[i] = PyInt_AsLong(item);
	}

	status = DeleteMessage(self->object, ids, cn_messages);
	talloc_free(ids);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);

	return Py_None;
}

static PyObject *py_folder_set_read_flags(PyMapiObjectObject *self, PyObject *args)
{
	int flags;
	PyObject *py_ids;
	enum MAPISTATUS status;
	uint16_t cn_ids;
    uint64_t *ids;
	int i;
	if (!PyArg_ParseTuple(args, "iO", &flags, &py_ids))
		return NULL;

	if (!PySequence_Check(py_ids)) {
		PyErr_SetString(PyExc_TypeError, "ids should be a list of ids");
		return NULL;
	}

	cn_ids = PySequence_Size(py_ids);
	ids = talloc_array(NULL, uint64_t, cn_ids);
	if (ids == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	for (i = 0; i < cn_ids; i++) {
		PyObject *item;
		item = PySequence_GetItem(py_ids, i);
		ids[i] = PyInt_AsLong(item);
	}

	status = SetReadFlags(self->object, flags, cn_ids, ids);
	talloc_free(ids);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);

	return Py_None;
}

static PyObject *py_folder_get_message_status(PyMapiObjectObject *self, PyObject *args)
{
	mapi_id_t msgid;
	uint32_t lstatus;
	enum MAPISTATUS status;
	if (!PyArg_ParseTuple(args, "i", &msgid))
		return NULL;
	
	status = GetMessageStatus(self->object, msgid, &lstatus);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);

	return PyInt_FromLong(lstatus);
}

static PyObject *py_message_get_best_body(PyMapiObjectObject *self)
{
	enum MAPISTATUS status;
	uint8_t format;
	
	status = GetBestBody(self->object, &format);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);

	return PyInt_FromLong(status);
}

static PyObject *py_get_default_folder(PyMapiObjectObject *self, PyObject *args)
{
	enum MAPISTATUS status;
	uint32_t type;
	uint64_t folder;

	if (!PyArg_ParseTuple(args, "i", &type))
		return NULL;

	status = GetDefaultFolder(self->object, &folder, type);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);

	return PyLong_FromLong(folder);
}

static PyObject *py_get_default_public_folder(PyMapiObjectObject *self, PyObject *args)
{
	enum MAPISTATUS status;
	uint32_t type;
	uint64_t folder;

	if (!PyArg_ParseTuple(args, "i", &type))
		return NULL;

	status = GetDefaultPublicFolder(self->object, &folder, type);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);

	return PyLong_FromLong(folder);
}

static PyObject *py_folder_add_user_permission(PyMapiObjectObject *self, PyObject *args)
{
	char *name;
	int rights;
	enum MAPISTATUS status;
	if (!PyArg_ParseTuple(args, "si", &name, &rights))
		return NULL;

	status = AddUserPermission(self->object, name, rights);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);

	return Py_None;
}

static PyObject *py_folder_modify_user_permission(PyMapiObjectObject *self, PyObject *args)
{
	char *name;
	int rights;
	enum MAPISTATUS status;
	if (!PyArg_ParseTuple(args, "si", &name, &rights))
		return NULL;

	status = ModifyUserPermission(self->object, name, rights);
	PyErr_MAPISTATUS_IS_ERR_RAISE(status);

	return Py_None;
}

static PyMethodDef object_methods[] = {
	{ "get_folder_items_count", (PyCFunction)py_folder_get_items_count, METH_NOARGS, 
		"S.get_folder_items_count() -> (unread, total)" },
	{ "remove_user_permission", (PyCFunction)py_folder_remove_user_permissions, METH_VARARGS,
		"S.remove_user_permissions(user) -> None" },
	{ "add_user_permission", (PyCFunction)py_folder_add_user_permission, METH_VARARGS,
		"S.add_user_permission(user, perm) -> None" },
	{ "modify_user_permission", (PyCFunction)py_folder_modify_user_permission, METH_VARARGS,
		"S.modify_user_permission(user, perm) -> None" },
	{ "create_folder", (PyCFunction)py_folder_create, METH_VARARGS,
		"S.create_folder(type, name, comment, flags) -> None" },
	{ "empty_folder", (PyCFunction)py_folder_empty, METH_NOARGS, 
		"S.empty_folder() -> None" },
	{ "delete_folder", (PyCFunction)py_folder_delete, METH_VARARGS,
		"S.delete_folder(folderid, flags) -> None" },
	{ "create_message", (PyCFunction)py_folder_create_message, METH_NOARGS,
		"S.create_message() -> message" },
	{ "delete_messages", (PyCFunction)py_folder_delete_messages, METH_VARARGS,
		"S.delete_messages([ids]) -> None" },
	{ "get_message_status", (PyCFunction)py_folder_get_message_status, METH_VARARGS,
		"S.get_message_status(id) -> status" },
	{ "set_read_flags", (PyCFunction)py_folder_set_read_flags, METH_VARARGS,
		"S.set_read_flags(flags, [ids]) -> None" },
	{ "get_best_body", (PyCFunction)py_message_get_best_body, METH_NOARGS,
		"S.get_best_body() -> format" },
	{ "get_default_folder", (PyCFunction)py_get_default_folder, METH_VARARGS,
		"S.get_default_folder(type) -> id" },
	{ "get_default_public_folder", (PyCFunction)py_get_default_public_folder, METH_VARARGS,
		"S.get_default_public_folder(type) -> id" },
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

