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
#include "testsuite_common.h"
#include <libmapistore/mapistore.h>
#include <libmapistore/mapistore_errors.h>
#include <libmapistore/backends/namedprops_mysql.h>
#include "libmapistore/backends/namedprops_mysql.c"

#include <mysql/mysql.h>
#include <mysql/mysqld_error.h>

#define NAMEDPROPS_MYSQL_SCHEMA_PATH	"setup/mapistore"
#define	NAMEDPROPS_MYSQL_TMPDIR		"/tmp"

/* Global variables used for test fixture */
static MYSQL				*conn;
static struct namedprops_context	*g_nprops;
static struct loadparm_context		*g_lp_ctx;
static TALLOC_CTX			*g_mem_ctx;


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
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_data", "setup/mapistore") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_sock", "/tmp/mysql.sock") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_host", "localhost") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_user", "root") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_pass", "openchange") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_port", "3305") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_db", "db_openchange") == true));

	retval = mapistore_namedprops_mysql_parameters(lp_ctx, &p);
	ck_assert(retval == MAPISTORE_SUCCESS);

	ck_assert_str_eq(p.data, "setup/mapistore");
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

	rconn = mysql_real_connect(conn, OC_TESTSUITE_MYSQL_HOST,
				   OC_TESTSUITE_MYSQL_USER, OC_TESTSUITE_MYSQL_PASS,
				   NULL, 0, NULL, 0);
	ck_assert(rconn != NULL);

	query = talloc_asprintf(mem_ctx, "DROP DATABASE %s", OC_TESTSUITE_MYSQL_DB);
	ck_assert(query != NULL);
	ret = mysql_query(conn, query);
	talloc_free(query);

	query = talloc_asprintf(mem_ctx, "CREATE DATABASE %s", OC_TESTSUITE_MYSQL_DB);
	ck_assert(query != NULL);
	ret = mysql_query(conn, query);
	talloc_free(query);
	ck_assert_int_eq(ret, 0);

	ret = mysql_select_db(conn, OC_TESTSUITE_MYSQL_DB);
	ck_assert_int_eq(ret, 0);

	talloc_free(mem_ctx);
}

static void checked_mysql_teardown(void)
{
	drop_mysql_database(conn, OC_TESTSUITE_MYSQL_DB);
}

START_TEST (test_is_schema_created) {
	bool schema_created;

	ck_assert(is_schema_created(conn) == false);
	ck_assert(is_database_empty(conn) == true);

	schema_created = create_schema(conn, NAMEDPROPS_MYSQL_SCHEMA_PATH"/"NAMEDPROPS_MYSQL_SCHEMA);
	ck_assert(schema_created);

	ck_assert(is_schema_created(conn) == true);
	ck_assert(is_database_empty(conn) == true);

} END_TEST

START_TEST (test_initialize_database) {
	enum mapistore_error	retval;

	/* test sanity checks */
	retval = initialize_database(NULL, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_DATABASE_INIT);

	ck_assert(is_database_empty(conn) == true);

	retval = initialize_database(conn, NAMEDPROPS_MYSQL_SCHEMA_PATH);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	ck_assert(is_database_empty(conn) == false);

} END_TEST

static void unchecked_mysql_query_setup(void)
{
	enum mapistore_error retval;

	g_mem_ctx = talloc_named(NULL, 0, "checked_mysql_query_setup");
	ck_assert(g_mem_ctx != NULL);

	g_lp_ctx = loadparm_init(g_mem_ctx);
	ck_assert(g_lp_ctx != NULL);

	ck_assert((lpcfg_set_cmdline(g_lp_ctx, "mapistore:namedproperties", "mysql") == true));
	ck_assert((lpcfg_set_cmdline(g_lp_ctx, "namedproperties:mysql_data", NAMEDPROPS_MYSQL_SCHEMA_PATH) == true));
	ck_assert((lpcfg_set_cmdline(g_lp_ctx, "namedproperties:mysql_host", OC_TESTSUITE_MYSQL_HOST) == true));
	ck_assert((lpcfg_set_cmdline(g_lp_ctx, "namedproperties:mysql_user", OC_TESTSUITE_MYSQL_USER) == true));
	ck_assert((lpcfg_set_cmdline(g_lp_ctx, "namedproperties:mysql_pass", OC_TESTSUITE_MYSQL_PASS) == true));
	ck_assert((lpcfg_set_cmdline(g_lp_ctx, "namedproperties:mysql_port", "3306") == true));
	ck_assert((lpcfg_set_cmdline(g_lp_ctx, "namedproperties:mysql_db", OC_TESTSUITE_MYSQL_DB) == true));

	retval = mapistore_namedprops_init(g_mem_ctx, g_lp_ctx, &g_nprops);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
}

static void unchecked_mysql_query_teardown(void)
{
	drop_mysql_database(g_nprops->data, OC_TESTSUITE_MYSQL_DB);
	talloc_free(g_nprops);
	talloc_free(g_lp_ctx);
	talloc_free(g_mem_ctx);
}

