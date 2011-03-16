/*
   OpenChange MAPI implementation.

   Python interface to mapistore

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

#ifndef	__PYMAPISTORE_H_
#define	__PYMAPISTORE_H_

#include <Python.h>
#include "mapistore_errors.h"
#include "mapistore.h"
#include "mapistore_private.h"
#include "mapistore_common.h"

typedef struct {
	PyObject_HEAD
	TALLOC_CTX			*mem_ctx;
	struct mapistore_context	*mstore_ctx;
} PyMAPIStoreObject;

typedef struct {
	PyObject_HEAD
	TALLOC_CTX			*mem_ctx;
	struct mapistore_context	*mstore_ctx;
	PyObject			*parent_object;
	uint32_t			context_id;
	uint64_t			fid;
} PyMAPIStoreFolderObject;

typedef struct {
	PyObject_HEAD
	TALLOC_CTX			*mem_ctx;
	struct mapistoredb_context	*mdb_ctx;
} PyMAPIStoreDBObject;

PyAPI_DATA(PyTypeObject)	PyMAPIStore;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreFolder;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreDB;

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS		extern "C" {
#define __END_DECLS		}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

__BEGIN_DECLS

/* These definitions come from pymapistore_folder.c */
PyObject *pymapistore_folder_new(TALLOC_CTX *, struct mapistore_context *, PyObject *, uint32_t, uint64_t);

__END_DECLS

#endif	/* ! __PYMAPISTORE_H_ */
