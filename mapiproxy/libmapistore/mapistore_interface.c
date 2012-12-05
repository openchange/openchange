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
#include <time.h>

#include "mapistore.h"
#include "mapistore_errors.h"
#include "mapistore_private.h"
#include "mapistore_nameid.h"

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

#if 0
	mstore_ctx->mq_ipc = mq_open(MAPISTORE_MQUEUE_IPC, O_WRONLY|O_NONBLOCK|O_CREAT, 0755, NULL);
	if (mstore_ctx->mq_ipc == -1) {
		DEBUG(0, ("[%s:%d]: Failed to open mqueue for %s\n", __FUNCTION__, __LINE__, MAPISTORE_MQUEUE_IPC));
		talloc_free(mstore_ctx);
		return NULL;
	}
#endif

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

/* TODO: the "owner" parameter should be deduced from the uri */
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
								  void *target_folder, void *source_folder, TALLOC_CTX *mem_ctx, uint32_t mid_count, uint64_t *source_mids, uint64_t *target_mids, struct Binary_r **target_change_keys, uint8_t want_copy)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_folder_move_copy_messages(backend_ctx, target_folder, source_folder, mem_ctx, mid_count, source_mids, target_mids, target_change_keys, want_copy);
}

/**

 */
_PUBLIC_ enum mapistore_error mapistore_folder_move_folder(struct mapistore_context *mstore_ctx, uint32_t context_id,
							   void *move_folder, void *target_folder, const char *new_folder_name)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_folder_move_folder(backend_ctx, move_folder, target_folder, new_folder_name);
}


/**

 */
_PUBLIC_ enum mapistore_error mapistore_folder_copy_folder(struct mapistore_context *mstore_ctx, uint32_t context_id,
							   void *move_folder, void *target_folder, bool recursive, const char *new_folder_name)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_folder_copy_folder(backend_ctx, move_folder, target_folder, recursive, new_folder_name);
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
_PUBLIC_ enum mapistore_error mapistore_folder_get_deleted_fmids(struct mapistore_context *mstore_ctx, uint32_t context_id, void *folder, TALLOC_CTX *mem_ctx, enum mapistore_table_type table_type, uint64_t change_num, struct UI8Array_r **fmidsp, uint64_t *cnp)
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

_PUBLIC_ enum mapistore_error mapistore_folder_preload_message_bodies(struct mapistore_context *mstore_ctx, uint32_t context_id, void *folder, enum mapistore_table_type table_type, const struct UI8Array_r *mids)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_folder_preload_message_bodies(backend_ctx, folder, table_type, mids);
}

/* freebusy helper */
static int mapistore_days_in_month(int month, int year)
{
	static int	max_mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int		dec_year, days;

	if (month == 1) {
		dec_year = year % 100;
		if ((dec_year == 0
		     && ((((year + 1900) / 100) % 4) == 0))
		    || (dec_year % 4) == 0) {
			days = 29;
		}
		else {
			days = max_mdays[month];
		}
	}
	else {
		days = max_mdays[month];
	}

	return days;
}

static int mapistore_mins_in_ymon(uint32_t ymon)
{
	return mapistore_days_in_month((ymon & 0xf) - 1, ymon >> 4) * 24 * 60;
}

static inline void mapistore_freebusy_make_range(struct tm *start_time, struct tm *end_time)
{
	time_t							now;
	struct tm						time_data;
	int							mw_delta, month;

	/* (from OXOPFFB - 3.1.4.1.1)
	   Start of range is 12:00 A.M. UTC on the first day of the month or the first day of the week, whichever occurs earlier at the time of publishing.
	   End of range is calculated by adding the value of the PidTagFreeBusyCountMonths property ([MS-OXOCAL] section 2.2.12.1) to start of range.

	   Since PidTagFreeBusyCountMonths is not supported yet, we use a count of 3 months
	*/

	now = time(NULL);
	time_data = *gmtime(&now);
	time_data.tm_hour = 0;
	time_data.tm_min = 0;
	time_data.tm_sec = 0;

	/* take the first day of the week OR the first day of the month */
	month = time_data.tm_mon;
	if (time_data.tm_mday < 7) {
		mw_delta = (time_data.tm_wday + 1 - time_data.tm_mday);
		if (mw_delta > 0) {
			if (time_data.tm_mon > 0) {
				time_data.tm_mon--;
			}
			else {
				time_data.tm_mon = 11;
				time_data.tm_year--;
			}
			time_data.tm_mday = mapistore_days_in_month(time_data.tm_mon, time_data.tm_year) + 1 - mw_delta;
		}
		else {
			time_data.tm_mday = 1;
		}
	}
	else {
		mw_delta = 0;
		time_data.tm_mday = 1;
	}

	*start_time = time_data;

	time_data.tm_mon = month + 2;
	if (time_data.tm_mon > 11) {
		time_data.tm_year++;
		time_data.tm_mon -= 12;
	}
	time_data.tm_mday = mapistore_days_in_month(time_data.tm_mon, time_data.tm_year) + 1 - mw_delta;
	time_data.tm_hour = 23;
	time_data.tm_min = 59;
	time_data.tm_sec = 59;

	*end_time = time_data;
}

