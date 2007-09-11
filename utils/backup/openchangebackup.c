/*
   MAPI Backup application suite

   OpenChange Project

   Copyright (C) Julien Kerihuel 2007

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

#include "openchangebackup.h"

/**
 * Initialize OCB (OpenChange Backup) subsystem
 * and open a pointer on the LDB database
 */
struct ocb_context *ocb_init(TALLOC_CTX *mem_ctx, const char *dbpath)
{
	struct ocb_context	*ocb_ctx = NULL;
	char			*url = NULL;
	int			ret;

	/* sanity check */
	if (!mem_ctx) {
		DEBUG(3, ("[ocb]: talloc context not valid\n"));
		return NULL;
	}

	if (!dbpath) {
		DEBUG(3, ("[ocb]: dbpath not set\n"));
		return NULL;
	}
	
	ocb_ctx = talloc_zero(mem_ctx, struct ocb_context);

	/* init ldb */
	ret = ldb_global_init();
	if (ret != LDB_SUCCESS) return NULL;

	/* init ldb store */
	ocb_ctx->ldb_ctx = ldb_init((TALLOC_CTX *)ocb_ctx);
	if (!ocb_ctx->ldb_ctx) goto failed;

	url = talloc_asprintf(mem_ctx, "tdb://%s", dbpath);
	ret = ldb_connect(ocb_ctx->ldb_ctx, url, 0, NULL);
	talloc_free(url);
	if (ret != LDB_SUCCESS) goto failed;

	return ocb_ctx;
failed:
	ocb_release(ocb_ctx);
	return NULL;
}


/**
 * Release OCB subsystem
 */
uint32_t ocb_release(struct ocb_context *ocb_ctx)
{
	if (!ocb_ctx) {
		DEBUG(3, ("[ocb] subsystem not initialized\n"));
		return -1;
	}
	
	talloc_free(ocb_ctx);

	return 0;
}


/**
 * Add a new record to OCB ldap database
 */
uint32_t ocb_create_record(struct ocb_context *ocb_ctx, const char *containerdn, 
			   const char *uid)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_context		*ldb_ctx;
	enum ldb_scope			scope = LDB_SCOPE_SUBTREE;
	struct ldb_message		msg;
	struct ldb_message_element	el[2];
	struct ldb_val			vals[2][1];
	struct ldb_result		*res;
	struct ldb_dn			*basedn;
	int				ret;
	const char * const		attrs[] = { "*", NULL };

	/* sanity check */
	if (!containerdn || !uid) {
		return -1;
	}

	if (!ocb_ctx->ldb_ctx) {
		return -1;
	}

	mem_ctx = (TALLOC_CTX *)ocb_ctx;
	ldb_ctx = ocb_ctx->ldb_ctx;

	/* Does the entry already exist? */
	ret = ldb_search(ldb_ctx, ldb_get_default_basedn(ldb_ctx), scope, containerdn, attrs, &res);
	if (res->msgs) {
		return -1;
	}

	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, containerdn);
	if (!ldb_dn_validate(basedn)) {
		DEBUG(3, ("Invalid dn = %s\n", containerdn));
		return -1;
	}

	msg.dn = ldb_dn_copy(mem_ctx, basedn);
	msg.num_elements = 2;
	msg.elements = el;

	el[0].flags = 0;
	el[0].name = talloc_strdup(mem_ctx, "cn");
	el[0].num_values = 1;
	el[0].values = vals[0];
	vals[0][0].data = (uint8_t *)talloc_strdup(mem_ctx, uid);
	vals[0][0].length = strlen(uid);

	el[1].flags = 0;
	el[1].name = talloc_strdup(mem_ctx, "objectID");
	el[1].num_values = 1;
	el[1].values = vals[1];
	vals[1][0].data = (uint8_t *)talloc_strdup(mem_ctx, uid);
	vals[1][0].length = strlen(uid);

	ret = ldb_add(ldb_ctx, &msg);

	if (ret != LDB_SUCCESS) return -1;

	return 0;
}


/**
 * Add a property (value+ attr) to a ldb database record
 */
