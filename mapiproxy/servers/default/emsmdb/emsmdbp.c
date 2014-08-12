/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009-2014

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
   \file emsmdbp.c

   \brief EMSMDB Provider implementation
 */

#define TEVENT_DEPRECATED
#include "mapiproxy/dcesrv_mapiproxy.h"
#include "dcesrv_exchange_emsmdb.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"

#include <ldap_ndr.h>

/* Expose samdb_connect prototype */
struct ldb_context *samdb_connect(TALLOC_CTX *, struct tevent_context *,
				  struct loadparm_context *,
				  struct auth_session_info *,
				  unsigned int);

static struct GUID MagicGUID = {
	.time_low = 0xbeefface,
	.time_mid = 0xcafe,
	.time_hi_and_version = 0xbabe,
	.clock_seq = { 0x12, 0x34 },
	.node = { 0xde, 0xad, 0xfa, 0xce, 0xca, 0xfe }
};
const struct GUID *const MagicGUIDp = &MagicGUID;

/**
   \details Release the MAPISTORE context used by EMSMDB provider
   context

   \param data pointer on data to destroy

   \return 0 on success, otherwise -1
 */
static int emsmdbp_mapi_store_destructor(void *data)
{
	struct mapistore_context *mstore_ctx = (struct mapistore_context *) data;

	mapistore_release(mstore_ctx);
	DEBUG(6, ("[%s:%d]: MAPISTORE context released\n", __FUNCTION__, __LINE__));
	return true;
}

/**
   \details Release the MAPI handles context used by EMSMDB provider
   context

   \param data pointer on data to destroy

   \return 0 on success, otherwise -1
 */
static int emsmdbp_mapi_handles_destructor(void *data)
{
	enum MAPISTATUS			retval;
	struct mapi_handles_context	*handles_ctx = (struct mapi_handles_context *) data;

	retval = mapi_handles_release(handles_ctx);
	DEBUG(6, ("[%s:%d]: MAPI handles context released (%s)\n", __FUNCTION__, __LINE__,
		  mapi_get_errstr(retval)));

	return (retval == MAPI_E_SUCCESS) ? 0 : -1;
}

/**
   \details Initialize the EMSMDBP context and open connections to
   Samba databases.

   \param lp_ctx pointer to the loadparm_context
   \param username account name for current session
   \param ldb_ctx pointer to the openchange dispatcher ldb database
   
   \return Allocated emsmdbp_context pointer on success, otherwise
   NULL
 */
_PUBLIC_ struct emsmdbp_context *emsmdbp_init(struct loadparm_context *lp_ctx,
					      const char *username,
					      void *oc_ctx)
{
	TALLOC_CTX		*mem_ctx;
	struct emsmdbp_context	*emsmdbp_ctx;
	struct tevent_context	*ev;
	enum mapistore_error	ret;
	const char		*samdb_url;

	/* Sanity Checks */
	if (!lp_ctx) return NULL;

	mem_ctx  = talloc_named(NULL, 0, "emsmdbp_init");

	emsmdbp_ctx = talloc_zero(mem_ctx, struct emsmdbp_context);
	if (!emsmdbp_ctx) {
		talloc_free(mem_ctx);
		return NULL;
	}

	emsmdbp_ctx->mem_ctx = mem_ctx;

	ev = tevent_context_init(mem_ctx);
	if (!ev) {
		talloc_free(mem_ctx);
		return NULL;
	}
	tevent_loop_allow_nesting(ev); 

	/* Save a pointer to the loadparm context */
	emsmdbp_ctx->lp_ctx = lp_ctx;

	/* Retrieve samdb url (local or external) */
	samdb_url = lpcfg_parm_string(lp_ctx, NULL, "dcerpc_mapiproxy", "samdb_url");

	/* return an opaque context pointer on samDB database */
	if (!samdb_url) {
		emsmdbp_ctx->samdb_ctx = samdb_connect(mem_ctx, ev, lp_ctx, system_session(lp_ctx), 0);
	} else {
		emsmdbp_ctx->samdb_ctx = samdb_connect_url(mem_ctx, ev, lp_ctx, system_session(lp_ctx), 0, samdb_url);
	}

	if (!emsmdbp_ctx->samdb_ctx) {
		talloc_free(mem_ctx);
		DEBUG(0, ("[%s:%d]: Connection to \"sam.ldb\" failed\n", __FUNCTION__, __LINE__));
		return NULL;
	}

	/* Reference global OpenChange dispatcher database pointer within current context */
	emsmdbp_ctx->oc_ctx = oc_ctx;

	/* Initialize the mapistore context */		
	emsmdbp_ctx->mstore_ctx = mapistore_init(mem_ctx, lp_ctx, NULL);
	if (!emsmdbp_ctx->mstore_ctx) {
		DEBUG(0, ("[%s:%d]: MAPISTORE initialization failed\n", __FUNCTION__, __LINE__));

		talloc_free(mem_ctx);
		return NULL;
	}

	ret = mapistore_set_connection_info(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->oc_ctx, username);
	if (ret != MAPISTORE_SUCCESS) {
		DEBUG(0, ("[%s:%d]: MAPISTORE connection info initialization failed\n", __FUNCTION__, __LINE__));
		talloc_free(mem_ctx);
		return NULL;
	}
	talloc_set_destructor((void *)emsmdbp_ctx->mstore_ctx, (int (*)(void *))emsmdbp_mapi_store_destructor);

	/* Initialize MAPI handles context */
	emsmdbp_ctx->handles_ctx = mapi_handles_init(mem_ctx);
	if (!emsmdbp_ctx->handles_ctx) {
		DEBUG(0, ("[%s:%d]: MAPI handles context initialization failed\n", __FUNCTION__, __LINE__));
		talloc_free(mem_ctx);
		return NULL;
	}
	talloc_set_destructor((void *)emsmdbp_ctx->handles_ctx, (int (*)(void *))emsmdbp_mapi_handles_destructor);

	return emsmdbp_ctx;
}


