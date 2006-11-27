/*
   MAPI Implementation.

   interface functions for the store database

   OpenChange Project

   Copyright (C) Julien Kerihuel 2006
   Copyright (C) Jerome Medegan 2006
   Copyright (C) Pauline Khun 2006

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "openchange.h"
#include "ndr_exchange.h"
#include "libmapi/include/proto.h"
#include "libmapi/IMAPISession.h"
#include "libmapi/include/mapi_proto.h"

/*
  Connect to the STORE database
  return an opaque context pointer on success, or NULL on failure
*/

struct ldb_context	*storedb_connect(TALLOC_CTX *mem_ctx)
{
  	struct ldb_context	*ldb;
	const char		*storedb = lp_parm_string(-1, "exchange", "store");
	const char		*dbtype = lp_parm_string(-1, "exchange", "dbtype");
	const char		*url;

	ldb = ldb_init(mem_ctx);
	if (!storedb) {
	  DEBUG(0, ("No exchange:store option specified\n"));
	  return NULL;
	}
	if (!dbtype) {
	  DEBUG(0, ("No exchange:dbtype option specified\n"));
	  return NULL;
	}

	url = talloc_asprintf(ldb, "%s://%s", dbtype, storedb);
	
	if (0 != ldb_connect(ldb, url, LDB_FLG_RDONLY, NULL)) {
	  DEBUG(0, ("storedb connection failed\n"));
	  return NULL;
	}
	return (ldb);
}

/*
  search the store for the specified attributes - va_list variant
 */

struct ldb_result		*storedb_search_v(struct ldb_context		*ldb_ctx,
						  TALLOC_CTX			*mem_ctx,
						  const struct ldb_dn		*basedn,
						  uint32_t			scope,
						  const char * const		*attrs,
						  const char			*format,
						  va_list ap) _PRINTF_ATTRIBUTE(6,0)
{
	struct ldb_result	*result;
	char			*expr = NULL;
	int			count;
	
	if (!scope) {
		scope = LDB_SCOPE_SUBTREE;
	}
	
	if (format) {
		vasprintf(&expr, format, ap);
		if (expr == NULL) {
			return NULL;
		}
	} else {
		scope = LDB_SCOPE_BASE;
	}
	
	result = NULL;

	count = ldb_search(ldb_ctx, basedn, scope, expr, attrs, &result);

	if (result) talloc_steal(mem_ctx, result);
	
/* 	DEBUG(4, ("storedb_search_v: %s %s -> %d (%s)\n", */
/* 		  basedn?ldb_dn_linearize(mem_ctx, basedn):"NULL", */
/* 		  expr?expr:"NULL", count, */
/* 		  count==-1?ldb_errstring(ldb_ctx):"OK")); */

	free(expr);

	return result;
}

/*
  search the store for a single record matching the query
 */

struct ldb_dn		*storedb_search_dn(struct ldb_context *store_ldb,
					   TALLOC_CTX *mem_ctx,
					   const struct ldb_dn *basedn,
					   const char *format, ...) _PRINTF_ATTRIBUTE(4,5)
{
	va_list			ap;
	struct ldb_dn		*ret;
	struct ldb_result	*result;

	va_start(ap, format);
	result = storedb_search_v(store_ldb, mem_ctx, basedn, 0, NULL, format, ap);
	va_end(ap);

	if (result->count != 1) return NULL;

	ret = talloc_steal(mem_ctx, result->msgs[0]->dn);
	talloc_free(result);

	return ret;
}

/*
  return the count of the number of records in the store matching the query
*/
int			storedb_search_count(struct ldb_context *store_ldb,
					     TALLOC_CTX *mem_ctx,
					     const struct ldb_dn *basedn,
					     const char *format, ...) _PRINTF_ATTRIBUTE(4,5)
{
	va_list			ap;
	const char * const	attrs[] = { NULL };
	struct ldb_result	*result;

	va_start(ap, format);
	result = storedb_search_v(store_ldb, mem_ctx, basedn, 0, attrs, format, ap);
	va_end(ap);

	return result->count;
}

/*
  search the store for the parentID and returns a SBinary structure
 */

struct SBinary		*store_search_container_parentid(struct ldb_context *ldb_ctx,
						TALLOC_CTX *mem_ctx,
						const struct ldb_dn *basedn,
						const char *format, ...) _PRINTF_ATTRIBUTE(4,5)
{
	va_list			ap;
	struct SBinary		*bin;
	const char * const	attrs[] = { "entryID" , 0};
	struct ldb_result	*result;

	va_start(ap, format);
	result = storedb_search_v(ldb_ctx, mem_ctx, basedn, LDB_SCOPE_SUBTREE, attrs, format, ap);
	va_end(ap);

	if (result->count != 1) {
		return NULL;
	}

	bin = generate_recipient_entryid(mem_ctx, ldb_msg_find_attr_as_string(result->msgs[0], "entryID", NULL));

	return bin;
}

