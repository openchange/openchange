/*
   OpenChange Server implementation.

   EMSABP: Address Book Provider implementation

   Copyright (C) Julien Kerihuel 2006-2014.
   Copyright (C) Pauline Khun 2006.

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

/**
   \file emsabp.c

   \brief Address Book Provider implementation
 */

#define TEVENT_DEPRECATED
#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/fault_util.h"
#include "dcesrv_exchange_nsp.h"
#include "ldb.h"

/* Expose samdb_connect prototype */
struct ldb_context *samdb_connect(TALLOC_CTX *, struct tevent_context *,
				  struct loadparm_context *,
				  struct auth_session_info *,
				  unsigned int);

/* Single connection to samdb */
static struct ldb_context *samdb_ctx = NULL;

/**
   \details Initialize ldb_context to samdb, creates one for all emsabp
   contexts

   \param lp_ctx pointer to the loadparm context
 */
static struct ldb_context *samdb_init(struct loadparm_context *lp_ctx)
{
	TALLOC_CTX		*mem_ctx;
	struct tevent_context	*ev;
	const char		*samdb_url;

	if (samdb_ctx) return samdb_ctx;

	mem_ctx = talloc_autofree_context();
	ev = tevent_context_init(mem_ctx);
	if (!ev) {
		OC_PANIC(false, ("Fail to initialize tevent_context\n"));
		return NULL;
	}
	tevent_loop_allow_nesting(ev);

	/* Retrieve samdb url (local or external) */
	samdb_url = lpcfg_parm_string(lp_ctx, NULL, "dcerpc_mapiproxy", "samdb_url");

	if (!samdb_url) {
		samdb_ctx = samdb_connect(mem_ctx, ev, lp_ctx, system_session(lp_ctx), 0);
	} else {
		samdb_ctx = samdb_connect_url(mem_ctx, ev, lp_ctx, system_session(lp_ctx),
					      LDB_FLG_RECONNECT, samdb_url);
	}

	return samdb_ctx;
}

/**
   \details Initialize the EMSABP context and open connections to
   Samba databases.

   \param lp_ctx pointer to the loadparm context
   \param tdb_ctx pointer to the EMSABP TDB context

   \return Allocated emsabp_context on success, otherwise NULL
 */
_PUBLIC_ struct emsabp_context *emsabp_init(struct loadparm_context *lp_ctx,
					    TDB_CONTEXT *tdb_ctx)
{
	TALLOC_CTX		*mem_ctx;
	struct emsabp_context	*emsabp_ctx;

	/* Sanity checks */
	if (!lp_ctx) return NULL;

	mem_ctx = talloc_named(NULL, 0, "emsabp_init");

	emsabp_ctx = talloc_zero(mem_ctx, struct emsabp_context);
	if (!emsabp_ctx) {
		talloc_free(mem_ctx);
		return NULL;
	}

	emsabp_ctx->mem_ctx = mem_ctx;

	/* Save a pointer to the loadparm context */
	emsabp_ctx->lp_ctx = lp_ctx;

	emsabp_ctx->samdb_ctx = samdb_init(lp_ctx);
	if (!emsabp_ctx->samdb_ctx) {
		talloc_free(mem_ctx);
		OC_DEBUG(0, "[nspi] Connection to \"sam.ldb\" failed");
		return NULL;
	}

	/* Reference the global TDB context to the current emsabp context */
	emsabp_ctx->tdb_ctx = tdb_ctx;

	/* Initialize a temporary (on-memory) TDB database to store
	 * temporary MId used within EMSABP */
	emsabp_ctx->ttdb_ctx = emsabp_tdb_init_tmp(emsabp_ctx->mem_ctx);
	if (!emsabp_ctx->ttdb_ctx) {
		talloc_free(mem_ctx);
		OC_PANIC(false, ("[nspi] Unable to create on-memory TDB database\n"));
		return NULL;
	}

	return emsabp_ctx;
}


_PUBLIC_ bool emsabp_destructor(void *data)
{
	struct emsabp_context	*emsabp_ctx = (struct emsabp_context *)data;

	if (emsabp_ctx) {
		if (emsabp_ctx->ttdb_ctx) {
			tdb_close(emsabp_ctx->ttdb_ctx);
		}

		talloc_free(emsabp_ctx->mem_ctx);
		return true;
	}

	return false;
}

_PUBLIC_ void emsabp_enable_debug(struct emsabp_context *emsabp_ctx)
{
	ldb_set_flags(emsabp_ctx->samdb_ctx, LDB_FLG_ENABLE_TRACING);
}

/**
   \details Get AD record for Mail-enabled account

   \param mem_ctx pointer to the session context
   \param emsabp_ctx pointer to the EMSABP context
   \param username User common name
   \param ldb_msg Pointer on pointer to ldb_message to return result to

   \return MAPI_E_SUCCESS on success, otherwise
   MAPI_E_NOT_ENOUGH_RESOURCES, MAPI_E_NOT_FOUND or
   MAPI_E_CORRUPT_STORE
 */
_PUBLIC_ enum MAPISTATUS emsabp_get_account_info(TALLOC_CTX *mem_ctx,
						 struct emsabp_context *emsabp_ctx,
						 const char *username,
						 struct ldb_message **ldb_msg)
{
	int			ret;
	int			msExchUserAccountControl;
	struct ldb_result	*res = NULL;
	const char * const	recipient_attrs[] = { "*", NULL };

	ret = ldb_search(emsabp_ctx->samdb_ctx, mem_ctx, &res,
			 ldb_get_default_basedn(emsabp_ctx->samdb_ctx),
			 LDB_SCOPE_SUBTREE, recipient_attrs, "sAMAccountName=%s",
			 ldb_binary_encode_string(mem_ctx, username));
	OPENCHANGE_RETVAL_IF((ret != LDB_SUCCESS || !res->count), MAPI_E_NOT_FOUND, NULL);

	/* Check if more than one record was found */
	OPENCHANGE_RETVAL_IF(res->count != 1, MAPI_E_CORRUPT_STORE, NULL);

	msExchUserAccountControl = ldb_msg_find_attr_as_int(res->msgs[0], "msExchUserAccountControl", -1);
	switch (msExchUserAccountControl) {
	case -1: /* msExchUserAccountControl attribute is not found */
		return MAPI_E_NOT_FOUND;
	case 0:
		*ldb_msg = res->msgs[0];
		return MAPI_E_SUCCESS;
	case 2: /* Account is disabled */
		*ldb_msg = res->msgs[0];
		return MAPI_E_ACCOUNT_DISABLED;
	default: /* Unknown value for msExchUserAccountControl attribute */
		return MAPI_E_CORRUPT_STORE;
	}

	/* We should never get here anyway */
	return MAPI_E_CORRUPT_STORE;
}

/**
   \details Check if the authenticated user belongs to the Exchange
   organization

   \param dce_call pointer to the session context
   \param emsabp_ctx pointer to the EMSABP context

   \return true on success, otherwise false
 */
_PUBLIC_ bool emsabp_verify_user(struct dcesrv_call_state *dce_call,
				 struct emsabp_context *emsabp_ctx)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	const char		*username = NULL, *exdn = NULL;
	char			*exdn0, *exdn1;
	struct ldb_message	*ldb_msg = NULL;

	username = dcesrv_call_account_name(dce_call);

	mem_ctx = talloc_named(emsabp_ctx->mem_ctx, 0, __FUNCTION__);
	if (!mem_ctx) {
		return false;
	}

	retval = emsabp_get_account_info(mem_ctx, emsabp_ctx, username, &ldb_msg);
	if (retval != MAPI_E_SUCCESS) goto end;

	// cache both account_name and organization upon success
	exdn = ldb_msg_find_attr_as_string(ldb_msg, "legacyExchangeDN", NULL);
	if (exdn == NULL) {
		OC_DEBUG(0, "User %s doesn't have legacyExchangeDN attribute", username);
		retval = MAPI_E_NOT_FOUND;
		goto end;
	}
	exdn0 = strstr(exdn, "/o=");
	exdn1 = strstr(exdn, "/ou=");
	if (!exdn0 || !exdn1) {
		OC_DEBUG(0, "User %s has bad formed legacyExchangeDN attribute: %s\n",
			  username, exdn);
		retval = MAPI_E_NOT_FOUND;
		goto end;
	}
	emsabp_ctx->organization_name = talloc_strndup(emsabp_ctx->mem_ctx, exdn0 + 3, exdn1 - exdn0 - 3);
	emsabp_ctx->account_name = talloc_strdup(emsabp_ctx->mem_ctx, username);
	if (!emsabp_ctx->organization_name || !emsabp_ctx->account_name) {
		retval = MAPI_E_NOT_ENOUGH_MEMORY;
	}
