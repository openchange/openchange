/*
   OpenChange Storage Abstraction Layer library

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

#define __STDC_FORMAT_MACROS	1
#include <inttypes.h>

#include <string.h>

#include "mapistore_errors.h"
#include "mapistore.h"
#include "mapistore_private.h"
#include <dlinklist.h>
#include "libmapi/libmapi_private.h"

#include <tdb.h>

/**
   \details Search the indexing record matching the username

   \param mstore_ctx pointer to the mapistore context
   \param username the username to lookup

   \return pointer to the tdb_wrap structure on success, otherwise NULL
 */
struct indexing_context_list *mapistore_indexing_search(struct mapistore_context *mstore_ctx, 
							const char *username)
{
	struct indexing_context_list	*el;

	/* Sanity checks */
	if (!mstore_ctx) return NULL;
	if (!username) return NULL;

	for (el = mstore_ctx->indexing_list; el; el = el->next) {
		if (el && el->username && !strcmp(el->username, username)) {
			return el;
		}
	}

	return NULL;
}

/**
   \details Open connection to indexing database for a given user

   \param mstore_ctx pointer to the mapistore context
   \param username name for which the indexing database has to be
   created

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_indexing_add(struct mapistore_context *mstore_ctx, 
						     const char *username)
{
	TALLOC_CTX			*mem_ctx;
	struct indexing_context_list	*ictx;
	char				*dbpath = NULL;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERROR, NULL);

	/* Step 1. Search if the context already exists */
	ictx = mapistore_indexing_search(mstore_ctx, username);
	MAPISTORE_RETVAL_IF(ictx, MAPISTORE_SUCCESS, NULL);

	mem_ctx = talloc_named(NULL, 0, "mapistore_indexing_init");
	ictx = talloc_zero(mstore_ctx->indexing_list, struct indexing_context_list);

	/* Step 1. Open/Create the indexing database */
	dbpath = talloc_asprintf(mem_ctx, "%s/%s/indexing.tdb", 
				 mapistore_get_mapping_path(), username);
	ictx->index_ctx = tdb_wrap_open(ictx, dbpath, 0, 0, O_RDWR|O_CREAT, 0600);
	talloc_free(dbpath);
	if (!ictx->index_ctx) {
		DEBUG(3, ("[%s:%d]: %s\n", __FUNCTION__, __LINE__, strerror(errno)));
		talloc_free(ictx);
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_DATABASE_INIT;
	}
	ictx->username = talloc_strdup(ictx, username);
	ictx->ref_count = 0;
	DLIST_ADD_END(mstore_ctx->indexing_list, ictx, struct indexing_context_list *);

	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}


/**
   \details Close connection to indexing database for a given user

   \param mstore_ctx pointer to the mapistore context
   \param username name for which the indexing database has to be
   deleted

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_indexing_del(struct mapistore_context *mstore_ctx,
				    const char *username)
{
	struct indexing_context_list	*ictx;
	
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERROR, NULL);

	/* Step 1. Search for the context */
	ictx = mapistore_indexing_search(mstore_ctx, username);
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);

	/* Step 2. Return if ref_count is not 0 */
	MAPISTORE_RETVAL_IF(ictx->ref_count, MAPISTORE_SUCCESS, NULL);

	/* Step 3. Remove it from the list and free if 0 */
	DLIST_REMOVE(mstore_ctx->indexing_list, ictx);
	talloc_free(ictx);

	return MAPISTORE_SUCCESS;
}


/**
   \details Increase the ref count associated to a given indexing context

   \param ictx pointer to the indexing context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
enum MAPISTORE_ERROR mapistore_indexing_add_ref_count(struct indexing_context_list *ictx)
{
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);

	ictx->ref_count += 1;

	return MAPISTORE_SUCCESS;
}


/**
   \details Decrease the ref count associated to a given indexing context

   \param ictx pointer to the indexing context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
enum MAPISTORE_ERROR mapistore_indexing_del_ref_count(struct indexing_context_list *ictx)
{
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!ictx->ref_count, MAPISTORE_SUCCESS, NULL);

	ictx->ref_count -= 1;

	return MAPISTORE_SUCCESS;
}


/**
   \details Convenient function to check if the folder/message ID
   passed in parameter already exists in the database or not and
   whether it is soft deleted or not

   \param ictx pointer to the indexing context
   \param fmid folder/message ID to lookup
   \param IsSoftDeleted pointer to boolean returned by the function
   which indicates whether the record is soft_deleted or not

   \return MAPISTORE_SUCCESS if the folder/message ID doesn't exist,
   otherwise MAPISTORE_ERR_EXIST.
 */
