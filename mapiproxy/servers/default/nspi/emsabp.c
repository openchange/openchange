/*
   OpenChange Server implementation.

   EMSABP: Address Book Provider implementation

   Copyright (C) Julien Kerihuel 2006-2009.
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

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy.h"
#include "dcesrv_exchange_nsp.h"
#include <ldb.h>
#include <ldb_errors.h>
#include <util/debug.h>

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
	struct event_context	*ev;
	char			*configuration = NULL;
	char			*users = NULL;
	int			ret;

	/* Sanity checks */
	if (!lp_ctx) return NULL;

	mem_ctx = talloc_init("emsabp_init");
	
	emsabp_ctx = talloc_zero(mem_ctx, struct emsabp_context);
	if (!emsabp_ctx) {
		talloc_free(mem_ctx);
		return NULL;
	}

	emsabp_ctx->mem_ctx = mem_ctx;

	ev = event_context_init(mem_ctx);
	if (!ev) {
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Return an opaque context pointer on the configuration database */
	configuration = private_path(mem_ctx, lp_ctx, "configuration.ldb");
	emsabp_ctx->conf_ctx = ldb_init(mem_ctx, ev);
	if (!emsabp_ctx->conf_ctx) {
		talloc_free(configuration);
		talloc_free(mem_ctx);
		return NULL;
	}

	ret = ldb_connect(emsabp_ctx->conf_ctx, configuration, LDB_FLG_RDONLY, NULL);
	talloc_free(configuration);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("[%s:%d]: Connection to \"configuration.ldb\" failed\n", __FUNCTION__, __LINE__));
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Return an opaque context pointer to the users database */
	users = private_path(mem_ctx, lp_ctx, "users.ldb");
	emsabp_ctx->users_ctx = ldb_init(mem_ctx, ev);
	if (!emsabp_ctx->users_ctx) {
		talloc_free(users);
		talloc_free(mem_ctx);
		return NULL;
	}

	ret = ldb_connect(emsabp_ctx->users_ctx, users, LDB_FLG_RDONLY, NULL);
	talloc_free(users);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("[%s:%d]: Connection to \"users.ldb\" failed\n", __FUNCTION__, __LINE__));
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Reference the global TDB context to the current emsabp context */
	emsabp_ctx->tdb_ctx = tdb_ctx;

	return emsabp_ctx;
}


_PUBLIC_ bool emsabp_destructor(void *data)
{
	struct emsabp_context	*emsabp_ctx = (struct emsabp_context *)data;

	if (emsabp_ctx) {
		DEBUG(6, ("emsabp_ctx is at addr %p\n", emsabp_ctx));
		talloc_free(emsabp_ctx->mem_ctx);
		return true;
	}

	return false;
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
	int			ret;
	const char		*username = NULL;
	int			msExchUserAccountControl;
	enum ldb_scope		scope = LDB_SCOPE_SUBTREE;
	struct ldb_result	*res = NULL;
	char			*ldb_filter;
	const char * const	recipient_attrs[] = { "msExchUserAccountControl", NULL };

	username = dce_call->context->conn->auth_state.session_info->server_info->account_name;

	ldb_filter = talloc_asprintf(emsabp_ctx->mem_ctx, "CN=%s", username);
	ret = ldb_search(emsabp_ctx->users_ctx, emsabp_ctx->mem_ctx, &res, 
			 ldb_get_default_basedn(emsabp_ctx->users_ctx),
			 scope, recipient_attrs, ldb_filter);
	talloc_free(ldb_filter);

	/* If the search failed */
	if (ret != LDB_SUCCESS || !res->count) {
		return false;
	}

	/* If msExchUserAccountControl attribute is not found */
	if (!res->msgs[0]->num_elements) {
		return false;
	}

	/* If the attribute exists check its value */
	msExchUserAccountControl = ldb_msg_find_attr_as_int(res->msgs[0], "msExchUserAccountControl", 2);
	if (msExchUserAccountControl == 2) {
		return false;
	}

	return true;
}


/**
   \details Check if the provided codepage is correct

   \param lp_ctx pointer to the loadparm context
   \param emsabp_ctx pointer to the EMSABP context
   \param CodePage the codepage identifier

   \note The prototype is currently incorrect, but we are looking for
   a better way to check codepage, maybe within AD. At the moment this
   function is just a wrapper over libmapi valid_codepage function.

   \return true on success, otherwise false
 */
