/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2011

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
   \file mapistore_mgmt.c

   \brief Provides mapistore management routine for
   administrative/services tools. Using this interface virtually
   restrict mapistore features to the specific management functions
   subset.
 */

#include "mapistore.h"
#include "mapistore_errors.h"
#include "mapistore_private.h"

/**
   \details Initialize a mapistore manager context.

   \param mem_ctx pointer to the memory context
   \param path pointer to folder where mapistore backends are
   installed

   \return allocated mapistore_mgmt context on success, otherwise NULL
 */
_PUBLIC_ struct mapistore_mgmt_context *mapistore_mgmt_init(TALLOC_CTX *mem_ctx, const char *path)
{
	struct mapistore_mgmt_context	*mgmt_ctx;

	mgmt_ctx = talloc_zero(mem_ctx, struct mapistore_mgmt_context);
	if (!mgmt_ctx) {
		return NULL;
	}

	mgmt_ctx->mstore_ctx = mapistore_init(mem_ctx, NULL);
	if (!mgmt_ctx->mstore_ctx) return NULL;

	return mgmt_ctx;
}


/**
   \details Check if the specified backend is registered in mapistore

   \param mgmt_ctx pointer to the mapistore management context
   \param backend pointer to the backend name

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_mgmt_registered_backend(struct mapistore_mgmt_context *mgmt_ctx, const char *backend)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(backend == NULL, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(mgmt_ctx == NULL, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(mgmt_ctx->mstore_ctx == NULL, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	
	return mapistore_backend_registered(backend);
}
