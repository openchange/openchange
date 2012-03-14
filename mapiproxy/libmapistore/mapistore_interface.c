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

#include <sys/stat.h>
#include <sys/types.h>

#include "mapistore.h"
#include "mapistore_errors.h"
#include "mapistore_private.h"
#include <dlinklist.h>
#include "libmapi/libmapi_private.h"

#include <string.h>

/**
   \details Initialize the mapistore context

   \param mem_ctx pointer to the memory context
   \param path the path to the location to load the backend providers from (NULL for default)

   \return allocate mapistore context on success, otherwise NULL
 */
_PUBLIC_ struct mapistore_context *mapistore_init(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx, const char *path)
{
	int				retval;
	struct mapistore_context	*mstore_ctx;
	const char			*private_dir;
	char				*mapping_path;

	if (!lp_ctx) {
		return NULL;
	}

	mstore_ctx = talloc_zero(mem_ctx, struct mapistore_context);
	if (!mstore_ctx) {
		return NULL;
	}

	mstore_ctx->processing_ctx = talloc_zero(mstore_ctx, struct processing_context);

	private_dir = lpcfg_private_dir(lp_ctx);
	if (!private_dir) {
		DEBUG(5, ("private directory was not returned from configuration\n"));
		return NULL;
	}

	mapping_path = talloc_asprintf(NULL, "%s/mapistore", private_dir);
	mkdir(mapping_path, 0700);

	mapistore_set_mapping_path(mapping_path);
	talloc_free(mapping_path);

	retval = mapistore_init_mapping_context(mstore_ctx->processing_ctx);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("[%s:%d]: %s\n", __FUNCTION__, __LINE__, mapistore_errstr(retval)));
		talloc_free(mstore_ctx);
		return NULL;
	}

	retval = mapistore_backend_init(mstore_ctx, path);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("[%s:%d]: %s\n", __FUNCTION__, __LINE__, mapistore_errstr(retval)));
		talloc_free(mstore_ctx);
		return NULL;
	}

	mstore_ctx->context_list = NULL;
	mstore_ctx->indexing_list = talloc_zero(mstore_ctx, struct indexing_context_list);
	mstore_ctx->replica_mapping_list = talloc_zero(mstore_ctx, struct replica_mapping_context_list);
	mstore_ctx->notifications = NULL;
	mstore_ctx->subscriptions = NULL;
	mstore_ctx->conn_info = NULL;

	mstore_ctx->nprops_ctx = NULL;
	retval = mapistore_namedprops_init(mstore_ctx, &(mstore_ctx->nprops_ctx));
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("[%s:%d]: %s\n", __FUNCTION__, __LINE__, mapistore_errstr(retval)));
		talloc_free(mstore_ctx);
		return NULL;
	}

	mstore_ctx->mq_ipc = mq_open(MAPISTORE_MQUEUE_IPC, O_WRONLY|O_NONBLOCK|O_CREAT, 0755, NULL);
	if (mstore_ctx->mq_ipc == -1) {
		DEBUG(0, ("[%s:%d]: Failed to open mqueue for %s\n", __FUNCTION__, __LINE__, MAPISTORE_MQUEUE_IPC));
		talloc_free(mstore_ctx);
		return NULL;
	}

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
_PUBLIC_ enum mapistore_error mapistore_release(struct mapistore_context *mstore_ctx)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	DEBUG(5, ("freeing up mstore_ctx ref: %p\n", mstore_ctx));

	talloc_free(mstore_ctx->nprops_ctx);
	talloc_free(mstore_ctx->processing_ctx);
	talloc_free(mstore_ctx->context_list);

	return MAPISTORE_SUCCESS;
}

