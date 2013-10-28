#include "namedprops_mysql.h"
#include "../mapistore.h"
#include "../mapistore_private.h"
#include <string.h>
#include <mysql/mysql.h>


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
	return MAPISTORE_SUCCESS;
}

static enum mapistore_error transaction_commit(struct namedprops_context *self)
{
	return MAPISTORE_SUCCESS;
}

static int mapistore_namedprops_mysql_destructor(struct namedprops_context *nprops)
{
	MYSQL *conn = nprops->data;
	mysql_close(conn);
	return 0;
}

static MYSQL *connect_to_database(MYSQL *conn, const char *connection_string)
{
	// connection_string has format mysql://user[:pass]@host/database
	TALLOC_CTX *local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	char *host, *user, *passwd, *db;

	int prefix_size = strlen("mysql://");
	if (!connection_string || strlen(connection_string) < prefix_size)
		return NULL;
	const char *s = connection_string + prefix_size;
	if (strchr(s, ':') == NULL || strchr(s, ':') > strchr(s, '@')) {
		// No password
		int user_size = strchr(s, '@') - s;
		user = talloc_zero_array(local_mem_ctx, char, user_size + 1);
		strncpy(user, s, user_size);
		user[user_size] = '\0';
		passwd = talloc_zero_array(local_mem_ctx, char, 1);
		passwd[0] = '\0';
	} else {
		// User
		int user_size = strchr(s, ':') - s;
		user = talloc_zero_array(local_mem_ctx, char, user_size);
		strncpy(user, s, user_size);
		user[user_size] = '\0';
		// Password
		int passwd_size = strchr(s, '@') - strchr(s, ':') - 1;
		passwd = talloc_zero_array(local_mem_ctx, char, passwd_size + 1);
		strncpy(passwd, strchr(s, ':') + 1, passwd_size);
		passwd[passwd_size] = '\0';
	}
	// Host
	int host_size = strchr(s, '/') - strchr(s, '@') - 1;
	host = talloc_zero_array(local_mem_ctx, char, host_size + 1);
	strncpy(host, strchr(s, '@') + 1, host_size);
	host[host_size] = '\0';
	// Database name
	int db_size = strlen(strchr(s, '/') + 1);
	db = talloc_zero_array(local_mem_ctx, char, db_size + 1);
	strncpy(db, strchr(s, '/') + 1, db_size);
	db[db_size] = '\0';

	MYSQL *ret = mysql_real_connect(conn, host, user, passwd, db, 0, NULL, 0);

	talloc_free(local_mem_ctx);
	return ret;
}


enum mapistore_error mapistore_namedprops_mysql_init(TALLOC_CTX *mem_ctx,
						     const char *database,
						     struct namedprops_context **_nprops)
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
	if (connect_to_database(conn, database) == NULL) {
		DEBUG(0, ("Connection to mysql failed using %s", database));
		MAPISTORE_RETVAL_IF(1, MAPISTORE_ERR_BACKEND_INIT, NULL);
	}
	nprops->data = conn;
	talloc_set_destructor(nprops, mapistore_namedprops_mysql_destructor);

	// 2) Populate database
	//TODO

	*_nprops = nprops;

	return MAPISTORE_SUCCESS;
}
