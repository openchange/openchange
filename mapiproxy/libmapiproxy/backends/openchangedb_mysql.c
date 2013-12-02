#include "openchangedb_mysql.h"

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapistore/mapistore.h"
#include "../libmapiproxy.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include <mysql/mysql.h>
#include <samba_util.h>
#include <inttypes.h>


#define SCHEMA_FILE "openchangedb_schema.sql"
#define PUBLIC_FOLDER "public"
#define SYSTEM_FOLDER "system"

#define TRANSPORT_FOLDER_NAME "Outbox"
#define TRANSPORT_FOLDER_LOCALE "en_US"

// We only used one connection
// FIXME assumption that every user will use the same mysql server. Change
// into list of connections indexed by username, like indexing backend.
static MYSQL *conn = NULL;

static enum MAPISTATUS _not_implemented(const char *caller) {
	DEBUG(0, ("Called not implemented function `%s` from mysql backend",
		  caller));
	return MAPI_E_NOT_IMPLEMENTED;
}
#define not_implemented() _not_implemented(__PRETTY_FUNCTION__)


static enum MAPISTATUS execute_query(MYSQL *conn, const char *sql)
{
	if (mysql_query(conn, sql) != 0) {
		DEBUG(0, ("Error on query `%s`: %s", sql, mysql_error(conn)));
		return MAPI_E_CALL_FAILED;
	}
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS select_without_fetch(MYSQL *conn, const char *sql,
					    MYSQL_RES **res)
{
	enum MAPISTATUS ret;

	ret = execute_query(conn, sql);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, NULL);

	*res = mysql_store_result(conn);
	if (*res == NULL) {
		DEBUG(0, ("Error getting results of `%s`: %s", sql,
			  mysql_error(conn)));
		return MAPI_E_CALL_FAILED;
	}

	if (mysql_num_rows(*res) == 0) {
		mysql_free_result(*res);
		return MAPI_E_NOT_FOUND;
	}

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS select_all_strings(TALLOC_CTX *mem_ctx, MYSQL *conn,
					  const char *sql,
					  struct StringArrayW_r **_results)
{
	MYSQL_RES *res;
	struct StringArrayW_r *results;
	uint32_t i;
	enum MAPISTATUS ret;

	ret = select_without_fetch(conn, sql, &res);
	if (ret == MAPI_E_NOT_FOUND) {
		results = talloc_zero(mem_ctx, struct StringArrayW_r);
		results->cValues = 0;
	} else if (ret == MAPI_E_SUCCESS) {
		results = talloc_zero(mem_ctx, struct StringArrayW_r);
		results->cValues = mysql_num_rows(res);
	} else {
		// Unexpected error on sql query
		return ret;
	}

	results->lppszW = talloc_zero_array(results, const char *,
					    results->cValues);

	for (i = 0; i < results->cValues; i++) {
		MYSQL_ROW row = mysql_fetch_row(res);
		if (row == NULL) {
			DEBUG(0, ("Error getting row %d of `%s`: %s", i, sql,
				  mysql_error(conn)));
			mysql_free_result(res);
			return MAPI_E_CALL_FAILED;
		}
		results->lppszW[i] = talloc_strdup(results, row[0]);
	}

	mysql_free_result(res);
	*_results = results;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS select_first_string(TALLOC_CTX *mem_ctx, MYSQL *conn,
					   const char *sql, const char **s)
{
	MYSQL_RES *res;
	enum MAPISTATUS ret;

	ret = select_without_fetch(conn, sql, &res);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, NULL);

	MYSQL_ROW row = mysql_fetch_row(res);
	if (row == NULL) {
		DEBUG(0, ("Error getting row of `%s`: %s", sql,
			  mysql_error(conn)));
		return MAPI_E_CALL_FAILED;
	}

	*s = talloc_strdup(mem_ctx, row[0]);
	mysql_free_result(res);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS select_first_uint(MYSQL *conn, const char *sql,
					 uint64_t *n)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "select_first_uint");
	const char *result;
	enum MAPISTATUS ret;

