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
#include "testsuite_common.h"
#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/backends/indexing_tdb.h"
#include "mapiproxy/util/mysql.h"

#undef MAPISTORE_LDIF
#define MAPISTORE_LDIF "setup/mapistore"
#include "mapiproxy/libmapistore/backends/indexing_mysql.c"

#define INDEXING_MYSQL_HOST	"127.0.0.1"
#define INDEXING_MYSQL_USER	"root"
#define INDEXING_MYSQL_PASS	""
#define INDEXING_MYSQL_DB	"openchange_indexing_test"

#define INDEXING_TEST_FMID	0x123456
#define INDEXING_TEST_FMID_NOK	0x0
#define INDEXING_TEST_URI	"idxtest://url/test"
#define INDEXING_TEST_URI_2	"idxtest://url/test2"
/* Existing FMID/URL to be populated on setup */
#define INDEXING_EXIST_FMID	0xEEEE
#define INDEXING_EXIST_URL	"idxtest://existing_url"

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

	/* check with NULL pointer to indexing context */
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
	retval = mapistore_indexing_mysql_init(mstore_ctx, g_test_username, "mysql://root@/somedb", &ictx);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	/* check wrong password */
	retval = mapistore_indexing_mysql_init(mstore_ctx, g_test_username, conn_string_wrong_pass, &ictx);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	/* check successful connection */
	retval = mapistore_indexing_mysql_init(mstore_ctx, g_test_username, conn_string, &ictx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert(ictx != NULL);
	ck_assert(ictx->data != NULL);
	/* explicitly expect for url to be the username - current behavior */
	ck_assert_str_eq(ictx->url, g_test_username);

	talloc_free(mem_ctx);
} END_TEST


/* mysql_search_existing_fmid */

START_TEST(test_mysql_search_existing_fmid_invalid_input) {
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
} END_TEST


/* add_fmid */

START_TEST(test_add_fmid_sanity) {
	enum mapistore_error	retval;

	/* check missing mapistore context */
	retval = g_ictx->add_fmid(NULL, g_test_username, INDEXING_TEST_FMID, INDEXING_TEST_URI);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	/* check with missing username */
	retval = g_ictx->add_fmid(g_ictx, NULL, INDEXING_TEST_FMID, INDEXING_TEST_URI);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* test for invalid FMID */
	retval = g_ictx->add_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID_NOK, INDEXING_TEST_URI);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* check with missing mapistore URI */
	retval = g_ictx->add_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);
} END_TEST

START_TEST(test_add_fmid) {
	enum mapistore_error	retval;
	char			*retrieved_uri = NULL;
	bool			soft_deleted = true;

	retval = g_ictx->add_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, INDEXING_TEST_URI);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	retval = g_ictx->get_uri(g_ictx, g_test_username, g_ictx, INDEXING_TEST_FMID, &retrieved_uri, &soft_deleted);
	ck_assert_int_eq(soft_deleted, false);
	ck_assert_str_eq(INDEXING_TEST_URI, retrieved_uri);
	ck_assert(talloc_is_parent(retrieved_uri, g_ictx));
} END_TEST

START_TEST(test_add_fmid_duplicate_value_should_fail) {
	enum mapistore_error	retval;

	/* populate record with test FMID */
	retval = g_ictx->add_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, INDEXING_TEST_URI);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* create same record */
	retval = g_ictx->add_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, INDEXING_TEST_URI);
	ck_assert_int_eq(retval, MAPISTORE_ERR_EXIST);

	/* create same record but different URL */
	retval = g_ictx->add_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, INDEXING_TEST_URI_2);
	ck_assert_int_eq(retval, MAPISTORE_ERR_EXIST);
} END_TEST


/* update_fmid */

START_TEST(test_update_fmid_sanity) {
	enum mapistore_error	retval;

	/* missing indexing context */
	retval = g_ictx->update_fmid(NULL, g_test_username, INDEXING_TEST_FMID, INDEXING_TEST_URI);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	/* missing username */
	retval = g_ictx->update_fmid(g_ictx, NULL, INDEXING_TEST_FMID, INDEXING_TEST_URI);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* invalid FMID */
	retval = g_ictx->update_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID_NOK, INDEXING_TEST_URI);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* missing mapistore URI */
	retval = g_ictx->update_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

} END_TEST