end:
	talloc_free(mem_ctx);
	return retval == MAPI_E_SUCCESS;
}


/**
   \details Check if the provided codepage is correct

   \param emsabp_ctx pointer to the EMSABP context
   \param CodePage the codepage identifier

   \note The prototype is currently incorrect, but we are looking for
   a better way to check codepage, maybe within AD. At the moment this
   function is just a wrapper over libmapi valid_codepage function.

   \return true on success, otherwise false
 */
_PUBLIC_ bool emsabp_verify_codepage(struct emsabp_context *emsabp_ctx,
				     uint32_t CodePage)
{
	return mapi_verify_cpid(CodePage);
}


/**
   \details Build an EphemeralEntryID structure

   \param emsabp_ctx pointer to the EMSABP context
   \param DisplayType the AB object display type
   \param MId the MId value
   \param ephEntryID pointer to the EphemeralEntryID returned by the
   function

   \return MAPI_E_SUCCESS on success, otherwise
   MAPI_E_NOT_ENOUGH_RESOURCES or MAPI_E_CORRUPT_STORE
 */
_PUBLIC_ enum MAPISTATUS emsabp_set_EphemeralEntryID(struct emsabp_context *emsabp_ctx,
						     uint32_t DisplayType, uint32_t MId,
						     struct EphemeralEntryID *ephEntryID)
{
	struct GUID	*guid = (struct GUID *) NULL;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ephEntryID, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);

	guid = (struct GUID *) samdb_ntds_objectGUID(emsabp_ctx->samdb_ctx);
	OPENCHANGE_RETVAL_IF(!guid, MAPI_E_CORRUPT_STORE, NULL);

	ephEntryID->ID_type = 0x87;
	ephEntryID->R1 = 0x0;
	ephEntryID->R2 = 0x0;
	ephEntryID->R3 = 0x0;
	ephEntryID->ProviderUID.ab[0] = (guid->time_low & 0xFF);
	ephEntryID->ProviderUID.ab[1] = ((guid->time_low >> 8)  & 0xFF);
	ephEntryID->ProviderUID.ab[2] = ((guid->time_low >> 16) & 0xFF);
	ephEntryID->ProviderUID.ab[3] = ((guid->time_low >> 24) & 0xFF);
	ephEntryID->ProviderUID.ab[4] = (guid->time_mid & 0xFF);
	ephEntryID->ProviderUID.ab[5] = ((guid->time_mid >> 8)  & 0xFF);
	ephEntryID->ProviderUID.ab[6] = (guid->time_hi_and_version & 0xFF);
	ephEntryID->ProviderUID.ab[7] = ((guid->time_hi_and_version >> 8) & 0xFF);
	memcpy(ephEntryID->ProviderUID.ab + 8,  guid->clock_seq, sizeof (uint8_t) * 2);
	memcpy(ephEntryID->ProviderUID.ab + 10, guid->node, sizeof (uint8_t) * 6);
	ephEntryID->R4 = 0x1;
	ephEntryID->DisplayType = DisplayType;
	ephEntryID->MId = MId;

	return MAPI_E_SUCCESS;
}


/**
   \details Map an EphemeralEntryID structure into a Binary_r structure

   \param mem_ctx pointer to the memory context
   \param ephEntryID pointer to the Ephemeral EntryID structure
   \param bin pointer to the Binary_r structure the server will return

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_INVALID_PARAMETER
 */
_PUBLIC_ enum MAPISTATUS emsabp_EphemeralEntryID_to_Binary_r(TALLOC_CTX *mem_ctx,
							     struct EphemeralEntryID *ephEntryID,
							     struct Binary_r *bin)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ephEntryID, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!bin, MAPI_E_INVALID_PARAMETER, NULL);

	bin->cb = sizeof (*ephEntryID);
	bin->lpb = talloc_array(mem_ctx, uint8_t, bin->cb);

	/* Copy EphemeralEntryID into bin->lpb */
	memset(bin->lpb, 0, bin->cb);
	bin->lpb[0] = ephEntryID->ID_type;
	bin->lpb[1] = ephEntryID->R1;
	bin->lpb[2] = ephEntryID->R2;
	bin->lpb[3] = ephEntryID->R3;
	memcpy(bin->lpb + 4, ephEntryID->ProviderUID.ab, 16);
	bin->lpb[20] = (ephEntryID->R4 & 0xFF);
	bin->lpb[21] = ((ephEntryID->R4 >> 8)  & 0xFF);
	bin->lpb[22] = ((ephEntryID->R4 >> 16) & 0xFF);
	bin->lpb[23] = ((ephEntryID->R4 >> 24) & 0xFF);
	bin->lpb[24] = (ephEntryID->DisplayType & 0xFF);
	bin->lpb[25] = ((ephEntryID->DisplayType >> 8)  & 0xFF);
	bin->lpb[26] = ((ephEntryID->DisplayType >> 16) & 0xFF);
	bin->lpb[27] = ((ephEntryID->DisplayType >> 24) & 0xFF);
	bin->lpb[28] = (ephEntryID->MId & 0xFF);
	bin->lpb[29] = ((ephEntryID->MId >> 8)  & 0xFF);
	bin->lpb[30] = ((ephEntryID->MId >> 16) & 0xFF);
	bin->lpb[31] = ((ephEntryID->MId >> 24) & 0xFF);

	return MAPI_E_SUCCESS;
}


/**
   \details Build a PermanentEntryID structure

   \param emsabp_ctx pointer to the EMSABP context
   \param DisplayType the AB object display type
   \param msg pointer to the LDB message
   \param permEntryID pointer to the PermanentEntryID returned by the
   function

   \note This function only covers DT_CONTAINER AddressBook
   objects. It should be extended in the future to support more
   containers.

   \return MAPI_E_SUCCESS on success, otherwise
   MAPI_E_NOT_ENOUGH_RESOURCES or MAPI_E_CORRUPT_STORE
 */
