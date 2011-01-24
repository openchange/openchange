/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Note: init and load functions have been copied from
   samba4/source4/param/util.c initially written by Jelmer.

   Copyright (C) Jelmer Vernooij 2005-2007
   Copyright (C) Julien Kerihuel 2009-2011

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
   \file mapistore_backend_public.c

   \brief Provides a public API mapistore backends can use to interact
   with mapistore internals. Some functions are just wrappers over
   existing mapistore functions.
 */

#include "mapistore_errors.h"
#include "mapistore.h"
#include "mapistore_common.h"
#include "mapistore_private.h"
#include "mapistore_backend.h"


/**
   \details Open a wrapped context to a LDB database

   This function is a wrapper to mapistore_ldb_wrap_connect which
   helps keeping mapistore_backend_context opaque to backends.

   \return valid LDB context pointer on success, otherwise NULL
 */
struct ldb_context *mapistore_public_ldb_connect(struct mapistore_backend_context *ctx,
						 const char *path)
{
	struct mapistore_context	*mstore_ctx = (struct mapistore_context *)ctx;
	struct tevent_context		*ev;

	/* Sanity checks */
	if (!path || !mstore_ctx) {
		return NULL;
	}

	ev = tevent_context_init(mstore_ctx);
	if (!ev) return NULL;

	return mapistore_ldb_wrap_connect(mstore_ctx, ev, path, 0);
}
