/*
** mapitables.c for openchange
** 
** Copyright (C) Gregory Schiro
** Mail   <g.schiro@openchange.org>
** 
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
** 
** Started on  Sun Mar 12 13:03:39 2006 gregory schiro
** Last update Fri Apr 14 04:20:48 2006 root
*/

#include "openchange.h"
#include "ndr_exchange.h"
#include "mapitags.h"
#include "libmapi/include/mapidefs.h"
#include "libmapi/include/property.h"
#include "server/dcesrv_exchange.h"
#include "libmapi/IMAPISession.h"
#include "libmapi/include/proto.h"
#include "libmapi/include/mapi_proto.h"
#include "libmapi/include/mapitables.h"

extern struct GUID_users	*GUID_user_list;

/*
  mapi_setColumns() : Returns requested properties.
*/
/* struct SPropValue *mapitables_setcolumns(TALLOC_CTX *mem_ctx, struct dcesrv_call_state *dce_call, */
/* 					 struct SPropTagArray **pPropTagArray) */
/* { */
/* 	void **mapiValues; */
	
/* 	mapiValues = get_value_tags(mem_ctx, dce_call, pPropTagArray); */
	
/* 	return set_SPropValues(mem_ctx, *pPropTagArray, mapiValues); */
/* } */

/*
  mapitables_restrict_check() : Returns True if the property matchs with the restrictions.
*/
BOOL mapitables_restrict_check(struct SPropValue *pPropValue, void *restriction, uint32_t type)
{
	BOOL	res = True;

	switch(type) {
	case RES_AND:
		res &= mapitables_restrict_check(pPropValue,
						 ((struct SAndRestriction *)restriction)->lpRes,
						 RES_AND);
		break;
	case RES_BITMASK:
		break;
	case RES_COMMENT:
		break;
	case RES_COMPAREPROPS:
		break;
	case RES_CONTENT:
		break;
	case RES_EXIST:
		break;
	case RES_NOT:
		break;
	case RES_OR:
		break;
	case RES_PROPERTY:
		break;
	case RES_SIZE:
		break;
	case RES_SUBRESTRICTION:
		break;
	default:
		break;
	}

	return res;
}

/*
  mapitables_restrict() : Returns requested properties with applied restrictions.
  size is pPropValue length, modified by address.
*/
struct SPropValue *mapitables_restrict(TALLOC_CTX *mem_ctx, struct SRestriction *restrictions,
				       struct SPropValue *pPropValue, uint32_t *size)
{
	struct SPropValue	*lpProps = NULL;
	uint32_t		i = 0;
	uint32_t		new_size = 0;

	for (i = 0; i < *size; i++) {
		if (mapitables_restrict_check(pPropValue + i, &(restrictions->res), restrictions->rt) == True) {
			lpProps = talloc_realloc_size(mem_ctx, lpProps, sizeof(*lpProps));
			memcpy(lpProps + i, pPropValue + i, sizeof(*lpProps));
			new_size++;
		}
	}
	*size = new_size;

	return lpProps;
}

/*
  mapitables_sorttable() : Sorts the properties and returns a RowSet.
  size is pPropValue length, modified by address.
*/
struct SRowSet *mapitables_sorttable(TALLOC_CTX *mem_ctx, struct SSortOrderSet *order,
				      struct SPropValue *pPropValue, uint32_t *size)
{
	return NULL;
}

/*
  mapitables_guid_get_user_guid() : Gets the GUID of an user identified by username.
*/
BOOL mapitables_guid_get_user_guid(TALLOC_CTX *mem_ctx, struct dcesrv_call_state *dce_call, char *username, uint8_t guid[])
{
	struct ldb_context	*conn;
	struct ldb_result	*res;
	char			*expr;
	int			ret;

	conn = samdb_connect(mem_ctx, NTLM_AUTH_SESSION_INFO(dce_call));
        if (conn == NULL) {
                return False;
        }
	expr = talloc_asprintf(mem_ctx, "(&(objectClass=user)(cn=*%s*))", username);
        if (expr == NULL) {
                return False;
        }
        ret = ldb_search(conn, NULL, LDB_SCOPE_SUBTREE, expr, NULL, &res);
        if ((ret != LDB_SUCCESS) || (res->count != 1)) {
                DEBUG(3, ("Failed to find a record for user: %s\n", username));
                return False;
        }
/* 	(mem_ctx, res->msgs[0], "userGUID"); */

	return True;
}