static uint32_t ocb_add_string_attribute(struct ocb_context *ocb_ctx, 
					 const char *recdn,
					 char *attr,
					 char *value)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_message		msg;
	struct ldb_val			vals[1][1];
	struct ldb_message_element	el[1];
	struct ldb_context		*ldb_ctx;
	struct ldb_dn			*basedn;
	int				ret;

	mem_ctx = talloc_init("ocb_add_string_attribute");
	ldb_ctx = ocb_ctx->ldb_ctx;

	/* Retrieve the record context */
	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, recdn);
	if (!ldb_dn_validate(basedn)) {
		DEBUG(3, ("Invalid dn = %s\n", recdn));
		return -1;
	}

	msg.dn = ldb_dn_copy(mem_ctx, basedn);
	msg.num_elements = 1;
	msg.elements = el;

	el[0].flags = LDB_FLAG_MOD_ADD;
	el[0].name = talloc_strdup(mem_ctx, attr);
	el[0].num_values = 1;
	el[0].values = vals[0];
	vals[0][0].data = (uint8_t *)talloc_strdup(mem_ctx, value);
	vals[0][0].length = strlen(value);

	if ((ret = ldb_modify(ldb_ctx, &msg)) != LDB_SUCCESS) {
		talloc_free(mem_ctx);
		return -1;
	}
	
	talloc_free(mem_ctx);
	return 0;
}

/**
 * Public function: Add properties (name, value) to an existing record
 *
 * This function shouldn't be used when adding a full record to the
 * database. It performs one ldb transaction for each property rather
 * than a single one for the whole record.
 */
uint32_t ocb_add_attribute(struct ocb_context *ocb_ctx, const char *recdn,
			     struct mapi_SPropValue *lpProp)
{
	TALLOC_CTX     	*mem_ctx;
	int    		i;
	char	       	*attr;
	char	       	*value;

	mem_ctx = talloc_init("ocb_add_attribute");
	attr = talloc_asprintf(mem_ctx, "0x%.8x", lpProp->ulPropTag);

	switch (lpProp->ulPropTag & 0xFFFF) {
	case PT_SHORT:
		value = talloc_asprintf(mem_ctx, "%hd", lpProp->value.i);
		ocb_add_string_attribute(ocb_ctx, recdn, attr, value);
		break;
	case PT_STRING8:
		if (!lpProp->value.lpszA) {
			talloc_free(mem_ctx);
			return 0;
		}
		if (lpProp->value.lpszA && strlen((char *)lpProp->value.lpszA) == 0) {
			talloc_free(mem_ctx);
			return 0;
		}
		value = talloc_strdup(mem_ctx, lpProp->value.lpszA);
		ocb_add_string_attribute(ocb_ctx, recdn, attr, value);
		break;
	case PT_UNICODE:
		if (!lpProp->value.lpszW) {
			talloc_free(mem_ctx);
			return 0;
		}
		if (lpProp->value.lpszW && strlen((char *)lpProp->value.lpszW) == 0) {
			talloc_free(mem_ctx);
			return 0;
		}
		value = talloc_strdup(mem_ctx, lpProp->value.lpszW);
		ocb_add_string_attribute(ocb_ctx, recdn, attr, value);
		break;
	case PT_ERROR:
		/* We shouldn't need to backup error properties */
		return 0;
		break;
	case PT_LONG:
		value = talloc_asprintf(mem_ctx, "%d", lpProp->value.l);
		ocb_add_string_attribute(ocb_ctx, recdn, attr, value);
		break;
	case PT_BOOLEAN:
		value = talloc_strdup(mem_ctx, (lpProp->value.b == true) ? "true" : "false");
		ocb_add_string_attribute(ocb_ctx, recdn, attr, value);
		break;
	case PT_I8:
		if (!lpProp->value.d) {
			talloc_free(mem_ctx);
			return 0;
		}
		value = talloc_asprintf(mem_ctx, "%16llx", lpProp->value.d);
		ocb_add_string_attribute(ocb_ctx, recdn, attr, value);
		break;
	case PT_SYSTIME:
		value = ocb_ldb_timestring(mem_ctx, &lpProp->value.ft);
		ocb_add_string_attribute(ocb_ctx, recdn, attr, value);
		break;
	case 0xFB:
	case PT_BINARY:
		value = ldb_base64_encode(mem_ctx, (char *)lpProp->value.bin.lpb, 
					  lpProp->value.bin.cb);
		ocb_add_string_attribute(ocb_ctx, recdn, attr, value);
		break;
	case PT_MV_LONG:
		for (i = 0; i < lpProp->value.MVl.cValues; i++) {
			value = talloc_asprintf(mem_ctx, "%d", lpProp->value.MVl.lpl[i]);		
			ocb_add_string_attribute(ocb_ctx, recdn, attr, value);
		}
		break;
	case PT_MV_BINARY:
		for (i = 0; i < lpProp->value.MVbin.cValues; i++) {
			value = ldb_base64_encode(mem_ctx, (char *)lpProp->value.MVbin.bin[i].lpb, 
						  lpProp->value.MVbin.bin[i].cb);		
			ocb_add_string_attribute(ocb_ctx, recdn, attr, value);
		}
		break;
	case PT_MV_STRING8:
		for (i = 0; i < lpProp->value.MVszA.cValues; i++) {
			value = talloc_strdup(mem_ctx, lpProp->value.MVszA.strings[i].lppszA);
			ocb_add_string_attribute(ocb_ctx, recdn, attr, value);
		}
		break;
	default:
		printf("%s case %d not supported\n", attr, lpProp->ulPropTag & 0xFFFF);
		value = talloc_strdup(mem_ctx, "MAPI_E_NO_SUPPORT");
		ocb_add_string_attribute(ocb_ctx, recdn, attr, value);
		break;
	}

