/*
   MAPI Proxy - Indexing backend MySQL implementation

   OpenChange Project

   Copyright (C) Carlos Pérez-Aradros Herce 2014
   Copyright (C) Jesús García Sáez 2014
   Copyright (C) Julien Kerihuel 2015

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

#include <string.h>

#include "../mapistore.h"
#include "../mapistore_private.h"
#include "../mapistore_errors.h"
#include "indexing_mysql.h"
#include "libmapi/libmapi_private.h"
#include <mysql/mysql.h>
#include "../../util/mysql.h"
#include "../../util/ccan/hash/hash.h"
#include "../../util/schema_migration.h"
#include "mapiproxy/libmapiproxy/backends/openchangedb_mysql.h"

#include <talloc.h>
#include <libmemcached/memcached.h>

#define MYSQL(context)	((MYSQL *)context->data)


/**
   \details Generate key for FMID cache

   \param mem_ctx pointer to the memory context
   \param uri pointer to the uri to use for hashing

   \return String allocated with TALLOC on success, otherwise NULL
 */
static char *_memcached_gen_key(TALLOC_CTX *mem_ctx, const char *uri)
{
	return talloc_asprintf(mem_ctx, "%"PRIx64, hash64((void *)uri, strlen(uri), 0));
}


/**
   \details Generate value for FMID cache

   \param mem_ctx pointer to the memory context
   \param fmid the fmid to convert

   \return String allocated with TALLOC on success, otherwise NULL
 */
static char *_memcached_gen_value(TALLOC_CTX *mem_ctx, uint64_t fmid)
{
	return talloc_asprintf(mem_ctx, "%"PRIu64, fmid);
}


/**
   \details Prepare FMID cache for specified user

   \param ictx valid pointer to the indexing context
   \param conn_str connection string to memcached server
   \param username name of the user for to create the FMID cache for

   \note Fallback to 127.0.0.1:11211 if server_string is missing

   \return valid memcached_st pointer on success, otherwise NULL
 */
static memcached_st *_memcached_setup(struct indexing_context *ictx,
				      const char *conn_str,
				      const char *username)
{
	TALLOC_CTX		*mem_ctx;
	char			*sql;
	int			mret;
	MYSQL_RES		*res;
	uint32_t		num_rows = 0;
	memcached_server_st	*servers = NULL;
	memcached_st		*memc = NULL;
	memcached_return	rc;
	uint32_t		i;

	OC_DEBUG(5, "[INFO] _memcached_setup for '%s'\n", username);

	/* Sanity checks */
	if (!ictx) return NULL;
	if (!username) return NULL;

	if (ictx->cache != NULL) {
		return ictx->cache;
	}

	/* Initialize memcached connection */
	if (conn_str) {
		memc = memcached(conn_str, strlen(conn_str));
		if (!memc) return NULL;
	} else {
		memc = (memcached_st *) memcached_create(NULL);
		if (!memc) return NULL;

		servers = memcached_server_list_append(servers, "127.0.0.1", 11211, &rc);
		rc = memcached_server_push(memc, servers);
		if (rc != MEMCACHED_SUCCESS) {
			OC_DEBUG(0, "[ERR]: Unable to add server to memcached list\n");
			return NULL;
		}
	}
	/* Retrieve indexing records for user */
	mem_ctx = talloc_new(NULL);
	if (!mem_ctx) return NULL;

	sql = talloc_asprintf(mem_ctx, "SELECT fmid,url FROM "INDEXING_TABLE" "
			      "WHERE username = '%s' AND soft_deleted = '%d'",
			      _sql(mem_ctx, username), 0);

	mret = select_without_fetch(MYSQL(ictx), sql, &res);
	if (mret != MYSQL_SUCCESS) {
		talloc_free(mem_ctx);
		return NULL;
	}

	num_rows = mysql_num_rows(res);
	OC_DEBUG(5, "[INFO] _memcached_setup: %d values to index\n", num_rows);

	/* store records into memcached */
	for (i = 0; i < num_rows; i++) {
		MYSQL_ROW row = mysql_fetch_row(res);

		if (row) {
			char *hashstr = NULL;

			hashstr = _memcached_gen_key(mem_ctx, row[1]);
			if (hashstr) {
				rc = memcached_add(memc, hashstr, strlen(hashstr),
						   row[0], strlen(row[0]), 0, 0);
				talloc_free(hashstr);
				if (rc != MEMCACHED_SUCCESS) {
					OC_DEBUG(8, "[ERR] Key %s not stored\n", row[1]);
				}
			}
		}
	}

	mysql_free_result(res);
	talloc_free(mem_ctx);
	return memc;
}