/*
  mapitables_guid_add_by_username() : Adds the GUID of an user (identified by username) to the list.
*/
BOOL mapitables_guid_add_by_username(TALLOC_CTX *mem_ctx, struct dcesrv_call_state *dce_call, char *username)
{
	uint8_t			guid[16];

	if (mapitables_guid_get_user_guid(mem_ctx, dce_call, username, guid) == True) {
		return mapitables_guid_add_by_guid(mem_ctx, guid);
	}

	return False;
}

/*
  mapitables_guid_add_by_guid() : Adds the GUID of an user (identified by username) to the list.
*/
BOOL mapitables_guid_add_by_guid(TALLOC_CTX *mem_ctx, uint8_t guid[])
{
	struct GUID_users	*new;
	
	if (mapitables_guid_find_by_guid(guid) == NULL) {
		new = talloc_size(mem_ctx, sizeof(*new));
		memcpy(new->guid, guid, sizeof(guid));
		new->next = GUID_user_list;
		GUID_user_list = new;

		return True;
	}

	return False;
}

/*
  mapitables_guid_is_equal() : Compares two GUIDs, returns True if equal, False otherwise.
*/
BOOL mapitables_guid_is_equal(uint8_t guid1[], uint8_t guid2[])
{
	uint32_t	i;
	uint32_t	len = sizeof(guid1);

	if (len != sizeof(guid2)) {
		return False;
	}
	for (i = 0; i < len; i++) {
		if (guid1[i] != guid2[i]) {
			return False;
		}
	}

	return True;
}

/*
  mapitables_guid_del_by_username() : Deletes the GUID of an user (identified by username) from the list.
*/
BOOL mapitables_guid_del_by_username(TALLOC_CTX *mem_ctx, struct dcesrv_call_state *dce_call, char *username)
{
	uint8_t	guid[16];

	if (mapitables_guid_get_user_guid(mem_ctx, dce_call, username, guid) == False) {
		return False;
	}

	return mapitables_guid_del_by_guid(guid);
}

/*
  mapitables_guid_del_by_guid() : Deletes the GUID of an user (identified by guid) from the list.
*/
BOOL mapitables_guid_del_by_guid(uint8_t guid[])
{
	struct GUID_users	*guid_list = GUID_user_list;
	struct GUID_users	*guid_prev = NULL;

	while (guid_list) {
		if (mapitables_guid_is_equal(guid_list->guid, guid)) {
			if (guid_prev) {
				guid_prev->next = guid_list->next;
			}
			else {
				GUID_user_list = guid_list->next;
			}
			talloc_free(guid_list);

			return True;
		}
		guid_prev = guid_list;
		guid_list = guid_list->next;
	}

	return False;
}

/*
  mapitables_guid_find_by_username() : Searchs the GUID of an user (identified by username) in the list.
*/
struct GUID_users *mapitables_guid_find_by_username(TALLOC_CTX *mem_ctx, struct dcesrv_call_state *dce_call, char *username)
{
	uint8_t	guid[16];

        if (mapitables_guid_get_user_guid(mem_ctx, dce_call, username, guid) == False) {
                return NULL;
        }

	return mapitables_guid_find_by_guid(guid);
}

/*
  mapitables_guid_find_by_guid() : Searchs the GUID of an user (identified by GUID) in the list.
*/
struct GUID_users *mapitables_guid_find_by_guid(uint8_t guid[])
{
	struct GUID_users       *guid_list = GUID_user_list;

        while (guid_list) {
                if (mapitables_guid_is_equal(guid_list->guid, guid)) {
                        return guid_list;
		}
                guid_list = guid_list->next;
        }
	
	return NULL;
}

