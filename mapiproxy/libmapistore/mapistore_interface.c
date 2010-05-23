/*
   OpenChange Storage Abstraction Layer library

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

#include "mapistore.h"
#include "mapistore_errors.h"
#include "mapistore_private.h"
#include <dlinklist.h>

#include <string.h>

/**
   \details Initialize the mapistore context

   \param mem_ctx pointer to the memory context

   \return allocate mapistore context on success, otherwise NULL
 */
_PUBLIC_ struct mapistore_context *mapistore_init(TALLOC_CTX *mem_ctx, const char *path)
{
	int				retval;
	struct mapistore_context	*mstore_ctx;

	mstore_ctx = talloc_zero(mem_ctx, struct mapistore_context);
	if (!mstore_ctx) {
		return NULL;
	}

	mstore_ctx->processing_ctx = talloc_zero(mstore_ctx, struct processing_context);

	retval = mapistore_init_mapping_context(mstore_ctx->processing_ctx);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(5, ("[%s:%d]: %s\n", __FUNCTION__, __LINE__, mapistore_errstr(retval)));
		talloc_free(mstore_ctx);
		return NULL;
	}

	retval = mapistore_backend_init(mstore_ctx, path);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(5, ("[%s:%d]: %s\n", __FUNCTION__, __LINE__, mapistore_errstr(retval)));
		talloc_free(mstore_ctx);
		return NULL;
	}

	mstore_ctx->context_list = NULL;

	return mstore_ctx;
}


/**
   \details Release the mapistore context and destroy any data
   associated

   \param mstore_ctx pointer to the mapistore context

   \note The function needs to rely on talloc destructors which is not
   implemented in code yet.

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_release(struct mapistore_context *mstore_ctx)
{
	if (!mstore_ctx) return MAPISTORE_ERR_NOT_INITIALIZED;

	talloc_free(mstore_ctx->processing_ctx);
	talloc_free(mstore_ctx->context_list);
	talloc_free(mstore_ctx);

	return MAPISTORE_SUCCESS;
}


/**
   \details Add a new connection context to mapistore

   \param mstore_ctx pointer to the mapistore context
   \param uri the connection context URI
   \param pointer to the context identifier the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_add_context(struct mapistore_context *mstore_ctx, 
				   const char *uri, uint32_t *context_id)
{
	TALLOC_CTX				*mem_ctx;
	int					retval;
	struct backend_context			*backend_ctx;
	struct backend_context_list    		*backend_list;
	char					*namespace;
	char					*namespace_start;
	char					*backend_uri;

	/* Step 1. Perform Sanity Checks on URI */
	if (!uri || strlen(uri) < 4) {
		return MAPISTORE_ERR_INVALID_NAMESPACE;
	}

	mem_ctx = talloc_named(NULL, 0, "mapistore_add_context");
	namespace = talloc_strdup(mem_ctx, uri);
	namespace_start = namespace;
	namespace = strchr(namespace, ':');
	if (!namespace) {
		DEBUG(0, ("[%s:%d]: Error - Invalid namespace '%s'\n", __FUNCTION__, __LINE__, namespace_start));
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_INVALID_NAMESPACE;
	}

	if (namespace[1] && namespace[1] == '/' &&
	    namespace[2] && namespace[2] == '/' &&
	    namespace[3]) {
		backend_uri = talloc_strdup(mem_ctx, &namespace[3]);
		namespace[3] = '\0';
		backend_ctx = mapistore_backend_create_context((TALLOC_CTX *)mstore_ctx, namespace_start, backend_uri);
		if (!backend_ctx) {
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}

		backend_list = talloc_zero((TALLOC_CTX *) mstore_ctx, struct backend_context_list);
		talloc_steal(backend_list, backend_ctx);
		backend_list->ctx = backend_ctx;
		retval = mapistore_get_context_id(mstore_ctx->processing_ctx, &backend_list->ctx->context_id);
		if (retval != MAPISTORE_SUCCESS) {
			talloc_free(mem_ctx);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		*context_id = backend_list->ctx->context_id;
		DLIST_ADD_END(mstore_ctx->context_list, backend_list, struct backend_context_list *);

	} else {
		DEBUG(0, ("[%s:%d]: Error - Invalid URI '%s'\n", __FUNCTION__, __LINE__, uri));
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_INVALID_NAMESPACE;
	}

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}