/**
   \details Retrieve FMID value from cache database given its uri

   \param ictx valid pointer to the indexing context
   \param uri pointer to the uri used to hash the key
   \param pointer to the fmid to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static enum mapistore_error _memcached_get_record(struct indexing_context *ictx,
						  const char *uri, uint64_t *fmid)
{
	TALLOC_CTX		*mem_ctx;
	memcached_st		*memc;
	memcached_return_t	error;
	uint32_t		flags;
	uint64_t		_fmid = 0;
	char			*key;
	char			*value;
	size_t			value_len;
	bool			bret = false;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!ictx->cache, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	key = _memcached_gen_key(mem_ctx, uri);
	MAPISTORE_RETVAL_IF(!key, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	memc = ictx->cache;
	value = memcached_get(memc, key, strlen(key), &value_len, &flags, &error);
	MAPISTORE_RETVAL_IF(!value, MAPISTORE_ERROR, mem_ctx);

	bret = convert_string_to_ull(value, &_fmid);
	MAPISTORE_RETVAL_IF(!bret, MAPISTORE_ERROR, mem_ctx);
	*fmid = _fmid;

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Add record to the cache database

   \param ictx valid pointer to the indexing context
   \param uri pointer to the uri used to hash the key
   \param fmid the value of the key/value pair

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static enum mapistore_error _memcached_add_record(struct indexing_context *ictx,
						  const char *uri, uint64_t fmid)
{
	TALLOC_CTX		*mem_ctx;
	memcached_st		*memc;
	memcached_return_t	rc;
	char			*key;
	char			*value;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Return MAPISTORE_SUCCESS if cache is not configured */
	MAPISTORE_RETVAL_IF(!ictx->cache, MAPISTORE_SUCCESS, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	key = _memcached_gen_key(mem_ctx, uri);
	MAPISTORE_RETVAL_IF(!key, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	value = _memcached_gen_value(mem_ctx, fmid);
	MAPISTORE_RETVAL_IF(!value, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	memc = (memcached_st *)ictx->cache;
	rc = memcached_add(memc, key, strlen(key), value, strlen(value), 0, 0);
	MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, MAPISTORE_ERROR, mem_ctx);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Update record in the cache database

   \param ictx valid pointer to the indexing context
   \param uri pointer to the uri used to hash the key
   \param fmid the value of the key/value pair

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static enum mapistore_error _memcached_update_record(struct indexing_context *ictx,
						     const char *uri, uint64_t fmid)
{
	TALLOC_CTX		*mem_ctx;
	memcached_st		*memc;
	memcached_return_t	rc;
	char			*key;
	char			*value;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Return MAPISTORE_SUCCESS if cache is not configured */
	MAPISTORE_RETVAL_IF(!ictx->cache, MAPISTORE_SUCCESS, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	key = _memcached_gen_key(mem_ctx, uri);
	MAPISTORE_RETVAL_IF(!key, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	value = _memcached_gen_value(mem_ctx, fmid);
	MAPISTORE_RETVAL_IF(!value, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	memc = (memcached_st *)ictx->cache;
	rc = memcached_set(memc, key, strlen(key), value, strlen(value), 0, 0);
	MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, MAPISTORE_ERROR, mem_ctx);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Delte record from the cache database

   \param ictx valid pointer to the indexing context
   \param uri pointer to the uri used to hash the key
   \param fmid the value of the key/value pair

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static enum mapistore_error _memcached_delete_record(struct indexing_context *ictx,
						     const char *uri)
{
	TALLOC_CTX		*mem_ctx;
	memcached_st		*memc;
	memcached_return_t	rc;
	char			*key;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Return MAPISTORE_SUCCESS if cache is not configured */
	MAPISTORE_RETVAL_IF(!ictx->cache, MAPISTORE_SUCCESS, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	key = _memcached_gen_key(mem_ctx, uri);
	MAPISTORE_RETVAL_IF(!key, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	memc = (memcached_st *)ictx->cache;
	rc = memcached_delete(memc, key, strlen(key), 0);
	MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, MAPISTORE_ERROR, mem_ctx);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}

/**
  \details Search for existing FMID in indexing database

  \param ictx valid pointer to indexing context
  \param username samAccountName for current user
  \param fmid FMID to search for
  \param is_soft_deleted pointer to output location to return soft deleted state

  \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mysql_search_existing_fmid(struct indexing_context *ictx,
						       const char *username,
						       uint64_t fmid, bool *is_soft_deleted)
{
	int		ret;
	uint64_t	soft_deleted;
	char		*sql;
	TALLOC_CTX	*mem_ctx;

	/* Sanity */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!is_soft_deleted, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_new(NULL);
	sql = talloc_asprintf(mem_ctx,
		"SELECT soft_deleted FROM %s "
		"WHERE username = '%s' AND fmid = '%"PRIu64"'",
		INDEXING_TABLE, _sql(mem_ctx, username), fmid);
	ret = select_first_uint(MYSQL(ictx), sql, &soft_deleted);
	MAPI_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_EXIST, mem_ctx);

	*is_soft_deleted = (soft_deleted == 1);
	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}

/**
  \details Adds FMID and related URL in indexing database

  \param ictx valid pointer to indexing context
  \param username samAccountName for current user
  \param fmid FMID to record
  \param mapistore_URI mapistore URI string to associate with fmid

  \return MAPISTORE_SUCCESS on success,
	  MAPISTORE_ERR_EXIST if such entry already exists
	  MAPISTORE_ERR_NOT_INITIALIZED if ictx pointer is invalid (NULL)
	  MAPISTORE_ERR_INVALID_PARAMETER in case other parameters are not valid
	  MAPISTORE_ERR_DATABASE_OPS in case of MySQL error
 */
static enum mapistore_error mysql_record_add(struct indexing_context *ictx,
					   const char *username,
					   uint64_t fmid,
					   const char *mapistore_URI)
{
	enum mapistore_error	retval;
	TALLOC_CTX		*mem_ctx;
	int			ret;
	bool			IsSoftDeleted = false;
	char			*sql;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mapistore_URI, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Check if the fid/mid doesn't already exist within the database */
	ret = mysql_search_existing_fmid(ictx, username, fmid, &IsSoftDeleted);
	MAPISTORE_RETVAL_IF(ret == MAPISTORE_SUCCESS, MAPISTORE_ERR_EXIST, NULL);

	mem_ctx = talloc_new(NULL);
	sql = talloc_asprintf(mem_ctx,
		"INSERT INTO %s "
		"(username, fmid, url, soft_deleted) "
		"VALUES ('%s', '%"PRIu64"', '%s', '%d')",
		INDEXING_TABLE, _sql(mem_ctx, username), fmid,
		_sql(mem_ctx, mapistore_URI), 0);

	ret = execute_query(MYSQL(ictx), sql);
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);

	retval = _memcached_add_record(ictx, mapistore_URI, fmid);
	if (retval != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "[indexing] Failed to add record `%s: %"PRIu64"` on memcached (%s)",
			 mapistore_URI, fmid, mapistore_errstr(retval));
	}

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}

/**
  \details Update Mapistore URI for existing FMID

  \param ictx valid pointer to indexing context
  \param username samAccountName for current user
  \param fmid FMID to update
  \param mapistore_URI mapistore URI string to associate with fmid

  \return MAPISTORE_SUCCESS on success,
	  MAPISTORE_ERR_NOT_FOUND if FMID entry doesn't exists
	  MAPISTORE_ERR_NOT_INITIALIZED if ictx pointer is invalid (NULL)
	  MAPISTORE_ERR_INVALID_PARAMETER in case other parameters are not valid
	  MAPISTORE_ERR_DATABASE_OPS in case of MySQL error
 */
static enum mapistore_error mysql_record_update(struct indexing_context *ictx,
						const char *username,
						uint64_t fmid,
						const char *mapistore_URI)
{
	enum mapistore_error	retval;
	int			ret;
	char			*sql;
	TALLOC_CTX		*mem_ctx;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mapistore_URI, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	sql = talloc_asprintf(mem_ctx,
		"UPDATE %s "
		"SET url = '%s' "
		"WHERE username = '%s' AND fmid = '%"PRIu64"'",
		INDEXING_TABLE, _sql(mem_ctx, mapistore_URI), _sql(mem_ctx, username), fmid);
	MAPISTORE_RETVAL_IF(!sql, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	ret = execute_query(MYSQL(ictx), sql);
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);

	/* did we updated anything? */
	/* TODO: Move mysql_affected_rows() in execute_query() */
	MAPISTORE_RETVAL_IF(mysql_affected_rows(MYSQL(ictx)) == 0, MAPISTORE_ERR_NOT_FOUND, mem_ctx);

	retval = _memcached_update_record(ictx, mapistore_URI, fmid);
	if (retval != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "[indexing] Failed to update record `%s` with `%"PRIu64"` on memcached (%s)",
			 mapistore_URI, fmid, mapistore_errstr(retval));
	}

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
  \details Get mapistore URI by FMID.

  \param ictx valid pointer to indexing context
  \param username samAccountName for current user
  \param mem_ctx TALLOC_CTX to allocate mapistore URI
  \param fmid FMID to search for
  \param soft_deletedp Pointer to bool var to return Soft Deleted state

  \return MAPISTORE_SUCCESS on success
	  MAPISTORE_ERR_NOT_INITIALIZED if ictx pointer is invalid (NULL)
	  MAPISTORE_ERR_INVALID_PARAMETER in case other parameters are not valid
	  MAPISTORE_ERR_DATABASE_OPS in case of MySQL error
 */
static enum mapistore_error mysql_record_get_uri(struct indexing_context *ictx,
						 const char *username,
						 TALLOC_CTX *mem_ctx,
						 uint64_t fmid,
						 char **urip,
						 bool *soft_deletedp)
{
	int		ret;
	char		*sql;
	MYSQL_RES	*res = NULL;
	MYSQL_ROW	row;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!urip, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!soft_deletedp, MAPISTORE_ERR_INVALID_PARAMETER, NULL);


	sql = talloc_asprintf(mem_ctx,
		"SELECT url, soft_deleted FROM %s "
		"WHERE username = '%s' AND fmid = '%"PRIu64"'",
		INDEXING_TABLE, _sql(mem_ctx, username), fmid);
	MAPISTORE_RETVAL_IF(!sql, MAPISTORE_ERR_NO_MEMORY, NULL);

	ret = select_without_fetch(MYSQL(ictx), sql, &res);
	MAPISTORE_RETVAL_IF(ret == MYSQL_NOT_FOUND, MAPISTORE_ERR_NOT_FOUND, sql);
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, sql);

	row = mysql_fetch_row(res);

	*urip = talloc_strdup(mem_ctx, row[0]);
	*soft_deletedp = strtoull(row[1], NULL, 0) == 1;

	mysql_free_result(res);
	talloc_free(sql);

	return MAPISTORE_SUCCESS;
}


