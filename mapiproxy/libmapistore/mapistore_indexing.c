/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010
   Copyright (C) Carlos Pérez-Aradros Herce 2014
   Copyright (C) Jesús García Sáez 2014

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
   \file mapistore_indexing.c

   \brief MAPISTORE internal indexing functions
   
   This file contains functionality to map between folder / message
   identifiers and backend URI strings.
 */
#include "mapistore.h"
#include "mapistore_errors.h"
#include "mapistore_private.h"
#include "backends/indexing_tdb.h"
#include "backends/indexing_mysql.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"


char *default_indexing_url = NULL;

/**
   \details Set the default backend url. If none is set, a tdb file per user
   will be used.

   \param url default backend url to be used
 */
_PUBLIC_ void mapistore_set_default_indexing_url(const char *url)
{
	TALLOC_CTX *mem_ctx;

	if (default_indexing_url) talloc_free(default_indexing_url);

	if (url == NULL) {
		default_indexing_url = NULL;
	} else {
		mem_ctx = talloc_autofree_context();
		default_indexing_url = talloc_strdup(mem_ctx, url);
	}
}

/**
   \details Search the indexing record matching the username

   \param mstore_ctx pointer to the mapistore context
   \param username the username to lookup

   \return pointer to the tdb_wrap structure on success, otherwise NULL
 */
struct indexing_context *mapistore_indexing_search(struct mapistore_context *mstore_ctx,
						   const char *username)
{
	struct indexing_context_list	*el;

	/* Sanity checks */
	if (!mstore_ctx) return NULL;
	if (!mstore_ctx->indexing_list) return NULL;
	if (!username) return NULL;

	for (el = mstore_ctx->indexing_list; el; el = el->next) {
		/* TODO: extract url from backend mapping, by the moment we use the username */
		if (el && el->ctx && !strcmp(el->ctx->url, username)) {
			return el->ctx;
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
_PUBLIC_ enum mapistore_error mapistore_indexing_add(struct mapistore_context *mstore_ctx, 
						     const char *username,
						     struct indexing_context **ictxp)
{
	struct indexing_context_list *ictx;
	const char *indexing_url;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->conn_info, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 1. Search if the context already exists */
	*ictxp = mapistore_indexing_search(mstore_ctx, username);
	MAPISTORE_RETVAL_IF(*ictxp, MAPISTORE_SUCCESS, NULL);

	// indexing context has not been found, let's create it.
	indexing_url = openchangedb_get_indexing_url(mstore_ctx->conn_info->oc_ctx,
						     username);
	if (indexing_url == NULL) {
		indexing_url = default_indexing_url;
	}

	// indexing_url NULL means to use the default backend: tdb
	if (indexing_url == NULL) {
		ictx = talloc_zero(mstore_ctx, struct indexing_context_list);
		mapistore_indexing_tdb_init(mstore_ctx, username, &ictx->ctx);
	} else if (strncmp(indexing_url, "mysql://", strlen("mysql://")) == 0) {
		ictx = talloc_zero(mstore_ctx, struct indexing_context_list);
		mapistore_indexing_mysql_init(mstore_ctx, username, indexing_url,
					      &ictx->ctx);
	} else {
		DEBUG(0, ("ERROR unknown indexing url %s\n", indexing_url));
		return MAPISTORE_ERROR;
	}

	/* ictx->ref_count = 0; */
	DLIST_ADD_END(mstore_ctx->indexing_list, ictx, struct indexing_context_list *);

	*ictxp = ictx->ctx;
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
enum mapistore_error mapistore_indexing_record_add_fmid(struct mapistore_context *mstore_ctx,
							uint32_t context_id,
							const char *username,
							uint64_t fmid, int type)
{
	int				ret;
	struct backend_context		*backend_ctx;
	struct indexing_context		*ictx;
	char				*mapistore_URI = NULL;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!context_id, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERROR, NULL);

	/* Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!backend_ctx->indexing, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Check if the fid/mid doesn't already exist within the database */
	ret = mapistore_indexing_add(mstore_ctx, username, &ictx);
	MAPISTORE_RETVAL_IF(ret, ret, NULL);
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);

	/* Retrieve the mapistore URI given context_id and fmid */
	mapistore_backend_get_path(NULL, backend_ctx, fmid, &mapistore_URI);
	/* DEBUG(0, ("mapistore_URI = %s\n", mapistore_URI)); */
	MAPISTORE_RETVAL_IF(!mapistore_URI, MAPISTORE_ERROR, NULL);


	/* Add the record given its fid and mapistore_uri */
	switch(type) {
	case MAPISTORE_FOLDER:
	case MAPISTORE_MESSAGE:
		ret = ictx->add_fmid(ictx, username, fmid, mapistore_URI);
		break;
	default:
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}
	talloc_free(mapistore_URI);

	return ret;
}


/**
   \details Remove a folder or message record from the indexing database

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier referencing the indexing
   database to update
   \param fmid the folder or message ID to delete
   \param flags the type of deletion MAPISTORE_SOFT_DELETE or MAPISTORE_PERMANENT_DELETE
   \param type MAPISTORE_FOLDER or MAPISTORE_MESSAGE

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum mapistore_error mapistore_indexing_record_del_fmid(struct mapistore_context *mstore_ctx,
							uint32_t context_id, const char *username, uint64_t fmid,
							uint8_t flags, int type)
{
	int				ret;
	struct backend_context		*backend_ctx;
	struct indexing_context		*ictx;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!context_id, MAPISTORE_ERR_INVALID_CONTEXT, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_BACKEND, NULL);
	MAPISTORE_RETVAL_IF(!backend_ctx->indexing, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Check if the fid/mid still exists within the database */
	ret = mapistore_indexing_add(mstore_ctx, username, &ictx);
	MAPISTORE_RETVAL_IF(ret, ret, NULL);
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);

	switch(type) {
	case MAPISTORE_FOLDER:
	case MAPISTORE_MESSAGE:
		ret = ictx->del_fmid(ictx, username, fmid, flags);
		break;
	default:
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	return ret;
}

/**
   \details Returns record data

   \param mstore_ctx pointer to the mapistore context
   \param username the name of the account where to look for the
   indexing database
   \param mem_ctx pointer to the memory context
   \param fmid the fmid/key to the record
   \param urip pointer to the uri pointer
   \param soft_deletedp pointer to the soft deleted pointer

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_indexing_record_get_uri(struct mapistore_context *mstore_ctx, const char *username, TALLOC_CTX *mem_ctx, uint64_t fmid, char **urip, bool *soft_deletedp)
{
	struct indexing_context	*ictx;
	int			ret;
	
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!urip, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!soft_deletedp, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Check if the fmid exists within the database */
	ret = mapistore_indexing_add(mstore_ctx, username, &ictx);
	MAPISTORE_RETVAL_IF(ret, ret, NULL);
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);

	return ictx->get_uri(ictx, username, mem_ctx, fmid, urip, soft_deletedp);
}