/**
   \details Set connection info for current mapistore context

   \param mstore_ctx pointer to the mapistore context
   \param oc_ctx pointer to the openchange ldb database
   \param username pointer to the current username

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_set_connection_info(struct mapistore_context *mstore_ctx, 
							    struct ldb_context *sam_ctx, struct ldb_context *oc_ctx, const char *username)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!sam_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!oc_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mstore_ctx->conn_info = talloc_zero(mstore_ctx, struct mapistore_connection_info);
	mstore_ctx->conn_info->mstore_ctx = mstore_ctx;
	mstore_ctx->conn_info->sam_ctx = sam_ctx;
	mstore_ctx->conn_info->oc_ctx = oc_ctx;
	(void) talloc_reference(mstore_ctx->conn_info, mstore_ctx->conn_info->oc_ctx);
	mstore_ctx->conn_info->username = talloc_strdup(mstore_ctx->conn_info, username);

	return MAPISTORE_SUCCESS;
}

/**
   \details Add a new connection context to mapistore

   \param mstore_ctx pointer to the mapistore context
   \param uri the connection context URI
   \param context_id pointer to the context identifier the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_add_context(struct mapistore_context *mstore_ctx, const char *owner,
						    const char *uri, uint64_t fid, uint32_t *context_id, void **backend_object)
{
	TALLOC_CTX				*mem_ctx;
	int					retval;
	struct backend_context			*backend_ctx;
	struct backend_context_list    		*backend_list;
	char					*namespace;
	char					*namespace_start;
	char					*backend_uri;
	char					*mapistore_dir;
	struct indexing_context_list		*ictx;

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
		/* ensure the user mapistore directory exists before any mapistore operation occurs */
		mapistore_dir = talloc_asprintf(mem_ctx, "%s/%s", mapistore_get_mapping_path(), owner);
		mkdir(mapistore_dir, 0700);

		mapistore_indexing_add(mstore_ctx, owner, &ictx);
		/* mapistore_indexing_add_ref_count(ictx); */

		backend_uri = talloc_strdup(mem_ctx, &namespace[3]);
		namespace[3] = '\0';
		retval = mapistore_backend_create_context(mstore_ctx, mstore_ctx->conn_info, ictx->index_ctx, namespace_start, backend_uri, fid, &backend_ctx);
		if (retval != MAPISTORE_SUCCESS) {
			return retval;
		}

		backend_ctx->indexing = ictx;

		backend_list = talloc_zero((TALLOC_CTX *) mstore_ctx, struct backend_context_list);
		backend_list->ctx = backend_ctx;
		retval = mapistore_get_context_id(mstore_ctx->processing_ctx, &backend_list->ctx->context_id);
		if (retval != MAPISTORE_SUCCESS) {
			talloc_free(mem_ctx);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		*context_id = backend_list->ctx->context_id;
		*backend_object = backend_list->ctx->root_folder_object;
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
_PUBLIC_ enum mapistore_error mapistore_add_context_ref_count(struct mapistore_context *mstore_ctx,
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

	/* Step 2. Increment backend indexing ref count */
	if (backend_ctx->indexing) {
		/* mapistore_indexing_add_ref_count(backend_ctx->indexing); */
	} else {
		DEBUG(0, ("[%s:%d]: This should never occur\n", __FUNCTION__, __LINE__));
		abort();
	}

	return retval;
}


/**
   \details Search for an existing context given its uri

   \param mstore_ctx pointer to the mapistore context
   \param uri the URI to lookup
   \param context_id pointer to the context identifier to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_search_context_by_uri(struct mapistore_context *mstore_ctx,
							      const char *uri, uint32_t *context_id, void **backend_object)
{
	struct backend_context		*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	if (!uri) return MAPISTORE_ERROR;

	backend_ctx = mapistore_backend_lookup_by_uri(mstore_ctx->context_list, uri);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_NOT_FOUND, NULL);

	*context_id = backend_ctx->context_id;
	*backend_object = backend_ctx->root_folder_object;

	return MAPISTORE_SUCCESS;
}


/**
   \details Delete an existing connection context from mapistore

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the context to
   delete

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_del_context(struct mapistore_context *mstore_ctx, 
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

	/* Step 1. Release the indexing context within backend */
	/* if (backend_ctx->indexing) {
		mapistore_indexing_del_ref_count(backend_ctx->indexing);
		if (backend_ctx->indexing->ref_count == 0) {
			DEBUG(5, ("freeing up mapistore_indexing ctx: %p\n", backend_ctx->indexing));
			DLIST_REMOVE(mstore_ctx->indexing_list, backend_ctx->indexing);
			talloc_unlink(mstore_ctx->indexing_list, backend_ctx->indexing);
			backend_ctx->indexing = NULL;
		}
	} */

	/* Step 2. Delete the context within backend */
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
_PUBLIC_ const char *mapistore_errstr(enum mapistore_error mapistore_err)
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
		return "Invalid Parameter";
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
		return "Failed creating the context";
	case MAPISTORE_ERR_INVALID_NAMESPACE:
		return "Invalid Namespace";
	case MAPISTORE_ERR_NOT_FOUND:
		return "Not Found";
	case MAPISTORE_ERR_REF_COUNT:
		return "Reference counter not NULL";
	case MAPISTORE_ERR_EXIST:
		return "Already Exists";
	case MAPISTORE_ERR_INVALID_DATA:
		return "Invalid Data";
	case MAPISTORE_ERR_MSG_SEND:
		return "Error while sending message";
	case MAPISTORE_ERR_MSG_RCV:
		return "Error receiving message";
	case MAPISTORE_ERR_DENIED:
		return "Insufficient rights to perform the operation";
	case MAPISTORE_ERR_NOT_IMPLEMENTED:
		return "Not implemented";
	}

	return "Unknown error";
}

