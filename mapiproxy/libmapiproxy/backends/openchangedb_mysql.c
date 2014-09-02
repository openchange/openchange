/*
   MAPI Proxy - OpenchangeDB backend MySQL implementation

   OpenChange Project

   Copyright (C) Jesús García Sáez 2014

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

#include "openchangedb_mysql.h"

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapistore/mapistore.h"
#include "../libmapiproxy.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include <mysql/mysql.h>
#include <samba_util.h>
#include <inttypes.h>
#include <time.h>
#include "../../util/mysql.h"

#define SCHEMA_FILE "openchangedb_schema.sql"
#define PUBLIC_FOLDER "public"
#define SYSTEM_FOLDER "system"

#define THRESHOLD_SLOW_QUERIES 0.25

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

// Status map from MYSQL methods
static enum MAPISTATUS status(enum MYSQLRESULT ret) {
	switch (ret) {
	case MYSQL_SUCCESS : return MAPI_E_SUCCESS;
	case MYSQL_NOT_FOUND : return MAPI_E_NOT_FOUND;
	default: return MAPI_E_CALL_FAILED;
	}
};

// v openchangedb -------------------------------------------------------------
static enum MAPISTATUS get_SpecialFolderID(struct openchangedb_context *self,
					   const char *recipient,
					   uint32_t system_idx,
					   int64_t *folder_id)
{
	char *sql;
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_SpecialFolderId");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;

	// FIXME ou_id
	sql = talloc_asprintf(mem_ctx,
		"SELECT f.folder_id FROM folders f "
		"JOIN mailboxes m ON f.mailbox_id = m.id "
		"  AND m.name = '%s' "
		"WHERE f.SystemIdx = %"PRIu32
		"  AND f.folder_class = '"SYSTEM_FOLDER"' "
		"  AND f.parent_folder_id IS NOT NULL",
		_sql(mem_ctx, recipient), system_idx);

	ret = status(select_first_int64(conn, sql, folder_id));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_SystemFolderID(struct openchangedb_context *self,
					  const char *recipient,
					  uint32_t SystemIdx,
					  int64_t *FolderId)
{
	char *sql;
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_SystemFolderId");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;

	if (SystemIdx == 0x1) {
		// FIXME ou_id
		sql = talloc_asprintf(mem_ctx,
			"SELECT folder_id FROM mailboxes WHERE name = '%s'",
			_sql(mem_ctx, recipient));
	} else {
		// FIXME ou_id
		sql = talloc_asprintf(mem_ctx,
			"SELECT f.folder_id FROM folders f "
			"JOIN mailboxes m ON f.mailbox_id = m.id "
			"  AND m.name = '%s' "
			"WHERE f.SystemIdx = %"PRIu32
			"  AND f.folder_class = '%s' "
			"ORDER BY parent_folder_id",
			_sql(mem_ctx, recipient), SystemIdx, SYSTEM_FOLDER);
	}

	ret = status(select_first_int64(conn, sql, FolderId));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_PublicFolderID(struct openchangedb_context *self,
					  uint32_t SystemIdx,
					  int64_t *FolderId)
{
	char *sql;
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_PublicFolderID");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;

	// FIXME ou_id
	sql = talloc_asprintf(mem_ctx,
		"SELECT f.folder_id FROM folders f WHERE f.SystemIdx = %"PRIu32
		" AND f.folder_class = '%s'", SystemIdx, PUBLIC_FOLDER);

	ret = status(select_first_int64(conn, sql, FolderId));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_distinguishedName(TALLOC_CTX *parent_ctx,
					     struct openchangedb_context *self,
					     int64_t fid,
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
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_MailboxGuid");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;

	// FIXME ou_id
	sql = talloc_asprintf(mem_ctx,
		"SELECT MailboxGUID FROM mailboxes WHERE name = '%s'",
		_sql(mem_ctx, recipient));

	ret = status(select_first_string(mem_ctx, conn, sql, &guid));
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
	int64_t n;
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_MailboxReplica");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;

	if (ReplID) {
		// FIXME ou_id
		sql = talloc_asprintf(mem_ctx,
			"SELECT ReplicaID FROM mailboxes WHERE name = '%s'",
			_sql(mem_ctx, recipient));

		ret = status(select_first_int64(conn, sql, &n));
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

		*ReplID = (uint16_t) n;
	}

	if (ReplGUID) {
		// FIXME ou_id
		sql = talloc_asprintf(mem_ctx,
			"SELECT ReplicaGUID FROM mailboxes WHERE name = '%s'",
			_sql(mem_ctx, recipient));

		ret = status(select_first_string(mem_ctx, conn, sql, &guid));
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
	int64_t n;
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_PublicFolderReplica");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;

	if (ReplID) {
		// FIXME ou_id
		sql = talloc_asprintf(mem_ctx,
			"SELECT ReplicaID FROM public_folders");

		ret = status(select_first_int64(conn, sql, &n));
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

		*ReplID = (uint16_t) n;
	}

	if (ReplGUID) {
		// FIXME ou_id
		sql = talloc_asprintf(mem_ctx,
			"SELECT StoreGUID FROM public_folders");

		ret = status(select_first_string(mem_ctx, conn, sql, &guid));
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

		GUID_from_string(guid, ReplGUID);
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_mapistoreURI(TALLOC_CTX *parent_ctx,
				        struct openchangedb_context *self,
				        const char *username, int64_t fid,
				        char **mapistoreURL, bool mailboxstore)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_mapistoreURI");
	enum MAPISTATUS ret;
	char *sql;
	MYSQL *conn = self->data;

	if (!mailboxstore) {
		// TODO is it possible?
		return _not_implemented("get_mapistoreURI with mailboxstore=false");
	}

	sql = talloc_asprintf(mem_ctx,
		"SELECT MAPIStoreURI FROM folders f "
		"JOIN mailboxes m ON m.id = f.mailbox_id AND m.name = '%s' "
		"WHERE f.folder_id = %"PRIu64, _sql(mem_ctx, username), fid);

	ret = status(select_first_string(parent_ctx, conn, sql,
					 (const char **)mapistoreURL));
	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS set_mapistoreURI(struct openchangedb_context *self,
					const char *username, int64_t fid,
					const char *mapistoreURL)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "set_mapistoreURI");
	enum MAPISTATUS ret;
	char *sql;
	MYSQL *conn = self->data;
	// FIXME ou_id
	sql = talloc_asprintf(mem_ctx,
		"UPDATE folders f "
		"JOIN mailboxes m ON m.id = f.mailbox_id AND m.name = '%s' "
		"SET f.MAPIStoreURI = '%s' "
		"WHERE f.folder_id = %"PRIu64, _sql(mem_ctx, username),
		mapistoreURL, fid);
	ret = status(execute_query(conn, sql));
	if (mysql_affected_rows(conn) == 0) {
		ret = MAPI_E_NOT_FOUND;
	}
	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS get_parent_fid(struct openchangedb_context *self,
				      const char *username, int64_t fid,
				      int64_t *parent_fidp, bool mailboxstore)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_parent_fid");
	enum MAPISTATUS ret;
	char *sql;
	MYSQL *conn = self->data;

	if (mailboxstore) {
		sql = talloc_asprintf(mem_ctx, // FIXME ou_id
			"SELECT f1.folder_id FROM folders f1 "
			"JOIN folders f2 ON f1.id = f2.parent_folder_id"
			"  AND f2.folder_id = %"PRIu64" "
			"JOIN mailboxes m ON m.id = f2.mailbox_id"
			"  AND m.name = '%s' "
			"UNION "
			"SELECT m.folder_id FROM mailboxes m "
			"JOIN folders f1 ON f1.mailbox_id = m.id"
			"  AND f1.parent_folder_id IS NULL "
			"  AND f1.folder_id = %"PRIu64" "
			"WHERE m.name = '%s'",
			fid, _sql(mem_ctx, username),
			fid, _sql(mem_ctx, username));
	} else {
		sql = talloc_asprintf(mem_ctx, // FIXME ou_id
			"SELECT f1.folder_id FROM folders f1 "
			"JOIN folders f2 ON f1.id = f2.parent_folder_id"
			"  AND f2.folder_id = %"PRIu64" "
			"WHERE f1.folder_class = '%s'", fid, PUBLIC_FOLDER);
	}

	ret = status(select_first_int64(conn, sql, parent_fidp));
	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS get_fid(struct openchangedb_context *self,
			       const char *mapistore_uri, int64_t *fidp)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_fid");
	enum MAPISTATUS ret;
	MYSQL *conn = self->data;
	char *sql, *mapistore_uri_2;

	mapistore_uri_2 = talloc_strdup(mem_ctx, mapistore_uri);
	if (mapistore_uri_2[strlen(mapistore_uri_2)-1] == '/') {
		mapistore_uri_2[strlen(mapistore_uri_2)-1] = '\0';
	} else {
		mapistore_uri_2 = talloc_asprintf(mem_ctx, "%s/",
						  mapistore_uri_2);
	}

	sql = talloc_asprintf(mem_ctx, // FIXME ou_id
		"SELECT folder_id FROM folders "
		"WHERE MAPIStoreURI = '%s' OR MAPIStoreURI = '%s'",
		_sql(mem_ctx, mapistore_uri), _sql(mem_ctx, mapistore_uri_2));

	ret = status(select_first_int64(conn, sql, fidp));
	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS get_MAPIStoreURIs(struct openchangedb_context *self,
					 const char *username,
					 TALLOC_CTX *mem_ctx,
					 struct StringArrayW_r **urisP)
{
	TALLOC_CTX *local_mem_ctx = talloc_named(NULL, 0, "get_MAPIStoreURIs");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;
	char *sql;

	// FIXME ou_id
	sql = talloc_asprintf(local_mem_ctx,
		"SELECT MAPIStoreURI FROM folders f JOIN mailboxes m "
		"ON f.mailbox_id = m.id AND m.name = '%s' "
		"WHERE MAPIStoreURI IS NOT NULL", _sql(local_mem_ctx, username));

	ret = status(select_all_strings(mem_ctx, conn, sql, urisP));

	talloc_free(local_mem_ctx);
	return ret;
}

static enum MAPISTATUS get_ReceiveFolder(TALLOC_CTX *_mem_ctx,
					 struct openchangedb_context *self,
					 const char *recipient,
					 const char *MessageClass,
					 int64_t *fid,
					 const char **ExplicitMessageClass)
{
	TALLOC_CTX	*mem_ctx;
	enum MAPISTATUS retval = MAPI_E_SUCCESS;
	MYSQL		*conn = NULL;
	char		*sql = NULL;
	char		*explicit = NULL;
	MYSQL_RES	*res = NULL;
	my_ulonglong	nrows, i;
	size_t		length;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!self, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!self->data, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!fid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ExplicitMessageClass, MAPI_E_INVALID_PARAMETER, NULL);

	conn = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_ReceiveFolder");
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	/* Check PidTagMessageClass from mailbox and folders */
	/* TODO: ou_id */
	sql = talloc_asprintf(mem_ctx,
		"SELECT fp.value, f.folder_id FROM folders f "
		"JOIN mailboxes m ON m.id = f.mailbox_id AND m.name = '%s' "
		"JOIN folders_properties fp ON fp.folder_id = f.id AND"
		"     fp.name = 'PidTagMessageClass' "
		"UNION "
		"SELECT mp.value, m2.folder_id FROM mailboxes m2 "
		"JOIN mailboxes_properties mp ON mp.mailbox_id = m2.id AND"
		"     mp.name = 'PidTagMessageClass' "
		"WHERE m2.name = '%s'", _sql(_mem_ctx, recipient),
		_sql(_mem_ctx, recipient));
	OPENCHANGE_RETVAL_IF(!sql, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);

	retval = status(select_without_fetch(conn, sql, &res));
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, mem_ctx);

	nrows = mysql_num_rows(res);
	for (i = 0, length = 0; i < nrows; i++) {
		MYSQL_ROW	row;
		const char	*message_class = NULL;
		size_t		message_class_length = 0;

		row = mysql_fetch_row(res);
		if (row == NULL || row[0] == NULL) {
			DEBUG(0, ("[ERR][%s]: Error getting row %d of `%s`: %s", __location__,
				  (int) i, sql, mysql_error(conn)));
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
				explicit = talloc_strdup(_mem_ctx, message_class);
				OPENCHANGE_RETVAL_IF(!explicit, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
			}
			*ExplicitMessageClass = explicit;

			retval = MAPI_E_CALL_FAILED;
			if (convert_string_to_ll(row[1], fid)) {
				retval = MAPI_E_SUCCESS;
			}

			length = message_class_length;
		}
	}

	talloc_free(mem_ctx);
	mysql_free_result(res);

	return retval;
}

static enum MAPISTATUS get_ReceiveFolderTable(TALLOC_CTX *mem_ctx,
					      struct openchangedb_context *oc_ctx,
					      const char *recipient,
					      uint32_t *cValues,
					      struct ReceiveFolder **entries)
{
	TALLOC_CTX		*tmp_ctx;
	enum MAPISTATUS		ret;
	char			*sql;
	MYSQL_RES		*res;
	struct ReceiveFolder	*rcvfolders;
	my_ulonglong		nrows, i;
	MYSQL_ROW		row;
	int			num_fields;
	NTTIME			nt;

