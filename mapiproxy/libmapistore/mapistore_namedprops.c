/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010-2013
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

#include <stdbool.h>
#include <string.h>
#include "mapistore.h"
// available named_properties backend implementations
#include "backends/namedprops_ldb.h"
#include "backends/namedprops_mysql.h"


// Known prefixes for connection strings
#define LDB_PREFIX "ldb://"
#define MYSQL_PREFIX "mysql://"

/**
   \details Return the path to the ldif file holding initial set of
   named properties to populate the backend

   \return path to the ldif file
 */
const char *mapistore_namedprops_get_ldif_path(void)
{
       return MAPISTORE_LDIF; // Defined on compilation time
}


/**
   \details Check if a string starts with the specified prefix

   \param str pointer to the string to lookup
   \param prefix pointer to the prefix to search

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error starts_with(const char *str, const char *prefix)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!str, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!prefix, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	if (!strncmp(str, prefix, strlen(prefix))) {
		return MAPISTORE_SUCCESS;
	}

	return MAPISTORE_ERR_NOT_FOUND;
}


/**
   \details Initialize the named properties database or return pointer
   to the existing one if already initialized/opened.

   \param mem_ctx pointer to the memory context
   \param conn_info pointer to the mapistore namedprops backend URL
   \param nprops pointer on pointer to the namedprops context the
   function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum mapistore_error mapistore_namedprops_init(TALLOC_CTX *mem_ctx,
					       const char *conn_info,
					       struct namedprops_context **nprops)
{
	const char		*database;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!conn_info, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!nprops, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	if (starts_with(conn_info, LDB_PREFIX) == MAPISTORE_SUCCESS) {
		database = &conn_info[strlen(LDB_PREFIX)];
		return mapistore_namedprops_ldb_init(mem_ctx, database, nprops);
	} else if (starts_with(conn_info, MYSQL_PREFIX) == MAPISTORE_SUCCESS) {
		return mapistore_namedprops_mysql_init(mem_ctx, conn_info, nprops);
	} 
		
	DEBUG(0, ("[%s:%d]: ERROR Invalid named_properties backend type for %s\n", 
		  __FUNCTION__, __LINE__, conn_info));
	return MAPISTORE_ERR_INVALID_PARAMETER;
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

/* [MS-OXCDATA] 2.11.1 */
struct mapistore_namedprops_typarray {
	uint16_t	type_value;
	const char	*type_name;
	const char	*alt_name;
};

static struct mapistore_namedprops_typarray mapistore_typarray[] = {
	{ PT_UNSPECIFIED, "PtypUnspecified", "PT_UNSPECIFIED" },
	{ PT_NULL, "PtypNull", "PT_NULL" },
	{ PT_I2, "PtypInteger16", "PT_I2" },
	{ PT_SHORT, "PtypInteger16", "PT_SHORT" },
	{ PT_LONG, "PtypInteger32", "PT_LONG" },
	{ PT_FLOAT, "PtypFloating32", "PT_FLOAT" },
	{ PT_DOUBLE, "PtypFloating64", "PT_DOUBLE" },
	{ PT_CURRENCY, "PtypCurrency", "PT_CURRENCY" },
	{ PT_APPTIME, "PtypFloatingTime", "PT_APPTIME" },
	{ PT_ERROR, "PtypErrorCode", "PT_ERROR" },
	{ PT_BOOLEAN, "PtypBoolean", "PT_BOOLEAN" },
	{ PT_OBJECT, "PtypObject", "PT_OBJECT" },
	{ PT_I8, "PtypInteger64", "PT_I8" },
	{ PT_UNICODE, "PtypString", "PT_UNICODE" },
	{ PT_STRING8, "PtypeString8", "PT_STRING8" },
	{ PT_SYSTIME, "PtypTime", "PT_SYSTIME" },
	{ PT_CLSID, "PtypGuid", "PT_CLSID" },
	{ PT_SVREID, "PtypServerId", "PT_SVREID" },
	{ PT_SRESTRICT, "PtypRestriction", "PT_SRESTRICT" },
	{ PT_ACTIONS, "PtypRuleAction", "PT_ACTIONS"},
	{ PT_BINARY, "PtypBinary", "PT_BINARY" },
	{ PT_MV_SHORT, "PtypMultipleInteger16", "PT_MV_SHORT" },
	{ PT_MV_LONG, "PtypMultipleInteger32", "PT_MV_LONG" },
	{ PT_MV_FLOAT, "PtypMultipleFloating32", "PT_MV_FLOAT" },
	{ PT_MV_DOUBLE, "PtypMultipleFloating64", "PT_MV_DOUBLE" },
	{ PT_MV_CURRENCY, "PtypMultipleCurrency", "PT_MV_CURRENCY" },
	{ PT_MV_APPTIME, "PtypFloatingTime", "PT_MV_APPTIME" },
	{ PT_MV_I8, "PtypMultipleInteger64", "PT_MV_I8" },
	{ PT_MV_UNICODE, "PtypMultipleString", "PT_MV_UNICODE" },
	{ PT_MV_STRING8, "PtypMultipleString8", "PT_MV_STRING8" },
	{ PT_MV_SYSTIME, "PtypMultipleTime", "PT_MV_SYSTIME" },
	{ PT_MV_CLSID, "PtypMultipleGuid", "PT_MV_CLSID" },
	{ PT_MV_BINARY, "PtypMultipleBinary", "PT_MV_BINARY" },
	{ 0x0, NULL, NULL }
};

/**
   \details Return the property type given the alternate name

   \param prop_type_str the alternate name to lookup

   \return valid uint16_t property type on success, otherwise -1
 */
int mapistore_namedprops_prop_type_from_string(const char *prop_type_str)
{
	uint8_t	i;

	/* Sanity checks */
	if (!prop_type_str) return -1;

	for (i = 0; mapistore_typarray[i].alt_name != NULL; i++) {
		if (!strcmp(prop_type_str, mapistore_typarray[i].alt_name)) {
			return mapistore_typarray[i].type_value;
		}
	}

	return -1;
}

_PUBLIC_ enum mapistore_error mapistore_namedprops_transaction_start(struct namedprops_context *nprops)
{
	MAPISTORE_RETVAL_IF(!nprops, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	return nprops->transaction_start(nprops);
}

_PUBLIC_ enum mapistore_error mapistore_namedprops_transaction_commit(struct namedprops_context *nprops)
{
	MAPISTORE_RETVAL_IF(!nprops, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	return nprops->transaction_commit(nprops);
}
