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
#define TDB_WRAP(context)	(context)
#define MYSQL(context)	((MYSQL *)context->data)


static enum mapistore_error mysql_search_existing_fmid(struct indexing_context *ictx,
						     const char *username,
						     uint64_t fmid, bool *IsSoftDeleted)
{
	int		ret;
	uint64_t	soft_deleted;
	char		*sql;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);

	/* Sanity */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERROR, NULL);

	sql = talloc_asprintf(mem_ctx,
		"SELECT soft_deleted FROM %s "
		"WHERE username = '%s' AND fmid = %"PRIu64,
		INDEXING_TABLE, _sql(mem_ctx, username), fmid);
	ret = select_first_uint(MYSQL(ictx), sql, &soft_deleted);
	MAPI_RETVAL_IF(ret != MYSQL_SUCCESS, MAPISTORE_ERR_EXIST, mem_ctx);

	*IsSoftDeleted = (soft_deleted == 1);
	return MAPISTORE_SUCCESS;
}

static enum mapistore_error mysql_record_add(struct indexing_context *ictx,
					   const char *username,
					   uint64_t fmid,
					   const char *mapistore_URI)
{
	int		ret;
	TDB_DATA	key;
	TDB_DATA	dbuf;
	bool		IsSoftDeleted = false;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERROR, NULL);

	/* Check if the fid/mid doesn't already exist within the database */
	ret = mysql_search_existing_fmid(ictx, username, fmid, &IsSoftDeleted);
	MAPISTORE_RETVAL_IF(ret, ret, NULL);

	/* Add the record given its fid and mapistore_uri */
	key.dptr = (unsigned char *) talloc_asprintf(ictx, "0x%.16"PRIx64, fmid);
	key.dsize = strlen((const char *) key.dptr);

	dbuf.dptr = (unsigned char *) talloc_strdup(ictx, mapistore_URI);
	dbuf.dsize = strlen((const char *) dbuf.dptr);

	ret = mysql_store(TDB_WRAP(ictx)->tdb, key, dbuf, TDB_INSERT);
	talloc_free(key.dptr);
	talloc_free(dbuf.dptr);

	if (ret == -1) {
		DEBUG(3, ("[%s:%d]: Unable to create 0x%.16"PRIx64" record: %s\n", __FUNCTION__, __LINE__,
			  fmid, mapistore_URI));
		return MAPISTORE_ERR_DATABASE_OPS;
	}

	return MAPISTORE_SUCCESS;
}

static enum mapistore_error mysql_record_update(struct indexing_context *ictx,
					      const char *username,
					      uint64_t fmid,
					      const char *mapistore_URI)
{
	int		ret;
	TDB_DATA	key;
	TDB_DATA	dbuf;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERROR, NULL);

	/* Add the record given its fid and mapistore_uri */
	key.dptr = (unsigned char *) talloc_asprintf(ictx, "0x%.16"PRIx64, fmid);
	key.dsize = strlen((const char *) key.dptr);

	dbuf.dptr = (unsigned char *) talloc_strdup(ictx, mapistore_URI);
	dbuf.dsize = strlen((const char *) dbuf.dptr);

	ret = mysql_store(TDB_WRAP(ictx)->tdb, key, dbuf, TDB_MODIFY);
	talloc_free(key.dptr);
	talloc_free(dbuf.dptr);

	if (ret == -1) {
		DEBUG(3, ("[%s:%d]: Unable to update 0x%.16"PRIx64" record: %s\n", __FUNCTION__, __LINE__,
			  fmid, mapistore_URI));
		return MAPISTORE_ERR_DATABASE_OPS;
	}

	return MAPISTORE_SUCCESS;
}