	tmp_ctx = talloc_named(NULL, 0, "get_ReceiveFolderTable");
	OPENCHANGE_RETVAL_IF(!tmp_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	sql = talloc_asprintf(tmp_ctx,
			      "SELECT f.folder_id, fp.value, fp2.value FROM folders f "
			      "JOIN mailboxes m ON m.id = f.mailbox_id AND m.name = '%s' "
			      "JOIN folders_properties fp ON fp.folder_id = f.id AND "
			      "fp.name = 'PidTagMessageClass' "
			      "JOIN folders_properties fp2 on fp2.folder_id = f.id AND "
			      "fp2.name = 'PidTagLastModificationTime' "
			      "UNION "
			      "SELECT m2.folder_id, mp.value, mp2.value FROM mailboxes m2 "
			      "JOIN mailboxes_properties mp ON mp.mailbox_id = m2.id AND "
			      "mp.name = 'PidTagMessageClass' "
			      "JOIN mailboxes_properties mp2 ON mp2.mailbox_id = m2.id AND "
			      "mp2.name = 'PidTagLastModificationTime' "
			      "WHERE m2.name='%s';", _sql(mem_ctx, recipient),
			      _sql(mem_ctx, recipient));
	OPENCHANGE_RETVAL_IF(!sql, MAPI_E_NOT_ENOUGH_MEMORY, tmp_ctx);

	ret = status(select_without_fetch(conn, sql, &res));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, tmp_ctx);

	nrows = mysql_num_rows(res);
	rcvfolders = talloc_array(tmp_ctx, struct ReceiveFolder, nrows + 1);
	for (i = 0; i < nrows; i++) {
		row = mysql_fetch_row(res);
		if (row == NULL) {
			DEBUG(0, ("Error getting row %d of `%s`: %s\n",
				  (int)i, sql, mysql_error(conn)));
			mysql_free_result(res);
			OPENCHANGE_RETVAL_IF(row == NULL, MAPI_E_CALL_FAILED, tmp_ctx);
		}

		num_fields = mysql_num_fields(res);
		OPENCHANGE_RETVAL_IF(num_fields != 3, MAPI_E_CALL_FAILED, tmp_ctx);

		rcvfolders[i].flag = 0;
		rcvfolders[i].fid = strtoll(row[0], NULL, 10);
		if (!row[1] || (row[1] && !strcmp(row[1], "All"))) {
			rcvfolders[i].lpszMessageClass = "";
		} else {
			rcvfolders[i].lpszMessageClass = talloc_strdup(rcvfolders, row[1]);
		}

		nt = strtoull(row[2], NULL, 10);

		rcvfolders[i].modiftime.dwLowDateTime = (nt << 32) >> 32;
		rcvfolders[i].modiftime.dwHighDateTime = (nt >> 32);
	}

	*cValues = (uint32_t) nrows;
	talloc_steal(mem_ctx, rcvfolders);
	*entries = rcvfolders;

	mysql_free_result(res);
	talloc_free(tmp_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_TransportFolder(struct openchangedb_context *self,
					   const char *recipient,
					   int64_t *folder_id)
{
	return get_SystemFolderID(self, recipient, TRANSPORT_FOLDER_SYSTEM_IDX,
				  folder_id);
}

static enum MAPISTATUS get_mailbox_ids_by_name(MYSQL *conn,
					       const char *username,
					       int64_t *mailbox_id,
					       int64_t *mailbox_folder_id)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_mailbox_ids_by_name");
	enum MAPISTATUS ret;
	char * sql;
	MYSQL_RES *res;
	MYSQL_ROW row;

	sql = talloc_asprintf(mem_ctx, // FIXME ou_id
		"SELECT m.id, m.folder_id FROM mailboxes m WHERE m.name = '%s'",
		_sql(mem_ctx, username));
	ret = status(select_without_fetch(conn, sql, &res));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);
	row = mysql_fetch_row(res);
	if (row == NULL) {
		DEBUG(0, ("Error getting user's mailbox `%s`: %s", sql,
			  mysql_error(conn)));
		ret = MAPI_E_NOT_FOUND;
		goto end;
	}

	if (mailbox_id) {
		*mailbox_id = strtoll(row[0], NULL, 10);
	}
	if (mailbox_folder_id) {
		*mailbox_folder_id = strtoll(row[1], NULL, 10);
	}
end:
	mysql_free_result(res);
	talloc_free(mem_ctx);
	return ret;
}

#define is_public_folder(id) is_public_folder_id(NULL, id)
static bool is_public_folder_id(struct openchangedb_context *self, int64_t fid)
{
	int64_t real_fid = (fid >> 56) | (((fid >> 48) & 0x00FF) << 8);
	return ((fid & 0xFFFF000000000000) << 16) == 0x0 &&
		real_fid <= MAX_PUBLIC_FOLDER_ID;
}

static enum MAPISTATUS get_folder_count(struct openchangedb_context *self,
					const char *username, int64_t fid,
					uint32_t *RowCount)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_folder_count");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;
	char *sql;
	int64_t count = 0, mailbox_id = 0, mailbox_folder_id = 0;

	if (is_public_folder(fid)) {
		sql = talloc_asprintf(mem_ctx,
			"SELECT count(f1.id) FROM folders f1 "
			"JOIN folders f2 ON f2.id = f1.parent_folder_id"
			"  AND f2.folder_id = %"PRIu64" "
			"WHERE f1.folder_class = '%s'", fid, PUBLIC_FOLDER);
	} else {
		// The fid could be from either the mailbox or a system folder
		ret = get_mailbox_ids_by_name(conn, username,
					      &mailbox_id, &mailbox_folder_id);
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

		if (mailbox_folder_id == fid) {
			sql = talloc_asprintf(mem_ctx,
				"SELECT count(f.id) FROM folders f "
				"WHERE f.mailbox_id = %"PRIu64" "
				"  AND f.parent_folder_id IS NULL",
				mailbox_id);
		} else {
			sql = talloc_asprintf(mem_ctx,
				"SELECT count(f1.id) FROM folders f1 "
				"JOIN folders f2 ON f2.id = f1.parent_folder_id"
				"  AND f2.folder_id = %"PRIu64" "
				"WHERE f1.mailbox_id = %"PRIu64,
				fid, mailbox_id);
		}
	}

	ret = status(select_first_int64(conn, sql, &count));
	*RowCount = count;

	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS lookup_folder_property(struct openchangedb_context *self,
					      uint32_t proptag, int64_t fid)
{
	return not_implemented();
}

/**
   \details Get the current change number field
 */
static enum MAPISTATUS get_server_change_number(MYSQL *conn,
						int64_t *change_number)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_server_change_number");
	char *sql;
	enum MAPISTATUS ret;

	sql = talloc_asprintf(mem_ctx, // FIXME ou_id
		"SELECT change_number FROM servers s "
		"JOIN company c ON c.id = s.company_id "
		"JOIN organizational_units ou ON ou.company_id = c.id "
		);
	ret = status(select_first_int64(conn, sql, change_number));
	talloc_free(mem_ctx);
	return ret;
}

/**
   \details Set the current change number field
 */
static enum MAPISTATUS set_server_change_number(MYSQL *conn,
						int64_t change_number)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "set_server_change_number");
	char *sql;
	enum MAPISTATUS ret;

	sql = talloc_asprintf(mem_ctx, //FIXME ou_id
		"UPDATE servers s "
		"JOIN company c ON c.id = s.company_id "
		"JOIN organizational_units ou ON ou.company_id = c.id "
		"SET s.change_number=%"PRIu64"",
		change_number);
	ret = status(execute_query(conn, sql));
	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS get_new_changeNumber(struct openchangedb_context *self,
					    int64_t *cn)
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
					     TALLOC_CTX *mem_ctx, int64_t max,
					     struct UI8Array_r **cns_p)
{
	enum MAPISTATUS ret;
	MYSQL *conn = self->data;
	struct UI8Array_r *cns;
	int64_t cn;
	size_t count;

	ret = get_server_change_number(conn, &cn);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, NULL);

	// Transform the numbers the way exchange protocol likes it
	cns = talloc_zero(mem_ctx, struct UI8Array_r);
	cns->cValues = max;
	cns->lpui8 = talloc_array(cns, int64_t, max);

	for (count = 0; count < max; count++) {
		cns->lpui8[count] = (exchange_globcnt(cn + count) << 16) | 0x0001;
	}

	ret = set_server_change_number(conn, cn + max); // TODO ou_id
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, NULL);

	*cns_p = cns;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_next_changeNumber(struct openchangedb_context *self,
					     int64_t *cn)
{
	enum MAPISTATUS ret;
	MYSQL *conn = self->data;

	ret = get_server_change_number(conn, cn);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, NULL);

	// Transform the number the way exchange protocol likes it
	*cn = (exchange_globcnt(*cn) << 16) | 0x0001;

	return MAPI_E_SUCCESS;
}

static char *_unknown_property(TALLOC_CTX *mem_ctx, uint32_t proptag)
{
	return talloc_asprintf(mem_ctx, "Unknown%.8x", proptag);
}

static struct BinaryArray_r *decode_mv_binary(TALLOC_CTX *mem_ctx, const char *str)
{
	const char *start;
	char *tmp;
	size_t i, current, len;
	uint32_t j;
	struct BinaryArray_r *bin_array;

	bin_array = talloc_zero(mem_ctx, struct BinaryArray_r);

	start = str;
	len = strlen(str);
	i = 0;
	while (i < len && start[i] != ';') {
		i++;
	}
	if (i < len) {
		tmp = talloc_memdup(NULL, start, i + 1);
		tmp[i] = 0;
		bin_array->cValues = strtol(tmp, NULL, 16);
		bin_array->lpbin = talloc_array(bin_array, struct Binary_r, bin_array->cValues);
		talloc_free(tmp);

		i++;
		for (j = 0; j < bin_array->cValues; j++) {
			current = i;
			while (i < len && start[i] != ';') {
				i++;
			}

			tmp = talloc_memdup(bin_array, start + current, i - current + 1);
			tmp[i - current] = 0;
			i++;

			if (tmp[0] != 0 && strcmp(tmp, nil_string) != 0) {
				bin_array->lpbin[j].lpb = (uint8_t *) tmp;
				bin_array->lpbin[j].cb = ldb_base64_decode((char *) bin_array->lpbin[j].lpb);
			}
			else {
				bin_array->lpbin[j].lpb = talloc_zero(bin_array, uint8_t);
				bin_array->lpbin[j].cb = 0;
			}
		}
	}

	return bin_array;
}

static struct LongArray_r *decode_mv_long(TALLOC_CTX *mem_ctx, const char *str)
{
	const char *start;
	char *tmp;
	size_t i, current, len;
	uint32_t j;
	struct LongArray_r *long_array;

	long_array = talloc_zero(mem_ctx, struct LongArray_r);

	start = str;
	len = strlen(str);
	i = 0;
	while (i < len && start[i] != ';') {
		i++;
	}
	if (i < len) {
		tmp = talloc_memdup(NULL, start, i + 1);
		tmp[i] = 0;
		long_array->cValues = strtol(tmp, NULL, 16);
		long_array->lpl = talloc_array(long_array, uint32_t, long_array->cValues);
		talloc_free(tmp);

		i++;
		for (j = 0; j < long_array->cValues; j++) {
			current = i;
			while (i < len && start[i] != ';') {
				i++;
			}

			tmp = talloc_memdup(long_array, start + current, i - current + 1);
			tmp[i - current] = 0;
			i++;

			long_array->lpl[j] = strtol(tmp, NULL, 16);
			talloc_free(tmp);
		}
	}

	return long_array;
}

static void *_get_special_property(TALLOC_CTX *mem_ctx, uint32_t proptag)
{
	uint32_t *l;

	switch (proptag) {
	case PR_DEPTH:
		l = talloc_zero(mem_ctx, uint32_t);
		*l = 0;
		return (void *)l;
	}

	return NULL;
}

/**
   \details Retrieve a MAPI property from an OpenChange LDB message

   \param mem_ctx pointer to the memory context
   \param msg pointer to the LDB message
   \param proptag the MAPI property tag to lookup
   \param PidTagAttr the mapped MAPI property name

   \return valid data pointer on success, otherwise NULL
 */
static void *get_property_data(TALLOC_CTX *mem_ctx, uint32_t proptag, const char *value)
{
	void			*data;
	int64_t			*ll;
	uint32_t		*l;
	int			*b;
	struct FILETIME		*ft;
	struct Binary_r		*bin;
	struct BinaryArray_r	*bin_array;
	struct LongArray_r	*long_array;

	switch (proptag & 0xFFFF) {
	case PT_BOOLEAN:
		b = talloc_zero(mem_ctx, int);
		if (strlen(value) == 4 && strncasecmp(value, "TRUE", 4) == 0) {
			*b = 1;
		}
		data = (void *)b;
		break;
	case PT_LONG:
		l = talloc_zero(mem_ctx, uint32_t);
		*l = strtoul(value, NULL, 10);
		data = (void *)l;
		break;
	case PT_I8:
		ll = talloc_zero(mem_ctx, int64_t);
		*ll = strtoll(value, NULL, 10);
		data = (void *)ll;
		break;
	case PT_STRING8:
	case PT_UNICODE:
		data = (void *)talloc_strdup(mem_ctx, value);
		break;
	case PT_SYSTIME:
		ft = talloc_zero(mem_ctx, struct FILETIME);
		ll = talloc_zero(mem_ctx, int64_t);
		*ll = strtoll(value, NULL, 10);
		ft->dwLowDateTime = (*ll & 0xffffffff);
		ft->dwHighDateTime = *ll >> 32;
		data = (void *)ft;
		talloc_free(ll);
		break;
	case PT_BINARY:
		bin = talloc_zero(mem_ctx, struct Binary_r);
		if (strcmp(value, nil_string) == 0) {
			bin->lpb = (uint8_t *) talloc_zero(mem_ctx, uint8_t);
			bin->cb = 0;
		} else {
			bin->lpb = (uint8_t *) talloc_strdup(mem_ctx, value);
			bin->cb = ldb_base64_decode((char *) bin->lpb);
		}
		data = (void *)bin;
		break;
	case PT_MV_BINARY:
		bin_array = decode_mv_binary(mem_ctx, value);
		data = (void *)bin_array;
		break;
	case PT_MV_LONG:
		long_array = decode_mv_long(mem_ctx, value);
		data = (void *)long_array;
		break;
	default:
		DEBUG(0, ("[%s:%d] Property Type 0x%.4x not supported\n", __FUNCTION__, __LINE__, (proptag & 0xFFFF)));
		abort();
		return NULL;
	}

	return data;
}

