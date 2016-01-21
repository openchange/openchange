/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009-2014
   Copyright (C) Enrique J. Hernandez 2016

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
#include "mapiproxy/libmapiproxy/fault_util.h"
#include "mapiproxy/util/samdb.h"

#include <ldap_ndr.h>


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
	OC_DEBUG(6, "MAPISTORE context released\n");
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
	OC_DEBUG(6, "MAPI handles context released (%s)\n", mapi_get_errstr(retval));

	return (retval == MAPI_E_SUCCESS) ? 0 : -1;
}

/**
   \details Release the MAPI logon context used by EMSMDB provider
   context

   \param data pointer on data to destroy

   \return 0 on success, otherwise -1
 */
static int emsmdbp_mapi_logon_destructor(void *data)
{
	enum MAPISTATUS			retval;
	struct mapi_logon_context	*logon_ctx = (struct mapi_logon_context *) data;

	retval = mapi_logon_release(logon_ctx);
	OC_DEBUG(6, "MAPI logon context released (%s)", mapi_get_errstr(retval));

	return (retval == MAPI_E_SUCCESS) ? 0 : -1;
}

/**
   \details Initialize the EMSMDBP context and open connections to
   the databases.

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
	enum mapistore_error	ret;

	/* Sanity Checks */
	if (!lp_ctx) return NULL;

	mem_ctx  = talloc_named(NULL, 0, "emsmdbp_init");

	emsmdbp_ctx = talloc_zero(mem_ctx, struct emsmdbp_context);
	if (!emsmdbp_ctx) {
		talloc_free(mem_ctx);
		return NULL;
	}

	emsmdbp_ctx->mem_ctx = mem_ctx;

	/* Save a pointer to the loadparm context */
	emsmdbp_ctx->lp_ctx = lp_ctx;

	emsmdbp_ctx->samdb_ctx = samdb_init(emsmdbp_ctx);
	if (!emsmdbp_ctx->samdb_ctx) {
		talloc_free(mem_ctx);
		OC_DEBUG(0, "Connection to \"sam.ldb\" failed\n");
		return NULL;
	}

	/* Reference global OpenChange dispatcher database pointer within current context */
	emsmdbp_ctx->oc_ctx = oc_ctx;

	/* Initialize the mapistore context */
	emsmdbp_ctx->mstore_ctx = mapistore_init(mem_ctx, lp_ctx, NULL);
	if (!emsmdbp_ctx->mstore_ctx) {
		OC_DEBUG(0, "MAPISTORE initialization failed\n");
		talloc_free(mem_ctx);
		return NULL;
	}

	ret = mapistore_set_connection_info(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->samdb_ctx, emsmdbp_ctx->oc_ctx, username);
	if (ret != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "MAPISTORE connection info initialization failed\n");
		talloc_free(mem_ctx);
		return NULL;
	}
	talloc_set_destructor((void *)emsmdbp_ctx->mstore_ctx, (int (*)(void *))emsmdbp_mapi_store_destructor);

	/* Initialize MAPI handles context */
	emsmdbp_ctx->handles_ctx = mapi_handles_init(mem_ctx);
	if (!emsmdbp_ctx->handles_ctx) {
		OC_DEBUG(0, "MAPI handles context initialization failed\n");
		talloc_free(mem_ctx);
		return NULL;
	}
	talloc_set_destructor((void *)emsmdbp_ctx->handles_ctx, (int (*)(void *))emsmdbp_mapi_handles_destructor);

	/* Initialise MAPI logon context */
	emsmdbp_ctx->logon_ctx = mapi_logon_init(mem_ctx);
	if (!emsmdbp_ctx->logon_ctx) {
		OC_DEBUG(1, "MAPI logon context initialisation failed");
		talloc_free(mem_ctx);
		return NULL;
	}
	talloc_set_destructor((void *)emsmdbp_ctx->logon_ctx, (int (*)(void *))emsmdbp_mapi_logon_destructor);

	return emsmdbp_ctx;
}