	talloc_free(mem_ctx);
	return 0;
}


/**
 * Retrieve UUID from Sbinary_short struct
 * Generally used to map PR_STORE_KEY to a string
 * Used for attachments
 */
char *get_record_uuid(TALLOC_CTX *mem_ctx, const struct SBinary_short *bin)
{
	uint32_t	i;
	char		*lpb;

	if (!bin) {
		DEBUG(3, ("Invalid PR_RECORD_KEY value\n"));
		return NULL;
	}

	lpb = talloc_asprintf(mem_ctx, "%.2X", bin->lpb[0]);
	for (i = 1; i < bin->cb; i++) {
		lpb = talloc_asprintf_append(lpb, "%.2X", bin->lpb[i]);
	}
	
	return lpb;
}


/**
 * Extract MAPI object unique ID from PR_SOURCE_KEY Sbinary_short data:
 * PR_SOURCE_KEY = 22 bytes field
 * - 16 first bytes = MAPI Store GUID
 * - 6 last bytes = MAPI object unique ID
 */
char *get_MAPI_uuid(TALLOC_CTX *mem_ctx, const struct SBinary_short *bin)
{
	uint32_t       	i;
	char		*ab;

	if (!bin || bin->cb != 22) {
		DEBUG(3, ("Invalid SBinary_short structure\n"));
		return NULL;
	}

	ab = talloc_asprintf(mem_ctx, "%.2X", bin->lpb[16]);
	for (i = 17; i < bin->cb; i++) {
		ab = talloc_asprintf_append(ab, "%.2X", bin->lpb[i]);
	}

	return ab;
}


/**
 * Retrieve the store GUID from a given record.
 * This GUID should be unique for each store and identical for all
 * objects belonging to this store
 */
char *get_MAPI_store_guid(TALLOC_CTX *mem_ctx, const struct SBinary_short *bin)
{
	int		i;
	char		*ab;

	if (!bin || bin->cb != 22) {
		DEBUG(3, ("Invalid SBinary_short structure\n"));
		return NULL;
	}

	ab = talloc_asprintf(mem_ctx, "%.2X", bin->lpb[0]);
	for (i = 1; i < 16; i++) {
		ab = talloc_asprintf_append(ab, "%.2X", bin->lpb[i]);
	}
	
	return ab;
}


/**
 * Convert date from MAPI property to ldb format
 * Easier to manipulate
 */
char *ocb_ldb_timestring(TALLOC_CTX *mem_ctx, struct FILETIME *ft)
{
	NTTIME		time;
	time_t		t;

	if (!ft) {
		DEBUG(3, ("Invalid FILETIME structure\n"));
		return NULL;
	}

	time = ft->dwHighDateTime;
	time = time << 32;
	time |= ft->dwLowDateTime;

	t = nt_time_to_unix(time);
	return ldb_timestring(mem_ctx, t);
}
