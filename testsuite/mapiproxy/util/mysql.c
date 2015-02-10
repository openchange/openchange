/*
   OpenChange Unit Testing

   OpenChange Project

   Copyright (C) Kamen Mazdrashki <kamenim@openchange.org> 2014

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

#include "testsuite.h"
#include "testsuite_common.h"
#include "mapiproxy/util/mysql.c"

/* Global test variables */
static TALLOC_CTX 	*mem_ctx;
static MYSQL		*conn;

#define MYSQL_TMP_SCHEMA	"/tmp/fake_schema.sql"

// v Unit test ----------------------------------------------------------------

START_TEST (test_parse_connection_string_fail) {
	char *host;
	char *user;
	char *passwd;
	char *db;
	int port;
	bool parsed;

	// mysql://user:pass@localhost/db

	/* invalid input parameters */
	parsed = parse_connection_string(mem_ctx, NULL,
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(!parsed, "NULL connection string, but parsing succeeded");

	parsed = parse_connection_string(mem_ctx, "",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(!parsed, "Empty connection string, but parsing succeeded");

	parsed = parse_connection_string(mem_ctx, "mysql://user:pass@localhost/db",
					 NULL, &port, &user, &passwd, &db);
	ck_assert_msg(!parsed, "NULL host parameter, but parsing succeeded");

	parsed = parse_connection_string(mem_ctx, "mysql://user:pass@localhost/db",
					 &host, NULL, &user, &passwd, &db);
	ck_assert_msg(!parsed, "NULL port parameter, but parsing succeeded");

	parsed = parse_connection_string(mem_ctx, "mysql://user:pass@localhost/db",
					 &host, &port, NULL, &passwd, &db);
	ck_assert_msg(!parsed, "NULL user parameter, but parsing succeeded");

	parsed = parse_connection_string(mem_ctx, "mysql://user:pass@localhost/db",
					 &host, &port, &user, &passwd, NULL);
	ck_assert_msg(!parsed, "NULL db parameter, but parsing succeeded");

	/* invalid prefix */
	parsed = parse_connection_string(mem_ctx, "sql://user:pass@localhost/db",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(!parsed, "Truncated mysql prefix, but parsing succeeded");

	parsed = parse_connection_string(mem_ctx, "://user:pass@localhost/db",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(!parsed, "No mysql prefix, but parsing succeeded");

	parsed = parse_connection_string(mem_ctx, "user:pass@localhost/db",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(!parsed, "No prefix at all, but parsing succeeded");

	/* missing username */
	parsed = parse_connection_string(mem_ctx, "mysql://:pass@localhost/db",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(!parsed, "No username given, but parsing succeeded");

	parsed = parse_connection_string(mem_ctx, "mysql://:pass@localhost:12/db",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(!parsed, "No username given, but parsing succeeded");

	/* missing hostname */
	parsed = parse_connection_string(mem_ctx, "mysql://user:pass@/db",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(!parsed, "No hostname given, but parsing succeeded");

	parsed = parse_connection_string(mem_ctx, "mysql://user:pass@:12/db",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(!parsed, "No hostname given, but parsing succeeded");

	/* missing db name */
	parsed = parse_connection_string(mem_ctx, "mysql://user:pass@localhost",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(!parsed, "No db name given, but parsing succeeded");

	parsed = parse_connection_string(mem_ctx, "mysql://user:pass@localhost/",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(!parsed, "No db name given, but parsing succeeded");

	parsed = parse_connection_string(mem_ctx, "mysql://user:pass@localhost:123",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(!parsed, "No db name given, but parsing succeeded");

	/* invliad port name */
	parsed = parse_connection_string(mem_ctx, "mysql://user:pass@localhost:batman/db",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(!parsed, "Invalid port, but parsing succeeded");
} END_TEST


START_TEST (test_parse_connection_string_success) {
	char *host;
	char *user;
	char *passwd;
	char *db;
	int port;
	bool parsed;

	// mysql://user:pass@localhost/db

	/* valid string with all fields set */
	parsed = parse_connection_string(mem_ctx, "mysql://user:pass@localhost:1234/db",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(parsed, "parse_connection_string failed for full string");
	ck_assert_str_eq(host, "localhost");
	ck_assert_int_eq(port, 1234);
	ck_assert_str_eq(user, "user");
	ck_assert_str_eq(passwd, "pass");
	ck_assert_str_eq(db, "db");

	/* connect without port but with password */
	parsed = parse_connection_string(mem_ctx, "mysql://user:pass@localhost/db",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(parsed, "parse_connection_string failed for full string");
	ck_assert_str_eq(host, "localhost");
	ck_assert_msg(port == 0, "We expect no port to be parsed out");
	ck_assert_str_eq(user, "user");
	ck_assert_str_eq(passwd, "pass");
	ck_assert_str_eq(db, "db");

	/* connect with port and without password */
	parsed = parse_connection_string(mem_ctx, "mysql://user@localhost:12/db",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(parsed, "parse_connection_string failed for full string");
	ck_assert_str_eq(host, "localhost");
	ck_assert_int_eq(port, 12);
	ck_assert_str_eq(user, "user");
	ck_assert_str_eq(db, "db");
	ck_assert_msg(passwd == NULL, "We expect no password to be parsed out");

	/* connect without port and without password */
	parsed = parse_connection_string(mem_ctx, "mysql://user@localhost/db",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(parsed, "parse_connection_string failed for full string");
	ck_assert_str_eq(host, "localhost");
	ck_assert_msg(port == 0, "We expect no port to be parsed out");
	ck_assert_str_eq(user, "user");
	ck_assert_str_eq(db, "db");
	ck_assert_msg(passwd == NULL, "We expect no password to be parsed out");

	/* case sensitivity */
	parsed = parse_connection_string(mem_ctx, "MySQL://User:Pass@LocalHost/Db",
					 &host, &port, &user, &passwd, &db);
	ck_assert_msg(parsed, "parse_connection_string failed for full string");
	ck_assert_str_eq(host, "LocalHost");
	ck_assert_int_eq(port, 0);
	ck_assert_str_eq(user, "User");
	ck_assert_str_eq(passwd, "Pass");
	ck_assert_str_eq(db, "Db");

} END_TEST

START_TEST (test_create_schema) {
	bool		schema_created;
	int		ret;
	const char	*tmp_schema;
	char		str[] = "invalid schema file";
        char            simple_schema[] = "CREATE TABLE IF NOT EXISTS `named_properties` ("
                         "`id` int(10) unsigned NOT NULL AUTO_INCREMENT,"
                         "`type` TINYINT(1) NOT NULL,"
                         "`propType` INT(10) unsigned NOT NULL,"
                         "`oleguid` VARCHAR(255) NOT NULL,"
                         "`mappedId` INT(10) unsigned NOT NULL,"
                         "`propId` INT(10) unsigned DEFAULT NULL,"
                         "`propName` VARCHAR(255) DEFAULT NULL,"
                         "`oom` VARCHAR(255) DEFAULT NULL,"
                         "`canonical` VARCHAR(255) DEFAULT NULL,"
                         "PRIMARY KEY(`id`)"
                      ") ENGINE=InnoDB CHARSET=utf8;";
	FILE		*fp;

	tmp_schema = MYSQL_TMP_SCHEMA;

	/* check sanity checks */
	schema_created = create_schema(NULL, NULL);
	ck_assert(!schema_created);

	/* test non existent schema file */
	unlink(tmp_schema);

	schema_created = create_schema(conn, tmp_schema);
	ck_assert(!schema_created);

	/* test empty schema file */
	ret = unlink(tmp_schema);
	ck_assert_int_eq(ret, -1);

	fp = fopen(tmp_schema, "w+");
	ck_assert(fp != NULL);
	fclose(fp);

	schema_created = create_schema(conn, tmp_schema);
	ck_assert(!schema_created);

	/* test invalid schema file */
	fp = fopen(tmp_schema, "w+");
	ck_assert(fp != NULL);
	fwrite(str, 1, sizeof(str), fp);
	fclose(fp);

	schema_created = create_schema(conn, tmp_schema);
	ck_assert(!schema_created);

	/* test simplified real schema file */
	fp = fopen(tmp_schema, "w");
	ck_assert(fp != NULL);
	fwrite(simple_schema, 1, sizeof(simple_schema), fp);
	fclose(fp);

	schema_created = create_schema(conn, tmp_schema);
	ck_assert(schema_created);

	ret = unlink(tmp_schema);
	ck_assert_int_eq(ret, 0);

} END_TEST

// ^ unit tests ---------------------------------------------------------------

// v suite definition ---------------------------------------------------------

static void unchecked_mysql_util_setup(void)
{
	const char	*connection_string;
	bool		connection_created;

	connection_string = "mysql://"OC_TESTSUITE_MYSQL_USER":"OC_TESTSUITE_MYSQL_PASS
			    "@"OC_TESTSUITE_MYSQL_HOST"/"OC_TESTSUITE_MYSQL_DB;
	connection_created = create_connection(connection_string, &conn);
	ck_assert(connection_created);
}

static void unchecked_mysql_util_teardown(void)
{
	drop_mysql_database(conn, OC_TESTSUITE_MYSQL_DB);
}

static void checked_mysql_util_setup(void)
{
	mem_ctx = talloc_named(NULL, 0, __FUNCTION__);
}

static void checked_mysql_util_teardown(void)
{
	talloc_free(mem_ctx);
}

Suite *mapiproxy_util_mysql_suite(void)
{
	Suite *s = suite_create("Mapiproxy/util/mysql");

	TCase *tc = tcase_create("mysql utility functions");
	tcase_add_unchecked_fixture(tc, unchecked_mysql_util_setup, unchecked_mysql_util_teardown);
	tcase_add_checked_fixture(tc, checked_mysql_util_setup, checked_mysql_util_teardown);

	tcase_add_test(tc, test_parse_connection_string_fail);
	tcase_add_test(tc, test_parse_connection_string_success);
	tcase_add_test(tc, test_create_schema);

	suite_add_tcase(s, tc);

	return s;
}