/**
   \details Open openchange.ldb database

   \param lp_ctx pointer on the loadparm_context

   \note This function is just a wrapper over
   mapiproxy_server_openchange_ldb_init

   \return Allocated openchangedb context on success, otherwise NULL
 */
_PUBLIC_ void *emsmdbp_openchangedb_init(struct loadparm_context *lp_ctx)
{
	/* Sanity checks */
	if (!lp_ctx) return NULL;

	return mapiproxy_server_openchangedb_init(lp_ctx);
}


_PUBLIC_ bool emsmdbp_destructor(void *data)
{
	struct emsmdbp_context	*emsmdbp_ctx = (struct emsmdbp_context *)data;

	if (!emsmdbp_ctx) return false;

	talloc_unlink(emsmdbp_ctx, emsmdbp_ctx->oc_ctx);
	talloc_free(emsmdbp_ctx->mem_ctx);

	DEBUG(0, ("[%s:%d]: emsmdbp_ctx found and released\n", __FUNCTION__, __LINE__));

	return true;
}


/**
   \details Check if the authenticated user belongs to the Exchange
   organization and is enabled

   \param dce_call pointer to the session context
   \param emsmdbp_ctx pointer to the EMSMDBP context

   \return true on success, otherwise false
 */
_PUBLIC_ bool emsmdbp_verify_user(struct dcesrv_call_state *dce_call,
				  struct emsmdbp_context *emsmdbp_ctx)
{
	int			ret;
	const char		*username = NULL;
	int			msExchUserAccountControl;
	struct ldb_result	*res = NULL;
	const char * const	recipient_attrs[] = { "msExchUserAccountControl", NULL };

	username = dcesrv_call_account_name(dce_call);

	ret = ldb_search(emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res,
			 ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx),
			 LDB_SCOPE_SUBTREE, recipient_attrs, "sAMAccountName=%s", username);

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

	/* Get a copy of the username for later use and setup missing conn_info components */
	emsmdbp_ctx->username = talloc_strdup(emsmdbp_ctx, username);
	openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, &emsmdbp_ctx->mstore_ctx->conn_info->repl_id, &emsmdbp_ctx->mstore_ctx->conn_info->replica_guid);

	return true;
}


/**
   \details Check if the user record which legacyExchangeDN points to
   belongs to the Exchange organization and is enabled

   \param dce_call pointer to the session context
   \param emsmdbp_ctx pointer to the EMSMDBP context
   \param legacyExchangeDN pointer to the userDN to lookup
   \param msg pointer on pointer to the LDB message matching the record

   \note Users can set msg to NULL if they do not intend to retrieve
   the message

   \return true on success, otherwise false
 */
_PUBLIC_ bool emsmdbp_verify_userdn(struct dcesrv_call_state *dce_call,
				    struct emsmdbp_context *emsmdbp_ctx,
				    const char *legacyExchangeDN,
				    struct ldb_message **msg)
{
	int			ret;
	int			msExchUserAccountControl;
	struct ldb_result	*res = NULL;
	const char * const	recipient_attrs[] = { "*", NULL };