START_TEST(test_update_fmid) {
	enum mapistore_error	retval;
	char			*mapistore_uri = NULL;
	bool			soft_deleted = true;

	/* prepare some data */
	retval = g_ictx->add_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, INDEXING_TEST_URI);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* actual test */
	retval = g_ictx->update_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, INDEXING_TEST_URI_2);
	ck_assert_int_eq(retval,  MAPISTORE_SUCCESS);

	/* check what is in db */
	retval = g_ictx->get_uri(g_ictx, g_test_username, g_ictx, INDEXING_TEST_FMID, &mapistore_uri, &soft_deleted);
	ck_assert(!soft_deleted);
	ck_assert_str_eq(mapistore_uri, INDEXING_TEST_URI_2);
} END_TEST

START_TEST(test_update_fmid_should_fail) {
	enum mapistore_error	retval;

	/* update non-existent FMID */
	retval = g_ictx->update_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, INDEXING_TEST_URI);
	ck_assert_int_eq(retval,  MAPISTORE_ERR_NOT_FOUND);
} END_TEST

START_TEST(test_update_fmid_same_value) {
	enum mapistore_error	retval;
	char			*mapistore_uri = NULL;
	bool			soft_deleted = true;

	/* prepare some data */
	retval = g_ictx->add_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, INDEXING_TEST_URI);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* actual test */
	retval = g_ictx->update_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, INDEXING_TEST_URI_2);
	ck_assert_int_eq(retval,  MAPISTORE_SUCCESS);

	/* Recheck update a matched value successes */
	retval = g_ictx->update_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, INDEXING_TEST_URI_2);
	ck_assert_int_eq(retval,  MAPISTORE_SUCCESS);

	/* check what is in db */
	retval = g_ictx->get_uri(g_ictx, g_test_username, g_ictx, INDEXING_TEST_FMID, &mapistore_uri, &soft_deleted);
	ck_assert(!soft_deleted);
	ck_assert_str_eq(mapistore_uri, INDEXING_TEST_URI_2);
} END_TEST

/* del_fmid */

START_TEST(test_del_fmid_sanity) {
	enum mapistore_error	retval;

	/* missing indexing context */
	retval = g_ictx->del_fmid(NULL, g_test_username, INDEXING_EXIST_FMID, MAPISTORE_SOFT_DELETE);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	/* missing username */
	retval = g_ictx->del_fmid(g_ictx, NULL, INDEXING_EXIST_FMID, MAPISTORE_SOFT_DELETE);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* invalid FMID */
	retval = g_ictx->del_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID_NOK, MAPISTORE_SOFT_DELETE);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* invalid delete flags */
	retval = g_ictx->del_fmid(g_ictx, g_test_username, INDEXING_EXIST_FMID, MAPISTORE_PERMANENT_DELETE + 1);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);
} END_TEST

START_TEST(test_del_fmid_unknown_fmid) {
	enum mapistore_error	retval;

	/* soft delete */
	retval = g_ictx->del_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, MAPISTORE_SOFT_DELETE);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* hard delete */
	retval = g_ictx->del_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, MAPISTORE_PERMANENT_DELETE);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
} END_TEST

START_TEST(test_del_fmid_soft) {
	enum mapistore_error	retval;
	char			*mapistore_uri = NULL;
	bool			soft_deleted = false;

	retval = g_ictx->add_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, INDEXING_TEST_URI);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	retval = g_ictx->del_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, MAPISTORE_SOFT_DELETE);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	retval = g_ictx->get_uri(g_ictx, g_test_username, g_ictx, INDEXING_TEST_FMID, &mapistore_uri, &soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert(soft_deleted);
	ck_assert_str_eq(mapistore_uri, INDEXING_TEST_URI);
} END_TEST

START_TEST(test_del_fmid_permanent) {
	enum mapistore_error	retval;
	char			*mapistore_uri = NULL;
	bool			soft_deleted = true;

	retval = g_ictx->add_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, INDEXING_TEST_URI);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	retval = g_ictx->del_fmid(g_ictx, g_test_username, INDEXING_TEST_FMID, MAPISTORE_PERMANENT_DELETE);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	retval = g_ictx->get_uri(g_ictx, g_test_username, g_ictx, INDEXING_TEST_FMID, &mapistore_uri, &soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);
} END_TEST


