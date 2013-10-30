#include "namedprops_mysql.h"
#include "test_common.h"

#include "mapiproxy/libmapistore/backends/namedprops_mysql.c"


START_TEST (test_parse_connection_with_password) {
	TALLOC_CTX *mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	char *host, *user, *passwd, *db;
	bool ret;

	ret = parse_connection_string(mem_ctx,
				      "mysql://root:secret@localhost/mysql",
				      &host, &user, &passwd, &db);
	ck_assert(ret == true);
	ck_assert_str_eq(user, "root");
	ck_assert_str_eq(passwd, "secret");
	ck_assert_str_eq(host, "localhost");
	ck_assert_str_eq(db, "mysql");

	ret = parse_connection_string(mem_ctx,
				      "mysql://lang:langpass@10.11.12.13/openchange_db",
				      &host, &user, &passwd, &db);
	ck_assert(ret == true);
	ck_assert_str_eq(user, "lang");
	ck_assert_str_eq(passwd, "langpass");
	ck_assert_str_eq(host, "10.11.12.13");
	ck_assert_str_eq(db, "openchange_db");

	talloc_free(mem_ctx);
} END_TEST


START_TEST (test_parse_connection_without_password) {
	TALLOC_CTX *mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	char *host, *user, *passwd, *db;
	bool ret;

	ret = parse_connection_string(mem_ctx, "mysql://root@localhost/mysql",
				      &host, &user, &passwd, &db);
	ck_assert(ret == true);
	ck_assert_str_eq(user, "root");
	ck_assert_str_eq(passwd, "");
	ck_assert_str_eq(host, "localhost");
	ck_assert_str_eq(db, "mysql");

	ret = parse_connection_string(mem_ctx,
				      "mysql://chuck@192.168.42.42/norris",
		 		      &host, &user, &passwd, &db);
	ck_assert(ret == true);
	ck_assert_str_eq(user, "chuck");
	ck_assert_str_eq(passwd, "");
	ck_assert_str_eq(host, "192.168.42.42");
	ck_assert_str_eq(db, "norris");

	talloc_free(mem_ctx);
} END_TEST


START_TEST (test_parse_connection_fail) {
	TALLOC_CTX *mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	char *host, *user, *passwd, *db;

	bool ret = parse_connection_string(mem_ctx, "ldb:///tmp/lalal",
					  &host, &user, &passwd, &db);
	ck_assert(ret == false);

	ret = parse_connection_string(mem_ctx, "mysql://ÑAÑÑAÑAÑAÑAÑA",
				      &host, &user, &passwd, &db);
	ck_assert(ret == false);

	talloc_free(mem_ctx);
} END_TEST


MYSQL *conn = NULL;

void mysql_setup(void)
{
	if (conn) return;

	conn = mysql_init(NULL);
	CHECK_MYSQL_ERROR;

	mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASS,
			   NULL, 0, NULL, 0);
	CHECK_MYSQL_ERROR;

	mysql_query(conn, "DROP DATABASE " MYSQL_DB);
	mysql_query(conn, "CREATE DATABASE " MYSQL_DB);
	CHECK_MYSQL_ERROR;

	mysql_select_db(conn, MYSQL_DB);
	CHECK_MYSQL_ERROR;
}

void mysql_teardown(void)
{
	mysql_query(conn, "DROP DATABASE " MYSQL_DB);
	CHECK_MYSQL_ERROR;
}


START_TEST (test_is_schema_created) {
	ck_assert(is_schema_created(conn) == false);
	ck_assert(is_database_empty(conn) == true);

	ck_assert(create_schema(conn) == true);

	ck_assert(is_schema_created(conn) == true);
	ck_assert(is_database_empty(conn) == true);
} END_TEST


START_TEST (test_initialize_database) {
	ck_assert(is_database_empty(conn) == true);

	ck_assert_int_eq(initialize_database(conn), MAPISTORE_SUCCESS);

	ck_assert(is_database_empty(conn) == false);
} END_TEST


TALLOC_CTX *mem_ctx;
struct namedprops_context *nprops;

void mysql_q_setup(void)
{
	mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	char *database;
	if (strlen(MYSQL_PASS) == 0) {
		database = talloc_asprintf(mem_ctx, "mysql://" MYSQL_USER "@"
					   MYSQL_HOST "/" MYSQL_DB);
	} else {
		database = talloc_asprintf(mem_ctx, "mysql://" MYSQL_USER ":"
					   MYSQL_PASS "@" MYSQL_HOST "/"
					   MYSQL_DB);
	}
	nprops = talloc_zero(mem_ctx, struct namedprops_context);
	if (mapistore_namedprops_mysql_init(mem_ctx, database, &nprops) != MAPISTORE_SUCCESS) {
		fprintf(stderr, "Error initializing namedprops %d", errno);
	}
}

void mysql_q_teardown(void)
{
	mysql_query(nprops->data, "DROP DATABASE " MYSQL_DB);
	talloc_free(mem_ctx);
}

START_TEST (test_next_unused_id) {
	ck_assert_int_eq(next_unused_id(nprops), 38392);
} END_TEST


Suite *namedprops_mysql_suite(void)
{
	Suite *s = suite_create ("Named Properties Mysql Backend");

	TCase *tc_core = tcase_create("Core");
	tcase_add_test(tc_core, test_parse_connection_without_password);
	tcase_add_test(tc_core, test_parse_connection_with_password);
	tcase_add_test(tc_core, test_parse_connection_fail);

	TCase *tc_mysql = tcase_create("Mysql initialization");
	// We insert 1410 entries to mysql.
	// It take a little bit more than 4 seconds (the default timeout)
	tcase_set_timeout(tc_mysql, 60);
	tcase_add_checked_fixture(tc_mysql, mysql_setup, mysql_teardown);
	tcase_add_test(tc_mysql, test_is_schema_created);
	tcase_add_test(tc_mysql, test_initialize_database);

	TCase *tc_mysql_q = tcase_create("Mysql queries");
	tcase_add_unchecked_fixture(tc_mysql_q, mysql_q_setup, mysql_q_teardown);
	tcase_add_test(tc_mysql_q, test_next_unused_id);

	suite_add_tcase(s, tc_core);
	suite_add_tcase(s, tc_mysql);
	suite_add_tcase(s, tc_mysql_q);

	return s;
}