_PUBLIC_ enum MAPISTATUS emsabp_set_PermanentEntryID(struct emsabp_context *emsabp_ctx,
						     uint32_t DisplayType, struct ldb_message *msg,
						     struct PermanentEntryID *permEntryID)
{
	struct GUID		*guid = (struct GUID *) NULL;
	const struct ldb_val	*ldb_value = NULL;
	const char		*dn_str;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!permEntryID, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);


	permEntryID->ID_type = 0x0;
	permEntryID->R1 = 0x0;
	permEntryID->R2 = 0x0;
	permEntryID->R3 = 0x0;
	memcpy(permEntryID->ProviderUID.ab, GUID_NSPI, 16);
	permEntryID->R4 = 0x1;
	permEntryID->DisplayType = DisplayType;
	permEntryID->dn = NULL;

	if (!msg) {
		permEntryID->dn = talloc_strdup(emsabp_ctx->mem_ctx, "/");
		OPENCHANGE_RETVAL_IF(!permEntryID->dn, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
	} else if (DisplayType == DT_CONTAINER) {
		ldb_value = ldb_msg_find_ldb_val(msg, "objectGUID");
		OPENCHANGE_RETVAL_IF(!ldb_value, MAPI_E_CORRUPT_STORE, NULL);

		guid = talloc_zero(emsabp_ctx->mem_ctx, struct GUID);
		GUID_from_data_blob(ldb_value, guid);
		permEntryID->dn = talloc_asprintf(emsabp_ctx->mem_ctx, EMSABP_DN,
						  guid->time_low, guid->time_mid,
						  guid->time_hi_and_version,
						  guid->clock_seq[0],
						  guid->clock_seq[1],
						  guid->node[0], guid->node[1],
						  guid->node[2], guid->node[3],
						  guid->node[4], guid->node[5]);
		OPENCHANGE_RETVAL_IF(!permEntryID->dn, MAPI_E_NOT_ENOUGH_RESOURCES, guid);
		talloc_free(guid);

	}  else {
		dn_str = ldb_msg_find_attr_as_string(msg, "legacyExchangeDN", NULL);
		OPENCHANGE_RETVAL_IF(!dn_str, MAPI_E_CORRUPT_STORE, NULL);
		permEntryID->dn = talloc_strdup(emsabp_ctx->mem_ctx, dn_str);
		OPENCHANGE_RETVAL_IF(!permEntryID->dn, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Map a PermanentEntryID structure into a Binary_r
   structure (for PR_ENTRYID and PR_EMS_AB_PARENT_ENTRYID properties)

   \param mem_ctx pointer to the memory context
   \param permEntryID pointer to the Permanent EntryID structure
   \param bin pointer to the Binary_r structure the server will return

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_INVALID_PARAMETER
 */
_PUBLIC_ enum MAPISTATUS emsabp_PermanentEntryID_to_Binary_r(TALLOC_CTX *mem_ctx,
							     struct PermanentEntryID *permEntryID,
							     struct Binary_r *bin)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!permEntryID, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!bin, MAPI_E_INVALID_PARAMETER, NULL);

	/* Remove const char * size and replace it with effective dn string length */
	bin->cb = 28 + strlen(permEntryID->dn) + 1;
	bin->lpb = talloc_array(mem_ctx, uint8_t, bin->cb);

	/* Copy PermanantEntryID intro bin->lpb */
	memset(bin->lpb, 0, bin->cb);
	bin->lpb[0] = permEntryID->ID_type;
	bin->lpb[1] = permEntryID->R1;
	bin->lpb[2] = permEntryID->R2;
	bin->lpb[3] = permEntryID->R3;
	memcpy(bin->lpb + 4, permEntryID->ProviderUID.ab, 16);
	bin->lpb[20] = (permEntryID->R4 & 0xFF);
	bin->lpb[21] = ((permEntryID->R4 >> 8)  & 0xFF);
	bin->lpb[22] = ((permEntryID->R4 >> 16) & 0xFF);
	bin->lpb[23] = ((permEntryID->R4 >> 24) & 0xFF);
	bin->lpb[24] = (permEntryID->DisplayType & 0xFF);
	bin->lpb[25] = ((permEntryID->DisplayType >> 8)  & 0xFF);
	bin->lpb[26] = ((permEntryID->DisplayType >> 16) & 0xFF);
	bin->lpb[27] = ((permEntryID->DisplayType >> 24) & 0xFF);
	memcpy(bin->lpb + 28, permEntryID->dn, strlen(permEntryID->dn) + 1);

	return MAPI_E_SUCCESS;
}


/**
   \details Find the attribute matching the specified property tag and
   return the associated data.

   \param mem_ctx pointer to the memory context
   \param emsabp_ctx pointer to the EMSABP context
   \param msg pointer to the LDB message
   \param ulPropTag the property tag to lookup
   \param MId Minimal Entry ID associated to the current message
   \param dwFlags bit flags specifying whether or not the server must
   return the values of the property PidTagEntryId in the Ephemeral
   or Permanent Entry ID format


   \note This implementation is at the moment limited to MAILUSER,
   which means we arbitrary set PR_OBJECT_TYPE and PR_DISPLAY_TYPE
   while we should have a generic method to fill these properties.

   \return Valid generic pointer on success, otherwise NULL
 */
_PUBLIC_ void *emsabp_query(TALLOC_CTX *mem_ctx, struct emsabp_context *emsabp_ctx,
			    struct ldb_message *msg, uint32_t ulPropTag, uint32_t MId,
			    uint32_t dwFlags)
{
	enum MAPISTATUS			retval;
	void				*data = (void *) NULL;
	const char			*attribute;
	const char			*ref_attribute;
	const char			*ldb_string = NULL;
	const struct ldb_val		*ldb_val;
	char				*tmp_str;
	struct Binary_r			*bin;
	struct StringArray_r		*mvszA;
	struct EphemeralEntryID		ephEntryID;
	struct PermanentEntryID         permEntryID;
	struct ldb_message		*msg2 = NULL;
	struct ldb_message_element	*ldb_element;
	int				ret;
	const char			*dn = NULL;
	uint32_t			i;

	/* Step 1. Fill attributes not in AD but created using EMSABP databases */
	switch (ulPropTag) {
	case PR_ADDRTYPE:
	case PR_ADDRTYPE_UNICODE:
		data = (void *) talloc_strdup(mem_ctx, EMSABP_ADDRTYPE /* "SMTP" */);
		return data;
	case PR_OBJECT_TYPE:
		data = talloc_zero(mem_ctx, uint32_t);
		*((uint32_t *)data) = MAPI_MAILUSER;
		return data;
	case PR_DISPLAY_TYPE:
		data = talloc_zero(mem_ctx, uint32_t);
		*((uint32_t *)data) = DT_MAILUSER;
		return data;
	case PR_SEND_RICH_INFO:
		data = talloc_zero(mem_ctx, uint8_t);
		*((uint8_t *)data) = false;
		return data;
	case PR_SEND_INTERNET_ENCODING:
		data = talloc_zero(mem_ctx, uint32_t);
		*((uint32_t *)data) = 0x00160000;
		return data;
	case PidTagAddressBookHierarchicalIsHierarchicalGroup:
		data = talloc_zero(mem_ctx, uint32_t);
		*((uint32_t *)data) = 0;
		return data;
	case PR_ENTRYID:
	case PR_ORIGINAL_ENTRYID:
		bin = talloc(mem_ctx, struct Binary_r);
		if (dwFlags & fEphID) {
			retval = emsabp_set_EphemeralEntryID(emsabp_ctx, DT_MAILUSER, MId, &ephEntryID);
			if (retval != MAPI_E_SUCCESS) {
				talloc_free(bin);
				return NULL;
			}
			retval = emsabp_EphemeralEntryID_to_Binary_r(mem_ctx, &ephEntryID, bin);
		} else {
			retval = emsabp_set_PermanentEntryID(emsabp_ctx, DT_MAILUSER, msg, &permEntryID);
			if (retval != MAPI_E_SUCCESS) {
				talloc_free(bin);
				return NULL;
			}
			retval = emsabp_PermanentEntryID_to_Binary_r(mem_ctx, &permEntryID, bin);
		}
		return bin;
	case PR_SEARCH_KEY:
		/* retrieve email address attribute, i.e. legacyExchangeDN */
		ldb_string = ldb_msg_find_attr_as_string(msg, emsabp_property_get_attribute(PidTagEmailAddress), NULL);
		if (!ldb_string) return NULL;
		tmp_str = talloc_strdup_upper(mem_ctx, ldb_string);
		if (!tmp_str) return NULL;
		/* make binary for PR_SEARCH_KEY */
		bin = talloc(mem_ctx, struct Binary_r);
		bin->lpb = (uint8_t *)talloc_asprintf(mem_ctx, "EX:%s", tmp_str);
		bin->cb = strlen((const char *)bin->lpb) + 1;
		talloc_free(tmp_str);
		return bin;
	case PR_INSTANCE_KEY:
		bin = talloc_zero(mem_ctx, struct Binary_r);
		bin->cb = 4;
		bin->lpb = talloc_array(mem_ctx, uint8_t, bin->cb);
		memset(bin->lpb, 0, bin->cb);
		bin->lpb[0] = MId & 0xFF;
		bin->lpb[1] = (MId >> 8)  & 0xFF;
		bin->lpb[2] = (MId >> 16) & 0xFF;
		bin->lpb[3] = (MId >> 24) & 0xFF;
		return bin;
	case PR_EMS_AB_OBJECT_GUID:
		ldb_val = ldb_msg_find_ldb_val(msg, emsabp_property_get_attribute(PR_EMS_AB_OBJECT_GUID));
		if (!ldb_val) return NULL;
		bin = talloc_zero(mem_ctx, struct Binary_r);
		bin->cb = ldb_val->length;
		bin->lpb = talloc_memdup(bin, ldb_val->data, ldb_val->length);
		return bin;
	default:
		break;
	}

	/* Step 2. Retrieve the attribute name associated to ulPropTag and handle the ref case */
	attribute = emsabp_property_get_attribute(ulPropTag);
	if (!attribute) return NULL;

	ret = emsabp_property_is_ref(ulPropTag);
	if (ret == 1) {
		ref_attribute = emsabp_property_get_ref_attr(ulPropTag);
		if (ref_attribute) {
			dn = ldb_msg_find_attr_as_string(msg, attribute, NULL);
			retval = emsabp_search_dn(emsabp_ctx, dn, &msg2);
			if (retval != MAPI_E_SUCCESS) {
				return NULL;
			}
			attribute = ref_attribute;
		}
	} else {
		msg2 = msg;
	}

	/* Step 3. Retrieve data associated to the attribute/tag */
	switch (ulPropTag & 0xFFFF) {
	case PT_STRING8:
	case PT_UNICODE:
		ldb_string = ldb_msg_find_attr_as_string(msg2, attribute, NULL);
		if (!ldb_string) return NULL;
		data = talloc_strdup(mem_ctx, ldb_string);
		break;
	case PT_MV_STRING8:
	case PT_MV_UNICODE:
		ldb_element = ldb_msg_find_element(msg2, attribute);
		if (!ldb_element) return NULL;

		mvszA = talloc(mem_ctx, struct StringArray_r);
		mvszA->cValues = ldb_element[0].num_values & 0xFFFFFFFF;
		mvszA->lppszA = talloc_array(mem_ctx, const char *, mvszA->cValues);
		for (i = 0; i < mvszA->cValues; i++) {
			mvszA->lppszA[i] = talloc_strdup(mem_ctx, (char *)ldb_element->values[i].data);
		}
		data = (void *) mvszA;
		break;
	default:
		OC_DEBUG(3, "Unsupported property type: 0x%x", (ulPropTag & 0xFFFF));
		break;
	}

	return data;
}


/**
   \details Build the SRow array entry for the specified ldb_message.

   \param mem_ctx pointer to the memory context
   \param emsabp_ctx pointer to the EMSABP context
   \param aRow pointer to the SRow structure where results will be stored
   \param ldb_msg ldb_message record to fetch results from
   \param MId MId of the entry to fetch (may be 0)
   \param dwFlags input call flags (default to 0)
   \param pPropTags pointer to the property tags array

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS emsabp_fetch_attrs_from_msg(TALLOC_CTX *mem_ctx,
						     struct emsabp_context *emsabp_ctx,
						     struct PropertyRow_r *aRow,
						     struct ldb_message *ldb_msg,
						     uint32_t MId, uint32_t dwFlags,
						     struct SPropTagArray *pPropTags)
{
	enum MAPISTATUS	retval;
	const char	*dn;
	void		*data;
	uint32_t	ulPropTag;
	int		i;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!pPropTags, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!emsabp_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!aRow, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ldb_msg, MAPI_E_INVALID_PARAMETER, NULL);

	/* Step 0. Create MId if necessary */
	if (MId == 0) {
		dn = ldb_msg_find_attr_as_string(ldb_msg, "distinguishedName", NULL);
		OPENCHANGE_RETVAL_IF(!dn, MAPI_E_CORRUPT_DATA, NULL);
		retval = emsabp_tdb_fetch_MId(emsabp_ctx->ttdb_ctx, dn, &MId);
		if (retval) {
			retval = emsabp_tdb_insert(emsabp_ctx->ttdb_ctx, dn);
			OPENCHANGE_RETVAL_IF(retval, MAPI_E_CORRUPT_STORE, NULL);

			retval = emsabp_tdb_fetch_MId(emsabp_ctx->ttdb_ctx, dn, &MId);
			OPENCHANGE_RETVAL_IF(retval, MAPI_E_CORRUPT_STORE, NULL);
		}
	}

	/* Step 1. Retrieve property values and build aRow */
	aRow->Reserved = 0x0;
	aRow->cValues = pPropTags->cValues;
	aRow->lpProps = talloc_array(mem_ctx, struct PropertyValue_r, aRow->cValues);

	for (i = 0; i < aRow->cValues; i++) {
		ulPropTag = pPropTags->aulPropTag[i];
		data = emsabp_query(mem_ctx, emsabp_ctx, ldb_msg, ulPropTag, MId, dwFlags);
		if (!data) {
			ulPropTag = (ulPropTag & 0xFFFF0000) | PT_ERROR;
		}

		aRow->lpProps[i].ulPropTag = (enum MAPITAGS) ulPropTag;
		aRow->lpProps[i].dwAlignPad = 0x0;
		set_PropertyValue(&(aRow->lpProps[i]), data);
	}

	return MAPI_E_SUCCESS;
}

