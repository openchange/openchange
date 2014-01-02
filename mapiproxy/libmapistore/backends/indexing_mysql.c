#include <string.h>

#include "../mapistore.h"
#include "../mapistore_private.h"
#include "../mapistore_errors.h"
#include "indexing_mysql.h"
#include "libmapi/libmapi_private.h"
#include <mysql/mysql.h>
#include "../../util/mysql.h"

#include <talloc.h>


#define SCHEMA_FILE "indexing_schema.sql"
#define INDEXING_TABLE "mapistore_indexing"
#define INDEXING_ALLOC_TABLE "mapistore_indexes"
#define TDB_WRAP(context)	(context)
#define MYSQL(context)	((MYSQL *)context->data)


static enum mapistore_error mysql_search_existing_fmid(struct indexing_context *ictx,
						     const char *username,
						     uint64_t fmid, bool *IsSoftDeleted)
{
	int		ret;
	uint64_t	soft_deleted;
	char		*sql;
	TALLOC_CTX	*mem_ctx;

	/* Sanity */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERROR, NULL);

	mem_ctx = talloc_new(NULL);
	sql = talloc_asprintf(mem_ctx,
		"SELECT soft_deleted FROM %s "
		"WHERE username = '%s' AND fmid = %"PRIu64,
		INDEXING_TABLE, _sql(mem_ctx, username), fmid);
	ret = select_first_uint(MYSQL(ictx), sql, &soft_deleted);
	MAPI_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_EXIST, mem_ctx);

	*IsSoftDeleted = (soft_deleted == 1);
	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}

static enum mapistore_error mysql_record_add(struct indexing_context *ictx,
					   const char *username,
					   uint64_t fmid,
					   const char *mapistore_URI)
{
	int		ret;
	bool		IsSoftDeleted = false;
	char		*sql;
	TALLOC_CTX	*mem_ctx;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERROR, NULL);

	/* Check if the fid/mid doesn't already exist within the database */
	ret = mysql_search_existing_fmid(ictx, username, fmid, &IsSoftDeleted);
	MAPISTORE_RETVAL_IF(ret == MAPISTORE_SUCCESS, MAPISTORE_ERR_EXIST, NULL);

	mem_ctx = talloc_new(NULL);
	sql = talloc_asprintf(mem_ctx,
		"INSERT INTO %s "
		"(username, fmid, url, soft_deleted) "
		"VALUES ('%s', %"PRIu64", '%s', '%d')",
		INDEXING_TABLE, _sql(mem_ctx, username), fmid,
		_sql(mem_ctx, mapistore_URI), 0);

	ret = execute_query(MYSQL(ictx), sql);
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}

static enum mapistore_error mysql_record_update(struct indexing_context *ictx,
					      const char *username,
					      uint64_t fmid,
					      const char *mapistore_URI)
{
	int		ret;
	char		*sql;
	TALLOC_CTX	*mem_ctx;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERROR, NULL);

	mem_ctx = talloc_new(NULL);
	sql = talloc_asprintf(mem_ctx,
		"UPDATE %s "
		"SET url = '%s' "
		"WHERE username = '%s' AND fmid = %"PRIu64,
		INDEXING_TABLE, _sql(mem_ctx, mapistore_URI), _sql(mem_ctx, username), fmid);

	ret = execute_query(MYSQL(ictx), sql);
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}

static enum mapistore_error mysql_record_del(struct indexing_context *ictx,
					   const char *username,
					   uint64_t fmid,
					   uint8_t flags)
{
	int		ret;
	bool		IsSoftDeleted = false;
	char		*sql;
	TALLOC_CTX	*mem_ctx;


	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERROR, NULL);

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
			"WHERE username = '%s' AND fmid = %"PRIu64,
			INDEXING_TABLE, _sql(mem_ctx, username), fmid);
		break;
	case MAPISTORE_PERMANENT_DELETE:
		sql = talloc_asprintf(mem_ctx,
			"DELETE FROM %s "
			"WHERE username = '%s' AND fmid = %"PRIu64,
			INDEXING_TABLE, _sql(mem_ctx, username), fmid);
		break;
	}

	execute_query(MYSQL(ictx), sql);
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}

static enum mapistore_error mysql_record_get_uri(struct indexing_context *ictx,
					       const char *username,
					       TALLOC_CTX *mem_ctx,
					       uint64_t fmid,
					       char **urip,
					       bool *soft_deletedp)
{
	int		ret;
	char		*sql;
	MYSQL_RES	*res;
	MYSQL_ROW	row;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!urip, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!soft_deletedp, MAPISTORE_ERR_NOT_INITIALIZED, NULL);


	sql = talloc_asprintf(mem_ctx,
		"SELECT url, soft_deleted FROM %s "
		"WHERE username = '%s' AND fmid = %"PRIu64,
		INDEXING_TABLE, _sql(mem_ctx, username), fmid);

	ret = select_without_fetch(MYSQL(ictx), sql, &res);
	MAPISTORE_RETVAL_IF(ret == MYSQL_NOT_FOUND, MAPISTORE_ERR_NOT_FOUND, sql);
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, sql);

	row = mysql_fetch_row(res);

	*urip = talloc_strdup(mem_ctx, row[0]);
	*soft_deletedp = strtoull(row[1], NULL, 0) == 1;

	talloc_free(sql);
	return MAPISTORE_SUCCESS;
}