_PUBLIC_ enum mapistore_error mapistore_list_contexts_for_user(struct mapistore_context *mstore_ctx, const char *owner, TALLOC_CTX *mem_ctx, struct mapistore_contexts_list **contexts_listp)
{
	char					*mapistore_dir;
	struct indexing_context_list		*ictx;

	/* ensure the user mapistore directory exists before any mapistore operation occurs */
	mapistore_dir = talloc_asprintf(mem_ctx, "%s/%s", mapistore_get_mapping_path(), owner);
	mkdir(mapistore_dir, 0700);

	mapistore_indexing_add(mstore_ctx, owner, &ictx);
	/* mapistore_indexing_add_ref_count(ictx); */
 
	return mapistore_backend_list_contexts(owner, ictx->index_ctx, mem_ctx, contexts_listp);
}

_PUBLIC_ enum mapistore_error mapistore_create_root_folder(const char *username, enum mapistore_context_role ctx_role, uint64_t fid, const char *name, TALLOC_CTX *mem_ctx, char **mapistore_urip)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mapistore_urip, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	return mapistore_backend_create_root_folder(username, ctx_role, fid, name, mem_ctx, mapistore_urip);
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
_PUBLIC_ enum mapistore_error mapistore_folder_open_folder(struct mapistore_context *mstore_ctx, uint32_t context_id,
							   void *folder, TALLOC_CTX *mem_ctx, uint64_t fid, void **child_folder)
{
	struct backend_context		*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend open_folder */
	return mapistore_backend_folder_open_folder(backend_ctx, folder, mem_ctx, fid, child_folder);
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
_PUBLIC_ enum mapistore_error mapistore_folder_create_folder(struct mapistore_context *mstore_ctx, uint32_t context_id,
							     void *folder, TALLOC_CTX *mem_ctx, uint64_t fid, struct SRow *aRow, void **child_folder)
{
	struct backend_context		*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);	
	
	/* Step 2. Call backend create_folder */
	return mapistore_backend_folder_create_folder(backend_ctx, folder, mem_ctx, fid, aRow, child_folder);
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
_PUBLIC_ enum mapistore_error mapistore_folder_delete(struct mapistore_context *mstore_ctx, uint32_t context_id, void *folder, uint8_t flags)
{
	struct backend_context	*backend_ctx;
	enum mapistore_error	ret;
	TALLOC_CTX		*mem_ctx;
	void			*subfolder;
	uint64_t		*child_fmids;
	uint32_t		i, child_count;

	/* TODO : handle the removal of entries in indexing.tdb */

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	/* Step 1. Find the backend context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	if (!backend_ctx) {
		ret = MAPISTORE_ERR_INVALID_PARAMETER;
		goto end;
	}

	/* Step 2a. Handle deletion of normal messages */
	ret = mapistore_folder_get_child_fmids(mstore_ctx, context_id, folder, MAPISTORE_MESSAGE_TABLE, mem_ctx, &child_fmids, &child_count);
	if (ret != MAPISTORE_SUCCESS) {
		goto end;
	}
	if (child_count > 0) {
		if ((flags & DEL_MESSAGES)) {
			for (i = 0; i < child_count; i++) {
				ret = mapistore_backend_folder_delete_message(backend_ctx, folder, child_fmids[i], 0);
				if (ret != MAPISTORE_SUCCESS) {
					goto end;
				}
			}
		}
		else {
			ret = MAPISTORE_ERR_EXIST;
			goto end;
		}
	}

	/* Step 2b. Handle deletion of FAI messages */
	ret = mapistore_folder_get_child_fmids(mstore_ctx, context_id, folder, MAPISTORE_FAI_TABLE, mem_ctx, &child_fmids, &child_count);
	if (ret != MAPISTORE_SUCCESS) {
		goto end;
	}
	if (child_count > 0) {
		if ((flags & DEL_MESSAGES)) {
			for (i = 0; i < child_count; i++) {
				ret = mapistore_backend_folder_delete_message(backend_ctx, folder, child_fmids[i], 0);
				if (ret != MAPISTORE_SUCCESS) {
					goto end;
				}
			}
		}
		else {
			ret = MAPISTORE_ERR_EXIST;
			goto end;
		}
	}

	/* Step 3. Handle deletion of child folders */
	ret = mapistore_folder_get_child_fmids(mstore_ctx, context_id, folder, MAPISTORE_FOLDER_TABLE, mem_ctx, &child_fmids, &child_count);
	if (ret != MAPISTORE_SUCCESS) {
		goto end;
	}
	if (child_count > 0) {
		if ((flags & DEL_FOLDERS)) {
			for (i = 0; i < child_count; i++) {
				ret = mapistore_backend_folder_open_folder(backend_ctx, folder, mem_ctx, child_fmids[i], &subfolder);
				if (ret != MAPISTORE_SUCCESS) {
					goto end;
				}

				ret = mapistore_backend_folder_delete(backend_ctx, subfolder);
				if (ret != MAPISTORE_SUCCESS) {
					goto end;
				}
			}
		}
		else {
			ret = MAPISTORE_ERR_EXIST;
			goto end;
		}
	}

	/* Step 3. Call backend delete_folder */
	ret = mapistore_backend_folder_delete(backend_ctx, folder);

end:
	talloc_free(mem_ctx);

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
_PUBLIC_ enum mapistore_error mapistore_folder_open_message(struct mapistore_context *mstore_ctx, uint32_t context_id,
							    void *folder, TALLOC_CTX *mem_ctx, uint64_t mid, bool read_write, void **messagep)
{
	struct backend_context		*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend open_message */
	return mapistore_backend_folder_open_message(backend_ctx, folder, mem_ctx, mid, read_write, messagep);
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
_PUBLIC_ enum mapistore_error mapistore_folder_create_message(struct mapistore_context *mstore_ctx, uint32_t context_id,
							      void *folder, TALLOC_CTX *mem_ctx, uint64_t mid, uint8_t associated, void **messagep)
{
	struct backend_context		*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	
	/* Step 2. Call backend create_message */
	return mapistore_backend_folder_create_message(backend_ctx, folder, mem_ctx, mid, associated, messagep);
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
_PUBLIC_ enum mapistore_error mapistore_folder_delete_message(struct mapistore_context *mstore_ctx, uint32_t context_id,
							      void *folder, uint64_t mid, uint8_t flags)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_folder_delete_message(backend_ctx, folder, mid, flags);
}

/**

 */
_PUBLIC_ enum mapistore_error mapistore_folder_move_copy_messages(struct mapistore_context *mstore_ctx, uint32_t context_id,
								  void *target_folder, void *source_folder, uint32_t mid_count, uint64_t *source_mids, uint64_t *target_mids, struct Binary_r **target_change_keys, uint8_t want_copy)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_folder_move_copy_messages(backend_ctx, target_folder, source_folder, mid_count, source_mids, target_mids, target_change_keys, want_copy);
}


/**
   \details Get the array of deleted items following a specific change number

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   where the message's to be located is stored
   \param folder the folder backend object
   \param mem_ctx the TALLOC_CTX that should be used as parent for the returned array
   \param table_type the type of object that we want to take into account
   \param change_num the reference change number
   \param fmidsp a pointer to the returned array

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum mapistore_error mapistore_folder_get_deleted_fmids(struct mapistore_context *mstore_ctx, uint32_t context_id, void *folder, TALLOC_CTX *mem_ctx, enum mapistore_table_type table_type, uint64_t change_num, struct I8Array_r **fmidsp, uint64_t *cnp)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_folder_get_deleted_fmids(backend_ctx, folder, mem_ctx, table_type, change_num, fmidsp, cnp);
}

/**
   \details Retrieve the number of child messages within a mapistore folder

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the backend
   \param fid the folder identifier
   \param RowCount pointer to the count result to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE errors
 */
_PUBLIC_ enum mapistore_error mapistore_folder_get_child_count(struct mapistore_context *mstore_ctx, uint32_t context_id, void *folder, enum mapistore_table_type table_type, uint32_t *RowCount)
{
	struct backend_context		*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 0. Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend get_child_count */
	return mapistore_backend_folder_get_child_count(backend_ctx, folder, table_type, RowCount);
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
_PUBLIC_ enum mapistore_error mapistore_folder_get_child_fmids(struct mapistore_context *mstore_ctx, uint32_t context_id, void *folder, enum mapistore_table_type table_type, TALLOC_CTX *mem_ctx, uint64_t *child_fmids[], uint32_t *child_fmid_count)
{
	TALLOC_CTX			*local_mem_ctx;
	enum mapistore_error		ret;
	void				*backend_table;
	uint32_t			i, row_count;
	uint64_t			*fmids, *current_fmid;
	enum MAPITAGS			fmid_column;
	struct mapistore_property_data	*row_data;

	switch (table_type) {
	case MAPISTORE_FOLDER_TABLE:
		fmid_column = PR_FID;
		break;
	case MAPISTORE_MESSAGE_TABLE:
	case MAPISTORE_FAI_TABLE:
		fmid_column = PR_MID;
		break;
	case MAPISTORE_RULE_TABLE:
		fmid_column = PR_RULE_ID;
		break;
	case MAPISTORE_ATTACHMENT_TABLE:
		fmid_column = PR_ATTACH_ID;
		break;
	case MAPISTORE_PERMISSIONS_TABLE:
		fmid_column = PR_MEMBER_ID;
		break;
	}

	local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	ret = mapistore_folder_open_table(mstore_ctx, context_id, folder, local_mem_ctx, table_type, 0, &backend_table, &row_count);
	if (ret != MAPISTORE_SUCCESS) {
		goto end;
	}

	ret = mapistore_table_set_columns(mstore_ctx, context_id, backend_table, 1, &fmid_column);
	if (ret != MAPISTORE_SUCCESS) {
		goto end;
	}

	*child_fmid_count = row_count;
	fmids = talloc_array(mem_ctx, uint64_t, row_count);
	*child_fmids = fmids;
	current_fmid = fmids;
	for (i = 0; i < row_count; i++) {
		mapistore_table_get_row(mstore_ctx, context_id, backend_table, local_mem_ctx,
					MAPISTORE_PREFILTERED_QUERY, i, &row_data);
		*current_fmid = *(uint64_t *) row_data->data;
		current_fmid++;
	}

end:
	talloc_free(local_mem_ctx);

	return ret;
}

_PUBLIC_ enum mapistore_error mapistore_folder_get_child_fid_by_name(struct mapistore_context *mstore_ctx, uint32_t context_id, void *folder, const char *name, uint64_t *fidp)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_folder_get_child_fid_by_name(backend_ctx, folder, name, fidp);
}

_PUBLIC_ enum mapistore_error mapistore_folder_open_table(struct mapistore_context *mstore_ctx, uint32_t context_id,
							  void *folder, TALLOC_CTX *mem_ctx, enum mapistore_table_type table_type, uint32_t handle_id, void **table, uint32_t *row_count)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_folder_open_table(backend_ctx, folder, mem_ctx, table_type, handle_id, table, row_count);
}

_PUBLIC_ enum mapistore_error mapistore_folder_modify_permissions(struct mapistore_context *mstore_ctx, uint32_t context_id, void *folder, uint8_t flags, uint16_t pcount, struct PermissionData *permissions)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_folder_modify_permissions(backend_ctx, folder, flags, pcount, permissions);
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
enum mapistore_error mapistore_message_get_message_data(struct mapistore_context *mstore_ctx, uint32_t context_id, void *message, TALLOC_CTX *mem_ctx, struct mapistore_message **msg)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend modifyrecipients */
	return mapistore_backend_message_get_message_data(backend_ctx, message, mem_ctx, msg);
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
enum mapistore_error mapistore_message_modify_recipients(struct mapistore_context *mstore_ctx, uint32_t context_id, void *message, struct SPropTagArray *columns, uint16_t count, struct mapistore_message_recipient *recipients)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend modifyrecipients */
	return mapistore_backend_message_modify_recipients(backend_ctx, message, columns, count, recipients);
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
_PUBLIC_ enum mapistore_error mapistore_message_set_read_flag(struct mapistore_context *mstore_ctx, uint32_t context_id, void *message, uint8_t flag)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend savechangesmessage */
	return mapistore_backend_message_set_read_flag(backend_ctx, message, flag);
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
_PUBLIC_ enum mapistore_error mapistore_message_save(struct mapistore_context *mstore_ctx, uint32_t context_id, void *message)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend savechangesmessage */
	return mapistore_backend_message_save(backend_ctx, message);
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
_PUBLIC_ enum mapistore_error mapistore_message_submit(struct mapistore_context *mstore_ctx, uint32_t context_id,
						       void *message, enum SubmitFlags flags)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend submitmessage */
	return mapistore_backend_message_submit(backend_ctx, message, flags);
}