static enum MAPISTATUS get_folder_property(TALLOC_CTX *parent_ctx,
					   struct openchangedb_context *self,
					   const char *username,
					   uint32_t proptag, int64_t fid,
					   void **data)
{
	// FIXME always a folder from a mailbox?
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_folder_property");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;
	char *sql;
	int64_t mailbox_id, mailbox_folder_id;
	int64_t *n;
	const char *attr, *value;

	attr = openchangedb_property_get_attribute(proptag);
	if (!attr) {
		attr = _unknown_property(parent_ctx, proptag);
	}

	*data = _get_special_property(parent_ctx, proptag);
	if (*data != NULL) goto end;

	if (proptag == PidTagFolderId) {
		n = talloc_zero(parent_ctx, int64_t);
		*n = fid;
		*data = (void *) n;
		goto end;
	}

	if (is_public_folder(fid)) {
		if (proptag == PidTagParentFolderId) {
			n = talloc_zero(parent_ctx, int64_t);
			ret = get_parent_fid(self, username, fid, n, true);
			OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);
			*data = (void *) n;
			goto end;
		}

		sql = talloc_asprintf(mem_ctx, // FIXME ou_id
			"SELECT fp.value FROM folders_properties fp "
			"JOIN folders f ON f.id = fp.folder_id "
			"  AND f.folder_class = '%s'"
			"  AND f.folder_id = %"PRIu64" "
			"WHERE fp.name = '%s'",
			PUBLIC_FOLDER, fid, _sql(mem_ctx, attr));
	} else {
		// system folder
		ret = get_mailbox_ids_by_name(conn, username,
					      &mailbox_id, &mailbox_folder_id);
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

		if (mailbox_folder_id == fid) {
			sql = talloc_asprintf(mem_ctx,
				"SELECT mp.value FROM mailboxes_properties mp "
				"WHERE mp.mailbox_id = %"PRIu64" AND mp.name = '%s'",
				mailbox_id, _sql(mem_ctx, attr));
		} else if (proptag == PidTagParentFolderId) {
			n = talloc_zero(parent_ctx, int64_t);
			ret = get_parent_fid(self, username, fid, n, true);
			OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);
			*data = (void *) n;
			goto end;
		} else {
			sql = talloc_asprintf(mem_ctx,
				"SELECT fp.value FROM folders_properties fp "
				"JOIN folders f ON f.id = fp.folder_id "
				"  AND f.mailbox_id = %"PRIu64" "
				"  AND f.folder_id = %"PRIu64" "
				"WHERE fp.name = '%s'",
				mailbox_id, fid, _sql(mem_ctx, attr));
		}
	}

	ret = status(select_first_string(mem_ctx, conn, sql, &value));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);
	// Transform string into the expected data type
	*data = get_property_data(parent_ctx, proptag, value);
end:
	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}

static char *str_list_join_for_sql(TALLOC_CTX *mem_ctx, const char **list)
{
	char *ret = NULL;
	int i;

	if (list[0] == NULL) {
		return talloc_strdup(mem_ctx, "");
	}

	ret = talloc_asprintf(mem_ctx, "'%s'", _sql(mem_ctx, list[0]));

	for (i = 1; list[i]; i++) {
		ret = talloc_asprintf_append_buffer(ret, ",'%s'",
						    _sql(mem_ctx, list[i]));
	}

	return ret;
}

static enum MAPISTATUS set_folder_properties(struct openchangedb_context *self,
					     const char *username, int64_t fid,
					     struct SRow *row)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "set_folder_property");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;
	char *sql;
	int64_t mailbox_id, mailbox_folder_id, id;
	uint32_t i;
	const char *attr;
	struct SPropValue *value;
	enum MAPITAGS tag;
	const char **names, **values;
	char *table, *str_value, *column_id, *names_for_sql, *values_for_sql;
	time_t unix_time = time(NULL);
	NTTIME nt_time;

	if (is_public_folder(fid)) {
		// Updating public folder
		table = talloc_strdup(mem_ctx, "folders_properties");
		column_id = talloc_strdup(mem_ctx, "folder_id");
		sql = talloc_asprintf(mem_ctx, // FIXME ou_id
			"SELECT f.id FROM folders f "
			"WHERE f.folder_id = %"PRIu64
			"  AND f.folder_class = '%s'", fid, PUBLIC_FOLDER);
		ret = status(select_first_int64(conn, sql, &id));
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);
	} else {
		ret = get_mailbox_ids_by_name(conn, username,
					      &mailbox_id, &mailbox_folder_id);
		if (mailbox_folder_id == fid) {
			// Updating mailbox properties
			table = talloc_strdup(mem_ctx, "mailboxes_properties");
			column_id = talloc_strdup(mem_ctx, "mailbox_id");
			id = mailbox_id;
		} else {
			// Updating folder
			table = talloc_strdup(mem_ctx, "folders_properties");
			column_id = talloc_strdup(mem_ctx, "folder_id");
			sql = talloc_asprintf(mem_ctx,
				"SELECT f.id FROM folders f "
				"WHERE f.folder_id = %"PRIu64" "
				"  AND f.mailbox_id = %"PRIu64, fid, mailbox_id);
			ret = status(select_first_int64(conn, sql, &id));
			OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);
		}
	}

	names = (const char **)str_list_make_empty(mem_ctx);
	values = (const char **)str_list_make_empty(mem_ctx);
	for (i = 0; i < row->cValues; i++) {
		value = row->lpProps + i;
		tag = value->ulPropTag;
		if (tag == PR_DEPTH || tag == PR_SOURCE_KEY ||
		    tag == PR_PARENT_SOURCE_KEY || tag == PR_CREATION_TIME ||
		    tag == PR_LAST_MODIFICATION_TIME) {
			DEBUG(5, ("Ignored attempt to set handled property %.8x\n",
				  tag));
			continue;
		}

		attr = openchangedb_property_get_attribute(tag);
		if (!attr) {
			attr = _unknown_property(mem_ctx, tag);
		}

		str_value = openchangedb_set_folder_property_data(mem_ctx, value);
		if (!str_value) {
			DEBUG(5, ("Ignored property of unhandled type %.4x\n",
				  (tag & 0xffff)));
			continue;
		}

		names = str_list_add(names, attr);
		values = str_list_add(values, str_value);
	}

	// Add last modification
	value = talloc_zero(mem_ctx, struct SPropValue);
	value->ulPropTag = PR_LAST_MODIFICATION_TIME;
	unix_to_nt_time(&nt_time, unix_time);
	value->value.ft.dwLowDateTime = nt_time & 0xffffffff;
	value->value.ft.dwHighDateTime = nt_time >> 32;
	attr = openchangedb_property_get_attribute(value->ulPropTag);
	str_value = openchangedb_set_folder_property_data(mem_ctx, value);
	names = str_list_add(names, attr);
	values = str_list_add(values, str_value);

	// Add change number
	value->ulPropTag = PidTagChangeNumber;
	get_new_changeNumber(self, (int64_t *) &value->value.d);
	attr = openchangedb_property_get_attribute(value->ulPropTag);
	str_value = openchangedb_set_folder_property_data(mem_ctx, value);
	names = str_list_add(names, attr);
	values = str_list_add(values, str_value);

	// Delete previous values
	names_for_sql = str_list_join_for_sql(mem_ctx, names);
	sql = talloc_asprintf(mem_ctx,
		"DELETE FROM %s WHERE %s = %"PRIu64" AND name IN (%s)",
		table, column_id, id, names_for_sql);
	ret = status(execute_query(conn, sql));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	// Insert new values
	values_for_sql = talloc_asprintf(mem_ctx, "(%"PRIu64", '%s', '%s')",
					 id, _sql(mem_ctx, names[0]),
					 _sql(mem_ctx, values[0]));
	for (i = 1; names[i]; i++) {
		values_for_sql = talloc_asprintf_append_buffer(values_for_sql,
			",(%"PRIu64", '%s', '%s')", id, _sql(mem_ctx, names[i]),
			_sql(mem_ctx, values[i]));
	}
	sql = talloc_asprintf(mem_ctx, "INSERT INTO %s VALUES %s",
			      table, values_for_sql);
	ret = status(execute_query(conn, sql));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
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
				       const char *username,
				       int64_t parent_fid,
				       const char *foldername, int64_t *fid)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_fid_by_name");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;
	char *sql;
	int64_t mailbox_id, mailbox_folder_id;
	bool is_public;

	is_public = is_public_folder(parent_fid);

	if (!is_public) {
		ret = get_mailbox_ids_by_name(conn, username,
					      &mailbox_id, &mailbox_folder_id);
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);
	}

	// FIXME i18n
	if (!is_public && mailbox_folder_id == parent_fid) {
		// The parent folder is the mailbox itself
		sql = talloc_asprintf(mem_ctx,
			"SELECT f.folder_id FROM folders f "
			"JOIN folders_properties p ON p.folder_id = f.id"
			"  AND p.name = 'PidTagDisplayName'"
			"  AND p.value = '%s' "
			"WHERE f.mailbox_id = %"PRIu64,
			_sql(mem_ctx, foldername), mailbox_id);
	} else if (is_public) {
		// system folder
		sql = talloc_asprintf(mem_ctx,
			"SELECT f1.folder_id FROM folders f1 "
			"JOIN folders_properties p ON p.folder_id = f1.id"
			"  AND p.name = 'PidTagDisplayName'"
			"  AND p.value = '%s' "
			"JOIN folders f2 ON f2.id = f1.parent_folder_id"
			"  AND f2.folder_id = %"PRIu64" ",
			_sql(mem_ctx, foldername), parent_fid);
	} else {
		// system folder
		sql = talloc_asprintf(mem_ctx,
			"SELECT f1.folder_id FROM folders f1 "
			"JOIN folders_properties p ON p.folder_id = f1.id"
			"  AND p.name = 'PidTagDisplayName'"
			"  AND p.value = '%s' "
			"JOIN folders f2 ON f2.id = f1.parent_folder_id"
			"  AND f2.folder_id = %"PRIu64" "
			"WHERE f1.mailbox_id = %"PRIu64,
			_sql(mem_ctx, foldername), parent_fid, mailbox_id);
	}

	ret = status(select_first_int64(conn, sql, fid));

	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS get_mid_by_subject_from_public_folder(struct openchangedb_context *self,
							     const char *username,
							     int64_t parent_fid,
					  	  	     const char *subject,
							     int64_t *mid)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0,
		"get_mid_by_subject_from_public_folder");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;
	char *sql;

	// Parent folder is a public folder
	sql = talloc_asprintf(mem_ctx, //FIXME ou_id
		"SELECT m.message_id FROM messages m "
		"JOIN folders f1 ON f1.id = m.folder_id"
		"  AND f1.folder_class = '%s'"
		"  AND f1.folder_id = %"PRIu64" "
		"WHERE m.normalized_subject = '%s'",
		PUBLIC_FOLDER, parent_fid, _sql(mem_ctx, subject));

	ret = status(select_first_int64(conn, sql, mid));

	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS get_mid_by_subject_from_system_folder(struct openchangedb_context *self,
							     const char *username,
							     int64_t parent_fid,
					  	  	     const char *subject,
							     int64_t *mid)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0,
		"get_mid_by_subject_from_system_folder");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;
	char *sql;
	int64_t mailbox_id, mailbox_folder_id;

	ret = get_mailbox_ids_by_name(conn, username,
				      &mailbox_id, &mailbox_folder_id);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	if (mailbox_folder_id == parent_fid) {
		// The parent folder is the mailbox itself
		sql = talloc_asprintf(mem_ctx,
			"SELECT m.message_id FROM messages m "
			"WHERE m.mailbox_id = %"PRIu64
			"  AND m.normalized_subject = '%s'",
			mailbox_id, _sql(mem_ctx, subject));
	} else {
		// Parent folder is a system folder
		sql = talloc_asprintf(mem_ctx,
			"SELECT m.message_id FROM messages m "
			"JOIN folders f1 ON f1.id = m.folder_id "
			"  AND f1.folder_class = '%s'"
			"  AND f1.folder_id = %"PRIu64" "
			"  AND f1.mailbox_id = %"PRIu64" "
			"WHERE m.normalized_subject = '%s'",
			SYSTEM_FOLDER, parent_fid, mailbox_id,
			_sql(mem_ctx, subject));
	}

	ret = status(select_first_int64(conn, sql, mid));

	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS get_mid_by_subject(struct openchangedb_context *self,
					  const char *username,
					  int64_t parent_fid,
					  const char *subject,
					  bool mailboxstore, int64_t *mid)
{
	if (mailboxstore) {
		return get_mid_by_subject_from_system_folder(self, username,
							     parent_fid,
							     subject, mid);
	} else {
		return get_mid_by_subject_from_public_folder(self, username,
							     parent_fid,
							     subject, mid);
	}
}