static void mapistore_freebusy_convert_filetime(struct FILETIME *ft_value, uint32_t *ymon, uint32_t *mins)
{
	NTTIME		nt_time;
	time_t		u_time;
	struct tm	*gm_time;

	nt_time = ((NTTIME) ft_value->dwHighDateTime << 32) | ft_value->dwLowDateTime;
	u_time = nt_time_to_unix(nt_time);
	gm_time = gmtime(&u_time);

	*ymon = ((gm_time->tm_year + 1900) << 4) | (gm_time->tm_mon + 1);
	*mins = gm_time->tm_min + (gm_time->tm_hour + ((gm_time->tm_mday - 1) * 24)) * 60;
}

static uint16_t mapistore_freebusy_find_month_range(uint32_t ymon, uint32_t *months_ranges, uint16_t nbr_months, bool *overflow)
{
	uint16_t	range;

	if (nbr_months > 0) {
		if (months_ranges[0] > ymon) {
			*overflow = true;
			return 0;
		}
		else {
			if (months_ranges[nbr_months - 1] < ymon) {
				*overflow = true;
				return (nbr_months - 1);
			}
			else {
				*overflow = false;
				for (range = 0; range < nbr_months; range++) {
					if (months_ranges[range] == ymon) {
						return range;
					}
				}
			}
		}
	}

	return (uint16_t) -1;
}

/* TODO: both following methods could be merged. This would certainly enhance performance by avoiding to wander through long arrays multiple times */
static void mapistore_freebusy_fill_fbarray(uint8_t **minutes_array, uint32_t *months_ranges, uint16_t nbr_months, struct FILETIME *start, struct FILETIME *end)
{
	uint32_t	i, max, start_ymon, start_mins, end_ymon, end_mins;
	uint16_t	start_mr_idx, end_mr_idx;
	bool		start_range_overflow, end_range_overflow;

	mapistore_freebusy_convert_filetime(start, &start_ymon, &start_mins);
	mapistore_freebusy_convert_filetime(end, &end_ymon, &end_mins);

	start_mr_idx = mapistore_freebusy_find_month_range(start_ymon, months_ranges, nbr_months, &start_range_overflow);
	if (start_range_overflow) {
		start_mins = 0;
	}
	end_mr_idx = mapistore_freebusy_find_month_range(end_ymon, months_ranges, nbr_months, &end_range_overflow);
	if (end_range_overflow) {
		end_mins = mapistore_mins_in_ymon(end_ymon);
	}

	/* head */
	if (end_mr_idx > start_mr_idx) {
		/* end occurs after start range */

		/* middle */
		for (i = start_mr_idx + 1; i < end_mr_idx; i++) {
			memset(minutes_array[i], 1, mapistore_mins_in_ymon(months_ranges[i]));
		}

		/* tail */
		memset(minutes_array[end_mr_idx], 1, end_mins);

		max = mapistore_mins_in_ymon(start_ymon); /* = max chunk for first range */
	}
	else {
		/* end occurs on same range as start */

		max = end_mins;
	}
	memset(minutes_array[start_mr_idx] + start_mins, 1, (max - start_mins));
}

static const int	max_mins_per_month = 31 * 24 * 60;

static void mapistore_freebusy_compile_fbarray(TALLOC_CTX *mem_ctx, uint8_t *minutes_array, struct Binary_r *fb_bin)
{
	int			i;
	bool			filled;
	struct ndr_push		*ndr;
	TALLOC_CTX		*local_mem_ctx;

	local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	ndr = ndr_push_init_ctx(local_mem_ctx);

	filled = (minutes_array[0] != 0);
	if (filled) {
		ndr_push_uint16(ndr, NDR_SCALARS, 0);
	}

	for (i = 1; i < max_mins_per_month; i++) {
		if (filled && !minutes_array[i]) {
			ndr_push_uint16(ndr, NDR_SCALARS, (i - 1));
			filled = false;
		}
		else if (!filled && minutes_array[i]) {
			ndr_push_uint16(ndr, NDR_SCALARS, i);
			filled = true;
		}
	}
	if (filled) {
		ndr_push_uint16(ndr, NDR_SCALARS, (max_mins_per_month - 1));
	}

	fb_bin->cb = ndr->offset;
	fb_bin->lpb = ndr->data;
	(void) talloc_reference(mem_ctx, fb_bin->lpb);

	talloc_free(local_mem_ctx);
}

