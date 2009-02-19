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
#include <libmapi/dlinklist.h>

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
	struct backend_context_list	*el;
	int				retval;
	bool				found = false;

	/* Sanity checks */
	if (!mstore_ctx) return MAPISTORE_ERR_NOT_INITIALIZED;
	if (!mstore_ctx->processing_ctx) return MAPISTORE_ERR_NOT_INITIALIZED;
	if (!mstore_ctx->context_list) return MAPISTORE_ERR_NOT_INITIALIZED;

	/* Step 0. Ensure the context exists */
	for (el = mstore_ctx->context_list; el; el = el->next) {
		if (el->ctx && el->ctx->context_id == context_id) {
			found = true;
			break;
		}
	}
	if (found == false) return MAPISTORE_ERR_INVALID_PARAMETER;

	/* Step 1. Delete the context within backend */
	retval = mapistore_backend_delete_context(el->ctx);
	if (retval) return retval;

	/* Step 2. Delete the context from the processing layer */

	/* Step 2. Add the free'd context id to the free list */
	retval = mapistore_free_context_id(mstore_ctx->processing_ctx, context_id);
	return retval;
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