/**
  \details Delete FMID mapping from database.
	   Note that function will succeed when there is no such FMID

  \param ictx valid pointer to indexing context
  \param username samAccountName for current user
  \param fmid FMID to delete
  \param flags MAPISTORE_SOFT_DELETE - soft delete the entry,
	       MAPISTORE_PERMANENT_DELETE - permanently delete

  \return MAPISTORE_SUCCESS on success
	  MAPISTORE_ERR_NOT_INITIALIZED if ictx pointer is invalid (NULL)
	  MAPISTORE_ERR_INVALID_PARAMETER in case other parameters are not valid
	  MAPISTORE_ERR_DATABASE_OPS in case of MySQL error
 */
static enum mapistore_error mysql_record_del(struct indexing_context *ictx,
					     const char *username,
					     uint64_t fmid,
					     uint8_t flags)
{
	enum mapistore_error	retval;
	TALLOC_CTX		*mem_ctx;
	int			ret;
	bool			IsSoftDeleted = false;
	char			*sql;
	char			*uri = NULL;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Check if the fid/mid still exists within the database */
	ret = mysql_search_existing_fmid(ictx, username, fmid, &IsSoftDeleted);
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_SUCCESS, NULL);

	mem_ctx = talloc_new(NULL);

	switch (flags) {
	case MAPISTORE_SOFT_DELETE:
		/* nothing to do if the record is already soft deleted */
		MAPISTORE_RETVAL_IF(IsSoftDeleted == true, MAPISTORE_SUCCESS, NULL);
		sql = talloc_asprintf(mem_ctx,
			"UPDATE %s "
			"SET soft_deleted=1 "
			"WHERE username = '%s' AND fmid = '%"PRIu64"'",
			INDEXING_TABLE, _sql(mem_ctx, username), fmid);
		break;
	case MAPISTORE_PERMANENT_DELETE:
		sql = talloc_asprintf(mem_ctx,
			"DELETE FROM %s "
			"WHERE username = '%s' AND fmid = '%"PRIu64"'",
			INDEXING_TABLE, _sql(mem_ctx, username), fmid);
		break;
	default:
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	retval = mysql_record_get_uri(ictx, username, mem_ctx, fmid, &uri, &IsSoftDeleted);
	MAPISTORE_RETVAL_IF(retval != MAPISTORE_SUCCESS, retval, mem_ctx);

	ret = execute_query(MYSQL(ictx), sql);
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);

	retval = _memcached_delete_record(ictx, uri);
	if (retval != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "[indexing] Failed to delete record `%s` on memcached (%s)",
			 uri, mapistore_errstr(retval));
	}

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}