static void mapistore_freebusy_merge_subarray(uint8_t *minutes_array, uint8_t *included_array)
{
	int i;

	for (i = 0; i < max_mins_per_month; i++) {
		if (included_array[i]) {
			minutes_array[i] = 1;
		}
	}
}

enum mapistore_error mapistore_folder_fetch_freebusy_properties(struct mapistore_context *mstore_ctx, uint32_t context_id, void *folder, struct tm *start_tm, struct tm *end_tm, TALLOC_CTX *mem_ctx, struct mapistore_freebusy_properties **fb_props_p)
{
	enum mapistore_error			ret;
	struct mapistore_freebusy_properties	*fb_props;
	struct backend_context			*backend_ctx;
	TALLOC_CTX				*local_mem_ctx;
	void					*table;
	uint32_t				row_count;
	struct SPropTagArray			*props;
	struct mapistore_property_data		*row_data;
	struct mapi_SRestriction		and_res;
	uint8_t					state;
	struct tm				local_start_tm, local_end_tm;
	time_t					start_time, end_time;
	NTTIME					nt_time;
	struct mapi_SRestriction_and		time_restrictions[2];
	int					i, month, nbr_months;
	uint8_t					**minutes_array, **free_array, **tentative_array, **busy_array, **oof_array;
	char					*tz;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	/* fetch events from this month for 3 months: start + enddate + fbstatus */
	ret = mapistore_folder_open_table(mstore_ctx, context_id, folder, local_mem_ctx, MAPISTORE_MESSAGE_TABLE, 0, &table, &row_count);
	if (ret != MAPISTORE_SUCCESS) {
		goto end;
	}

	fb_props = talloc_zero(local_mem_ctx, struct mapistore_freebusy_properties);

	/* fetch freebusy range */
	if (start_tm && end_tm) {
		local_start_tm = *start_tm;
		local_end_tm = *end_tm;
	}
	else {
		mapistore_freebusy_make_range(&local_start_tm, &local_end_tm);
	}

	unix_to_nt_time(&nt_time, time(NULL));
	fb_props->timestamp.dwLowDateTime = (nt_time & 0xffffffff);
	fb_props->timestamp.dwHighDateTime = nt_time >> 32;

	tz = getenv("TZ");
	setenv("TZ", "", 1);
	tzset();
	start_time = mktime(&local_start_tm);
	end_time = mktime(&local_end_tm);
	if (tz) {
		setenv("TZ", tz, 1);
	}
	else {
		unsetenv("TZ");
	}
	tzset();

	/* setup restriction */
	and_res.rt = RES_AND;
	and_res.res.resAnd.cRes = 2;
	and_res.res.resAnd.res = time_restrictions;

	time_restrictions[0].rt = RES_PROPERTY;
	time_restrictions[0].res.resProperty.relop = RELOP_GE;
	time_restrictions[0].res.resProperty.ulPropTag = PidLidAppointmentEndWhole; 
	time_restrictions[0].res.resProperty.lpProp.ulPropTag = PidLidAppointmentEndWhole;
	unix_to_nt_time(&nt_time, start_time);
	time_restrictions[0].res.resProperty.lpProp.value.ft.dwLowDateTime = (nt_time & 0xffffffff);
	time_restrictions[0].res.resProperty.lpProp.value.ft.dwHighDateTime = nt_time >> 32;
	fb_props->publish_start = (uint32_t) (nt_time / (60 * 10000000));

	time_restrictions[1].rt = RES_PROPERTY;
	time_restrictions[1].res.resProperty.relop = RELOP_LE;
	time_restrictions[1].res.resProperty.ulPropTag = PidLidAppointmentStartWhole; 
	time_restrictions[1].res.resProperty.lpProp.ulPropTag = PidLidAppointmentStartWhole;
	unix_to_nt_time(&nt_time, end_time);
	time_restrictions[1].res.resProperty.lpProp.value.ft.dwLowDateTime = (nt_time & 0xffffffff);
	time_restrictions[1].res.resProperty.lpProp.value.ft.dwHighDateTime = nt_time >> 32;
	fb_props->publish_end = (uint32_t) (nt_time / (60 * 10000000));

	mapistore_table_set_restrictions(mstore_ctx, context_id, table, &and_res, &state);

