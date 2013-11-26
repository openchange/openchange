#include "test_common.h"
#include "openchangedb_backends.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiproxy/backends/openchangedb_mysql.h"
#include "libmapi/libmapi.h"
#include <inttypes.h>


#define CHECK_SUCCESS ck_assert_int_eq(ret, MAPI_E_SUCCESS)
#define CHECK_FAILURE ck_assert_int_ne(ret, MAPI_E_SUCCESS)
#define OPENCHANGEDB_LDB         RESOURCES_DIR "/openchange.ldb"
#define OPENCHANGEDB_SAMPLE_LDIF RESOURCES_DIR "/openchangedb_sample.ldif"
#define LDB_DEFAULT_CONTEXT "CN=First Administrative Group,CN=First Organization,CN=ZENTYAL,DC=zentyal-domain,DC=lan"
#define LDB_ROOT_CONTEXT "CN=ZENTYAL,DC=zentyal-domain,DC=lan"


static TALLOC_CTX *mem_ctx;
static struct openchangedb_context *oc_ctx;
static enum MAPISTATUS ret;


static void mysql_setup(void)
{
	const char *database;
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
}

static void mysql_teardown(void)
{
	mysql_query(oc_ctx->data, "DROP DATABASE " MYSQL_DB);
	talloc_free(mem_ctx);
}


START_TEST (test_get_SystemFolderID) {
	/*uint64_t folder_id = 0;

	ret = openchangedb_get_SystemFolderID(oc_ctx, "paco", 14, &folder_id);
	CHECK_SUCCESS;
	ck_assert_int_eq(folder_id, 1657324662872342529);

	ret = openchangedb_get_SystemFolderID(oc_ctx, "chuck", 14, &folder_id);
	CHECK_FAILURE;

	ret = openchangedb_get_SystemFolderID(oc_ctx, "paco", 15, &folder_id);
	CHECK_SUCCESS;
	ck_assert_int_eq(folder_id, 1729382256910270465);*/
} END_TEST


Suite *openchangedb_mysql_suite(void)
{
	Suite *s = suite_create ("OpenchangeDB MySQL Backend");

	TCase *tc_mysql = tcase_create("mysql backend public functions");
	tcase_add_unchecked_fixture(tc_mysql, mysql_setup, mysql_teardown);

	tcase_add_test(tc_mysql, test_get_SystemFolderID);

	suite_add_tcase(s, tc_mysql);

	return s;
}
