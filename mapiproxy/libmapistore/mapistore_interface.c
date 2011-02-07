/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

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
   \file mapistore_interface.c

   \brief MAPISTORE public user interface
   
   This file contains general functions, primarily for
   users of the store (rather than storage providers).
 */

#define __STDC_FORMAT_MACROS	1
#include <inttypes.h>

#include "mapistore_errors.h"
#include "mapistore.h"
#include "mapistore_private.h"
#include "mapistore_backend.h"
#include "mapistore_common.h"
#include <dlinklist.h>
#include "libmapi/libmapi_private.h"

#include <string.h>

/**
   \details Initialize the mapistore context

   \param mem_ctx pointer to the memory context
   \param path the path to the location to load the backend providers from (NULL for default)

   \return allocate mapistore context on success, otherwise NULL
 */
_PUBLIC_ struct mapistore_context *mapistore_init(TALLOC_CTX *mem_ctx, const char *path)
{
	enum MAPISTORE_ERROR		retval;
	struct mapistore_context	*mstore_ctx;

	mstore_ctx = talloc_zero(mem_ctx, struct mapistore_context);
	if (!mstore_ctx) {
		return NULL;
	}

	mstore_ctx->processing_ctx = talloc_zero(mstore_ctx, struct processing_context);

	retval = mapistore_init_mapping_context(mstore_ctx->processing_ctx);
	if (retval != MAPISTORE_SUCCESS) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "mapistore mapping context init failed: %s\n",
				   mapistore_errstr(retval));
		talloc_free(mstore_ctx);
		return NULL;
	}

	retval = mapistore_backend_init(mstore_ctx, path);
	if (retval != MAPISTORE_SUCCESS) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "mapistore backend init failed: %s\n", mapistore_errstr(retval));
		talloc_free(mstore_ctx);
		return NULL;
	}

	mstore_ctx->context_list = NULL;

	/* MAPISTORE_v1 */
	mstore_ctx->indexing_list = talloc_zero(mstore_ctx, struct indexing_context_list);
	/* !MAPISTORE_v1 */

	/* MAPISTORE_v2 */
	mstore_ctx->mapistore_indexing_list = talloc_zero(mstore_ctx, struct mapistore_indexing_context_list);

	mstore_ctx->mapistore_nprops_ctx = NULL;
	retval = mapistore_namedprops_init(mstore_ctx, &(mstore_ctx->mapistore_nprops_ctx));
	if (retval != MAPISTORE_SUCCESS) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, 
				   "mapistore named properties database init failed: %s\n", 
				   mapistore_errstr(retval));
		talloc_free(mstore_ctx);
		return NULL;
	}
	/* MAPISTORE_v2 */

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
_PUBLIC_ enum MAPISTORE_ERROR mapistore_release(struct mapistore_context *mstore_ctx)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	talloc_free(mstore_ctx->mapistore_nprops_ctx);
	talloc_free(mstore_ctx->processing_ctx);
	talloc_free(mstore_ctx->context_list);
	talloc_free(mstore_ctx);

	return MAPISTORE_SUCCESS;
}