_PUBLIC_ enum mapistore_error mapistore_message_get_attachment_table(struct mapistore_context *mstore_ctx, uint32_t context_id,
								     void *message, TALLOC_CTX *mem_ctx, void **table, uint32_t *row_count)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_message_get_attachment_table(backend_ctx, message, mem_ctx, table, row_count);
}

_PUBLIC_ enum mapistore_error mapistore_message_open_attachment(struct mapistore_context *mstore_ctx, uint32_t context_id,
								void *message, TALLOC_CTX *mem_ctx, uint32_t aid, void **attachment)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_message_open_attachment(backend_ctx, message, mem_ctx, aid, attachment);
}

_PUBLIC_ enum mapistore_error mapistore_message_create_attachment(struct mapistore_context *mstore_ctx, uint32_t context_id,
								  void *message, TALLOC_CTX *mem_ctx, void **attachment, uint32_t *aid)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_message_create_attachment(backend_ctx, message, mem_ctx, attachment, aid);
}

_PUBLIC_ enum mapistore_error mapistore_message_attachment_open_embedded_message(struct mapistore_context *mstore_ctx, uint32_t context_id, void *message, TALLOC_CTX *mem_ctx, void **embedded_message, uint64_t *mid, struct mapistore_message **msg)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_message_attachment_open_embedded_message(backend_ctx, message, mem_ctx, embedded_message, mid, msg);
}

