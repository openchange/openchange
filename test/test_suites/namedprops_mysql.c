#include "namedprops_backends.h"
#include "test_common.h"

#include "mapiproxy/libmapistore/backends/namedprops_mysql.c"

// According to the initial ldif file we insert into database
#define NEXT_UNUSED_ID 38392


START_TEST (test_parse_connection_with_password) {
	TALLOC_CTX *local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	char *host, *user, *passwd, *db;
	bool ret;

	ret = parse_connection_string(local_mem_ctx,
				      "mysql://root:secret@localhost/mysql",
				      &host, &user, &passwd, &db);
	ck_assert(ret == true);
	ck_assert_str_eq(user, "root");
	ck_assert_str_eq(passwd, "secret");
	ck_assert_str_eq(host, "localhost");
	ck_assert_str_eq(db, "mysql");

	ret = parse_connection_string(local_mem_ctx,
				      "mysql://lang:langpass@10.11.12.13/openchange_db",
				      &host, &user, &passwd, &db);
	ck_assert(ret == true);
	ck_assert_str_eq(user, "lang");
	ck_assert_str_eq(passwd, "langpass");
	ck_assert_str_eq(host, "10.11.12.13");
	ck_assert_str_eq(db, "openchange_db");

	talloc_free(local_mem_ctx);
} END_TEST


START_TEST (test_parse_connection_without_password) {
	TALLOC_CTX *local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	char *host, *user, *passwd, *db;
	bool ret;

	ret = parse_connection_string(local_mem_ctx, "mysql://root@localhost/mysql",
				      &host, &user, &passwd, &db);
	ck_assert(ret == true);
	ck_assert_str_eq(user, "root");
	ck_assert_str_eq(passwd, "");
	ck_assert_str_eq(host, "localhost");
	ck_assert_str_eq(db, "mysql");

	ret = parse_connection_string(local_mem_ctx,
				      "mysql://chuck@192.168.42.42/norris",
		 		      &host, &user, &passwd, &db);
	ck_assert(ret == true);
	ck_assert_str_eq(user, "chuck");
	ck_assert_str_eq(passwd, "");
	ck_assert_str_eq(host, "192.168.42.42");
	ck_assert_str_eq(db, "norris");

	talloc_free(local_mem_ctx);
} END_TEST


START_TEST (test_parse_connection_fail) {
	TALLOC_CTX *local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	char *host, *user, *passwd, *db;

	bool ret = parse_connection_string(local_mem_ctx, "ldb:///tmp/lalal",
					  &host, &user, &passwd, &db);
	ck_assert(ret == false);

	ret = parse_connection_string(local_mem_ctx, "mysql://ÑAÑÑAÑAÑAÑAÑA",
				      &host, &user, &passwd, &db);
	ck_assert(ret == false);

	talloc_free(local_mem_ctx);
} END_TEST


static MYSQL *conn = NULL;

static void mysql_setup(void)
{
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

static void mysql_teardown(void)
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


static TALLOC_CTX *mem_ctx;
static struct namedprops_context *nprops;

static void mysql_q_setup(void)
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

static void mysql_q_teardown(void)
{
	mysql_query(nprops->data, "DROP DATABASE " MYSQL_DB);
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
			 MAPISTORE_ERR_NOT_FOUND);
} END_TEST

START_TEST (test_get_nameid_type) {
	uint16_t prop_type = -1;

	get_nameid_type(nprops, 38306, &prop_type);
	ck_assert_int_eq(prop_type, PT_NULL);

	get_nameid_type(nprops, 37975, &prop_type);
	ck_assert_int_eq(prop_type, PT_UNICODE);

	get_nameid_type(nprops, 37097, &prop_type);
	ck_assert_int_eq(prop_type, PT_LONG);

	get_nameid_type(nprops, 37090, &prop_type);
	ck_assert_int_eq(prop_type, PT_SYSTIME);
} END_TEST

START_TEST (test_get_nameid_type_not_found) {
	uint16_t prop_type = -1;

	enum mapistore_error ret = get_nameid_type(nprops, 42, &prop_type);
	ck_assert_int_eq(ret, MAPISTORE_ERR_NOT_FOUND);
} END_TEST

START_TEST (test_get_nameid_MNID_STRING) {
	TALLOC_CTX *local_mem_ctx = talloc(NULL, TALLOC_CTX);
	struct MAPINAMEID *nameid;

	get_nameid(nprops, 38306, local_mem_ctx, &nameid);
	ck_assert(nameid != NULL);
	ck_assert_str_eq("urn:schemas:httpmail:junkemail",
			 nameid->kind.lpwstr.Name);

	get_nameid(nprops, 38344, local_mem_ctx, &nameid);
	ck_assert(nameid != NULL);
	ck_assert_str_eq("http://schemas.microsoft.com/exchange/mailbox-owner-name",
			 nameid->kind.lpwstr.Name);

	talloc_free(local_mem_ctx);
} END_TEST

START_TEST (test_get_nameid_MNID_ID) {
	TALLOC_CTX *local_mem_ctx = talloc(NULL, TALLOC_CTX);
	struct MAPINAMEID *nameid;

	get_nameid(nprops, 38212, local_mem_ctx, &nameid);
	ck_assert(nameid != NULL);
	ck_assert_int_eq(32778, nameid->kind.lid);

	get_nameid(nprops, 38111, local_mem_ctx, &nameid);
	ck_assert(nameid != NULL);
	ck_assert_int_eq(34063, nameid->kind.lid);

	talloc_free(local_mem_ctx);
} END_TEST


START_TEST (test_get_nameid_not_found) {
	TALLOC_CTX *local_mem_ctx = talloc(NULL, TALLOC_CTX);
	struct MAPINAMEID *nameid = NULL;

	enum mapistore_error ret = get_nameid(nprops, 42, local_mem_ctx, &nameid);
	ck_assert_int_eq(ret, MAPISTORE_ERR_NOT_FOUND);
	ck_assert(nameid == NULL);
	talloc_free(local_mem_ctx);
} END_TEST

START_TEST (test_create_id_MNID_ID) {
	struct MAPINAMEID nameid = {0};
	uint16_t mapped_id = 42;

	nameid.ulKind = MNID_ID;
	nameid.kind.lid = 42;

	int ret = create_id(nprops, nameid, mapped_id);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);

	mapped_id = 0;
	ret = get_mapped_id(nprops, nameid, &mapped_id);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert_int_eq(mapped_id, 42);
} END_TEST

START_TEST (test_create_id_MNID_STRING) {
	struct MAPINAMEID nameid = {0};
	uint16_t mapped_id = 41;
	TALLOC_CTX *local_mem_ctx = talloc(NULL, TALLOC_CTX);

	nameid.ulKind = MNID_STRING;
	nameid.kind.lpwstr.Name = talloc_strdup(local_mem_ctx, "foobar");

	int ret = create_id(nprops, nameid, mapped_id);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);

	mapped_id = 0;
	ret = get_mapped_id(nprops, nameid, &mapped_id);
	ck_assert_int_eq(ret, MAPISTORE_SUCCESS);
	ck_assert_int_eq(mapped_id, 41);

	talloc_free(local_mem_ctx);
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

	suite_add_tcase(s, tc_core);
	suite_add_tcase(s, tc_mysql);
	suite_add_tcase(s, tc_mysql_q);

	return s;
}