static enum MAPISTATUS delete_folder(struct openchangedb_context *self,
				     const char *username,
				     int64_t fid)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "delete_folder");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;
	char *sql;

	if (is_public_folder(fid)) {
		sql = talloc_asprintf(mem_ctx, //FIXME ou_id
			"DELETE f FROM folders f "
			"WHERE f.folder_id = %"PRIu64,
			fid);
	} else {
		// Parent folder is a public folder
		sql = talloc_asprintf(mem_ctx, //FIXME ou_id
			"DELETE f FROM folders f "
			"JOIN mailboxes m ON m.id = f.mailbox_id"
			"  AND m.name = '%s' "
			"WHERE f.folder_id = %"PRIu64,
			_sql(mem_ctx, username), fid);
	}

	ret = status(execute_query(conn, sql));

	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS set_ReceiveFolder(struct openchangedb_context *self,
					 const char *recipient,
					 const char *message_class,
					 int64_t fid)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "set_ReceiveFolder");
	char *sql;
	enum MAPISTATUS ret;
	int64_t mailbox_id, mailbox_folder_id;

	// Delete current receive folder for that message class if exists
	sql = talloc_asprintf(mem_ctx,
		"DELETE fp FROM folders_properties fp "
		"JOIN folders f ON f.id = fp.folder_id "
		"JOIN mailboxes m ON m.id = f.mailbox_id AND m.name = '%s' "
		"WHERE fp.name = 'PidTagMessageClass' AND fp.value = '%s'",
		_sql(mem_ctx, recipient), _sql(mem_ctx, message_class)); // FIXME ou_id
	ret = status(execute_query(conn, sql));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	sql = talloc_asprintf(mem_ctx,
		"DELETE mp FROM mailboxes_properties mp "
		"JOIN mailboxes mb ON mb.id = mp.mailbox_id AND mb.name = '%s' "
		"WHERE mp.name = 'PidTagMessageClass' AND mp.value = '%s'",
		_sql(mem_ctx, recipient), _sql(mem_ctx, message_class)); // FIXME ou_id
	ret = status(execute_query(conn, sql));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	if (fid == 0) {
		goto end;
	}

	// Create PidTagMessageClass folder property for the fid specified
	ret = get_mailbox_ids_by_name(conn, recipient, &mailbox_id,
				      &mailbox_folder_id);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);
	if (fid == mailbox_folder_id) {
		// Folder is the mailbox itself
		sql = talloc_asprintf(mem_ctx,
			"INSERT INTO mailboxes_properties VALUES "
			"(%"PRIu64", 'PidTagMessageClass', '%s')",
			mailbox_id, _sql(mem_ctx, message_class));
	} else {
		sql = talloc_asprintf(mem_ctx,
			"INSERT INTO folders_properties VALUES ("
			" ("
			"  SELECT f.id FROM folders f "
			"  JOIN mailboxes m ON m.id = f.mailbox_id AND m.name = '%s' "
			"  WHERE f.folder_id = %"PRIu64
			" ), 'PidTagMessageClass', '%s')",
			_sql(mem_ctx, recipient), fid, _sql(mem_ctx, message_class)); // FIXME ou_id
	}
	ret = status(execute_query(conn, sql));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

end:
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_fid_from_partial_uri(struct openchangedb_context *self,
						const char *partialURI,
						int64_t *fid)
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
				      int64_t fid, const char *display_name)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "create_mailbox");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;
	char *sql, *mailbox_guid, *replica_guid, *values_sql, *value;
	const char **l;
	struct GUID guid;
	int64_t change_number;
	NTTIME now;

	unix_to_nt_time(&now, time(NULL));

	get_new_changeNumber(self, &change_number);

	// Insert row in mailboxes
	guid = GUID_random();
	mailbox_guid = GUID_string(mem_ctx, &guid);
	guid = GUID_random();
	replica_guid = GUID_string(mem_ctx, &guid);
	sql = talloc_asprintf(mem_ctx, // FIXME ou_id
		"INSERT INTO mailboxes SET folder_id = %"PRIu64", name = '%s', "
		"MailboxGUID = '%s', ReplicaGUID = '%s', ReplicaID = %d, "
		"SystemIdx = %d, ou_id = (SELECT id FROM organizational_units LIMIT 1)",
		fid, _sql(mem_ctx, username), mailbox_guid, replica_guid, 1, systemIdx);
	ret = status(execute_query(conn, sql));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	// Insert mailboxes properties
	l = (const char **) str_list_make_empty(mem_ctx);
	l = str_list_add(l, "(LAST_INSERT_ID(), 'PidTagAccess', '63')");
	l = str_list_add(l, "(LAST_INSERT_ID(), 'PidTagRights', '2043')");
	l = str_list_add(l, "(LAST_INSERT_ID(), 'PidTagFolderType', '1')");
	l = str_list_add(l, "(LAST_INSERT_ID(), 'PidTagSubFolders', 'TRUE')");
	value = talloc_asprintf(mem_ctx,
		"(LAST_INSERT_ID(), 'PidTagDisplayName', '%s')", display_name);
	l = str_list_add(l, value);
	value = talloc_asprintf(mem_ctx,
		"(LAST_INSERT_ID(), 'PidTagCreationTime', '%"PRId64"')", now);
	l = str_list_add(l, value);
	value = talloc_asprintf(mem_ctx,
		"(LAST_INSERT_ID(), 'PidTagLastModificationTime', '%"PRId64"')",
		now);
	l = str_list_add(l, value);
	value = talloc_asprintf(mem_ctx,
		"(LAST_INSERT_ID(), 'PidTagChangeNumber', '%"PRIu64"')",
		change_number);
	l = str_list_add(l, value);
	values_sql = str_list_join(mem_ctx, l, ',');
	sql = talloc_asprintf(mem_ctx,
		"INSERT INTO mailboxes_properties VALUES %s", values_sql);
	ret = status(execute_query(conn, sql));

	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS create_folder(struct openchangedb_context *self,
				     const char *username,
				     int64_t pfid, int64_t fid,
				     int64_t changeNumber,
				     const char *MAPIStoreURI, int systemIdx)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "create_folder");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;
	char *sql, *values_sql, *value;
	const char **l;
	int64_t change_number;
	NTTIME now;

	unix_to_nt_time(&now, time(NULL));

	get_new_changeNumber(self, &change_number);

	if (is_public_folder(fid)) {
		// Insert row in folders
		sql = talloc_asprintf(mem_ctx,
			"INSERT INTO folders SET ou_id = ("
			"  SELECT ou_id FROM mailboxes WHERE name = '%s'),"
			"folder_id = %"PRIu64", folder_class = '%s', "
			"parent_folder_id = ("
			"  SELECT f.id FROM folders f"
			"  WHERE f.ou_id = (SELECT ou_id FROM mailboxes WHERE name = '%s')"
			"    AND f.folder_id = %"PRIu64"), "
			"FolderType = %d, SystemIdx = %d",
			_sql(mem_ctx, username), fid, PUBLIC_FOLDER,
			_sql(mem_ctx, username), pfid, 1, systemIdx);
		if (MAPIStoreURI) {
			sql = talloc_asprintf_append(sql, ",MAPIStoreURI = '%s'",
						     _sql(mem_ctx, MAPIStoreURI));
		}
	} else {
		// Insert row in folders
		sql = talloc_asprintf(mem_ctx,
			"INSERT INTO folders SET ou_id = (SELECT ou_id FROM mailboxes "
							 "WHERE name = '%s'),"
			"folder_id = %"PRIu64", folder_class = '%s', "
			"mailbox_id = (SELECT id FROM mailboxes WHERE name = '%s'), "
			"parent_folder_id = (SELECT f.id FROM folders f "
					    "JOIN mailboxes m ON m.id = f.mailbox_id "
					    "WHERE m.name = '%s' "
					    "  AND f.folder_id = %"PRIu64"), "
			"FolderType = %d, SystemIdx = %d",
			_sql(mem_ctx, username), fid, SYSTEM_FOLDER,
			_sql(mem_ctx, username), _sql(mem_ctx, username), pfid,
			1, systemIdx);
		if (MAPIStoreURI) {
			sql = talloc_asprintf_append(sql, ", MAPIStoreURI = '%s'",
						     MAPIStoreURI);
		} else {
			sql = talloc_asprintf_append(sql, ", MAPIStoreURI = NULL");
		}
	}
	ret = status(execute_query(conn, sql));
	// FIXME return MAPI_E_COLLISION if applies
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	// Insert mailboxes properties
	l = (const char **) str_list_make_empty(mem_ctx);
	l = str_list_add(l, "(LAST_INSERT_ID(), 'PidTagContentUnreadCount', '0')");
	l = str_list_add(l, "(LAST_INSERT_ID(), 'PidTagContentCount', '0')");
	l = str_list_add(l, "(LAST_INSERT_ID(), 'PidTagAttributeHidden', '0')");
	l = str_list_add(l, "(LAST_INSERT_ID(), 'PidTagAttributeSystem', '0')");
	l = str_list_add(l, "(LAST_INSERT_ID(), 'PidTagAttributeReadOnly', '0')");
	/* FIXME: PidTagAccess and PidTagRights are user-specific */
	l = str_list_add(l, "(LAST_INSERT_ID(), 'PidTagAccess', '63')");
	l = str_list_add(l, "(LAST_INSERT_ID(), 'PidTagRights', '2043')");
	value = talloc_asprintf(mem_ctx,
		"(LAST_INSERT_ID(), 'PidTagCreationTime', '%"PRId64"')", now);
	l = str_list_add(l, value);
	value = talloc_asprintf(mem_ctx,
		"(LAST_INSERT_ID(), 'PidTagChangeNumber', '%"PRIu64"')",
		change_number);
	l = str_list_add(l, value);
	values_sql = str_list_join(mem_ctx, l, ',');
	sql = talloc_asprintf(mem_ctx,
		"INSERT INTO folders_properties VALUES %s", values_sql);
	ret = status(execute_query(conn, sql));

	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS get_message_count(struct openchangedb_context *self,
					 const char *username, int64_t fid,
					 uint32_t *RowCount, bool fai)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_message_count");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;
	char *sql, *message_type;
	int64_t mailbox_id, mailbox_folder_id, count;

	if (fai) {
		message_type = talloc_strdup(mem_ctx, "faiMessage");
	} else {
		message_type = talloc_strdup(mem_ctx, "systemMessage");
	}

	if (is_public_folder(fid)) {
		sql = talloc_asprintf(mem_ctx, // FIXME ou_id
			"SELECT count(*) FROM messages m "
			"JOIN folders f1 ON f1.id = m.folder_id "
			"  AND f1.folder_class = '%s'"
			"  AND f1.folder_id = %"PRIu64" "
			"WHERE m.message_type = '%s'",
			PUBLIC_FOLDER, fid, message_type);
	} else {
		ret = get_mailbox_ids_by_name(conn, username,
					      &mailbox_id, &mailbox_folder_id);
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

		if (mailbox_folder_id == fid) {
			// The parent folder is the mailbox itself
			sql = talloc_asprintf(mem_ctx,
				"SELECT count(*) FROM messages m "
				"WHERE m.mailbox_id = %"PRIu64
				"  AND m.folder_id IS NULL"
				"  AND m.message_type = '%s'",
				mailbox_id, message_type);
		} else {
			// Parent folder is a system folder
			sql = talloc_asprintf(mem_ctx,
				"SELECT count(*) FROM messages m "
				"JOIN folders f1 ON f1.id = m.folder_id "
				"  AND f1.folder_class = '%s'"
				"  AND f1.folder_id = %"PRIu64" "
				"  AND f1.mailbox_id = %"PRIu64" "
				"WHERE m.message_type = '%s'",
				SYSTEM_FOLDER, fid, mailbox_id, message_type);
		}
	}

	ret = status(select_first_int64(conn, sql, &count));
	*RowCount = (uint32_t)count;

	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS get_system_idx(struct openchangedb_context *self,
				      const char *username, int64_t fid,
				      int *system_idx_p)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_system_idx");
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;
	char *sql;
	int64_t system_idx = 0;

	if (is_public_folder(fid)) {
		sql = talloc_asprintf(mem_ctx, // FIXME ou_id
			"SELECT f.SystemIdx FROM folders f "
			"WHERE f.folder_id = %"PRIu64" "
			"  AND f.folder_class = '%s'",
			fid, PUBLIC_FOLDER);
	} else {
		sql = talloc_asprintf(mem_ctx, // FIXME ou_id
			"SELECT f.SystemIdx FROM folders f "
			"JOIN mailboxes m ON m.id = f.mailbox_id AND m.name = '%s' "
			"WHERE f.folder_id = %"PRIu64,
			_sql(mem_ctx, username), fid);
	}

	ret = status(select_first_int64(conn, sql, &system_idx));
	*system_idx_p = (int)system_idx;

	talloc_free(mem_ctx);
	return ret;
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

