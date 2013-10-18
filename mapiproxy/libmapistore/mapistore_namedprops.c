/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010-2013

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

#include <stdbool.h>
#include <string.h>
#include "mapistore.h"
// available named_properties backend implementations
#include "backends/namedprops_ldb.h"

#define	MAPISTORE_DB_NAMED  "named_properties.ldb"

// Known prefixes for connection strings
#define LDB_PREFIX "ldb://"
#define MYSQL_PREFIX "mysql://"


static bool starts_with(const char *str, const char *prefix)
{
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

/**
   \details Initialize the named properties database or return pointer
   to the existing one if already initialized/opened.

   \param mem_ctx pointer to the memory context
   \param ldb_ctx pointer on pointer to the ldb context the function
   returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum mapistore_error mapistore_namedprops_init(TALLOC_CTX *mem_ctx,
					       const char *conn_info,
					       struct namedprops_context **nprops)
{
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!conn_info, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!nprops, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	if (starts_with(conn_info, LDB_PREFIX)) {
		char *database = talloc_strdup(mem_ctx, conn_info + strlen(LDB_PREFIX));
		return mapistore_namedprops_ldb_init(mem_ctx, database, nprops);
	} else if (starts_with(conn_info, MYSQL_PREFIX)) {
		return MAPISTORE_ERR_NOT_IMPLEMENTED;
	} else {
		DEBUG(5, ("Unknown type of named_properties backend for %s\n",
			  conn_info));
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}
}


/**
   \details return the next unmapped property ID

   \param ldb_ctx pointer to the namedprops ldb context

   \return 0 on error, the next mapped id otherwise
 */
_PUBLIC_ uint16_t mapistore_namedprops_next_unused_id(struct namedprops_context *nprops)
{
	MAPISTORE_RETVAL_IF(!nprops, MAPISTORE_ERROR, NULL);

	uint16_t highest_id = nprops->next_unused_id(nprops);
	DEBUG(5, ("next_mapped_id: %d\n", highest_id));

	return highest_id;
}

/**
   \details return the mapped property ID matching the nameid
   structure passed in parameter.

   \param ldb_ctx pointer to the namedprops ldb context
   \param nameid the MAPINAMEID structure to lookup
   \param propID pointer to the property ID the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
_PUBLIC_ enum mapistore_error mapistore_namedprops_create_id(struct namedprops_context *nprops,
							     struct MAPINAMEID nameid,
							     uint16_t mapped_id)
{
	MAPISTORE_RETVAL_IF(!nprops, MAPISTORE_ERROR, NULL);

	return nprops->create_id(nprops, nameid, mapped_id);
}

/**
   \details return the mapped property ID matching the nameid
   structure passed in parameter.

   \param ldb_ctx pointer to the namedprops ldb context
   \param nameid the MAPINAMEID structure to lookup
   \param propID pointer to the property ID the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
_PUBLIC_ enum mapistore_error mapistore_namedprops_get_mapped_id(struct namedprops_context *nprops,
								 struct MAPINAMEID nameid,
								 uint16_t *propID)
{
	MAPISTORE_RETVAL_IF(!nprops, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!propID, MAPISTORE_ERROR, NULL);

	return nprops->get_mapped_id(nprops, nameid, propID);
}

/**
   \details return the nameid structture matching the mapped property ID
   passed in parameter.

   \param ldb_ctx pointer to the namedprops ldb context
   \param propID the property ID to lookup
   \param nameid pointer to the MAPINAMEID structure the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
_PUBLIC_ enum mapistore_error mapistore_namedprops_get_nameid(struct namedprops_context *nprops,
							      uint16_t propID,
							      TALLOC_CTX *mem_ctx,
							      struct MAPINAMEID **nameidp)
{
	MAPISTORE_RETVAL_IF(!nprops, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(propID < 0x8000, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!nameidp, MAPISTORE_ERROR, NULL);

	return nprops->get_nameid(nprops, propID, mem_ctx, nameidp);
}

/**
   \details return the type matching the mapped property ID passed in parameter.

   \param ldb_ctx pointer to the namedprops ldb context
   \param propID the property ID to lookup
   \param propTypeP pointer to the uint16_t that will receive the property type

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
_PUBLIC_ enum mapistore_error mapistore_namedprops_get_nameid_type(struct namedprops_context *nprops,
								   uint16_t propID,
								   uint16_t *propTypeP)
{
	MAPISTORE_RETVAL_IF(!nprops, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(propID < 0x8000, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!propTypeP, MAPISTORE_ERROR, NULL);

	int ret = nprops->get_nameid_type(nprops, propID, propTypeP);
	MAPISTORE_RETVAL_IF(ret != MAPISTORE_SUCCESS, ret, NULL);

	switch (*propTypeP) {
	case PT_SHORT:
	case PT_LONG:
	case PT_FLOAT:
	case PT_DOUBLE:
	case PT_CURRENCY:
	case PT_APPTIME:
	case PT_ERROR:
	case PT_BOOLEAN:
	case PT_OBJECT:
	case PT_I8:
	case PT_STRING8:
	case PT_UNICODE:
	case PT_SYSTIME:
	case PT_CLSID:
	case PT_SVREID:
	case PT_SRESTRICT:
	case PT_ACTIONS:
	case PT_BINARY:
	case PT_MV_SHORT:
	case PT_MV_LONG:
	case PT_MV_FLOAT:
	case PT_MV_DOUBLE:
	case PT_MV_CURRENCY:
	case PT_MV_APPTIME:
	case PT_MV_I8:
	case PT_MV_STRING8:
	case PT_MV_UNICODE:
	case PT_MV_SYSTIME:
	case PT_MV_CLSID:
	case PT_MV_BINARY:
		break;
	default:
		DEBUG(0, ("%d is not a valid type for a named property (%.4x)\n",
			  *propTypeP, propID));
		abort();
	}

	return MAPISTORE_SUCCESS;
}
