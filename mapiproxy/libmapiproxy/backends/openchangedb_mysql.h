/*
   MAPI Proxy - OpenchangeDB backend LDB implementation

   OpenChange Project

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

#ifndef __OPENCHANGEDB_MYSQL_H__
#define __OPENCHANGEDB_MYSQL_H__

#include "openchangedb_backends.h"

#define MAX_PUBLIC_FOLDER_ID 1000

enum MAPISTATUS openchangedb_mysql_initialize(TALLOC_CTX *, struct loadparm_context *, struct openchangedb_context **);

#endif /* __OPENCHANGEDB_MYSQL_H__ */