_PUBLIC_ enum mapistore_error mapistore_table_get_available_properties(struct mapistore_context *mstore_ctx, uint32_t context_id, void *table, TALLOC_CTX *mem_ctx, struct SPropTagArray **propertiesp)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_table_get_available_properties(backend_ctx, table, mem_ctx, propertiesp);
}

_PUBLIC_ enum mapistore_error mapistore_table_set_columns(struct mapistore_context *mstore_ctx, uint32_t context_id, void *table, uint16_t count, enum MAPITAGS *properties)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_table_set_columns(backend_ctx, table, count, properties);
}

_PUBLIC_ enum mapistore_error mapistore_table_set_restrictions(struct mapistore_context *mstore_ctx, uint32_t context_id, void *table, struct mapi_SRestriction *restrictions, uint8_t *table_status)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_table_set_restrictions(backend_ctx, table, restrictions, table_status);
}

_PUBLIC_ enum mapistore_error mapistore_table_set_sort_order(struct mapistore_context *mstore_ctx, uint32_t context_id, void *table, struct SSortOrderSet *sort_order, uint8_t *table_status)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_table_set_sort_order(backend_ctx, table, sort_order, table_status);
}

_PUBLIC_ enum mapistore_error mapistore_table_get_row(struct mapistore_context *mstore_ctx, uint32_t context_id, void *table, TALLOC_CTX *mem_ctx,
						      enum mapistore_query_type query_type, uint32_t rowid, struct mapistore_property_data **data)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_table_get_row(backend_ctx, table, mem_ctx, query_type, rowid, data);
}

