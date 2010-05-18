/*
   OpenChange Storage Abstraction Layer library
   MAPIStore FS / OCPF backend

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

#include "mapistore_fsocpf.h"

/**
   \details Initialize fsocpf mapistore backend

   \return MAPISTORE_SUCCESS on success
 */
static int fsocpf_init(void)
{
	DEBUG(0, ("fsocpf backend initialized\n"));
	
	return MAPISTORE_SUCCESS;
}


/**
   \details Create a connection context to the fsocpf backend

   \param mem_ctx pointer to the memory context
   \param uri pointer to the fsocpf path
   \param private_data pointer to the private backend context 
 */
static int fsocpf_create_context(TALLOC_CTX *mem_ctx, const char *uri, void **private_data)
{
	struct fsocpf_context		*fsocpf_ctx;

	DEBUG(0, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	fsocpf_ctx = talloc_zero(mem_ctx, struct fsocpf_context);
	fsocpf_ctx->private_data = NULL;

	*private_data = (void *)fsocpf_ctx;

	return MAPISTORE_SUCCESS;
}


/**
   \details Delete a connection context from the fsocpf backend

   \param private_data pointer to the current fsocpf context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int fsocpf_delete_context(void *private_data)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	if (!fsocpf_ctx) {
		return MAPISTORE_SUCCESS;
	}

	return MAPISTORE_SUCCESS;
}

static int fsocpf_op_mkdir(void *private_data)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;

	if (!fsocpf_ctx) {
		return MAPISTORE_ERROR;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Entry point for mapistore FSOCPF backend

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
int mapistore_init_backend(void)
{
	struct mapistore_backend	backend;
	int				ret;

	/* Fill in our name */
	backend.name = "fsocpf";
	backend.description = "mapistore filesystem + ocpf backend";
	backend.namespace = "fsocpf://";

	/* Fill in all the operations */
	backend.init = fsocpf_init;
	backend.create_context = fsocpf_create_context;
	backend.delete_context = fsocpf_delete_context;
	backend.op_mkdir = fsocpf_op_mkdir;

	/* Register ourselves with the MAPISTORE subsystem */
	ret = mapistore_backend_register(&backend);
	if (ret != MAPISTORE_SUCCESS) {
		DEBUG(0, ("Failed to register the '%s' mapistore backend!\n", backend.name));
		return ret;
	}

	return MAPISTORE_SUCCESS;
}
