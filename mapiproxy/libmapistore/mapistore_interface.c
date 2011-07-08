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
#include "libmapi/libmapi_private.h"

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
	mstore_ctx->indexing_list = talloc_zero(mstore_ctx, struct indexing_context_list);
	mstore_ctx->subscriptions = NULL;
	mstore_ctx->replica_mapping_ctx = NULL;

	mstore_ctx->nprops_ctx = NULL;
	retval = mapistore_namedprops_init(mstore_ctx, &(mstore_ctx->nprops_ctx));

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

	talloc_free(mstore_ctx->nprops_ctx);
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
				   const char *uri, uint64_t fid,
                                   uint32_t *context_id)
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
		backend_ctx = mapistore_backend_create_context(mstore_ctx, mstore_ctx->conn_info, namespace_start, backend_uri, fid);
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
   \details Increase the reference counter of an existing context

   \param mstore_ctx pointer to the mapistore context
   \param contex_id the context identifier referencing the context to
   update

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_add_context_ref_count(struct mapistore_context *mstore_ctx,
					     uint32_t context_id)
{
	struct backend_context		*backend_ctx;
	int				retval;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	if (context_id == -1) return MAPISTORE_ERROR;

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
_PUBLIC_ int mapistore_search_context_by_uri(struct mapistore_context *mstore_ctx,
					     const char *uri,
					     uint32_t *context_id)
{
	struct backend_context		*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	if (!uri) return MAPISTORE_ERROR;

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
_PUBLIC_ int mapistore_del_context(struct mapistore_context *mstore_ctx, 
				   uint32_t context_id)
{
	struct backend_context_list	*backend_list;
	struct backend_context		*backend_ctx;
	int				retval;
	bool				found = false;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	if (context_id == -1) return MAPISTORE_ERROR;

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
   \details Release private backend data associated a folder / message
   opened within the mapistore backend

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   \param fmid a folder or message identifier
   \param type the type of fmid: MAPISTORE_FOLDER_TABLE, MAPISTORE_MESSAGE_TABLE, ....

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ int mapistore_release_record(struct mapistore_context *mstore_ctx,
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
_PUBLIC_ int mapistore_add_context_indexing(struct mapistore_context *mstore_ctx,
					    const char *username,
					    uint32_t context_id)
{
	struct indexing_context_list	*indexing_ctx;
	struct backend_context		*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(context_id == -1, MAPISTORE_ERROR, NULL);

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
   \param fid folder identifier to open

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ int mapistore_opendir(struct mapistore_context *mstore_ctx,
			       uint32_t context_id,
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
	ret = mapistore_backend_opendir(backend_ctx, fid);

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
   \param aRow pointer to MAPI data structures with properties to be
   added to the new folder

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ int mapistore_mkdir(struct mapistore_context *mstore_ctx,
			     uint32_t context_id,
			     uint64_t parent_fid,
			     uint64_t fid,
			     struct SRow *aRow)
{
	struct backend_context		*backend_ctx;
	int				ret;

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
_PUBLIC_ int mapistore_rmdir(struct mapistore_context *mstore_ctx,
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
			return MAPI_E_NOT_FOUND;
		}

		/* Delete each subfolder in mapistore */
		for (i = 0; i < childFolderCount; ++i) {
			DEBUG(4, ("mapistore_rmdir child: %d, FID: 0x%"PRIx64", parent: 0x%"PRIx64"\n", i, childFolders[i], fid));
			retval = mapistore_rmdir(mstore_ctx, context_id, fid, childFolders[i], flags);
			if (retval) {
				  DEBUG(4, ("mapistore_rmdir failed to delete fid 0x%"PRIx64" (0x%x)", childFolders[i], retval));
				  talloc_free(childFolders);
				  return MAPI_E_NOT_FOUND;
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
_PUBLIC_ int mapistore_get_folder_count(struct mapistore_context *mstore_ctx,
					uint32_t context_id,
					uint64_t fid,
					uint32_t *RowCount)
{
	struct backend_context		*backend_ctx;
	int				ret;

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
_PUBLIC_ int mapistore_get_message_count(struct mapistore_context *mstore_ctx,
					 uint32_t context_id,
					 uint64_t fid,
					 uint8_t table_type,
					 uint32_t *RowCount)
{
	struct backend_context		*backend_ctx;
	int				ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 0. Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend readdir_count */
	ret = mapistore_backend_readdir_count(backend_ctx, fid, table_type, RowCount);

	return ret;
}


/**
   \details Retrieve a MAPI property from a table

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   \param table_type the type of table (folders, messages, ...)
   \param fid the folder identifier where the search takes place
   \param proptag the MAPI property tag to retrieve value for
   \param pos the record position in search results
   \param data pointer on pointer to the data the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ int mapistore_get_table_property(struct mapistore_context *mstore_ctx,
					  uint32_t context_id,
					  uint8_t table_type,
					  enum table_query_type query_type,
					  uint64_t fid,
					  uint32_t proptag,
					  uint32_t pos,
					  void **data)
{
	struct backend_context		*backend_ctx;
	int				ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend readdir */
	ret = mapistore_backend_get_table_property(backend_ctx, fid, table_type, query_type, pos, proptag, data);

	return ret;
}

_PUBLIC_ int mapistore_get_available_table_properties(struct mapistore_context *mstore_ctx, uint32_t context_id, uint8_t table_type, struct SPropTagArray **propertiesp)
{
	struct backend_context		*backend_ctx;
	int				ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend readdir */
	ret = mapistore_backend_get_available_table_properties(backend_ctx, table_type, propertiesp);

	return ret;
}

/**
   \details Open a message in mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where the directory will be opened
   \param parent_fid the parent folder identifier
   \param mid the message identifier to open
   \param pointer to the mapistore_message structure

   \return MAPISTORE SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ int mapistore_openmessage(struct mapistore_context *mstore_ctx,
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
_PUBLIC_ int mapistore_createmessage(struct mapistore_context *mstore_ctx,
				     uint32_t context_id,
				     uint64_t parent_fid,
				     uint64_t mid,
				     uint8_t associated)
{
	struct backend_context		*backend_ctx;
	int				ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	
	/* Step 2. Call backend createmessage */
	ret = mapistore_backend_createmessage(backend_ctx, parent_fid, mid, associated);

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
_PUBLIC_ int mapistore_savechangesmessage(struct mapistore_context *mstore_ctx,
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
_PUBLIC_ int mapistore_submitmessage(struct mapistore_context *mstore_ctx,
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
_PUBLIC_ int mapistore_getprops(struct mapistore_context *mstore_ctx,
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
   \param foldername the name of the folder to search for
   \param fid the fid (result)

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ int mapistore_get_fid_by_name(struct mapistore_context *mstore_ctx,
				       uint32_t context_id,
				       uint64_t parent_fid,
				       const char *name,
				       uint64_t *fid)
{
	struct backend_context	*backend_ctx;
	int			ret;

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
_PUBLIC_ int mapistore_setprops(struct mapistore_context *mstore_ctx,
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
   \details Modify recipients of a message in mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where properties will be stored
   \param mid the identifier referencing the message
   \rows the array of recipient rows
   \count the number of elements in the array

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
int mapistore_modifyrecipients(struct mapistore_context *mstore_ctx, uint32_t context_id, uint64_t mid, struct ModifyRecipientRow *rows, uint16_t count)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend modifyrecipients */
	ret = mapistore_backend_modifyrecipients(backend_ctx, mid, rows, count);

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
_PUBLIC_ int mapistore_get_child_fids(struct mapistore_context *mstore_ctx,
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
		ret = mapistore_get_table_property(mstore_ctx, context_id, MAPISTORE_FOLDER_TABLE, MAPISTORE_PREFILTERED_QUERY, fid, PR_FID, i, &data);
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
   \param flags flags that control the behaviour of the operation (MAPISTORE_SOFT_DELETE
   or MAPISTORE_PERMANENT_DELETE)

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ int mapistore_deletemessage(struct mapistore_context *mstore_ctx,
				     uint32_t context_id,
				     uint64_t fid, uint64_t mid,
				     uint8_t flags)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_deletemessage(backend_ctx, fid, mid, flags);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

/**
   \details Does something

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where the message's to be located is stored
   \param fmid the folder/message identifier at the end of the list
   \param folder_list the destination of the list

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ struct backend_context *mapistore_find_container_backend(struct mapistore_context *mstore_ctx,
								  uint64_t fmid)
{
        char                    *path;
	struct backend_context_list *el;
	struct backend_context	*bctx = NULL;
        
	/* SANITY checks */
	if (!mstore_ctx) return NULL;

	/* Step 1. Search the context */
        path = NULL;
        if (!mstore_ctx->context_list)
                return NULL;
	for (el = mstore_ctx->context_list; el; el = el->next) {
		if (el->ctx && el->ctx->uri) {
                        if ((mapistore_get_path (el->ctx, fmid, MAPISTORE_MESSAGE, &path)
                             == MAPISTORE_SUCCESS)
                            || (mapistore_get_path (el->ctx, fmid, MAPISTORE_FOLDER, &path)
                                == MAPISTORE_SUCCESS)) {
				bctx = el->ctx;
				return bctx;
				break;
			}
		}
	}

	DEBUG(5, ("[%s:%d] %lld NOT FOUND\n", __FUNCTION__, __LINE__, (unsigned long long) fmid));

	return NULL;
}

/**
   \details Apply restrictions on a mapistore table

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend where the 
   \param table_type the table type
 */
_PUBLIC_ int mapistore_set_restrictions(struct mapistore_context *mstore_ctx,
					uint32_t context_id,
					uint64_t fid,
					uint8_t table_type,
					struct mapi_SRestriction *res,
					uint8_t *table_status)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_set_restrictions(backend_ctx, fid, table_type, res, table_status);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

/**
   \details Sort a mapistore table

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend where the 
 */
_PUBLIC_ int mapistore_set_sort_order(struct mapistore_context *mstore_ctx,
                                      uint32_t context_id,
                                      uint64_t fid, uint8_t type,
                                      struct SSortOrderSet *set,
                                      uint8_t *table_status)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_set_sort_order(backend_ctx, fid, type, set, table_status);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}


/* proof of concept */
int mapistore_pocop_open_table(struct mapistore_context *mstore_ctx, uint32_t context_id, uint64_t folder_id,
                               uint8_t table_type, uint32_t handle_id, void **table, uint32_t *row_count)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_pocop_open_table(backend_ctx, folder_id, table_type, handle_id, table, row_count);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

_PUBLIC_ int mapistore_pocop_get_attachment_table(struct mapistore_context *mstore_ctx, uint32_t context_id, uint64_t mid,
                                                  void **table, uint32_t *row_count)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_pocop_get_attachment_table(backend_ctx, mid, table, row_count);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

_PUBLIC_ int mapistore_pocop_get_attachment(struct mapistore_context *mstore_ctx, uint32_t context_id,
                                            uint64_t mid, uint32_t aid,
                                            void **attachment)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_pocop_get_attachment(backend_ctx, mid, aid, attachment);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

_PUBLIC_ int mapistore_pocop_create_attachment(struct mapistore_context *mstore_ctx, uint32_t context_id,
                                               uint64_t mid,
                                               uint32_t *aid, void **attachment)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_pocop_create_attachment(backend_ctx, mid, aid, attachment);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

_PUBLIC_ int mapistore_pocop_open_embedded_message(struct mapistore_context *mstore_ctx, uint32_t context_id, void *object, uint64_t *mid, enum OpenEmbeddedMessage_OpenModeFlags flags, struct mapistore_message *msg, void **message)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_pocop_open_embedded_message(backend_ctx, object, mid, flags, msg, message);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

_PUBLIC_ int mapistore_pocop_get_available_table_properties(struct mapistore_context *mstore_ctx, uint32_t context_id, void *table, struct SPropTagArray **propertiesp)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_pocop_get_available_table_properties(backend_ctx, table, propertiesp);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

_PUBLIC_ int mapistore_pocop_set_table_columns(struct mapistore_context *mstore_ctx, uint32_t context_id, void *table, uint16_t count, enum MAPITAGS *properties)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_pocop_set_table_columns(backend_ctx, table, count, properties);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

_PUBLIC_ int mapistore_pocop_set_restrictions(struct mapistore_context *mstore_ctx, uint32_t context_id, void *table, struct mapi_SRestriction *restrictions, uint8_t *table_status)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_pocop_set_table_restrictions(backend_ctx, table, restrictions, table_status);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

_PUBLIC_ int mapistore_pocop_set_sort_order(struct mapistore_context *mstore_ctx, uint32_t context_id, void *table, struct SSortOrderSet *sort_order, uint8_t *table_status)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_pocop_set_table_sort_order(backend_ctx, table, sort_order, table_status);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

_PUBLIC_ int mapistore_pocop_get_table_row(struct mapistore_context *mstore_ctx, uint32_t context_id, void *table,
                                           enum table_query_type query_type, uint32_t rowid, struct mapistore_property_data *data)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_pocop_get_table_row(backend_ctx, table, query_type, rowid, data);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

_PUBLIC_ int mapistore_pocop_get_available_properties(struct mapistore_context *mstore_ctx, uint32_t context_id, void *object, struct SPropTagArray **propertiesp)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_pocop_get_available_properties(backend_ctx, object, propertiesp);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}


_PUBLIC_ int mapistore_pocop_get_properties(struct mapistore_context
                                            *mstore_ctx, uint32_t context_id,
                                            void *object,
                                            uint16_t count, enum MAPITAGS *properties,
                                            struct mapistore_property_data *data)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_pocop_get_properties(backend_ctx, object, count, properties, data);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

_PUBLIC_ int mapistore_pocop_set_properties(struct mapistore_context
                                            *mstore_ctx, uint32_t context_id,
                                            void *object,
                                            struct SRow *aRow)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_pocop_set_properties(backend_ctx, object, aRow);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}

_PUBLIC_ int mapistore_pocop_release(struct mapistore_context
                                     *mstore_ctx, uint32_t context_id,
                                     void *object)
{
	struct backend_context	*backend_ctx;
	int			ret;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	ret = mapistore_backend_pocop_release(backend_ctx, object);

	return !ret ? MAPISTORE_SUCCESS : MAPISTORE_ERROR;
}
