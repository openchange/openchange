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


#include <sys/types.h>

#define __STDC_FORMAT_MACROS	1
#include <inttypes.h>

#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "mapistore_errors.h"
#include "mapistore.h"
#include "mapistore_common.h"
#include "mapistore_private.h"
#include <dlinklist.h>
#include "libmapi/libmapi_private.h"

#include <tdb.h>

char	*mapping_path = NULL;
char	*mapistore_dbpath = NULL;
char	*mapistore_firstorgdn = NULL;

/**
   \details Set the mapping path

   \param path pointer to the mapping path

   \note The mapping path can be set unless id_mapping_context is
   initialized. If path is NULL and mapping path is not yet
   initialized, then mapping_path will be reset to its default value
   when the initialization routine is called.

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTORE_ERROR mapistore_set_mapping_path(const char *path)
{
	TALLOC_CTX	*mem_ctx;
	DIR		*dir;

	/* Case 1. Path is set to NULL */
	if (!path) {
		if (mapping_path) {
			talloc_free(mapping_path);
		}
		mapping_path = NULL;
		return MAPISTORE_SUCCESS;
	}

	if (mapping_path) {
		talloc_free(mapping_path);
		mapping_path = NULL;
	}

	/* Case 2. path is initialized */

	/* Step 1. Check if path is valid path */
	dir = opendir(path);
	if (!dir) {
		return MAPISTORE_ERR_NO_DIRECTORY;
	}

	/* Step 2. TODO: Check for write permissions */

	if (closedir(dir) == -1) {
		/* FIXME: Should have a better error name here */
		return MAPISTORE_ERR_NO_DIRECTORY;
	}
	
	mem_ctx = talloc_autofree_context();
	mapping_path = talloc_strdup(mem_ctx, path);
	return MAPISTORE_SUCCESS;
}

/**
   \details Return the current mapping path

   \return pointer to the mapping path.
 */
const char *mapistore_get_mapping_path(void)
{
	return (!mapping_path) ? MAPISTORE_MAPPING_PATH : (const char *)mapping_path;
}


/**
   \details Set the mapistore.ldb mapping path

   \param dbname string pointer to the mapistore database path

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_set_database_path(const char *dbname)
{
	TALLOC_CTX	*mem_ctx;

	if (!dbname) {
		if (mapistore_dbpath) {
			talloc_free(mapistore_dbpath);
		}
		mapistore_dbpath = NULL;
		return MAPISTORE_SUCCESS;
	}

	if (mapistore_dbpath) {
		talloc_free(mapistore_dbpath);
		mapistore_dbpath = NULL;
	}

	mem_ctx = talloc_autofree_context();
	mapistore_dbpath = talloc_strdup(mem_ctx, dbname);

	return MAPISTORE_SUCCESS;
}


enum MAPISTORE_ERROR mapistore_set_firstorgdn(const char *firstou, const char *firstorg, const char *serverdn)
{
	TALLOC_CTX	*mem_ctx;

	if (mapistore_firstorgdn) {
		talloc_free(mapistore_firstorgdn);
	}

	mem_ctx = talloc_autofree_context();
	mapistore_firstorgdn = talloc_asprintf(mem_ctx, TMPL_MDB_FIRSTORGDN, firstou, firstorg, serverdn);
	if (!mapistore_firstorgdn) {
		DEBUG(5, ("! [%s:%d][%s]: Unable to allocate memory to set firstorgdn\n", 
			  __FILE__, __LINE__, __FUNCTION__));
		return MAPISTORE_ERR_NO_MEMORY;
	}

	return MAPISTORE_SUCCESS;
}

const char *mapistore_get_firstorgdn(void)
{
	return mapistore_firstorgdn;
}

/**
   \details Return the current path to mapistore.ldb database

   \return pointer to the mapistore database path
 */
const char *mapistore_get_database_path(void)
{
	return (!mapistore_dbpath) ? MAPISTORE_DBPATH : (const char *) mapistore_dbpath;
}