static enum MAPISTATUS get_new_public_folderID(struct openchangedb_context *self,
					       const char *username,
					       int64_t *fid)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_new_public_folderID");
	MYSQL *conn = self->data;
	char *sql;
	enum MAPISTATUS ret;

	sql = talloc_asprintf(mem_ctx, // TODO ou_id
		"SELECT count(f.folder_id) FROM folders f "
		"WHERE folder_class = '%s'", PUBLIC_FOLDER);
	ret = status(select_first_int64(conn, sql, fid));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	if (*fid > 0) {
		sql = talloc_asprintf(mem_ctx, // TODO ou_id
			"SELECT max((f.folder_id >> 56) | "
			"           (((f.folder_id >> 48) & 0x00ff) << 8)) "
			"FROM folders f WHERE folder_class = '"PUBLIC_FOLDER"'");
		ret = status(select_first_int64(conn, sql, fid));
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);
	}

	if (*fid >= MAX_PUBLIC_FOLDER_ID) {
		// FIXME search for free ids, but in theory public folders are
		// only created on provisioning so this should not be needed
		OPENCHANGE_RETVAL_ERR(MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);
	}

	(*fid)++;

	*fid = (exchange_globcnt(*fid) << 16) | 0x0001;

	talloc_free(mem_ctx);
	return ret;
}

static const char *get_indexing_url(struct openchangedb_context *self, const char *username)
{
	const char *indexing_url = NULL, *sql;
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_indexing_url");
	MYSQL *conn = self->data;

	// FIXME ou_id
	sql = talloc_asprintf(mem_ctx,
		"SELECT indexing_url FROM mailboxes WHERE name = '%s'",
		_sql(mem_ctx, username));

	select_first_string(self, conn, sql, &indexing_url);
	// TODO if not result, get default backend and save it to database

	talloc_free(mem_ctx);

	return indexing_url;
}

static bool set_locale(struct openchangedb_context *self, const char *username,
		       uint32_t lcid)
{
	TALLOC_CTX *mem_ctx;
	MYSQL *conn = self->data;
	char *sql;
	const char *locale, *current_locale;
	enum MYSQLRESULT ret;

	locale = mapi_get_locale_from_lcid(lcid);
	if (locale == NULL) {
		DEBUG(0, ("Unknown locale (lcid) %"PRIu32" for mailbox %s\n",
			  lcid, username));
		return false;
	}
	mem_ctx = talloc_named(NULL, 0, "set_locale");
	// FIXME ou_id
	sql = talloc_asprintf(mem_ctx,
		"SELECT locale FROM mailboxes WHERE name = '%s'", username);
	ret = select_first_string(mem_ctx, conn, sql, &current_locale);
	if (ret != MYSQL_SUCCESS) {
		DEBUG(0, ("Error getting locale of mailbox %s\n", username));
		talloc_free(mem_ctx);
		return false;
	}
	if (current_locale && strncmp(locale, current_locale, strlen(locale)) == 0) {
		talloc_free(mem_ctx);
		return false;
	}
	// FIXME ou_id
	sql = talloc_asprintf(mem_ctx,
		"UPDATE mailboxes SET locale='%s' WHERE name = '%s'",
		locale, username);
	ret = execute_query(conn, sql);
	if (ret != MYSQL_SUCCESS) {
		DEBUG(0, ("Error updating locale %s of mailbox %s\n",
			  locale, username));
	}
	talloc_free(mem_ctx);
	return true;
}

static const char **get_folders_names(TALLOC_CTX *mem_ctx,
				      struct openchangedb_context *self,
				      const char *locale, const char *type)
{
	TALLOC_CTX *local_mem_ctx = talloc_named(NULL, 0, "get_folders_name");
	char *table = talloc_asprintf(local_mem_ctx, "provisioning_%s", type);
	char *sql, *short_locale;
	const char **ret = NULL;
	struct StringArrayW_r *results;

	short_locale = talloc_memdup(local_mem_ctx, locale, sizeof(char) * 3);
	short_locale[2] = '\0';
	sql = talloc_asprintf(local_mem_ctx,
		"SELECT * FROM %s WHERE locale = '%s' "
		"UNION "
		"SELECT * FROM %s WHERE locale LIKE '%s%%' "
		"LIMIT 1", table, locale, table, short_locale);

	if (select_all_strings(mem_ctx, conn, sql, &results) == MYSQL_SUCCESS) {
		ret = results->cValues ? results->lppszW : NULL;
	}
	talloc_free(local_mem_ctx);
	return ret;
}
// ^ openchangedb -------------------------------------------------------------

// v openchangedb table -------------------------------------------------------

struct openchangedb_table_message_row {
	int64_t id;
	int64_t mid;
	char *normalized_subject;
};

struct openchangedb_table_folder_row {
	int64_t id;
	int64_t fid;
};

struct openchangedb_table_results {
	size_t count;
	union {
		struct openchangedb_table_folder_row **folders;
		struct openchangedb_table_message_row **messages;
	};
};

struct openchangedb_table {
	int64_t folder_id;
	const char *username;
	uint8_t	table_type;
	struct SSortOrderSet *lpSortCriteria;
	struct mapi_SRestriction *restrictions;
	struct openchangedb_table_results *res;
};


