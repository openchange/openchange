#include "indexing.h"
#include "test_common.h"

#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/backends/indexing_tdb.h"
#include "mapiproxy/libmapistore/backends/indexing_mysql.h"
#include "mapiproxy/util/mysql.h"

/* Global test variables */
static struct mapistore_context *mstore_ctx = NULL;
static struct indexing_context *ictx = NULL;
static const char *USERNAME = "testuser";

/* add_fmid */

START_TEST (test_backend_add_fmid)
{
	enum mapistore_error	ret;
	const uint64_t		fid = 0x11;
	const char		*uri = "random://url";
	char			*retrieved_uri = NULL;
	bool			softDel = true;

	ret = ictx->add_fmid(ictx, USERNAME, fid, uri);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = ictx->get_uri(ictx, USERNAME, ictx, fid, &retrieved_uri, &softDel);

	ck_assert(!softDel);
	ck_assert_str_eq(uri, retrieved_uri);
} END_TEST


START_TEST (test_backend_repeated_add_fails)
{
	enum mapistore_error	ret;
	const uint64_t		fid = 0x11;
	const char		*uri = "random://url";

	ret = ictx->add_fmid(ictx, USERNAME, fid, uri);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = ictx->add_fmid(ictx, USERNAME, fid, uri);
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

	ret = ictx->add_fmid(ictx, USERNAME, fid, uri);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = ictx->update_fmid(ictx, USERNAME, fid, uri2);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = ictx->get_uri(ictx, USERNAME, ictx, fid, &retrieved_uri, &softDel);

	ck_assert(!softDel);
	ck_assert_str_eq(uri2, retrieved_uri);
} END_TEST


/* del_fmid */

START_TEST (test_backend_del_unkown_fmid)
{
	enum mapistore_error	ret;
	const uint64_t		fid = 0x11;

	// Unkown fid returns SUCCESS
	ret = ictx->del_fmid(ictx, USERNAME, fid, MAPISTORE_SOFT_DELETE);
	ck_assert(ret == MAPISTORE_SUCCESS);
	ret = ictx->del_fmid(ictx, USERNAME, fid, MAPISTORE_PERMANENT_DELETE);
	ck_assert(ret == MAPISTORE_SUCCESS);
} END_TEST


START_TEST (test_backend_del_fmid_soft)
{
	enum mapistore_error	ret;
	const uint64_t		fid = 0x11;
	const char		*uri = "random://url";
	char			*retrieved_uri = NULL;
	bool			softDel = false;

	ret = ictx->add_fmid(ictx, USERNAME, fid, uri);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = ictx->del_fmid(ictx, USERNAME, fid, MAPISTORE_SOFT_DELETE);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = ictx->get_uri(ictx, USERNAME, ictx, fid, &retrieved_uri, &softDel);
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

	ret = ictx->add_fmid(ictx, USERNAME, fid, uri);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = ictx->del_fmid(ictx, USERNAME, fid, MAPISTORE_PERMANENT_DELETE);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = ictx->get_uri(ictx, USERNAME, ictx, fid, &retrieved_uri, &softDel);
	ck_assert(ret == MAPISTORE_ERR_NOT_FOUND);
} END_TEST


/* get_uri */


START_TEST (test_backend_get_uri_unknown) {
	enum mapistore_error	ret;
	char			*uri = NULL;
	bool			softDel = false;

	ret = ictx->get_uri(ictx, USERNAME, ictx, 0x13, &uri, &softDel);
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

	ret = ictx->add_fmid(ictx, USERNAME, fid, uri);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = ictx->get_fmid(ictx, USERNAME, uri, false, &retrieved_fid, &softDel);
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
	ret = ictx->add_fmid(ictx, USERNAME, fid_1, "foo://bar/user11");
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	fid_2 = 99;
	ret = ictx->add_fmid(ictx, USERNAME, fid_2, "foo://bar/user21");
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);

	ret = ictx->get_fmid(ictx, USERNAME, "foo://bar/*", true,
			     &retrieved_fid, &soft_del);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert(!soft_del);
	ck_assert(retrieved_fid == fid_1 || retrieved_fid == fid_2);

	ret = ictx->get_fmid(ictx, USERNAME, "foo://bar/user2*", true,
			     &retrieved_fid, &soft_del);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert(!soft_del);
	ck_assert_int_eq(retrieved_fid, fid_2);

	ret = ictx->get_fmid(ictx, USERNAME, "*user21", true, &retrieved_fid,
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

	ret = ictx->allocate_fmid(ictx, USERNAME, &fmid1);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = ictx->allocate_fmid(ictx, USERNAME, &fmid2);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ck_assert(fmid1 != fmid2);
} END_TEST