/**
   \details Add a new connection context to mapistore

   \param mstore_ctx pointer to the mapistore context
   \param login_user the username used for authentication
   \param username the username we want to impersonate
   \param uri the connection context URI
   \param context_id pointer to the context identifier the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_add_context(struct mapistore_context *mstore_ctx,
						    const char *login_user, const char *username,
						    const char *uri, uint32_t *context_id)
{
	TALLOC_CTX				*mem_ctx;
	int					retval;
	struct backend_context			*backend_ctx;
	struct backend_context_list    		*backend_list;
	char					*uri_namespace;
	char					*namespace_start;
	char					*backend_uri;

	/* Step 1. Perform Sanity Checks on URI */
	MAPISTORE_RETVAL_IF(!uri || strlen(uri) < 4, MAPISTORE_ERR_INVALID_NAMESPACE, NULL);

	mem_ctx = talloc_named(NULL, 0, "mapistore_add_context");
	uri_namespace = talloc_strdup(mem_ctx, uri);
	namespace_start = uri_namespace;
	uri_namespace= strchr(uri_namespace, ':');
	if (!uri_namespace) {
		DEBUG(0, ("[%s:%d]: Error - Invalid namespace '%s'\n", __FUNCTION__, __LINE__, namespace_start));
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_INVALID_NAMESPACE;
	}

	if (uri_namespace[1] && uri_namespace[1] == '/' &&
	    uri_namespace[2] && uri_namespace[2] == '/' &&
	    uri_namespace[3]) {
		backend_uri = talloc_strdup(mem_ctx, &uri_namespace[3]);
		uri_namespace[3] = '\0';
		backend_ctx = mapistore_backend_create_context((TALLOC_CTX *)mstore_ctx, login_user, username, namespace_start, backend_uri);
		if (!backend_ctx) {
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		backend_ctx->username = talloc_strdup((TALLOC_CTX *)backend_ctx, username);

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
   \details Increase the reference counter of an existing context

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the context to
   update

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_add_context_ref_count(struct mapistore_context *mstore_ctx,
							      uint32_t context_id)
{
	struct backend_context		*backend_ctx;
	enum MAPISTORE_ERROR		retval;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* TODO: Fix context_id sign */
	MAPISTORE_RETVAL_IF((int)context_id == -1, MAPISTORE_ERR_INVALID_CONTEXT, NULL);

	/* Step 0. Ensure the context exists */
	DEBUG(0, ("mapistore_add_context_ref_count: context_is to increment is %d\n", context_id));
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 1. Increment the ref count */
	retval = mapistore_backend_add_ref_count(backend_ctx);

	return retval;
}


/**
   \details Search for an existing context given its uri

   \param mstore_ctx pointer to the mapistore context
   \param uri the URI to lookup
   \param context_id pointer to the context identifier to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_search_context_by_uri(struct mapistore_context *mstore_ctx,
							      const char *uri,
							      uint32_t *context_id)
{
	struct backend_context		*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	backend_ctx = mapistore_backend_lookup_by_uri(mstore_ctx->context_list, uri);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_NOT_FOUND, NULL);

	*context_id = backend_ctx->context_id;
	return MAPISTORE_SUCCESS;
}


/**
   \details Delete an existing connection context from mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the context to
   delete

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_del_context(struct mapistore_context *mstore_ctx, 
						    uint32_t context_id)
{
	struct backend_context_list	*backend_list;
	struct backend_context		*backend_ctx;
	enum MAPISTORE_ERROR		retval;
	bool				found = false;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* TODO: Fix context_id sign */
	MAPISTORE_RETVAL_IF((int)context_id == -1, MAPISTORE_ERR_INVALID_CONTEXT, NULL);

	/* Step 0. Ensure the context exists */
	DEBUG(0, ("mapistore_del_context: context_id to del is %d\n", context_id));
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* search the backend_list item */
	for (backend_list = mstore_ctx->context_list; backend_list; backend_list = backend_list->next) {
		if (backend_list->ctx->context_id == context_id) {
			found = true;
			break;
		}		
	}
	if (found == false) {
		return MAPISTORE_ERROR;
	}

	/* Step 1. Delete the context within backend */
	retval = mapistore_backend_delete_context(backend_ctx);
	switch (retval) {
	case MAPISTORE_ERR_REF_COUNT:
		return MAPISTORE_SUCCESS;
	case MAPISTORE_SUCCESS:
		DLIST_REMOVE(mstore_ctx->context_list, backend_list);
		/* Step 2. Add the free'd context id to the free list */
		retval = mapistore_free_context_id(mstore_ctx->processing_ctx, context_id);
		break;
	default:
		return retval;
	}

	return retval;
}


/**
   \details Create a root default/system folder within the mailbox and
   return the folder identifier

   This operation is only meant to be called at mailbox provisioning
   time.

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   \param parent_index the parent default system/special folder index
   \param index the default system/special folder index
   \param folder_name the folder name to set

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_create_root_folder(struct mapistore_context *mstore_ctx,
							   uint32_t context_id,
							   enum MAPISTORE_DFLT_FOLDERS parent_index,
							   enum MAPISTORE_DFLT_FOLDERS index,
							   const char *folder_name)
{
	enum MAPISTORE_ERROR		retval;
	struct backend_context		*backend_ctx;
	char				*mapistore_uri;
	char				*parent_uri;
	uint64_t			fid;
	uint64_t			pfid;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Ensure the parent folder exists and retrieve its FID */
	retval = mapistore_create_uri(mstore_ctx, parent_index, backend_ctx->backend->uri_namespace,
				      backend_ctx->username, &parent_uri);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	retval = mapistore_indexing_context_add(mstore_ctx, backend_ctx->username, &(mstore_ctx->mapistore_indexing_list));
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	retval = mapistore_indexing_record_search_uri(mstore_ctx->mapistore_indexing_list, parent_uri);
	if (retval != MAPISTORE_ERR_EXIST) {
		talloc_free(parent_uri);
		goto error;
	}

	retval = mapistore_indexing_get_record_fmid_by_uri(mstore_ctx->mapistore_indexing_list, parent_uri, &pfid);
	talloc_free(parent_uri);
	if (retval) {
		goto error;
	}

	/* Step 3. Generate the URI */
	retval = mapistore_create_uri(mstore_ctx, index, backend_ctx->backend->uri_namespace,
				      backend_ctx->username, &mapistore_uri);
	if (retval) goto error;

	/* Step 4. Call backend root_mkdir operation */
	retval =  mapistore_backend_root_mkdir(backend_ctx, index, mapistore_uri, folder_name);
	if (retval) {
		talloc_free(mapistore_uri);
		goto error;
	}

	/* Step 5. Get a new FID for the folder */
	retval = mapistore_get_new_fmid(mstore_ctx->processing_ctx, backend_ctx->username, &fid);
	if (retval) {
		talloc_free(mapistore_uri);
		goto error;
	}
	
	/* Step 6. Register the folder within the indexing database */
	retval = mapistore_indexing_add_fmid_record(mstore_ctx->mapistore_indexing_list, fid, 
						    mapistore_uri, pfid, MAPISTORE_INDEXING_FOLDER);
	talloc_free(mapistore_uri);
	if (retval) goto error;

error:
	mapistore_indexing_context_del(mstore_ctx, backend_ctx->username);

	/* Step 7. Very unlikely to happen ... but still delete the folder */
	/* if (retval) { */

	/* } */

	return retval;
}

