/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2011
   Copyright (C) Brad Hards <bradh@openchange.org> 2010-2011

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

/**
   \file mapistore_common.h

   \brief MAPISTORE shared (common) API
   
   This header contains the shared declarations for functions that are 
   used across the MAPISTORE API.
 */

#ifndef	__MAPISTORE_COMMON_H
#define	__MAPISTORE_COMMON_H

#include <ldb.h>

struct mapistore_backend_context;

__BEGIN_DECLS

const char		*mapistore_get_mapping_path(void);
const char		*mapistore_get_firstorgdn(void);
const char		*mapistore_get_database_path(void);

/* definition from mapistore_backend_public.c */
struct ldb_context	*mapistore_public_ldb_connect(struct mapistore_backend_context *, const char *);
enum MAPISTORE_ERROR	mapistore_exist(struct mapistore_backend_context *, const char *, const char *);

__END_DECLS

#endif /* __MAPISTORE_COMMON_H */
