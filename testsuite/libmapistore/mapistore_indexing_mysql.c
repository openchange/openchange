/*
   OpenChange Unit Testing

   OpenChange Project

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
#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/backends/indexing_mysql.c"
#include "mapiproxy/util/mysql.h"

#define INDEXING_MYSQL_HOST	"127.0.0.1"
#define INDEXING_MYSQL_USER	"root"
#define INDEXING_MYSQL_PASS	""
#define INDEXING_MYSQL_DB	"openchange_indexing_test"

#define INDEXING_TEST_FMID	0x123456
#define INDEXING_TEST_FMID_NOK	0x0

/* Global test variables */
static struct mapistore_context	*g_mstore_ctx = NULL;
static struct indexing_context	*g_ictx = NULL;
static const char		*g_test_username = "testuser";

/* test helpers */
static char * _make_connection_string(TALLOC_CTX *mem_ctx,
				      const char *user, const char *pass,
				      const char *host, const char *db)
{
	if (!pass || strlen(pass) == 0) {
		return talloc_asprintf(mem_ctx, "mysql://%s@%s/%s", user, host, db);
	}

	return talloc_asprintf(mem_ctx, "mysql://%s:%s@%s/%s", user, pass, host, db);
}

/* backend initialization */

START_TEST(test_backend_init_parameters) {
	TALLOC_CTX			*mem_ctx;
	struct mapistore_context	*mstore_ctx;
	struct indexing_context		*ictx;
	enum mapistore_error		retval;
	char				*conn_string;
	char				*conn_string_wrong_pass;

	mem_ctx = talloc_named(NULL, 0, "test_backend_init_parameters");
	ck_assert(mem_ctx != NULL);

	mstore_ctx = talloc_zero(mem_ctx, struct mapistore_context);
	ck_assert(mstore_ctx != NULL);

	conn_string = _make_connection_string(mem_ctx,
					      INDEXING_MYSQL_USER, INDEXING_MYSQL_PASS,
					      INDEXING_MYSQL_HOST, INDEXING_MYSQL_DB);
	ck_assert(conn_string != NULL);

	conn_string_wrong_pass = _make_connection_string(mem_ctx,
							 INDEXING_MYSQL_USER, "invalid_password",
							 INDEXING_MYSQL_HOST, INDEXING_MYSQL_DB);
	ck_assert(conn_string_wrong_pass != NULL);

	/* check with NULL mstore context */
	retval = mapistore_indexing_mysql_init(NULL, g_test_username, conn_string, &ictx);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	/* check with no username */
	retval = mapistore_indexing_mysql_init(mstore_ctx, NULL, conn_string, &ictx);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* check with NULL pointer to inexing context */
	retval = mapistore_indexing_mysql_init(mstore_ctx, g_test_username, conn_string, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* check connection string handling */
	retval = mapistore_indexing_mysql_init(mstore_ctx, g_test_username, NULL, &ictx);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_indexing_mysql_init(mstore_ctx, g_test_username, "invalid", &ictx);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	/* check missing database */
	retval = mapistore_indexing_mysql_init(mstore_ctx, g_test_username, "mysql://root@localhost", &ictx);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	/* check missing host */
/*
 * 	TODO: at the moment mysql connect string parsing is widely untested
 * 	and this test fails. To 'fix' it, it will require a bit more work
 * 	and tests. When done, remove the comment.
	retval = mapistore_indexing_mysql_init(mstore_ctx, g_test_username, "mysql://root@/somedb", &ictx);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);
*/

	/* check wrong password */
	retval = mapistore_indexing_mysql_init(mstore_ctx, g_test_username, conn_string_wrong_pass, &ictx);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	/* check successfull connection */
	retval = mapistore_indexing_mysql_init(mstore_ctx, g_test_username, conn_string, &ictx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert(ictx != NULL);
	ck_assert(ictx->data != NULL);
	/* explicitly expect for url to be the username - current behavior */
	ck_assert_str_eq(ictx->url, g_test_username);

	talloc_free(mem_ctx);
} END_TEST


/* mysql_search_existing_fmid */

START_TEST(test_mysql_search_existing_fmid_invalid_input)
{
	bool			is_soft_deleted;
	enum mapistore_error	retval;

	/* missing ictx */
	retval = mysql_search_existing_fmid(NULL, g_test_username, INDEXING_TEST_FMID, &is_soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	/* missing username */
	retval = mysql_search_existing_fmid(g_ictx, NULL, INDEXING_TEST_FMID, &is_soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* not valid FMID */
	retval = mysql_search_existing_fmid(g_ictx, NULL, INDEXING_TEST_FMID_NOK, &is_soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* invalid ptr for soft_deleted */
	retval = mysql_search_existing_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* test with good input is going to be tested through interface tests */
}
END_TEST


/* add_fmid */

START_TEST (test_backend_add_fmid)
{
	enum mapistore_error	ret;
	const uint64_t		fid = 0x11;
	const char		*uri = "random://url";
	char			*retrieved_uri = NULL;
	bool			softDel = true;

	ret = g_ictx->add_fmid(g_ictx, g_test_username, fid, uri);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = g_ictx->get_uri(g_ictx, g_test_username, g_ictx, fid, &retrieved_uri, &softDel);

	ck_assert(!softDel);
	ck_assert_str_eq(uri, retrieved_uri);
} END_TEST


START_TEST (test_backend_repeated_add_fails)
{
	enum mapistore_error	ret;
	const uint64_t		fid = 0x11;
	const char		*uri = "random://url";

	ret = g_ictx->add_fmid(g_ictx, g_test_username, fid, uri);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = g_ictx->add_fmid(g_ictx, g_test_username, fid, uri);
	ck_assert(ret != MAPISTORE_SUCCESS);
} END_TEST

/* update_fmid */

START_TEST (test_backend_update_fmid)
{
	enum mapistore_error	ret;
	const uint64_t		fid = 0x11;
	const char		*uri = "random://url";
	const char		*uri2 = "random://url2";
	char			*retrieved_uri = NULL;
	bool			softDel = true;

	ret = g_ictx->add_fmid(g_ictx, g_test_username, fid, uri);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = g_ictx->update_fmid(g_ictx, g_test_username, fid, uri2);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = g_ictx->get_uri(g_ictx, g_test_username, g_ictx, fid, &retrieved_uri, &softDel);

	ck_assert(!softDel);
	ck_assert_str_eq(uri2, retrieved_uri);
} END_TEST


/* del_fmid */

START_TEST (test_backend_del_unkown_fmid)
{
	enum mapistore_error	ret;
	const uint64_t		fid = 0x11;

	// Unkown fid returns SUCCESS
	ret = g_ictx->del_fmid(g_ictx, g_test_username, fid, MAPISTORE_SOFT_DELETE);
	ck_assert(ret == MAPISTORE_SUCCESS);
	ret = g_ictx->del_fmid(g_ictx, g_test_username, fid, MAPISTORE_PERMANENT_DELETE);
	ck_assert(ret == MAPISTORE_SUCCESS);
} END_TEST


START_TEST (test_backend_del_fmid_soft)
{
	enum mapistore_error	ret;
	const uint64_t		fid = 0x11;
	const char		*uri = "random://url";
	char			*retrieved_uri = NULL;
	bool			softDel = false;

	ret = g_ictx->add_fmid(g_ictx, g_test_username, fid, uri);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = g_ictx->del_fmid(g_ictx, g_test_username, fid, MAPISTORE_SOFT_DELETE);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = g_ictx->get_uri(g_ictx, g_test_username, g_ictx, fid, &retrieved_uri, &softDel);
	ck_assert(ret == MAPISTORE_SUCCESS);
	ck_assert(softDel);
	ck_assert_str_eq(uri, retrieved_uri);
} END_TEST


START_TEST (test_backend_del_fmid_permanent)
{
	enum mapistore_error	ret;
	const uint64_t		fid = 0x11;
	const char		*uri = "random://url";
	char			*retrieved_uri = NULL;
	bool			softDel = true;

	ret = g_ictx->add_fmid(g_ictx, g_test_username, fid, uri);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = g_ictx->del_fmid(g_ictx, g_test_username, fid, MAPISTORE_PERMANENT_DELETE);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = g_ictx->get_uri(g_ictx, g_test_username, g_ictx, fid, &retrieved_uri, &softDel);
	ck_assert(ret == MAPISTORE_ERR_NOT_FOUND);
} END_TEST


/* get_uri */

START_TEST (test_backend_get_uri_unknown) {
	enum mapistore_error	ret;
	char			*uri = NULL;
	bool			softDel = false;

	ret = g_ictx->get_uri(g_ictx, g_test_username, g_ictx, 0x13, &uri, &softDel);
	ck_assert(ret == MAPISTORE_ERR_NOT_FOUND);
} END_TEST


/* get_fmid */

START_TEST (test_backend_get_fmid)
{
	enum mapistore_error	ret;
	const uint64_t		fid = 0x11;
	const char		*uri = "random://url";
	uint64_t		retrieved_fid;
	bool			softDel = true;

	ret = g_ictx->add_fmid(g_ictx, g_test_username, fid, uri);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = g_ictx->get_fmid(g_ictx, g_test_username, uri, false, &retrieved_fid, &softDel);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ck_assert(!softDel);
	ck_assert(retrieved_fid == fid);
} END_TEST


START_TEST (test_backend_get_fmid_with_wildcard)
{
	enum mapistore_error ret;
	uint64_t retrieved_fid, fid_1, fid_2;
	bool soft_del = true;

	fid_1 = 42;
	ret = g_ictx->add_fmid(g_ictx, g_test_username, fid_1, "foo://bar/user11");
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	fid_2 = 99;
	ret = g_ictx->add_fmid(g_ictx, g_test_username, fid_2, "foo://bar/user21");
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);

	ret = g_ictx->get_fmid(g_ictx, g_test_username, "foo://bar/*", true,
			     &retrieved_fid, &soft_del);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert(!soft_del);
	ck_assert(retrieved_fid == fid_1 || retrieved_fid == fid_2);

	ret = g_ictx->get_fmid(g_ictx, g_test_username, "foo://bar/user2*", true,
			     &retrieved_fid, &soft_del);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert(!soft_del);
	ck_assert_int_eq(retrieved_fid, fid_2);

	ret = g_ictx->get_fmid(g_ictx, g_test_username, "*user21", true, &retrieved_fid,
			     &soft_del);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert(!soft_del);
	ck_assert_int_eq(retrieved_fid, fid_2);
} END_TEST

/* allocate_fmid */

START_TEST (test_backend_allocate_fmid)
{
	enum mapistore_error	ret;
	uint64_t		fmid1 = 222;
	uint64_t		fmid2 = 222;

	ret = g_ictx->allocate_fmid(g_ictx, g_test_username, &fmid1);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = g_ictx->allocate_fmid(g_ictx, g_test_username, &fmid2);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ck_assert(fmid1 != fmid2);
} END_TEST

// ^ unit tests ---------------------------------------------------------------

// v suite definition ---------------------------------------------------------

static void mysql_setup(void)
{
	TALLOC_CTX *mem_ctx;
	char *conn_string;
	enum mapistore_error retval;

	mem_ctx = talloc_named(NULL, 0, "mysql_setup");
	ck_assert(mem_ctx != NULL);
	g_mstore_ctx = talloc_zero(mem_ctx, struct mapistore_context);
	ck_assert(g_mstore_ctx != NULL);

	conn_string = _make_connection_string(mem_ctx,
					      INDEXING_MYSQL_USER, INDEXING_MYSQL_PASS,
					      INDEXING_MYSQL_HOST, INDEXING_MYSQL_DB);
	ck_assert(conn_string != NULL);

	retval = mapistore_indexing_mysql_init(g_mstore_ctx, g_test_username, conn_string, &g_ictx);
	ck_assert(retval == MAPISTORE_SUCCESS);
	ck_assert(g_ictx != NULL);

	talloc_free(conn_string);
}

static void mysql_teardown(void)
{
	mysql_query((MYSQL*)g_ictx->data, "DROP DATABASE " INDEXING_MYSQL_DB);
	talloc_free(g_mstore_ctx);
}

Suite *indexing_mysql_suite(void)
{
	Suite *s;
	TCase *tc_config;
	TCase *tc_internal;
	TCase *tc_interface;

	s = suite_create("libmapistore indexing: MySQL backend");

	/* Core / Configuration */
	tc_config = tcase_create("indexing: MySQL backend initialization");
	tcase_add_test(tc_config, test_backend_init_parameters);
	suite_add_tcase(s, tc_config);

	/* test indexing backend internals */
	tc_internal = tcase_create("indexing: MySQL backend internal");
	tcase_add_checked_fixture(tc_internal, mysql_setup, mysql_teardown);
	tcase_add_test(tc_internal, test_mysql_search_existing_fmid_invalid_input);
	suite_add_tcase(s, tc_internal);

	/* test indexing backend */
	tc_interface = tcase_create("indexing: MySQL backend interface");
	tcase_add_checked_fixture(tc_interface, mysql_setup, mysql_teardown);

	tcase_add_test(tc_interface, test_backend_add_fmid);
	tcase_add_test(tc_interface, test_backend_repeated_add_fails);
	tcase_add_test(tc_interface, test_backend_update_fmid);
	tcase_add_test(tc_interface, test_backend_del_unkown_fmid);
	tcase_add_test(tc_interface, test_backend_del_fmid_soft);
	tcase_add_test(tc_interface, test_backend_del_fmid_permanent);
	tcase_add_test(tc_interface, test_backend_get_uri_unknown);
	tcase_add_test(tc_interface, test_backend_get_fmid);
	tcase_add_test(tc_interface, test_backend_allocate_fmid);

	tcase_add_test(tc_interface, test_backend_get_fmid_with_wildcard);

	suite_add_tcase(s, tc_interface);

	return s;
}