/**
   \details Builds the SRow array entry for the specified MId.

   The function retrieves the DN associated to the specified MId
   within its on-memory TDB database. It next fetches the LDB record
   matching the DN and finally retrieve the requested properties for
   this record.

   \param mem_ctx pointer to the memory context
   \param emsabp_ctx pointer to the EMSABP context
   \param aRow pointer to the SRow structure where results will be
   stored
   \param MId MId to fetch properties for
   \param dwFlags bit flags specifying whether or not the server must
   return the values of the property PidTagEntryId in the Ephemeral
   or Permanent Entry ID format
   \param pPropTags pointer to the property tags array

   \note We currently assume records are users.ldb

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS emsabp_fetch_attrs(TALLOC_CTX *mem_ctx, struct emsabp_context *emsabp_ctx,
					    struct PropertyRow_r *aRow, uint32_t MId, uint32_t dwFlags,
					    struct SPropTagArray *pPropTags)
{
	enum MAPISTATUS		retval;
	char			*dn;
	const char * const	recipient_attrs[] = { "*", NULL };
	struct ldb_result	*res = NULL;
	struct ldb_dn		*ldb_dn = NULL;
	int			ret;
	uint32_t		ulPropTag;
	void			*data;
	int			i;

	/* Step 0. Try to Retrieve the dn associated to the MId first from temp TDB (users) */
	retval = emsabp_tdb_fetch_dn_from_MId(mem_ctx, emsabp_ctx->ttdb_ctx, MId, &dn);
	if (retval != MAPI_E_SUCCESS) {
		/* If it fails try to retrieve it from the on-disk TDB database (conf) */
		retval = emsabp_tdb_fetch_dn_from_MId(mem_ctx, emsabp_ctx->tdb_ctx, MId, &dn);
	}
	OPENCHANGE_RETVAL_IF(retval, MAPI_E_INVALID_BOOKMARK, NULL);

	/* Step 1. Fetch the LDB record */
	ldb_dn = ldb_dn_new(mem_ctx, emsabp_ctx->samdb_ctx, dn);
	OPENCHANGE_RETVAL_IF(!ldb_dn_validate(ldb_dn), MAPI_E_CORRUPT_STORE, NULL);

	ret = ldb_search(emsabp_ctx->samdb_ctx, emsabp_ctx->mem_ctx, &res, ldb_dn, LDB_SCOPE_BASE,
			 recipient_attrs, NULL);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count || res->count != 1, MAPI_E_CORRUPT_STORE, NULL);

	/* Step 2. Retrieve property values and build aRow */
	aRow->Reserved = 0x0;
	aRow->cValues = pPropTags->cValues;
	aRow->lpProps = talloc_array(mem_ctx, struct PropertyValue_r, aRow->cValues);

	for (i = 0; i < aRow->cValues; i++) {
		ulPropTag = pPropTags->aulPropTag[i];
		data = emsabp_query(mem_ctx, emsabp_ctx, res->msgs[0], ulPropTag, MId, dwFlags);
		if (!data) {
			ulPropTag &= 0xFFFF0000;
			ulPropTag += PT_ERROR;
		}

		aRow->lpProps[i].ulPropTag = (enum MAPITAGS) ulPropTag;
		aRow->lpProps[i].dwAlignPad = 0x0;
		set_PropertyValue(&(aRow->lpProps[i]), data);
	}


	return MAPI_E_SUCCESS;
}


