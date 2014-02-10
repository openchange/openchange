#include "namedprops_backends.h"
#include "test_common.h"

#include "mapiproxy/libmapistore/backends/namedprops_ldb.c"


#define NAMEDPROPS_LDB_PATH "/tmp/nprops.ldb"


static TALLOC_CTX *mem_ctx;
static struct namedprops_context *nprops;
static int ret;


static void ldb_q_setup(void)
{
	mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	ret = mapistore_namedprops_ldb_init(mem_ctx, NAMEDPROPS_LDB_PATH, &nprops);

	if (ret != MAPISTORE_SUCCESS) {
		fprintf(stderr, "Error initializing namedprops %d", errno);
		ck_abort();
	}
}

static void ldb_q_teardown(void)
{
	unlink(NAMEDPROPS_LDB_PATH);
	talloc_free(mem_ctx);
}


START_TEST (test_next_unused_id) {
	ck_assert_int_eq(next_unused_id(nprops), NEXT_UNUSED_ID);
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

	ck_assert_int_eq(get_mapped_id(nprops, nameid, &prop),
			 MAPISTORE_SUCCESS);
	ck_assert_int_eq(prop, 37153);

	// A couple more...
	nameid.lpguid.time_low = 0x62004;
	nameid.kind.lid = 32978;
	ck_assert_int_eq(get_mapped_id(nprops, nameid, &prop),
			 MAPISTORE_SUCCESS);
	ck_assert_int_eq(prop, 37524);

	nameid.kind.lid = 32901;
	ck_assert_int_eq(get_mapped_id(nprops, nameid, &prop),
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

	ck_assert_int_eq(get_mapped_id(nprops, nameid, &prop),
			 MAPISTORE_SUCCESS);
	ck_assert_int_eq(prop, 38342);

	// Another...
	nameid.kind.lpwstr.Name = "http://schemas.microsoft.com/exchange/searchfolder";
	ck_assert_int_eq(get_mapped_id(nprops, nameid, &prop),
			 MAPISTORE_SUCCESS);
	ck_assert_int_eq(prop, 38365);
} END_TEST

START_TEST (test_get_mapped_id_not_found) {
	struct MAPINAMEID nameid = {0};
	uint16_t prop;

	ck_assert_int_eq(get_mapped_id(nprops, nameid, &prop),
			 MAPISTORE_ERROR);
} END_TEST

START_TEST (test_get_nameid_type) {
	uint16_t prop_type = 0;

	ret = get_nameid_type(nprops, 38306, &prop_type);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert_int_eq(prop_type, PT_NULL);

	get_nameid_type(nprops, 37975, &prop_type);
	ck_assert_int_eq(prop_type, PT_UNICODE);

	/* mappedId should be unique? It's not
	$ grep mappedId mapistore_namedprops.ldif | sort | uniq -c | grep "2 "
		2 mappedId: 37097
		2 mappedId: 37287
		2 mappedId: 37452
	ret = get_nameid_type(nprops, 37097, &prop_type);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert_int_eq(prop_type, PT_LONG);*/

	ret = get_nameid_type(nprops, 37090, &prop_type);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert_int_eq(prop_type, PT_SYSTIME);
} END_TEST

START_TEST (test_get_nameid_type_not_found) {
	uint16_t prop_type = -1;

	ret = get_nameid_type(nprops, 42, &prop_type);
	ck_assert_int_eq(ret, MAPISTORE_ERROR);
} END_TEST

START_TEST (test_get_nameid_MNID_STRING) {
	TALLOC_CTX *mem_ctx = talloc(NULL, TALLOC_CTX);
	struct MAPINAMEID *nameid;

	get_nameid(nprops, 38306, mem_ctx, &nameid);
	ck_assert(nameid != NULL);
	ck_assert_str_eq("urn:schemas:httpmail:junkemail",
			 nameid->kind.lpwstr.Name);

	get_nameid(nprops, 38344, mem_ctx, &nameid);
	ck_assert(nameid != NULL);
	ck_assert_str_eq("http://schemas.microsoft.com/exchange/mailbox-owner-name",
			 nameid->kind.lpwstr.Name);

	talloc_free(mem_ctx);
} END_TEST

START_TEST (test_get_nameid_MNID_ID) {
	TALLOC_CTX *mem_ctx = talloc(NULL, TALLOC_CTX);
	struct MAPINAMEID *nameid;

	get_nameid(nprops, 38212, mem_ctx, &nameid);
	ck_assert(nameid != NULL);
	ck_assert_int_eq(32778, nameid->kind.lid);

	get_nameid(nprops, 38111, mem_ctx, &nameid);
	ck_assert(nameid != NULL);
	ck_assert_int_eq(34063, nameid->kind.lid);

	talloc_free(mem_ctx);
} END_TEST

START_TEST (test_get_nameid_not_found) {
	TALLOC_CTX *mem_ctx = talloc(NULL, TALLOC_CTX);
	struct MAPINAMEID *nameid = NULL;

	ret = get_nameid(nprops, 42, mem_ctx, &nameid);
	ck_assert_int_eq(ret, MAPISTORE_ERROR);
	ck_assert(nameid == NULL);
} END_TEST

START_TEST (test_create_id_MNID_ID) {
	struct MAPINAMEID nameid = {0};
	uint16_t mapped_id = 42;

	nameid.ulKind = MNID_ID;
	nameid.kind.lid = 42;

	ret = create_id(nprops, nameid, mapped_id);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);

	mapped_id = 0;
	ret = get_mapped_id(nprops, nameid, &mapped_id);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert_int_eq(mapped_id, 42);
} END_TEST

START_TEST (test_create_id_MNID_STRING) {
	struct MAPINAMEID nameid = {0};
	uint16_t mapped_id = 41;
	TALLOC_CTX *mem_ctx = talloc(NULL, TALLOC_CTX);

	nameid.ulKind = MNID_STRING;
	nameid.kind.lpwstr.Name = talloc_strdup(mem_ctx, "foobar");

	ret = create_id(nprops, nameid, mapped_id);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);

	mapped_id = 0;
	ret = get_mapped_id(nprops, nameid, &mapped_id);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert_int_eq(mapped_id, 41);

	talloc_free(mem_ctx);
} END_TEST


Suite *namedprops_ldb_suite(void)
{
	Suite *s = suite_create("Named Properties LDB Backend");

	TCase *tc_ldb_q = tcase_create("LDB queries");
	tcase_add_unchecked_fixture(tc_ldb_q, ldb_q_setup, ldb_q_teardown);
	tcase_add_test(tc_ldb_q, test_next_unused_id);
	tcase_add_test(tc_ldb_q, test_get_mapped_id_MNID_ID);
	tcase_add_test(tc_ldb_q, test_get_mapped_id_MNID_STRING);
	tcase_add_test(tc_ldb_q, test_get_mapped_id_not_found);
	tcase_add_test(tc_ldb_q, test_get_nameid_type);
	tcase_add_test(tc_ldb_q, test_get_nameid_type_not_found);
	tcase_add_test(tc_ldb_q, test_get_nameid_MNID_ID);
	tcase_add_test(tc_ldb_q, test_get_nameid_MNID_STRING);
	tcase_add_test(tc_ldb_q, test_get_nameid_not_found);
	tcase_add_test(tc_ldb_q, test_create_id_MNID_ID);
	tcase_add_test(tc_ldb_q, test_create_id_MNID_STRING);

	suite_add_tcase(s, tc_ldb_q);

	return s;
}