_PUBLIC_ enum MAPISTORE_ERROR mapistore_set_mapistore_uri(struct mapistore_context *mstore_ctx,
							  uint32_t context_id,
							  enum MAPISTORE_DFLT_FOLDERS index,
							  const char *mapistore_uri)
{
	enum MAPISTORE_ERROR		retval;
	struct backend_context		*backend_ctx;
	char				*old_uri;
	uint64_t			fmid;
	
	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);
	MAPISTORE_RETVAL_IF(!mapistore_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	
	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	
	/* Step 2. Retrieve the URI associated to this system/default folder */
	retval = mapistore_create_uri(mstore_ctx, index, backend_ctx->backend->uri_namespace,
				      backend_ctx->username, &old_uri);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);
	
	/* Step 3. Create the mapistore_indexing context */
	retval = mapistore_indexing_context_add(mstore_ctx, backend_ctx->username, &(mstore_ctx->mapistore_indexing_list));
	MAPISTORE_RETVAL_IF(retval, retval, NULL);
	
	/* Step 4. Retrieve its FMID from the indexing database */
	retval = mapistore_indexing_get_record_fmid_by_uri(mstore_ctx->mapistore_indexing_list, old_uri, &fmid);
	if (retval) goto end;
	
	/* Step 5. Update the URI within the indexing database */
	retval = mapistore_indexing_update_mapistore_uri(mstore_ctx->mapistore_indexing_list, fmid, mapistore_uri);
	if (retval) goto end;
	