_PUBLIC_ bool emsabp_verify_codepage(struct loadparm_context *lp_ctx,
				     struct emsabp_context *emsabp_ctx,
				     uint32_t CodePage)
{
	return valid_codepage(CodePage);
}


/**
   \details Retrieve the NSPI server GUID from the server object in
   the configuration LDB database

   \param lp_ctx pointer to the loadparm context
   \param emsabp_ctx pointer to the EMSABP context

   \return An allocated GUID structure on success, otherwise NULL
 */
_PUBLIC_ struct GUID *emsabp_get_server_GUID(struct loadparm_context *lp_ctx,
					     struct emsabp_context *emsabp_ctx)
{
	int			ret;
	struct GUID		*guid = (struct GUID *) NULL;
	const char		*netbiosname = NULL;
	const char		*guid_str = NULL;
	enum ldb_scope		scope = LDB_SCOPE_SUBTREE;
	struct ldb_result	*res = NULL;
	char			*ldb_filter;
	char			*dn = NULL;
	struct ldb_dn		*ldb_dn = NULL;
	const char * const	recipient_attrs[] = { "*", NULL };
	const char		*firstorgdn = NULL;

	netbiosname = lp_netbios_name(lp_ctx);
	if (!netbiosname) return NULL;

	/* Step 1. Find the Exchange Organization */
	ldb_filter = talloc_strdup(emsabp_ctx->mem_ctx, "(objectClass=msExchOrganizationContainer)");
	ret = ldb_search(emsabp_ctx->conf_ctx, emsabp_ctx->mem_ctx, &res,
			 ldb_get_default_basedn(emsabp_ctx->conf_ctx),
			 scope, recipient_attrs, ldb_filter);
	talloc_free(ldb_filter);

	if (ret != LDB_SUCCESS || !res->count) {
		return NULL;
	}

	firstorgdn = ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL);
	if (!firstorgdn) {
		return NULL;
	}

	/* Step 2. Find the OpenChange Server object */
	dn = talloc_asprintf(emsabp_ctx->mem_ctx, "CN=Servers,CN=First Administrative Group,CN=Administrative Groups,%s",
			     firstorgdn);
	ldb_dn = ldb_dn_new(emsabp_ctx->mem_ctx, emsabp_ctx->conf_ctx, dn);
	talloc_free(dn);
	if (!ldb_dn_validate(ldb_dn)) {
		return NULL;
	}

	ret = ldb_search(emsabp_ctx->conf_ctx, emsabp_ctx->mem_ctx, &res, ldb_dn, 
			 scope, recipient_attrs, "(cn=%s)", netbiosname);
	if (ret != LDB_SUCCESS || !res->count) {
		return NULL;
	}

	/* Step 3. Retrieve the objectGUID GUID */
	guid_str = ldb_msg_find_attr_as_string(res->msgs[0], "objectGUID", NULL);
	if (!guid_str) return NULL;

	guid = talloc_zero(emsabp_ctx->mem_ctx, struct GUID);
	GUID_from_string(guid_str, guid);
	
	return guid;
}


/**
   \details Build an EphemeralEntryID structure

   \param lp_ctx pointer to the loadparm context
   \param emsabp_ctx pointer to the EMSABP context
   \param DisplayType the AB object display type
   \param MId the MId value
   \param ephEntryID pointer to the EphemeralEntryID returned by the
   function

   \return MAPI_E_SUCCESS on success, otherwise
   MAPI_E_NOT_ENOUGH_RESOURCES or MAPI_E_CORRUPT_STORE
 */
_PUBLIC_ enum MAPISTATUS emsabp_set_EphemeralEntryID(struct loadparm_context *lp_ctx, 
						     struct emsabp_context *emsabp_ctx,
						     uint32_t DisplayType, uint32_t MId,
						     struct EphemeralEntryID *ephEntryID)
{
	struct GUID	*guid = (struct GUID *) NULL;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ephEntryID, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);

	guid = emsabp_get_server_GUID(lp_ctx, emsabp_ctx);
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

	talloc_free(guid);

	return MAPI_E_SUCCESS;
}


/**
   \details Build a PermanentEntryID structure

   \param emsabp_ctx pointer to the EMSABP context
   \param DisplayType the AB object display type
   \param ldb_recipient pointer on the LDB message
   \param permEntryID pointer to the PermanentEntryID returned by the
   function

   \return MAPI_E_SUCCESS on success, otherwise
   MAPI_E_NOT_ENOUGH_RESOURCES or MAPI_E_CORRUPT_STORE
 */
