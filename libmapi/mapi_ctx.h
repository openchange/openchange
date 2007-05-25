/*
 *  OpenChange MAPI implementation.
 *  Status codes returned my MAPI
 *
 *  Copyright (C) Julien Kerihuel 2007.
 *  Copyright (C) Fabien Le Mentec 2007.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef	_MAPI_CTX_H
#define	_MAPI_CTX_H

#include <talloc.h>

/**
 * mapi global context
 */

struct ldb_context;
struct mapi_session;

typedef struct mapi_ctx
{
  TALLOC_CTX		*mem_ctx;
  struct ldb_context	*ldb_ctx;
  struct mapi_session	*session;
  BOOL			dumpdata;
} mapi_ctx_t;


#endif	/* ! _MAPI_CTX_H */
