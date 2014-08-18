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

#define	NAMEDPROPS_BACKEND_LDB	"ldb"

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
enum mapistore_error mapistore_namedprops_ldb_init(TALLOC_CTX *, struct loadparm_context *, struct namedprops_context **);
__END_DECLS

#endif /* __NAMEDPROPS_LDB_H_ */
