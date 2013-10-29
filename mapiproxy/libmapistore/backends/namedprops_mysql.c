#include "namedprops_mysql.h"
#include "../mapistore.h"
#include "../mapistore_private.h"
#include <string.h>
#include <stdlib.h>
#include <mysql/mysql.h>
#include <ldb.h>

#define SCHEMA_FILE "named_properties_schema.sql"
#define TABLE_NAME "named_properties"


static enum mapistore_error get_mapped_id(struct namedprops_context *self,
					  struct MAPINAMEID nameid,
					  uint16_t *propID)
{
	return MAPISTORE_SUCCESS;
}

static uint16_t next_unused_id(struct namedprops_context *self)
{
	uint16_t unused_id = 0;

	return ++unused_id;
}

static enum mapistore_error create_id(struct namedprops_context *self,
				      struct MAPINAMEID nameid,
				      uint16_t mapped_id)
{
	return MAPISTORE_SUCCESS;
}

static enum mapistore_error get_nameid(struct namedprops_context *self,
				       uint16_t propID,
				       TALLOC_CTX *mem_ctx,
				       struct MAPINAMEID **nameidp)
{
	return MAPISTORE_SUCCESS;
}

static enum mapistore_error get_nameid_type(struct namedprops_context *self,
					    uint16_t propID,
					    uint16_t *propTypeP)
{
	return MAPISTORE_SUCCESS;
}

static enum mapistore_error transaction_start(struct namedprops_context *self)
{
	MYSQL *conn = self->data;
	int res = mysql_query(conn, "START TRANSACTION");
	MAPISTORE_RETVAL_IF(res, MAPISTORE_ERR_DATABASE_OPS, NULL);
	return MAPISTORE_SUCCESS;
}

static enum mapistore_error transaction_commit(struct namedprops_context *self)
{
	MYSQL *conn = self->data;
	int res = mysql_query(conn, "COMMIT");
	MAPISTORE_RETVAL_IF(res, MAPISTORE_ERR_DATABASE_OPS, NULL);
	return MAPISTORE_SUCCESS;
}

static int mapistore_namedprops_mysql_destructor(struct namedprops_context *self)
{
	MYSQL *conn = self->data;
	mysql_close(conn);
	return 0;
}

static bool parse_connection_string(TALLOC_CTX *local_mem_ctx,
				   const char *connection_string,
				   char **host, char **user, char **passwd,
				   char **db)
{
	// connection_string has format mysql://user[:pass]@host/database
	int prefix_size = strlen("mysql://");
	const char *s = connection_string + prefix_size;
	if (!connection_string || strlen(connection_string) < prefix_size ||
	    !strstr(connection_string, "mysql://") || !strchr(s, '@') ||
	    !strchr(s, '/')) {
		// Invalid format
		return false;
	}
	if (strchr(s, ':') == NULL || strchr(s, ':') > strchr(s, '@')) {
		// No password
		int user_size = strchr(s, '@') - s;
		*user = talloc_zero_array(local_mem_ctx, char, user_size + 1);
		strncpy(*user, s, user_size);
		(*user)[user_size] = '\0';
		*passwd = talloc_zero_array(local_mem_ctx, char, 1);
		(*passwd)[0] = '\0';
	} else {
		// User
		int user_size = strchr(s, ':') - s;
		*user = talloc_zero_array(local_mem_ctx, char, user_size);
		strncpy(*user, s, user_size);
		(*user)[user_size] = '\0';
		// Password
		int passwd_size = strchr(s, '@') - strchr(s, ':') - 1;
		*passwd = talloc_zero_array(local_mem_ctx, char, passwd_size + 1);
		strncpy(*passwd, strchr(s, ':') + 1, passwd_size);
		(*passwd)[passwd_size] = '\0';
	}
	// Host
	int host_size = strchr(s, '/') - strchr(s, '@') - 1;
	*host = talloc_zero_array(local_mem_ctx, char, host_size + 1);
	strncpy(*host, strchr(s, '@') + 1, host_size);
	(*host)[host_size] = '\0';
	// Database name
	int db_size = strlen(strchr(s, '/') + 1);
	*db = talloc_zero_array(local_mem_ctx, char, db_size + 1);
	strncpy(*db, strchr(s, '/') + 1, db_size);
	(*db)[db_size] = '\0';

	return true;
}

static bool is_schema_created(MYSQL *conn)
{
	MYSQL_RES *res = mysql_list_tables(conn, TABLE_NAME);
	bool created = mysql_num_rows(res) == 1;
	mysql_free_result(res);
	return created;
}