enum MAPISTORE_ERROR mapistore_indexing_search_existing_fmid(struct indexing_context_list *ictx, 
							     uint64_t fmid, bool *IsSoftDeleted)
{
	int		ret;
	TDB_DATA	key;

	/* Sanity */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERROR, NULL);

	key.dptr = (unsigned char *) talloc_asprintf(ictx, "0x%.16"PRIx64, fmid);
	key.dsize = strlen((const char *)key.dptr);
	*IsSoftDeleted = false;

	ret = tdb_exists(ictx->index_ctx->tdb, key);
	talloc_free(key.dptr);

	/* If it doesn't exist look for a SOFT_DELETED entry */
	if (!ret) {
		key.dptr = (unsigned char *) talloc_asprintf(ictx, "%s0x%.16"PRIx64,
							     MAPISTORE_SOFT_DELETED_TAG,
							     fmid);
		key.dsize = strlen((const char *)key.dptr);
		ret = tdb_exists(ictx->index_ctx->tdb, key);
		if (ret) {
			*IsSoftDeleted = true;
		}
	}

	MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERR_EXIST, NULL);
	
	return MAPISTORE_SUCCESS;
}


/**
   \details Add a folder or message record to the indexing database

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the indexing
   database to update
   \param fmid the folder or message ID to add
   \param type MAPISTORE_FOLDER or MAPISTORE_MESSAGE

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_indexing_record_add_fmid(struct mapistore_context *mstore_ctx,
							uint32_t context_id,  uint64_t fmid, 
							uint8_t type)
{
	enum MAPISTORE_ERROR		ret;
	int				retval;
	struct backend_context		*backend_ctx;
	struct indexing_context_list	*ictx;
	TDB_DATA			key;
	TDB_DATA			dbuf;
	char				*mapistore_URI = NULL;
	bool				IsSoftDeleted = false;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!context_id, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERROR, NULL);

	/* Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!backend_ctx->indexing, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Check if the fid/mid doesn't already exist within the database */
	ictx = backend_ctx->indexing;
	ret = mapistore_indexing_search_existing_fmid(ictx, fmid, &IsSoftDeleted);
	MAPISTORE_RETVAL_IF(ret, ret, NULL);

	/* Retrieve the mapistore URI given context_id and fmid */
	mapistore_get_path(backend_ctx, fmid, type, &mapistore_URI);
	DEBUG(0, ("mapistore_URI = %s\n", mapistore_URI));
	MAPISTORE_RETVAL_IF(!mapistore_URI, MAPISTORE_ERROR, NULL);

	/* Add the record given its fid and mapistore_uri */
	key.dptr = (unsigned char *) talloc_asprintf(mstore_ctx, "0x%.16"PRIx64, fmid);
	key.dsize = strlen((const char *) key.dptr);

	dbuf.dptr = (unsigned char *) talloc_strdup(mstore_ctx, mapistore_URI);
	dbuf.dsize = strlen((const char *) dbuf.dptr);

	retval = tdb_store(ictx->index_ctx->tdb, key, dbuf, TDB_INSERT);
	talloc_free(key.dptr);
	talloc_free(dbuf.dptr);
	talloc_free(mapistore_URI);

	if (retval == -1) {
		DEBUG(3, ("[%s:%d]: Unable to create 0x%.16"PRIx64" record: %s\n", __FUNCTION__, __LINE__,
			  fmid, mapistore_URI));
		return MAPISTORE_ERR_DATABASE_OPS;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Remove a folder or message record from the indexing database

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the indexing
   database to update
   \param fmid the folder or message ID to delete
   \flags the type of deletion MAPISTORE_SOFT_DELETE or MAPISTORE_PERMANENT_DELETE

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_indexing_record_del_fmid(struct mapistore_context *mstore_ctx,
							uint32_t context_id, uint64_t fmid,
							uint8_t flags)
{
	enum MAPISTORE_ERROR		ret;
	int				retval;
	struct backend_context		*backend_ctx;
	struct indexing_context_list	*ictx;
	TDB_DATA			key;
	TDB_DATA			newkey;
	TDB_DATA			dbuf;
	bool				IsSoftDeleted = false;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!context_id, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERROR, NULL);

	/* Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!backend_ctx->indexing, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Check if the fid/mid still exists within the database */
	ictx = backend_ctx->indexing;
	ret = mapistore_indexing_search_existing_fmid(ictx, fmid, &IsSoftDeleted);
	MAPISTORE_RETVAL_IF(!ret, ret, NULL);

	if (IsSoftDeleted == true) {
		key.dptr = (unsigned char *) talloc_asprintf(mstore_ctx, "%s0x%.16"PRIx64, 
							     MAPISTORE_SOFT_DELETED_TAG, fmid);
	} else {
		key.dptr = (unsigned char *) talloc_asprintf(mstore_ctx, "0x%.16"PRIx64, fmid);
	}
	key.dsize = strlen((const char *) key.dptr);

	switch (flags) {
	case MAPISTORE_SOFT_DELETE:
		/* nothing to do if the record is already soft deleted */
		MAPISTORE_RETVAL_IF(IsSoftDeleted == true, MAPISTORE_SUCCESS, NULL);
		newkey.dptr = (unsigned char *) talloc_asprintf(mstore_ctx, "%s0x%.16"PRIx64, 
								MAPISTORE_SOFT_DELETED_TAG,
								fmid);
		newkey.dsize = strlen ((const char *)newkey.dptr);
		/* Retrieve previous value */
		dbuf = tdb_fetch(ictx->index_ctx->tdb, key);
		/* Add new record */
		retval = tdb_store(ictx->index_ctx->tdb, newkey, dbuf, TDB_INSERT);
		/* Delete previous record */
		retval = tdb_delete(ictx->index_ctx->tdb, key);
		talloc_free(key.dptr);
		talloc_free(newkey.dptr);
		break;
	case MAPISTORE_PERMANENT_DELETE:
		retval = tdb_delete(ictx->index_ctx->tdb, key);
		talloc_free(key.dptr);
		MAPISTORE_RETVAL_IF(retval, MAPISTORE_ERR_DATABASE_OPS, NULL);
		break;
	}
	
	return MAPISTORE_SUCCESS;
}


