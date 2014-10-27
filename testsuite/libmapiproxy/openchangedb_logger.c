/*
   OpenChange Unit Testing

   OpenChange Project

   Copyright (C) Kamen Mazdrashki <kamenim@openchange.org> 2014

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
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiproxy/backends/openchangedb_logger.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include <inttypes.h>


// According to the initial sample data
#define NEXT_CHANGE_NUMBER 1234
#define FOLDER_ID_EXPECTED 289356276058554369ul

#define CHECK_SUCCESS(ret) ck_assert_int_eq((ret), MAPI_E_SUCCESS)
#define CHECK_FAILURE(ret) ck_assert_int_ne((ret), MAPI_E_SUCCESS)


static TALLOC_CTX *mem_ctx;
static struct openchangedb_context *oc_ctx;

static struct openchangedb_context functions_called;

// v Unit test ----------------------------------------------------------------

START_TEST (test_call_get_SpecialFolderID) {
	uint64_t folder_id;

	CHECK_SUCCESS(openchangedb_get_SpecialFolderID(oc_ctx, "recipient", 1234, &folder_id));

	ck_assert_int_eq(folder_id, FOLDER_ID_EXPECTED);
	ck_assert_int_eq(functions_called.get_SpecialFolderID, 1);
} END_TEST

START_TEST (test_call_get_SystemFolderID) {
	uint64_t folder_id;

	CHECK_SUCCESS(openchangedb_get_SystemFolderID(oc_ctx, "recipient", 1234, &folder_id));

	ck_assert_int_eq(folder_id, FOLDER_ID_EXPECTED);
	ck_assert_int_eq(functions_called.get_SystemFolderID, 1);
} END_TEST


START_TEST (test_call_get_PublicFolderID) {
	uint64_t folder_id;

	CHECK_SUCCESS(openchangedb_get_PublicFolderID(oc_ctx, "usera", 1234, &folder_id));

	ck_assert_int_eq(folder_id, FOLDER_ID_EXPECTED);
	ck_assert_int_eq(functions_called.get_PublicFolderID, 1);
} END_TEST

START_TEST (test_call_get_distinguishedName) {
	char *dn;

	CHECK_SUCCESS(openchangedb_get_distinguishedName(mem_ctx, oc_ctx, 1234, &dn));

	ck_assert_int_eq(functions_called.get_distinguishedName, 1);
} END_TEST

START_TEST(test_call_get_MailboxGuid) {
	struct GUID MailboxGUID;

	CHECK_SUCCESS(openchangedb_get_MailboxGuid(oc_ctx, "recipient", &MailboxGUID));

	ck_assert_int_eq(functions_called.get_MailboxGuid, 1);
} END_TEST

START_TEST(test_call_get_MailboxReplica) {
	uint16_t ReplID;
	struct GUID ReplGUID;

	CHECK_SUCCESS(openchangedb_get_MailboxReplica(oc_ctx, "recipient", &ReplID, &ReplGUID));

	ck_assert_int_eq(functions_called.get_MailboxReplica, 1);
} END_TEST

START_TEST(test_call_get_PublicFolderReplica) {
	uint16_t ReplID;
	struct GUID ReplGUID;

	CHECK_SUCCESS(openchangedb_get_PublicFolderReplica(oc_ctx, "usera", &ReplID, &ReplGUID));

	ck_assert_int_eq(functions_called.get_PublicFolderReplica, 1);
} END_TEST

START_TEST(test_call_get_mapistoreURI) {
	char *mapistoreURL;

	CHECK_SUCCESS(openchangedb_get_mapistoreURI(mem_ctx, oc_ctx, "usera", FOLDER_ID_EXPECTED, &mapistoreURL, true));

	ck_assert_int_eq(functions_called.get_mapistoreURI, 1);
} END_TEST

START_TEST(test_call_set_mapistoreURI) {

	CHECK_SUCCESS(openchangedb_set_mapistoreURI(oc_ctx, "mail_user", FOLDER_ID_EXPECTED, "mapistoreURL"));

	ck_assert_int_eq(functions_called.set_mapistoreURI, 1);
} END_TEST

START_TEST(test_call_get_parent_fid) {
	uint64_t parent_fid;

	CHECK_SUCCESS(openchangedb_get_parent_fid(oc_ctx, "mail_user", FOLDER_ID_EXPECTED, &parent_fid, true));

	ck_assert_int_eq(functions_called.get_parent_fid, 1);
} END_TEST

START_TEST(test_call_get_fid) {
	uint64_t fid;

	CHECK_SUCCESS(openchangedb_get_fid(oc_ctx, "mapistoreURL", &fid));

	ck_assert_int_eq(functions_called.get_fid, 1);
} END_TEST

START_TEST(test_call_get_MAPIStoreURIs) {
	struct StringArrayW_r *uris;

	CHECK_SUCCESS(openchangedb_get_MAPIStoreURIs(oc_ctx, "mail_user", mem_ctx, &uris));

	ck_assert_int_eq(functions_called.get_MAPIStoreURIs, 1);
} END_TEST

START_TEST(test_call_get_ReceiveFolder) {
	uint64_t fid;
	const char *ExplicitMessageClass;

	CHECK_SUCCESS(openchangedb_get_ReceiveFolder(mem_ctx, oc_ctx, "recipient", "MessageClass",
		      &fid, &ExplicitMessageClass));

	ck_assert_int_eq(functions_called.get_ReceiveFolder, 1);
} END_TEST
// ^ Unit test ----------------------------------------------------------------


// v Mocked backend ----------------------------------------------------------------
static enum MAPISTATUS get_SpecialFolderID(struct openchangedb_context *self,
					  const char *recipient, uint32_t system_idx,
					  uint64_t *folder_id)
{
	*folder_id = FOLDER_ID_EXPECTED;
	functions_called.get_SpecialFolderID++;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_SystemFolderID(struct openchangedb_context *self,
					  const char *recipient, uint32_t SystemIdx,
					  uint64_t *FolderId)
{
	*FolderId = FOLDER_ID_EXPECTED;
	functions_called.get_SystemFolderID++;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_PublicFolderID(struct openchangedb_context *self,
					  const char *username,
					  uint32_t SystemIdx,
					  uint64_t *FolderId)
{
	*FolderId = FOLDER_ID_EXPECTED;
	functions_called.get_PublicFolderID++;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_distinguishedName(TALLOC_CTX *parent_ctx,
					     struct openchangedb_context *self,
					     uint64_t fid,
					     char **distinguishedName)
{
	*distinguishedName = "distinguishedName";
	functions_called.get_distinguishedName++;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_mailboxDN(TALLOC_CTX *parent_ctx,
				     struct openchangedb_context *self,
				     uint64_t fid,
				     char **mailboxDN)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_MailboxGuid(struct openchangedb_context *self,
				       const char *recipient,
				       struct GUID *MailboxGUID)
{
	functions_called.get_MailboxGuid++;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_MailboxReplica(struct openchangedb_context *self,
					  const char *recipient, uint16_t *ReplID,
				  	  struct GUID *ReplGUID)
{
	functions_called.get_MailboxReplica++;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_PublicFolderReplica(struct openchangedb_context *self,
					       const char *username,
					       uint16_t *ReplID,
					       struct GUID *ReplGUID)
{
	functions_called.get_PublicFolderReplica++;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_mapistoreURI(TALLOC_CTX *parent_ctx,
				        struct openchangedb_context *self,
				        const char *username,
				        uint64_t fid, char **mapistoreURL,
				        bool mailboxstore)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS set_mapistoreURI(struct openchangedb_context *self,
					const char *username, uint64_t fid,
					const char *mapistoreURL)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_parent_fid(struct openchangedb_context *self,
				      const char *username, uint64_t fid,
				      uint64_t *parent_fidp, bool mailboxstore)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_fid(struct openchangedb_context *self,
			       const char *mapistoreURL, uint64_t *fidp)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_MAPIStoreURIs(struct openchangedb_context *self,
					 const char *username,
					 TALLOC_CTX *mem_ctx,
					 struct StringArrayW_r **urisP)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_ReceiveFolder(TALLOC_CTX *parent_ctx,
					 struct openchangedb_context *self,
					 const char *recipient,
					 const char *MessageClass,
					 uint64_t *fid,
					 const char **ExplicitMessageClass)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_TransportFolder(struct openchangedb_context *self,
					   const char *recipient,
					   uint64_t *FolderId)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_folder_count(struct openchangedb_context *self,
					const char *username, uint64_t fid,
					uint32_t *RowCount)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS lookup_folder_property(struct openchangedb_context *self,
					      uint32_t proptag, uint64_t fid)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_new_changeNumber(struct openchangedb_context *self,
					    const char *username, uint64_t *cn)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_new_changeNumbers(struct openchangedb_context *self,
					     TALLOC_CTX *mem_ctx,
					     const char *username,
					     uint64_t max,
					     struct UI8Array_r **cns_p)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_next_changeNumber(struct openchangedb_context *self,
					     const char *username,
					     uint64_t *cn)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_folder_property(TALLOC_CTX *parent_ctx,
					   struct openchangedb_context *self,
					   const char *username,
					   uint32_t proptag, uint64_t fid,
					   void **data)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS set_folder_properties(struct openchangedb_context *self,
					     const char *username, uint64_t fid,
					     struct SRow *row)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_table_property(TALLOC_CTX *parent_ctx,
					  struct openchangedb_context *self,
					  const char *ldb_filter,
					  uint32_t proptag, uint32_t pos,
					  void **data)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_fid_by_name(struct openchangedb_context *self,
				       const char *username,
				       uint64_t parent_fid,
				       const char* foldername, uint64_t *fid)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_mid_by_subject(struct openchangedb_context *self,
					  const char *username,
					  uint64_t parent_fid,
					  const char *subject,
					  bool mailboxstore, uint64_t *mid)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS delete_folder(struct openchangedb_context *self,
				     const char *username, uint64_t fid)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS set_ReceiveFolder(struct openchangedb_context *self,
					 const char *recipient,
					 const char *MessageClass, uint64_t fid)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_fid_from_partial_uri(struct openchangedb_context *self,
						const char *partialURI, uint64_t *fid)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_users_from_partial_uri(TALLOC_CTX *parent_ctx,
						  struct openchangedb_context *self,
						  const char *partialURI,
						  uint32_t *count,
						  char ***MAPIStoreURI,
						  char ***users)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS create_mailbox(struct openchangedb_context *self,
				      const char *username,
				      const char *organization_name,
				      const char *groupo_name,
				      int systemIdx, uint64_t fid,
				      const char *display_name)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS create_folder(struct openchangedb_context *self,
				     const char *username,
				     uint64_t parentFolderID, uint64_t fid,
				     uint64_t changeNumber,
				     const char *MAPIStoreURI, int systemIdx)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_message_count(struct openchangedb_context *self,
					 const char *username, uint64_t fid,
					 uint32_t *RowCount, bool fai)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_system_idx(struct openchangedb_context *self,
				      const char *username, uint64_t fid,
				      int *system_idx_p)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS transaction_start(struct openchangedb_context *self)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS transaction_commit(struct openchangedb_context *self)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_new_public_folderID(struct openchangedb_context *self,
					       const char *username,
					       uint64_t *fid)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static bool is_public_folder_id(struct openchangedb_context *self, uint64_t fid)
{
	return false;
}

static enum MAPISTATUS get_indexing_url(struct openchangedb_context *self,
					const char *username,
					const char **indexing_url)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static bool set_locale(struct openchangedb_context *self, const char *username, uint32_t lcid)
{
	return true;
}

static const char **get_folders_names(TALLOC_CTX *mem_ctx, struct openchangedb_context *self, const char *locale, const char *type)
{
	return NULL;
}
// ^ openchangedb -------------------------------------------------------------

// v openchangedb table -------------------------------------------------------

static enum MAPISTATUS table_init(TALLOC_CTX *mem_ctx,
				  struct openchangedb_context *self,
				  const char *username,
				  uint8_t table_type, uint64_t folderID,
				  void **table_object)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS table_set_sort_order(struct openchangedb_context *self,
					    void *table_object,
					    struct SSortOrderSet *lpSortCriteria)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS table_set_restrictions(struct openchangedb_context *self,
					      void *table_object,
					      struct mapi_SRestriction *res)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS table_get_property(TALLOC_CTX *mem_ctx,
					  struct openchangedb_context *self,
					  void *table_object,
					  enum MAPITAGS proptag, uint32_t pos,
					  bool live_filtered, void **data)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

// ^ openchangedb table -------------------------------------------------------

// v openchangedb message -----------------------------------------------------

static enum MAPISTATUS message_create(TALLOC_CTX *mem_ctx,
				      struct openchangedb_context *self,
				      const char *username,
				      uint64_t messageID, uint64_t folderID,
				      bool fai, void **message_object)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS message_save(struct openchangedb_context *self,
				    void *_msg, uint8_t SaveFlags)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS message_open(TALLOC_CTX *mem_ctx,
				    struct openchangedb_context *self,
				    const char *username,
				    uint64_t messageID, uint64_t folderID,
				    void **message_object, void **msgp)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS message_get_property(TALLOC_CTX *mem_ctx,
					    struct openchangedb_context *self,
					    void *message_object,
					    uint32_t proptag, void **data)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS message_set_properties(TALLOC_CTX *mem_ctx,
					      struct openchangedb_context *self,
					      void *message_object,
					      struct SRow *row)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

// ^ openchangedb message -----------------------------------------------------

static enum MAPISTATUS mock_backend_init(TALLOC_CTX *mem_ctx,
					 struct openchangedb_context **ctx)
{
	struct openchangedb_context	*oc_ctx = talloc_zero(mem_ctx, struct openchangedb_context);

	OPENCHANGE_RETVAL_IF(oc_ctx == NULL, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);

	ZERO_STRUCT(functions_called);
//	oc_ctx->data = data;

	// Initialize struct with function pointers
	oc_ctx->backend_type = talloc_strdup(oc_ctx, "mocked_backend");
	OPENCHANGE_RETVAL_IF(oc_ctx->backend_type == NULL, MAPI_E_NOT_ENOUGH_RESOURCES, oc_ctx);

	oc_ctx->get_new_changeNumber = get_new_changeNumber;
	oc_ctx->get_new_changeNumbers = get_new_changeNumbers;
	oc_ctx->get_next_changeNumber = get_next_changeNumber;
	oc_ctx->get_SystemFolderID = get_SystemFolderID;
	oc_ctx->get_SpecialFolderID = get_SpecialFolderID;
	oc_ctx->get_PublicFolderID = get_PublicFolderID;
	oc_ctx->get_distinguishedName = get_distinguishedName;
	oc_ctx->get_MailboxGuid = get_MailboxGuid;
	oc_ctx->get_MailboxReplica = get_MailboxReplica;
	oc_ctx->get_PublicFolderReplica = get_PublicFolderReplica;
	oc_ctx->get_parent_fid = get_parent_fid;
	oc_ctx->get_MAPIStoreURIs = get_MAPIStoreURIs;
	oc_ctx->get_mapistoreURI = get_mapistoreURI;
	oc_ctx->set_mapistoreURI = set_mapistoreURI;
	oc_ctx->get_fid = get_fid;
	oc_ctx->get_ReceiveFolder = get_ReceiveFolder;
	oc_ctx->get_TransportFolder = get_TransportFolder;
	oc_ctx->lookup_folder_property = lookup_folder_property;
	oc_ctx->set_folder_properties = set_folder_properties;
	oc_ctx->get_folder_property = get_folder_property;
	oc_ctx->get_folder_count = get_folder_count;
	oc_ctx->get_message_count = get_message_count;
	oc_ctx->get_system_idx = get_system_idx;
	oc_ctx->get_table_property = get_table_property;
	oc_ctx->get_fid_by_name = get_fid_by_name;
	oc_ctx->get_mid_by_subject = get_mid_by_subject;
	oc_ctx->set_ReceiveFolder = set_ReceiveFolder;
	oc_ctx->create_mailbox = create_mailbox;
	oc_ctx->create_folder = create_folder;
	oc_ctx->delete_folder = delete_folder;
	oc_ctx->get_fid_from_partial_uri = get_fid_from_partial_uri;
	oc_ctx->get_users_from_partial_uri = get_users_from_partial_uri;

	oc_ctx->table_init = table_init;
	oc_ctx->table_set_sort_order = table_set_sort_order;
	oc_ctx->table_set_restrictions = table_set_restrictions;
	oc_ctx->table_get_property = table_get_property;

	oc_ctx->message_create = message_create;
	oc_ctx->message_save = message_save;
	oc_ctx->message_open = message_open;
	oc_ctx->message_get_property = message_get_property;
	oc_ctx->message_set_properties = message_set_properties;

	oc_ctx->transaction_start = transaction_start;
	oc_ctx->transaction_commit = transaction_commit;

	oc_ctx->get_new_public_folderID = get_new_public_folderID;
	oc_ctx->is_public_folder_id = is_public_folder_id;

	oc_ctx->get_indexing_url = get_indexing_url;
	oc_ctx->set_locale = set_locale;
	oc_ctx->get_folders_names = get_folders_names;

	*ctx = oc_ctx;

	return MAPI_E_SUCCESS;
}
// ^ Mocked backend ----------------------------------------------------------------

// v Suite definition ---------------------------------------------------------

static void ocdb_logger_setup(void)
{
	enum MAPISTATUS mapi_status;
	struct openchangedb_context *backend_ctx;

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	mapi_status = mock_backend_init(mem_ctx, &backend_ctx);
	if (mapi_status != MAPI_E_SUCCESS) {
		fprintf(stderr, "Failed to initialize Mocked backend %d\n", mapi_status);
		ck_abort();
	}
	mapi_status = openchangedb_logger_initialize(mem_ctx, 0, ">>> ", backend_ctx, &oc_ctx);
	if (mapi_status != MAPI_E_SUCCESS) {
		fprintf(stderr, "Failed to initialize Logger backend %d\n", mapi_status);
		ck_abort();
	}
}

static void ocdb_logger_teardown(void)
{
	talloc_free(mem_ctx);
}


static Suite *openchangedb_create_suite(SFun setup, SFun teardown)
{
	char *suite_name = talloc_asprintf(talloc_autofree_context(),
					   "Openchangedb Logger backend");
	Suite *s = suite_create(suite_name);

	TCase *tc = tcase_create("Openchangedb Logger interface");
	tcase_add_unchecked_fixture(tc, setup, teardown);

	tcase_add_test(tc, test_call_get_SpecialFolderID);
	tcase_add_test(tc, test_call_get_SystemFolderID);
	tcase_add_test(tc, test_call_get_PublicFolderID);
	tcase_add_test(tc, test_call_get_distinguishedName);
	tcase_add_test(tc, test_call_get_MailboxGuid);
	tcase_add_test(tc, test_call_get_MailboxReplica);
	tcase_add_test(tc, test_call_get_PublicFolderReplica);

	suite_add_tcase(s, tc);
	return s;
}

Suite *mapiproxy_openchangedb_logger_suite(void)
{
	return openchangedb_create_suite(ocdb_logger_setup, ocdb_logger_teardown);
}