	/* Sanity Checks */
	if (!legacyExchangeDN) return false;

	ret = ldb_search(emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res,
			 ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx),
			 LDB_SCOPE_SUBTREE, recipient_attrs, "(legacyExchangeDN=%s)",
			 legacyExchangeDN);

	/* If the search failed */
	if (ret != LDB_SUCCESS || !res->count) {
		return false;
	}

	/* Checks msExchUserAccountControl value */
	msExchUserAccountControl = ldb_msg_find_attr_as_int(res->msgs[0], "msExchUserAccountControl", 2);
	if (msExchUserAccountControl == 2) {
		return false;
	}

	if (msg) {
		*msg = res->msgs[0];
	}

	return true;
}


/**
   \details Resolve a recipient and build the associated RecipientRow
   structure

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the EMSMDBP context
   \param recipient pointer to the recipient string
   \param properties array of properties to lookup for a recipient
   \param row the RecipientRow to fill in

   \note This is a very preliminary implementation with a lot of
   pseudo-hardcoded things. Lot of work is required to make this
   function generic and to cover all different cases

   \return Allocated RecipientRow on success, otherwise NULL
 */
_PUBLIC_ enum MAPISTATUS emsmdbp_resolve_recipient(TALLOC_CTX *mem_ctx,
						   struct emsmdbp_context *emsmdbp_ctx,
						   char *recipient,
						   struct mapi_SPropTagArray *properties,
						   struct RecipientRow *row)
{
	enum MAPISTATUS		retval;
	struct ldb_result	*res = NULL;
	const char * const	recipient_attrs[] = { "*", NULL };
	int			ret;
	uint32_t		i;
	uint32_t		property = 0;
	void			*data;
	char			*str;
	char			*username;
	char			*legacyExchangeDN;
	uint32_t		org_length;
	uint32_t		l;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx->samdb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!properties, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!row, MAPI_E_INVALID_PARAMETER, NULL);

	ret = ldb_search(emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res,
			 ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx),
			 LDB_SCOPE_SUBTREE, recipient_attrs,
			 "(&(objectClass=user)(sAMAccountName=*%s*)(!(objectClass=computer)))",
			 recipient);

	/* If the search failed, build an external recipient: very basic for the moment */
	if (ret != LDB_SUCCESS || !res->count) {
	failure:
		row->RecipientFlags = 0x07db;
		row->EmailAddress.lpszW = talloc_strdup(mem_ctx, recipient);
		row->DisplayName.lpszW = talloc_strdup(mem_ctx, recipient);
		row->SimpleDisplayName.lpszW = talloc_strdup(mem_ctx, recipient);
		row->prop_count = properties->cValues;
		row->layout = 0x1;
		row->prop_values.length = 0;
		for (i = 0; i < properties->cValues; i++) {
			switch (properties->aulPropTag[i]) {
			case PidTagSmtpAddress:
				property = properties->aulPropTag[i];
				data = (void *) recipient;
				break;
			default:
				retval = MAPI_E_NOT_FOUND;
				property = (properties->aulPropTag[i] & 0xFFFF0000) + PT_ERROR;
				data = (void *)&retval;
				break;
			}

			libmapiserver_push_property(mem_ctx,
						    property, (const void *)data, &row->prop_values, 
						    row->layout, 0, 0);
		}

		return MAPI_E_SUCCESS;
	}

	/* Otherwise build a RecipientRow for resolved username */

	username = (char *) ldb_msg_find_attr_as_string(res->msgs[0], "mailNickname", NULL);
	legacyExchangeDN = (char *) ldb_msg_find_attr_as_string(res->msgs[0], "legacyExchangeDN", NULL);
	if (!username || !legacyExchangeDN) {
		DEBUG(0, ("record found but mailNickname or legacyExchangeDN is missing for %s\n", recipient));
		goto failure;
	}
	org_length = strlen(legacyExchangeDN) - strlen(username);

	/* Check if we need a flagged blob */
	row->layout = 0;
	for (i = 0; i < properties->cValues; i++) {
		switch (properties->aulPropTag[i]) {
		case PR_DISPLAY_TYPE:
		case PR_OBJECT_TYPE:
		case PidTagAddressBookDisplayNamePrintable:
		case PidTagSmtpAddress:
			break;
		default:
			row->layout = 1;
			break;
		}
	}

	row->RecipientFlags = 0x06d1;
	row->AddressPrefixUsed.prefix_size = org_length;
	row->DisplayType.display_type = SINGLE_RECIPIENT;
	row->X500DN.recipient_x500name = talloc_strdup(mem_ctx, username);

	row->DisplayName.lpszW = talloc_strdup(mem_ctx, username);
	row->SimpleDisplayName.lpszW = talloc_strdup(mem_ctx, username);
	row->prop_count = properties->cValues;
	row->prop_values.length = 0;

	/* Add this very small set of properties */
	for (i = 0; i < properties->cValues; i++) {
		switch (properties->aulPropTag[i]) {
		case PR_DISPLAY_TYPE:
			property = properties->aulPropTag[i];
			l = 0x0;
			data = (void *)&l;
			break;
		case PR_OBJECT_TYPE:
			property = properties->aulPropTag[i];
			l = MAPI_MAILUSER;
			data = (void *)&l;
			break;
		case PidTagAddressBookDisplayNamePrintable:
			property = properties->aulPropTag[i];
			str = (char *) ldb_msg_find_attr_as_string(res->msgs[0], "mailNickname", NULL);
			data = (void *) str;
			break;
		case PidTagSmtpAddress:
			property = properties->aulPropTag[i];
			str = (char *) ldb_msg_find_attr_as_string(res->msgs[0], "legacyExchangeDN", NULL);
			data = (void *) str;
			break;
		default:
			retval = MAPI_E_NOT_FOUND;
			property = (properties->aulPropTag[i] & 0xFFFF0000) + PT_ERROR;
			data = (void *)&retval;
			break;
		}
		libmapiserver_push_property(mem_ctx,
					    property, (const void *)data, &row->prop_values, 
					    row->layout, 0, 0);
	}

	return MAPI_E_SUCCESS;
}

