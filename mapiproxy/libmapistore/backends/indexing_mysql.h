/*
   MAPI Proxy - Indexing backend MySQL implementation

   OpenChange Project

   Copyright (C) Carlos Pérez-Aradros Herce 2014
   Copyright (C) Jesús García Sáez 2014

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

#ifndef __INDEXING_MYSQL_H__
#define __INDEXING_MYSQL_H__

#define INDEXING_SCHEMA_FILE	"indexing_schema.sql"
#define INDEXING_TABLE		"mapistore_indexing"
#define INDEXING_ALLOC_TABLE	"mapistore_indexes"


enum mapistore_error mapistore_indexing_mysql_init(TALLOC_CTX *,
						   const char *, const char *,
						   struct indexing_context **);


#endif /* __INDEXING_MYSQL_H__ */