START_TEST (test_next_unused_id) {
	enum mapistore_error	retval;
	uint16_t		highest_id = 0;

	/* test sanity checks */
	retval = next_unused_id(NULL, &highest_id);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = next_unused_id(g_nprops, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = next_unused_id(g_nprops, &highest_id);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
} END_TEST

START_TEST (test_get_mapped_id_MNID_ID) {
	/*
	Looking for:
		dn: CN=0x8102,CN=00062003-0000-0000-c000-000000000046,CN=default
		objectClass: MNID_ID
		cn: 0x8102
		canonical: PidLidPercentComplete
		oleguid: 00062003-0000-0000-c000-000000000046
		mappedId: 37153
		propId: 33026
		propType: 5
		oom: PercentComplete
	 */
	struct MAPINAMEID nameid = {0};
	nameid.ulKind = MNID_ID;
	nameid.lpguid.time_low = 0x62003;
	nameid.lpguid.clock_seq[0] = 0xc0;
	nameid.lpguid.node[5] = 0x46;
	nameid.kind.lid = 33026;
	uint16_t prop;

	ck_assert_int_eq(get_mapped_id(g_nprops, nameid, &prop),
			 MAPISTORE_SUCCESS);
	ck_assert_int_eq(prop, 37153);

	// A couple more...
	nameid.lpguid.time_low = 0x62004;
	nameid.kind.lid = 32978;
	ck_assert_int_eq(get_mapped_id(g_nprops, nameid, &prop),
			 MAPISTORE_SUCCESS);
	ck_assert_int_eq(prop, 37524);

	nameid.kind.lid = 32901;
	ck_assert_int_eq(get_mapped_id(g_nprops, nameid, &prop),
			 MAPISTORE_SUCCESS);
	ck_assert_int_eq(prop, 37297);

} END_TEST

START_TEST (test_get_mapped_id_MNID_STRING) {
	/*
	Looking for:
		dn: CN=http://schemas.microsoft.com/exchange/smallicon,CN=00020329-0000-0000-c000-000000000046,CN=default
		objectClass: MNID_STRING
		cn: http://schemas.microsoft.com/exchange/smallicon
		canonical: PidNameSmallicon
		oleguid: 00020329-0000-0000-c000-000000000046
		mappedId: 38342
		propId: 0
		propType: PT_NULL
		propName: http://schemas.microsoft.com/exchange/smallicon
	 */
	struct MAPINAMEID nameid = {0};
	nameid.ulKind = MNID_STRING;
	nameid.lpguid.time_low = 0x20329;
	nameid.lpguid.clock_seq[0] = 0xc0;
	nameid.lpguid.node[5] = 0x46;
	nameid.kind.lpwstr.Name = "http://schemas.microsoft.com/exchange/smallicon";
	uint16_t prop;

	ck_assert_int_eq(get_mapped_id(g_nprops, nameid, &prop),
			 MAPISTORE_SUCCESS);
	ck_assert_int_eq(prop, 38342);

	// Another...
	nameid.kind.lpwstr.Name = "http://schemas.microsoft.com/exchange/searchfolder";
	ck_assert_int_eq(get_mapped_id(g_nprops, nameid, &prop),
			 MAPISTORE_SUCCESS);
	ck_assert_int_eq(prop, 38365);
} END_TEST

START_TEST (test_get_mapped_id_not_found) {
	struct MAPINAMEID nameid = {0};
	uint16_t prop;

	ck_assert_int_eq(get_mapped_id(g_nprops, nameid, &prop),
			 MAPISTORE_ERR_NOT_FOUND);
} END_TEST

START_TEST (test_get_nameid_type) {
	uint16_t prop_type = -1;

	get_nameid_type(g_nprops, 38306, &prop_type);
	ck_assert_int_eq(prop_type, PT_NULL);

	get_nameid_type(g_nprops, 37975, &prop_type);
	ck_assert_int_eq(prop_type, PT_UNICODE);

	get_nameid_type(g_nprops, 37097, &prop_type);
	ck_assert_int_eq(prop_type, PT_LONG);

	get_nameid_type(g_nprops, 37090, &prop_type);
	ck_assert_int_eq(prop_type, PT_SYSTIME);
} END_TEST

START_TEST (test_get_nameid_type_not_found) {
	uint16_t prop_type = -1;

	enum mapistore_error ret = get_nameid_type(g_nprops, 42, &prop_type);
	ck_assert_int_eq(ret, MAPISTORE_ERR_NOT_FOUND);
} END_TEST

START_TEST (test_get_nameid_MNID_STRING) {
	TALLOC_CTX *local_mem_ctx = talloc_new(NULL);
	struct MAPINAMEID *nameid;

	get_nameid(g_nprops, 38306, local_mem_ctx, &nameid);
	ck_assert(nameid != NULL);
	ck_assert_str_eq("urn:schemas:httpmail:junkemail",
			 nameid->kind.lpwstr.Name);

	get_nameid(g_nprops, 38344, local_mem_ctx, &nameid);
	ck_assert(nameid != NULL);
	ck_assert_str_eq("http://schemas.microsoft.com/exchange/mailbox-owner-name",
			 nameid->kind.lpwstr.Name);

	talloc_free(local_mem_ctx);
} END_TEST

START_TEST (test_get_nameid_MNID_ID) {
	TALLOC_CTX *local_mem_ctx = talloc_new(NULL);
	struct MAPINAMEID *nameid;

	get_nameid(g_nprops, 38212, local_mem_ctx, &nameid);
	ck_assert(nameid != NULL);
	ck_assert_int_eq(32778, nameid->kind.lid);

	get_nameid(g_nprops, 38111, local_mem_ctx, &nameid);
	ck_assert(nameid != NULL);
	ck_assert_int_eq(34063, nameid->kind.lid);

	talloc_free(local_mem_ctx);
} END_TEST


START_TEST (test_get_nameid_not_found) {
	TALLOC_CTX *local_mem_ctx = talloc_new(NULL);
	struct MAPINAMEID *nameid = NULL;

	enum mapistore_error ret = get_nameid(g_nprops, 42, local_mem_ctx, &nameid);
	ck_assert_int_eq(ret, MAPISTORE_ERR_NOT_FOUND);
	ck_assert(nameid == NULL);
	talloc_free(local_mem_ctx);
} END_TEST

START_TEST (test_create_id_MNID_ID) {
	struct MAPINAMEID nameid = {0};
	uint16_t mapped_id = 42;

	nameid.ulKind = MNID_ID;
	nameid.kind.lid = 42;

	int ret = create_id(g_nprops, nameid, mapped_id);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);

	mapped_id = 0;
	ret = get_mapped_id(g_nprops, nameid, &mapped_id);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert_int_eq(mapped_id, 42);
} END_TEST