_PUBLIC_ int emsmdbp_guid_to_replid(struct emsmdbp_context *emsmdbp_ctx, const char *username, const struct GUID *guidP, uint16_t *replidP)
{
	uint16_t	replid;
	struct GUID	guid;

	if (GUID_equal(guidP, MagicGUIDp)) {
		*replidP = 2;
		return MAPI_E_SUCCESS;
	}

	openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, username, &replid, &guid);
	if (GUID_equal(guidP, &guid)) {
		*replidP = replid;
		return MAPI_E_SUCCESS;
	}

	if (mapistore_replica_mapping_guid_to_replid(emsmdbp_ctx->mstore_ctx, username, guidP, &replid) == MAPISTORE_SUCCESS) {
		*replidP = replid;
		return MAPI_E_SUCCESS;
	}

	return MAPI_E_NOT_FOUND;
}

_PUBLIC_ int emsmdbp_replid_to_guid(struct emsmdbp_context *emsmdbp_ctx, const char *username, const uint16_t replid, struct GUID *guidP)
{
	uint16_t	db_replid;
	struct GUID	guid;

	if (replid == 2) {
		*guidP = MagicGUID;
		return MAPI_E_SUCCESS;
	}

	openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, username, &db_replid, &guid);
	if (replid == db_replid) {
		*guidP = guid;
		return MAPI_E_SUCCESS;
	}

	if (mapistore_replica_mapping_replid_to_guid(emsmdbp_ctx->mstore_ctx, username, replid, &guid) == MAPISTORE_SUCCESS) {
		*guidP = guid;
		return MAPI_E_SUCCESS;
	}

	return MAPI_E_NOT_FOUND;
}

_PUBLIC_ int emsmdbp_source_key_from_fmid(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, const char *username, uint64_t fmid, struct Binary_r **source_keyP)
{
	struct Binary_r	*source_key;
	uint64_t	gc;
	uint16_t	replid;
	uint8_t		*bytes;
	int		i;

	replid = fmid & 0xffff;
	source_key = talloc_zero(NULL, struct Binary_r);
	source_key->cb = 22;
	source_key->lpb = talloc_array(source_key, uint8_t, source_key->cb);
	if (emsmdbp_replid_to_guid(emsmdbp_ctx, username, replid, (struct GUID *) source_key->lpb)) {
		talloc_free(source_key);
		return MAPISTORE_ERROR;
	}

	(void) talloc_reference(mem_ctx, source_key);
	talloc_unlink(NULL, source_key);

	gc = fmid >> 16;

	bytes = source_key->lpb + 16;
	for (i = 0; i < 6; i++) {
		bytes[i] = gc & 0xff;
		gc >>= 8;
	}

	*source_keyP = source_key;

	return MAPISTORE_SUCCESS;
}
