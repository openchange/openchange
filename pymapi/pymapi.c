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

#include <libmapi/libmapi.h>
#include <Python.h>

void initmapi(void);

static PyMethodDef mapi_methods[] = {
	{ NULL }
};

void initmapi(void)
{
	PyObject *m;

	m = Py_InitModule3("mapi", mapi_methods, "MAPI/RPC Python bindings");
	if (m == NULL)
		return;
}