end:
	/* Step 6. Delete the indexing context */
	retval = mapistore_indexing_context_del(mstore_ctx, backend_ctx->username);
	
	return retval;
}

/**
   \details Retrieve the next backend available in the list

   \param backend_name pointer to the backend name to return
   \param backend_namespace pointer to the backend namespace to return
   \param backend_description pointer to the backend description to
   return
   \param backend_index pointer to the backend index in the the
   backend's list to return

   \note backend_index must be initialized to 0 prior any calls to
   mapistore_get_next_backend.

   \return MAPISTORE_SUCCESS on success, MAPISTORE_ERR_NOT_FOUND if
   the end of the list is reached, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_get_next_backend(const char **backend_name,
						const char **backend_namespace,
						const char **backend_description,
						uint32_t *backend_index)
{
	enum MAPISTORE_ERROR		retval;
	const char			*_backend_name;
	const char			*_backend_namespace;
	const char			*_backend_description;
	uint32_t			_backend_index;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!backend_name && !backend_namespace && !backend_description, 
			    MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!backend_index, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	_backend_index = *backend_index;
	retval = mapistore_backend_get_next_backend(&_backend_name, 
						    &_backend_namespace,
						    &_backend_description,
						    &_backend_index);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	if (backend_name) {
		*backend_name = _backend_name;
	}

	if (backend_namespace) {
		*backend_namespace = _backend_namespace;
	}
	
	if (backend_description) {
		*backend_description = _backend_description;
	}

	*backend_index = _backend_index;

	return MAPISTORE_SUCCESS;
}

/**
   \details Retrieve the LDIF data associated to registered backends
   by sequentially calling op_db_provision_namedprops operation.

   \param mstore_ctx pointer to the mapistore context
   \param backend_name pointer on pointer to the backend name to return
   \param ldif pointer on pointer to the LDIF data to return
   \param ntype pointer to the type of LDIF data to return

   \note It is also up to the caller application to free memory
   associated to ldif data.

   \return MAPISTORE_SUCCESS on success, or MAPISTORE_ERR_NOT_FOUND if
   there is no more available backends, otherwise MAPISTORE_ERROR
 */
enum MAPISTORE_ERROR mapistore_get_backend_ldif(struct mapistore_context *mstore_ctx,
						const char *backend_name,
						char **ldif,
						enum MAPISTORE_NAMEDPROPS_PROVISION_TYPE *ntype)
{
	enum MAPISTORE_ERROR				retval;
	enum MAPISTORE_NAMEDPROPS_PROVISION_TYPE	_ntype;
	char						*_ldif;
	
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!backend_name, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!ldif, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!ntype, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	retval = mapistore_backend_get_namedprops_ldif((TALLOC_CTX *) mstore_ctx, backend_name, &_ldif, &_ntype);

	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	*ldif = _ldif;
	*ntype = _ntype;

	return MAPISTORE_SUCCESS;
}


/**
   \details Release private backend data associated a folder / message
   opened within the mapistore backend

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   \param fmid a folder or message identifier
   \param type the type of fmid, either MAPISTORE_FOLDER or MAPISTORE_MESSAGE

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_release_record(struct mapistore_context *mstore_ctx,
						       uint32_t context_id,
						       uint64_t fmid,
						       uint8_t type)
{
	struct backend_context		*backend_ctx;
	int				ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend release_record */
	ret = mapistore_backend_release_record(backend_ctx, fmid, type);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}