_PUBLIC_ enum MAPISTATUS emsabp_set_PermanentEntryID(struct emsabp_context *emsabp_ctx, 
						     uint32_t DisplayType, struct ldb_message *msg, 
						     struct PermanentEntryID *permEntryID)
{
	struct GUID	*guid = (struct GUID *) NULL;
	const char	*guid_str;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!permEntryID, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
	

	permEntryID->ID_type = 0x0;
	permEntryID->R1 = 0x0;
	permEntryID->R2 = 0x0;
	permEntryID->R3 = 0x0;
	memcpy(permEntryID->ProviderUID.ab, GUID_NSPI, 16);
	permEntryID->R4 = 0x1;
	permEntryID->DisplayType = DisplayType;

	if (!msg) {
		permEntryID->dn = talloc_strdup(emsabp_ctx->mem_ctx, "/");
	} else {
		guid_str = ldb_msg_find_attr_as_string(msg, "objectGUID", NULL);
		OPENCHANGE_RETVAL_IF(!guid_str, MAPI_E_CORRUPT_STORE, NULL);
		guid = talloc_zero(emsabp_ctx->mem_ctx, struct GUID);
		GUID_from_string(guid_str, guid);
		permEntryID->dn = talloc_asprintf(emsabp_ctx->mem_ctx, EMSABP_DN, 
						  guid->time_low, guid->time_mid,
						  guid->time_hi_and_version,
						  guid->clock_seq[0],
						  guid->clock_seq[1],
						  guid->node[0], guid->node[1],
						  guid->node[2], guid->node[3],
						  guid->node[4], guid->node[5]);
		talloc_free(guid);
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Map a PermanentEntryID structure into a SBinary_r
   structure (for PR_ENTRYID and PR_EMS_AB_PARENT_ENTRYID properties)

   \param mem_ctx pointer to the memory context
   \param permEntryID pointer to the Permanent EntryID structure
   \param pointer to the Binary_r structure the server will return

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
	bin->cb = sizeof (*permEntryID) - 4 + strlen(permEntryID->dn) + 1;
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
						  struct SRow *aRow, uint32_t dwFlags,
						  struct PermanentEntryID *permEntryID,
						  struct PermanentEntryID *parentPermEntryID,
						  struct ldb_message *msg, bool child)
{
	enum MAPISTATUS			retval;
	struct SPropTagArray		*SPropTagArray;
	struct SPropValue		lpProps;
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
	aRow->ulAdrEntryPad = 0x0;
	aRow->cValues = 0x0;
	aRow->lpProps = talloc_zero(mem_ctx, struct SPropValue);

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
			case PR_DISPLAY_NAME:
				lpProps.value.lpszA = NULL;
				break;
			case PR_DISPLAY_NAME_UNICODE:
				lpProps.value.lpszW = NULL;
				break;
			case PR_EMS_AB_IS_MASTER:
				lpProps.value.b = false;
				break;
			default:
				break;
			}
			SRow_addprop(aRow, lpProps);
			/* SRow_addprop internals overwrite with MAPI_E_NOT_FOUND when data is NULL */
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
				switch (child) {
				case true:
					lpProps.value.l = AB_RECIPIENTS | AB_SUBCONTAINERS | AB_UNMODIFIABLE;
					break;
				case false:
					lpProps.value.l = AB_RECIPIENTS | AB_UNMODIFIABLE;
				}
				break;
			case PR_DEPTH:
				switch (child) {
				case true:
					lpProps.value.l = 0x1;
					break;
				case false:
					lpProps.value.l = 0x0;
					break;
				}
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
			case PR_DISPLAY_NAME:
				lpProps.value.lpszA = talloc_strdup(mem_ctx, ldb_msg_find_attr_as_string(msg, "displayName", NULL));
				if (!lpProps.value.lpszA) {
					lpProps.ulPropTag &= 0xFFFF0000;
					lpProps.ulPropTag += PT_ERROR;
				}
				break;
			case PR_DISPLAY_NAME_UNICODE:
				lpProps.value.lpszW = talloc_strdup(mem_ctx, ldb_msg_find_attr_as_string(msg, "displayName", NULL));
				if (!lpProps.value.lpszW) {
					lpProps.ulPropTag &= 0xFFFF0000;
					lpProps.ulPropTag += PT_ERROR;
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
			SRow_addprop(aRow, lpProps);
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
						   uint32_t dwFlags, struct SRowSet **SRowSet)
{
	enum MAPISTATUS			retval;
	struct SRow			*aRow;
	struct PermanentEntryID		gal;
	struct PermanentEntryID		parentPermEntryID;
	struct PermanentEntryID		permEntryID;
	enum ldb_scope			scope = LDB_SCOPE_SUBTREE;
	struct ldb_request		*req;
	struct ldb_result		*res = NULL;
	char				*ldb_filter;
	struct ldb_dn			*ldb_dn = NULL;
	struct ldb_control		**controls;
	const char * const		recipient_attrs[] = { "*", NULL };
	const char			*control_strings[2] = { "server_sort:0:0:displayName", NULL };
	const char			*addressBookRoots;
	int				ret;
	uint32_t			aRow_idx;
	uint32_t			i;

	/* Step 1. Build the 'Global Address List' object using PermanentEntryID */
	aRow = talloc_zero(mem_ctx, struct SRow);
	OPENCHANGE_RETVAL_IF(!aRow, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
	aRow_idx = 0;

	retval = emsabp_set_PermanentEntryID(emsabp_ctx, DT_CONTAINER, NULL, &gal);
	OPENCHANGE_RETVAL_IF(retval, retval, aRow);

	retval = emsabp_table_fetch_attrs(mem_ctx, emsabp_ctx, &aRow[aRow_idx], dwFlags, &gal, NULL, NULL, false);
	aRow_idx++;

	/* Step 2. Retrieve the object pointed by addressBookRoots attribute: 'All Address Lists' */
	ldb_filter = talloc_strdup(emsabp_ctx->mem_ctx, "(addressBookRoots=*)");
	ret = ldb_search(emsabp_ctx->conf_ctx, emsabp_ctx->mem_ctx, &res,
			 ldb_get_default_basedn(emsabp_ctx->conf_ctx),
			 scope, recipient_attrs, ldb_filter);
	talloc_free(ldb_filter);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_CORRUPT_STORE, aRow);

	addressBookRoots = ldb_msg_find_attr_as_string(res->msgs[0], "addressBookRoots", NULL);
	OPENCHANGE_RETVAL_IF(!addressBookRoots, MAPI_E_CORRUPT_STORE, aRow);
	talloc_free(res);

	ldb_dn = ldb_dn_new(emsabp_ctx->mem_ctx, emsabp_ctx->conf_ctx, addressBookRoots);
	OPENCHANGE_RETVAL_IF(!ldb_dn_validate(ldb_dn), MAPI_E_CORRUPT_STORE, aRow);

	scope = LDB_SCOPE_BASE;
	ret = ldb_search(emsabp_ctx->conf_ctx, emsabp_ctx->mem_ctx, &res, ldb_dn, 
			 scope, recipient_attrs, NULL);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count || res->count != 1, MAPI_E_CORRUPT_STORE, aRow);

