/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Note: init and load functions have been copied from
   samba4/source4/param/util.c initially written by Jelmer.

   Copyright (C) Jelmer Vernooij 2005-2007
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
   \file mapistore_backend_public.c

   \brief Provides a public API mapistore backends can use to interact
   with mapistore internals. Some functions are just wrappers over
   existing mapistore functions.
 */

#include "mapistore_errors.h"
#include "mapistore.h"
#include "mapistore_common.h"
#include "mapistore_private.h"
#include "mapistore_backend.h"


/**
   \details Open a wrapped context to a LDB database

   This function is a wrapper to mapistore_ldb_wrap_connect which
   helps keeping mapistore_backend_context opaque to backends.

   \param ctx pointer to the mapistore backend opaque context
   \param path the path to the database (mapistore.ldb) to open

   \return valid LDB context pointer on success, otherwise NULL
 */
struct ldb_context *mapistore_public_ldb_connect(struct mapistore_backend_context *ctx,
						 const char *path)
{
	struct mapistore_context	*mstore_ctx = (struct mapistore_context *)ctx;
	struct tevent_context		*ev;

	/* Sanity checks */
	if (!path || !mstore_ctx) {
		return NULL;
	}

	ev = tevent_context_init(mstore_ctx);
	if (!ev) return NULL;

	return mapistore_ldb_wrap_connect(mstore_ctx, ev, path, 0);
}


/**
   \details Let a backend checks if a message of folder is already
   indexed in mapistore.ldb

   \param ctx pointer to the mapistore backend opaque context
   \param username the username where to look for the URI
   \param mapistore_uri the mapistore URI to lookup

   \return MAPISTORE_ERR_EXIST if the URI was found,
   MAPISTORE_ERR_NOT_FOUND if it wasn't, other MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_exist(struct mapistore_backend_context *ctx,
				     const char *username,
				     const char *mapistore_uri)
{
	enum MAPISTORE_ERROR		retval;
	struct mapistore_context	*mstore_ctx;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!ctx->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mapistore_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mstore_ctx = ctx->mstore_ctx;

	/* Step 1. Create an indexing context */
	retval = mapistore_indexing_context_add(mstore_ctx,
						username,
						&(mstore_ctx->mapistore_indexing_list));
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Step 2. Search the URI */
	retval = mapistore_indexing_record_search_uri(mstore_ctx->mapistore_indexing_list, mapistore_uri);

	/* Step 3. Delete indexing context */
	mapistore_indexing_context_del(mstore_ctx, username);

	return retval;
}


/**
   \details Return a mapistore URI stripped from its namespace

   \param uri the URI to strip
   \param stripped_uri pointer on pointer to the stripped URI to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_strip_ns_from_uri(const char *uri, const char **stripped_uri)
{
	char		*tmp;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!stripped_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 1. Search for :// which terminates any valid namespace */
	tmp = strstr(uri, MAPISTORE_NAMESPACE_COMMON);
	MAPISTORE_RETVAL_IF(!tmp, MAPISTORE_ERR_NOT_FOUND, NULL);
	MAPISTORE_RETVAL_IF(!tmp[strlen(MAPISTORE_NAMESPACE_COMMON)+1], MAPISTORE_ERR_INVALID_NAMESPACE, NULL);

	*stripped_uri = &tmp[strlen(MAPISTORE_NAMESPACE_COMMON)];

	return MAPISTORE_SUCCESS;
}


/**
   \details Let backends register a folder and index it within
   mapistore indexing database

   \param ctx pointer to the mapistore backend opaque context
   \param username the username used to register the folder
   \param parent_uri the mapistore URI of the parent folder
   \param mapistore_uri the mapistore URI to register
   \param range the number of message IDs we want to reserve for this
   folder

   \return MAPISTORE success on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_register_folder(struct mapistore_backend_context *ctx,
					       const char *username,
					       const char *parent_uri,
					       const char *mapistore_uri,
					       uint64_t range)
{
	enum MAPISTORE_ERROR		retval;
	struct mapistore_context	*mstore_ctx;
	uint64_t			parent_fid;
	uint64_t			fid;
	uint64_t			rstart;
	uint64_t			rend;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!ctx->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mapistore_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	if (!range) {
		range = MAPISTORE_INDEXING_DFLT_ALLOC_RANGE_VAL;
	}

	mstore_ctx = ctx->mstore_ctx;

	/* Step 1. Ensure the URI doesn't exist */
	retval = mapistore_exist(ctx, username, mapistore_uri);
	MAPISTORE_RETVAL_IF(retval != MAPISTORE_ERR_NOT_FOUND, retval, NULL);

	/* Step 2. Create or retrieve an indexing context */
	retval = mapistore_indexing_context_add(mstore_ctx, username, &(mstore_ctx->mapistore_indexing_list));
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Step 3. Ensure the parent_fid URI exists and retrieve its
	 * folder identifier value */
	retval = mapistore_indexing_get_record_fmid_by_uri(mstore_ctx->mapistore_indexing_list, parent_uri, &parent_fid);
	if (retval) goto finish;

	/* Step 4. Ask for a new FID */
	retval = mapistore_get_new_fmid(mstore_ctx->processing_ctx, username, &fid);
	if (retval) goto finish;

	/* Step 5. Register the folder within the indexing database */
	retval = mapistore_indexing_add_fmid_record(mstore_ctx->mapistore_indexing_list, fid, 
						    mapistore_uri, parent_fid, 
						    MAPISTORE_INDEXING_FOLDER);
	if (retval) goto finish;

	/* Step 6. Request an allocation range for messages */
	retval = mapistore_get_new_allocation_range(mstore_ctx->processing_ctx,
						    username, range, &rstart, &rend);
	if (retval) goto finish;

	/* Step 7. Set the allocation range for the folder */
	retval = mapistore_indexing_add_folder_record_allocation_range(mstore_ctx->mapistore_indexing_list,
								       fid, rstart, rend);
	if (retval) goto finish;

	/* Step 8. Delete the indexing context */
finish:
	retval = mapistore_indexing_context_del(mstore_ctx, username);
	return retval;
}


/**
   \details Retrieve the folder or message identifier matching the
   mapistore_uri within mapistore indexing database

   \param ctx pointer to the mapistore backend opaque context
   \param username the username used to register the folder
   \param mapistore_uri the mapistore URI to lookup
   \param fmid pointer to the folder or message identifier to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_get_identifier(struct mapistore_backend_context *ctx,
					      const char *username,
					      const char *mapistore_uri,
					      uint64_t *fmid)
{
	enum MAPISTORE_ERROR		retval;
	struct mapistore_context	*mstore_ctx;
	uint64_t			_fmid = 0;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!ctx->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mapistore_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mstore_ctx = ctx->mstore_ctx;

	/* Step 1. Ensure the URI exists */
	retval = mapistore_exist(ctx, username, mapistore_uri);
	MAPISTORE_RETVAL_IF(retval != MAPISTORE_ERR_EXIST, retval, NULL);

	/* Step 2. Create or retrieve an indexing context */
	retval = mapistore_indexing_context_add(mstore_ctx, username, &(mstore_ctx->mapistore_indexing_list));
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Step 3. Retrieve the folder or message identifier
	 * associated to the URI */
	retval = mapistore_indexing_get_record_fmid_by_uri(mstore_ctx->mapistore_indexing_list, mapistore_uri, &_fmid);
	if (retval) goto finish;

	*fmid = _fmid;

	/* Step 4. Delete the indexing context */
finish:
	retval = mapistore_indexing_context_del(mstore_ctx, username);
	return retval;
}