/**
   \details Associate an indexing context to a mapistore context

   \param mstore_ctx pointer to the mapistore context
   \param username account name referencing the indexing record
   \param context_id the context identifier referencing the context to
   alter

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_add_context_indexing(struct mapistore_context *mstore_ctx,
							     const char *username,
							     uint32_t context_id)
{
	struct indexing_context_list	*indexing_ctx;
	struct backend_context		*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERROR, NULL);
	/* TODO: Fix context_id sign */
	MAPISTORE_RETVAL_IF((int)context_id == -1, MAPISTORE_ERROR, NULL);

	/* Step 0. Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	/* If the indexing pointer is already existing, return success */
	MAPISTORE_RETVAL_IF(backend_ctx->indexing, MAPISTORE_SUCCESS, NULL);

	/* Step 1. Search the indexing record */
	indexing_ctx = mapistore_indexing_search(mstore_ctx, username);
	MAPISTORE_RETVAL_IF(!indexing_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Reference the indexing record within backend context */
	backend_ctx->indexing = indexing_ctx;

	/* Step 3. Increment the indexing ref counter */
	mapistore_indexing_add_ref_count(indexing_ctx);

	DEBUG(0, ("mapistore_add_context_indexing username: %s\n", backend_ctx->indexing->username));

	return MAPISTORE_SUCCESS;
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
_PUBLIC_ const char *mapistore_errstr(enum MAPISTORE_ERROR mapistore_err)
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
	case MAPISTORE_ERR_CORRUPTED:
		return "Corrupted";
	case MAPISTORE_ERR_INVALID_PARAMETER:
		return "Invalid parameter";
	case MAPISTORE_ERR_NO_DIRECTORY:
		return "No such file or directory";
	case MAPISTORE_ERR_DATABASE_INIT:
		return "Database initialization failed";
	case MAPISTORE_ERR_DATABASE_OPS:
		return "Database operation failed";
	case MAPISTORE_ERR_BACKEND_REGISTER:
		return "Storage backend registration failed";
	case MAPISTORE_ERR_BACKEND_INIT:
		return "Storage backend initialization failed";
	case MAPISTORE_ERR_CONTEXT_FAILED:
		return "Context creation failed";
	case MAPISTORE_ERR_INVALID_NAMESPACE:
		return "Invalid namespace";
	case MAPISTORE_ERR_NOT_FOUND:
		return "Record or data not found";
	case MAPISTORE_ERR_REF_COUNT:
		return "Reference count still exists";
	case MAPISTORE_ERR_EXIST:
		return "Already exists";
	case MAPISTORE_ERR_INVALID_OBJECT:
		return "Invalid object";
	case MAPISTORE_ERR_INVALID_CONTEXT:
		return "Invalid mapistore context";
	case MAPISTORE_ERR_INVALID_URI:
		return "Invalid mapistore URI";
	case MAPISTORE_ERR_NOT_IMPLEMENTED:
		return "Not implemented";
	case MAPISTORE_ERR_RESERVED:
		return "Record or data reserved";
	}

	return "Unknown error";
}


_PUBLIC_ enum MAPISTORE_ERROR mapistore_create_uri(struct mapistore_context *mstore_ctx,
						   uint32_t index,
						   const char *namespace_uri,
						   const char *username,
						   char **_uri)
{
	enum MAPISTORE_ERROR	ret;
	char			*uri;
	char			*ref_str;
	char			*ns;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF((!namespace_uri || strlen(namespace_uri) < 4), MAPISTORE_ERR_INVALID_NAMESPACE, NULL);

	ref_str = (char *)namespace_uri;
	ns = strchr(namespace_uri, ':');
	if (!ns) {
		DEBUG(0, ("! [%s:%d][%s]: Invalid namespace '%s'\n", __FILE__, __LINE__, __FUNCTION__, ref_str));
		return MAPISTORE_ERR_INVALID_NAMESPACE;
	}

	if (ns[1] && ns[1] == '/' && ns[2] && ns[2] == '/') {
		if (ns[3]) {
			ns[3] = '\0';
		}
		ret = mapistore_backend_create_uri((TALLOC_CTX *)mstore_ctx, index, ref_str, username, &uri);
		if (ret == MAPISTORE_SUCCESS) {
			*_uri = uri;
		}
		return ret;
	}

	return MAPISTORE_ERR_NOT_FOUND;
}


