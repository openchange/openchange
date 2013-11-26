#include "test_common.h"
#include "openchangedb_backends.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiproxy/backends/openchangedb_ldb.h"
#include "libmapi/libmapi.h"
#include <inttypes.h>


#define CHECK_SUCCESS ck_assert_int_eq(ret, MAPI_E_SUCCESS)
#define CHECK_FAILURE ck_assert_int_ne(ret, MAPI_E_SUCCESS)
#define OPENCHANGEDB_LDB         RESOURCES_DIR "/openchange.ldb"
#define OPENCHANGEDB_SAMPLE_LDB  RESOURCES_DIR "/openchangedb_sample.ldb"
#define OPENCHANGEDB_SAMPLE_LDIF RESOURCES_DIR "/openchangedb_sample.ldif"


static TALLOC_CTX *mem_ctx;
static struct openchangedb_context *oc_ctx;
static enum MAPISTATUS ret;


static void ldb_setup(void)
{
	// Create from ldif is better because ldif is 14kb and ldb is 1.6MB but
	// I don't know how to import the rootDSE
	//create_ldb_from_ldif(OPENCHANGEDB_LDB, OPENCHANGEDB_SAMPLE_LDIF);
	copy(OPENCHANGEDB_SAMPLE_LDB, OPENCHANGEDB_LDB);

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	int ret = openchangedb_ldb_initialize(mem_ctx, RESOURCES_DIR, &oc_ctx);
	if (ret != MAPI_E_SUCCESS) {
		fprintf(stderr, "Error initializing openchangedb %d\n", ret);
		ck_abort();
	}
}

static void ldb_teardown(void)
{
	talloc_free(mem_ctx);
	unlink(OPENCHANGEDB_LDB);
}


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

START_TEST (test_get_mapistoreURI) {
	char *mapistoreURI;

	ret = openchangedb_get_mapistoreURI(mem_ctx, oc_ctx, 2305843009213693953,
					    &mapistoreURI, true);
	CHECK_SUCCESS;
	ck_assert_str_eq(mapistoreURI, "sogo://paco:paco@mail/folderSpam/");
	ret = openchangedb_get_mapistoreURI(mem_ctx, oc_ctx, 2305843009213693953,
					    &mapistoreURI, false);
	CHECK_SUCCESS;

	ret = openchangedb_get_mapistoreURI(mem_ctx, oc_ctx, 1873497444986126337,
					    &mapistoreURI, true);
	CHECK_SUCCESS;
	ck_assert_str_eq(mapistoreURI, "sogo://paco:paco@mail/folderDrafts/");
} END_TEST

START_TEST (test_set_mapistoreURI) {
	char *initial_uri = "sogo://paco:paco@mail/folderA1/";
	char *uri;
	uint64_t fid = 15708555500268290049U;
	ret = openchangedb_get_mapistoreURI(mem_ctx, oc_ctx, fid, &uri, true);
	CHECK_SUCCESS;
	ck_assert_str_eq(uri, initial_uri);

	ret = openchangedb_set_mapistoreURI(oc_ctx, fid, "foobar", true);
	CHECK_SUCCESS;

	ret = openchangedb_get_mapistoreURI(mem_ctx, oc_ctx, fid, &uri, true);
	CHECK_SUCCESS;
	ck_assert_str_eq(uri, "foobar");

	ret = openchangedb_set_mapistoreURI(oc_ctx, fid, initial_uri, true);
	CHECK_SUCCESS;
} END_TEST

START_TEST (test_get_parent_fid) {
	uint64_t pfid = 0ul, fid = 0ul;

	fid = 15708555500268290049ul;
	ret = openchangedb_get_parent_fid(oc_ctx, fid, &pfid, true);
	CHECK_SUCCESS;
	ck_assert_int_eq(1513209474796486657ul, pfid);

	fid = 216172782113783809ul;
	ret = openchangedb_get_parent_fid(oc_ctx, fid, &pfid, true);
	CHECK_SUCCESS;
	ck_assert_int_eq(72057594037927937ul, pfid);
} END_TEST