_PUBLIC_ enum mapistore_error mapistore_indexing_record_get_fmid(struct mapistore_context *mstore_ctx, const char *username, const char *uri, bool partial, uint64_t *fmidp, bool *soft_deletedp)
{
	struct indexing_context		*ictx;
	int				ret;

	/* SANITY checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!fmidp, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!soft_deletedp, MAPISTORE_ERR_INVALID_PARAMETER, NULL);


	ret = mapistore_indexing_add(mstore_ctx, username, &ictx);
	MAPISTORE_RETVAL_IF(ret, ret, NULL);
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);

	return ictx->get_fmid(ictx, username, uri, partial, fmidp, soft_deletedp);
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
_PUBLIC_ enum mapistore_error mapistore_indexing_record_add_fid(struct mapistore_context *mstore_ctx,
								uint32_t context_id, const char *username, uint64_t fid)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	return mapistore_indexing_record_add_fmid(mstore_ctx, context_id, username, fid, MAPISTORE_FOLDER);
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
_PUBLIC_ enum mapistore_error mapistore_indexing_record_del_fid(struct mapistore_context *mstore_ctx,
								uint32_t context_id, const char *username, uint64_t fid, 
								uint8_t flags)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	return mapistore_indexing_record_del_fmid(mstore_ctx, context_id, username, fid, flags, MAPISTORE_FOLDER);
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
_PUBLIC_ enum mapistore_error mapistore_indexing_record_add_mid(struct mapistore_context *mstore_ctx,
								uint32_t context_id, const char *username, uint64_t mid)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	return mapistore_indexing_record_add_fmid(mstore_ctx, context_id, username, mid, MAPISTORE_MESSAGE);
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
_PUBLIC_ enum mapistore_error mapistore_indexing_record_del_mid(struct mapistore_context *mstore_ctx,
								uint32_t context_id, const char *username, uint64_t mid,
								uint8_t flags)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	return mapistore_indexing_record_del_fmid(mstore_ctx, context_id, username, mid, flags, MAPISTORE_MESSAGE);
}

static enum mapistore_error mapistore_indexing_allocate_fid(struct mapistore_context *mstore_ctx,
							    const char *username,
							    uint64_t range_len, uint64_t *fid)
{
	enum mapistore_error ret;
	struct indexing_context	*ictx;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!fid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ictx = mapistore_indexing_search(mstore_ctx, username);
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ret = ictx->allocate_fmids(ictx, username, range_len, fid);
	MAPISTORE_RETVAL_IF(ret != MAPISTORE_SUCCESS, ret, NULL);

	return MAPISTORE_SUCCESS;
}

/**
   \details Allocates a new FolderID for a specific user and returns it

   \param mstore_ctx pointer to the mapistore context
   \param username name of the mailbox
   \param fid pointer to the fid value the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_indexing_get_new_folderID_as_user(struct mapistore_context *mstore_ctx,
									  const char *username, uint64_t *fid)
{
	enum mapistore_error ret;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!fid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ret = mapistore_indexing_allocate_fid(mstore_ctx, username, 1, fid);
	MAPISTORE_RETVAL_IF(ret != MAPISTORE_SUCCESS, ret, NULL);

	*fid = (exchange_globcnt(*fid) << 16) | 0x0001;

	return MAPISTORE_SUCCESS;
}

/**
   \details Allocates a new FolderID and returns it

   \param mstore_ctx pointer to the mapistore context
   \param fid pointer to the fid value the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_indexing_get_new_folderID(struct mapistore_context *mstore_ctx,
								  uint64_t *fid)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->conn_info, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!fid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	return mapistore_indexing_get_new_folderID_as_user(mstore_ctx, mstore_ctx->conn_info->username, fid);
}

/**
   \details Allocates a batch of new folder ids and returns them

   \param mstore_ctx pointer to the mapistore context
   \param mem_ctx memory context where the fid will be allocated
   \param max number of fids to allocate
   \param fids_p pointer array of fids values the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_indexing_get_new_folderIDs(struct mapistore_context *mstore_ctx,
								   TALLOC_CTX *mem_ctx,
								   uint64_t max,
								   struct UI8Array_r **fids_p)
{
	uint64_t fid;
	uint64_t count;
	enum mapistore_error ret;
	struct UI8Array_r *fids;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->conn_info, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!fids_p, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ret = mapistore_indexing_allocate_fid(mstore_ctx, mstore_ctx->conn_info->username, max, &fid);
	MAPISTORE_RETVAL_IF(ret != MAPISTORE_SUCCESS, ret, NULL);

	fids = talloc_zero(mem_ctx, struct UI8Array_r);
	fids->cValues = max;
	fids->lpui8 = talloc_array(fids, uint64_t, max);

	for (count = 0; count < max; count++) {
		fids->lpui8[count] = (exchange_globcnt(fid + count) << 16) | 0x0001;
	}

	*fids_p = fids;

	return MAPISTORE_SUCCESS;
}

/**
   \details Reserve a range of FMID

   \param mstore_ctx pointer to the mapistore context
   \param range_len size of the range of fmids to reserve
   \param first_fmidp pointer to the first reserved fid value the function
   returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_indexing_reserve_fmid_range(struct mapistore_context *mstore_ctx,
								    uint64_t range_len,
								    uint64_t *first_fmidp)
{
	enum mapistore_error ret;
	uint64_t fmid;
	struct indexing_context	*ictx;
	const char *username;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->conn_info, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!first_fmidp, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	username = mstore_ctx->conn_info->username;
	ictx = mapistore_indexing_search(mstore_ctx, username);
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ret = ictx->allocate_fmids(ictx, username, range_len, &fmid);
	MAPISTORE_RETVAL_IF(ret != MAPISTORE_SUCCESS, ret, NULL);

	*first_fmidp = (exchange_globcnt(fmid) << 16) | 0x0001;

	return MAPISTORE_SUCCESS;
}