/**
   \details Associate the session uuid to the emsmdb context

   \param emsmdbp_ctx pointer to the emsmdb context
   \param uuid the uuid to associate

   \return true on success, otherwise false
 */
_PUBLIC_ bool emsmdbp_set_session_uuid(struct emsmdbp_context *emsmdbp_ctx, struct GUID uuid)
{
	if (!emsmdbp_ctx) return false;

  emsmdbp_ctx->session_uuid = uuid;
  return true;
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

	OC_DEBUG(0, "emsmdbp_ctx found and released\n");

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

	ret = safe_ldb_search (&emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res,
			       ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx),
			       LDB_SCOPE_SUBTREE, recipient_attrs, "sAMAccountName=%s",
			       ldb_binary_encode_string(emsmdbp_ctx, username));

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

	ret = safe_ldb_search(&emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res,
			      ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx),
			      LDB_SCOPE_SUBTREE, recipient_attrs, "(legacyExchangeDN=%s)",
			      ldb_binary_encode_string(emsmdbp_ctx, legacyExchangeDN));

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

	ret = safe_ldb_search(&emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res,
			      ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx),
			      LDB_SCOPE_SUBTREE, recipient_attrs,
			      "(&(objectClass=user)(sAMAccountName=*%s*)(!(objectClass=computer)))",
			      ldb_binary_encode_string(mem_ctx, recipient));

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
		OC_DEBUG(0, "record found but mailNickname or legacyExchangeDN is missing for %s\n", recipient);
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

	if (openchangedb_replica_mapping_guid_to_replid(emsmdbp_ctx->oc_ctx, username, guidP, &replid) == MAPI_E_SUCCESS) {
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

	if (openchangedb_replica_mapping_replid_to_guid(emsmdbp_ctx->oc_ctx, username, replid, &guid) == MAPI_E_SUCCESS) {
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

/**
   \details Extract organization name and group name from the legacy exchange
   dn of the current logged user.

   \param mem_ctx memory context used for returned values
   \param emsmdbp_ctx pointer to the EMSMDBP context
   \param organization_name pointer to the returned organization name
   \param group_name pointer to the returned group name

   \note You can set organization_name or group_name to NULL if you don't want
   this values

   \return MAPI_E_SUCCESS or an error if something happens
 */
_PUBLIC_ enum MAPISTATUS emsmdbp_fetch_organizational_units(TALLOC_CTX *mem_ctx,
							    struct emsmdbp_context *emsmdbp_ctx,
							    char **organization_name,
							    char **group_name)
{
	char 		*exdn0, *exdn1;
	const char 	*EssDN;

	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx->szUserDN, MAPI_E_NOT_INITIALIZED, NULL);

	EssDN = emsmdbp_ctx->szUserDN;
	// EssDN has format: /o=organizacion name/ou=group name/cn=Recipients/cn=username

	if (organization_name) {
		exdn0 = strstr(EssDN, "/o=");
		OPENCHANGE_RETVAL_IF(!exdn0, MAPI_E_BAD_VALUE, NULL);
		exdn1 = strstr(EssDN, "/ou=");
		OPENCHANGE_RETVAL_IF(!exdn1, MAPI_E_BAD_VALUE, NULL);
		*organization_name = talloc_strndup(mem_ctx, exdn0 + 3, exdn1 - exdn0 - 3);
		OPENCHANGE_RETVAL_IF(!*organization_name, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
	}

	if (group_name) {
		exdn0 = strstr(EssDN, "/ou=");
		OPENCHANGE_RETVAL_IF(!exdn0, MAPI_E_BAD_VALUE, NULL);
		exdn1 = strstr(EssDN, "/cn=");
		OPENCHANGE_RETVAL_IF(!exdn1, MAPI_E_BAD_VALUE, NULL);
		*group_name = talloc_strndup(mem_ctx, exdn0 + 4, exdn1 - exdn0 - 4);
		OPENCHANGE_RETVAL_IF(!*group_name, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
	}

	return MAPI_E_SUCCESS;
}

/**
   \details Get the organization name (like "First Organization") as a DN.

   \param emsmdbp_ctx pointer to the EMSMDBP context
   \param basedn pointer to the returned struct ldb_dn

   \return MAPI_E_SUCCESS or an error if something happens
 */
_PUBLIC_ enum MAPISTATUS emsmdbp_get_org_dn(struct emsmdbp_context *emsmdbp_ctx, struct ldb_dn **basedn)
{
	enum MAPISTATUS		retval;
	int			ret;
	struct ldb_result	*res = NULL;
	char			*org_name;

	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx->samdb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!basedn, MAPI_E_INVALID_PARAMETER, NULL);

	retval = emsmdbp_fetch_organizational_units(emsmdbp_ctx, emsmdbp_ctx, &org_name, NULL);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, NULL);

	ret = safe_ldb_search(&emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res,
			      ldb_get_config_basedn(emsmdbp_ctx->samdb_ctx),
			      LDB_SCOPE_SUBTREE, NULL,
			      "(&(objectClass=msExchOrganizationContainer)(cn=%s))",
			      ldb_binary_encode_string(emsmdbp_ctx, org_name));
	talloc_free(org_name);

	/* If the search failed */
	if (ret != LDB_SUCCESS) {
		OC_DEBUG(1, "emsmdbp_get_org_dn ldb_search failure.\n");
		return MAPI_E_NOT_FOUND;
	}

	*basedn = ldb_dn_new(emsmdbp_ctx, emsmdbp_ctx->samdb_ctx,
			     ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL));
	return MAPI_E_SUCCESS;
}