START_TEST (test_get_fid) {
	uint64_t fid;
	char *uri;

	uri = talloc_strdup(mem_ctx, "sogo://paco@contacts/personal/");
	ret = openchangedb_get_fid(oc_ctx, uri, &fid);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, 2017612633061982209ul);
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

START_TEST (test_get_ReceiveFolder) {
	uint64_t fid;
	const char *explicit;

	ret = openchangedb_get_ReceiveFolder(mem_ctx, oc_ctx, "paco",
					     "Report.IPM", &fid, &explicit);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, 1585267068834414593ul);
	ck_assert_str_eq(explicit, "Report.IPM");

	ret = openchangedb_get_ReceiveFolder(mem_ctx, oc_ctx, "paco", "IPM",
					     &fid, &explicit);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, 1585267068834414593ul);
	ck_assert_str_eq(explicit, "IPM");

	ret = openchangedb_get_ReceiveFolder(mem_ctx, oc_ctx, "paco", "all",
					     &fid, &explicit);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, 1585267068834414593ul);
	ck_assert_str_eq(explicit, "All");
} END_TEST

START_TEST (test_get_TransportFolder) {
	uint64_t fid;
	ret = openchangedb_get_TransportFolder(oc_ctx, "paco", &fid);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, 1657324662872342529ul);
} END_TEST

START_TEST (test_get_folder_count) {
	uint32_t count;
	uint64_t fid;

	fid = 1513209474796486657ul;
	ret = openchangedb_get_folder_count(oc_ctx, fid, &count);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 15);

	fid = 216172782113783809ul;
	ret = openchangedb_get_folder_count(oc_ctx, fid, &count);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 4);
} END_TEST

START_TEST (test_get_new_changeNumber) {
	uint64_t cn = 0, new_cn;
	int i;
	for (i = 0; i < 5; i++) {
		ret = openchangedb_get_new_changeNumber(oc_ctx, &cn);
		CHECK_SUCCESS;
		new_cn = ((exchange_globcnt(NEXT_CHANGE_NUMBER+i) << 16) | 0x0001);
		ck_assert(cn == new_cn);
	}
} END_TEST

START_TEST (test_get_next_changeNumber) {
	uint64_t new_cn = 0, next_cn = 0;
	int i;
	for (i = 0; i < 5; i++) {
		ret = openchangedb_get_next_changeNumber(oc_ctx, &next_cn);
		CHECK_SUCCESS;
		ret = openchangedb_get_new_changeNumber(oc_ctx, &new_cn);
		CHECK_SUCCESS;
		ck_assert(next_cn == new_cn);
	}
} END_TEST

START_TEST (test_get_folder_property) {
	void *data;
	uint64_t fid;
	uint32_t proptag;

	// PT_LONG
	proptag = PidTagAccess;
	fid = 2089670227099910145ul;
	openchangedb_get_folder_property(mem_ctx, oc_ctx, proptag, fid, &data);
	ck_assert_int_eq(63, *(int *)data);

	// PT_UNICODE
	proptag = PidTagDisplayName;
	fid = 15852670688344145921ul;
	openchangedb_get_folder_property(mem_ctx, oc_ctx, proptag, fid, &data);
	ck_assert_str_eq("A3", (char *)data);
	proptag = PidTagRights;
	openchangedb_get_folder_property(mem_ctx, oc_ctx, proptag, fid, &data);
	ck_assert_int_eq(2043, *(int *)data);

	// PT_SYSTIME
	proptag = PidTagLastModificationTime;
	fid = 720575940379279361ul;
	openchangedb_get_folder_property(mem_ctx, oc_ctx, proptag, fid, &data);
	ck_assert_int_eq(130268260180000000 >> 32,
			 ((struct FILETIME *)data)->dwHighDateTime);
	ck_assert_int_eq(130268260180000000 & 0xffffffff,
			 ((struct FILETIME *)data)->dwLowDateTime);

	// PT_BINARY
	proptag = PidTagIpmDraftsEntryId;
	fid = 720575940379279361ul;
	openchangedb_get_folder_property(mem_ctx, oc_ctx, proptag, fid, &data);
	ck_assert_int_eq(46, ((struct Binary_r *)data)->cb);
} END_TEST