/**
   \details Initialize the ID mapping context or return the existing
   one if already initialized.

   \param pctx pointer to the processing context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_init_mapping_context(struct processing_context *pctx)
{
	TDB_DATA	key;
	TDB_DATA	dbuf;
	TALLOC_CTX	*mem_ctx;
	char		*dbpath;
	uint64_t	last_id;
	char		*tmp_buf;
	int		ret;

	/* mapistore_v2 */
	const char	*db_path;

	/* Sanity Checks */
	MAPISTORE_RETVAL_IF(!pctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(pctx->mapping_ctx, MAPISTORE_ERR_ALREADY_INITIALIZED, NULL);

	pctx->mapping_ctx = talloc_zero(pctx, struct id_mapping_context);
	MAPISTORE_RETVAL_IF(!pctx->mapping_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	mem_ctx = talloc_named(NULL, 0, "mapistore_init_mapping_context");

	/* Step 1. Retrieve the mapistore database path */
	db_path = mapistore_get_database_path();
	if (!db_path) {
		DEBUG(5, ("! [%s:%d][%s]: Unable to retrieve the mapistore database path\n", __FILE__, __LINE__, __FUNCTION__));
		talloc_free(mem_ctx);
		talloc_free(pctx->mapping_ctx);
		return MAPISTORE_ERR_DATABASE_INIT;
	}

	/* Step 2. Initialize tevent structure */
	pctx->mapping_ctx->ev = tevent_context_init(pctx);

	/* Step 3. Open a wrapped connection to mapistore.ldb */
	pctx->mapping_ctx->ldb_ctx = mapistore_ldb_wrap_connect(pctx, pctx->mapping_ctx->ev, db_path, 0);
	if (!pctx->mapping_ctx->ldb_ctx) {
		DEBUG(5, ("! [%s:%d][%s]: Failed to open wrapped connection over mapistore.ldb\n", __FILE__, __LINE__, __FUNCTION__));
		talloc_free(pctx->mapping_ctx);
		return MAPISTORE_ERR_DATABASE_INIT;
	}

	/* Open/Create the used ID database */
	if (!pctx->mapping_ctx->used_ctx) {
		dbpath = talloc_asprintf(mem_ctx, "%s/%s", mapistore_get_mapping_path(), MAPISTORE_DB_NAME_USED_ID);
		pctx->mapping_ctx->used_ctx = tdb_wrap_open(pctx, dbpath, 0, 0, O_RDWR|O_CREAT, 0600);
		talloc_free(dbpath);
		if (!pctx->mapping_ctx->used_ctx) {
			DEBUG(3, ("[%s:%d]: %s\n", __FUNCTION__, __LINE__, strerror(errno)));
			talloc_free(mem_ctx);
			talloc_free(pctx->mapping_ctx);
			return MAPISTORE_ERR_DATABASE_INIT;
		}
	}

	/* Retrieve the last ID value */
	key.dptr = (unsigned char *) MAPISTORE_DB_LAST_ID_KEY;
	key.dsize = strlen(MAPISTORE_DB_LAST_ID_KEY);

	dbuf = tdb_fetch(pctx->mapping_ctx->used_ctx->tdb, key);

	/* If the record doesn't exist, insert it */
	if (!dbuf.dptr || !dbuf.dsize) {
		dbuf.dptr = (unsigned char *) talloc_asprintf(mem_ctx, "0x%"PRIx64, (uint64_t) MAPISTORE_DB_LAST_ID_VAL);
		dbuf.dsize = strlen((const char *) dbuf.dptr);
		last_id = MAPISTORE_DB_LAST_ID_VAL;

		ret = tdb_store(pctx->mapping_ctx->used_ctx->tdb, key, dbuf, TDB_INSERT);
		talloc_free(dbuf.dptr);
		if (ret == -1) {
			DEBUG(3, ("[%s:%d]: Unable to create %s record: %s\n", __FUNCTION__, __LINE__,
				  MAPISTORE_DB_LAST_ID_KEY, tdb_errorstr(pctx->mapping_ctx->used_ctx->tdb)));
			talloc_free(mem_ctx);
			talloc_free(pctx->mapping_ctx);

			return MAPISTORE_ERR_DATABASE_OPS;
		}

	} else {
		tmp_buf = talloc_strndup(mem_ctx, (char *)dbuf.dptr, dbuf.dsize);
		free(dbuf.dptr);
		last_id = strtoull(tmp_buf, NULL, 16);
		talloc_free(tmp_buf);
	}

	pctx->mapping_ctx->last_id = last_id;

	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}


enum MAPISTORE_ERROR mapistore_write_ldif_string_to_store(struct processing_context *pctx, const char *ldif_string)
{
	struct ldb_context	*ldb_ctx;
	struct ldb_ldif		*ldif;
	int			ret;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!pctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!pctx->mapping_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!pctx->mapping_ctx->ldb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!ldif_string, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ldb_ctx = pctx->mapping_ctx->ldb_ctx;

	while ((ldif = ldb_ldif_read_string(ldb_ctx, (const char **)&ldif_string))) {
		ret = ldb_msg_normalize(ldb_ctx, ldif, ldif->msg, &ldif->msg);
		if (ret != LDB_SUCCESS) {
			ldb_ldif_read_free(ldb_ctx, ldif);
			DEBUG(5, ("! [%s:%d][%s]: Unable to normalize ldif\n", __FILE__, __LINE__, __FUNCTION__));
			return MAPISTORE_ERROR;
		}
		ret = ldb_add(ldb_ctx, ldif->msg);
		if (ret != LDB_SUCCESS) {
			ldb_ldif_read_free(ldb_ctx, ldif);
			DEBUG(5, ("! [%s:%d][%s]: Unable to add ldb msg\n", __FILE__, __LINE__, __FUNCTION__));
			return MAPISTORE_ERROR;
		}
		ldb_ldif_read_free(ldb_ctx, ldif);
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Retrieve the next available folder or message identifier

   \param pctx pointer to the mapistore processing context
   \param username the username's mailbox to retrieve GlobalCount from
   \param fmid pointer to the first available fmid within the range

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_get_new_fmid(struct processing_context *pctx,
					    const char *username,
					    uint64_t *fmid)
{
	int			ret;
	TALLOC_CTX		*mem_ctx;
	struct ldb_context	*ldb_ctx;
	struct ldb_result	*res = NULL;
	struct ldb_message	*msg;
	const char * const	attrs[] = { "*", NULL };
	uint64_t		ReplicaID;
	uint64_t		GlobalCount;
	uint64_t		new_GlobalCount;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!pctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!pctx->mapping_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!pctx->mapping_ctx->ldb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "mapistore_get_new_fmid");

	/* Step 1. Retrieve the server object */
	ldb_ctx = pctx->mapping_ctx->ldb_ctx;

	/* TODO: In the future, we may want to deal with different servers */
	/* TODO: Use ldb_transaction to lock between get/set operations */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(&(cn=%s)(objectClass=store))", username);
	if (ret != LDB_SUCCESS || res->count != 1) {
		talloc_free(mem_ctx);
		DEBUG(5, ("! [%s:%d][%s]: Unable to find the user store object\n", __FILE__, __LINE__, __FUNCTION__));
		return MAPISTORE_ERR_NOT_FOUND;
	}

	/* Step 2. Get the [48 GlobalCount][16 ReplicaID] */
	GlobalCount = ldb_msg_find_attr_as_uint64(res->msgs[0], "GlobalCount", 0);
	new_GlobalCount = GlobalCount + 1;

	ReplicaID = ldb_msg_find_attr_as_uint64(res->msgs[0], "ReplicaID", 0);

	/* Step 3. Update the GlobalCount */
	msg = ldb_msg_new(mem_ctx);
	msg->dn = ldb_dn_copy(msg, ldb_msg_find_attr_as_dn(ldb_ctx, mem_ctx, res->msgs[0], "distinguishedName"));
	ldb_msg_add_fmt(msg, "GlobalCount", "0x%"PRIx64, new_GlobalCount);
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;
	ret = ldb_modify(ldb_ctx, msg);
	if (ret != LDB_SUCCESS) {
		talloc_free(mem_ctx);
		DEBUG(5, ("! [%s:%d][%s]: Unable to update GlobalCount for server object\n", __FILE__, __LINE__, __FUNCTION__));
		return MAPISTORE_ERROR;
	}

	/* Step 4. Set the fmid value */

	talloc_free(mem_ctx);
	*fmid = (GlobalCount << 16) + ReplicaID;

	return MAPISTORE_SUCCESS;
}


/**
   \details Retrieve an allocation range

   \param pctx pointer to the mapistore processing context
   \param username the user store from which to retrieve the new allocation range
   \param range the range to allocate
   \param range_start pointer to the first allocation range fmid to return
   \param range_end pointer to the last allocation range fmid to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_get_new_allocation_range(struct processing_context *pctx,
							const char *username,
							uint64_t range,
							uint64_t *range_start,
							uint64_t *range_end)
{
	int			ret;
	TALLOC_CTX		*mem_ctx;
	struct ldb_context	*ldb_ctx;
	struct ldb_result	*res = NULL;
	struct ldb_message	*msg;
	const char * const	attrs[] = { "*", NULL };
	uint64_t		ReplicaID;
	uint64_t		GlobalCount;
	uint64_t		new_GlobalCount;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!pctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!pctx->mapping_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!pctx->mapping_ctx->ldb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!range_start || !range_end, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "mapistore_get_next_range");

	/* Step 1. Retrieve the user store object */
	ldb_ctx = pctx->mapping_ctx->ldb_ctx;
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(&(objectClass=store)(cn=%s))", username);
	if (ret != LDB_SUCCESS || res->count != 1) {
		talloc_free(mem_ctx);
		DEBUG(5, ("! [%s:%d][%s]: Unable to find the user store object\n", __FILE__, __LINE__, __FUNCTION__));
		return MAPISTORE_ERR_NOT_FOUND;
	}

	/* Step 2. Get the current GlobalCount and set the new one */
	GlobalCount = ldb_msg_find_attr_as_uint64(res->msgs[0], "GlobalCount", 0);
	ReplicaID = ldb_msg_find_attr_as_uint64(res->msgs[0], "ReplicaID", 0);

	*range_start = GlobalCount;

	new_GlobalCount = GlobalCount + range -1;
	*range_end = new_GlobalCount;

	new_GlobalCount += 1;

	/* Step 3. Update the GlobalCount */
	msg = ldb_msg_new(mem_ctx);
	msg->dn = ldb_dn_copy(msg, ldb_msg_find_attr_as_dn(ldb_ctx, mem_ctx, res->msgs[0], "distinguishedName"));
	ldb_msg_add_fmt(msg, "GlobalCount", "0x%"PRIx64, new_GlobalCount);
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;
	ret = ldb_modify(ldb_ctx, msg);
	if (ret != LDB_SUCCESS) {
		talloc_free(mem_ctx);
		DEBUG(5, ("! [%s:%d][%s]: Unable to update GlobalCount for server object\n", __FILE__, __LINE__, __FUNCTION__));
		return MAPISTORE_ERROR;
	}

	/* Step 4. Set the fmid value ranges */
	talloc_free(mem_ctx);
	*range_start = (*range_start << 16) + ReplicaID;
	*range_end = (*range_end << 16) + ReplicaID;

	return MAPISTORE_SUCCESS;
}


/**
   \details Retrieve the mapistore URI for a given user mailbox

   \param pctx pointer to the processing context
   \param username pointer to the username to lookup
   \param mapistore_uri pointer on pointer to the mapistore URI

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_get_mailbox_uri(struct processing_context *pctx,
					       const char *username,
					       char **mapistore_uri)
{
	int			ret;
	TALLOC_CTX		*mem_ctx;
	const char		*uri;
	const char * const	attrs[] = { "*", NULL };
	struct ldb_context	*ldb_ctx;
	struct ldb_result	*res;
	struct ldb_dn		*dn;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!pctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mapistore_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_new(pctx);
	ldb_ctx = pctx->mapping_ctx->ldb_ctx;

	dn = ldb_dn_new_fmt(mem_ctx, ldb_ctx, TMPL_MDB_USERSTORE, username, mapistore_get_firstorgdn());
	ret = ldb_search(ldb_ctx, mem_ctx, &res, dn, LDB_SCOPE_SUBTREE, attrs, "(&(objectClass=mailbox)(objectClass=container))");
	if (ret != LDB_SUCCESS || res->count != 1) {
		talloc_free(mem_ctx);
		DEBUG(5, ("! [%s:%d][%s]: Unable to find the user mailbox root container\n", __FILE__, __LINE__, __FUNCTION__));
		return MAPISTORE_ERR_NOT_FOUND;
	}

	uri = ldb_msg_find_attr_as_string(res->msgs[0], "mapistore_uri", NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_NOT_FOUND, mem_ctx);

	*mapistore_uri = talloc_strdup(pctx, (char *)uri);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Return an unused or new context identifier

   \param pctx pointer to the processing context
   \param context_id pointer to the context identifier the function
   returns

   \return a non zero context identifier on success, otherwise 0.
 */
enum MAPISTORE_ERROR mapistore_get_context_id(struct processing_context *pctx, uint32_t *context_id)
{
	struct context_id_list	*el;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!pctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	/* Step 1. The free context list doesn't exist yet */
	if (!pctx->free_ctx) {
		pctx->last_context_id++;
		*context_id = pctx->last_context_id;
	}

	/* Step 2. We have a free list */
	for (el = pctx->free_ctx; el; el = el->next) {
		if (el->context_id) {
			*context_id = el->context_id;
			DLIST_REMOVE(pctx->free_ctx, el);
			break;
		}
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Add a context identifier to the list

   \param pctx pointer to the processing context
   \param context_id the identifier referencing the context to free

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_free_context_id(struct processing_context *pctx, uint32_t context_id)
{
	struct context_id_list	*el;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!pctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	/* Step 1. Ensure the list is not corrupted */
	for (el = pctx->free_ctx; el; el = el->next) {
		if (el->context_id == context_id) {
			return MAPISTORE_ERR_CORRUPTED;
		}
	}

	/* Step 2. Create the element and add it to the list */
	el = talloc_zero((TALLOC_CTX *)pctx, struct context_id_list);
	el->context_id = context_id;
	DLIST_ADD_END(pctx->free_ctx, el, struct context_id_list *);

	return MAPISTORE_SUCCESS;
}