static bool create_schema(MYSQL *conn)
{
	TALLOC_CTX *mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	char *filename = talloc_asprintf(mem_ctx, "%s/" SCHEMA_FILE,
					 mapistore_namedprops_get_ldif_path());
	FILE *f = fopen(filename, "r");
	MAPISTORE_RETVAL_IF(!f, MAPISTORE_ERR_BACKEND_INIT, mem_ctx);
	fseek(f, 0, SEEK_END);
	int sql_size = ftell(f);
	rewind(f);
	char *sql = talloc_zero_array(mem_ctx, char, sql_size + 1);
	int bytes_read = fread(sql, sizeof(char), sql_size, f);
	if (bytes_read != sql_size) {
		talloc_free(mem_ctx);
		fclose(f);
		return false;
	}

	bool ret = mysql_query(conn, sql) ? false : true;

	talloc_free(mem_ctx);
	fclose(f);

	return ret;
}

static bool is_database_empty(MYSQL *conn)
{
	if (mysql_query(conn, "SELECT count(*) FROM " TABLE_NAME)) {
		// Query failed
		return true;
	} else {
		MYSQL_RES *res = mysql_store_result(conn);
		MYSQL_ROW row = mysql_fetch_row(res);
		int n = atoi(row[0]);
		mysql_free_result(res);
		return n == 0;
	}
}

static bool insert_ldif_entry(MYSQL *conn, struct ldb_message *ldif)
{
	return true;
}

static enum mapistore_error initialize_database(MYSQL *conn)
{
	if (!create_schema(conn)) {
		return MAPISTORE_ERR_DATABASE_OPS;
	}
	TALLOC_CTX *mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	struct tevent_context *ev = tevent_context_init(mem_ctx);
	MAPISTORE_RETVAL_IF(!ev, MAPISTORE_ERR_BACKEND_INIT, mem_ctx);
	struct ldb_context *ldb_ctx = ldb_init(mem_ctx, ev);
	MAPISTORE_RETVAL_IF(!ldb_ctx, MAPISTORE_ERR_BACKEND_INIT, mem_ctx);

	char *filename = talloc_asprintf(mem_ctx, "%s/mapistore_namedprops.ldif",
					 mapistore_namedprops_get_ldif_path());
	FILE *f = fopen(filename, "r");
	MAPISTORE_RETVAL_IF(!f, MAPISTORE_ERROR, mem_ctx);

	struct ldb_ldif *ldif;
	while ((ldif = ldb_ldif_read_file(ldb_ctx, f))) {
		struct ldb_message *normalized_msg;
		int ret = ldb_msg_normalize(ldb_ctx, mem_ctx, ldif->msg,
					    &normalized_msg);
		MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERR_DATABASE_INIT, mem_ctx);
		bool inserted = insert_ldif_entry(conn, normalized_msg);
		ldb_ldif_read_free(ldb_ctx, ldif);
		if (!inserted) {
			fclose(f);
			MAPISTORE_RETVAL_IF(true, MAPISTORE_ERR_DATABASE_OPS,
					    mem_ctx);
		}
	}

	talloc_free(mem_ctx);
	fclose(f);

	return MAPISTORE_SUCCESS;
}

enum mapistore_error mapistore_namedprops_mysql_init(TALLOC_CTX *mem_ctx,
						     const char *connection_string,
						     struct namedprops_context **ctx)
{
	// 0) Create context
	struct namedprops_context *nprops = talloc_zero(mem_ctx, struct namedprops_context);
	nprops->backend_type = talloc_strdup(mem_ctx, "mysql");
	nprops->create_id = create_id;
	nprops->get_mapped_id = get_mapped_id;
	nprops->get_nameid = get_nameid;
	nprops->get_nameid_type = get_nameid_type;
	nprops->next_unused_id = next_unused_id;
	nprops->transaction_commit = transaction_commit;
	nprops->transaction_start = transaction_start;

	// 1) Establish mysql connection
	MYSQL *conn = mysql_init(NULL);
	TALLOC_CTX *local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	char *host, *user, *passwd, *db;
	bool parsed = parse_connection_string(local_mem_ctx, connection_string,
					  &host, &user, &passwd, &db);
	if (!parsed || mysql_real_connect(conn, host, user, passwd, db, 0, NULL, 0) == NULL) {
		DEBUG(0, ("Can't connect to mysql using %s", connection_string));
		MAPISTORE_RETVAL_IF(1, MAPISTORE_ERR_BACKEND_INIT, local_mem_ctx);
	}
	nprops->data = conn;
	talloc_set_destructor(nprops, mapistore_namedprops_mysql_destructor);

	// 2) Initialize database
	bool should_initialize_database = !is_schema_created(conn) || is_database_empty(conn);
	if (should_initialize_database) {
		enum mapistore_error ret = initialize_database(conn);
		MAPISTORE_RETVAL_IF(ret != MAPISTORE_SUCCESS, ret, local_mem_ctx);
	}

	*ctx = nprops;
	talloc_free(local_mem_ctx);

	return MAPISTORE_SUCCESS;
}