START_TEST (test_set_folder_properties) {
	uint64_t fid;
	uint32_t proptag;
	struct FILETIME *last_modification, *last_modification_after;
	uint64_t change_number, change_number_after;
	struct SRow *row = talloc_zero(mem_ctx, struct SRow);
	char *display_name;

	// We have to check that last_modification_time and change_number have
	// changed (besides the property we are setting)
	fid = 15852670688344145921ul;
	proptag = PidTagLastModificationTime;
	ret = openchangedb_get_folder_property(mem_ctx, oc_ctx, proptag, fid,
					       (void **)&last_modification);
	CHECK_SUCCESS;
	proptag = PidTagChangeNumber;
	ret = openchangedb_get_folder_property(mem_ctx, oc_ctx, proptag, fid,
					       (void **)&change_number);
	CHECK_SUCCESS;

	proptag = PidTagDisplayName;
	ret = openchangedb_get_folder_property(mem_ctx, oc_ctx, proptag, fid,
					       (void **)&display_name);
	CHECK_SUCCESS;
	ck_assert_str_eq(display_name, "A3");

	row->cValues = 1;
	row->lpProps = talloc_zero(mem_ctx, struct SPropValue);
	row->lpProps[0].ulPropTag = PidTagDisplayName;
	row->lpProps[0].value.lpszW = talloc_strdup(mem_ctx, "foo");
	ret = openchangedb_set_folder_properties(oc_ctx, fid, row);
	CHECK_SUCCESS;

	proptag = PidTagLastModificationTime;
	ret = openchangedb_get_folder_property(mem_ctx, oc_ctx, proptag, fid,
					       (void **)&last_modification_after);
	CHECK_SUCCESS;
	proptag = PidTagChangeNumber;
	ret = openchangedb_get_folder_property(mem_ctx, oc_ctx, proptag, fid,
					       (void **)&change_number_after);
	CHECK_SUCCESS;

	ck_assert(change_number != change_number_after);
	ck_assert(last_modification->dwHighDateTime
		  != last_modification_after->dwHighDateTime);
	ck_assert(last_modification->dwLowDateTime
		  != last_modification_after->dwLowDateTime);

	proptag = PidTagDisplayName;
	ret = openchangedb_get_folder_property(mem_ctx, oc_ctx, proptag, fid,
					       (void **)&display_name);
	CHECK_SUCCESS;
	ck_assert_str_eq(display_name, "foo");
} END_TEST

START_TEST (test_get_fid_by_name) {
	uint64_t pfid, fid = 0;
	pfid = 1513209474796486657ul;
	ret = openchangedb_get_fid_by_name(oc_ctx, pfid, "A2", &fid);
	CHECK_SUCCESS;
	ck_assert(fid == 15780613094306217985ul);
} END_TEST

START_TEST (test_get_mid_by_subject) {
	uint64_t pfid, mid = 0;
	pfid = 576460752303423489ul;
	ret = openchangedb_get_mid_by_subject(oc_ctx, pfid,
					      "USER-/CN=RECIPIENTS/CN=PACO",
					      true, &mid);
	CHECK_SUCCESS;
	ck_assert(mid == 2522015791327477761ul);
} END_TEST