/**
   \details Delete an existing connection context from mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the context to
   delete

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_del_context(struct mapistore_context *mstore_ctx, 
				   uint32_t context_id)
{
	struct backend_context		*backend_ctx;
	int				retval;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 0. Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 1. Delete the context within backend */
	retval = mapistore_backend_delete_context(backend_ctx);
	if (retval) return retval;

	/* Step 2. Delete the context from the processing layer */

	/* Step 2. Add the free'd context id to the free list */
	retval = mapistore_free_context_id(mstore_ctx->processing_ctx, context_id);
	return retval;
}


void mapistore_set_errno(int status)
{
	errno = status;
}


/**
   \details return a string explaining what a mapistore error constant
   means.

   \param mapistore_err the mapistore error constant

   \return constant string
 */
_PUBLIC_ const char *mapistore_errstr(int mapistore_err)
{
	switch (mapistore_err) {
	case MAPISTORE_SUCCESS:
		return "Success";
	case MAPISTORE_ERROR:
		return "Non-specific error";
	case MAPISTORE_ERR_NO_MEMORY:
		return "No memory available";
	case MAPISTORE_ERR_ALREADY_INITIALIZED:
		return "Already initialized";
	case MAPISTORE_ERR_NOT_INITIALIZED:
		return "Not initialized";
	case MAPISTORE_ERR_NO_DIRECTORY:
		return "No such file or directory";
	case MAPISTORE_ERR_DATABASE_INIT:
		return "Database initialization failed";
	case MAPISTORE_ERR_DATABASE_OPS:
		return "database operation failed";
	case MAPISTORE_ERR_BACKEND_REGISTER:
		return "storage backend registration failed";
	case MAPISTORE_ERR_BACKEND_INIT:
		return "storage backend initialization failed";
	}

	return "Unknown error";
}


/**
   \details Open a directory in mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where the directory will be opened
   \param parent_fid the parent folder identifier
   \param fid pointer to the folder identifier of the opened folder

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ int mapistore_opendir(struct mapistore_context *mstore_ctx,
			       uint32_t context_id,
			       uint64_t parent_fid,
			       uint64_t *fid)
{
	struct backend_context		*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 0. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* mapistore_backend_opendir() */

	return MAPISTORE_SUCCESS;
}


/**
   \details Close a directory in mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where the directory has to be closed/released
   \param fid the folder identifier referencing the folder to close

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ int mapistore_closedir(struct mapistore_context *mstore_ctx,
				uint32_t context_id,
				uint64_t fid)
{
	struct backend_context		*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 0. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* mapistore_backend_closedir() */

	return MAPISTORE_SUCCESS;
}


/**
   \details Create a directory in mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where the directory will be created
   \param parent_fid the parent folder identifier
   \param new_fid the folder identifier for the new folder
   \param properties pointer to MAPI data structures with properties
   applied to the new folder

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ int mapistore_mkdir(struct mapistore_context *mstore_ctx,
			     uint32_t context_id,
			     uint64_t parent_fid,
			     uint64_t new_fid,
			     struct mapi_SPropValue *properties)
{
	struct backend_context		*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 0. Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);	

	return MAPISTORE_SUCCESS;
}


/**
   \details Remove a directory in mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   \param parent_fid the parent folder identifier
   \param fid the folder identifier representing the folder to delete
   \param flags Flags specifying the rmdir operation behavior
   (recursive for folders, messages)

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ int mapistore_rmdir(struct mapistore_context *mstore_ctx,
			     uint32_t context_id,
			     uint64_t parent_fid,
			     uint64_t fid,
			     uint8_t flags)
{
	struct backend_context		*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 0. Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);	

	return MAPISTORE_SUCCESS;	
}
