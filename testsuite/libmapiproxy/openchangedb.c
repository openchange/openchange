#include "testsuite.h"
#include "testsuite_common.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiproxy/backends/openchangedb_ldb.h"
#include "mapiproxy/libmapiproxy/backends/openchangedb_mysql.h"
#include "libmapi/libmapi.h"
#include <inttypes.h>
#include <mysql/mysql.h>

#define OPENCHANGEDB_SAMPLE_SQL		RESOURCES_DIR "/openchangedb_sample.sql"
#define OPENCHANGEDB_LDB		RESOURCES_DIR "/openchange.ldb"
#define OPENCHANGEDB_SAMPLE_LDIF	RESOURCES_DIR "/openchangedb_sample.ldif"
#define LDB_DEFAULT_CONTEXT 		"CN=First Administrative Group,CN=First Organization,CN=ZENTYAL,DC=zentyal-domain,DC=lan"
#define LDB_ROOT_CONTEXT 		"CN=ZENTYAL,DC=zentyal-domain,DC=lan"

#define CHECK_SUCCESS ck_assert_int_eq(retval, MAPI_E_SUCCESS)
#define CHECK_FAILURE ck_assert_int_ne(retval, MAPI_E_SUCCESS)

static TALLOC_CTX 			*g_mem_ctx;
static struct openchangedb_context 	*g_oc_ctx;
static enum MAPISTATUS			retval;

#define USER1 "paco"
// v Unit test ----------------------------------------------------------------

START_TEST (test_get_SystemFolderID) {
	uint64_t folder_id = 0;

	retval = openchangedb_get_SystemFolderID(g_oc_ctx, USER1, 2, &folder_id);
	CHECK_SUCCESS;
	ck_assert_int_eq(folder_id, 17510839776146620417ul);

	retval = openchangedb_get_SystemFolderID(g_oc_ctx, USER1, 14, &folder_id);
	CHECK_SUCCESS;
	ck_assert_int_eq(folder_id, 18375530904601755649ul);

	retval = openchangedb_get_SystemFolderID(g_oc_ctx, "chuck", 14, &folder_id);
	CHECK_FAILURE;

	retval = openchangedb_get_SystemFolderID(g_oc_ctx, USER1, 15, &folder_id);
	CHECK_SUCCESS;
	ck_assert_int_eq(folder_id, 1125899906842625ul);
} END_TEST

START_TEST (test_get_SpecialFolderID) {
	uint64_t folder_id = 0;

	retval = openchangedb_get_SpecialFolderID(g_oc_ctx, USER1, 2, &folder_id);
	CHECK_SUCCESS;
	ck_assert_int_eq(folder_id, 289356276058554369);

	retval = openchangedb_get_SpecialFolderID(g_oc_ctx, USER1, 5, &folder_id);
	CHECK_SUCCESS;
	ck_assert_int_eq(folder_id, 505529058172338177);

	retval = openchangedb_get_SpecialFolderID(g_oc_ctx, USER1, 6, &folder_id);
	CHECK_FAILURE;
} END_TEST

START_TEST (test_get_PublicFolderID) {
	uint64_t folder_id = 0;

	retval = openchangedb_get_PublicFolderID(g_oc_ctx, USER1, 14, &folder_id);
	CHECK_FAILURE;

	retval = openchangedb_get_PublicFolderID(g_oc_ctx, USER1, 5, &folder_id);
	CHECK_SUCCESS;
	ck_assert_int_eq(folder_id, 504403158265495553);

	retval = openchangedb_get_PublicFolderID(g_oc_ctx, USER1, 6, &folder_id);
	CHECK_SUCCESS;
	ck_assert_int_eq(folder_id, 360287970189639681);
} END_TEST

START_TEST (test_get_MailboxGuid) {
	struct GUID *guid = talloc_zero(g_mem_ctx, struct GUID);
	struct GUID *expected_guid = talloc_zero(g_mem_ctx, struct GUID);

	retval = openchangedb_get_MailboxGuid(g_oc_ctx, USER1, guid);
	CHECK_SUCCESS;

	GUID_from_string("13c54881-02f6-4ade-ba7d-8b28c5f638c6", expected_guid);
	ck_assert(GUID_equal(expected_guid, guid));

	talloc_free(guid);
	talloc_free(expected_guid);
} END_TEST

START_TEST (test_get_MailboxReplica) {
	TALLOC_CTX *local_mem_ctx = talloc_new(NULL);
	struct GUID *repl = talloc_zero(local_mem_ctx, struct GUID);
	struct GUID *expected_repl = talloc_zero(local_mem_ctx, struct GUID);
	uint16_t *repl_id = talloc_zero(local_mem_ctx, uint16_t);

	retval = openchangedb_get_MailboxReplica(g_oc_ctx, USER1, repl_id, repl);
	CHECK_SUCCESS;

	GUID_from_string("d87292c1-1bc3-4370-a734-98b559b69a52", expected_repl);
	ck_assert(GUID_equal(expected_repl, repl));

	ck_assert_int_eq(*repl_id, 1);

	talloc_free(local_mem_ctx);
} END_TEST

START_TEST (test_get_PublicFolderReplica) {
	TALLOC_CTX *local_mem_ctx = talloc_new(NULL);
	struct GUID *repl = talloc_zero(local_mem_ctx, struct GUID);
	struct GUID *expected_repl = talloc_zero(local_mem_ctx, struct GUID);
	uint16_t *repl_id = talloc_zero(local_mem_ctx, uint16_t);

	retval = openchangedb_get_PublicFolderReplica(g_oc_ctx, USER1, repl_id, repl);
	CHECK_SUCCESS;

	GUID_from_string("c4898b91-da9d-4f3e-9ae4-8a8bd5051b89", expected_repl);
	ck_assert(GUID_equal(expected_repl, repl));

	ck_assert_int_eq(*repl_id, 1);

	talloc_free(local_mem_ctx);
} END_TEST

START_TEST (test_get_mapistoreURI) {
	char *mapistoreURI;
	uint64_t fid;

	fid = 577586652210266113ul;
	retval = openchangedb_get_mapistoreURI(g_mem_ctx, g_oc_ctx, USER1,
					    fid, &mapistoreURI, true);
	CHECK_SUCCESS;
	ck_assert_str_eq(mapistoreURI, "sogo://paco:paco@mail/folderSpam/");

	retval = openchangedb_get_mapistoreURI(g_mem_ctx, g_oc_ctx, USER1,
					    fid, &mapistoreURI, false);
	CHECK_FAILURE;

	fid = 145241087982698497ul;
	retval = openchangedb_get_mapistoreURI(g_mem_ctx, g_oc_ctx, USER1,
					    fid, &mapistoreURI, true);
	CHECK_SUCCESS;
	ck_assert_str_eq(mapistoreURI, "sogo://paco:paco@mail/folderDrafts/");
} END_TEST

