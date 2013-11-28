#include "test_common.h"
#include "openchangedb_backends.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiproxy/backends/openchangedb_mysql.h"
#include "libmapi/libmapi.h"
#include <inttypes.h>
#include <mysql/mysql.h>

#define OPENCHANGEDB_SAMPLE_SQL RESOURCES_DIR "/openchangedb_sample.sql"

static TALLOC_CTX *mem_ctx;
static struct openchangedb_context *oc_ctx;
static enum MAPISTATUS ret;


static void mysql_setup(void)
{
	const char *database;
	FILE *f;
	long int sql_size;
	size_t bytes_read;
	char *sql, *insert;
	bool inserts_to_execute;
	MYSQL *conn;

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	if (strlen(MYSQL_PASS) == 0) {
		database = talloc_asprintf(mem_ctx, "mysql://" MYSQL_USER "@"
					   MYSQL_HOST "/" MYSQL_DB);
	} else {
		database = talloc_asprintf(mem_ctx, "mysql://" MYSQL_USER ":"
					   MYSQL_PASS "@" MYSQL_HOST "/"
					   MYSQL_DB);
	}
	ret = openchangedb_mysql_initialize(mem_ctx, database, &oc_ctx);

	if (ret != MAPI_E_SUCCESS) {
		fprintf(stderr, "Error initializing openchangedb %d\n", ret);
		ck_abort();
	}

	// Populate database with sample data
	conn = oc_ctx->data;
	f = fopen(OPENCHANGEDB_SAMPLE_SQL, "r");
	if (!f) {
		fprintf(stderr, "file %s not found", OPENCHANGEDB_SAMPLE_SQL);
		ck_abort();
	}
	fseek(f, 0, SEEK_END);
	sql_size = ftell(f);
	rewind(f);
	sql = talloc_zero_array(mem_ctx, char, sql_size + 1);
	bytes_read = fread(sql, sizeof(char), sql_size, f);
	if (bytes_read != sql_size) {
		fprintf(stderr, "error reading file %s", OPENCHANGEDB_SAMPLE_SQL);
		ck_abort();
	}
	insert = strtok(sql, ";");
	inserts_to_execute = insert != NULL;
	while (inserts_to_execute) {
		ret = mysql_query(conn, insert) ? false : true;
		if (!ret) break;
		insert = strtok(NULL, ";");
		inserts_to_execute = ret && insert && strlen(insert) > 10;
	}
}

static void mysql_teardown(void)
{
	mysql_query(oc_ctx->data, "DROP DATABASE " MYSQL_DB);
	talloc_free(mem_ctx);
}

// v unit test ----------------------------------------------------------------

START_TEST (test_get_SystemFolderID) {
	uint64_t folder_id = 0;

	ret = openchangedb_get_SystemFolderID(oc_ctx, "paco", 14, &folder_id);
	CHECK_SUCCESS;
	ck_assert_int_eq(folder_id, 1657324662872342529);

	ret = openchangedb_get_SystemFolderID(oc_ctx, "chuck", 14, &folder_id);
	CHECK_FAILURE;

	ret = openchangedb_get_SystemFolderID(oc_ctx, "paco", 15, &folder_id);
	CHECK_SUCCESS;
	ck_assert_int_eq(folder_id, 1729382256910270465);
} END_TEST

START_TEST (test_get_PublicFolderID) {
	uint64_t folder_id = 0;

	ret = openchangedb_get_PublicFolderID(oc_ctx, 14, &folder_id);
	CHECK_FAILURE;

	ret = openchangedb_get_PublicFolderID(oc_ctx, 5, &folder_id);
	CHECK_SUCCESS;
	ck_assert_int_eq(folder_id, 504403158265495553);

	ret = openchangedb_get_PublicFolderID(oc_ctx, 6, &folder_id);
	CHECK_SUCCESS;
	ck_assert_int_eq(folder_id, 360287970189639681);
} END_TEST

START_TEST (test_get_MailboxGuid) {
	struct GUID *guid = talloc_zero(mem_ctx, struct GUID);
	struct GUID *expected_guid = talloc_zero(mem_ctx, struct GUID);

	ret = openchangedb_get_MailboxGuid(oc_ctx, "paco", guid);
	CHECK_SUCCESS;

	GUID_from_string("13c54881-02f6-4ade-ba7d-8b28c5f638c6", expected_guid);
	ck_assert(GUID_equal(expected_guid, guid));

	talloc_free(guid);
	talloc_free(expected_guid);
} END_TEST

START_TEST (test_get_MailboxReplica) {
	TALLOC_CTX *local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	struct GUID *repl = talloc_zero(local_mem_ctx, struct GUID);
	struct GUID *expected_repl = talloc_zero(local_mem_ctx, struct GUID);
	uint16_t *repl_id = talloc_zero(local_mem_ctx, uint16_t);

	ret = openchangedb_get_MailboxReplica(oc_ctx, "paco", repl_id, repl);
	CHECK_SUCCESS;

	GUID_from_string("d87292c1-1bc3-4370-a734-98b559b69a52", expected_repl);
	ck_assert(GUID_equal(expected_repl, repl));

	ck_assert_int_eq(*repl_id, 1);

	talloc_free(local_mem_ctx);
} END_TEST

START_TEST (test_get_PublicFolderReplica) {
	TALLOC_CTX *local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	struct GUID *repl = talloc_zero(local_mem_ctx, struct GUID);
	struct GUID *expected_repl = talloc_zero(local_mem_ctx, struct GUID);
	uint16_t *repl_id = talloc_zero(local_mem_ctx, uint16_t);

	ret = openchangedb_get_PublicFolderReplica(oc_ctx, repl_id, repl);
	CHECK_SUCCESS;

	GUID_from_string("c4898b91-da9d-4f3e-9ae4-8a8bd5051b89", expected_repl);
	ck_assert(GUID_equal(expected_repl, repl));

	ck_assert_int_eq(*repl_id, 1);

	talloc_free(local_mem_ctx);
} END_TEST

START_TEST (test_get_MAPIStoreURIs) {
	struct StringArrayW_r *uris = talloc_zero(mem_ctx, struct StringArrayW_r);
	bool found = false;
	int i;

	ret = openchangedb_get_MAPIStoreURIs(oc_ctx, "paco", mem_ctx, &uris);
	CHECK_SUCCESS;
	ck_assert_int_eq(uris->cValues, 23);

	for (i = 0; i < uris->cValues; i++) {
		found = strcmp(uris->lppszW[i], "sogo://paco:paco@mail/folderFUCK/") == 0;
		if (found) break;
	}
	ck_assert(found);
} END_TEST

// ^ unit test ----------------------------------------------------------------

// v test suite definition ----------------------------------------------------

Suite *openchangedb_mysql_suite(void)
{
	Suite *s = suite_create ("OpenchangeDB MySQL Backend");

	TCase *tc_mysql = tcase_create("mysql backend public functions");
	tcase_add_unchecked_fixture(tc_mysql, mysql_setup, mysql_teardown);

	tcase_add_test(tc_mysql, test_get_SystemFolderID);
	tcase_add_test(tc_mysql, test_get_PublicFolderID);
	tcase_add_test(tc_mysql, test_get_MailboxGuid);
	tcase_add_test(tc_mysql, test_get_MailboxReplica);
	tcase_add_test(tc_mysql, test_get_PublicFolderReplica);

	tcase_add_test(tc_mysql, test_get_MAPIStoreURIs);

	suite_add_tcase(s, tc_mysql);

	return s;
}