/**
   \details Builds the SRow array entry for the specified table
   record.

   \param mem_ctx pointer to the memory context
   \param emsabp_ctx pointer to the EMSABP context
   \param aRow pointer to the SRow structure where results will be
   stored
   \param dwFlags flags controlling whether strings should be unicode
   encoded or not
   \param permEntryID pointer to the current record Permanent
   EntryID
   \param parentPermEntryID pointer to the parent record Permanent
   EntryID
   \param msg pointer to the LDB message for current record
   \param child boolean value specifying whether current record is
   root or child within containers hierarchy

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS emsabp_table_fetch_attrs(TALLOC_CTX *mem_ctx, struct emsabp_context *emsabp_ctx,
						  struct PropertyRow_r *aRow, uint32_t dwFlags,
						  struct PermanentEntryID *permEntryID,
						  struct PermanentEntryID *parentPermEntryID,
						  struct ldb_message *msg, bool child)
{
	enum MAPISTATUS			retval;
	struct SPropTagArray		*SPropTagArray;
	struct PropertyValue_r		lpProps;
	int				proptag;
	uint32_t			i;
	uint32_t			containerID = 0;
	const char			*dn = NULL;

	/* Step 1. Build the array of properties to fetch and map */
	if (child == false) {
		SPropTagArray = set_SPropTagArray(mem_ctx, 0x6,
						  PR_ENTRYID,
						  PR_CONTAINER_FLAGS,
						  PR_DEPTH,
						  PR_EMS_AB_CONTAINERID,
						  ((dwFlags & NspiUnicodeStrings) ? PR_DISPLAY_NAME_UNICODE : PR_DISPLAY_NAME),
						  PR_EMS_AB_IS_MASTER);
	} else {
		SPropTagArray = set_SPropTagArray(mem_ctx, 0x7,
						  PR_ENTRYID,
						  PR_CONTAINER_FLAGS,
						  PR_DEPTH,
						  PR_EMS_AB_CONTAINERID,
						  ((dwFlags & NspiUnicodeStrings) ? PR_DISPLAY_NAME_UNICODE : PR_DISPLAY_NAME),
						  PR_EMS_AB_IS_MASTER,
						  PR_EMS_AB_PARENT_ENTRYID);
	}

	/* Step 2. Allocate SPropValue array and update SRow cValues field */
	aRow->Reserved = 0x0;
	aRow->cValues = 0x0;
	aRow->lpProps = talloc_zero(mem_ctx, struct PropertyValue_r);

	/* Step 3. Global Address List or real container */
	if (!msg) {
		/* Global Address List record is constant */
		for (i = 0; i < SPropTagArray->cValues; i++) {
			lpProps.ulPropTag = SPropTagArray->aulPropTag[i];
			lpProps.dwAlignPad = 0x0;

			switch (SPropTagArray->aulPropTag[i]) {
			case PR_ENTRYID:
				emsabp_PermanentEntryID_to_Binary_r(mem_ctx, permEntryID, &(lpProps.value.bin));
				break;
			case PR_CONTAINER_FLAGS:
				lpProps.value.l =  AB_RECIPIENTS | AB_UNMODIFIABLE;
				break;
			case PR_DEPTH:
				lpProps.value.l = 0x0;
				break;
			case PR_EMS_AB_CONTAINERID:
				lpProps.value.l = 0x0;
				break;
			case PidTagDisplayName:
				/* This value is temporal to workaround PropertyRow_addprop internals */
				/* It will be set to NULL aftr the call to PropertyRow_addprop */
				lpProps.value.lpszW = "t";
				break;
			case PR_EMS_AB_IS_MASTER:
				lpProps.value.b = false;
				break;
			default:
				break;
			}
			PropertyRow_addprop(aRow, lpProps);
			/* PropertyRow_addprop internals overwrite with MAPI_E_NOT_FOUND when data is NULL */
			if (SPropTagArray->aulPropTag[i] == PR_DISPLAY_NAME ||
			    SPropTagArray->aulPropTag[i] == PR_DISPLAY_NAME_UNICODE) {
				aRow->lpProps[aRow->cValues - 1].value.lpszA = NULL;
				aRow->lpProps[aRow->cValues - 1].value.lpszW = NULL;
			}
		}
	} else {
		for (i = 0; i < SPropTagArray->cValues; i++) {
			lpProps.ulPropTag = SPropTagArray->aulPropTag[i];
			lpProps.dwAlignPad = 0x0;

			switch (SPropTagArray->aulPropTag[i]) {
			case PR_ENTRYID:
				emsabp_PermanentEntryID_to_Binary_r(mem_ctx, permEntryID, &(lpProps.value.bin));
				break;
			case PR_CONTAINER_FLAGS:
				if (child) {
					lpProps.value.l = AB_RECIPIENTS | AB_UNMODIFIABLE;
				} else {
					lpProps.value.l = AB_RECIPIENTS | AB_SUBCONTAINERS | AB_UNMODIFIABLE;
				}
				break;
			case PR_DEPTH:
				lpProps.value.l = child ? 0x1 : 0x0;
				break;
			case PR_EMS_AB_CONTAINERID:
				dn = ldb_msg_find_attr_as_string(msg, "distinguishedName", NULL);
				retval = emsabp_tdb_fetch_MId(emsabp_ctx->tdb_ctx, dn, &containerID);
				if (retval) {
					retval = emsabp_tdb_insert(emsabp_ctx->tdb_ctx, dn);
					OPENCHANGE_RETVAL_IF(retval, MAPI_E_CORRUPT_STORE, NULL);
					retval = emsabp_tdb_fetch_MId(emsabp_ctx->tdb_ctx, dn, &containerID);
					OPENCHANGE_RETVAL_IF(retval, MAPI_E_CORRUPT_STORE, NULL);
				}
				lpProps.value.l = containerID;
				break;
			case PidTagDisplayName_string8:
				lpProps.value.lpszA = talloc_strdup(mem_ctx, ldb_msg_find_attr_as_string(msg, "displayName", NULL));
				if (!lpProps.value.lpszA) {
					proptag = (int) lpProps.ulPropTag;
					proptag &= 0xFFFF0000;
					proptag += PT_ERROR;
					lpProps.ulPropTag = (enum MAPITAGS) proptag;
				}
				break;
			case PidTagDisplayName:
				lpProps.value.lpszW = talloc_strdup(mem_ctx, ldb_msg_find_attr_as_string(msg, "displayName", NULL));
				if (!lpProps.value.lpszW) {
					proptag = (int) lpProps.ulPropTag;
					proptag &= 0xFFFF0000;
					proptag += PT_ERROR;
					lpProps.ulPropTag = (enum MAPITAGS) proptag;
				}
				break;
			case PR_EMS_AB_IS_MASTER:
				/* FIXME: harcoded value - no load balancing */
				lpProps.value.l = 0x0;
				break;
			case PR_EMS_AB_PARENT_ENTRYID:
				emsabp_PermanentEntryID_to_Binary_r(mem_ctx, parentPermEntryID, &lpProps.value.bin);
				break;
			default:
				break;
			}
			PropertyRow_addprop(aRow, lpProps);
		}
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve and build the HierarchyTable requested by
   GetSpecialTable NSPI call

   \param mem_ctx pointer to the memory context
   \param emsabp_ctx pointer to the EMSABP context
   \param dwFlags flags controlling whether strings should be UNICODE
   or not
   \param SRowSet pointer on pointer to the output SRowSet array

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_CORRUPT_STORE
 */
_PUBLIC_ enum MAPISTATUS emsabp_get_HierarchyTable(TALLOC_CTX *mem_ctx, struct emsabp_context *emsabp_ctx,
						   uint32_t dwFlags, struct PropertyRowSet_r **SRowSet)
{
	enum MAPISTATUS			retval;
	struct PropertyRow_r			*aRow;
	struct PermanentEntryID		gal;
	struct PermanentEntryID		parentPermEntryID;
	struct PermanentEntryID		permEntryID;
	enum ldb_scope			scope = LDB_SCOPE_SUBTREE;
	struct ldb_request		*req;
	struct ldb_result		*res = NULL;
	struct ldb_dn			*ldb_dn = NULL;
	struct ldb_control		**controls;
	const char * const		recipient_attrs[] = { "*", NULL };
	const char			*control_strings[2] = { "server_sort:0:0:displayName", NULL };
	const char			*addressBookRoots;
	int				ret;
	uint32_t			aRow_idx;
	uint32_t			i;

	/* Step 1. Build the 'Global Address List' object using PermanentEntryID */
	aRow = talloc_zero(mem_ctx, struct PropertyRow_r);
	OPENCHANGE_RETVAL_IF(!aRow, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
	aRow_idx = 0;

	retval = emsabp_set_PermanentEntryID(emsabp_ctx, DT_CONTAINER, NULL, &gal);
	OPENCHANGE_RETVAL_IF(retval, retval, aRow);

	retval = emsabp_table_fetch_attrs(mem_ctx, emsabp_ctx, &aRow[aRow_idx], dwFlags, &gal, NULL, NULL, false);
	aRow_idx++;

	/* Step 2. Retrieve the object pointed by addressBookRoots attribute: 'All Address Lists' */
	ret = ldb_search(emsabp_ctx->samdb_ctx, emsabp_ctx->mem_ctx, &res,
			 ldb_get_config_basedn(emsabp_ctx->samdb_ctx),
			 scope, recipient_attrs, "(addressBookRoots=*)");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_CORRUPT_STORE, aRow);

	addressBookRoots = ldb_msg_find_attr_as_string(res->msgs[0], "addressBookRoots", NULL);
	OPENCHANGE_RETVAL_IF(!addressBookRoots, MAPI_E_CORRUPT_STORE, aRow);

	ldb_dn = ldb_dn_new(emsabp_ctx->mem_ctx, emsabp_ctx->samdb_ctx, addressBookRoots);
	talloc_free(res);
	OPENCHANGE_RETVAL_IF(!ldb_dn_validate(ldb_dn), MAPI_E_CORRUPT_STORE, aRow);

	scope = LDB_SCOPE_BASE;
	ret = ldb_search(emsabp_ctx->samdb_ctx, emsabp_ctx->mem_ctx, &res, ldb_dn,
			 scope, recipient_attrs, NULL);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count || res->count != 1, MAPI_E_CORRUPT_STORE, aRow);

	aRow = talloc_realloc(mem_ctx, aRow, struct PropertyRow_r, aRow_idx + 1);
	retval = emsabp_set_PermanentEntryID(emsabp_ctx, DT_CONTAINER, res->msgs[0], &parentPermEntryID);
	emsabp_table_fetch_attrs(mem_ctx, emsabp_ctx, &aRow[aRow_idx], dwFlags, &parentPermEntryID, NULL, res->msgs[0], false);
	aRow_idx++;
	talloc_free(res);

	/* Step 3. Retrieve 'All Address Lists' subcontainers */
	res = talloc_zero(mem_ctx, struct ldb_result);
	OPENCHANGE_RETVAL_IF(!res, MAPI_E_NOT_ENOUGH_RESOURCES, aRow);

	controls = ldb_parse_control_strings(emsabp_ctx->samdb_ctx, emsabp_ctx->mem_ctx, control_strings);
	ret = ldb_build_search_req(&req, emsabp_ctx->samdb_ctx, emsabp_ctx->mem_ctx,
				   ldb_dn, LDB_SCOPE_SUBTREE, "(purportedSearch=*)",
				   recipient_attrs, controls, res, ldb_search_default_callback, NULL);

	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		talloc_free(aRow);
		return MAPI_E_CORRUPT_STORE;
	}

	ret = ldb_request(emsabp_ctx->samdb_ctx, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}
	talloc_free(req);

	if (ret != LDB_SUCCESS || !res->count) {
		talloc_free(res);
		talloc_free(aRow);
		return MAPI_E_CORRUPT_STORE;
	}

	aRow = talloc_realloc(mem_ctx, aRow, struct PropertyRow_r, aRow_idx + res->count + 1);

	for (i = 0; res->msgs[i]; i++) {
		retval = emsabp_set_PermanentEntryID(emsabp_ctx, DT_CONTAINER, res->msgs[i], &permEntryID);
		emsabp_table_fetch_attrs(mem_ctx, emsabp_ctx, &aRow[aRow_idx], dwFlags, &permEntryID, &parentPermEntryID, res->msgs[i], true);
		talloc_free(permEntryID.dn);
		memset(&permEntryID, 0, sizeof (permEntryID));
		aRow_idx++;
	}
	talloc_free(res);
	talloc_free(parentPermEntryID.dn);

	/* Step 4. Build output SRowSet */
	SRowSet[0]->cRows = aRow_idx;
	SRowSet[0]->aRow = aRow;

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve and build the CreationTemplates Table requested
   by GetSpecialTable NSPI call

   \param mem_ctx pointer to the memory context
   \param emsabp_ctx pointer to the EMSABP context
   \param dwFlags flags controlling whether strings should be UNICODE
   or not
   \param SRowSet pointer on pointer to the output SRowSet array

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_CORRUPT_STORE
 */