START_TEST (test_create_id_MNID_STRING) {
	struct MAPINAMEID nameid = {0};
	uint16_t mapped_id = 41;
	TALLOC_CTX *local_mem_ctx = talloc_new(NULL);

	nameid.ulKind = MNID_STRING;
	nameid.kind.lpwstr.Name = talloc_strdup(local_mem_ctx, "foobar");

	int ret = create_id(g_nprops, nameid, mapped_id);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);

	mapped_id = 0;
	ret = get_mapped_id(g_nprops, nameid, &mapped_id);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert_int_eq(mapped_id, 41);

	talloc_free(local_mem_ctx);
} END_TEST


Suite *mapistore_namedprops_mysql_suite(void)
{
	Suite	*s;
	TCase	*tc_config;
	TCase	*tc_mysql;
	TCase	*tc_mysql_q;

	s = suite_create("libmapistore named properties: MySQL backend");

	/* Core / Configuration */
	tc_config = tcase_create("MySQL backend configuration");
	tcase_add_test(tc_config, test_parameters);
	suite_add_tcase(s, tc_config);

	/* MySQL initialization */
	tc_mysql = tcase_create("MySQL initialization");
	/* database provisioning takes longer than default timeout */
	tcase_set_timeout(tc_mysql, 60);
	tcase_add_checked_fixture(tc_mysql, checked_mysql_setup, checked_mysql_teardown);
	tcase_add_test(tc_mysql, test_is_schema_created);
	tcase_add_test(tc_mysql, test_initialize_database);
	suite_add_tcase(s, tc_mysql);

	/* MySQL queries */
	tc_mysql_q = tcase_create("MySQL queries");
	tcase_set_timeout(tc_mysql_q, 60);
	tcase_add_unchecked_fixture(tc_mysql_q, unchecked_mysql_query_setup, unchecked_mysql_query_teardown);
	tcase_add_test(tc_mysql_q, test_next_unused_id);
	tcase_add_test(tc_mysql_q, test_get_mapped_id_MNID_ID);
	tcase_add_test(tc_mysql_q, test_get_mapped_id_MNID_STRING);
	tcase_add_test(tc_mysql_q, test_get_mapped_id_not_found);
	tcase_add_test(tc_mysql_q, test_get_nameid_type);
	tcase_add_test(tc_mysql_q, test_get_nameid_type_not_found);
	tcase_add_test(tc_mysql_q, test_get_nameid_MNID_ID);
	tcase_add_test(tc_mysql_q, test_get_nameid_MNID_STRING);
	tcase_add_test(tc_mysql_q, test_get_nameid_not_found);
	tcase_add_test(tc_mysql_q, test_create_id_MNID_ID);
	tcase_add_test(tc_mysql_q, test_create_id_MNID_STRING);
	suite_add_tcase(s, tc_mysql_q);

	return s;
}