START_TEST (test_set_mapistoreURI) {
	char *initial_uri = "sogo://paco:paco@mail/folderA1/";
	char *uri;
	uint64_t fid = 13980299143264862209ul;
	retval = openchangedb_get_mapistoreURI(g_mem_ctx, g_oc_ctx, USER1, fid, &uri, true);
	CHECK_SUCCESS;
	ck_assert_str_eq(uri, initial_uri);

	retval = openchangedb_set_mapistoreURI(g_oc_ctx, USER1, fid, "foobar");
	CHECK_SUCCESS;

	retval = openchangedb_get_mapistoreURI(g_mem_ctx, g_oc_ctx, USER1, fid, &uri, true);
	CHECK_SUCCESS;
	ck_assert_str_eq(uri, "foobar");

	retval = openchangedb_set_mapistoreURI(g_oc_ctx, USER1, fid, initial_uri);
	CHECK_SUCCESS;
} END_TEST

START_TEST (test_get_parent_fid) {
	uint64_t pfid = 0ul, fid;

	fid = 13980299143264862209ul;
	retval = openchangedb_get_parent_fid(g_oc_ctx, USER1, fid, &pfid, true);
	CHECK_SUCCESS;
	ck_assert_int_eq(18231415716525899777ul, pfid);

	fid = 216172782113783809ul;
	retval = openchangedb_get_parent_fid(g_oc_ctx, USER1, fid, &pfid, false);
	CHECK_SUCCESS;
	ck_assert_int_eq(72057594037927937ul, pfid);

	fid = 504403158265495553ul;
	retval = openchangedb_get_parent_fid(g_oc_ctx, USER1, fid, &pfid, false);
	CHECK_SUCCESS;
	ck_assert_int_eq(216172782113783809ul, pfid);
} END_TEST

START_TEST (test_get_parent_fid_which_is_the_mailbox) {
	uint64_t pfid = 0ul, fid = 0ul;

	fid = 18159358122487971841ul;
	retval = openchangedb_get_parent_fid(g_oc_ctx, USER1, fid, &pfid, true);
	CHECK_SUCCESS;
	ck_assert_int_eq(17438782182108692481ul, pfid);
} END_TEST

START_TEST (test_get_fid) {
	uint64_t fid;
	char *uri;

	uri = talloc_strdup(g_mem_ctx, "sogo://paco@contacts/personal/");
	retval = openchangedb_get_fid(g_oc_ctx, uri, &fid);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, 289356276058554369ul);
} END_TEST

START_TEST (test_get_MAPIStoreURIs) {
	struct StringArrayW_r *uris = talloc_zero(g_mem_ctx, struct StringArrayW_r);
	bool found = false;
	int i;

	retval = openchangedb_get_MAPIStoreURIs(g_oc_ctx, USER1, g_mem_ctx, &uris);
	CHECK_SUCCESS;
	ck_assert_int_eq(uris->cValues, 23);

	for (i = 0; i < uris->cValues; i++) {
		found = strcmp(uris->lppszW[i], "sogo://paco:paco@mail/folderFUCK/") == 0;
		if (found) break;
	}
	ck_assert(found);
} END_TEST

START_TEST (test_get_ReceiveFolder) {
	uint64_t fid = 0, expected_fid = 18303473310563827713ul;
	const char *explicit;

	retval = openchangedb_get_ReceiveFolder(g_mem_ctx, g_oc_ctx, USER1,
						"Report.IPM", &fid, &explicit);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, expected_fid);
	ck_assert_str_eq(explicit, "Report.IPM");

	fid = 0;
	retval = openchangedb_get_ReceiveFolder(g_mem_ctx, g_oc_ctx, USER1, "IPM", &fid, &explicit);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, expected_fid);
	ck_assert_str_eq(explicit, "IPM");

	fid = 0;
	retval = openchangedb_get_ReceiveFolder(g_mem_ctx, g_oc_ctx, USER1, "all", &fid, &explicit);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, expected_fid);
	ck_assert_str_eq(explicit, "All");
} END_TEST

START_TEST (test_get_TransportFolder) {
	uint64_t fid;
	retval = openchangedb_get_TransportFolder(g_oc_ctx, USER1, &fid);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, 18375530904601755649ul);
} END_TEST

START_TEST (test_get_Transport_folder_when_has_unusual_display_name) {
	uint64_t fid = 0, transport_fid = 18375530904601755649ul;
	struct SRow *row = talloc_zero(g_mem_ctx, struct SRow);

	row->cValues = 1;
	row->lpProps = talloc_zero(g_mem_ctx, struct SPropValue);
	row->lpProps[0].ulPropTag = PidTagDisplayName;
	row->lpProps[0].value.lpszW = talloc_strdup(g_mem_ctx, "wH4t3v3r mate");
	retval = openchangedb_set_folder_properties(g_oc_ctx, USER1, transport_fid, row);
	CHECK_SUCCESS;

	retval = openchangedb_get_TransportFolder(g_oc_ctx, USER1, &fid);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, transport_fid);
} END_TEST

START_TEST (test_get_folder_count) {
	uint32_t count;
	uint64_t fid;

	fid = 18231415716525899777ul;
	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, fid, &count);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 15);

	fid = 17438782182108692481ul;
	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, fid, &count);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 12);
} END_TEST

START_TEST (test_get_folder_count_public_folder) {
	uint32_t count;

	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, 216172782113783809ul, &count);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 4);

	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, 648518346341351425ul, &count);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 0);

	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, 72057594037927937ul, &count);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 2);
} END_TEST

START_TEST (test_get_new_changeNumber) {
	uint64_t cn = 0, new_cn;
	int i;
	for (i = 0; i < 5; i++) {
		retval = openchangedb_get_new_changeNumber(g_oc_ctx, USER1, &cn);
		CHECK_SUCCESS;
		new_cn = ((exchange_globcnt(NEXT_CHANGE_NUMBER+i) << 16) | 0x0001);
		ck_assert(cn == new_cn);
	}
} END_TEST

START_TEST (test_get_new_changeNumbers) {
	uint64_t cn;
	int i;
	struct UI8Array_r *cns;

	retval = openchangedb_get_new_changeNumbers(g_oc_ctx, g_mem_ctx, USER1, 10, &cns);
	CHECK_SUCCESS;
	ck_assert_int_eq(10, cns->cValues);
	for (i = 0; i < 10; i++) {
		cn = ((exchange_globcnt(NEXT_CHANGE_NUMBER+5+i) << 16) | 0x0001);
		ck_assert(cns->lpui8[i] == cn);
	}
} END_TEST

