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

/**
   \file mapistore_indexing.c

   \brief MAPISTORE internal indexing functions
   
   This file contains functionality to map between folder / message
   identifiers and backend URI strings.
 */

#include <string.h>

#include "mapistore.h"
#include "mapistore_errors.h"
#include "mapistore_private.h"
#include "backends/indexing_tdb.h"

#include <dlinklist.h>
#include "libmapi/libmapi_private.h"
#include <tdb.h>

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
	struct indexing_context_list	*ictx;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERROR, NULL);

	/* Step 1. Search if the context already exists */
	*ictxp = mapistore_indexing_search(mstore_ctx, username);
	MAPISTORE_RETVAL_IF(*ictxp, MAPISTORE_SUCCESS, NULL);

	ictx = talloc_zero(mstore_ctx, struct indexing_context_list);
	mapistore_indexing_tdb_init(mstore_ctx, username, &ictx->ctx);

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
	MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);

	/* Retrieve the mapistore URI given context_id and fmid */
	mapistore_backend_get_path(backend_ctx, NULL, fmid, &mapistore_URI);
	/* DEBUG(0, ("mapistore_URI = %s\n", mapistore_URI)); */
	MAPISTORE_RETVAL_IF(!mapistore_URI, MAPISTORE_ERROR, NULL);


	/* Add the record given its fid and mapistore_uri */
	switch(type) {
	case MAPISTORE_FOLDER:
		ret = ictx->add_fid(ictx, username, fmid, mapistore_URI);
		break;
	case MAPISTORE_MESSAGE:
		ret = ictx->add_mid(ictx, username, fmid, mapistore_URI);
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
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!context_id, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERROR, NULL);

	/* Ensure the context exists */
	backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!backend_ctx->indexing, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Check if the fid/mid still exists within the database */
	ret = mapistore_indexing_add(mstore_ctx, username, &ictx);
	MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);

	switch(type) {
	case MAPISTORE_FOLDER:
		ret = ictx->del_fid(ictx, username, fmid, flags);
		break;
	case MAPISTORE_MESSAGE:
		ret = ictx->del_mid(ictx, username, fmid, flags);
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
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!urip, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!soft_deletedp, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	/* Check if the fmid exists within the database */
	ret = mapistore_indexing_add(mstore_ctx, username, &ictx);
	MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);

	return ictx->get_uri(ictx, username, mem_ctx, fmid, urip, soft_deletedp);
}

_PUBLIC_ enum mapistore_error mapistore_indexing_record_get_fmid(struct mapistore_context *mstore_ctx, const char *username, const char *uri, bool partial, uint64_t *fmidp, bool *soft_deletedp)
{
	struct indexing_context		*ictx;
	int				ret;

	/* SANITY checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!fmidp, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!soft_deletedp, MAPISTORE_ERR_NOT_INITIALIZED, NULL);


	ret = mapistore_indexing_add(mstore_ctx, username, &ictx);
	MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERROR, NULL);
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
	return mapistore_indexing_record_del_fmid(mstore_ctx, context_id, username, mid, flags, MAPISTORE_MESSAGE);
}
