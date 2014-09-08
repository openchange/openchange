/*
   MAPI Proxy - Indexing backend TDB implementation

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010

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

#ifndef __INDEXING_TDB_H__
#define __INDEXING_TDB_H__

#include <tdb.h>

#define	MAPISTORE_DB_INDEXING		"indexing.tdb"
#define	MAPISTORE_SOFT_DELETED_TAG	"SOFT_DELETED:"


enum mapistore_error mapistore_indexing_tdb_init(TALLOC_CTX *,
						 const char *,
						 struct indexing_context **);


#endif /* __INDEXING_TDB_H__ */