_PUBLIC_ enum MAPISTATUS emsabp_get_CreationTemplatesTable(TALLOC_CTX *mem_ctx, struct emsabp_context *emsabp_ctx,
							   uint32_t dwFlags, struct PropertyRowSet_r **SRowSet)
{
	return MAPI_E_SUCCESS;
}


/**
    \details Include to some ldap filter the organizational restriction of the
    current user

    \param emsabp_ctx pointer to the EMSABP context, here we have cached
    the organization name
    \param filter the ldap filter string: something like: (&(...)(...))
    \param expression pointer on pointer to char to return the new ldap filter
    with organization restriction
 */
static enum MAPISTATUS emsabp_include_organization_restriction(struct emsabp_context *emsabp_ctx, const char *filter, char **expression)
{
	OPENCHANGE_RETVAL_IF(!expression, MAPI_E_BAD_VALUE, NULL);
	OPENCHANGE_RETVAL_IF(!filter, MAPI_E_BAD_VALUE, NULL);
	OPENCHANGE_RETVAL_IF(!emsabp_ctx, MAPI_E_BAD_VALUE, NULL);
	OPENCHANGE_RETVAL_IF(!emsabp_ctx->organization_name, MAPI_E_NOT_INITIALIZED, NULL);

	*expression = talloc_asprintf(emsabp_ctx->mem_ctx,
				      "(&(legacyExchangeDN=/o=%s/ou=*)%s)",
				      emsabp_ctx->organization_name, filter);