/**
  \details Get FMID by mapistore URI.

  \param ictx valid pointer to indexing context
  \param username samAccountName for current user
  \param uri mapistore URI or pattern to search for
  \param partia if true, uri is pattern to search for
  \param fmidp pointer to valid location to store found FMID
  \param soft_deletedp Pointer to bool var to return Soft Deleted state

  \return MAPISTORE_SUCCESS on success
	  MAPISTORE_ERR_NOT_FOUND if uri does not exists in DB
	  MAPISTORE_ERR_NOT_INITIALIZED if ictx pointer is invalid (NULL)
	  MAPISTORE_ERR_INVALID_PARAMETER in case other parameters are not valid
	  MAPISTORE_ERR_DATABASE_OPS in case of MySQL error
 */
static enum mapistore_error mysql_record_get_fmid(struct indexing_context *ictx,
						  const char *username,
						  const char *uri,
						  bool partial,
						  uint64_t *fmidp,
						  bool *soft_deletedp)
{
	enum mapistore_error	retval;
	enum MYSQLRESULT	ret;
	char			*sql, *uri_like;
	MYSQL_RES		*res;
	MYSQL_ROW		row;
	TALLOC_CTX		*mem_ctx;
	uint64_t		fmid = 0;

	// Sanity checks
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!fmidp, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!soft_deletedp, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	retval = _memcached_get_record(ictx, uri, &fmid);
	if (retval == MAPISTORE_SUCCESS) {
		*fmidp = fmid;
		*soft_deletedp = 0;
		return retval;
	}

	mem_ctx = talloc_named(NULL, 0, "mysql_record_get_fmid");

	sql = talloc_asprintf(mem_ctx,
		"SELECT fmid, soft_deleted FROM "INDEXING_TABLE" "
		"WHERE username = '%s'", _sql(mem_ctx, username));

	if (partial) {
		uri_like = talloc_strdup(mem_ctx, uri);
		string_replace(uri_like, '*', '%');
		sql = talloc_asprintf_append(sql, " AND url LIKE '%s'",
					     _sql(mem_ctx, uri_like));
	} else {
		sql = talloc_asprintf_append(sql, " AND url = '%s'",
					     _sql(mem_ctx, uri));
	}

	ret = select_without_fetch(MYSQL(ictx), sql, &res);
	MAPISTORE_RETVAL_IF(ret == MYSQL_NOT_FOUND, MAPISTORE_ERR_NOT_FOUND, mem_ctx);
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);

	row = mysql_fetch_row(res);

	*fmidp = strtoull(row[0], NULL, 0);
	*soft_deletedp = strtoull(row[1], NULL, 0) == 1;

	mysql_free_result(res);
	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}


