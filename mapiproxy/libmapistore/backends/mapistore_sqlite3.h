/*
   OpenChange Storage Abstraction Layer library
   MAPIStore SQLite backend

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009

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

#ifndef	__MAPISTORE_SQLITE3_H
#define	__MAPISTORE_SQLITE3_H

#include <mapiproxy/libmapistore/mapistore.h>
#include <mapiproxy/libmapistore/mapistore_errors.h>
#include <libmapi/dlinklist.h>
#include <sqlite3.h>

struct sqlite3_context {
	sqlite3		*db;
	void		*private_data;
};


__BEGIN_DECLS

int	mapistore_init_backend(void);

__END_DECLS

#endif	/* ! __MAPISTORE_SQLITE3_H */