static enum mapistore_error mysql_record_get_fmid(struct indexing_context *ictx,
					        const char *username,
					        const char *uri, bool partial,
					        uint64_t *fmidp, bool *soft_deletedp)
{
	int		ret;
	char		*sql;
	MYSQL_RES	*res;
	MYSQL_ROW	row;
	TALLOC_CTX	*mem_ctx;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!fmidp, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!soft_deletedp, MAPISTORE_ERR_NOT_INITIALIZED, NULL);


	mem_ctx = talloc_new(NULL);
	// TODO take partial into account and do a LIKE search
	sql = talloc_asprintf(mem_ctx,
		"SELECT fmid, soft_deleted FROM %s "
		"WHERE username = '%s' AND url = '%s'",
		INDEXING_TABLE, _sql(mem_ctx, username), _sql(mem_ctx, uri));

	ret = select_without_fetch(MYSQL(ictx), sql, &res);
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, sql);
	MAPISTORE_RETVAL_IF(!mysql_num_rows(res), MAPISTORE_ERR_NOT_FOUND, sql);

	row = mysql_fetch_row(res);

	*fmidp = strtoull(row[0], NULL, 0);
	*soft_deletedp = strtoull(row[1], NULL, 0) == 1;

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
	MAPISTORE_RETVAL_IF(count <= 0, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	/* Retrieve and increment the counter */
	ret = execute_query(MYSQL(ictx), "START TRANSACTION");
	MAPISTORE_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, NULL);

	mem_ctx = talloc_new(NULL);
	sql = talloc_asprintf(mem_ctx,
		"SELECT next_fmid FROM %s "
		"WHERE username = '%s'",
		INDEXING_ALLOC_TABLE, _sql(mem_ctx, username));
	ret = select_first_uint(MYSQL(ictx), sql, &next_fmid);
	switch (ret) {
	case MYSQL_SUCCESS:
		// Update next fmid
		sql = talloc_asprintf(mem_ctx,
			"UPDATE %s SET next_fmid = %"PRIu64
			" WHERE username='%s'",
			INDEXING_ALLOC_TABLE,
			next_fmid + count,
			_sql(mem_ctx, username));
		break;
	case MYSQL_NOT_FOUND:
		// First allocation, insert in the database
		next_fmid = 1;
		sql = talloc_asprintf(mem_ctx,
			"INSERT INTO %s (username, next_fmid) "
			"VALUES('%s', %"PRIu64")",
			INDEXING_ALLOC_TABLE,
			_sql(mem_ctx, username),
			next_fmid + count);
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

	return MAPISTORE_SUCCESS;
}

static enum mapistore_error mysql_record_allocate_fmid(struct indexing_context *ictx,
						     const char *username,
						     uint64_t *fmidp)
{
	return mysql_record_allocate_fmids(ictx, username, 1, fmidp);
}


/**
   \details Open connection to indexing database for a given user

   \param mstore_ctx pointer to the mapistore context
   \param username name for which the indexing database has to be
   created

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */

_PUBLIC_ enum mapistore_error mapistore_indexing_mysql_init(struct mapistore_context *mstore_ctx,
							    const char *username,
							    struct indexing_context **ictxp)
{
	struct indexing_context	*ictx;
	char			*connection_string = "mysql://root@localhost/openchange_test";
	bool			schema_created;
	char			*schema_file;
	MYSQL			*conn = NULL;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERROR, NULL);

	ictx = talloc_zero(mstore_ctx, struct indexing_context);

	ictx->data = create_connection(connection_string, &conn);
	OPENCHANGE_RETVAL_IF(!ictx->data, MAPI_E_NOT_INITIALIZED, ictx);
	if (!table_exists(conn, INDEXING_TABLE)) {
		DEBUG(0, ("Creating schema for indexing on mysql %s",
			  connection_string));

		schema_file = talloc_asprintf(ictx, "%s/%s", MAPISTORE_LDIF, SCHEMA_FILE);
		schema_created = create_schema(MYSQL(ictx), schema_file);
		talloc_free(schema_file);

		OPENCHANGE_RETVAL_IF(!schema_created, MAPI_E_NOT_INITIALIZED, ictx);
	}


	/* TODO: extract url from backend mapping, by the moment we use the username */
	ictx->url = talloc_strdup(ictx, username);

	/* Fill function pointers */
	ictx->add_fmid = mysql_record_add;
	ictx->del_fmid = mysql_record_del;
	ictx->update_fmid = mysql_record_update;
	ictx->get_uri = mysql_record_get_uri;
	ictx->get_fmid = mysql_record_get_fmid;
	ictx->allocate_fmid = mysql_record_allocate_fmid;
	ictx->allocate_fmids = mysql_record_allocate_fmids;

	*ictxp = ictx;

	return MAPISTORE_SUCCESS;
}
