/*
   MAPI Backup application suite

   OpenChange Project

   Copyright (C) Julien Kerihuel 2007

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

#include "openchangebackup.h"
#include "libmapi/libmapi_private.h"
#include <util/debug.h>

/**
 * Initialize OCB (OpenChange Backup) subsystem
 * and open a pointer on the LDB database
 */
struct ocb_context *ocb_init(TALLOC_CTX *mem_ctx, const char *dbpath)
{
	struct ocb_context	*ocb_ctx = NULL;
	char			*url = NULL;
	int			ret;
	struct tevent_context	*ev;

	/* sanity check */
	OCB_RETVAL_IF_CODE(!mem_ctx, "invalid memory context", NULL, NULL);
	OCB_RETVAL_IF_CODE(!dbpath, "dbpath not set", NULL, NULL);

	ocb_ctx = talloc_zero(mem_ctx, struct ocb_context);

	ev = tevent_context_init(ocb_ctx);
	if (!ev) goto failed;

	/* init ldb store */
	ocb_ctx->ldb_ctx = ldb_init((TALLOC_CTX *)ocb_ctx, ev);
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
	OCB_RETVAL_IF(!ocb_ctx, "subsystem not initialized\n", NULL);
	talloc_free(ocb_ctx);

	return 0;
}

/**
 * init and prepare a record
 */

int ocb_record_init(struct ocb_context *ocb_ctx, const char *objclass, const char *dn, 
		    const char *id, struct mapi_SPropValue_array *props)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_context	*ldb_ctx;
	struct ldb_result	*res;
	enum ldb_scope		scope = LDB_SCOPE_SUBTREE;
	struct ldb_dn		*basedn;
	const char * const     	attrs[] = { "*", NULL };

	/* sanity check */
	OCB_RETVAL_IF(!ocb_ctx, "Subsystem not initialized", NULL);
	OCB_RETVAL_IF(!dn, "Not a valid DN", NULL);
	OCB_RETVAL_IF(!id, "Not a valid ID", NULL);

	mem_ctx = (TALLOC_CTX *)ocb_ctx;
	ldb_ctx = ocb_ctx->ldb_ctx;

	/* Check if the record already exists */
	ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx), scope, attrs, "%s", dn);
	OCB_RETVAL_IF(res->msgs, "Record already exists", NULL);

	/* Retrieve the record basedn */
	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, dn);
	OCB_RETVAL_IF(!ldb_dn_validate(basedn), "Invalid DN", NULL);

	ocb_ctx->msg = ldb_msg_new(mem_ctx);
	ocb_ctx->msg->dn = ldb_dn_copy(mem_ctx, basedn);

	/* add records for cn */
	ldb_msg_add_string(ocb_ctx->msg, "cn", id);

	/* add filters attributes */
	ldb_msg_add_string(ocb_ctx->msg, "objectClass", objclass);

	talloc_free(basedn);

	return 0;
}


/**
 * Commit the record with all its attributes: single transaction
 */
uint32_t ocb_record_commit(struct ocb_context *ocb_ctx)
{
	int		ret;

	/* sanity checks */
	OCB_RETVAL_IF(!ocb_ctx, "Subsystem not initialized", NULL);
	OCB_RETVAL_IF(!ocb_ctx->ldb_ctx, "LDB context not initialized", NULL);
	OCB_RETVAL_IF(!ocb_ctx->msg, "Message not initialized", NULL);

	ret = ldb_add(ocb_ctx->ldb_ctx, ocb_ctx->msg);
	if (ret != LDB_SUCCESS) {
		OC_DEBUG(3, "LDB operation failed: %s", ldb_errstring(ocb_ctx->ldb_ctx));
		return -1;
	}

	talloc_free(ocb_ctx->msg);

	return 0;
}


/**
 * Add a property (attr, value) couple to the current record
 */
uint32_t ocb_record_add_property(struct ocb_context *ocb_ctx, 
				 struct mapi_SPropValue *lpProp)
{
	TALLOC_CTX     	*mem_ctx;
	uint32_t	i;
	char	       	*attr;
	char	       	*value = NULL;
	const char	*tag;