START_TEST (test_delete_folder) {
	uint64_t fid, pfid;
	uint32_t count = 0;

	fid = 11961560610296037377ul;
	pfid = 1513209474796486657ul;
	ret = openchangedb_get_folder_count(oc_ctx, pfid, &count);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 15);

	ret = openchangedb_delete_folder(oc_ctx, fid);
	CHECK_SUCCESS;

	ret = openchangedb_get_folder_count(oc_ctx, pfid, &count);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 14);
} END_TEST

START_TEST (test_set_ReceiveFolder) {
	uint64_t fid_1 = 15708555500268290049ul,
		 fid_2 = 15780613094306217985ul, fid;
	const char *e;

	ret = openchangedb_set_ReceiveFolder(oc_ctx, "paco", "whatever", fid_1);
	CHECK_SUCCESS;

	ret = openchangedb_get_ReceiveFolder(mem_ctx, oc_ctx, "paco",
					     "whatever", &fid, &e);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, fid_1);

	ret = openchangedb_set_ReceiveFolder(oc_ctx, "paco", "whatever", fid_2);
	CHECK_SUCCESS;
	ret = openchangedb_get_ReceiveFolder(mem_ctx, oc_ctx, "paco",
						     "whatever", &fid, &e);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, fid_2);
} END_TEST

START_TEST (test_get_users_from_partial_uri) {
	uint32_t count;
	char **uris, **users;
	const char *partial = "sogo://*";

	ret = openchangedb_get_users_from_partial_uri(mem_ctx, oc_ctx, partial,
						      &count, &uris, &users);
	// FIXME mailboxDN bug?
	ck_assert_int_eq(ret, MAPI_E_NOT_FOUND);
} END_TEST

START_TEST (test_create_mailbox) {
	char *data;

	ret = openchangedb_create_mailbox(oc_ctx, "chuck", 1, 11);
	CHECK_SUCCESS;

	ret = openchangedb_get_folder_property(mem_ctx, oc_ctx,
					       PidTagDisplayName, 11,
					       (void **)&data);
	CHECK_SUCCESS;
	ck_assert_str_eq("OpenChange Mailbox: chuck", (char *)data);

} END_TEST

START_TEST (test_create_folder) {
	uint64_t pfid, fid, changenumber;
	uint32_t count, count_after;

	pfid = 1513209474796486657ul;

	ret = openchangedb_get_folder_count(oc_ctx, pfid, &count);
	CHECK_SUCCESS;

	fid = 4242;
	changenumber = 424242;
	ret = openchangedb_create_folder(oc_ctx, pfid, fid, changenumber,
					 "sogo://paco@mail/folderOhlala", 100);
	CHECK_SUCCESS;

	ret = openchangedb_get_folder_count(oc_ctx, pfid, &count_after);
	CHECK_SUCCESS;
	ck_assert_int_eq(count + 1, count_after);
} END_TEST

START_TEST (test_get_message_count) {
	uint32_t count = 0;
	uint64_t fid = 576460752303423489ul;
	ret = openchangedb_get_message_count(oc_ctx, fid, &count, false);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 1);
	ret = openchangedb_get_message_count(oc_ctx, fid, &count, true);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 0);
} END_TEST

START_TEST (test_get_system_idx) {
	int system_idx = 0;
	uint64_t fid = 432345564227567617ul;
	ret = openchangedb_get_system_idx(oc_ctx, fid, &system_idx);
	CHECK_SUCCESS;
	ck_assert_int_eq(9, system_idx);

	fid = 1729382256910270465ul;
	ret = openchangedb_get_system_idx(oc_ctx, fid, &system_idx);
	CHECK_SUCCESS;
	ck_assert_int_eq(15, system_idx);
} END_TEST