/**
   \details Retrieve the list of parent folder identifiers until we
   reach the expected item

   \param mstore_ctx pointer to the mapistore context
   \param username the name of the account where to look for the
   indexing database
   \param fmid the folder/message ID to search
   \param parents pointer to an array of elements that subsequently
   leads to fmid returned by the function
   \param count pointer to the number of parents the function returns

   \note This function is useful for the emsmdb provider when we are
   trying to open a folder/message using an InputHandleIdx referencing
   the store object. In such situation, no context ID is available
   since the store object access openchange.ldb.

   It means we need to retrieve the list of folders to the item within
   the mapistore context and rely on an existing parent (if opened) or
   opens everything from the top parent if none is available.

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR

   \sa mapistore_indexing_record_open_fmid
 */
_PUBLIC_ int mapistore_indexing_get_folder_list(struct mapistore_context *mstore_ctx,
						const char *username, uint64_t fmid,
						struct indexing_folders_list **_flist)
{
	int				ret;
	struct indexing_context_list	*ictx;
	struct indexing_folders_list	*flist = NULL;
	TDB_DATA			key;
	TDB_DATA			dbuf;
	bool				IsSoftDeleted = false;
	char				*uri = NULL;
	char				*tmp_uri;
	char				*substr;
	char				*folder;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!_flist, MAPISTORE_ERROR, NULL);
	/* MAPISTORE_RETVAL_IF(!parents, MAPISTORE_ERROR, NULL); */
	/* MAPISTORE_RETVAL_IF(!count, MAPISTORE_ERROR, NULL); */

	/* 1. Search for an existing indexing context */
	ictx = mapistore_indexing_search(mstore_ctx, username);
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);

	/* 2. Ensure the fid/mid exist within the indexing database */
	ret = mapistore_indexing_search_existing_fmid(ictx, fmid, &IsSoftDeleted);
	DEBUG(0, ("ret = %d\n", ret));
	MAPISTORE_RETVAL_IF(!ret, MAPISTORE_ERROR, NULL);

	/* 3. Retrieve the mapistore_uri */
	if (IsSoftDeleted == true) {
		key.dptr = (unsigned char *) talloc_asprintf(mstore_ctx, "%s0x%.16"PRIx64,
							     MAPISTORE_SOFT_DELETED_TAG, fmid);
	} else {
		key.dptr = (unsigned char *) talloc_asprintf(mstore_ctx, "0x%.16"PRIx64, fmid);
		DEBUG(0, ("Search for record 0x%.16"PRIx64"\n", fmid));
	}
	key.dsize = strlen((const char *) key.dptr);

	dbuf = tdb_fetch(ictx->index_ctx->tdb, key);
	talloc_free(key.dptr);

	uri = talloc_strndup(mstore_ctx, (const char *)dbuf.dptr, dbuf.dsize);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERROR, NULL);
	DEBUG(0, ("uri = %s\n", uri));

	/* FIXME: Look for folders starting with 0x ... nasty but will do the trick for now */

	flist = talloc_zero(mstore_ctx, struct indexing_folders_list);
	flist->folderID = talloc_array(flist, uint64_t, 2);
	flist->count = 0;
	
	tmp_uri = uri;
	while ((substr = strcasestr(uri, "0x")) != NULL) {
		folder = talloc_strndup(mstore_ctx, substr, 18);
		flist->folderID[flist->count] = strtoull(folder, NULL, 16);
		if (flist->folderID[flist->count] != fmid) {
			flist->count += 1;
			flist->folderID = talloc_realloc(flist, flist->folderID, uint64_t, flist->count + 1);
		} else {
			flist->folderID[flist->count] = 0;
		}
		talloc_free(folder);
		uri = substr + 18;
	}

	talloc_free(tmp_uri);

	*_flist = flist;

	return MAPISTORE_SUCCESS;
}


