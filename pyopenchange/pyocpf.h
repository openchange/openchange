/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Python interface to ocpf

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

#ifndef	_PYOCPF_H_
#define	_PYOCPF_H_

#include <Python.h>
#include <talloc.h>

typedef struct {
	PyObject_HEAD
	uint32_t	context_id;
	TALLOC_CTX	*mem_ctx;
} PyOcpfObject;

#endif /* !_PYOCPF_H_ */
