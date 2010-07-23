/*
   OpenChange MAPI implementation.

   Python interface to openchange

   Copyright (C) Julien Kerihuel 2010.

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

#ifndef	__PYMAPI_H_
#define	__PYMAPI_H_

#include <Python.h>
#include "libmapi/libmapi.h"
#include <talloc.h>

typedef struct {
	PyObject_HEAD
	TALLOC_CTX		*mem_ctx;
	struct SPropValue	*SPropValue;
	uint32_t		cValues;
} PySPropValueObject;

PyAPI_DATA(PyTypeObject)	PySPropValue;

/* definitions from auto-generated pymapi_properties.c */
int pymapi_add_properties(PyObject *);

#endif /* ! __PYMAPI_H_ */