	/* setup table columns */
	props = talloc_zero(local_mem_ctx, struct SPropTagArray);
	props->cValues = 3;
	props->aulPropTag = talloc_array(props, enum MAPITAGS, props->cValues);
	props->aulPropTag[0] = PidLidAppointmentStartWhole;
	props->aulPropTag[1] = PidLidAppointmentEndWhole;
	props->aulPropTag[2] = PidLidBusyStatus;
	mapistore_table_set_columns(mstore_ctx, context_id, table, props->cValues, props->aulPropTag);

	/* setup months arrays */
	if (local_start_tm.tm_year == local_end_tm.tm_year) {
		nbr_months = (local_end_tm.tm_mon - local_start_tm.tm_mon + 1);
	}
	else {
		nbr_months = (12 - local_start_tm.tm_mon) + local_end_tm.tm_mon + 1;
	}
	fb_props->months_ranges = talloc_array(fb_props, uint32_t, nbr_months);
	if (local_start_tm.tm_year == local_end_tm.tm_year) {
		for (i = 0; i < nbr_months; i++) {
			fb_props->months_ranges[i] = ((local_start_tm.tm_year + 1900) << 4) + (local_start_tm.tm_mon + 1 + i);
		}
	}
	else {
		month = local_start_tm.tm_mon;
		i = 0;
		while (month < 12) {
			fb_props->months_ranges[i] = ((local_start_tm.tm_year + 1900) << 4) + month + 1;
			i++;
			month++;
		}
		month = 0;
		while (month < local_end_tm.tm_mon) {
			fb_props->months_ranges[i] = ((local_end_tm.tm_year + 1900) << 4) + month + 1;
			i++;
			month++;
		}
		fb_props->months_ranges[i] = ((local_end_tm.tm_year + 1900) << 4) + month + 1;
	}

	/* fetch events and fill freebusy arrays */
	free_array = talloc_array(local_mem_ctx, uint8_t *, nbr_months);
	tentative_array = talloc_array(local_mem_ctx, uint8_t *, nbr_months);
	busy_array = talloc_array(local_mem_ctx, uint8_t *, nbr_months);
	oof_array = talloc_array(local_mem_ctx, uint8_t *, nbr_months);
	for (i = 0; i < nbr_months; i++) {
		free_array[i] = talloc_array(free_array, uint8_t, max_mins_per_month);
		memset(free_array[i], 0, max_mins_per_month);
		tentative_array[i] = talloc_array(tentative_array, uint8_t, max_mins_per_month);
		memset(tentative_array[i], 0, max_mins_per_month);
		busy_array[i] = talloc_array(tentative_array, uint8_t, max_mins_per_month);
		memset(busy_array[i], 0, max_mins_per_month);
		oof_array[i] = talloc_array(tentative_array, uint8_t, max_mins_per_month);
		memset(oof_array[i], 0, max_mins_per_month);
	}

	i = 0;
	while (mapistore_table_get_row(mstore_ctx, context_id, table, local_mem_ctx, MAPISTORE_PREFILTERED_QUERY, i, &row_data) == MAPISTORE_SUCCESS) {
		if (row_data[0].error == MAPISTORE_SUCCESS && row_data[1].error == MAPISTORE_SUCCESS && row_data[2].error == MAPISTORE_SUCCESS) {
			switch (*((uint32_t *) row_data[2].data)) {
			case olFree:
				minutes_array = free_array;
				break;
			case olTentative:
				minutes_array = tentative_array;
				break;
			case olBusy:
				minutes_array = busy_array;
				break;
			case olOutOfOffice:
				minutes_array = oof_array;
				break;
			default:
				minutes_array = NULL;
			}
			if (minutes_array) {
				mapistore_freebusy_fill_fbarray(minutes_array, fb_props->months_ranges, nbr_months, row_data[0].data, row_data[1].data);
			}
		}
		i++;
	}

        /* compile minutes array into arrays of ranges */
	fb_props->nbr_months = nbr_months;
	fb_props->freebusy_free = talloc_array(fb_props, struct Binary_r, nbr_months);
	fb_props->freebusy_tentative = talloc_array(fb_props, struct Binary_r, nbr_months);
	fb_props->freebusy_busy = talloc_array(fb_props, struct Binary_r, nbr_months);
	fb_props->freebusy_away = talloc_array(fb_props, struct Binary_r, nbr_months);
	fb_props->freebusy_merged = talloc_array(fb_props, struct Binary_r, nbr_months);
	for (i = 0; i < nbr_months; i++) {
		mapistore_freebusy_compile_fbarray(fb_props, free_array[i], fb_props->freebusy_free + i);
		mapistore_freebusy_compile_fbarray(fb_props, tentative_array[i], fb_props->freebusy_tentative + i);
		mapistore_freebusy_compile_fbarray(fb_props, busy_array[i], fb_props->freebusy_busy + i);
		mapistore_freebusy_compile_fbarray(fb_props, oof_array[i], fb_props->freebusy_away + i);
		mapistore_freebusy_merge_subarray(busy_array[i], oof_array[i]);
		mapistore_freebusy_compile_fbarray(fb_props, busy_array[i], fb_props->freebusy_merged + i);
	}