/**
   \details Add a fid record to the indexing database

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the indexing
   database to update
   \param fid the fid to add

   \note This is a wrapper to the internal common
   mapistore_indexing_record_add_fmid function.

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_indexing_record_add_fid(struct mapistore_context *mstore_ctx,
					       uint32_t context_id, uint64_t fid)
{
	return mapistore_indexing_record_add_fmid(mstore_ctx, context_id, fid, MAPISTORE_FOLDER);
}


/**
   \details Delete a fid record from the indexing database

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the indexing
   database to update
   \param fid the fid to remove
   \param flags the type of deletion MAPISTORE_SOFT_DELETE or
   MAPISTORE_PERMANENT_DELETE

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_indexing_record_del_fid(struct mapistore_context *mstore_ctx,
					       uint32_t context_id, uint64_t fid, 
					       uint8_t flags)
{
	return mapistore_indexing_record_del_fmid(mstore_ctx, context_id, fid, flags);
}


/**
   \details Add a mid record to the indexing database

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the indexing
   database to update
   \param mid the mid to add

   \note This is a wrapper to the internal common
   mapistore_indexing_record_add_fmid function.

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_indexing_record_add_mid(struct mapistore_context *mstore_ctx,
					       uint32_t context_id, uint64_t mid)
{
	return mapistore_indexing_record_add_fmid(mstore_ctx, context_id, mid, MAPISTORE_MESSAGE);
}


/**
   \details Delete a mid record from the indexing database

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the indexing
   database to update
   \param mid the mid to remove
   \param flags the type of deletion MAPISTORE_SOFT_DELETE or
   MAPISTORE_PERMANENT_DELETE

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_indexing_record_del_mid(struct mapistore_context *mstore_ctx,
					       uint32_t context_id, uint64_t mid,
					       uint8_t flags)
{
	return mapistore_indexing_record_del_fmid(mstore_ctx, context_id, mid, flags);
}