	aRow = talloc_realloc(mem_ctx, aRow, struct SRow, aRow_idx + 1);
	retval = emsabp_set_PermanentEntryID(emsabp_ctx, DT_CONTAINER, res->msgs[0], &parentPermEntryID);
	emsabp_table_fetch_attrs(mem_ctx, emsabp_ctx, &aRow[aRow_idx], dwFlags, &parentPermEntryID, NULL, res->msgs[0], false);
	aRow_idx++;
	talloc_free(res);

	/* Step 3. Retrieve 'All Address Lists' subcontainers */
	res = talloc_zero(mem_ctx, struct ldb_result);
	OPENCHANGE_RETVAL_IF(!res, MAPI_E_NOT_ENOUGH_RESOURCES, aRow);

	controls = ldb_parse_control_strings(emsabp_ctx->conf_ctx, emsabp_ctx->mem_ctx, control_strings);
	ret = ldb_build_search_req(&req, emsabp_ctx->conf_ctx, emsabp_ctx->mem_ctx,
				   ldb_dn, LDB_SCOPE_SUBTREE, "(purportedSearch=*)",
				   recipient_attrs, controls, res, ldb_search_default_callback, NULL);

	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_CORRUPT_STORE, aRow);
	}

	ret = ldb_request(emsabp_ctx->conf_ctx, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}
	talloc_free(req);
	
	if (ret != LDB_SUCCESS || !res->count) {
		talloc_free(res);
		OPENCHANGE_RETVAL_IF(1, MAPI_E_CORRUPT_STORE, aRow);
	}

	aRow = talloc_realloc(mem_ctx, aRow, struct SRow, aRow_idx + res->count + 1);

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
							   uint32_t dwFlags, struct SRowSet **SRowSet)
{
	return MAPI_E_SUCCESS;
}