static enum mapistore_error mysql_record_del(struct indexing_context *ictx,
					   const char *username,
					   uint64_t fmid,
					   uint8_t flags)
{
	int				ret;
	TDB_DATA			key;
	TDB_DATA			newkey;
	TDB_DATA			dbuf;
	bool				IsSoftDeleted = false;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!fmid, MAPISTORE_ERROR, NULL);

	/* Check if the fid/mid still exists within the database */
	ret = mysql_search_existing_fmid(ictx, username, fmid, &IsSoftDeleted);
	MAPISTORE_RETVAL_IF(!ret, ret, NULL);

	if (IsSoftDeleted == true) {
		key.dptr = (unsigned char *) talloc_asprintf(ictx, "%s0x%.16"PRIx64,
							     MAPISTORE_SOFT_DELETED_TAG, fmid);
	} else {
		key.dptr = (unsigned char *) talloc_asprintf(ictx, "0x%.16"PRIx64, fmid);
	}
	key.dsize = strlen((const char *) key.dptr);

	switch (flags) {
	case MAPISTORE_SOFT_DELETE:
		/* nothing to do if the record is already soft deleted */
		MAPISTORE_RETVAL_IF(IsSoftDeleted == true, MAPISTORE_SUCCESS, NULL);
		newkey.dptr = (unsigned char *) talloc_asprintf(ictx, "%s0x%.16"PRIx64,
								MAPISTORE_SOFT_DELETED_TAG,
								fmid);
		newkey.dsize = strlen ((const char *)newkey.dptr);
		/* Retrieve previous value */
		dbuf = mysql_fetch(TDB_WRAP(ictx)->tdb, key);
		/* Add new record */
		ret = mysql_store(TDB_WRAP(ictx)->tdb, newkey, dbuf, TDB_INSERT);
		free(dbuf.dptr);
		/* Delete previous record */
		ret = mysql_delete(TDB_WRAP(ictx)->tdb, key);
		talloc_free(key.dptr);
		talloc_free(newkey.dptr);
		break;
	case MAPISTORE_PERMANENT_DELETE:
		ret = mysql_delete(TDB_WRAP(ictx)->tdb, key);
		talloc_free(key.dptr);
		MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERR_DATABASE_OPS, NULL);
		break;
	}

	return MAPISTORE_SUCCESS;
}

static enum mapistore_error mysql_record_get_uri(struct indexing_context *ictx,
					       const char *username,
					       TALLOC_CTX *mem_ctx,
					       uint64_t fmid,
					       char **urip,
					       bool *soft_deletedp)
{
	TDB_DATA			key, dbuf;
	int				ret;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!urip, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!soft_deletedp, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	/* Check if the fmid exists within the database */
	key.dptr = (unsigned char *) talloc_asprintf(ictx, "0x%.16"PRIx64, fmid);
	key.dsize = strlen((const char *) key.dptr);

	ret = mysql_exists(TDB_WRAP(ictx)->tdb, key);
	if (ret) {
		*soft_deletedp = false;
	}
	else {
		talloc_free(key.dptr);
		key.dptr = (unsigned char *) talloc_asprintf(ictx, "%s0x%.16"PRIx64,
							     MAPISTORE_SOFT_DELETED_TAG,
							     fmid);
		key.dsize = strlen((const char *)key.dptr);
		ret = mysql_exists(TDB_WRAP(ictx)->tdb, key);
		if (ret) {
			*soft_deletedp = true;
		}
		else {
			talloc_free(key.dptr);
			*urip = NULL;
			return MAPISTORE_ERR_NOT_FOUND;
		}
	}
	dbuf = mysql_fetch(TDB_WRAP(ictx)->tdb, key);
	*urip = talloc_strndup(mem_ctx, (const char *) dbuf.dptr, dbuf.dsize);
	free(dbuf.dptr);
	talloc_free(key.dptr);

	return MAPISTORE_SUCCESS;
}