	ret = select_first_string(mem_ctx, conn, sql, &result);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	*n = strtoul(result, NULL, 10);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

// v openchangedb -------------------------------------------------------------

static enum MAPISTATUS get_SystemFolderID(struct openchangedb_context *self,
					  const char *recipient,
					  uint32_t SystemIdx,
					  uint64_t *FolderId)
{
	char *sql;
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_SystemFolderId");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;

	if (SystemIdx == 0x1) {
		// FIXME ou_id
		sql = talloc_asprintf(mem_ctx,
			"SELECT folder_id FROM mailboxes WHERE name = '%s'",
			recipient);
	} else {
		// FIXME ou_id
		sql = talloc_asprintf(mem_ctx,
			"SELECT f.folder_id FROM folders f JOIN mailboxes m ON "
			"f.mailbox_id = m.id AND m.name = '%s' "
			"WHERE f.SystemIdx = %"PRIu32" AND f.folder_class = '%s'",
			recipient, SystemIdx, SYSTEM_FOLDER);
	}

	ret = select_first_uint(conn, sql, FolderId);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_PublicFolderID(struct openchangedb_context *self,
					  uint32_t SystemIdx,
					  uint64_t *FolderId)
{
	char *sql;
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_PublicFolderID");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;

	// FIXME ou_id
	sql = talloc_asprintf(mem_ctx,
		"SELECT f.folder_id FROM folders f WHERE f.SystemIdx = %"PRIu32
		" AND f.folder_class = '%s'", SystemIdx, PUBLIC_FOLDER);

	ret = select_first_uint(conn, sql, FolderId);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_distinguishedName(TALLOC_CTX *parent_ctx,
					     struct openchangedb_context *self,
					     uint64_t fid,
					     char **distinguishedName)
{
	return not_implemented();
}

static enum MAPISTATUS get_MailboxGuid(struct openchangedb_context *self,
				       const char *recipient,
				       struct GUID *MailboxGUID)
{
	char *sql;
	const char *guid;
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_PublicFolderID");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;

	// FIXME ou_id
	sql = talloc_asprintf(mem_ctx,
		"SELECT MailboxGUID FROM mailboxes WHERE name = '%s'",
		recipient);

	ret = select_first_string(mem_ctx, conn, sql, &guid);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	GUID_from_string(guid, MailboxGUID);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_MailboxReplica(struct openchangedb_context *self,
					  const char *recipient,
					  uint16_t *ReplID,
					  struct GUID *ReplGUID)
{
	char *sql;
	const char *guid;
	uint64_t n;
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_PublicFolderID");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;

	if (ReplID) {
		// FIXME ou_id
		sql = talloc_asprintf(mem_ctx,
			"SELECT ReplicaID FROM mailboxes WHERE name = '%s'",
			recipient);

		ret = select_first_uint(conn, sql, &n);
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

		*ReplID = (uint16_t) n;
	}

	if (ReplGUID) {
		// FIXME ou_id
		sql = talloc_asprintf(mem_ctx,
			"SELECT ReplicaGUID FROM mailboxes WHERE name = '%s'",
			recipient);

		ret = select_first_string(mem_ctx, conn, sql, &guid);
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

		GUID_from_string(guid, ReplGUID);
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_PublicFolderReplica(struct openchangedb_context *self,
					       uint16_t *ReplID,
					       struct GUID *ReplGUID)
{
	char *sql;
	const char *guid;
	uint64_t n;
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_PublicFolderReplica");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;

	if (ReplID) {
		// FIXME ou_id
		sql = talloc_asprintf(mem_ctx,
			"SELECT ReplicaID FROM public_folders");

		ret = select_first_uint(conn, sql, &n);
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

		*ReplID = (uint16_t) n;
	}

	if (ReplGUID) {
		// FIXME ou_id
		sql = talloc_asprintf(mem_ctx,
			"SELECT StoreGUID FROM public_folders");

		ret = select_first_string(mem_ctx, conn, sql, &guid);
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

		GUID_from_string(guid, ReplGUID);
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_mapistoreURI(TALLOC_CTX *parent_ctx,
				        struct openchangedb_context *self,
				        const char *username, uint64_t fid,
				        char **mapistoreURL, bool mailboxstore)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_mapistoreURI");
	enum MAPISTATUS ret;
	char *sql;

	if (!mailboxstore) { // FIXME is it possible?
		return _not_implemented("get_mapistoreURI with mailboxstore=false");
	}

	sql = talloc_asprintf(mem_ctx,
		"SELECT MAPIStoreURI FROM folders f "
		"JOIN mailboxes m ON m.id = f.mailbox_id AND m.name = '%s' "
		"WHERE f.folder_id = %"PRIu64, username, fid);

	ret = select_first_string(parent_ctx, conn, sql,
				  (const char **)mapistoreURL);
	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS set_mapistoreURI(struct openchangedb_context *self,
					const char *username, uint64_t fid,
					const char *mapistoreURL)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "set_mapistoreURI");
	enum MAPISTATUS ret;
	char *sql;

	sql = talloc_asprintf(mem_ctx,
		"UPDATE folders f "
		"JOIN mailboxes m ON m.id = f.mailbox_id AND m.name = '%s' "
		"SET f.MAPIStoreURI = '%s' "
		"WHERE f.folder_id = %"PRIu64, username, mapistoreURL, fid);
	ret = execute_query(conn, sql);
	if (mysql_affected_rows(conn) == 0) {
		ret = MAPI_E_NOT_FOUND;
	}
	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS get_parent_fid(struct openchangedb_context *self,
				      uint64_t fid, uint64_t *parent_fidp,
				      bool mailboxstore)
{//TODO NEEDS USER
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_fid(struct openchangedb_context *self,
			       const char *mapistoreURL, uint64_t *fidp)
{//TODO NEEDS USER
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_MAPIStoreURIs(struct openchangedb_context *self,
					 const char *username,
					 TALLOC_CTX *mem_ctx,
					 struct StringArrayW_r **urisP)
{
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;
	char *sql;

	// TODO ou_id
	sql = talloc_asprintf(mem_ctx,
		"SELECT MAPIStoreURI FROM folders f JOIN mailboxes m "
		"ON f.mailbox_id = m.id AND m.name = '%s' "
		"WHERE MAPIStoreURI IS NOT NULL", username);

	ret = select_all_strings(mem_ctx, conn, sql, urisP);

	talloc_free(sql);
	return ret;
}

static enum MAPISTATUS get_ReceiveFolder(TALLOC_CTX *mem_ctx,
					 struct openchangedb_context *self,
					 const char *recipient,
					 const char *MessageClass,
					 uint64_t *fid,
					 const char **ExplicitMessageClass)
{
	TALLOC_CTX *local_mem_ctx = talloc_named(NULL, 0, "get_ReceiveFolder");
	MYSQL *conn = self->data;
	char *sql, *explicit = NULL;
	enum MAPISTATUS ret;
	MYSQL_RES *res;
	my_ulonglong nrows, i;
	size_t length;

	// Check PidTagMessageClass from mailbox and folders
	// TODO ou_id
	sql = talloc_asprintf(local_mem_ctx,
		"SELECT fp.value, f.folder_id FROM folders f "
		"JOIN mailboxes m ON m.id = f.mailbox_id AND m.name = '%s' "
		"JOIN folders_properties fp ON fp.folder_id = f.id AND"
		"     fp.name = 'PidTagMessageClass' "
		"UNION "
		"SELECT mp.value, m2.folder_id FROM mailboxes m2 "
		"JOIN mailboxes_properties mp ON mp.mailbox_id = m2.id AND"
		"     mp.name = 'PidTagMessageClass' "
		"WHERE m2.name = '%s'", recipient, recipient);

	ret = select_without_fetch(conn, sql, &res);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, local_mem_ctx);

	nrows = mysql_num_rows(res);
	for (i = 0, length = 0; i < nrows; i++) {
		MYSQL_ROW row = mysql_fetch_row(res);
		const char *message_class;
		size_t message_class_length;
		if (row == NULL) {
			DEBUG(0, ("Error getting row %d of `%s`: %s", (int) i,
				  sql, mysql_error(conn)));
			mysql_free_result(res);
			return MAPI_E_CALL_FAILED;
		}
		message_class = row[0];
		message_class_length = strlen(message_class);
		if (!strncasecmp(MessageClass, message_class, message_class_length) &&
		    message_class_length > length) {
			if (explicit && strcmp(explicit, "")) {
				talloc_free(explicit);
			}
			if (MessageClass && !strcmp(MessageClass, "All")) {
				explicit = "";
			} else {
				explicit = talloc_strdup(mem_ctx, message_class);
			}
			*ExplicitMessageClass = explicit;
			*fid = strtoul(row[1], NULL, 10);
			length = message_class_length;
		}
	}

	mysql_free_result(res);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_TransportFolder(struct openchangedb_context *self,
					   const char *recipient,
					   uint64_t *FolderId)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_TransportFolder");
	MYSQL *conn = self->data;
	char *sql;
	enum MAPISTATUS ret;

	// FIXME ou_id
	sql = talloc_asprintf(mem_ctx,
		"SELECT f.folder_id FROM folders f "
		"JOIN mailboxes m ON f.mailbox_id = m.id AND m.name = '%s' "
		"JOIN folders_names n ON n.folder_id = f.id"
		" AND n.locale = '%s' AND n.display_name = '%s'",
		recipient, TRANSPORT_FOLDER_LOCALE, TRANSPORT_FOLDER_NAME);

	ret = select_first_uint(conn, sql, FolderId);
	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS get_folder_count(struct openchangedb_context *self,
					uint64_t fid, uint32_t *RowCount)
{//TODO NEEDS USER
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS lookup_folder_property(struct openchangedb_context *self,
					      uint32_t proptag, uint64_t fid)
{
	return not_implemented();
}

/**
 * Get the current change number field
 * TODO ou_id
 */
static enum MAPISTATUS get_server_change_number(MYSQL *conn, uint64_t *change_number)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_server_change_number");
	char *sql;
	enum MAPISTATUS ret;

	sql = talloc_asprintf(mem_ctx,
		"SELECT change_number FROM servers s "
		"JOIN company c ON c.id = s.company_id "
		"JOIN organizational_units ou ON ou.company_id = c.id "
		//FIXME restrictions
		);
	ret = select_first_uint(conn, sql, change_number);
	talloc_free(mem_ctx);
	return ret;
}

/**
 * Set the current change number field
 * TODO ou_id
 */
static enum MAPISTATUS set_server_change_number(MYSQL *conn, uint64_t change_number)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "set_server_change_number");
	char *sql;
	enum MAPISTATUS ret;

	sql = talloc_asprintf(mem_ctx,
		"UPDATE servers s "
		"JOIN company c ON c.id = s.company_id "
		"JOIN organizational_units ou ON ou.company_id = c.id "
		"SET s.change_number=%"PRIu64""
		//FIXME ou_id
		, change_number);
	ret = execute_query(conn, sql);
	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS get_new_changeNumber(struct openchangedb_context *self,
					    uint64_t *cn)
{
	enum MAPISTATUS ret;
	MYSQL *conn = self->data;

	ret = get_server_change_number(conn, cn); // TODO ou_id
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, NULL);

	ret = set_server_change_number(conn, (*cn) + 1); // TODO ou_id
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, NULL);

	// Transform the number the way exchange protocol likes it
	*cn = (exchange_globcnt(*cn) << 16) | 0x0001;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_new_changeNumbers(struct openchangedb_context *self,
					     TALLOC_CTX *mem_ctx, uint64_t max,
					     struct UI8Array_r **cns_p)
{
	return not_implemented();
}

static enum MAPISTATUS get_next_changeNumber(struct openchangedb_context *self,
					     uint64_t *cn)
{
	enum MAPISTATUS ret;
	MYSQL *conn = self->data;

	ret = get_server_change_number(conn, cn);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, NULL);

	// Transform the number the way exchange protocol likes it
	*cn = (exchange_globcnt(*cn) << 16) | 0x0001;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_folder_property(TALLOC_CTX *parent_ctx,
					   struct openchangedb_context *self,
					   uint32_t proptag, uint64_t fid,
					   void **data)
{//TODO NEEDS USER
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS set_folder_properties(struct openchangedb_context *self,
					     uint64_t fid, struct SRow *row)
{//TODO NEEDS USER
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_table_property(TALLOC_CTX *parent_ctx,
					  struct openchangedb_context *self,
					  const char *ldb_filter,
					  uint32_t proptag, uint32_t pos,
					  void **data)
{
	return not_implemented();
}

static enum MAPISTATUS get_fid_by_name(struct openchangedb_context *self,
				       uint64_t parent_fid,
				       const char* foldername, uint64_t *fid)
{//TODO NEEDS USER
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_mid_by_subject(struct openchangedb_context *self,
					  uint64_t parent_fid,
					  const char *subject,
					  bool mailboxstore, uint64_t *mid)
{//TODO NEEDS USER
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS delete_folder(struct openchangedb_context *self,
				     uint64_t fid)
{//TODO NEEDS USER
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS set_ReceiveFolder(struct openchangedb_context *self,
					 const char *recipient,
					 const char *MessageClass, uint64_t fid)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "set_ReceiveFolder");
	char *sql;
	enum MAPISTATUS ret;

	// Delete current receive folder for that message class if exists
	sql = talloc_asprintf(mem_ctx,
		"DELETE fp FROM folders_properties fp "
		"JOIN folders f ON f.id = fp.folder_id "
		"JOIN mailboxes m ON m.id = f.mailbox_id AND m.name = '%s' "
		"WHERE fp.name = 'PidTagMessageClass' AND fp.value = '%s'",
		recipient, MessageClass); //TODO ou_id
	ret = execute_query(conn, sql);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	// Create PidTagMessageClass folder property for the fid specified
	sql = talloc_asprintf(mem_ctx,
		"INSERT INTO folders_properties VALUES ("
		" ("
		"  SELECT f.id FROM folders f "
		"  JOIN mailboxes m ON m.id = f.mailbox_id AND m.name = '%s' "
		"  WHERE f.folder_id = %"PRIu64
		" ), 'PidTagMessageClass', '%s')",
		recipient, fid, MessageClass); //TODO ou_id
	ret = execute_query(conn, sql);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_fid_from_partial_uri(struct openchangedb_context *self,
						const char *partialURI,
						uint64_t *fid)
{
	return not_implemented();
}

static enum MAPISTATUS get_users_from_partial_uri(TALLOC_CTX *parent_ctx,
						  struct openchangedb_context *self,
						  const char *partialURI,
						  uint32_t *count,
						  char ***MAPIStoreURI,
						  char ***users)
{
	return not_implemented();
}

static enum MAPISTATUS create_mailbox(struct openchangedb_context *self,
				      const char *username, int systemIdx,
				      uint64_t fid)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS create_folder(struct openchangedb_context *self,
				     uint64_t parentFolderID, uint64_t fid,
				     uint64_t changeNumber,
				     const char *MAPIStoreURI, int systemIdx)
{//TODO NEEDS USER
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_message_count(struct openchangedb_context *self,
					 uint64_t fid, uint32_t *RowCount,
					 bool fai)
{//TODO NEEDS USER
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS get_system_idx(struct openchangedb_context *self,
				      uint64_t fid, int *system_idx_p)
{//TODO NEEDS USER
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS transaction_start(struct openchangedb_context *self)
{
	MYSQL *conn = self->data;
	int res = mysql_query(conn, "START TRANSACTION");
	OPENCHANGE_RETVAL_IF(res, MAPI_E_CALL_FAILED, NULL);
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS transaction_rollback(struct openchangedb_context *self)
{
	MYSQL *conn = self->data;
	int res = mysql_query(conn, "ROLLBACK");
	OPENCHANGE_RETVAL_IF(res, MAPI_E_CALL_FAILED, NULL);
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS transaction_commit(struct openchangedb_context *self)
{
	MYSQL *conn = self->data;
	int res = mysql_query(conn, "COMMIT");
	OPENCHANGE_RETVAL_IF(res, MAPI_E_CALL_FAILED, NULL);
	return MAPI_E_SUCCESS;
}

// ^ openchangedb -------------------------------------------------------------

// v openchangedb table -------------------------------------------------------
// TODO types
struct openchangedb_table {
	uint64_t			folderID;
	uint8_t				table_type;
	struct SSortOrderSet		*lpSortCriteria;
	struct mapi_SRestriction	*restrictions;
	struct ldb_result		*res;
};


static enum MAPISTATUS table_init(TALLOC_CTX *mem_ctx,
				  struct openchangedb_context *self,
				  uint8_t table_type, uint64_t folderID,
				  void **table_object)
{//TODO NEEDS USER
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

// TODO types
enum openchangedb_message_type {
	OPENCHANGEDB_MESSAGE_SYSTEM	= 0x1,
	OPENCHANGEDB_MESSAGE_FAI	= 0x2
};

struct openchangedb_message_properties {
	char		**names;
	char 		**values;
	uint64_t	size;
};

struct openchangedb_message {
	uint64_t				id;
	uint64_t				message_id;
	enum openchangedb_message_type		message_type;
	uint64_t				folder_id;
	uint64_t				mailbox_id;
	char					*normalized_subject;
	struct openchangedb_message_properties	properties;
};

static enum MAPISTATUS message_create(TALLOC_CTX *mem_ctx,
				      struct openchangedb_context *self,
				      uint64_t message_id, uint64_t folder_id,
				      bool fai, void **message_object)
{ // TODO
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS message_save(struct openchangedb_context *self,
				    void *_msg, uint8_t save_flags)
{ // TODO
	return MAPI_E_NOT_IMPLEMENTED;
}

static enum MAPISTATUS message_open(TALLOC_CTX *mem_ctx,
				    struct openchangedb_context *self,
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
	// PidTagParentFolderId -> folder_id
	// PidTagMessageId -> message_id
	// PidTagNormalizedSubject -> normalized_subject
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

static const char *openchangedb_data_dir(void)
{
	return OPENCHANGEDB_DATA_DIR; // defined on compilation time
}

static bool parse_connection_string(TALLOC_CTX *mem_ctx,
				   const char *connection_string,
				   char **host, char **user, char **passwd,
				   char **db)
{
	// connection_string has format mysql://user[:pass]@host/database
	int prefix_size = strlen("mysql://");
	const char *s = connection_string + prefix_size;
	if (!connection_string || strlen(connection_string) < prefix_size ||
	    !strstr(connection_string, "mysql://") || !strchr(s, '@') ||
	    !strchr(s, '/')) {
		// Invalid format
		return false;
	}
	if (strchr(s, ':') == NULL || strchr(s, ':') > strchr(s, '@')) {
		// No password
		int user_size = strchr(s, '@') - s;
		*user = talloc_zero_array(mem_ctx, char, user_size + 1);
		strncpy(*user, s, user_size);
		(*user)[user_size] = '\0';
		*passwd = talloc_zero_array(mem_ctx, char, 1);
		(*passwd)[0] = '\0';
	} else {
		// User
		int user_size = strchr(s, ':') - s;
		*user = talloc_zero_array(mem_ctx, char, user_size);
		strncpy(*user, s, user_size);
		(*user)[user_size] = '\0';
		// Password
		int passwd_size = strchr(s, '@') - strchr(s, ':') - 1;
		*passwd = talloc_zero_array(mem_ctx, char, passwd_size + 1);
		strncpy(*passwd, strchr(s, ':') + 1, passwd_size);
		(*passwd)[passwd_size] = '\0';
	}
	// Host
	int host_size = strchr(s, '/') - strchr(s, '@') - 1;
	*host = talloc_zero_array(mem_ctx, char, host_size + 1);
	strncpy(*host, strchr(s, '@') + 1, host_size);
	(*host)[host_size] = '\0';
	// Database name
	int db_size = strlen(strchr(s, '/') + 1);
	*db = talloc_zero_array(mem_ctx, char, db_size + 1);
	strncpy(*db, strchr(s, '/') + 1, db_size);
	(*db)[db_size] = '\0';

	return true;
}

static bool is_schema_created(MYSQL *conn)
{
	MYSQL_RES *res;
	bool created;

	res = mysql_list_tables(conn, "folders");
	if (res == NULL) return false;
	created = mysql_num_rows(res) == 1;
	mysql_free_result(res);

	return created;
}

static bool create_schema(MYSQL *conn)
{
	TALLOC_CTX *mem_ctx;
	FILE *f;
	int sql_size, bytes_read;
	char *schema, *schema_file, *query;
	bool ret, queries_to_execute;

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	schema_file = talloc_asprintf(mem_ctx, "%s/"SCHEMA_FILE,
				      openchangedb_data_dir());
	f = fopen(schema_file, "r");
	if (!f) {
		DEBUG(0, ("schema file %s not found", schema_file));
		ret = false;
		goto end;
	}
	fseek(f, 0, SEEK_END);
	sql_size = ftell(f);
	rewind(f);
	schema = talloc_zero_array(mem_ctx, char, sql_size + 1);
	bytes_read = fread(schema, sizeof(char), sql_size, f);
	if (bytes_read != sql_size) {
		DEBUG(0, ("error reading schema file %s", schema_file));
		ret = false;
		goto end;
	}
	// schema is a series of create table/index queries separated by ';'
	query = strtok (schema, ";");
	queries_to_execute = query != NULL;
	while (queries_to_execute) {
		ret = mysql_query(conn, query) ? false : true;
		if (!ret) break;
		query = strtok(NULL, ";");
		queries_to_execute = ret && query && strlen(query) > 10;
	}
end:
	talloc_free(mem_ctx);
	if (f) fclose(f);

	return ret;
}

static MYSQL *create_connection(const char *connection_string)
{
	TALLOC_CTX *mem_ctx;
	my_bool reconnect;
	char *host, *user, *passwd, *db, *sql;
	bool parsed;

	if (conn != NULL) return conn;

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	conn = mysql_init(NULL);
	reconnect = true;
	mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);
	parsed = parse_connection_string(mem_ctx, connection_string,
					 &host, &user, &passwd, &db);
	if (!parsed) {
		DEBUG(0, ("Wrong connection string to mysql %s", connection_string));
		conn = NULL;
		goto end;
	}
	// First try to connect to the database, if it fails try to create it
	if (mysql_real_connect(conn, host, user, passwd, db, 0, NULL, 0)) {
		goto end;
	}

	// Try to create database
	if (!mysql_real_connect(conn, host, user, passwd, NULL, 0, NULL, 0)) {
		// Nop
		DEBUG(0, ("Can't connect to mysql using %s", connection_string));
		conn = NULL;
	} else {
		// Connect it!, let's try to create database
		sql = talloc_asprintf(mem_ctx, "CREATE DATABASE %s", db);
		if (mysql_query(conn, sql) != 0 || mysql_select_db(conn, db) != 0) {
			DEBUG(0, ("Can't connect to mysql using %s",
				  connection_string));
			conn = NULL;
		}
	}
end:
	talloc_free(mem_ctx);
	return conn;

}

_PUBLIC_ enum MAPISTATUS openchangedb_mysql_initialize(TALLOC_CTX *mem_ctx,
					 	       const char *connection_string,
						       struct openchangedb_context **ctx)
{
	struct openchangedb_context *oc_ctx;

	oc_ctx = talloc_zero(mem_ctx, struct openchangedb_context);
	// Initialize context with function pointers
	oc_ctx->backend_type = talloc_strdup(oc_ctx, "mysql");

	oc_ctx->get_new_changeNumber = get_new_changeNumber;
	oc_ctx->get_new_changeNumbers = get_new_changeNumbers;
	oc_ctx->get_next_changeNumber = get_next_changeNumber;
	oc_ctx->get_SystemFolderID = get_SystemFolderID;
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

	// Connect to mysql
	oc_ctx->data = create_connection(connection_string);
	OPENCHANGE_RETVAL_IF(!oc_ctx->data, MAPI_E_NOT_INITIALIZED, oc_ctx);
	if (!is_schema_created(oc_ctx->data)) {
		DEBUG(0, ("Creating schema for openchangedb on mysql %s",
			  connection_string));
		bool schema_created = create_schema(oc_ctx->data);
		OPENCHANGE_RETVAL_IF(!schema_created, MAPI_E_NOT_INITIALIZED, oc_ctx);
	}

	*ctx = oc_ctx;

	return MAPI_E_SUCCESS;
}