START_TEST (test_get_next_changeNumber) {
	uint64_t new_cn = 0, next_cn = 0;
	int i;
	for (i = 0; i < 5; i++) {
		retval = openchangedb_get_next_changeNumber(g_oc_ctx, USER1, &next_cn);
		CHECK_SUCCESS;
		retval = openchangedb_get_new_changeNumber(g_oc_ctx, USER1, &new_cn);
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
	fid = 17871127746336260097ul;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag, fid, &data);
	CHECK_SUCCESS;
	ck_assert_int_eq(63, *(int *)data);

	// PT_UNICODE
	proptag = PidTagDisplayName;
	fid = 14124414331340718081ul;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag, fid, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("A3", (char *)data);
	proptag = PidTagRights;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag, fid, &data);
	CHECK_SUCCESS;
	ck_assert_int_eq(2043, *(int *)data);

	// PT_SYSTIME
	proptag = PidTagLastModificationTime;
	fid = 17438782182108692481ul;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag, fid, &data);
	CHECK_SUCCESS;
	ck_assert_int_eq(130268260180000000 >> 32, ((struct FILETIME *)data)->dwHighDateTime);
	ck_assert_int_eq(130268260180000000 & 0xffffffff, ((struct FILETIME *)data)->dwLowDateTime);

	// Mailbox display name
	proptag = PidTagDisplayName;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag, fid, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("OpenChange Mailbox: paco", (char *)data);

	// PT_BINARY
	proptag = PidTagIpmDraftsEntryId;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag, fid, &data);
	ck_assert_int_eq(46, ((struct Binary_r *)data)->cb);
} END_TEST

START_TEST (test_get_public_folder_property) {
	void *data;
	uint32_t proptag;
	uint64_t fid;

	fid = 216172782113783809ul;
	proptag = PidTagAttributeReadOnly;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag, fid, &data);
	CHECK_SUCCESS;
	ck_assert_int_eq(0, *(int *)data);

	proptag = PidTagDisplayName;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag, fid, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("NON_IPM_SUBTREE", (char *)data);

	proptag = PidTagCreationTime;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag, fid, &data);
	CHECK_SUCCESS;
	ck_assert_int_eq(130264095410000000 >> 32, ((struct FILETIME *)data)->dwHighDateTime);
	ck_assert_int_eq(130264095410000000 & 0xffffffff, ((struct FILETIME *)data)->dwLowDateTime);
} END_TEST

START_TEST (test_set_folder_properties) {
	uint64_t fid;
	uint32_t proptag;
	struct FILETIME *last_modification, *last_modification_after;
	uint64_t change_number, change_number_after;
	struct SRow *row = talloc_zero(g_mem_ctx, struct SRow);
	char *display_name;

	// We have to check that last_modification_time and change_number have
	// changed (besides the property we are setting)
	fid = 14124414331340718081ul;
	proptag = PidTagLastModificationTime;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag,
						  fid, (void **)&last_modification);
	CHECK_SUCCESS;

	proptag = PidTagChangeNumber;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag,
						  fid, (void **)&change_number);
	CHECK_SUCCESS;

	proptag = PidTagDisplayName;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag,
						  fid, (void **)&display_name);
	CHECK_SUCCESS;
	ck_assert_str_eq(display_name, "A3");

	row->cValues = 1;
	row->lpProps = talloc_zero(g_mem_ctx, struct SPropValue);
	row->lpProps[0].ulPropTag = PidTagDisplayName;
	row->lpProps[0].value.lpszW = talloc_strdup(g_mem_ctx, "foo");
	retval = openchangedb_set_folder_properties(g_oc_ctx, USER1, fid, row);
	CHECK_SUCCESS;

	proptag = PidTagLastModificationTime;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag,
						  fid, (void **)&last_modification_after);
	CHECK_SUCCESS;

	proptag = PidTagChangeNumber;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag,
						  fid, (void **)&change_number_after);
	CHECK_SUCCESS;

	ck_assert(change_number != change_number_after);
	ck_assert(last_modification->dwHighDateTime != last_modification_after->dwHighDateTime);
	ck_assert(last_modification->dwLowDateTime != last_modification_after->dwLowDateTime);

	proptag = PidTagDisplayName;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag,
						  fid, (void **)&display_name);
	CHECK_SUCCESS;
	ck_assert_str_eq(display_name, "foo");
} END_TEST

START_TEST (test_set_folder_properties_on_mailbox) {
	uint64_t fid;
	uint32_t proptag;
	struct SRow *row = talloc_zero(g_mem_ctx, struct SRow);
	struct Binary_r *s1, *s2;

	fid = 18303473310563827713ul;
	proptag = PidTagIpmTaskEntryId;

	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag,
						  fid, (void **)&s1);
	CHECK_SUCCESS;
	row->cValues = 1;
	row->lpProps = talloc_zero(g_mem_ctx, struct SPropValue);
	row->lpProps[0].ulPropTag = proptag;
	row->lpProps[0].value.bin.lpb = (uint8_t *)talloc_strdup(g_mem_ctx, "foo");
	row->lpProps[0].value.bin.cb = 3;
	retval = openchangedb_set_folder_properties(g_oc_ctx, USER1, fid, row);
	CHECK_SUCCESS;

	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag,
						  fid, (void **)&s2);
	CHECK_SUCCESS;
	ck_assert_int_eq(s2->cb, 3);
	ck_assert_str_eq((char *)s2->lpb, "foo");
} END_TEST