static enum MAPISTATUS table_init(TALLOC_CTX *mem_ctx,
				  struct openchangedb_context *self,
				  const char *username,
				  uint8_t table_type, int64_t folder_id,
				  void **table_object)
{
	struct openchangedb_table *table = talloc_zero(mem_ctx, struct openchangedb_table);

	if (!table) {
		return MAPI_E_NOT_ENOUGH_MEMORY;
	}

	table->folder_id = folder_id;
	table->username = _sql(table, username);
	table->table_type = table_type;
	table->lpSortCriteria = NULL;
	table->restrictions = NULL;
	table->res = NULL;

	*table_object = (void *)table;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS table_set_sort_order(struct openchangedb_context *self,
					    void *_table,
					    struct SSortOrderSet *lpSortCriteria)
{
	struct openchangedb_table *table = (struct openchangedb_table *)_table;

	if (table->res) {
		talloc_free(table->res);
		table->res = NULL;
	}

	if (table->lpSortCriteria) {
		talloc_free(table->lpSortCriteria);
	}

	if (lpSortCriteria) {
		table->lpSortCriteria = talloc_memdup(table, lpSortCriteria,
						      sizeof(struct SSortOrderSet));
		if (!table->lpSortCriteria)
			return MAPI_E_NOT_ENOUGH_MEMORY;

		table->lpSortCriteria->aSort = talloc_memdup(
			table->lpSortCriteria, lpSortCriteria->aSort,
			lpSortCriteria->cSorts * sizeof(struct SSortOrder));

		if (!table->lpSortCriteria->aSort)
			return MAPI_E_NOT_ENOUGH_MEMORY;
	} else {
		table->lpSortCriteria = NULL;
	}

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS table_set_restrictions(struct openchangedb_context *self,
					      void *_table,
					      struct mapi_SRestriction *res)
{
	struct openchangedb_table *table = (struct openchangedb_table *)_table;

	if (table->res) {
		talloc_free(table->res);
		table->res = NULL;
	}

	if (table->restrictions) {
		talloc_free(table->restrictions);
		table->restrictions = NULL;
	}

	table->restrictions = talloc_zero(table, struct mapi_SRestriction);

	if (res->rt != RES_PROPERTY) {
		DEBUG(5, ("Unsupported restriction type: 0x%x\n", res->rt));
		return MAPI_E_INVALID_PARAMETER;
	}

	table->restrictions->rt = res->rt;
	table->restrictions->res.resProperty.relop = res->res.resProperty.relop;
	table->restrictions->res.resProperty.ulPropTag = res->res.resProperty.ulPropTag;
	table->restrictions->res.resProperty.lpProp.ulPropTag = res->res.resProperty.lpProp.ulPropTag;

	switch (table->restrictions->res.resProperty.lpProp.ulPropTag & 0xFFFF) {
	case PT_STRING8:
		table->restrictions->res.resProperty.lpProp.value.lpszA =
			talloc_strdup(table->restrictions, res->res.resProperty.lpProp.value.lpszA);
		break;
	case PT_UNICODE:
		table->restrictions->res.resProperty.lpProp.value.lpszW =
			talloc_strdup(table->restrictions, res->res.resProperty.lpProp.value.lpszW);
		break;
	default:
		DEBUG(0, ("Unsupported property type for RES_PROPERTY restriction\n"));
		return MAPI_E_INVALID_PARAMETER;
	}

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS _table_get_attr_and_value_from_restrictions(
	TALLOC_CTX *mem_ctx, struct mapi_SRestriction *restrictions,
	const char **attr, const char **value)
{
	if (restrictions) {
		*attr = openchangedb_property_get_attribute(restrictions->res.resProperty.ulPropTag);
		switch (restrictions->res.resProperty.ulPropTag & 0xFFFF) {
		case PT_STRING8:
			*value = _sql(mem_ctx, restrictions->res.resProperty.lpProp.value.lpszA);
			break;
		case PT_UNICODE:
			*value = _sql(mem_ctx, restrictions->res.resProperty.lpProp.value.lpszW);
			break;
		default:
			DEBUG(0, ("Unsupported RES_PROPERTY property type: 0x%.4x\n",
				  (restrictions->res.resProperty.ulPropTag & 0xFFFF)));
			return MAPI_E_TOO_COMPLEX;
		}
	}
	return MAPI_E_SUCCESS;

}

static enum MAPISTATUS _table_fetch_messages(MYSQL *conn,
					     struct openchangedb_table *table,
					     bool fai, bool live_filtered)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "_table_fetch_messages");
	char *sql, *msg_type;
	const char *attr, *value;
	MYSQL_RES *res;
	MYSQL_ROW row;
	enum MAPISTATUS ret;
	int64_t id;
	size_t i;
	struct openchangedb_table_results *results;
	struct openchangedb_table_message_row *msg_row;
	struct mapi_SRestriction *restrictions = table->restrictions;

	msg_type = talloc_strdup(mem_ctx, fai ? "faiMessage" : "systemMessage");

	ret = _table_get_attr_and_value_from_restrictions(mem_ctx, restrictions, &attr, &value);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	if (live_filtered || !restrictions) {
		sql = talloc_asprintf(mem_ctx,
			"SELECT m1.id, m1.message_id, m1.normalized_subject "
			"FROM messages m1 "
			"JOIN mailboxes mb1 ON mb1.id = m1.mailbox_id "
			"  AND mb1.folder_id = %"PRIu64" AND mb1.name = '%s' "
			"WHERE m1.message_type = '%s' "
			"UNION "
			"SELECT m2.id, m2.message_id, m2.normalized_subject "
			"FROM messages m2 "
			"JOIN folders f ON f.id = m2.folder_id "
			"  AND f.folder_id = %"PRIu64" "
			"JOIN mailboxes mb2 ON mb2.id = f.mailbox_id AND mb2.name = '%s' "
			"WHERE m2.message_type = '%s' "
			"UNION "
			"SELECT m.id, m.message_id, m.normalized_subject "
			"FROM messages m "
			"JOIN folders f ON f.id = m.folder_id "
			"  AND f.folder_id = %"PRIu64
			"  AND f.folder_class = '%s' " // FIXME ou_id
			"WHERE m2.message_type = '%s' ",
			table->folder_id, table->username, msg_type,
			table->folder_id, table->username, msg_type,
			table->folder_id, PUBLIC_FOLDER, msg_type);
	} else if (restrictions->res.resProperty.ulPropTag == PidTagMid) {
		id = strtoll(value, NULL, 10);
		sql = talloc_asprintf(mem_ctx,
			"SELECT m1.id, m1.message_id, m1.normalized_subject "
			"FROM messages m1 "
			"JOIN mailboxes mb1 ON mb1.id = m1.mailbox_id "
			"  AND mb1.folder_id = %"PRIu64" AND mb1.name = '%s' "
			"WHERE m1.message_type = '%s' "
			"  AND m1.message_id = %"PRIu64" "
			"UNION "
			"SELECT m2.id, m2.message_id, m2.normalized_subject "
			"FROM messages m2 "
			"JOIN folders f ON f.id = m2.folder_id "
			"  AND f.folder_id = %"PRIu64" "
			"JOIN mailboxes mb2 ON mb2.id = f.mailbox_id AND mb2.name = '%s' "
			"WHERE m2.message_type = '%s'"
			"  AND m2.message_id = %"PRIu64" "
			"UNION "
			"SELECT m.id, m.message_id, m.normalized_subject "
			"FROM messages m "
			"JOIN folders f ON f.id = m.folder_id " // FIXME ou_id
			"  AND f.folder_id = %"PRIu64" "
			"  AND f.folder_class = '%s' "
			"WHERE m.message_type = '%s'"
			"  AND m.message_id = %"PRIu64,
			table->folder_id, table->username, msg_type, id,
			table->folder_id, table->username, msg_type, id,
			table->folder_id, PUBLIC_FOLDER, msg_type, id);
	} else if (restrictions->res.resProperty.ulPropTag == PidTagNormalizedSubject) {
		sql = talloc_asprintf(mem_ctx,
			"SELECT m1.id, m1.message_id, m1.normalized_subject "
			"FROM messages m1 "
			"JOIN mailboxes mb1 ON mb1.id = m1.mailbox_id "
			"  AND mb1.folder_id = %"PRIu64" AND mb1.name = '%s' "
			"WHERE m1.message_type = '%s' "
			"  AND m1.normalized_subject = '%s' "
			"UNION "
			"SELECT m2.id, m2.message_id, m2.normalized_subject "
			"FROM messages m2 "
			"JOIN folders f ON f.id = m2.folder_id "
			"  AND f.folder_id = %"PRIu64" "
			"JOIN mailboxes mb2 ON mb2.id = f.mailbox_id AND mb2.name = '%s' "
			"WHERE m2.message_type = '%s'"
			"  AND m2.normalized_subject = '%s' "
			"UNION "
			"SELECT m.id, m.message_id, m.normalized_subject "
			"FROM messages m "
			"JOIN folders f ON f.id = m.folder_id "
			"  AND f.folder_id = %"PRIu64" "
			"  AND f.folder_class = '%s' " // FIXME ou_id
			"WHERE m.message_type = '%s'"
			"  AND m.normalized_subject = '%s'",
			table->folder_id, table->username, msg_type, value,
			table->folder_id, table->username, msg_type, value,
			table->folder_id, PUBLIC_FOLDER, msg_type, value);
	} else {
		sql = talloc_asprintf(mem_ctx,
			"SELECT m1.id, m1.message_id, m1.normalized_subject "
			"FROM messages m1 "
			"JOIN mailboxes mb1 ON mb1.id = m1.mailbox_id "
			"  AND mb1.folder_id = %"PRIu64" AND mb1.name = '%s' "
			"WHERE m1.message_type = '%s' "
			"  AND EXISTS ("
			"     SELECT mp.message_id FROM messages_properties mp "
			"     WHERE mp.message_id = m1.id "
			"       AND mp.name = '%s' AND mp.value = '%s') "
			"UNION "
			"SELECT m2.id, m2.message_id, m2.normalized_subject "
			"FROM messages m2 "
			"JOIN folders f ON f.id = m2.folder_id "
			"  AND f.folder_id = %"PRIu64" "
			"JOIN mailboxes mb2 ON mb2.id = f.mailbox_id AND mb2.name = '%s' "
			"WHERE m2.message_type = '%s'"
			"  AND EXISTS ("
			"     SELECT mp.message_id FROM messages_properties mp "
			"     WHERE mp.message_id = m2.id "
			"       AND mp.name = '%s' AND mp.value = '%s') "
			"UNION "
			"SELECT m.id, m.message_id, m.normalized_subject "
			"FROM messages m "
			"JOIN folders f ON f.id = m.folder_id "
			"  AND f.folder_id = %"PRIu64" "
			"  AND f.folder_class = '%s' " // FIXME ou_id
			"WHERE m.message_type = '%s'"
			"  AND EXISTS ("
			"     SELECT mp.message_id FROM messages_properties mp "
			"     WHERE mp.message_id = m.id "
			"       AND mp.name = '%s' AND mp.value = '%s')",
			table->folder_id, table->username, msg_type, attr, value,
			table->folder_id, table->username, msg_type, attr, value,
			table->folder_id, PUBLIC_FOLDER, msg_type, attr, value);
	}

	ret = status(select_without_fetch(conn, sql, &res));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	results = talloc_zero(table, struct openchangedb_table_results);
	table->res = results;
	results->count = mysql_num_rows(res);
	results->messages = talloc_array(results,
					 struct openchangedb_table_message_row *,
					 results->count);
	for (i = 0; i < results->count; i++) {
		msg_row = talloc_zero(results,
				      struct openchangedb_table_message_row);
		row = mysql_fetch_row(res);

		msg_row->id = strtoll(row[0], NULL, 0);
		msg_row->mid = strtoll(row[1], NULL, 0);
		msg_row->normalized_subject = talloc_strdup(results, row[2]);

		results->messages[i] = msg_row;
	}
	mysql_free_result(res);
	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS _table_fetch_folders(MYSQL *conn,
					    struct openchangedb_table *table,
					    bool live_filtered)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "_table_fetch_folders");
	char *sql;
	const char *attr, *value;
	MYSQL_RES *res;
	MYSQL_ROW row;
	enum MAPISTATUS ret;
	int64_t id;
	size_t i;
	struct openchangedb_table_results *results;
	struct openchangedb_table_folder_row *folder_row;
	struct mapi_SRestriction *restrictions = table->restrictions;

	ret = _table_get_attr_and_value_from_restrictions(mem_ctx, restrictions, &attr, &value);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	if (live_filtered || !restrictions) {
		sql = talloc_asprintf(mem_ctx,
			"SELECT f1.id, f1.folder_id FROM folders f1 "
			"JOIN folders f2 ON f2.id = f1.parent_folder_id "
			"   AND f2.folder_id = %"PRIu64" "
			"JOIN mailboxes mb1 ON mb1.id = f1.mailbox_id "
			"   AND mb1.name = '%s' "
			"UNION "
			"SELECT f3.id, f3.folder_id FROM folders f3 "
			"JOIN mailboxes mb2 ON mb2.id = f3.mailbox_id "
			"   AND mb2.folder_id = %"PRIu64" AND mb2.name = '%s' "
			"WHERE f3.parent_folder_id IS NULL "
			"UNION "
			"SELECT f1.id, f1.folder_id FROM folders f1 "
			"JOIN folders f2 ON f2.id = f1.parent_folder_id "
			"   AND f2.folder_id = %"PRIu64" "
			"WHERE f1.folder_class = '%s'", // FIXME ou_id
			table->folder_id, table->username,
			table->folder_id, table->username,
			table->folder_id, PUBLIC_FOLDER);
	} else if (restrictions->res.resProperty.ulPropTag == PidTagFolderId) {
		id = strtoll(value, NULL, 10);
		sql = talloc_asprintf(mem_ctx,
			"SELECT f1.id, f1.folder_id FROM folders f1 "
			"JOIN folders f2 ON f2.id = f1.parent_folder_id "
			"   AND f2.folder_id = %"PRIu64" "
			"JOIN mailboxes mb1 ON mb1.id = f1.mailbox_id "
			"   AND mb1.name = '%s' "
			"WHERE f1.folder_id = %"PRIu64" "
			"UNION "
			"SELECT f3.id, f3.folder_id FROM folders f3 "
			"JOIN mailboxes mb2 ON mb2.id = f3.mailbox_id "
			"   AND mb2.folder_id = %"PRIu64" AND mb2.name = '%s' "
			"WHERE f3.parent_folder_id IS NULL"
			"   AND f3.folder_id = %"PRIu64" "
			"UNION "
			"SELECT f1.id, f1.folder_id FROM folders f1 "
			"JOIN folders f2 ON f2.id = f1.parent_folder_id " // FIXME ou_id
			"   AND f2.folder_id = %"PRIu64" "
			"WHERE f1.folder_id = %"PRIu64" "
			"  AND f1.folder_class = '%s'",
			table->folder_id, table->username, id,
			table->folder_id, table->username, id,
			table->folder_id, id, PUBLIC_FOLDER);
	} else {
		sql = talloc_asprintf(mem_ctx,
			"SELECT f1.id, f1.folder_id FROM folders f1 "
			"JOIN folders f2 ON f2.id = f1.parent_folder_id "
			"   AND f2.folder_id = %"PRIu64" "
			"JOIN mailboxes mb ON mb.id = f1.mailbox_id "
			"   AND mb.name = '%s' "
			"WHERE EXISTS ("
			"     SELECT fp.folder_id FROM folders_properties fp "
			"     WHERE fp.folder_id = f1.id "
			"       AND fp.name = '%s' AND fp.value = '%s') "
			"UNION "
			"SELECT f.id, f.folder_id FROM folders f "
			"JOIN mailboxes mb ON mb.id = f.mailbox_id "
			"   AND mb.folder_id = %"PRIu64" AND mb.name = '%s' "
			"WHERE f.parent_folder_id IS NULL"
			"  AND EXISTS ("
			"     SELECT fp.folder_id FROM folders_properties fp "
			"     WHERE fp.folder_id = f.id "
			"       AND fp.name = '%s' AND fp.value = '%s') "
			"UNION "
			"SELECT f1.id, f1.folder_id FROM folders f1 "
			"JOIN folders f2 ON f2.id = f1.parent_folder_id "
			"   AND f2.folder_id = %"PRIu64" "
			"WHERE f1.folder_class = '%s' AND EXISTS (" // FIXME ou_id
			"     SELECT fp.folder_id FROM folders_properties fp "
			"     WHERE fp.folder_id = f1.id "
			"       AND fp.name = '%s' AND fp.value = '%s') ",
			table->folder_id, table->username, attr, value,
			table->folder_id, table->username, attr, value,
			table->folder_id, PUBLIC_FOLDER, attr, value);
	}

	ret = status(select_without_fetch(conn, sql, &res));
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	results = talloc_zero(table, struct openchangedb_table_results);
	table->res = results;
	results->count = mysql_num_rows(res);
	results->folders = talloc_array(results,
					struct openchangedb_table_folder_row *,
					results->count);
	for (i = 0; i < results->count; i++) {
		folder_row = talloc_zero(results,
					 struct openchangedb_table_folder_row);
		row = mysql_fetch_row(res);

		folder_row->id = strtoll(row[0], NULL, 0);
		folder_row->fid = strtoll(row[1], NULL, 0);

		results->folders[i] = folder_row;
	}
	mysql_free_result(res);
	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS _table_fetch_results(MYSQL *conn,
					    struct openchangedb_table *table,
					    bool live_filtered)
{
	bool is_message = table->table_type == 0x3 || table->table_type == 0x2;

	if (is_message) {
		bool fai = table->table_type == 0x3;
		return _table_fetch_messages(conn, table, fai, live_filtered);
	} else {
		return _table_fetch_folders(conn, table, live_filtered);
	}
}

static bool _table_check_message_match_restrictions(MYSQL *conn,
						    struct openchangedb_table *table,
						    bool fai,
						    struct openchangedb_table_message_row *row)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "_table_check_message_match_restrictions");
	char *sql, *msg_type;
	const char *attr, *value;
	struct mapi_SRestriction *restrictions = table->restrictions;
	int64_t id;
	bool ret;

	ret = _table_get_attr_and_value_from_restrictions(mem_ctx, restrictions, &attr, &value);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	if (restrictions->res.resProperty.ulPropTag == PidTagMid) {
		ret = row->mid == strtoll(value, NULL, 10);
		goto end;
	} else if (restrictions->res.resProperty.ulPropTag == PidTagNormalizedSubject) {
		ret = strcmp(row->normalized_subject, value) == 0;
		goto end;
	}

	msg_type = talloc_strdup(mem_ctx, fai ? "faiMessage" : "systemMessage");

	sql = talloc_asprintf(mem_ctx,
		"SELECT m1.id FROM messages m1 "
		"JOIN mailboxes mb1 ON mb1.id = m1.mailbox_id "
		"  AND mb1.folder_id = %"PRIu64" AND mb1.name = '%s' "
		"WHERE m1.message_type = '%s' "
		"  AND EXISTS ("
		"     SELECT mp.message_id FROM messages_properties mp "
		"     WHERE mp.message_id = m1.id "
		"       AND mp.name = '%s' AND mp.value = '%s') "
		"  AND m1.id = %"PRIu64" "
		"UNION "
		"SELECT m2.id FROM messages m2 "
		"JOIN folders f ON f.id = m2.folder_id "
		"  AND f.folder_id = %"PRIu64" "
		"JOIN mailboxes mb2 ON mb2.id = f.mailbox_id AND mb2.name = '%s' "
		"WHERE m2.message_type = '%s'"
		"  AND EXISTS ("
		"     SELECT mp.message_id FROM messages_properties mp "
		"     WHERE mp.message_id = m2.id "
		"       AND mp.name = '%s' AND mp.value = '%s') "
		"  AND m2.id = %"PRIu64" "
		"UNION "
		"SELECT m.id FROM messages m "
		"JOIN folders f ON f.id = m.folder_id "
		"  AND f.folder_id = %"PRIu64" "
		"  AND f.folder_class = '%s' " // FIXME ou_id
		"WHERE m2.message_type = '%s'"
		"  AND EXISTS ("
		"     SELECT mp.message_id FROM messages_properties mp "
		"     WHERE mp.message_id = m2.id "
		"       AND mp.name = '%s' AND mp.value = '%s') "
		"  AND m2.id = %"PRIu64,
		table->folder_id, table->username, msg_type, attr, value, row->id,
		table->folder_id, table->username, msg_type, attr, value, row->id,
		table->folder_id, PUBLIC_FOLDER, msg_type, attr, value, row->id);

	ret = status(select_first_int64(conn, sql, &id)) == MAPI_E_SUCCESS;
end:
	talloc_free(mem_ctx);
	return ret;
}

