#include "mysql.h"

#include <time.h>
#include <util/debug.h>
#include <libmapi/mapicode.h>
#include <string.h>

static float timespec_diff_in_seconds(struct timespec *end, struct timespec *start)
{
	return ((float)((end->tv_sec * 1000000000 + end->tv_nsec) -
			(start->tv_sec * 1000000000 + start->tv_nsec)))
		/ 1000000000;
}

static bool parse_connection_string(TALLOC_CTX *mem_ctx,
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
		*user = talloc_zero_array(mem_ctx, char, user_size + 1);
		strncpy(*user, s, user_size);
		(*user)[user_size] = '\0';
		*passwd = talloc_zero_array(mem_ctx, char, 1);
		(*passwd)[0] = '\0';
	} else {
		// User
		int user_size = strchr(s, ':') - s;
		*user = talloc_zero_array(mem_ctx, char, user_size);
		strncpy(*user, s, user_size);
		(*user)[user_size] = '\0';
		// Password
		int passwd_size = strchr(s, '@') - strchr(s, ':') - 1;
		*passwd = talloc_zero_array(mem_ctx, char, passwd_size + 1);
		strncpy(*passwd, strchr(s, ':') + 1, passwd_size);
		(*passwd)[passwd_size] = '\0';
	}
	// Host
	int host_size = strchr(s, '/') - strchr(s, '@') - 1;
	*host = talloc_zero_array(mem_ctx, char, host_size + 1);
	strncpy(*host, strchr(s, '@') + 1, host_size);
	(*host)[host_size] = '\0';
	// Database name
	int db_size = strlen(strchr(s, '/') + 1);
	*db = talloc_zero_array(mem_ctx, char, db_size + 1);
	strncpy(*db, strchr(s, '/') + 1, db_size);
	(*db)[db_size] = '\0';

	return true;
}



MYSQL* create_connection(const char *connection_string, MYSQL **conn)
{
	TALLOC_CTX *mem_ctx;
	my_bool reconnect;
	char *host, *user, *passwd, *db, *sql;
	bool parsed;

	if (*conn != NULL) return *conn;

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	*conn = mysql_init(NULL);
	reconnect = true;
	mysql_options(*conn, MYSQL_OPT_RECONNECT, &reconnect);
	parsed = parse_connection_string(mem_ctx, connection_string,
					 &host, &user, &passwd, &db);
	if (!parsed) {
		DEBUG(0, ("Wrong connection string to mysql %s", connection_string));
		*conn = NULL;
		goto end;
	}
	// First try to connect to the database, if it fails try to create it
	if (mysql_real_connect(*conn, host, user, passwd, db, 0, NULL, 0)) {
		goto end;
	}

	// Try to create database
	if (!mysql_real_connect(*conn, host, user, passwd, NULL, 0, NULL, 0)) {
		// Nop
		DEBUG(0, ("Can't connect to mysql using %s", connection_string));
		conn = NULL;
	} else {
		// Connect it!, let's try to create database
		sql = talloc_asprintf(mem_ctx, "CREATE DATABASE %s", db);
		if (mysql_query(*conn, sql) != 0 || mysql_select_db(*conn, db) != 0) {
			DEBUG(0, ("Can't connect to mysql using %s",
				  connection_string));
			conn = NULL;
		}
	}
end:
	talloc_free(mem_ctx);
	return *conn;

}

enum MYSQLRESULT execute_query(MYSQL *conn, const char *sql)
{
	struct timespec start, end;
	float seconds_spent;

	clock_gettime(CLOCK_MONOTONIC, &start);
	if (mysql_query(conn, sql) != 0) {
		printf("Error on query `%s`: %s\n", sql, mysql_error(conn));
		DEBUG(5, ("Error on query `%s`: %s", sql, mysql_error(conn)));
		return MYSQL_ERROR;
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	seconds_spent = timespec_diff_in_seconds(&end, &start);
	if (seconds_spent > THRESHOLD_SLOW_QUERIES) {
		printf("Openchangedb mysql backend slow query!\n"
		       "\tQuery: `%s`\n\tTime: %.3f\n", sql, seconds_spent);
		DEBUG(5, ("Openchangedb mysql backend slow query!\n"
			  "\tQuery: `%s`\n\tTime: %.3f\n", sql, seconds_spent));
	}
	return MYSQL_SUCCESS;
}

enum MYSQLRESULT select_without_fetch(MYSQL *conn, const char *sql,
					    MYSQL_RES **res)
{
	enum MYSQLRESULT ret;

	ret = execute_query(conn, sql);
	OPENCHANGE_RETVAL_IF(ret != MYSQL_SUCCESS, ret, NULL);

	*res = mysql_store_result(conn);
	if (*res == NULL) {
		DEBUG(0, ("Error getting results of `%s`: %s", sql,
			  mysql_error(conn)));
		return MYSQL_ERROR;
	}

	if (mysql_num_rows(*res) == 0) {
		mysql_free_result(*res);
		return MYSQL_NOT_FOUND;
	}

	return MYSQL_SUCCESS;
}

enum MYSQLRESULT select_all_strings(TALLOC_CTX *mem_ctx, MYSQL *conn,
				   const char *sql,
				   struct StringArrayW_r **_results)
{
	MYSQL_RES *res;
	struct StringArrayW_r *results;
	uint32_t i;
	enum MYSQLRESULT ret;

	ret = select_without_fetch(conn, sql, &res);
	if (ret == MYSQL_NOT_FOUND) {
		results = talloc_zero(mem_ctx, struct StringArrayW_r);
		results->cValues = 0;
	} else if (ret == MYSQL_SUCCESS) {
		results = talloc_zero(mem_ctx, struct StringArrayW_r);
		results->cValues = mysql_num_rows(res);
	} else {
		// Unexpected error on sql query
		return ret;
	}

	results->lppszW = talloc_zero_array(results, const char *,
					    results->cValues);

	for (i = 0; i < results->cValues; i++) {
		MYSQL_ROW row = mysql_fetch_row(res);
		if (row == NULL) {
			DEBUG(0, ("Error getting row %d of `%s`: %s", i, sql,
				  mysql_error(conn)));
			mysql_free_result(res);
			return MYSQL_ERROR;
		}
		results->lppszW[i] = talloc_strdup(results, row[0]);
	}

	mysql_free_result(res);
	*_results = results;

	return MYSQL_SUCCESS;
}

enum MYSQLRESULT select_first_string(TALLOC_CTX *mem_ctx, MYSQL *conn,
				    const char *sql, const char **s)
{
	MYSQL_RES *res;
	enum MYSQLRESULT ret;

	ret = select_without_fetch(conn, sql, &res);
	OPENCHANGE_RETVAL_IF(ret != MYSQL_SUCCESS, ret, NULL);

	MYSQL_ROW row = mysql_fetch_row(res);
	if (row == NULL) {
		DEBUG(0, ("Error getting row of `%s`: %s", sql,
			  mysql_error(conn)));
		return MYSQL_ERROR;
	}

	*s = talloc_strdup(mem_ctx, row[0]);
	mysql_free_result(res);

	return MYSQL_SUCCESS;
}

enum MYSQLRESULT select_first_uint(MYSQL *conn, const char *sql,
				  uint64_t *n)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "select_first_uint");
	const char *result;
	enum MYSQLRESULT ret;

	ret = select_first_string(mem_ctx, conn, sql, &result);
	OPENCHANGE_RETVAL_IF(ret != MYSQL_SUCCESS, ret, mem_ctx);

	*n = strtoul(result, NULL, 10);
	talloc_free(mem_ctx);

	return MYSQL_SUCCESS;
}