START_TEST (test_create_and_edit_message) {
	void *msg;
	uint64_t mid, fid;
	uint32_t prop;
	void *data;
	struct SRow row;

	fid = 720575940379279361ul;
	mid = 10;
	ret = openchangedb_message_create(mem_ctx, oc_ctx, mid, fid, false, &msg);
	CHECK_SUCCESS;

	ret = openchangedb_message_save(oc_ctx, msg, 0);
	CHECK_SUCCESS;

	ret = openchangedb_message_open(mem_ctx, oc_ctx, mid, fid, &msg, 0);
	CHECK_SUCCESS;

	prop = PidTagParentFolderId;
	ret = openchangedb_message_get_property(mem_ctx, oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert(fid == *(uint64_t *)data);

	row.cValues = 1;
	row.lpProps = talloc_zero(mem_ctx, struct SPropValue);
	row.lpProps[0].ulPropTag = PidTagDisplayName;
	row.lpProps[0].value.lpszW = talloc_strdup(mem_ctx, "foobar");
	ret = openchangedb_message_set_properties(mem_ctx, oc_ctx, msg, &row);

	prop = PidTagDisplayName;
	ret = openchangedb_message_get_property(mem_ctx, oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("foobar", (char *)data);
} END_TEST

START_TEST (test_build_table) {
	void *table, *data;
	uint64_t fid;
	enum MAPITAGS prop;
	uint32_t i;

	fid = 720575940379279361ul;
	ret = openchangedb_table_init(mem_ctx, oc_ctx, 1, fid, &table);
	CHECK_SUCCESS;
	prop = PidTagFolderId;
	for (i = 0; i < 13; i++) {
		CHECK_SUCCESS;
		ret = openchangedb_table_get_property(mem_ctx, oc_ctx, table,
						      prop, i, true, &data);
	}
	ck_assert_int_eq(ret, MAPI_E_INVALID_OBJECT);
} END_TEST


Suite *openchangedb_ldb_suite(void)
{
	Suite *s = suite_create ("OpenchangeDB LDB Backend");

	TCase *tc_ldb = tcase_create("ldb backend public functions");
	tcase_add_unchecked_fixture(tc_ldb, ldb_setup, ldb_teardown);

	tcase_add_test(tc_ldb, test_get_SystemFolderID);
	tcase_add_test(tc_ldb, test_get_PublicFolderID);
	tcase_add_test(tc_ldb, test_get_MailboxGuid);
	tcase_add_test(tc_ldb, test_get_MailboxReplica);
	tcase_add_test(tc_ldb, test_get_PublicFolderReplica);
	tcase_add_test(tc_ldb, test_get_mapistoreURI);
	tcase_add_test(tc_ldb, test_set_mapistoreURI);
	tcase_add_test(tc_ldb, test_get_parent_fid);
	tcase_add_test(tc_ldb, test_get_fid);
	tcase_add_test(tc_ldb, test_get_MAPIStoreURIs);
	tcase_add_test(tc_ldb, test_get_ReceiveFolder);
	tcase_add_test(tc_ldb, test_get_TransportFolder);
	tcase_add_test(tc_ldb, test_get_folder_count);
	tcase_add_test(tc_ldb, test_get_new_changeNumber);
	tcase_add_test(tc_ldb, test_get_next_changeNumber);
	tcase_add_test(tc_ldb, test_get_folder_property);
	tcase_add_test(tc_ldb, test_set_folder_properties);
	tcase_add_test(tc_ldb, test_get_fid_by_name);
	tcase_add_test(tc_ldb, test_get_mid_by_subject);
	tcase_add_test(tc_ldb, test_delete_folder);
	tcase_add_test(tc_ldb, test_set_ReceiveFolder);
	tcase_add_test(tc_ldb, test_get_users_from_partial_uri);
	tcase_add_test(tc_ldb, test_create_mailbox);
	tcase_add_test(tc_ldb, test_create_folder);
	tcase_add_test(tc_ldb, test_get_message_count);
	tcase_add_test(tc_ldb, test_get_system_idx);

	tcase_add_test(tc_ldb, test_create_and_edit_message);

	tcase_add_test(tc_ldb, test_build_table);

	suite_add_tcase(s, tc_ldb);

	return s;
}