/**
   \details Open a directory in mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where the directory will be opened
   \param parent_fid the parent folder identifier
   \param fid folder identifier to open

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_opendir(struct mapistore_context *mstore_ctx,
						uint32_t context_id,
						uint64_t parent_fid,
						uint64_t fid)
{
	struct backend_context		*backend_ctx;
	int				ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend opendir */
	ret = mapistore_backend_opendir(backend_ctx, parent_fid, fid);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}


/**
   \details Close a directory in mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where the directory has to be closed/released
   \param fid the folder identifier referencing the folder to close

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_closedir(struct mapistore_context *mstore_ctx,
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
   \param fid the folder identifier for the new folder
   \param aRow pointer to MAPI data structures with properties to be
   added to the new folder

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_mkdir(struct mapistore_context *mstore_ctx,
					      uint32_t context_id,
					      uint64_t parent_fid,
					      uint64_t fid,
					      struct SRow *aRow)
{
	struct backend_context		*backend_ctx;
	enum MAPISTORE_ERROR		ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);	
	
	/* Step 2. Call backend mkdir */
	ret = mapistore_backend_mkdir(backend_ctx, parent_fid, fid, aRow);

	return ret;
}


/**
   \details Remove a directory in mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   \param parent_fid the parent folder identifier
   \param fid the folder identifier representing the folder to delete
   \param flags flags that control the behaviour of the operation

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_rmdir(struct mapistore_context *mstore_ctx,
					      uint32_t context_id,
					      uint64_t parent_fid,
					      uint64_t fid,
					      uint8_t flags)
{
	struct backend_context		*backend_ctx;
	int				ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);
	DEBUG(4, ("mapistore_rmdir interface, fid 0x%"PRIx64" from parent 0x%"PRIx64"\n", fid, parent_fid));

	/* Step 1. Find the backend context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);	

	/* Step 2. Handle deletion of child folders / messages */
	if (flags | DEL_FOLDERS) {
		uint64_t	*childFolders;
		uint32_t	childFolderCount;
		int		retval;
		uint32_t	i;

		/* Get subfolders list */
		retval = mapistore_get_child_fids(mstore_ctx, context_id, fid,
						  &childFolders, &childFolderCount);
		DEBUG(4, ("mapistore_rmdir fid: 0x%"PRIx64", child count: %d\n", fid, childFolderCount));
		if (retval) {
			DEBUG(4, ("mapistore_rmdir bad retval: 0x%x", retval));
			return MAPISTORE_ERR_NOT_FOUND;
		}

		/* Delete each subfolder in mapistore */
		for (i = 0; i < childFolderCount; ++i) {
			DEBUG(4, ("mapistore_rmdir child: %d, FID: 0x%"PRIx64", parent: 0x%"PRIx64"\n", i, childFolders[i], fid));
			retval = mapistore_rmdir(mstore_ctx, context_id, fid, childFolders[i], flags);
			if (retval) {
				  DEBUG(4, ("mapistore_rmdir failed to delete fid 0x%"PRIx64" (0x%x)", childFolders[i], retval));
				  talloc_free(childFolders);
				  return MAPISTORE_ERR_NOT_FOUND;
			}
		}

	}
	
	/* Step 3. Call backend rmdir */
	DEBUG(4, ("mapistore_rmdir backend delete of fid 0x%"PRIx64" from parent 0x%"PRIx64"\n", fid, parent_fid));
	ret = mapistore_backend_rmdir(backend_ctx, parent_fid, fid);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}