	*fb_props_p = fb_props;

	/* we bind fb_props to mem_ctx, because it will be released with the local_mem_ctx */
	(void) talloc_reference(mem_ctx, fb_props);

	ret = MAPISTORE_SUCCESS;

end:
	talloc_free(local_mem_ctx);

	return ret;
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

_PUBLIC_ enum mapistore_error mapistore_message_attachment_open_embedded_message(struct mapistore_context *mstore_ctx, uint32_t context_id, void *attachment, TALLOC_CTX *mem_ctx, void **embedded_message, uint64_t *mid, struct mapistore_message **msg)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_message_attachment_open_embedded_message(backend_ctx, attachment, mem_ctx, embedded_message, mid, msg);
}

_PUBLIC_ enum mapistore_error mapistore_message_attachment_create_embedded_message(struct mapistore_context *mstore_ctx, uint32_t context_id, void *attachment, TALLOC_CTX *mem_ctx, void **embedded_message, struct mapistore_message **msg)
{
	struct backend_context	*backend_ctx;

	/* Sanity checks */
	MAPISTORE_SANITY_CHECKS(mstore_ctx, NULL);

	/* Step 1. Search the context */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 2. Call backend operation */
	return mapistore_backend_message_attachment_create_embedded_message(backend_ctx, attachment, mem_ctx, embedded_message, msg);
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

_PUBLIC_ enum MAPISTATUS mapistore_error_to_mapi(enum mapistore_error mapistore_err)
{
	enum MAPISTATUS mapi_err;

	switch(mapistore_err) {
	case MAPISTORE_SUCCESS:
		mapi_err = MAPI_E_SUCCESS;
		break;
	case MAPISTORE_ERROR:
		mapi_err = MAPI_E_NO_SUPPORT;
		break;
	case MAPISTORE_ERR_NO_MEMORY:
		mapi_err = MAPI_E_NOT_ENOUGH_MEMORY;
		break;
	case MAPISTORE_ERR_ALREADY_INITIALIZED:
	case MAPISTORE_ERR_NOT_INITIALIZED:
		mapi_err = MAPI_E_NOT_INITIALIZED;
		break;
	case MAPISTORE_ERR_CORRUPTED:
		mapi_err = MAPI_E_CORRUPT_STORE;
		break;
	case MAPISTORE_ERR_INVALID_PARAMETER:
		mapi_err = MAPI_E_INVALID_PARAMETER;
		break;
	case MAPISTORE_ERR_NO_DIRECTORY:
	case MAPISTORE_ERR_DATABASE_INIT:
	case MAPISTORE_ERR_DATABASE_OPS:
	case MAPISTORE_ERR_BACKEND_REGISTER:
	case MAPISTORE_ERR_BACKEND_INIT:
	case MAPISTORE_ERR_CONTEXT_FAILED:
	case MAPISTORE_ERR_INVALID_NAMESPACE:
		mapi_err = MAPI_E_DISK_ERROR;
		break;
	case MAPISTORE_ERR_NOT_FOUND:
		mapi_err = MAPI_E_NOT_FOUND;
		break;
	case MAPISTORE_ERR_REF_COUNT:
		mapi_err = MAPI_E_COLLISION;
		break;
	case MAPISTORE_ERR_EXIST:
		mapi_err = MAPI_E_COLLISION;
		break;
	case MAPISTORE_ERR_INVALID_DATA:
	case MAPISTORE_ERR_MSG_SEND:
	case MAPISTORE_ERR_MSG_RCV:
		mapi_err = MAPI_E_DISK_ERROR;
		break;
	case MAPISTORE_ERR_DENIED:
		mapi_err = MAPI_E_NO_ACCESS;
		break;
	case MAPISTORE_ERR_NOT_IMPLEMENTED:
		mapi_err = MAPI_E_NOT_IMPLEMENTED;
		break;
	default:
		DEBUG (4, ("[%s] unknown mapistore error: %.8x\n", __PRETTY_FUNCTION__, mapistore_err));
		mapi_err = MAPI_E_INVALID_PARAMETER;
	}

	return mapi_err;
}
