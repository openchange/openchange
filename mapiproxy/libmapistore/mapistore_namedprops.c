/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010

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

#include "mapistore.h"
#include "mapistore_errors.h"
#include "mapistore_private.h"
#include "libmapi/libmapi_private.h"
#include <ldb.h>

#include <sys/stat.h>

static struct MAPINAMEID	**nameids_cache = NULL;

static const char *mapistore_namedprops_get_ldif_path(void)
{
	return MAPISTORE_LDIF;
}

/**
   \details Initialize the named properties database or return pointer
   to the existing one if already initialized/opened.

   \param mem_ctx pointer to the memory context
   \param ldb_ctx pointer on pointer to the ldb context the function
   returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum mapistore_error mapistore_namedprops_init(TALLOC_CTX *mem_ctx, struct ldb_context **_ldb_ctx)
{
	int			ret;
	struct stat		sb;
	struct ldb_context	*ldb_ctx = NULL;
	struct ldb_ldif		*ldif;
	char			*filename;
	FILE			*f;
	struct tevent_context	*ev;
	char			*database;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!_ldb_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ev = tevent_context_init(mem_ctx);
	MAPISTORE_RETVAL_IF(!ev, MAPISTORE_ERR_NO_MEMORY, NULL);

	database = talloc_asprintf(mem_ctx, "%s/%s", mapistore_get_mapping_path(), MAPISTORE_DB_NAMED);
	DEBUG(0, ("database = %s\n", database));

	/* Step 1. Stat the database and populate it if it doesn't exist */
	if (stat(database, &sb) == -1) {
		ldb_ctx = mapistore_ldb_wrap_connect(ldb_ctx, ev, database, 0);
		talloc_free(database);
		MAPISTORE_RETVAL_IF(!ldb_ctx, MAPISTORE_ERR_DATABASE_INIT, NULL);

		filename = talloc_asprintf(mem_ctx, "%s/mapistore_namedprops.ldif", 
					   mapistore_namedprops_get_ldif_path());
		f = fopen(filename, "r");
		talloc_free(filename);
		MAPISTORE_RETVAL_IF(!f, MAPISTORE_ERROR, NULL);
		
		ldb_transaction_start(ldb_ctx);

		while ((ldif = ldb_ldif_read_file(ldb_ctx, f))) {
			struct ldb_message *normalized_msg;
			ret = ldb_msg_normalize(ldb_ctx, mem_ctx, ldif->msg, &normalized_msg);
			MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERR_DATABASE_INIT, NULL);
			ret = ldb_add(ldb_ctx, normalized_msg);
			talloc_free(normalized_msg);
			if (ret != LDB_SUCCESS) {
				fclose(f);
				MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERR_DATABASE_INIT, NULL);
			}
			ldb_ldif_read_free(ldb_ctx, ldif);
		}

		ldb_transaction_commit(ldb_ctx);
		fclose(f);

	} else {
		ldb_ctx = mapistore_ldb_wrap_connect(ldb_ctx, ev, database, 0);
		talloc_free(database);
		MAPISTORE_RETVAL_IF(!ldb_ctx, MAPISTORE_ERR_DATABASE_INIT, NULL);
	}

	*_ldb_ctx = ldb_ctx;

	return MAPISTORE_SUCCESS;
}


/**
   \details return the next unmapped property ID

   \param ldb_ctx pointer to the namedprops ldb context

   \return 0 on error, the next mapped id otherwise
 */
_PUBLIC_ uint16_t mapistore_namedprops_next_unused_id(struct ldb_context *ldb_ctx)
{
	uint16_t		highest_id = 0, current_id;
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "mappedId", NULL };
	int			ret;
	unsigned int		i;

	mem_ctx = talloc_named(NULL, 0, "mapistore_namedprops_get_mapped_propID");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(cn=*)");
	MAPISTORE_RETVAL_IF(ret != LDB_SUCCESS, 0, mem_ctx);

	for (i = 0; i < res->count; i++) {
		current_id = ldb_msg_find_attr_as_uint(res->msgs[i], "mappedId", 0);
		if (current_id > 0 && highest_id < current_id) {
			highest_id = current_id;
		}
	}

	talloc_free(mem_ctx);

	DEBUG(5, ("next_mapped_id: %d\n", (highest_id + 1)));

	return (highest_id + 1);
}

/**
   \details return the mapped property ID matching the nameid
   structure passed in parameter.

   \param ldb_ctx pointer to the namedprops ldb context
   \param nameid the MAPINAMEID structure to lookup
   \param propID pointer to the property ID the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
_PUBLIC_ enum mapistore_error mapistore_namedprops_create_id(struct ldb_context *ldb_ctx, struct MAPINAMEID nameid, uint16_t mapped_id)
{
	int ret;
	TALLOC_CTX *mem_ctx;
	char *ldif_record;
	struct ldb_ldif *ldif;
	char *hex_id, *dec_id, *dec_mappedid, *guid;
	struct ldb_message *normalized_msg;
	const char *ldif_records[] = { NULL, NULL };

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	dec_mappedid = talloc_asprintf(mem_ctx, "%u", mapped_id);
	guid = GUID_string(mem_ctx, &nameid.lpguid);
	switch (nameid.ulKind) {
	case MNID_ID:
		hex_id = talloc_asprintf(mem_ctx, "%.4x", nameid.kind.lid);
		dec_id = talloc_asprintf(mem_ctx, "%u", nameid.kind.lid);
		ldif_record = talloc_asprintf(mem_ctx, "dn: CN=0x%s,CN=%s,CN=default\nobjectClass: MNID_ID\ncn: 0x%s\npropType: PT_NULL\noleguid: %s\nmappedId: %s\npropId: %s\n",
					      hex_id, guid, hex_id, guid, dec_mappedid, dec_id);
		break;
	case MNID_STRING:
		ldif_record = talloc_asprintf(mem_ctx, "dn: CN=%s,CN=%s,CN=default\nobjectClass: MNID_STRING\ncn: %s\npropType: PT_NULL\noleguid: %s\nmappedId: %s\npropName: %s\n",
					      nameid.kind.lpwstr.Name, guid, nameid.kind.lpwstr.Name, guid, dec_mappedid, nameid.kind.lpwstr.Name);
		break;
	default:
		abort();
	}

	DEBUG(5, ("inserting record:\n%s\n", ldif_record));
	ldif_records[0] = ldif_record;
	ldif = ldb_ldif_read_string(ldb_ctx, ldif_records);
	ret = ldb_msg_normalize(ldb_ctx, mem_ctx, ldif->msg, &normalized_msg);
	MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERR_DATABASE_INIT, NULL);
	ret = ldb_add(ldb_ctx, normalized_msg);
	talloc_free(normalized_msg);
	if (ret != LDB_SUCCESS) {
		MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERR_DATABASE_INIT, NULL);
	}

	/* we invalidate the cache, if present */
	if (nameids_cache) {
		talloc_free(nameids_cache);
		nameids_cache = NULL;
	}

	return ret;
}

