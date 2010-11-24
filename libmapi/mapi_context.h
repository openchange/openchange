/*
   OpenChange MAPI implementation.
   MAPI Context.

   Copyright (C) Julien Kerihuel 2007-2010.
   Copyright (C) Fabien Le Mentec 2007.

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

#ifndef	_MAPI_CONTEXT_H
#define	_MAPI_CONTEXT_H

#include <talloc.h>

struct ldb_context;
struct mapi_session;

struct mapi_context
{
  TALLOC_CTX		*mem_ctx;
  struct ldb_context	*ldb_ctx;
  struct mapi_session	*session;
  bool			dumpdata;
  struct loadparm_context *lp_ctx;
};


#endif	/* ! _MAPI_CONTEXT_H */
