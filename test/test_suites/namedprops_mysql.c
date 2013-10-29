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

	ck_assert(initialize_database(conn) == MAPISTORE_SUCCESS);

	ck_assert(is_database_empty(conn) == false);
} END_TEST


Suite *namedprops_mysql_suite (void)
{
	Suite *s = suite_create ("Named Properties Mysql Backend");

	TCase *tc_core = tcase_create("Core");
	tcase_add_test(tc_core, test_parse_connection_without_password);
	tcase_add_test(tc_core, test_parse_connection_with_password);
	tcase_add_test(tc_core, test_parse_connection_fail);

	TCase *tc_mysql = tcase_create("Mysql");
	tcase_add_checked_fixture(tc_mysql, mysql_setup, mysql_teardown);
	tcase_add_test(tc_mysql, test_is_schema_created);
	tcase_add_test(tc_mysql, test_initialize_database);

	suite_add_tcase(s, tc_core);
	suite_add_tcase(s, tc_mysql);

	return s;
}