/*
  search the store and retrieve the container list with a scope = LDB_SCOPE_SUBTREE (2)
  containers are returned in ldb_message.
  The function returns the number of container it found
*/

struct SRowSet *storedb_search_container_tree(struct ldb_context	*ldb_ctx, 
					      TALLOC_CTX		*mem_ctx,
					      const struct ldb_dn	*basedn,
					      struct ldb_result		**result,
					      const char *format, ...) _PRINTF_ATTRIBUTE(5,6)
{
	va_list			ap;
	const char * const	attr[] = { "*" , 0};
	uint32_t		i;
	struct SRowSet		*SRowSet;
	struct SBinary		*entryID;
	struct SBinary		*parentID = (struct SBinary *) NULL;
	uint32_t		*containerFlags;
	uint32_t		containerID;
	const char     		*display_name_const;
	char     		*display_name;
	const char     		*memberOf;
	uint32_t		depth;
	const char     		*IsMaster;
	uint32_t		IsMaster_nb;
	uint32_t		prop_count;

	va_start(ap, format);
	*result = storedb_search_v(ldb_ctx, mem_ctx, basedn, LDB_SCOPE_SUBTREE, attr, format, ap);
	va_end(ap);

	SRowSet = talloc(mem_ctx, struct SRowSet);
	SRowSet->cRows = (*result)->count;
	SRowSet->aRow = talloc_array(mem_ctx, struct SRow, (*result)->count);

	for (i=0; i != (*result)->count; i++) {

		entryID = generate_recipient_entryid(mem_ctx, ldb_msg_find_attr_as_string((*result)->msgs[i], "entryID", NULL));
		
		containerFlags = talloc(mem_ctx, uint32_t);
		*containerFlags = ldb_msg_find_attr_as_int((*result)->msgs[i], "containerFlags", 0);
		containerID = ldb_msg_find_attr_as_int((*result)->msgs[i], "containerID", 0);
		display_name_const = ldb_msg_find_attr_as_string((*result)->msgs[i], "cn", NULL);
		display_name = talloc_strdup(mem_ctx, display_name_const);

		IsMaster = ldb_msg_find_attr_as_string((*result)->msgs[i], "IsMaster", NULL);
		if (IsMaster) {
			IsMaster_nb = (!strcmp(IsMaster, "TRUE") ? 0x1 : 0x0);
		} else
			IsMaster_nb = 0x0;


		memberOf = ldb_msg_find_attr_as_string((*result)->msgs[i], "memberOf", NULL);

		if (memberOf) {
			memberOf = talloc_strdup(mem_ctx, memberOf);
			parentID = store_search_container_parentid(ldb_ctx, mem_ctx, NULL,
								   "(&(objectClass=recipient)(dn=%s))", memberOf);
		}

		prop_count = (memberOf) ? 7 : 6;
		depth = (memberOf) ? 1 : 0;

		SRowSet->aRow[i].ulAdrEntryPad = 0;
		SRowSet->aRow[i].cValues = prop_count;

		SRowSet->aRow[i].lpProps = talloc_array(mem_ctx, struct SPropValue, prop_count);

		set_SPropValue_proptag(&SRowSet->aRow[i].lpProps[0], PR_ENTRYID, (void *)entryID);
		set_SPropValue_proptag(&SRowSet->aRow[i].lpProps[1], PR_CONTAINER_FLAGS, (void *)containerFlags);
		set_SPropValue_proptag(&SRowSet->aRow[i].lpProps[2], PR_DEPTH, (void *)&depth);
		set_SPropValue_proptag(&SRowSet->aRow[i].lpProps[3], PR_EMS_AB_CONTAINERID, (void *)&containerID);
		set_SPropValue_proptag(&SRowSet->aRow[i].lpProps[4], PR_DISPLAY_NAME, (void *)display_name);
		set_SPropValue_proptag(&SRowSet->aRow[i].lpProps[5], PR_EMS_AB_IS_MASTER, (void *)&IsMaster_nb);

		if (memberOf) {
		  set_SPropValue_proptag(&SRowSet->aRow[i].lpProps[6], PR_EMS_AB_PARENT_ENTRYID, (void *)parentID);
		}
/* 		DEBUG(4, ("dn = %s\n", ldb_dn_linearize(mem_ctx, (*result)->msgs[i]->dn))); */
		DEBUG(4, ("cn = %s\n", display_name));
		DEBUG(4, ("containerID = 0x%x\n", containerID));
		DEBUG(4, ("containerFlags = 0x%x\n", *containerFlags));
		DEBUG(4, ("IsMaster = %d\n", IsMaster_nb));
	}

	return SRowSet;
}



/*
  pull a string from a result set.
*/
const char *storedb_result_string(struct ldb_message *msg, const char *attr,
				const char *default_value)
{
	return ldb_msg_find_attr_as_string(msg, attr, default_value);
}

/*
  pull a uint from a result set.
*/
uint_t			storedb_result_uint(struct ldb_message *msg, const char *attr, uint_t default_value)
{
	return ldb_msg_find_attr_as_uint(msg, attr, default_value);
}