static enum mapistore_error mysql_record_allocate_fmids(struct indexing_context *ictx,
						      const char *username,
						      int count,
						      uint64_t *fmidp)
{
	int		ret;
	uint64_t	next_fmid;
	char		*sql;
	TALLOC_CTX	*mem_ctx;

	/* SANITY checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!fmidp, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(count < 0, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(count == 0, MAPISTORE_SUCCESS, NULL);

	/* Retrieve and increment the counter */
	ret = execute_query(MYSQL(ictx), "START TRANSACTION");
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	sql = talloc_asprintf(mem_ctx,
		"SELECT next_fmid FROM %s "
		"WHERE username = '%s'",
		INDEXING_ALLOC_TABLE, _sql(mem_ctx, username));
	MAPISTORE_RETVAL_IF(!sql, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	ret = select_first_uint(MYSQL(ictx), sql, &next_fmid);
	switch (ret) {
	case MYSQL_SUCCESS:
		if (next_fmid <= MAX_PUBLIC_FOLDER_ID) {
			next_fmid = MAX_PUBLIC_FOLDER_ID + 1;
		}
		// Update next fmid
		sql = talloc_asprintf(mem_ctx,
			"UPDATE %s SET next_fmid = %"PRIu64
			" WHERE username='%s'",
			INDEXING_ALLOC_TABLE,
			next_fmid + count,
			_sql(mem_ctx, username));
		MAPISTORE_RETVAL_IF(!sql, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
		break;
	case MYSQL_NOT_FOUND:
		// First allocation, insert in the database
		next_fmid = MAX_PUBLIC_FOLDER_ID + 1;
		sql = talloc_asprintf(mem_ctx,
			"INSERT INTO %s (username, next_fmid) "
			"VALUES('%s', %"PRIu64")",
			INDEXING_ALLOC_TABLE,
			_sql(mem_ctx, username),
			next_fmid + count);
		MAPISTORE_RETVAL_IF(!sql, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
		break;

	default:
		// Unknown error
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_DATABASE_OPS;
	}
	ret = execute_query(MYSQL(ictx), sql);
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);

	ret = execute_query(MYSQL(ictx), "COMMIT");
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);

	*fmidp = next_fmid;

	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}

static enum mapistore_error mysql_record_allocate_fmid(struct indexing_context *ictx,
						     const char *username,
						     uint64_t *fmidp)
{
	return mysql_record_allocate_fmids(ictx, username, 1, fmidp);
}


static int mapistore_indexing_mysql_destructor(struct indexing_context *ictx)
{
	if (ictx && ictx->data) {
		MYSQL *conn = ictx->data;
		if (ictx->cache) {
			memcached_free((memcached_st *)ictx->cache);
		}
		if (ictx->url) {
			OC_DEBUG(5, "Destroying indexing context `%s`\n", ictx->url);
		} else {
			OC_DEBUG(5, "Destroying unknown indexing context\n");
		}
		release_connection(conn);
	} else {
		OC_DEBUG(0, "Error: tried to destroy corrupted indexing mysql context\n");
	}
	return 0;
}


/**
   \details Open connection to indexing database for a given user

   \param mstore_ctx pointer to the mapistore context
   \param username name for which the indexing database has to be
   created
   \param connection_string mysql connection string
   \param ictxp returned value with the indexing context created

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_indexing_mysql_init(struct mapistore_context *mstore_ctx,
							    const char *username,
							    const char *connection_string,
							    struct indexing_context **ictxp)
{
	struct indexing_context	*ictx;
	MYSQL			*conn = NULL;
	int			schema_created_ret;
	char			*cache_url = NULL;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!connection_string, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!ictxp, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ictx = talloc_zero(mstore_ctx, struct indexing_context);
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_NO_MEMORY, NULL);

	create_connection(connection_string, &conn);
	MAPISTORE_RETVAL_IF(!conn, MAPISTORE_ERR_NOT_INITIALIZED, ictx);
	ictx->data = conn;
	talloc_set_destructor(ictx, mapistore_indexing_mysql_destructor);
	MAPISTORE_RETVAL_IF(!ictx->data, MAPISTORE_ERR_NOT_INITIALIZED, ictx);

	if (!table_exists(conn, INDEXING_TABLE)) {
		OC_DEBUG(3, "Creating schema for indexing on mysql %s\n",
			  connection_string);

		schema_created_ret = migrate_indexing_schema(connection_string);
		if (schema_created_ret) {
			OC_DEBUG(1, "Failed indexing schema creation using migration framework: %d\n",
				 schema_created_ret);
			MAPISTORE_RETVAL_ERR(MAPISTORE_ERR_NOT_INITIALIZED, ictx);
		}
	}

	/* TODO: extract url from backend mapping, by the moment we use the username */
	ictx->url = talloc_strdup(ictx, username);
	MAPISTORE_RETVAL_IF(!ictx->url, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Fill function pointers */
	ictx->add_fmid = mysql_record_add;
	ictx->del_fmid = mysql_record_del;
	ictx->update_fmid = mysql_record_update;
	ictx->get_uri = mysql_record_get_uri;
	ictx->get_fmid = mysql_record_get_fmid;
	ictx->allocate_fmid = mysql_record_allocate_fmid;
	ictx->allocate_fmids = mysql_record_allocate_fmids;

	/* Data pointers */
	cache_url = mapistore_get_default_cache_url();
	ictx->cache = _memcached_setup(ictx, cache_url, username);

	*ictxp = ictx;

	return MAPISTORE_SUCCESS;
}