_PUBLIC_ enum mapistore_error mapistore_table_get_row_count(struct mapistore_context *mstore_ctx, uint32_t context_id, void *table, enum mapistore_query_type query_type, uint32_t *row_countp)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_table_get_row_count(backend_ctx, table, query_type, row_countp);
}

_PUBLIC_ enum mapistore_error mapistore_table_handle_destructor(struct mapistore_context *mstore_ctx, uint32_t context_id, void *table, uint32_t handle_id)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_table_handle_destructor(backend_ctx, table, handle_id);
}

_PUBLIC_ enum mapistore_error mapistore_properties_get_available_properties(struct mapistore_context *mstore_ctx, uint32_t context_id, void *object, TALLOC_CTX *mem_ctx, struct SPropTagArray **propertiesp)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_properties_get_available_properties(backend_ctx, object, mem_ctx, propertiesp);
}


_PUBLIC_ enum mapistore_error mapistore_properties_get_properties(struct mapistore_context *mstore_ctx, uint32_t context_id,
								  void *object, TALLOC_CTX *mem_ctx,
								  uint16_t count, enum MAPITAGS *properties,
								  struct mapistore_property_data *data)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_properties_get_properties(backend_ctx, object, mem_ctx, count, properties, data);
}

_PUBLIC_ enum mapistore_error mapistore_properties_set_properties(struct mapistore_context
								  *mstore_ctx, uint32_t context_id,
								  void *object,
								  struct SRow *aRow)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_properties_set_properties(backend_ctx, object, aRow);
}