static bool _table_check_folder_match_restrictions(MYSQL *conn,
						   struct openchangedb_table *table,
						   struct openchangedb_table_folder_row *row)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "_table_check_folder_match_restrictions");
	char *sql;
	const char *attr, *value;
	struct mapi_SRestriction *restrictions = table->restrictions;
	int64_t id;
	bool ret;

	ret = _table_get_attr_and_value_from_restrictions(mem_ctx, restrictions, &attr, &value);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	if (restrictions->res.resProperty.ulPropTag == PidTagFolderId) {
		ret = row->fid == strtoll(value, NULL, 10);
		goto end;
	}

	sql = talloc_asprintf(mem_ctx,
		"SELECT f1.id FROM folders f1 "
		"JOIN folders f2 ON f2.id = f1.parent_folder_id "
		"   AND f2.folder_id = %"PRIu64" "
		"JOIN mailboxes mb1 ON mb1.id = f1.mailbox_id "
		"   AND mb1.name = '%s' "
		"WHERE EXISTS ("
		"     SELECT fp.folder_id FROM folders_properties fp "
		"     WHERE fp.folder_id = f1.id "
		"       AND fp.name = '%s' AND fp.value = '%s') "
		"   AND f1.id = %"PRIu64" "
		"UNION "
		"SELECT f3.id FROM folders f3 "
		"JOIN mailboxes mb2 ON mb2.id = f3.mailbox_id "
		"   AND mb2.folder_id = %"PRIu64" AND mb2.name = '%s' "
		"WHERE f3.parent_folder_id IS NULL"
		"  AND EXISTS ("
		"     SELECT fp.folder_id FROM folders_properties fp "
		"     WHERE fp.folder_id = f3.id "
		"       AND fp.name = '%s' AND fp.value = '%s') "
		"   AND f3.id = %"PRIu64" "
		"UNION "
		"SELECT f1.id FROM folders f1 "
		"JOIN folders f2 ON f2.id = f1.parent_folder_id "
		"   AND f2.folder_id = %"PRIu64" "
		"WHERE f1.folder_class = '%s' AND EXISTS (" // FIXME ou_id
		"     SELECT fp.folder_id FROM folders_properties fp "
		"     WHERE fp.folder_id = f1.id "
		"       AND fp.name = '%s' AND fp.value = '%s') "
		"   AND f1.id = %"PRIu64" ",
		table->folder_id, table->username, attr, value, row->id,
		table->folder_id, table->username, attr, value, row->id,
		table->folder_id, PUBLIC_FOLDER, attr, value, row->id);

	ret = status(select_first_int64(conn, sql, &id)) == MAPI_E_SUCCESS;
end:
	talloc_free(mem_ctx);
	return ret;
}

static bool _table_check_match_restrictions(MYSQL *conn,
					    struct openchangedb_table *table,
					    uint32_t pos)
{
	bool is_message = table->table_type == 0x3 || table->table_type == 0x2;

	if (!table->restrictions) return true;

	if (is_message) {
		bool fai = table->table_type == 0x3;
		return _table_check_message_match_restrictions(conn, table, fai, table->res->messages[pos]);
	} else {
		return _table_check_folder_match_restrictions(conn, table, table->res->folders[pos]);
	}
}

static const char *_table_fetch_message_attribute(MYSQL *conn,
						  struct openchangedb_table *table,
						  uint32_t pos,
						  enum MAPITAGS proptag)
{
	const char *value = NULL;
	struct openchangedb_table_message_row *row = table->res->messages[pos];

	if (proptag == PidTagMid) {
		value = talloc_asprintf(table->res, "%"PRIu64, row->mid);
	} else if (proptag == PidTagNormalizedSubject) {
		value = talloc_strdup(table->res, row->normalized_subject);
	} else {
		const char *attr = openchangedb_property_get_attribute(proptag);
		char *sql = talloc_asprintf(NULL,
			"SELECT mp.value FROM messages_properties mp "
			"WHERE mp.message_id = %"PRIu64" AND mp.name = '%s'",
			row->id, attr);
		if (!attr) return NULL;
		select_first_string(table->res, conn, sql, &value);
		talloc_free(sql);
	}

	return (const char *)talloc_strdup(table->res, value);
}

static const char *_table_fetch_folder_attribute(MYSQL *conn,
						 struct openchangedb_table *table,
						 uint32_t pos,
						 enum MAPITAGS proptag)
{
	const char *value = NULL;
	struct openchangedb_table_folder_row *row = table->res->folders[pos];

	if (proptag == PidTagFolderId) {
		value = talloc_asprintf(table->res, "%"PRIu64, row->fid);
	} else {
		const char *attr = openchangedb_property_get_attribute(proptag);
		char *sql = talloc_asprintf(NULL,
			"SELECT fp.value FROM folders_properties fp "
			"WHERE fp.folder_id = %"PRIu64" AND fp.name = '%s'",
			row->id, attr);
		if (!attr) return NULL;
		select_first_string(table->res, conn, sql, &value);
		talloc_free(sql);
	}

	return (const char *)talloc_strdup(table->res, value);
}

static const char *_table_fetch_attribute(MYSQL *conn,
					  struct openchangedb_table *table,
					  uint32_t pos, enum MAPITAGS proptag)
{
	bool is_message = table->table_type == 0x3 || table->table_type == 0x2;

	if (is_message) {
		return _table_fetch_message_attribute(conn, table, pos, proptag);
	} else {
		return _table_fetch_folder_attribute(conn, table, pos, proptag);
	}
}

static enum MAPISTATUS table_get_property(TALLOC_CTX *mem_ctx,
					  struct openchangedb_context *self,
					  void *_table,
					  enum MAPITAGS proptag, uint32_t pos,
					  bool live_filtered, void **data)
{
	struct openchangedb_table *table = (struct openchangedb_table *)_table;
	const char *value;
	enum MAPISTATUS ret;
	MYSQL *conn = self->data;
	struct openchangedb_table_results *res;
	uint32_t *id;

	/* Fetch results */
	if (!table->res) {
		ret = _table_fetch_results(conn, table, live_filtered);
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, NULL);
	}
	res = table->res;

	// Ensure position is within search results range
	OPENCHANGE_RETVAL_IF(pos >= res->count, MAPI_E_INVALID_OBJECT, NULL);

	/* If live filtering, make sure the specified row match the restrictions */
	if (live_filtered) {
		if (!_table_check_match_restrictions(conn, table, pos)) {
			return MAPI_E_INVALID_OBJECT;
		}
	}

	// workarounds for some specific attributes
	if (proptag == PR_INST_ID) {
		proptag = table->table_type == 1 ? PR_FID : PR_MID;
	} else if (proptag == PR_INSTANCE_NUM) {
		id = talloc_zero(mem_ctx, uint32_t);
		*data = id;
		goto end;
	}

	if ((table->table_type != 0x1) && proptag == PR_FID) {
		id = talloc_zero(mem_ctx, uint32_t);
		*id = table->folder_id;
		*data = id;
		goto end;
	}

	// Check if this is a "special property"
	*data = _get_special_property(mem_ctx, proptag);
	if (*data) goto end;

	value = _table_fetch_attribute(conn, table, pos, proptag);
	OPENCHANGE_RETVAL_IF(value == NULL, MAPI_E_NOT_FOUND, NULL);

	*data = get_property_data(mem_ctx, proptag, value);
	OPENCHANGE_RETVAL_IF(*data == NULL, MAPI_E_NOT_FOUND, NULL);
end:
	return MAPI_E_SUCCESS;
}

// ^ openchangedb table -------------------------------------------------------

// v openchangedb message -----------------------------------------------------

// TODO types
enum openchangedb_message_type {
	OPENCHANGEDB_MESSAGE_SYSTEM	= 0x1,
	OPENCHANGEDB_MESSAGE_FAI	= 0x2
};

struct openchangedb_message_properties {
	const char 	**names;
	const char 	**values;
	size_t 		size;
};

struct openchangedb_message {
	int64_t					id; // id from database
	int64_t					ou_id;
	int64_t					message_id;
	enum openchangedb_message_type		message_type;
	int64_t					folder_id; // id from database
	int64_t					mailbox_id;
	char					*normalized_subject;
	struct openchangedb_message_properties	properties;
};

/**
 * Return the database id field of a folder identified by folder_id and
 * mailbox's name
 */
static enum MAPISTATUS get_id_from_folder_id(MYSQL *conn, const char *username,
					      int64_t fid, int64_t *folder_id)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "get_folder_id_from_fid");
	enum MAPISTATUS ret;
	char *sql;

	if (is_public_folder(fid)) {
		sql = talloc_asprintf(mem_ctx,
			"SELECT f.id FROM folders f "
			"WHERE f.folder_id = %"PRIu64" " // FIXME ou_id
			"  AND f.folder_class = '%s'",
			fid, PUBLIC_FOLDER);
	} else {
		sql = talloc_asprintf(mem_ctx, // FIXME ou_id
			"SELECT f.id FROM folders f "
			"JOIN mailboxes m ON m.id = f.mailbox_id AND m.name = '%s' "
			"WHERE f.folder_id = %"PRIu64,
			_sql(mem_ctx, username), fid);
	}

	ret = status(select_first_int64(conn, sql, folder_id));

	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS message_create(TALLOC_CTX *parent_ctx,
				      struct openchangedb_context *self,
				      const char *username,
				      int64_t message_id, int64_t fid,
				      bool fai, void **message_object)
{
	int64_t mailbox_id = 0, mailbox_folder_id, folder_id;
	struct openchangedb_message *msg;
	enum MAPISTATUS ret;
	MYSQL *conn = self->data;
	bool parent_is_mailbox = false;

	ret = get_mailbox_ids_by_name(conn, username, &mailbox_id,
				      &mailbox_folder_id);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, NULL);

	parent_is_mailbox = mailbox_folder_id == fid;
	if (!parent_is_mailbox) {
		ret = get_id_from_folder_id(conn, username, fid, &folder_id);
		OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, NULL);
	}

	msg = talloc_zero(parent_ctx, struct openchangedb_message);
	OPENCHANGE_RETVAL_IF(!msg, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
	msg->id = 0; // Only new records will have id equal to 0
	// TODO ou_id
	msg->message_id = message_id;
	msg->message_type = fai ? OPENCHANGEDB_MESSAGE_FAI : OPENCHANGEDB_MESSAGE_SYSTEM;
	if (!parent_is_mailbox)
		msg->folder_id = folder_id;
	if (mailbox_id)
		msg->mailbox_id = mailbox_id;

	msg->properties.size = 4;
	msg->properties.names = (const char **)talloc_zero_array(msg, char *, msg->properties.size);
	msg->properties.values = (const char **)talloc_zero_array(msg, char *, msg->properties.size);
	// Add required properties as described in [MS_OXCMSG] 3.2.5.2
	msg->properties.names[0] = talloc_strdup(msg, "PidTagDisplayBcc");
	msg->properties.names[1] = talloc_strdup(msg, "PidTagDisplayCc");
	msg->properties.names[2] = talloc_strdup(msg, "PidTagDisplayTo");
	msg->properties.names[3] = talloc_strdup(msg, "PidTagHasNamedProperties");
	msg->properties.values[0] = talloc_strdup(msg, "");
	msg->properties.values[1] = talloc_strdup(msg, "");
	msg->properties.values[2] = talloc_strdup(msg, "");
	msg->properties.values[3] = talloc_strdup(msg, "0");

	*message_object = (void *)msg;

	return MAPI_E_SUCCESS;
}

static char *_message_type(TALLOC_CTX *mem_ctx, enum openchangedb_message_type msg_type)
{
	if (msg_type == OPENCHANGEDB_MESSAGE_SYSTEM) {
		return talloc_strdup(mem_ctx, "systemMessage");
	} else if (msg_type == OPENCHANGEDB_MESSAGE_FAI) {
		return talloc_strdup(mem_ctx, "faiMessage");
	} else {
		DEBUG(0, ("Bug: Unknown message_type"));
		return NULL;
	}
}