START_TEST (test_set_public_folder_properties) {
	uint32_t proptag;
	struct FILETIME *last_modification, *last_modification_after;
	uint64_t change_number, change_number_after, fid;
	struct SRow *row = talloc_zero(g_mem_ctx, struct SRow);
	char *display_name;

	fid = 648518346341351425ul;
	// We have to check that last_modification_time and change_number have
	// changed (besides the property we are setting)
	proptag = PidTagLastModificationTime;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag,
						  fid, (void **)&last_modification);
	CHECK_SUCCESS;

	proptag = PidTagChangeNumber;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag,
						  fid, (void **)&change_number);
	CHECK_SUCCESS;

	proptag = PidTagDisplayName;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag,
						  fid, (void **)&display_name);
	CHECK_SUCCESS;
	ck_assert_str_eq(display_name, "Events Root");

	row->cValues = 1;
	row->lpProps = talloc_zero(g_mem_ctx, struct SPropValue);
	row->lpProps[0].ulPropTag = PidTagDisplayName;
	row->lpProps[0].value.lpszW = talloc_strdup(g_mem_ctx, "foo public");
	retval = openchangedb_set_folder_properties(g_oc_ctx, USER1, fid, row);
	CHECK_SUCCESS;

	proptag = PidTagLastModificationTime;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag,
						  fid, (void **)&last_modification_after);
	CHECK_SUCCESS;

	proptag = PidTagChangeNumber;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag,
						  fid, (void **)&change_number_after);
	CHECK_SUCCESS;

	ck_assert(change_number != change_number_after);
	ck_assert(last_modification->dwHighDateTime != last_modification_after->dwHighDateTime);
	ck_assert(last_modification->dwLowDateTime != last_modification_after->dwLowDateTime);

	proptag = PidTagDisplayName;
	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, USER1, proptag,
						  fid, (void **)&display_name);
	CHECK_SUCCESS;
	ck_assert_str_eq(display_name, "foo public");
} END_TEST

START_TEST (test_get_fid_by_name) {
	uint64_t pfid, fid = 0;

	pfid = 18231415716525899777ul;
	retval = openchangedb_get_fid_by_name(g_oc_ctx, USER1, pfid, "A2", &fid);
	CHECK_SUCCESS;
	ck_assert(fid == 14052356737302790145ul);

	pfid = 216172782113783809ul;
	retval = openchangedb_get_fid_by_name(g_oc_ctx, USER1, pfid, "EFORMS REGISTRY", &fid);
	CHECK_SUCCESS;
	ck_assert(fid == 288230376151711745ul);
} END_TEST

START_TEST (test_get_mid_by_subject) {
	uint64_t pfid, mid = 0;
	pfid = 145241087982698497ul;
	retval = openchangedb_get_mid_by_subject(g_oc_ctx, USER1, pfid,
						 "Sample message on system folder",
						 true, &mid);
	CHECK_SUCCESS;
	ck_assert(mid == 2522015791327477762ul);
} END_TEST

START_TEST (test_get_mid_by_subject_on_public_folder) {
	uint64_t pfid, mid = 0;
	pfid = 576460752303423489ul;
	retval = openchangedb_get_mid_by_subject(g_oc_ctx, USER1, pfid,
						 "USER-/CN=RECIPIENTS/CN=PACO",
						 false, &mid);
	CHECK_SUCCESS;
	ck_assert(mid == 2522015791327477761ul);
} END_TEST

START_TEST (test_delete_folder) {
	uint64_t fid, pfid;
	uint32_t count = 0;

	fid = 10233304253292609537ul;
	pfid = 18231415716525899777ul;
	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, pfid, &count);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 15);

	retval = openchangedb_delete_folder(g_oc_ctx, USER1, fid);
	CHECK_SUCCESS;

	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, pfid, &count);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 14);
} END_TEST

START_TEST (test_delete_public_folder) {
	uint64_t fid, pfid;
	uint32_t count = 0;
	size_t n;

	fid = 288230376151711745ul;
	pfid = 216172782113783809ul;
	n = 4;
	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, pfid, &count);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, n);

	retval = openchangedb_delete_folder(g_oc_ctx, USER1, fid);
	CHECK_SUCCESS;

	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, pfid, &count);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, n - 1);
} END_TEST

START_TEST (test_set_ReceiveFolder) {
	uint64_t fid_1, fid_2, fid;
	const char *explicit;

	fid_1 = 13980299143264862209ul;
	retval = openchangedb_set_ReceiveFolder(g_oc_ctx, USER1, "trash1", fid_1);
	CHECK_SUCCESS;
	retval = openchangedb_set_ReceiveFolder(g_oc_ctx, USER1, "whatever", fid_1);
	CHECK_SUCCESS;
	retval = openchangedb_set_ReceiveFolder(g_oc_ctx, USER1, "trash2", fid_1);
	CHECK_SUCCESS;

	retval = openchangedb_get_ReceiveFolder(g_mem_ctx, g_oc_ctx, USER1,
						"whatever", &fid, &explicit);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, fid_1);
	ck_assert_str_eq(explicit, "whatever");

	fid_2 = 14052356737302790145ul;
	retval = openchangedb_set_ReceiveFolder(g_oc_ctx, USER1, "trash1", fid_2);
	CHECK_SUCCESS;
	retval = openchangedb_set_ReceiveFolder(g_oc_ctx, USER1, "whatever", fid_2);
	CHECK_SUCCESS;
	retval = openchangedb_set_ReceiveFolder(g_oc_ctx, USER1, "trash2", fid_2);
	CHECK_SUCCESS;
	retval = openchangedb_get_ReceiveFolder(g_mem_ctx, g_oc_ctx, USER1,
						"whatever", &fid, &explicit);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid, fid_2);
	ck_assert_str_eq(explicit, "whatever");
} END_TEST

START_TEST (test_get_users_from_partial_uri) {
	uint32_t count;
	char **uris, **users;
	const char *partial = "sogo://*";

	retval = openchangedb_get_users_from_partial_uri(g_mem_ctx, g_oc_ctx, partial,
							 &count, &uris, &users);
	// FIXME mailboxDN bug?
	ck_assert_int_ne(retval, MAPI_E_SUCCESS);
} END_TEST

START_TEST (test_create_mailbox) {
	char *data;
	uint32_t *data_int;
	uint64_t fid = 14989949884725985281ul;

	retval = openchangedb_create_mailbox(g_oc_ctx, "chuck", "First Organization",
					     "First Administrative Group", fid,
					     "OpenChange Mailbox: chuck");
	CHECK_SUCCESS;

	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, "chuck",
						  PidTagDisplayName, fid,
						  (void **)&data);
	CHECK_SUCCESS;
	ck_assert_str_eq("OpenChange Mailbox: chuck", (char *)data);

	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, "chuck",
						  PidTagAccess, fid,
						  (void **)&data_int);
	CHECK_SUCCESS;
	ck_assert_int_eq(63, *data_int);

	retval = openchangedb_get_folder_property(g_mem_ctx, g_oc_ctx, "chuck",
						  PidTagRights, fid,
						  (void **)&data_int);
	CHECK_SUCCESS;
	ck_assert_int_eq(2043, *data_int);
} END_TEST

