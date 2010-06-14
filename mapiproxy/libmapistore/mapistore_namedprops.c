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
#include <libmapi/defs_private.h>
#include <ldb.h>


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
int mapistore_namedprops_init(TALLOC_CTX *mem_ctx, void **_ldb_ctx)
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

		while ((ldif = ldb_ldif_read_file(ldb_ctx, f))) {
			ldif->msg = ldb_msg_canonicalize(ldb_ctx, ldif->msg);
			ret = ldb_add(ldb_ctx, ldif->msg);
			if (ret != LDB_SUCCESS) {
				fclose(f);
				MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERR_DATABASE_INIT, NULL);
			}
			ldb_ldif_read_free(ldb_ctx, ldif);
		}
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
   \details return the mapped property ID matching the nameid
   structure passed in parameter.

   \param ldb_ctx pointer to the namedprops ldb context
   \param nameid the MAPINAMEID structure to lookup
   \param propID pointer to the property ID the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
_PUBLIC_ int mapistore_namedprops_get_mapped_id(void *ldb_ctx, 
						struct MAPINAMEID nameid, 
						uint16_t *propID)
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
			 LDB_SCOPE_SUBTREE, attrs, filter);
	MAPISTORE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPISTORE_ERROR, mem_ctx);

	*propID = ldb_msg_find_attr_as_uint(res->msgs[0], "mapped_id", 0);
	MAPISTORE_RETVAL_IF(!*propID, MAPISTORE_ERROR, mem_ctx);

	talloc_free(filter);
	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}