	/* sanity checks */
	OCB_RETVAL_IF(!ocb_ctx, "Subsystem not initialized", NULL);
	OCB_RETVAL_IF(!ocb_ctx->ldb_ctx, "LDB context not initialized", NULL);
	OCB_RETVAL_IF(!ocb_ctx->msg, "Message not initialized", NULL);

	mem_ctx = (TALLOC_CTX *)ocb_ctx->msg;

	tag = get_proptag_name(lpProp->ulPropTag);
	if (tag) {
		attr = talloc_asprintf(mem_ctx, "%s", tag);
	} else {
		attr = talloc_asprintf(mem_ctx, "PR-x%.8x", lpProp->ulPropTag);
	}

	for (i = 0; attr[i]; i++) {
		if (attr[i] == '_') attr[i] = '-';
	}
	
	switch (lpProp->ulPropTag & 0xFFFF) {
	case PT_SHORT:
		ldb_msg_add_fmt(ocb_ctx->msg, attr, "%hd", lpProp->value.i);
		break;
	case PT_STRING8:
		ldb_msg_add_string(ocb_ctx->msg, attr, lpProp->value.lpszA);
		break;
	case PT_UNICODE:
		ldb_msg_add_string(ocb_ctx->msg, attr, lpProp->value.lpszW);
		break;
	case PT_ERROR: /* We shouldn't need to backup error properties */
		return 0;
	case PT_LONG:
		ldb_msg_add_fmt(ocb_ctx->msg, attr, "%d", lpProp->value.l);
		break;
	case PT_BOOLEAN:
		ldb_msg_add_fmt(ocb_ctx->msg, attr, "%s", 
				((lpProp->value.b == true) ? "true" : "false"));
		break;
	case PT_I8:
		ldb_msg_add_fmt(ocb_ctx->msg, attr, "%"PRId64, lpProp->value.d);
		break;
	case PT_SYSTIME:
		value = ocb_ldb_timestring(mem_ctx, &lpProp->value.ft);
		ldb_msg_add_string(ocb_ctx->msg, attr, value);
		break;
	case 0xFB:
	case PT_BINARY:
		if (lpProp->value.bin.cb) {
			value = ldb_base64_encode(mem_ctx, (char *)lpProp->value.bin.lpb,
						  lpProp->value.bin.cb);
			ldb_msg_add_string(ocb_ctx->msg, attr, value);
		}
		break;
	case PT_MV_LONG:
		for (i = 0; i < lpProp->value.MVl.cValues; i++) {
			ldb_msg_add_fmt(ocb_ctx->msg, attr, "%d", 
					lpProp->value.MVl.lpl[i]);
		}
		break;
	case PT_MV_BINARY:
		for (i = 0; i < lpProp->value.MVbin.cValues; i++) {
			struct SBinary_short bin;

			bin = lpProp->value.MVbin.bin[i];
			if (bin.cb) {
				value = ldb_base64_encode(mem_ctx, (char *)bin.lpb, bin.cb);
				ldb_msg_add_string(ocb_ctx->msg, attr, value);
			}
		}
		break;
	case PT_MV_STRING8:
		for (i = 0; i < lpProp->value.MVszA.cValues; i++) {
			ldb_msg_add_string(ocb_ctx->msg, attr, 
					   lpProp->value.MVszA.strings[i].lppszA);
		}
		break;
	default:
		printf("%s case %d not supported\n", attr, lpProp->ulPropTag & 0xFFFF);
		break;
	}

	talloc_free(attr);
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

	OCB_RETVAL_IF_CODE(!bin, "Invalid PR_RECORD_KEY val", NULL, NULL);
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

	OCB_RETVAL_IF_CODE(!bin || bin->cb != 22, "Invalid SBinary", NULL, NULL);

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

	OCB_RETVAL_IF_CODE(!bin || bin->cb != 22, "Invalid SBinary", NULL, NULL);

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

	OCB_RETVAL_IF_CODE(!ft, "Invalid FILTIME", NULL, NULL);

	time = ft->dwHighDateTime;
	time = time << 32;
	time |= ft->dwLowDateTime;

	t = nt_time_to_unix(time);
	return ldb_timestring(mem_ctx, t);
}