START_TEST (test_create_folder) {
	uint64_t pfid, fid, changenumber;
	uint32_t count, count_after;

	pfid = 18231415716525899777ul;

	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, pfid, &count);
	CHECK_SUCCESS;

	fid = 10524912329164849153ul;
	changenumber = 424242;
	retval = openchangedb_create_folder(g_oc_ctx, USER1, pfid, fid, changenumber,
					    "sogo://paco@mail/folderOhlala", 100);
	CHECK_SUCCESS;

	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, pfid, &count_after);
	CHECK_SUCCESS;
	ck_assert_int_eq(count + 1, count_after);
} END_TEST

START_TEST (test_create_folder_without_mapistore_uri) {
	uint64_t pfid, fid, changenumber;
	uint32_t count, count_after;
	struct StringArrayW_r *uris = talloc_zero(g_mem_ctx, struct StringArrayW_r);
	int uris_before;

	retval = openchangedb_get_MAPIStoreURIs(g_oc_ctx, USER1, g_mem_ctx, &uris);
	CHECK_SUCCESS;
	uris_before = uris->cValues;

	pfid = 18231415716525899777ul;
	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, pfid, &count);
	CHECK_SUCCESS;

	fid = 10596969923202777089ul;
	changenumber = 424243;
	retval = openchangedb_create_folder(g_oc_ctx, USER1, pfid, fid, changenumber, NULL, 100);
	CHECK_SUCCESS;

	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, pfid, &count_after);
	CHECK_SUCCESS;
	ck_assert_int_eq(count + 1, count_after);

	// Check this has not changed
	retval = openchangedb_get_MAPIStoreURIs(g_oc_ctx, USER1, g_mem_ctx, &uris);
	CHECK_SUCCESS;
	ck_assert_int_eq(uris->cValues, uris_before);
} END_TEST

START_TEST (test_create_folder_and_display_name) {
	struct SRow *row = talloc_zero(g_mem_ctx, struct SRow);
	uint64_t pfid, fid, fid2, changenumber;
	uint32_t count, count_after;

	pfid = 18231415716525899777ul;

	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, pfid, &count);
	CHECK_SUCCESS;

	fid = 10669027517240705025ul;
	changenumber = 424244;
	retval = openchangedb_create_folder(g_oc_ctx, USER1, pfid, fid, changenumber,
					    "sogo://paco@mail/folderOhlala", 100);
	CHECK_SUCCESS;

	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, pfid, &count_after);
	CHECK_SUCCESS;
	ck_assert_int_eq(count + 1, count_after);

	row->cValues = 1;
	row->lpProps = talloc_zero(g_mem_ctx, struct SPropValue);
	row->lpProps[0].ulPropTag = PidTagDisplayName;
	row->lpProps[0].value.lpszW = talloc_strdup(g_mem_ctx, "some name");
	retval = openchangedb_set_folder_properties(g_oc_ctx, USER1, fid, row);
	CHECK_SUCCESS;

	retval = openchangedb_get_fid_by_name(g_oc_ctx, USER1, pfid, "some name", &fid2);
	CHECK_SUCCESS;
	ck_assert_int_eq(fid2, fid);
} END_TEST

START_TEST (test_create_public_folder) {
	uint64_t pfid, fid, changenumber;
	uint32_t count, count_after;

	pfid = 216172782113783809ul;

	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, pfid, &count);
	CHECK_SUCCESS;

	fid = 42;
	changenumber = 4242;
	retval = openchangedb_create_folder(g_oc_ctx, USER1, pfid, fid, changenumber, NULL, 2);
	CHECK_SUCCESS;

	retval = openchangedb_get_folder_count(g_oc_ctx, USER1, pfid, &count_after);
	CHECK_SUCCESS;
	ck_assert_int_eq(count + 1, count_after);
} END_TEST

START_TEST (test_get_message_count) {
	uint32_t count = 0;
	uint64_t fid = 145241087982698497ul;
	retval = openchangedb_get_message_count(g_oc_ctx, USER1, fid, &count, false);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 1);

	retval = openchangedb_get_message_count(g_oc_ctx, USER1, fid, &count, true);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 0);
} END_TEST

START_TEST (test_get_message_count_from_public_folder) {
	uint32_t count = 0;
	uint64_t fid = 576460752303423489ul;
	retval = openchangedb_get_message_count(g_oc_ctx, USER1, fid, &count, false);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 1);

	retval = openchangedb_get_message_count(g_oc_ctx, USER1, 1, &count, false);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 0);

	retval = openchangedb_get_message_count(g_oc_ctx, USER1, fid, &count, true);
	CHECK_SUCCESS;
	ck_assert_int_eq(count, 0);
} END_TEST

START_TEST (test_get_system_idx) {
	int system_idx = 0;
	uint64_t fid = 17582897370184548353ul;
	retval = openchangedb_get_system_idx(g_oc_ctx, USER1, fid, &system_idx);
	CHECK_SUCCESS;
	ck_assert_int_eq(3, system_idx);

	fid = 1125899906842625ul;
	retval = openchangedb_get_system_idx(g_oc_ctx, USER1, fid, &system_idx);
	CHECK_SUCCESS;
	ck_assert_int_eq(15, system_idx);
} END_TEST

START_TEST (test_get_system_idx_from_public_folder) {
	int system_idx = 0;
	uint64_t fid = 504403158265495553ul;
	retval = openchangedb_get_system_idx(g_oc_ctx, USER1, fid, &system_idx);
	CHECK_SUCCESS;
	ck_assert_int_eq(5, system_idx);

	fid = 216172782113783809ul;
	retval = openchangedb_get_system_idx(g_oc_ctx, USER1, fid, &system_idx);
	CHECK_SUCCESS;
	ck_assert_int_eq(3, system_idx);
} END_TEST