/**
   \details A slow but effective way to retrieve an fmid from a uri

   \param mstore_ctx pointer to the mapistore context
   \param mem_ctx pointer to the talloc context
   \param fmid the fmid/key to the record
   \param urip pointer to the uri pointer
   \param soft_deletedp pointer to the soft deleted pointer

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
struct mysql_get_fid_data {
	bool		found;
	uint64_t	fmid;
	char		*uri;
	size_t		uri_len;
	uint32_t	wildcard_count;
	char		*startswith;
	char		*endswith;
};

static int mysql_get_fid_traverse(struct mysql_context *mysql_ctx, TDB_DATA key, TDB_DATA value, void *data)
{
	struct mysql_get_fid_data	*mysql_data;
	char			*key_str, *cmp_uri, *slash_ptr;
	TALLOC_CTX		*mem_ctx;
	int			ret = 0;

	mem_ctx = talloc_zero(NULL, void);
	mysql_data = data;
	cmp_uri = talloc_array(mem_ctx, char, value.dsize + 1);
	memcpy(cmp_uri, value.dptr, value.dsize);
	*(cmp_uri + value.dsize) = 0;
	slash_ptr = cmp_uri + value.dsize - 1;
	if (*slash_ptr == '/') {
		*slash_ptr = 0;
	}
	if (strcmp(cmp_uri, mysql_data->uri) == 0) {
		key_str = talloc_strndup(mem_ctx, (char *) key.dptr, key.dsize);
		mysql_data->fmid = strtoull(key_str, NULL, 16);
		mysql_data->found = true;
		ret = 1;
	}
	
	talloc_free(mem_ctx);

	return ret;
}

static int mysql_get_fid_traverse_partial(struct mysql_context *mysql_ctx, TDB_DATA key, TDB_DATA value, void *data)
{
	struct mysql_get_fid_data	*mysql_data;
	char			*key_str, *cmp_uri, *slash_ptr;
	TALLOC_CTX		*mem_ctx;
	int			ret = 0;

	mem_ctx = talloc_zero(NULL, void);
	mysql_data = data;
	cmp_uri = talloc_array(mem_ctx, char, value.dsize + 1);
	memcpy(cmp_uri, value.dptr, value.dsize);
	*(cmp_uri + value.dsize) = 0;
	slash_ptr = cmp_uri + value.dsize - 1;
	if (*slash_ptr == '/') {
		*slash_ptr = 0;
	}

	if (!strncmp(cmp_uri, mysql_data->startswith, strlen(mysql_data->startswith)) &&
	    !strncmp(cmp_uri + (strlen(cmp_uri) - strlen(mysql_data->endswith)), mysql_data->endswith,
		     strlen(mysql_data->endswith))) {
		    key_str = talloc_strndup(mem_ctx, (char *) key.dptr, key.dsize);
		    mysql_data->fmid = strtoull(key_str, NULL, 16);
		    mysql_data->found = true;
		    ret = 1;
	}
	
	talloc_free(mem_ctx);

	return ret;
}

static enum mapistore_error mysql_record_get_fmid(struct indexing_context *ictx,
					        const char *username,
					        const char *uri, bool partial,
					        uint64_t *fmidp, bool *soft_deletedp)
{
	int				ret;
	struct mysql_get_fid_data		mysql_data;
	char				*slash_ptr;
	uint32_t			i;

	/* SANITY checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!fmidp, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!soft_deletedp, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	/* Check if the fmid exists within the database */
	mysql_data.found = false;
	mysql_data.uri = talloc_strdup(NULL, uri);
	mysql_data.uri_len = strlen(uri);

	mysql_data.startswith = NULL;
	mysql_data.endswith = NULL;
	mysql_data.wildcard_count = 0;

	slash_ptr = mysql_data.uri + mysql_data.uri_len - 1;
	if (*slash_ptr == '/') {
		*slash_ptr = 0;
		mysql_data.uri_len--;
	}
	if (partial == false) {
		mysql_traverse_read(TDB_WRAP(ictx)->tdb, mysql_get_fid_traverse, &mysql_data);
	} else {
		for (mysql_data.wildcard_count = 0, i = 0; i < strlen(uri); i++) {
			if (uri[i] == '*') mysql_data.wildcard_count += 1;
		}

		switch (mysql_data.wildcard_count) {
		case 0: /* complete URI */
			partial = true;
			break;
		case 1: /* start and end only */
			mysql_data.endswith = strchr(uri, '*') + 1;
			mysql_data.startswith = talloc_strndup(NULL, uri, strlen(uri) - strlen(mysql_data.endswith) - 1);
			break;
		default:
			DEBUG(0, ("[%s:%d]: Too many wildcards found (1 maximum)\n", __FUNCTION__, __LINE__));
			talloc_free(mysql_data.uri);
			return MAPISTORE_ERR_NOT_FOUND;
		}

		if (partial == true) {
			mysql_traverse_read(TDB_WRAP(ictx)->tdb, mysql_get_fid_traverse_partial, &mysql_data);
			talloc_free(mysql_data.startswith);
		} else {
			mysql_traverse_read(TDB_WRAP(ictx)->tdb, mysql_get_fid_traverse, &mysql_data);
		}
	}

	talloc_free(mysql_data.uri);
	if (mysql_data.found) {
		*fmidp = mysql_data.fmid;
		*soft_deletedp = false; /* TODO: implement the feature */
		ret = MAPISTORE_SUCCESS;
	}
	else {
		ret = MAPISTORE_ERR_NOT_FOUND;
	}

	return ret;
}