	OPENCHANGE_RETVAL_IF(!*expression, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Search Active Directory given input search criterias. The
   function associates for each records returned by the search a
   unique session Minimal Entry ID and a LDB message.

   \param mem_ctx pointer to the memory context
   \param emsabp_ctx pointer to the EMSABP context
   \param MIds pointer to the list of MIds the function returns
   \param restriction pointer to restriction rules to apply to the
   search
   \param pStat pointer the STAT structure associated to the search
   \param limit the limit number of results the function can return

   \note SortTypePhoneticDisplayName sort type is currently not supported.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS emsabp_search(TALLOC_CTX *mem_ctx, struct emsabp_context *emsabp_ctx,
				       struct PropertyTagArray_r *MIds, struct Restriction_r *restriction,
				       struct STAT *pStat, uint32_t limit)
{
	TALLOC_CTX			*local_mem_ctx;
	enum MAPISTATUS			retval;
	struct ldb_result		*ldb_res = NULL;
	struct PropertyRestriction_r	*res_prop = NULL;
	const char * const		recipient_attrs[] = { "*", NULL };
	uint32_t			i;
	const char			*dn;
	char				*fmt_str, *search_filter = NULL;
	const char			*fmt_attr;
	char				*attr;
	int				ldb_ret;
	struct ldb_server_sort_control	**ldb_sort_controls = NULL;
	struct ldb_request		*ldb_req;

	/* Step 0. Sanity Checks (MS-NSPI Server Processing Rules) */
	if (pStat->SortType == SortTypePhoneticDisplayName) {
		return MAPI_E_CALL_FAILED;
	}

	if (((pStat->SortType == SortTypeDisplayName) || (pStat->SortType == SortTypePhoneticDisplayName)) &&
	    (pStat->ContainerID && (emsabp_tdb_lookup_MId(emsabp_ctx->tdb_ctx, pStat->ContainerID) == false))) {
		return MAPI_E_INVALID_BOOKMARK;
	}

	if (restriction && (pStat->SortType != SortTypeDisplayName) &&
	    (pStat->SortType != SortTypePhoneticDisplayName)) {
		return MAPI_E_CALL_FAILED;
	}

	local_mem_ctx = talloc_new(NULL);
	OPENCHANGE_RETVAL_IF(local_mem_ctx == NULL, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	/* Step 1. Apply sort type, restriction and retrieve results from AD */

	/* We sort only for displayName because phoneticDisplayName is not yet supported */
	if (pStat->SortType == SortTypeDisplayName) {
		ldb_sort_controls = talloc_zero_array(local_mem_ctx, struct ldb_server_sort_control *, 2);
		OPENCHANGE_RETVAL_IF(ldb_sort_controls == NULL, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

		ldb_sort_controls[0] = talloc(local_mem_ctx, struct ldb_server_sort_control);
		OPENCHANGE_RETVAL_IF(ldb_sort_controls[0] == NULL, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

		ldb_sort_controls[0]->attributeName = talloc_strdup(local_mem_ctx, "displayName");
		OPENCHANGE_RETVAL_IF(ldb_sort_controls[0]->attributeName == NULL, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

		ldb_sort_controls[0]->orderingRule = NULL;
	}

	if (restriction) {
		/* FIXME: We only support RES_PROPERTY restriction */
		OPENCHANGE_RETVAL_IF((uint32_t)restriction->rt != RES_PROPERTY, MAPI_E_TOO_COMPLEX, local_mem_ctx);

		res_prop = (struct PropertyRestriction_r *)&(restriction->res.resProperty);
		fmt_attr = emsabp_property_get_attribute(res_prop->ulPropTag);
		OPENCHANGE_RETVAL_IF(fmt_attr == NULL, MAPI_E_NO_SUPPORT, local_mem_ctx);

		attr = (char *)get_PropertyValue_data(res_prop->lpProp);
		OPENCHANGE_RETVAL_IF(attr == NULL, MAPI_E_NO_SUPPORT, local_mem_ctx);

		if ((res_prop->ulPropTag & 0xFFFF) == 0x101e) {
			struct StringArray_r *attr_ml = (struct StringArray_r *) get_PropertyValue_data(res_prop->lpProp);
			attr = (char *)attr_ml->lppszA[0];
		} else {
			attr = (char *)get_PropertyValue_data(res_prop->lpProp);
		}
		OPENCHANGE_RETVAL_IF(attr == NULL, MAPI_E_NO_SUPPORT, local_mem_ctx);

		/* Special case: anr doesn't return correct result with partial search */
		if (!strcmp(fmt_attr, "anr")) {
			fmt_str = talloc_asprintf(local_mem_ctx, "(&(objectClass=user)(|(%s=%s)(userPrincipalName=%s))(!(objectClass=computer)))", fmt_attr, attr, attr);
		} else if (!strcmp(fmt_attr, "legacyExchangeDN")) {
			fmt_str = talloc_asprintf(local_mem_ctx, "(&(objectClass=user)(|(%s=%s)(%s%s)(anr=%s))(!(objectClass=computer)))", fmt_attr, attr, fmt_attr, attr, attr);
		} else {
			fmt_str = talloc_asprintf(local_mem_ctx, "(&(objectClass=user)(%s=*%s*)(!(objectClass=computer)))", fmt_attr, attr);
		}
		OPENCHANGE_RETVAL_IF(fmt_str == NULL, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	} else {
		fmt_str = talloc_strdup(local_mem_ctx, "(&(objectClass=user)(displayName=*)(!(objectClass=computer)))");
		OPENCHANGE_RETVAL_IF(fmt_str == NULL, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
		attr = NULL;
	}

	retval = emsabp_include_organization_restriction(emsabp_ctx, fmt_str, &search_filter);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, local_mem_ctx);

	ldb_res = talloc_zero(local_mem_ctx, struct ldb_result);
	OPENCHANGE_RETVAL_IF(ldb_res == NULL, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

	ldb_req = NULL;
	ldb_ret = ldb_build_search_req(&ldb_req, emsabp_ctx->samdb_ctx, local_mem_ctx,
				       ldb_get_default_basedn(emsabp_ctx->samdb_ctx),
				       LDB_SCOPE_SUBTREE,
				       search_filter,
				       recipient_attrs,
				       NULL,
				       ldb_res,
				       ldb_search_default_callback,
				       NULL);
	OPENCHANGE_RETVAL_IF(ldb_ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, local_mem_ctx);

	if (ldb_sort_controls) {
		ldb_request_add_control(ldb_req, LDB_CONTROL_SERVER_SORT_OID, false, ldb_sort_controls);
	}

	ldb_ret = ldb_request(emsabp_ctx->samdb_ctx, ldb_req);
	OPENCHANGE_RETVAL_IF(ldb_ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, local_mem_ctx);

	ldb_ret = ldb_wait(ldb_req->handle, LDB_WAIT_ALL);
	OPENCHANGE_RETVAL_IF(ldb_ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, local_mem_ctx);
	OPENCHANGE_RETVAL_IF(ldb_res == NULL, MAPI_E_INVALID_OBJECT, local_mem_ctx);
	OPENCHANGE_RETVAL_IF(ldb_res->count == 0, MAPI_E_NOT_FOUND, local_mem_ctx);
	OPENCHANGE_RETVAL_IF(limit && ldb_res->count > limit, MAPI_E_TABLE_TOO_BIG, local_mem_ctx);

	MIds->aulPropTag = (uint32_t *) talloc_array(mem_ctx, uint32_t, ldb_res->count);
	OPENCHANGE_RETVAL_IF(MIds->aulPropTag == NULL, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	MIds->cValues = ldb_res->count;

	/* Step 2. Create session MId for all fetched records */

	for (i = 0; i < ldb_res->count; i++) {
		dn = ldb_msg_find_attr_as_string(ldb_res->msgs[i], "distinguishedName", NULL);
		retval = emsabp_tdb_fetch_MId(emsabp_ctx->ttdb_ctx, dn, (uint32_t *)&(MIds->aulPropTag[i]));
		if (retval != MAPI_E_SUCCESS) {
			retval = emsabp_tdb_insert(emsabp_ctx->ttdb_ctx, dn);
			OPENCHANGE_RETVAL_IF(retval, MAPI_E_CORRUPT_STORE, local_mem_ctx);
			retval = emsabp_tdb_fetch_MId(emsabp_ctx->ttdb_ctx, dn, (uint32_t *) &(MIds->aulPropTag[i]));
			OPENCHANGE_RETVAL_IF(retval, MAPI_E_CORRUPT_STORE, local_mem_ctx);
		}
	}

	talloc_free(local_mem_ctx);
	return MAPI_E_SUCCESS;
}


/**
   \details Search for a given DN within AD and return the associated
   LDB message.

   \param emsabp_ctx pointer to the EMSABP context
   \param dn pointer to the DN string to search for
   \param ldb_res pointer on pointer to the LDB message returned by
   the function

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS emsabp_search_dn(struct emsabp_context *emsabp_ctx, const char *dn,
					  struct ldb_message **ldb_res)
{
	struct ldb_dn		*ldb_dn = NULL;
	struct ldb_result	*res = NULL;
	const char * const	recipient_attrs[] = { "*", NULL };
	int			ret;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!dn, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ldb_res, MAPI_E_INVALID_PARAMETER, NULL);

	ldb_dn = ldb_dn_new(emsabp_ctx->mem_ctx, emsabp_ctx->samdb_ctx, dn);
	OPENCHANGE_RETVAL_IF(!ldb_dn_validate(ldb_dn), MAPI_E_CORRUPT_STORE, NULL);

	ret = ldb_search(emsabp_ctx->samdb_ctx, emsabp_ctx->mem_ctx, &res, ldb_dn,
			 LDB_SCOPE_BASE, recipient_attrs, NULL);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count || res->count != 1, MAPI_E_CORRUPT_STORE, NULL);

	*ldb_res = res->msgs[0];

	return MAPI_E_SUCCESS;
}


/**
   \details Search for a given AD record given its legacyDN parameter
   and return the associated LDB message.

   \param emsabp_ctx pointer to the EMSABP context
   \param legacyDN pointer to the legacyDN attribute value to lookup
   \param ldb_res pointer on pointer to the LDB message returned by
   the function
   \param pbUseConfPartition pointer on boolean specifying whether the
   legacyExchangeDN was retrieved from the Configuration parition or
   not

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS emsabp_search_legacyExchangeDN(struct emsabp_context *emsabp_ctx, const char *legacyDN,
							struct ldb_message **ldb_res, bool *pbUseConfPartition)
{
	const char * const	recipient_attrs[] = { "*", NULL };
	int			ret;
	struct ldb_result	*res = NULL;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!legacyDN, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ldb_res, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!pbUseConfPartition, MAPI_E_INVALID_PARAMETER, NULL);

	*pbUseConfPartition = true;
	ret = ldb_search(emsabp_ctx->samdb_ctx, emsabp_ctx->mem_ctx, &res,
			 ldb_get_config_basedn(emsabp_ctx->samdb_ctx),
			 LDB_SCOPE_SUBTREE, recipient_attrs, "(legacyExchangeDN=%s)",
			 ldb_binary_encode_string(emsabp_ctx->mem_ctx, legacyDN));

	if (ret != LDB_SUCCESS || res->count == 0) {
		*pbUseConfPartition = false;
		ret = ldb_search(emsabp_ctx->samdb_ctx, emsabp_ctx->mem_ctx, &res,
				 ldb_get_default_basedn(emsabp_ctx->samdb_ctx),
				 LDB_SCOPE_SUBTREE, recipient_attrs, "(legacyExchangeDN=%s)",
				 ldb_binary_encode_string(emsabp_ctx->mem_ctx, legacyDN));
	}
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, NULL);

	*ldb_res = res->msgs[0];

	return MAPI_E_SUCCESS;
}


/**
   \details Fetch filter to obtain Address Book entries for given ContainerID

   \param mem_ctx memory context for allocation
   \param emsabp_ctx pointer to the EMSABP context
   \param ContainerID id of the container to fetch
   \param filter pointer on pointer to the filter returned by the function

   \return MAPI_E_SUCCESS on success, otherwise MAPI_ERROR
 */
_PUBLIC_ enum MAPISTATUS emsabp_ab_fetch_filter(TALLOC_CTX *mem_ctx,
						struct emsabp_context *emsabp_ctx,
						uint32_t ContainerID,
						char **filter)
{
	int			ret;
	enum MAPISTATUS		retval;
	char			*dn;
	const char * const	recipient_attrs[] = { "globalAddressList", NULL };
	const char		*purportedSearch;
	struct ldb_result	*res = NULL;
	struct ldb_message	*ldb_msg = NULL;

	if (!ContainerID) {
		/* if GAL is requested */
		ret = ldb_search(emsabp_ctx->samdb_ctx, mem_ctx, &res,
				 ldb_get_config_basedn(emsabp_ctx->samdb_ctx),
				 LDB_SCOPE_SUBTREE, recipient_attrs, "(globalAddressList=*)");
		OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_CORRUPT_STORE, NULL);

		// We could have more than one GAL, but all of them are equal so
		// it really does not matter

		dn = (char *) ldb_msg_find_attr_as_string(res->msgs[0], "globalAddressList", NULL);
		OPENCHANGE_RETVAL_IF(!dn, MAPI_E_CORRUPT_STORE, NULL);
	} else {
		/* fetch a container we have already recorded */
		retval = emsabp_tdb_fetch_dn_from_MId(mem_ctx, emsabp_ctx->tdb_ctx, ContainerID, &dn);
		OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, MAPI_E_INVALID_BOOKMARK, NULL);
	}

	retval = emsabp_search_dn(emsabp_ctx, dn, &ldb_msg);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, MAPI_E_CORRUPT_STORE, NULL);

	// Fetch purportedSearch
	purportedSearch = ldb_msg_find_attr_as_string(ldb_msg, "purportedSearch", NULL);
	if (!purportedSearch) {
		return MAPI_E_INVALID_BOOKMARK;
	}

	// Add organization restriction
	return emsabp_include_organization_restriction(emsabp_ctx, purportedSearch, filter);
}


/**
   \details Enumerate AB container entries

   \param mem_ctx pointer to the memory context
   \param emsabp_ctx pointer to the EMSABP context
   \param ContainerID id of the container to fetch
   \param ldb_resp pointer on pointer to the LDB result returned by the
   function

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS emsabp_ab_container_enum(TALLOC_CTX *mem_ctx,
						  struct emsabp_context *emsabp_ctx,
						  uint32_t ContainerID,
						  struct ldb_result **ldb_resp)
{
	enum MAPISTATUS			retval;
	int				ldb_ret;
	struct ldb_request		*ldb_req;
	struct ldb_result		*ldb_res;
	char				*filter_search;
	const char * const		recipient_attrs[] = { "*", NULL };
	struct ldb_server_sort_control	**ldb_sort_controls;

	/* Fetch AB container record */
	retval = emsabp_ab_fetch_filter(mem_ctx, emsabp_ctx, ContainerID, &filter_search);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, MAPI_E_INVALID_BOOKMARK, NULL);

	/* Search AD with filter_search */

	ldb_res = talloc_zero(mem_ctx, struct ldb_result);
	OPENCHANGE_RETVAL_IF(!ldb_res, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	ldb_req = NULL;
	ldb_ret = ldb_build_search_req(&ldb_req, emsabp_ctx->samdb_ctx, mem_ctx,
				       ldb_get_default_basedn(emsabp_ctx->samdb_ctx),
				       LDB_SCOPE_SUBTREE,
				       filter_search,
				       recipient_attrs,
				       NULL,
				       ldb_res,
				       ldb_search_default_callback,
				       NULL);
	if (ldb_ret != LDB_SUCCESS) goto done;

	ldb_sort_controls = talloc_zero_array(filter_search, struct ldb_server_sort_control *, 2);
	ldb_sort_controls[0] = talloc(ldb_sort_controls, struct ldb_server_sort_control);
	ldb_sort_controls[0]->attributeName = talloc_strdup(ldb_sort_controls, "displayName");
	ldb_request_add_control(ldb_req, LDB_CONTROL_SERVER_SORT_OID, false, ldb_sort_controls);

	ldb_ret = ldb_request(emsabp_ctx->samdb_ctx, ldb_req);

	if (ldb_ret == LDB_SUCCESS) {
		ldb_ret = ldb_wait(ldb_req->handle, LDB_WAIT_ALL);
	}

done:
	talloc_free(filter_search);
	if (ldb_req) {
		talloc_free(ldb_req);
	}

	if (ldb_ret != LDB_SUCCESS) {
		talloc_free(ldb_res);
		ldb_res = NULL;
	}

	*ldb_resp = ldb_res;

	return (ldb_ret != LDB_SUCCESS) ? MAPI_E_NOT_FOUND : MAPI_E_SUCCESS;
}