START_TEST (test_create_and_edit_message) {
	void *msg;
	uint64_t mid, fid;
	uint32_t prop;
	void *data;
	struct SRow row;

	fid = 17438782182108692481ul;
	mid = 10;
	retval = openchangedb_message_create(g_mem_ctx, g_oc_ctx, USER1, mid, fid, false, &msg);
	CHECK_SUCCESS;

	retval = openchangedb_message_save(g_oc_ctx, msg, 0);
	CHECK_SUCCESS;

	retval = openchangedb_message_open(g_mem_ctx, g_oc_ctx, USER1, mid, fid, &msg, NULL);
	CHECK_SUCCESS;

	prop = PidTagParentFolderId;
	retval = openchangedb_message_get_property(g_mem_ctx, g_oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert(fid == *(uint64_t *)data);

	row.cValues = 2;
	row.lpProps = talloc_zero_array(g_mem_ctx, struct SPropValue, row.cValues);
	row.lpProps[0].ulPropTag = PidTagDisplayName;
	row.lpProps[0].value.lpszW = talloc_strdup(g_mem_ctx, "foobar");
	row.lpProps[1].ulPropTag = PidTagNormalizedSubject;
	row.lpProps[1].value.lpszW = talloc_strdup(g_mem_ctx, "subject foo'foo");
	retval = openchangedb_message_set_properties(g_mem_ctx, g_oc_ctx, msg, &row);
	CHECK_SUCCESS;

	prop = PidTagDisplayName;
	retval = openchangedb_message_get_property(g_mem_ctx, g_oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("foobar", (char *)data);

	prop = PidTagNormalizedSubject;
	retval = openchangedb_message_get_property(g_mem_ctx, g_oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("subject foo'foo", (char *)data);

	retval = openchangedb_message_save(g_oc_ctx, msg, 0);
	CHECK_SUCCESS;

	// Now reopen message and read again the property
	retval = openchangedb_message_open(g_mem_ctx, g_oc_ctx, USER1, mid, fid, &msg, 0);
	CHECK_SUCCESS;
	prop = PidTagDisplayName;
	retval = openchangedb_message_get_property(g_mem_ctx, g_oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("foobar", (char *)data);

	prop = PidTagNormalizedSubject;
	retval = openchangedb_message_get_property(g_mem_ctx, g_oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("subject foo'foo", (char *)data);
} END_TEST

START_TEST (test_create_and_edit_message_on_public_folder) {
	void *msg;
	uint64_t mid, fid;
	uint32_t prop;
	void *data;
	struct SRow row;

	fid = 648518346341351425ul;
	mid = 1234512345ul;
	retval = openchangedb_message_create(g_mem_ctx, g_oc_ctx, USER1, mid, fid, true, &msg);
	CHECK_SUCCESS;

	retval = openchangedb_message_save(g_oc_ctx, msg, 0);
	CHECK_SUCCESS;

	retval = openchangedb_message_open(g_mem_ctx, g_oc_ctx, USER1, mid, fid, &msg, 0);
	CHECK_SUCCESS;

	prop = PidTagParentFolderId;
	retval = openchangedb_message_get_property(g_mem_ctx, g_oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert(fid == *(uint64_t *)data);

	row.cValues = 1;
	row.lpProps = talloc_zero(g_mem_ctx, struct SPropValue);
	row.lpProps[0].ulPropTag = PidTagDisplayName;
	row.lpProps[0].value.lpszW = talloc_strdup(g_mem_ctx, "foobar 'foo'");
	retval = openchangedb_message_set_properties(g_mem_ctx, g_oc_ctx, msg, &row);
	CHECK_SUCCESS;

	prop = PidTagDisplayName;
	retval = openchangedb_message_get_property(g_mem_ctx, g_oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("foobar 'foo'", (char *)data);

	retval = openchangedb_message_save(g_oc_ctx, msg, 0);
	CHECK_SUCCESS;

	// Now reopen message and read again the property
	retval = openchangedb_message_open(g_mem_ctx, g_oc_ctx, USER1, mid, fid, &msg, 0);
	CHECK_SUCCESS;
	prop = PidTagDisplayName;
	retval = openchangedb_message_get_property(g_mem_ctx, g_oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("foobar 'foo'", (char *)data);
} END_TEST

START_TEST (test_set_property_message_with_change_key) {
	void *msg;
	uint64_t mid, fid;
	uint32_t prop;
	void *data;
	struct SRow row;

	fid = 17438782182108692481ul;
	mid = 11;
	retval = openchangedb_message_create(g_mem_ctx, g_oc_ctx, USER1, mid, fid, false, &msg);
	CHECK_SUCCESS;

	retval = openchangedb_message_save(g_oc_ctx, msg, 0);
	CHECK_SUCCESS;

	retval = openchangedb_message_open(g_mem_ctx, g_oc_ctx, USER1, mid, fid, &msg, NULL);
	CHECK_SUCCESS;

	prop = PidTagParentFolderId;
	retval = openchangedb_message_get_property(g_mem_ctx, g_oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert(fid == *(uint64_t *)data);

	row.cValues = 3;
	row.lpProps = talloc_zero_array(g_mem_ctx, struct SPropValue, row.cValues);
	row.lpProps[0].ulPropTag = PR_CHANGE_KEY; // it should be ignored
	row.lpProps[1].ulPropTag = PidTagDisplayName;
	row.lpProps[1].value.lpszW = talloc_strdup(g_mem_ctx, "foobar 2");
	row.lpProps[2].ulPropTag = PidTagNormalizedSubject;
	row.lpProps[2].value.lpszW = talloc_strdup(g_mem_ctx, "subject foo'foo 2");
	retval = openchangedb_message_set_properties(g_mem_ctx, g_oc_ctx, msg, &row);
	CHECK_SUCCESS;

	prop = PidTagDisplayName;
	retval = openchangedb_message_get_property(g_mem_ctx, g_oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("foobar 2", (char *)data);

	prop = PidTagNormalizedSubject;
	retval = openchangedb_message_get_property(g_mem_ctx, g_oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("subject foo'foo 2", (char *)data);

	retval = openchangedb_message_save(g_oc_ctx, msg, 0);
	CHECK_SUCCESS;

	// Now reopen message and read again the property
	retval = openchangedb_message_open(g_mem_ctx, g_oc_ctx, USER1, mid, fid, &msg, 0);
	CHECK_SUCCESS;
	prop = PidTagDisplayName;
	retval = openchangedb_message_get_property(g_mem_ctx, g_oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("foobar 2", (char *)data);

	prop = PidTagNormalizedSubject;
	retval = openchangedb_message_get_property(g_mem_ctx, g_oc_ctx, msg, prop, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("subject foo'foo 2", (char *)data);
} END_TEST

START_TEST (test_build_table_folders) {
	void *table, *data;
	uint64_t fid;
	enum MAPITAGS prop;
	uint32_t i;

	fid = 17438782182108692481ul;
	retval = openchangedb_table_init(g_mem_ctx, g_oc_ctx, USER1, 1, fid, &table);
	CHECK_SUCCESS;
	prop = PidTagFolderId;
	for (i = 0; i < 13; i++) {
		CHECK_SUCCESS;
		retval = openchangedb_table_get_property(g_mem_ctx, g_oc_ctx, table,
							 prop, i, false, &data);
	}
	ck_assert_int_eq(retval, MAPI_E_INVALID_OBJECT);

	retval = openchangedb_table_init(g_mem_ctx, g_oc_ctx, USER1, 1, fid, &table);
	CHECK_SUCCESS;
	prop = PidTagRights;
	for (i = 0; i < 13; i++) {
		CHECK_SUCCESS;
		retval = openchangedb_table_get_property(g_mem_ctx, g_oc_ctx, table,
							 prop, i, false, &data);
	}
	ck_assert_int_eq(retval, MAPI_E_INVALID_OBJECT);

	retval = openchangedb_table_init(g_mem_ctx, g_oc_ctx, USER1, 1, fid, &table);
	CHECK_SUCCESS;
	prop = PidTagDisplayName;
	for (i = 0; i < 13; i++) {
		CHECK_SUCCESS;
		retval = openchangedb_table_get_property(g_mem_ctx, g_oc_ctx, table,
							 prop, i, false, &data);
	}
	ck_assert_int_eq(retval, MAPI_E_INVALID_OBJECT);
} END_TEST

START_TEST (test_build_table_folders_with_restrictions) {
	void *table, *data;
	uint64_t fid;
	enum MAPITAGS prop;
	struct mapi_SRestriction res;

	fid = 17438782182108692481ul;
	retval = openchangedb_table_init(g_mem_ctx, g_oc_ctx, USER1, 1, fid, &table);
	CHECK_SUCCESS;

	res.rt = RES_PROPERTY;
	res.res.resProperty.ulPropTag = PidTagDisplayName;
	res.res.resProperty.lpProp.ulPropTag = PidTagDisplayName;
	res.res.resProperty.lpProp.value.lpszW = "Schedule";
	openchangedb_table_set_restrictions(g_oc_ctx, table, &res);
	CHECK_SUCCESS;

	prop = PidTagDisplayName;
	retval = openchangedb_table_get_property(g_mem_ctx, g_oc_ctx, table,
						 prop, 0, false, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("Schedule", (char *)data);
} END_TEST

START_TEST (test_build_table_folders_live_filtering) {
	void *table, *data;
	uint64_t fid;
	enum MAPITAGS prop;
	uint32_t i;
	struct mapi_SRestriction res;
	int ok = 0, bad = 0;
	int idx = 0;

	fid = 17438782182108692481ul;
	retval = openchangedb_table_init(g_mem_ctx, g_oc_ctx, USER1, 1, fid, &table);
	CHECK_SUCCESS;

	res.rt = RES_PROPERTY;
	res.res.resProperty.ulPropTag = PidTagDisplayName;
	res.res.resProperty.lpProp.ulPropTag = PidTagDisplayName;
	res.res.resProperty.lpProp.value.lpszW = "Schedule";
	openchangedb_table_set_restrictions(g_oc_ctx, table, &res);
	CHECK_SUCCESS;

	prop = PidTagDisplayName;
	for (i = 0; i < 13; i++) {
		retval = openchangedb_table_get_property(g_mem_ctx, g_oc_ctx, table,
							 prop, i, true, &data);
		if (retval == MAPI_E_SUCCESS) {
			ok++;
			idx = i;
		} else {
			bad++;
		}
	}
	ck_assert_int_eq(1, ok);
	retval = openchangedb_table_get_property(g_mem_ctx, g_oc_ctx, table,
						 prop, idx, true, &data);
	CHECK_SUCCESS;
	ck_assert_str_eq("Schedule", (char *)data);
} END_TEST

START_TEST (test_set_locale) {
	ck_assert(openchangedb_set_locale(g_oc_ctx, USER1, 0x1001));
	ck_assert(!openchangedb_set_locale(g_oc_ctx, USER1, 0x1001));
	ck_assert(openchangedb_set_locale(g_oc_ctx, USER1, 0x040c));
	ck_assert(openchangedb_set_locale(g_oc_ctx, USER1, 0x0422));
	ck_assert(!openchangedb_set_locale(g_oc_ctx, USER1, 0x0422));
} END_TEST

START_TEST (test_get_folders_names) {
	const char **names = openchangedb_get_folders_names(g_mem_ctx, g_oc_ctx, "en", "folders");
	ck_assert(names != NULL);
	ck_assert_str_eq(names[15], "Deleted Items");

	names = openchangedb_get_folders_names(g_mem_ctx, g_oc_ctx, "en", "special_folders");
	ck_assert(names != NULL);
	ck_assert_str_eq(names[0], "Drafts");
	ck_assert_str_eq(names[4], "Notes");
	ck_assert_str_eq(names[5], "Journal");

	names = openchangedb_get_folders_names(g_mem_ctx, g_oc_ctx, "fr", "special_folders");
	ck_assert(names == NULL);
} END_TEST

START_TEST (test_set_receive_folder_to_mailbox) {
	uint64_t mailbox_fid, fid;
	const char *explicit;

	mailbox_fid = 17438782182108692481ul;
	retval = openchangedb_set_ReceiveFolder(g_oc_ctx, USER1, "IPC", mailbox_fid);
	CHECK_SUCCESS;

	retval = openchangedb_get_ReceiveFolder(g_mem_ctx, g_oc_ctx, USER1, "IPC", &fid, &explicit);
	CHECK_SUCCESS;

	ck_assert_int_eq(fid, mailbox_fid);
	ck_assert_str_eq(explicit, "IPC");
} END_TEST

START_TEST (test_get_indexing_url) {
	enum MAPISTATUS retval;
	const char	*indexing_url;

	/* Valid indexing url */
	retval = openchangedb_get_indexing_url(g_oc_ctx, USER1, &indexing_url);
	CHECK_SUCCESS;
	ck_assert_str_eq(indexing_url, "mysql://openchange@localhost/openchange");

	/* indexing_url is NULL */
	retval = openchangedb_get_indexing_url(g_oc_ctx, "null_indexing_url", &indexing_url);
	ck_assert_int_eq(retval, MAPI_E_NOT_FOUND);

	/* indexing_url is empty - "" */
	retval = openchangedb_get_indexing_url(g_oc_ctx, "empty_indexing_url", &indexing_url);
	ck_assert_int_eq(retval, MAPI_E_NOT_FOUND);
} END_TEST

// ^ Unit test ----------------------------------------------------------------

// v Suite definition ---------------------------------------------------------

static void create_ldb_from_ldif(const char *ldb_path, const char *ldif_path,
				 const char *default_context, const char *root_context)
{
	FILE *f;
	struct ldb_ldif *ldif;
	struct ldb_context *ldb_ctx = NULL;
	TALLOC_CTX *local_mem_ctx = talloc_new(NULL);
	struct ldb_message *msg;

	ldb_ctx = ldb_init(local_mem_ctx, NULL);
	ldb_connect(ldb_ctx, ldb_path, 0, 0);
	f = fopen(ldif_path, "r");

	msg = ldb_msg_new(local_mem_ctx);
	msg->dn = ldb_dn_new(msg, ldb_ctx, "@INDEXLIST");
	ldb_msg_add_string(msg, "@IDXATTR", "cn");
	ldb_msg_add_string(msg, "@IDXATTR", "oleguid");
	ldb_msg_add_string(msg, "@IDXATTR", "mappedId");
	msg->elements[0].flags = LDB_FLAG_MOD_ADD;
	ldb_add(ldb_ctx, msg);

	while ((ldif = ldb_ldif_read_file(ldb_ctx, f))) {
		struct ldb_message *normalized_msg;
		ldb_msg_normalize(ldb_ctx, local_mem_ctx, ldif->msg,
				  &normalized_msg);
		ldb_add(ldb_ctx, normalized_msg);
		talloc_free(normalized_msg);
		ldb_ldif_read_free(ldb_ctx, ldif);
	}

	if (default_context && root_context) {
		msg = ldb_msg_new(local_mem_ctx);
		msg->dn = ldb_dn_new(msg, ldb_ctx, "@ROOTDSE");
		ldb_msg_add_string(msg, "defaultNamingContext", default_context);
		ldb_msg_add_string(msg, "rootDomainNamingContext", root_context);
		msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;
		ldb_add(ldb_ctx, msg);
	}

	fclose(f);
	talloc_free(local_mem_ctx);
}

static void ldb_setup(void)
{
	int ret;

	create_ldb_from_ldif(OPENCHANGEDB_LDB, OPENCHANGEDB_SAMPLE_LDIF,
			     LDB_DEFAULT_CONTEXT, LDB_ROOT_CONTEXT);

	g_mem_ctx = talloc_new(talloc_autofree_context());
	ret = openchangedb_ldb_initialize(g_mem_ctx, RESOURCES_DIR, &g_oc_ctx);
	if (ret != MAPI_E_SUCCESS) {
		fprintf(stderr, "Error initializing openchangedb %d\n", ret);
		ck_abort();
	}
}

static void ldb_teardown(void)
{
	talloc_free(g_mem_ctx);
	unlink(OPENCHANGEDB_LDB);
}

static void mysql_setup(void)
{
	g_mem_ctx = talloc_new(talloc_autofree_context());
	initialize_mysql_with_file(g_mem_ctx, OPENCHANGEDB_SAMPLE_SQL, &g_oc_ctx);
}

static void mysql_teardown(void)
{
	drop_mysql_database(g_oc_ctx->data, OC_TESTSUITE_MYSQL_DB);
	talloc_free(g_mem_ctx);
}

static Suite *openchangedb_create_suite(const char *backend_name,
					SFun setup, SFun teardown)
{
	char *suite_name = talloc_asprintf(talloc_autofree_context(),
					   "Openchangedb %s backend", backend_name);
	Suite *s = suite_create(suite_name);

	TCase *tc = tcase_create(suite_name);
	tcase_add_unchecked_fixture(tc, setup, teardown);

	tcase_add_test(tc, test_get_SystemFolderID);
	tcase_add_test(tc, test_get_SpecialFolderID);
	tcase_add_test(tc, test_get_PublicFolderID);
	tcase_add_test(tc, test_get_MailboxGuid);
	tcase_add_test(tc, test_get_MailboxReplica);
	tcase_add_test(tc, test_get_PublicFolderReplica);
	tcase_add_test(tc, test_get_mapistoreURI);
	tcase_add_test(tc, test_set_mapistoreURI);
	tcase_add_test(tc, test_get_parent_fid);
	tcase_add_test(tc, test_get_parent_fid_which_is_the_mailbox);
	tcase_add_test(tc, test_get_fid);
	tcase_add_test(tc, test_get_MAPIStoreURIs);
	tcase_add_test(tc, test_get_ReceiveFolder);
	tcase_add_test(tc, test_get_TransportFolder);
	tcase_add_test(tc, test_get_folder_count);
	tcase_add_test(tc, test_get_folder_count_public_folder);
	tcase_add_test(tc, test_get_new_changeNumber);
	tcase_add_test(tc, test_get_new_changeNumbers);
	tcase_add_test(tc, test_get_next_changeNumber);
	tcase_add_test(tc, test_get_folder_property);
	tcase_add_test(tc, test_get_public_folder_property);
	tcase_add_test(tc, test_set_folder_properties);
	tcase_add_test(tc, test_set_folder_properties_on_mailbox);
	tcase_add_test(tc, test_set_public_folder_properties);
	tcase_add_test(tc, test_get_fid_by_name);
	tcase_add_test(tc, test_get_mid_by_subject);
	tcase_add_test(tc, test_get_mid_by_subject_on_public_folder);
	tcase_add_test(tc, test_delete_folder);
	tcase_add_test(tc, test_delete_public_folder);
	tcase_add_test(tc, test_set_ReceiveFolder);
	tcase_add_test(tc, test_get_users_from_partial_uri);
	tcase_add_test(tc, test_create_mailbox);
	tcase_add_test(tc, test_create_folder);
	tcase_add_test(tc, test_create_folder_without_mapistore_uri);
	tcase_add_test(tc, test_create_folder_and_display_name);
	tcase_add_test(tc, test_create_public_folder);
	tcase_add_test(tc, test_get_message_count);
	tcase_add_test(tc, test_get_message_count_from_public_folder);
	tcase_add_test(tc, test_get_system_idx);
	tcase_add_test(tc, test_get_system_idx_from_public_folder);

	tcase_add_test(tc, test_create_and_edit_message);
	tcase_add_test(tc, test_create_and_edit_message_on_public_folder);
	tcase_add_test(tc, test_set_property_message_with_change_key);

	tcase_add_test(tc, test_build_table_folders);
	tcase_add_test(tc, test_build_table_folders_with_restrictions);
	tcase_add_test(tc, test_build_table_folders_live_filtering);
	tcase_add_test(tc, test_get_Transport_folder_when_has_unusual_display_name);

	if (strcmp(backend_name, "MySQL") == 0) {
		// Ugly workaround to test mysql only functions
		tcase_add_test(tc, test_set_locale);
		tcase_add_test(tc, test_get_folders_names);
		tcase_add_test(tc, test_get_indexing_url);
	}

	tcase_add_test(tc, test_set_receive_folder_to_mailbox);

	suite_add_tcase(s, tc);
	return s;
}

Suite *mapiproxy_openchangedb_ldb_suite(void)
{
	return openchangedb_create_suite("LDB", ldb_setup, ldb_teardown);
}

Suite *mapiproxy_openchangedb_mysql_suite(void)
{
	return openchangedb_create_suite("MySQL", mysql_setup, mysql_teardown);
}