/**
   \details Retrieve the number of child folders within a mapistore
   folder

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   \param fid the folder identifier
   \param RowCount pointer to the count result to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_get_folder_count(struct mapistore_context *mstore_ctx,
							 uint32_t context_id,
							 uint64_t fid,
							 uint32_t *RowCount)
{
	struct backend_context		*backend_ctx;
	enum MAPISTORE_ERROR		ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 0. Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 1. Call backend readdir */
	ret = mapistore_backend_readdir_count(backend_ctx, fid, MAPISTORE_FOLDER_TABLE, RowCount);

	return ret;
}


/**
   \details Retrieve the number of child messages within a mapistore folder

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   \param fid the folder identifier
   \param RowCount pointer to the count result to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_get_message_count(struct mapistore_context *mstore_ctx,
							  uint32_t context_id,
							  uint64_t fid,
							  uint32_t *RowCount)
{
	struct backend_context		*backend_ctx;
	enum MAPISTORE_ERROR		ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 0. Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend readdir_count */
	ret = mapistore_backend_readdir_count(backend_ctx, fid, MAPISTORE_MESSAGE_TABLE, RowCount);

	return ret;
}


/**
   \details Retrieve a MAPI property from a table

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   \param table_type the type of table (folders or messges)
   \param fid the folder identifier where the search takes place
   \param proptag the MAPI property tag to retrieve value for
   \param pos the record position in search results
   \param data pointer on pointer to the data the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_get_table_property(struct mapistore_context *mstore_ctx,
							   uint32_t context_id,
							   enum MAPISTORE_TABLE_TYPE table_type,
							   uint64_t fid,
							   enum MAPITAGS proptag,
							   uint32_t pos,
							   void **data)
{
	struct backend_context		*backend_ctx;
	enum MAPISTORE_ERROR		ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend readdir */
	ret = mapistore_backend_get_table_property(backend_ctx, fid, table_type, pos, proptag, data);

	return ret;
}


/**
   \details Open a message in mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where the directory will be opened
   \param parent_fid the parent folder identifier
   \param mid the message identifier to open
   \param msg pointer to the mapistore_message structure (result)

   \return MAPISTORE SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_openmessage(struct mapistore_context *mstore_ctx,
						    uint32_t context_id,
						    uint64_t parent_fid,
						    uint64_t mid,
						    struct mapistore_message *msg)
{
	struct backend_context		*backend_ctx;
	int				ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend openmessage */
	ret = mapistore_backend_openmessage(backend_ctx, parent_fid, mid, msg);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}


/**
   \details Create a message in mapistore

   \param mstore_ctx pointer to the mapistore context

   \param context_id the context identifier referencing the backend
   where the messagewill be created
   \param parent_fid the parent folder identifier
   \param mid the message identifier to create

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_createmessage(struct mapistore_context *mstore_ctx,
						      uint32_t context_id,
						      uint64_t parent_fid,
						      uint64_t mid)
{
	struct backend_context		*backend_ctx;
	int				ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	
	/* Step 2. Call backend createmessage */
	ret = mapistore_backend_createmessage(backend_ctx, parent_fid, mid);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}


/**
   \details Commit the changes made to a message in mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where the message's changes will be saved
   \param mid the message identifier to save
   \param flags flags associated to the commit operation

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_savechangesmessage(struct mapistore_context *mstore_ctx,
							   uint32_t context_id,
							   uint64_t mid,
							   uint8_t flags)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend savechangesmessage */
	ret = mapistore_backend_savechangesmessage(backend_ctx, mid, flags);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}


/**
   \details Submits a message for sending.

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where the message will be submitted
   \param mid the message identifier representing the message to submit
   \param flags flags associated to the submit operation

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_submitmessage(struct mapistore_context *mstore_ctx,
						      uint32_t context_id,
						      uint64_t mid,
						      uint8_t flags)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend submitmessage */
	ret = mapistore_backend_submitmessage(backend_ctx, mid, flags);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}