// ^ unit tests ---------------------------------------------------------------

// v suite definition ---------------------------------------------------------

static void tdb_setup(void)
{
	TALLOC_CTX		*mem_ctx;
	enum mapistore_error	ret;

	ret = mapistore_set_mapping_path("/tmp/");
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);

	mem_ctx = talloc_named(NULL, 0, "tdb_setup");
	mstore_ctx = talloc_zero(mem_ctx, struct mapistore_context);

	mapistore_indexing_tdb_init(mstore_ctx, USERNAME, &ictx);
	fail_if(!ictx);
}

static void tdb_teardown(void)
{
	char *indexing_file = NULL;

	indexing_file = talloc_asprintf(mstore_ctx, "%s%s/indexing.tdb",
					mapistore_get_mapping_path(),
					USERNAME);
	unlink(indexing_file);
	talloc_free(mstore_ctx);
}

static void mysql_setup(void)
{
	TALLOC_CTX *mem_ctx;
	char *database;

	mem_ctx = talloc_named(NULL, 0, "mysql_setup");
	mstore_ctx = talloc_zero(mem_ctx, struct mapistore_context);

	if (strlen(MYSQL_PASS) == 0) {
		database = talloc_asprintf(mem_ctx, "mysql://" MYSQL_USER "@"
					   MYSQL_HOST "/" MYSQL_DB);
	} else {
		database = talloc_asprintf(mem_ctx, "mysql://" MYSQL_USER ":"
					   MYSQL_PASS "@" MYSQL_HOST "/"
					   MYSQL_DB);
	}

	mapistore_indexing_mysql_init(mstore_ctx, USERNAME, database, &ictx);
	talloc_free(database);
	fail_if(!ictx);
}

static void mysql_teardown(void)
{
	mysql_query((MYSQL*)ictx->data, "DROP DATABASE " MYSQL_DB);
	talloc_free(mstore_ctx);
}

static Suite *indexing_create_suite(const char *backend_name, SFun setup,
				    SFun teardown)
{
	char *suite_name = talloc_asprintf(talloc_autofree_context(),
					   "Indexing %s backend",
					   backend_name);
	Suite *s = suite_create(suite_name);

	TCase *tc = tcase_create(suite_name);
	tcase_add_checked_fixture(tc, setup, teardown);

	tcase_add_test(tc, test_backend_add_fmid);
	tcase_add_test(tc, test_backend_repeated_add_fails);
	tcase_add_test(tc, test_backend_update_fmid);
	tcase_add_test(tc, test_backend_del_unkown_fmid);
	tcase_add_test(tc, test_backend_del_fmid_soft);
	tcase_add_test(tc, test_backend_del_fmid_permanent);
	tcase_add_test(tc, test_backend_get_uri_unknown);
	tcase_add_test(tc, test_backend_get_fmid);
	tcase_add_test(tc, test_backend_allocate_fmid);

	tcase_add_test(tc, test_backend_get_fmid_with_wildcard);

	suite_add_tcase(s, tc);

	return s;
}

Suite *indexing_tdb_suite(void)
{
	return indexing_create_suite("TDB", tdb_setup, tdb_teardown);
}

Suite *indexing_mysql_suite(void)
{
	return indexing_create_suite("MySQL", mysql_setup, mysql_teardown);
}