/**
   \details Get the email of current user.

   \param emsmdbp_ctx pointer to the EMSMDBP context
   \param email pointer to return the email

   \note The email is obtained from proxyAddresses attribute, the one that
   starts with "SMTP:" which is the one used to authenticate on external
   smtp service.

   \return MAPI_E_SUCCESS or an error if something happens
 */
_PUBLIC_ enum MAPISTATUS emsmdbp_get_external_email(struct emsmdbp_context *emsmdbp_ctx, const char **email)
{
	const char * const		attrs[] = { "proxyAddresses", NULL };
	const char			*prefix = "SMTP:";
	size_t				prefix_len;
	int				ret, i;
	struct ldb_result		*res = NULL;
	struct ldb_message_element	*el = NULL;
	const struct ldb_val		*value = NULL;
	bool				found = false;

	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx->samdb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!email, MAPI_E_INVALID_PARAMETER, NULL);

	ret = safe_ldb_search(&emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res,
			      ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx),
			      LDB_SCOPE_SUBTREE, attrs,
			      "(&(objectClass=user)(sAMAccountName=%s))",
			      ldb_binary_encode_string(emsmdbp_ctx, emsmdbp_ctx->username));

	if (ret != LDB_SUCCESS || res->count == 0) {
		OC_DEBUG(5, "Couldn't find %s using ldb_search", emsmdbp_ctx->username);
		return MAPI_E_NOT_FOUND;
	}

	el = ldb_msg_find_element(res->msgs[0], attrs[0]);
	if (!el) return MAPI_E_NOT_FOUND;

	prefix_len = strlen(prefix);
	for (i = 0; !found && i < el->num_values; i++) {
		value = (struct ldb_val *) &el->values[i];
		if (!value || value->length <= prefix_len || value->data[value->length] != '\0')
			continue;

		/* Searching for SMTP:some@email.com entry */
		if (strncmp((const char *)value->data, prefix, prefix_len) == 0) {
			*email = (char *)(value->data + prefix_len);
			found = true;
		}
	}

	return found ? MAPI_E_SUCCESS : MAPI_E_NOT_FOUND;
}
