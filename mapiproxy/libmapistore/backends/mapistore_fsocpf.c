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

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>

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
	DIR				*top_dir;
	struct fsocpf_context		*fsocpf_ctx;

	DEBUG(0, ("[%s:%d]\n", __FUNCTION__, __LINE__));
	DEBUG(4, ("[%s:%d]: fsocpf uri: %s\n", __FUNCTION__, __LINE__, uri));

	/* Step 1. Try to open context directory */
	top_dir = opendir(uri);
	if (!top_dir) {
		/* If it doesn't exist, try to create it */
		if (mkdir(uri, S_IRWXU) != 0 ) {
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		top_dir = opendir(uri);
		if (!top_dir) {
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
	}

	/* Step 2. Allocate / Initialize the fsocpf context structure */
	fsocpf_ctx = talloc_zero(mem_ctx, struct fsocpf_context);
	fsocpf_ctx->private_data = NULL;
	fsocpf_ctx->uri = talloc_strdup(fsocpf_ctx, uri);
	fsocpf_ctx->dir = top_dir;
	fsocpf_ctx->folders = talloc_zero(fsocpf_ctx, struct folder_list_context);

	/* Step 3. Store fsocpf context within the opaque private_data pointer */
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

	closedir(fsocpf_ctx->dir);
	talloc_free(fsocpf_ctx);

	return MAPISTORE_SUCCESS;
}


/**
   \details Create a folder in the fsocpf backend
   
   \param private_data pointer to the current fsocpf context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int fsocpf_op_mkdir(void *private_data)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;

	if (!fsocpf_ctx) {
		return MAPISTORE_ERROR;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Delete a folder from the fsocpf backend

   \param private_data pointer to the current fsocpf context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int fsocpf_op_rmdir(void *private_data)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;

	if (!fsocpf_ctx) {
		return MAPISTORE_ERROR;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Open a folder from the fsocpf backend

   \param private_data pointer to the current fsocpf context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int fsocpf_op_opendir(void *private_data)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;

	if (!fsocpf_ctx) {
		return MAPISTORE_ERROR;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Close a folder from the fsocpf backend

   \param private_data pointer to the current fsocpf context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int fsocpf_op_closedir(void *private_data)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;

	if (!fsocpf_ctx) {
		return MAPISTORE_ERROR;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Read directory content from the fsocpf backend

   \param private_data pointer to the current fsocpf context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int fsocpf_op_readdir(void *private_data)
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
	backend.op_rmdir = fsocpf_op_rmdir;
	backend.op_opendir = fsocpf_op_opendir;
	backend.op_closedir = fsocpf_op_closedir;
	backend.op_readdir = fsocpf_op_readdir;

	/* Register ourselves with the MAPISTORE subsystem */
	ret = mapistore_backend_register(&backend);
	if (ret != MAPISTORE_SUCCESS) {
		DEBUG(0, ("Failed to register the '%s' mapistore backend!\n", backend.name));
		return ret;
	}

	return MAPISTORE_SUCCESS;
}
