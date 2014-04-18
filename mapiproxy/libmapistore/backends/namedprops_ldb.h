/*
   MAPI Proxy - Named properties backend LDB implementation

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010-2013

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

#ifndef __NAMEDPROPS_LDB_H_
#define __NAMEDPROPS_LDB_H_

#include "namedprops_backend.h"

enum mapistore_error mapistore_namedprops_ldb_init(TALLOC_CTX *, const char *, struct namedprops_context **);

#endif /* __NAMEDPROPS_LDB_H_ */