/**
   \details Get properties of a message/folder in mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where properties will be fetched
   \param fmid the identifier referencing the message/folder
   \param type the object type (folder or message)
   \param properties pointer to the list of properties to fetch
   \param aRow pointer to the SRow structure

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_getprops(struct mapistore_context *mstore_ctx,
						 uint32_t context_id,
						 uint64_t fmid,
						 uint8_t type,
						 struct SPropTagArray *properties,
						 struct SRow *aRow)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend getprops */
	ret = mapistore_backend_getprops(backend_ctx, fmid, type, properties, aRow);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

/**
   \details Search for a folder ID by name

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where the folder will be searched for
   \param parent_fid the parent folder identifier
   \param name the name of the folder to search for
   \param fid the fid (result)

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_get_fid_by_name(struct mapistore_context *mstore_ctx,
							uint32_t context_id,
							uint64_t parent_fid,
							const char *name,
							uint64_t *fid)
{
	struct backend_context	*backend_ctx;
	enum MAPISTORE_ERROR	ret;

	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);
	MAPISTORE_RETVAL_IF(!name, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!fid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend getprops */
	ret = mapistore_backend_get_fid_by_name(backend_ctx, parent_fid, name, fid);

	return ret;
}

/**
   \details Set properties of a message/folder in mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where properties will be stored
   \param fmid the identifier referencing the message/folder
   \param type the object type (folder or message)
   \param aRow pointer to the SRow structure

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_setprops(struct mapistore_context *mstore_ctx,
						 uint32_t context_id,
						 uint64_t fmid,
						 uint8_t type,
						 struct SRow *aRow)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend setprops */
	ret = mapistore_backend_setprops(backend_ctx, fmid, type, aRow);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}


/**
   \details Retrieve the folder IDs of child folders within a mapistore
   folder

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   \param fid the folder identifier (for the parent folder)
   \param child_fids pointer to where to return the array of child fids
   \param child_fid_count pointer to the count result to return

   \note The caller is responsible for freeing the \p child_fids array
   when it is no longer required.
   
   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_get_child_fids(struct mapistore_context *mstore_ctx,
						       uint32_t context_id,
						       uint64_t fid,
						       uint64_t *child_fids[],
						       uint32_t *child_fid_count)
{
	struct backend_context		*backend_ctx;
	uint32_t			i;
	void				*data;
	int				ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 0. Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 1. Call backend readdir to get the folder count */
	ret = mapistore_backend_readdir_count(backend_ctx, fid, MAPISTORE_FOLDER_TABLE, child_fid_count);
	MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERR_NO_DIRECTORY, NULL);
	
	/* Step 2. Create a suitable sized array for the fids */
	*child_fids = talloc_zero_array((TALLOC_CTX *)mstore_ctx, uint64_t, *child_fid_count);

	/* Step 3. Fill the array */
	for (i = 0; i < *child_fid_count; ++i) {
		// TODO: add error checking for this call
		ret = mapistore_get_table_property(mstore_ctx, context_id, MAPISTORE_FOLDER_TABLE, fid,
						   PR_FID, i, &data);
		(*child_fids)[i] = *((uint64_t*)(data));
	}

	return MAPISTORE_SUCCESS;
}

/**
   \details Delete a message from mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where the message's to be located is stored
   \param mid the message identifier of the folder to delete
   \param deletion_type the type of deletion (MAPISTORE_SOFT_DELETE or MAPISTORE_PERMANENT_DELETE)

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_deletemessage(struct mapistore_context *mstore_ctx,
						      uint32_t context_id,
						      uint64_t mid,
						      enum MAPISTORE_DELETION_TYPE deletion_type)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_deletemessage(backend_ctx, mid, deletion_type);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}