static enum mapistore_error mysql_record_allocate_fmids(struct indexing_context *ictx,
						      const char *username,
						      int count,
						      uint64_t *fmidp)
{
	TDB_DATA		key, data;
	int			ret;
	uint64_t		GlobalCount;

	/* SANITY checks */
	MAPISTORE_RETVAL_IF(!ictx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!fmidp, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	/* Retrieve current counter */
	key.dptr = (unsigned char*)"GlobalCount";
	key.dsize = strlen((const char *)key.dptr);

	data = mysql_fetch(TDB_WRAP(ictx)->tdb, key);
	if (!data.dptr || !data.dsize) {
		GlobalCount = 1;
	}
	else {
		GlobalCount = strtoull((const char*)data.dptr, NULL, 16);
	}

	/* Save and increment the counter (reserve) */
	*fmidp = GlobalCount;
	GlobalCount += count,

	/* Store new counter */
	data.dptr = (unsigned char *) talloc_asprintf(ictx, "0x%.16"PRIx64, GlobalCount);
	data.dsize = strlen((const char *) data.dptr);
	ret = mysql_store(TDB_WRAP(ictx)->tdb, key, data, TDB_REPLACE);
	talloc_free(data.dptr);

	if (ret == -1) {
		DEBUG(3, ("[%s:%d]: Unable to create %s record: 0x%.16"PRIx64" \n", __FUNCTION__, __LINE__,
			  key.dptr, GlobalCount));
		return MAPISTORE_ERR_DATABASE_OPS;
	}

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
	TALLOC_CTX			*mem_ctx;
	struct indexing_context		*ictx;
	char				*connection_string = "mysql://root@localhost";
	bool				schema_created;
	char				*schema_file;
	MYSQL				*conn;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERROR, NULL);

	ictx = talloc_zero(mstore_ctx, struct indexing_context);

	ictx->data = create_connection(connection_string, &conn);
	OPENCHANGE_RETVAL_IF(!ictx->data, MAPI_E_NOT_INITIALIZED, ictx);
	if (!table_exists(ictx->data, INDEXING_TABLE)) {
		DEBUG(0, ("Creating schema for indexing on mysql %s",
			  connection_string));

		schema_file = talloc_asprintf(ictx, "%s/%s", MAPISTORE_LDIF, SCHEMA_FILE);
		schema_created = create_schema(ictx->data, schema_file);
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

	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}
