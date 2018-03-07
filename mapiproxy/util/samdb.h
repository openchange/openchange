/*
   SamDB util functions

   OpenChange Project

   Copyright (C) Carlos Pérez-Aradros Herce 2015

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

#ifndef __OC_SAMDB_H__
#define __OC_SAMDB_H__

#ifdef PRINTF_ATTRIBUTE
#undef PRINTF_ATTRIBUTE
#endif
#define PRINTF_ATTRIBUTE(a1, a2)

#include <tevent.h>
#include <talloc.h>
#include <param.h>
#include <ldb.h>

#include "libmapi/libmapi.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"

struct ldb_context *samdb_init(TALLOC_CTX *mem_ctx);
struct ldb_context *samdb_reconnect(struct ldb_context *samdb_ctx);


int safe_ldb_wait(struct ldb_context **ldb_ptr, struct ldb_request *req, enum ldb_wait_type type);


#undef PRINTF_ATTRIBUTE
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (__printf__, a1, a2)))

#endif /* __OC_SAMDB_H__ */