/* get_uri */

START_TEST(test_get_uri_sanity) {
	enum mapistore_error	retval;
	char			*uri;
	bool			soft_deleted;

	/* missing indexing context */
	retval = g_ictx->get_uri(NULL, g_test_username, g_ictx, INDEXING_EXIST_FMID, &uri, &soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	/* missing username */
	retval = g_ictx->get_uri(g_ictx, NULL, g_ictx, INDEXING_EXIST_FMID, &uri, &soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* invalid FMID */
	retval = g_ictx->get_uri(g_ictx, g_test_username, g_ictx, INDEXING_TEST_FMID_NOK, &uri, &soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* invalid delete flags */
	retval = g_ictx->get_uri(g_ictx, g_test_username, g_ictx, INDEXING_EXIST_FMID, NULL, &soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* invalid pointer for soft_deleted */
	retval = g_ictx->get_uri(g_ictx, g_test_username, g_ictx, INDEXING_EXIST_FMID, &uri, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);
} END_TEST

START_TEST(test_get_uri_uknown_fmid) {
	enum mapistore_error	retval;
	char			*uri = NULL;
	bool			soft_deleted = false;

	retval = g_ictx->get_uri(g_ictx, g_test_username, g_ictx, INDEXING_TEST_FMID, &uri, &soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);
} END_TEST


/* get_fmid */

START_TEST(test_get_fmid_sanity) {
	enum mapistore_error	retval;
	uint64_t		fmid_res;
	bool			soft_deleted;

	/* missing indexing context */
	retval = g_ictx->get_fmid(NULL, g_test_username, INDEXING_EXIST_URL, false, &fmid_res, &soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	/* missing username */
	retval = g_ictx->get_fmid(g_ictx, NULL, INDEXING_EXIST_URL, false, &fmid_res, &soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* invalid URI */
	retval = g_ictx->get_fmid(g_ictx, g_test_username, NULL, false, &fmid_res, &soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* invalid pointer to FMID res */
	retval = g_ictx->get_fmid(g_ictx, g_test_username, INDEXING_EXIST_URL, false, NULL, &soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* invalid pointer for soft_deleted */
	retval = g_ictx->get_fmid(g_ictx, g_test_username, INDEXING_EXIST_URL, false, &fmid_res, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);
} END_TEST

START_TEST(test_get_fmid) {
	enum mapistore_error	retval;
	uint64_t		fmid_res;
	bool			soft_deleted = true;

	retval = g_ictx->get_fmid(g_ictx, g_test_username, INDEXING_EXIST_URL, false, &fmid_res, &soft_deleted);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert(!soft_deleted);
	ck_assert(fmid_res == INDEXING_EXIST_FMID);
} END_TEST

START_TEST(test_get_fmid_with_wildcard) {
	enum mapistore_error	ret;
	uint64_t		fid_1, fid_2;
	uint64_t		fmid_res;
	bool			soft_deleted = true;

	fid_1 = INDEXING_TEST_FMID;
	fid_2 = fid_1 + 1;
	ret = g_ictx->add_fmid(g_ictx, g_test_username, fid_1, "foo://bar/u11");
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ret = g_ictx->add_fmid(g_ictx, g_test_username, fid_2, "foo://bar/u21");
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);

	ret = g_ictx->get_fmid(g_ictx, g_test_username, "foo://bar/*", true, &fmid_res, &soft_deleted);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert(!soft_deleted);
	ck_assert(fmid_res == fid_1 || fmid_res == fid_2);

	ret = g_ictx->get_fmid(g_ictx, g_test_username, "foo://bar/u2*", true, &fmid_res, &soft_deleted);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert(!soft_deleted);
	ck_assert_int_eq(fmid_res, fid_2);

	ret = g_ictx->get_fmid(g_ictx, g_test_username, "*u21", true, &fmid_res, &soft_deleted);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert(!soft_deleted);
	ck_assert_int_eq(fmid_res, fid_2);
} END_TEST


/* allocate_fmid */

START_TEST (test_allocate_fmid) {
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
	char *conn_string;
	enum mapistore_error retval;

	g_mstore_ctx = talloc_zero(NULL, struct mapistore_context);
	ck_assert(g_mstore_ctx != NULL);

	conn_string = _make_connection_string(g_mstore_ctx,
					      INDEXING_MYSQL_USER, INDEXING_MYSQL_PASS,
					      INDEXING_MYSQL_HOST, INDEXING_MYSQL_DB);
	ck_assert(conn_string != NULL);

	retval = mapistore_indexing_mysql_init(g_mstore_ctx, g_test_username, conn_string, &g_ictx);
	ck_assert(retval == MAPISTORE_SUCCESS);
	ck_assert(g_ictx != NULL);

	/* populate DB with some data */
	retval = g_ictx->add_fmid(g_ictx, g_test_username, INDEXING_EXIST_FMID, INDEXING_EXIST_URL);
	ck_assert(retval == MAPISTORE_SUCCESS);

	talloc_free(conn_string);
}

static void mysql_teardown(void)
{
	drop_mysql_database(g_ictx->data, INDEXING_MYSQL_DB);
	talloc_free(g_mstore_ctx);
}

static void tdb_setup(void)
{
	enum mapistore_error	retval;

	retval = mapistore_set_mapping_path("/tmp/");
	ck_assert(retval == MAPISTORE_SUCCESS);

	g_mstore_ctx = talloc_zero(NULL, struct mapistore_context);
	ck_assert(g_mstore_ctx != NULL);

	retval = mapistore_indexing_tdb_init(g_mstore_ctx, g_test_username, &g_ictx);
	ck_assert(retval == MAPISTORE_SUCCESS);
	ck_assert(g_ictx != NULL);

	/* populate DB with some data */
	g_ictx->add_fmid(g_ictx, g_test_username, INDEXING_EXIST_FMID, INDEXING_EXIST_URL);
}

static void tdb_teardown(void)
{
	char *indexing_file = NULL;

	indexing_file = talloc_asprintf(g_mstore_ctx, "%s%s/indexing.tdb",
					mapistore_get_mapping_path(),
					g_test_username);
	unlink(indexing_file);
	talloc_free(g_mstore_ctx);
}

static TCase *create_test_case_indexing_interface(const char *name, SFun setup,
						  SFun teardown)
{
	TCase 	*tc_interface;
	char 	*tcase_name;

	tcase_name= talloc_asprintf(talloc_autofree_context(),
				    "indexing: %s backend interface", name);

	/* test indexing backend */
	tc_interface = tcase_create(tcase_name);
	tcase_add_checked_fixture(tc_interface, setup, teardown);

	tcase_add_test(tc_interface, test_add_fmid_sanity);
	tcase_add_test(tc_interface, test_add_fmid);
	tcase_add_test(tc_interface, test_add_fmid_duplicate_value_should_fail);
	tcase_add_test(tc_interface, test_update_fmid_sanity);
	tcase_add_test(tc_interface, test_update_fmid);
	tcase_add_test(tc_interface, test_update_fmid_same_value);
	tcase_add_test(tc_interface, test_update_fmid_should_fail);
	tcase_add_test(tc_interface, test_del_fmid_sanity);
	tcase_add_test(tc_interface, test_del_fmid_unknown_fmid);
	tcase_add_test(tc_interface, test_del_fmid_soft);
	tcase_add_test(tc_interface, test_del_fmid_permanent);
	tcase_add_test(tc_interface, test_get_uri_sanity);
	tcase_add_test(tc_interface, test_get_uri_uknown_fmid);
	tcase_add_test(tc_interface, test_get_fmid_sanity);
	tcase_add_test(tc_interface, test_get_fmid);
	tcase_add_test(tc_interface, test_get_fmid_with_wildcard);
	tcase_add_test(tc_interface, test_allocate_fmid);

	return tc_interface;
}

Suite *mapistore_indexing_mysql_suite(void)
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

	tc_interface = create_test_case_indexing_interface("MySQL", mysql_setup, mysql_teardown);
	suite_add_tcase(s, tc_interface);

	return s;
}

Suite *mapistore_indexing_tdb_suite(void)
{
	Suite *s;
	TCase *tc_interface;

	s = suite_create("libmapistore indexing: TDB backend");

	tc_interface = create_test_case_indexing_interface("TDB", tdb_setup, tdb_teardown);
	suite_add_tcase(s, tc_interface);

	return s;
}