/**
   \details return the mapped property ID matching the nameid
   structure passed in parameter.

   \param ldb_ctx pointer to the namedprops ldb context
   \param nameid the MAPINAMEID structure to lookup
   \param propID pointer to the property ID the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
_PUBLIC_ enum mapistore_error mapistore_namedprops_get_mapped_id(struct ldb_context *ldb_ctx, struct MAPINAMEID nameid, uint16_t *propID)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	int			ret;
	char			*filter = NULL;
	char			*guid;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ldb_ctx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!propID, MAPISTORE_ERROR, NULL);

	*propID = 0;
	mem_ctx = talloc_named(NULL, 0, "mapistore_namedprops_get_mapped_propID");
	guid = GUID_string(mem_ctx, (const struct GUID *)&nameid.lpguid);

	switch (nameid.ulKind) {
	case MNID_ID:
		filter = talloc_asprintf(mem_ctx, "(&(objectClass=MNID_ID)(oleguid=%s)(cn=0x%.4x))",
					 guid, nameid.kind.lid);
		break;
	case MNID_STRING:
		filter = talloc_asprintf(mem_ctx, "(&(objectClass=MNID_STRING)(oleguid=%s)(cn=%s))",
					 guid, nameid.kind.lpwstr.Name);
		break;
	}
	talloc_free(guid);

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "%s", filter);
	MAPISTORE_RETVAL_IF((ret != LDB_SUCCESS || !res->count), MAPISTORE_ERROR, mem_ctx);

	*propID = ldb_msg_find_attr_as_uint(res->msgs[0], "mappedId", 0);
	MAPISTORE_RETVAL_IF(!*propID, MAPISTORE_ERROR, mem_ctx);

	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}

/**
   \details return the nameid structture matching the mapped property ID
   passed in parameter.

   \param ldb_ctx pointer to the namedprops ldb context
   \param propID the property ID to lookup
   \param nameid pointer to the MAPINAMEID structure the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
_PUBLIC_ enum mapistore_error mapistore_namedprops_get_nameid(struct ldb_context *ldb_ctx, 
							      uint16_t propID,
							      struct MAPINAMEID **nameidp)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	const char			*guid, *oClass, *cn;
        struct MAPINAMEID		*nameid;
	int				rc = MAPISTORE_SUCCESS;
	uint16_t			propidx;
					     
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ldb_ctx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!nameidp, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(propID < 0x8000, MAPISTORE_ERROR, NULL);

	if (!nameids_cache) {
		nameids_cache = talloc_array(NULL, struct MAPINAMEID *, 0x8000);
		memset(nameids_cache, 0, 0x8000 * sizeof (struct MAPINAMEID *));
	}

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	propidx = propID - 0x8000;
	nameid = nameids_cache[propidx];
	if (nameid) {
		*nameidp = nameid;
		return MAPISTORE_SUCCESS;
	}

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(mappedId=%d)", propID);
	MAPISTORE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPISTORE_ERROR, mem_ctx);

	guid = ldb_msg_find_attr_as_string(res->msgs[0], "oleguid", 0);
	MAPISTORE_RETVAL_IF(!guid, MAPISTORE_ERROR, mem_ctx);

	cn = ldb_msg_find_attr_as_string(res->msgs[0], "cn", 0);
	MAPISTORE_RETVAL_IF(!cn, MAPISTORE_ERROR, mem_ctx);

	oClass = ldb_msg_find_attr_as_string(res->msgs[0], "objectClass", 0);
	MAPISTORE_RETVAL_IF(!propID, MAPISTORE_ERROR, mem_ctx);

	nameid = talloc_zero(nameids_cache, struct MAPINAMEID);
	GUID_from_string(guid, &nameid->lpguid);
	if (strcmp(oClass, "MNID_ID") == 0) {
		nameid->ulKind = MNID_ID;
		nameid->kind.lid = strtol(cn, NULL, 16);
	}
	else if (strcmp(oClass, "MNID_STRING") == 0) {
		nameid->ulKind = MNID_STRING;
		nameid->kind.lpwstr.NameSize = strlen(cn) * 2 + 2;
		nameid->kind.lpwstr.Name = talloc_strdup(nameid, cn);
	}
	else {
		talloc_unlink(nameids_cache, nameid);
		nameid = NULL;
		rc = MAPISTORE_ERROR;
	}

	if (!rc) {
		nameids_cache[propidx] = nameid;
		*nameidp = nameid;
	}

	talloc_free(mem_ctx);

	return rc;
}