static enum MAPISTATUS message_save(struct openchangedb_context *self,
				    void *_msg, uint8_t save_flags)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "message_save");
	struct openchangedb_message *msg = (struct openchangedb_message *)_msg;
	char *sql;
	int64_t i;
	enum MAPISTATUS ret;
	const char **fields = (const char **)str_list_make_empty(mem_ctx);

	transaction_start(self);

	if (msg->id) {
		// Update, we don't need to update the properties, that's done
		// with another function
		// TODO ou_id
		fields = str_list_add(fields, talloc_asprintf(mem_ctx,
			"message_id=%"PRIu64, msg->message_id));
		fields = str_list_add(fields, talloc_asprintf(mem_ctx,
			"message_type='%s'",
			_message_type(mem_ctx, msg->message_type)));
		if (msg->folder_id) {
			fields = str_list_add(fields, talloc_asprintf(mem_ctx,
				"folder_id=%"PRIu64, msg->folder_id));
		} else {
			fields = str_list_add(fields, talloc_strdup(mem_ctx,
				"folder_id=NULL"));
		}
		if (msg->mailbox_id) {
			fields = str_list_add(fields, talloc_asprintf(mem_ctx,
				"mailbox_id=%"PRIu64, msg->mailbox_id));
		} else {
			fields = str_list_add(fields, talloc_strdup(mem_ctx,
				"mailbox_id=NULL"));
		}
		if (msg->normalized_subject) {
			fields = str_list_add(fields, talloc_asprintf(mem_ctx,
				"normalized_subject='%s'",
				_sql(mem_ctx, msg->normalized_subject)));
		}
		sql = talloc_asprintf(mem_ctx,
			"UPDATE messages SET %s WHERE id=%"PRIu64,
			str_list_join(mem_ctx, fields, ','), msg->id);
		ret = status(execute_query(conn, sql));
	} else {
		// Create message row and all its properties
		// TODO ou_id
		fields = str_list_add(fields, talloc_asprintf(mem_ctx,
			"message_id=%"PRIu64, msg->message_id));
		fields = str_list_add(fields, talloc_asprintf(mem_ctx,
			"message_type='%s'",
			_message_type(mem_ctx, msg->message_type)));
		if (msg->folder_id) {
			fields = str_list_add(fields, talloc_asprintf(mem_ctx,
				"folder_id=%"PRIu64, msg->folder_id));
		} else {
			fields = str_list_add(fields, talloc_asprintf(mem_ctx,
				"folder_id=NULL"));
		}
		if (msg->mailbox_id) {
			fields = str_list_add(fields, talloc_asprintf(mem_ctx,
				"mailbox_id=%"PRIu64, msg->mailbox_id));
		} else {
			fields = str_list_add(fields, talloc_asprintf(mem_ctx,
				"mailbox_id=NULL"));
		}
		if (msg->normalized_subject) {
			fields = str_list_add(fields, talloc_asprintf(mem_ctx,
				"normalized_subject='%s'",
				_sql(mem_ctx, msg->normalized_subject)));
		}
		sql = talloc_asprintf(mem_ctx, "INSERT INTO messages SET %s",
				      str_list_join(mem_ctx, fields, ','));
		ret = status(execute_query(conn, sql));
		if (ret != MAPI_E_SUCCESS)
			goto end;
		msg->id = mysql_insert_id(conn);
	}

	// Delete all properties, we are gonna insert now so...
	sql = talloc_asprintf(mem_ctx,
		"DELETE FROM messages_properties WHERE message_id = %"PRIu64,
		msg->id);
	ret = status(execute_query(conn, sql));
	if (ret != MAPI_E_SUCCESS)
		goto end;

	// Insert properties
	for (i = 0; i < msg->properties.size; i++) {
		sql = talloc_asprintf(mem_ctx,
			"INSERT INTO messages_properties (message_id, name, value) "
			"VALUES (%"PRIu64", '%s', '%s')", msg->id,
			_sql(mem_ctx, msg->properties.names[i]),
			_sql(mem_ctx, msg->properties.values[i]));
		ret = status(execute_query(conn, sql));
		if (ret != MAPI_E_SUCCESS)
			goto end;
	}
end:
	if (ret == MAPI_E_SUCCESS) {
		transaction_commit(self);
	} else {
		transaction_rollback(self);
	}

	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS message_open(TALLOC_CTX *parent_ctx,
				    struct openchangedb_context *self,
				    const char *username,
				    int64_t message_id, int64_t folder_id,
				    void **message_object, void **msgp)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "message_open");
	struct openchangedb_message *msg;
	struct mapistore_message *mmsg;
	char *sql;
	MYSQL *conn = self->data;
	MYSQL_RES *res;
	MYSQL_ROW row;
	enum MAPISTATUS ret;
	size_t i;
	int64_t mailbox_id = 0, mailbox_folder_id;

	msg = talloc_zero(parent_ctx, struct openchangedb_message);
	msg->message_id = message_id;

	ret = get_mailbox_ids_by_name(conn, username, &mailbox_id,
				      &mailbox_folder_id);
	OPENCHANGE_RETVAL_IF(ret != MAPI_E_SUCCESS, ret, mem_ctx);

	if (folder_id == mailbox_folder_id) {
		sql = talloc_asprintf(mem_ctx,
			"SELECT m.id, m.ou_id, m.message_type, m.mailbox_id, "
			       "m.folder_id, m.normalized_subject "
			"FROM messages m "
			"WHERE m.message_id = %"PRIu64
			"  AND m.mailbox_id = %"PRIu64
			"  AND m.folder_id IS NULL", // FIXME ou_id
			message_id, mailbox_id);
	} else {
		sql = talloc_asprintf(mem_ctx,
			"SELECT m.id, m.ou_id, m.message_type, m.mailbox_id, "
			       "m.folder_id, m.normalized_subject "
			"FROM messages m "
			"JOIN folders f ON f.id = m.folder_id "
			"  AND f.folder_id = %"PRIu64" "
			"WHERE m.message_id = %"PRIu64, // FIXME ou_id
			folder_id, message_id);
	}
	ret = status(select_without_fetch(conn, sql, &res));
	if (ret != MAPI_E_SUCCESS) {
		talloc_free(msg);
		OPENCHANGE_RETVAL_ERR(ret, mem_ctx);
	}
	row = mysql_fetch_row(res);
	msg->id = strtoll(row[0], NULL, 10);
	//msg->ou_id = strtoll(row[1], NULL, 10);
	if (row[2] && strncmp(row[2], "fai", 3) == 0 && strlen(row[2]) == 3) {
		msg->message_type = OPENCHANGEDB_MESSAGE_FAI;
	} else {
		msg->message_type = OPENCHANGEDB_MESSAGE_SYSTEM;
	}
	if (row[3]) {
		msg->mailbox_id = strtoll(row[3], NULL, 10);
	}
	if (row[4]) {
		msg->folder_id = strtoll(row[4], NULL, 10);
	}
	msg->normalized_subject = talloc_strdup(msg, row[5]);
	mysql_free_result(res);

	// Now fetch all properties
	sql = talloc_asprintf(mem_ctx,
		"SELECT name, value FROM messages_properties "
		"WHERE message_id = %"PRIu64, msg->id);
	ret = status(select_without_fetch(conn, sql, &res));
	if (ret != MAPI_E_SUCCESS) {
		talloc_free(msg);
		OPENCHANGE_RETVAL_ERR(ret, mem_ctx);
	}
	msg->properties.size = mysql_num_rows(res);
	msg->properties.names = (const char **)talloc_zero_array(msg, char *, msg->properties.size);
	msg->properties.values = (const char **)talloc_zero_array(msg, char *, msg->properties.size);
	for (i = 0; i < msg->properties.size; i++) {
		row = mysql_fetch_row(res);
		msg->properties.names[i] = talloc_strdup(msg, row[0]);
		msg->properties.values[i] = talloc_strdup(msg, row[1]);
	}
	mysql_free_result(res);
	*message_object = (void *)msg;

	if (msgp) {
		mmsg = talloc_zero(parent_ctx, struct mapistore_message);
		mmsg->subject_prefix = NULL;
		mmsg->normalized_subject = talloc_strdup(mmsg, msg->normalized_subject);
		mmsg->columns = NULL;
		mmsg->recipients_count = 0;
		mmsg->recipients = NULL;
		*msgp = (void *)mmsg;
	}

	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS message_get_property(TALLOC_CTX *parent_ctx,
					    struct openchangedb_context *self,
					    void *_msg, uint32_t proptag,
					    void **data)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "message_get_property");
	struct openchangedb_message *msg = (struct openchangedb_message *)_msg;
	char *sql;
	size_t attr_len;
	const char *attr, *value = NULL;
	MYSQL *conn = self->data;
	enum MAPISTATUS ret;
	int64_t *id;
	size_t i;

	// Special properties (they are on messages table instead of
	// messages_properties table):
	//     PidTagParentFolderId -> folder_id
	//     PidTagMessageId -> message_id
	//     PidTagNormalizedSubject -> normalized_subject
	if (proptag == PidTagParentFolderId) {
		if (msg->folder_id) {
			sql = talloc_asprintf(mem_ctx,
				"SELECT f.folder_id FROM folders f "
				"WHERE f.id = %"PRIu64,
				msg->folder_id);
		} else if (msg->mailbox_id) {
			sql = talloc_asprintf(mem_ctx,
				"SELECT m.folder_id FROM mailboxes m "
				"WHERE m.id = %"PRIu64,
				msg->mailbox_id);
		} else {
			DEBUG(5, ("Message without neither folder_id nor "
				  "mailbox_id"));
			ret = MAPI_E_CORRUPT_DATA;
			goto end;
		}
		id = talloc_zero(parent_ctx, int64_t);
		ret = status(select_first_int64(conn, sql, id));
		if (ret == MAPI_E_SUCCESS)
			*data = id;
		goto end;
	} else if (proptag == PidTagMid) {
		ret = MAPI_E_SUCCESS;
		id = talloc_zero(parent_ctx, int64_t);
		*id = msg->message_id;
		*data = id;
		goto end;
	} else if (proptag == PidTagNormalizedSubject) {
		ret = MAPI_E_SUCCESS;
		*data = talloc_strdup(parent_ctx, msg->normalized_subject);
		goto end;
	}

	ret = MAPI_E_NOT_FOUND;
	// Look for property in properties struct
	attr = openchangedb_property_get_attribute(proptag);
	if (!attr) {
		attr = _unknown_property(mem_ctx, proptag);
	}
	attr_len = strlen(attr);
	for (i = 0; i < msg->properties.size; i++) {
		if (strncmp(msg->properties.names[i], attr, attr_len) == 0) {
			value = msg->properties.values[i];
			break;
		}
	}
	// check if we have a value for proptag requested
	if (value != NULL) {
		// Transform string into the expected data type
		*data = get_property_data(parent_ctx, proptag, value);
		ret = MAPI_E_SUCCESS;
	}

end:
	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS message_set_properties(TALLOC_CTX *parent_ctx,
					      struct openchangedb_context *self,
					      void *_msg, struct SRow *row)
{
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "message_set_properties");
	struct openchangedb_message *msg = (struct openchangedb_message *)_msg;
	const char *attr;
	char *str_value;
	enum MAPITAGS tag;
	struct SPropValue *value;
	size_t i, j;
	bool found;

	for (i = 0; i < row->cValues; i++) {
		value = row->lpProps + i;
		tag = value->ulPropTag;
		if (tag == PR_DEPTH || tag == PR_SOURCE_KEY ||
		    tag == PR_PARENT_SOURCE_KEY || tag == PR_CHANGE_KEY) {
			DEBUG(5, ("Ignored attempt to set handled property %.8x\n",
				  tag));
			break;
		}
		attr = openchangedb_property_get_attribute(tag);
		if (!attr) {
			attr = _unknown_property(mem_ctx, tag);
		}

		str_value = openchangedb_set_folder_property_data(mem_ctx, value);
		if (!str_value) {
			DEBUG(5, ("Ignored property of unhandled type %.4x\n",
				  (tag & 0xffff)));
			continue;
		}

		// Special properties
		if (tag == PidTagNormalizedSubject) {
			msg->normalized_subject = talloc_strdup(msg, str_value);
			continue;
		}

		found = false;
		for (j = 0; j < msg->properties.size; j++) {
			if (strncmp(attr, msg->properties.names[j], strlen(attr)) == 0) {
				found = true;
				msg->properties.values[j] = talloc_strdup(msg, str_value);
			}
		}

		if (!found) {
			msg->properties.size++;
			msg->properties.names = (const char **)talloc_realloc(msg, msg->properties.names, char *, msg->properties.size);
			msg->properties.values = (const char **)talloc_realloc(msg, msg->properties.values, char *, msg->properties.size);
			msg->properties.names[msg->properties.size-1] = talloc_strdup(msg, attr);
			msg->properties.values[msg->properties.size-1] = talloc_strdup(msg, str_value);
		}
	}

	return MAPI_E_SUCCESS;
}

// ^ openchangedb message -----------------------------------------------------

static const char *openchangedb_data_dir(void)
{
	return OPENCHANGEDB_DATA_DIR; // defined on compilation time
}

_PUBLIC_
enum MAPISTATUS openchangedb_mysql_initialize(TALLOC_CTX *mem_ctx,
					      struct loadparm_context *lp_ctx,
					      struct openchangedb_context **ctx)
{
	struct openchangedb_context 	*oc_ctx;
	char 				*schema_file;
	const char			*connection_string;
	const char			*schema_dir;

	oc_ctx = talloc_zero(mem_ctx, struct openchangedb_context);
	// Initialize context with function pointers
	oc_ctx->backend_type = talloc_strdup(oc_ctx, "mysql");

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
	oc_ctx->get_ReceiveFolderTable = get_ReceiveFolderTable;
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

	connection_string = lpcfg_parm_string(lp_ctx, NULL, "mapiproxy", "openchangedb");
	if (!connection_string) {
		DEBUG(0, ("[%s:%d] mapiproxy:openchangedb must be defined",
			  __FUNCTION__, __LINE__));
		OPENCHANGE_RETVAL_ERR(MAPI_E_INVALID_PARAMETER, oc_ctx);
	}
	// Connect to mysql
	oc_ctx->data = create_connection(connection_string, &conn);
	OPENCHANGE_RETVAL_IF(!oc_ctx->data, MAPI_E_NOT_INITIALIZED, oc_ctx);
	if (!table_exists(oc_ctx->data, "folders")) {
		bool schema_created;
		DEBUG(0, ("Creating schema for openchangedb on mysql %s",
			  connection_string));

		schema_dir = lpcfg_parm_string(lp_ctx, NULL, "openchangedb", "data");
		schema_file = talloc_asprintf(mem_ctx, "%s/"SCHEMA_FILE,
					      schema_dir ? schema_dir :
					      openchangedb_data_dir());
		schema_created = create_schema(oc_ctx->data, schema_file);
		talloc_free(schema_file);

		OPENCHANGE_RETVAL_IF(!schema_created, MAPI_E_NOT_INITIALIZED,
				     oc_ctx);
	}

	*ctx = oc_ctx;

	return MAPI_E_SUCCESS;
}
