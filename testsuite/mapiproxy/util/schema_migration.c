/*
   Schema migration util procedures calling Python code unit tests

   OpenChange Project

   Copyright (C) Enrique J. Hernández <ejhernandez@zentyal.com> 2015

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

#include "testsuite/testsuite.h"
#include "testsuite/testsuite_common.h"
#include "mapiproxy/util/schema_migration.c"

/* Global test variables */
static TALLOC_CTX 	*mem_ctx;
static MYSQL		*conn;

// v Unit test ----------------------------------------------------------------

START_TEST (test_migrate_openchangedb_schema) {
	const char *connection_string;
	int	   ret;

	connection_string = "mysql://"OC_TESTSUITE_MYSQL_USER":"OC_TESTSUITE_MYSQL_PASS
			    "@"OC_TESTSUITE_MYSQL_HOST"/"OC_TESTSUITE_MYSQL_DB;

	ret = migrate_openchangedb_schema(mem_ctx, connection_string);
	ck_assert_int_eq(ret, 0);

} END_TEST

START_TEST (test_migrate_indexing_schema) {
	const char *connection_string;
	int	   ret;

	connection_string = "mysql://"OC_TESTSUITE_MYSQL_USER":"OC_TESTSUITE_MYSQL_PASS
			    "@"OC_TESTSUITE_MYSQL_HOST"/"OC_TESTSUITE_MYSQL_DB;

	ret = migrate_indexing_schema(mem_ctx, connection_string);
	ck_assert_int_eq(ret, 0);

} END_TEST

START_TEST (test_migrate_named_properties_schema) {
	const char *connection_string;
	int	   ret;

	connection_string = "mysql://"OC_TESTSUITE_MYSQL_USER":"OC_TESTSUITE_MYSQL_PASS
			    "@"OC_TESTSUITE_MYSQL_HOST"/"OC_TESTSUITE_MYSQL_DB;

	ret = migrate_named_properties_schema(mem_ctx, connection_string);
	ck_assert_int_eq(ret, 0);

} END_TEST

// ^ unit tests ---------------------------------------------------------------

// v suite definition ---------------------------------------------------------

static void unchecked_schema_migration_setup(void)
{
	const char	*connection_string;
	bool		connection_created;

	connection_string = "mysql://"OC_TESTSUITE_MYSQL_USER":"OC_TESTSUITE_MYSQL_PASS
			    "@"OC_TESTSUITE_MYSQL_HOST"/"OC_TESTSUITE_MYSQL_DB;
	connection_created = create_connection(connection_string, &conn);
	ck_assert(connection_created);
}

static void unchecked_schema_migration_teardown(void)
{
	drop_mysql_database(conn, OC_TESTSUITE_MYSQL_DB);
}

static void checked_schema_migration_setup(void)
{
	mem_ctx = talloc_named(NULL, 0, __FUNCTION__);
}

static void checked_schema_migration_teardown(void)
{
	talloc_free(mem_ctx);
}

Suite *mapiproxy_util_schema_migration_suite(void)
{
	Suite *s = suite_create("Mapiproxy/util/schema_migration");

	TCase *tc = tcase_create("schema migration tests");
	tcase_add_unchecked_fixture(tc, unchecked_schema_migration_setup, unchecked_schema_migration_teardown);
	tcase_add_checked_fixture(tc, checked_schema_migration_setup, checked_schema_migration_teardown);

	tcase_add_test(tc, test_migrate_openchangedb_schema);
	tcase_add_test(tc, test_migrate_indexing_schema);
	tcase_add_test(tc, test_migrate_named_properties_schema);

	suite_add_tcase(s, tc);

	return s;
}
