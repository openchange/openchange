/*
   OpenChange Unit Testing

   OpenChange Project

   Copyright (C) Julien Kerihuel 2014
   copyright (C) Jesús García Sáez 2014

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
#include <libmapistore/mapistore.h>
#include <libmapistore/mapistore_errors.h>
#include <libmapistore/backends/namedprops_mysql.h>
#include "libmapistore/backends/namedprops_mysql.c"

#include <mysql/mysql.h>
#include <mysql/mysqld_error.h>

#define	NAMEDPROPS_MYSQL_HOST		"127.0.0.1"
#define	NAMEDPROPS_MYSQL_USER		"root"
#define	NAMEDPROPS_MYSQL_DB		"myapp_test"
#define	NAMEDPROPS_MYSQL_SCHEMA_PATH	"setup/mapistore"

/* Global variables used for test fixture */
MYSQL	*conn;

START_TEST(test_parameters) {
	TALLOC_CTX			*mem_ctx;
	struct loadparm_context		*lp_ctx;
	enum mapistore_error		retval;
	struct namedprops_mysql_params	p;
	bool				bret;

	memset(&p, 0, sizeof (struct namedprops_mysql_params));

	mem_ctx = talloc_named(NULL, 0, "test_parameters");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	/* check sanity check compliance */
	retval = mapistore_namedprops_mysql_parameters(NULL, &p);
	ck_assert(retval == MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_namedprops_mysql_parameters(lp_ctx, NULL);
	ck_assert(retval == MAPISTORE_ERR_INVALID_PARAMETER);

	/* check missing username */
	retval = mapistore_namedprops_mysql_parameters(lp_ctx, &p);
	ck_assert(retval == MAPISTORE_ERR_BACKEND_INIT);

	bret = lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_user", "root");
	ck_assert(bret == true);

	/* check missing database */
	retval = mapistore_namedprops_mysql_parameters(lp_ctx, &p);
	ck_assert(p.user && (retval == MAPISTORE_ERR_BACKEND_INIT));

	bret = lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_db", "openchange");
	ck_assert(bret == true);

	/* check sock or host */
	retval = mapistore_namedprops_mysql_parameters(lp_ctx, &p);
	ck_assert(p.user && p.db && (retval == MAPISTORE_ERR_BACKEND_INIT));

	bret = lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_host", "localhost");
	ck_assert(bret == true);

	retval = mapistore_namedprops_mysql_parameters(lp_ctx, &p);
	ck_assert(retval == MAPISTORE_SUCCESS);

	/* test all parametric options */
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_sock", "/tmp/mysql.sock") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_host", "localhost") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_user", "root") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_pass", "openchange") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_port", "3305") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_db", "db_openchange") == true));

	retval = mapistore_namedprops_mysql_parameters(lp_ctx, &p);
	ck_assert(retval == MAPISTORE_SUCCESS);

	ck_assert_str_eq(p.sock, "/tmp/mysql.sock");
	ck_assert_str_eq(p.host, "localhost");
	ck_assert_str_eq(p.user, "root");
	ck_assert_str_eq(p.pass, "openchange");
	ck_assert_str_eq(p.db, "db_openchange");
	ck_assert_int_eq(p.port, 3305);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST

static void checked_mysql_setup(void)
{
	TALLOC_CTX	*mem_ctx = NULL;
	MYSQL		*rconn = NULL;
	char		*query = NULL;
	int		ret;

	mem_ctx = talloc_named(NULL, 0, "checked_mysql_setup");
	ck_assert(mem_ctx != NULL);

	conn = mysql_init(NULL);
	ck_assert(conn != NULL);

	rconn = mysql_real_connect(conn, NAMEDPROPS_MYSQL_HOST,
			   NAMEDPROPS_MYSQL_USER, NULL,
			   NULL, 0, NULL, 0);
	ck_assert(rconn != NULL);

	query = talloc_asprintf(mem_ctx, "DROP DATABASE %s", NAMEDPROPS_MYSQL_DB);
	ck_assert(query != NULL);
	ret = mysql_query(conn, query);
	talloc_free(query);

	query = talloc_asprintf(mem_ctx, "CREATE DATABASE %s", NAMEDPROPS_MYSQL_DB);
	ck_assert(query != NULL);
	ret = mysql_query(conn, query);
	talloc_free(query);
	ck_assert_int_eq(ret, 0);

	ret = mysql_select_db(conn, NAMEDPROPS_MYSQL_DB);
	ck_assert_int_eq(ret, 0);

	talloc_free(mem_ctx);
}

static void checked_mysql_teardown(void)
{
	TALLOC_CTX	*mem_ctx;
	char		*database = NULL;
	int		ret;

	mem_ctx = talloc_named(NULL, 0, "checked_mysql_teardown");
	ck_assert(mem_ctx != NULL);

	database = talloc_asprintf(mem_ctx, "DROP DATABASE %s", NAMEDPROPS_MYSQL_DB);
	ck_assert(database != NULL);

	ret = mysql_query(conn, database);
	talloc_free(database);
	ck_assert_int_eq(ret, 0);

	talloc_free(mem_ctx);
}

START_TEST (test_is_schema_created) {
	enum mapistore_error	retval;

	ck_assert(is_schema_created(conn) == false);
	ck_assert(is_database_empty(conn) == true);

	retval = create_schema(conn, NAMEDPROPS_MYSQL_SCHEMA_PATH);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	ck_assert(is_schema_created(conn) == true);
	ck_assert(is_database_empty(conn) == true);

} END_TEST

Suite *mapistore_namedprops_mysql_suite(void)
{
	Suite	*s;
	TCase	*tc_config;
	TCase	*tc_mysql;

	s = suite_create("libmapistore named properties: MySQL backend");

	/* Core / Configuration */
	tc_config = tcase_create("MySQL backend configuration");
	tcase_add_test(tc_config, test_parameters);
	suite_add_tcase(s, tc_config);

	/* MySQL initialization */
	tc_mysql = tcase_create("MySQL initialization");
	tcase_add_checked_fixture(tc_mysql, checked_mysql_setup, checked_mysql_teardown);
	tcase_add_test(tc_mysql, test_is_schema_created);
	suite_add_tcase(s, tc_mysql);


	return s;
}
