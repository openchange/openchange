/*
 *  OpenChange MAPI implementation.
 *
 *  Copyright (C) Julien Kerihuel 2007.
 *  Copyright (C) Fabien Le Mentec 2007.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <credentials.h>
#include <ldb_errors.h>
#include <ldb.h>

/*
 * Private functions
 */

/**
 * Load a MAPI profile into a mapi_profile struct
 */
static enum MAPISTATUS ldb_load_profile(TALLOC_CTX *mem_ctx,
					struct ldb_context *ldb_ctx,
					struct mapi_profile *profile,
					const char *profname, const char *password)
{
	int			ret;
	enum ldb_scope		scope = LDB_SCOPE_SUBTREE;
	char			*ldb_filter;
	struct ldb_result	*res;
	struct ldb_message	*msg;
	const char * const	attrs[] = { "*", NULL };

	/* fills in profile name */
	profile->profname = talloc_strdup(mem_ctx, profname);
	if (!profile->profname) return MAPI_E_NOT_ENOUGH_RESOURCES;

	/* ldb query */
	ldb_ctx = global_mapi_ctx->ldb_ctx;
	ldb_filter = talloc_asprintf(mem_ctx, "(cn=%s)(cn=Profiles)", profile->profname);
	if (!ldb_filter) return MAPI_E_NOT_ENOUGH_RESOURCES;

	ret = ldb_search(ldb_ctx, ldb_get_default_basedn(ldb_ctx), scope, ldb_filter, attrs, &res);
	talloc_free(ldb_filter);
	if (ret != LDB_SUCCESS) return MAPI_E_NOT_FOUND;

	/* profile not found */
	if (!res->count) return MAPI_E_NOT_FOUND;
	/* more than one profile */
	if (res->count > 1) return MAPI_E_COLLISION;

	/* fills in profile with query result */
	msg = res->msgs[0];

	profile->username = ldb_msg_find_attr_as_string(msg, "username", NULL);
	profile->password = password ? password : ldb_msg_find_attr_as_string(msg, "password", "");
	profile->workstation = ldb_msg_find_attr_as_string(msg, "workstation", NULL);
	profile->realm = ldb_msg_find_attr_as_string(msg, "realm", NULL);
	profile->domain = ldb_msg_find_attr_as_string(msg, "domain", NULL);
	profile->mailbox = ldb_msg_find_attr_as_string(msg, "EmailAddress", NULL);
	profile->homemdb = ldb_msg_find_attr_as_string(msg, "HomeMDB", NULL);
	profile->server = ldb_msg_find_attr_as_string(msg, "binding", NULL);
	profile->org = ldb_msg_find_attr_as_string(msg, "Organization", NULL);
	profile->ou = ldb_msg_find_attr_as_string(msg, "OrganizationUnit", NULL);
	profile->codepage = ldb_msg_find_attr_as_int(msg, "codepage", 0);
	profile->language = ldb_msg_find_attr_as_int(msg, "language", 0);
	profile->method = ldb_msg_find_attr_as_int(msg, "method", 0);

	if (!profile->password) return MAPI_E_INVALID_PARAMETER;

	return MAPI_E_SUCCESS;
}

/**
 * Search for and attribute within profiles
 */
static enum MAPISTATUS ldb_clear_default_profile(TALLOC_CTX *mem_ctx)
{
	enum ldb_scope		scope = LDB_SCOPE_SUBTREE;
	struct ldb_context	*ldb_ctx;
	struct ldb_dn		*basedn;
	struct ldb_message	*msg;
	struct ldb_result	*res;
	const char		*attrs[] = { "PR_DEFAULT_PROFILE", NULL };
	int			ret;
	int			i;

	ldb_ctx = global_mapi_ctx->ldb_ctx;

	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, "CN=Profiles");
	ret = ldb_search(ldb_ctx, basedn, scope, "(cn=*)", attrs, &res);
	
	if (ret != LDB_SUCCESS) return MAPI_E_NOT_FOUND;
	if (!res->count) return MAPI_E_NOT_FOUND;

	for (i = 0; i < res->count; i++) {
		struct ldb_message		*message;

		msg = res->msgs[i];
		if (msg->num_elements == 1) {
			message = talloc_steal(mem_ctx, msg);
			message->elements[0].flags = LDB_FLAG_MOD_DELETE;

			ret = ldb_modify(ldb_ctx, message);
			talloc_free(message);
		}
	}
	
	return MAPI_E_SUCCESS;
}

/**
 * Test if the password is correct
 */
static enum MAPISTATUS ldb_test_password(TALLOC_CTX *mem_ctx, const char *profile, 
					 const char *password)
{
	enum ldb_scope		scope = LDB_SCOPE_DEFAULT;
	struct ldb_context	*ldb_ctx;
	struct ldb_message	*msg;
	struct ldb_result	*res;
	const char		*attrs[] = {"cn", "password", NULL };
	const char		*ldb_password;
	int			ret;
	char			*ldb_filter;

	ldb_ctx = global_mapi_ctx->ldb_ctx;
	
	ldb_filter = talloc_asprintf(mem_ctx, "(cn=%s)(cn=Profiles)", profile);
	ret = ldb_search(ldb_ctx, 0, scope, ldb_filter, attrs, &res);
	talloc_free(ldb_filter);

	if (ret != LDB_SUCCESS) return MAPI_E_NO_SUPPORT;
	if (!res->count) return MAPI_E_NOT_FOUND;
	
	msg = res->msgs[0];
	
	ldb_password = ldb_msg_find_attr_as_string(msg, "password", NULL);
	if (!ldb_password) return MAPI_E_NO_SUPPORT;
	if (strncmp(password, ldb_password, strlen(password))) return MAPI_E_LOGON_FAILED;
	
	return MAPI_E_SUCCESS;
}


/*
 * Add a profile to the database with default fields
 */
static enum MAPISTATUS ldb_create_profile(TALLOC_CTX *mem_ctx,
					  struct ldb_context *ldb_ctx,
					  const char *profname)
{
	enum ldb_scope			scope = LDB_SCOPE_SUBTREE;
	struct ldb_message		msg;
	struct ldb_message_element	el[2];
	struct ldb_val			vals[2][1];
	struct ldb_result		*res;
	struct ldb_dn			*basedn;
	char				*dn;
	char				*ldb_filter;
	int				ret;
	const char * const		attrs[] = { "*", NULL };

	/* Sanity check */
	if (profname == NULL) 
		return MAPI_E_BAD_VALUE;

	/* Does the profile already exists? */
	ldb_filter = talloc_asprintf(mem_ctx, "(cn=%s)(cn=Profiles)", profname);
	ret = ldb_search(ldb_ctx, ldb_get_default_basedn(ldb_ctx), scope, ldb_filter, attrs, &res);
	talloc_free(ldb_filter);
	if (res->msgs) return MAPI_E_NO_ACCESS;

	/*
	 * We now prepare the entry for transaction
	 */
	
	dn = talloc_asprintf(mem_ctx, "CN=%s,CN=Profiles", profname);
	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, dn);
	talloc_free(dn);
	if (!ldb_dn_validate(basedn)) return MAPI_E_BAD_VALUE;

	msg.dn = ldb_dn_copy(mem_ctx, basedn);
	msg.num_elements = 2;
	msg.elements = el;

	el[0].flags = 0;
	el[0].name = talloc_strdup(mem_ctx, "cn");
	el[0].num_values = 1;
	el[0].values = vals[0];
	vals[0][0].data = (uint8_t *)talloc_strdup(mem_ctx, profname);
	vals[0][0].length = strlen(profname);

	el[1].flags = 0;
	el[1].name = talloc_strdup(mem_ctx, "name");
	el[1].num_values = 1;
	el[1].values = vals[1];
	vals[1][0].data = (uint8_t *)talloc_strdup(mem_ctx, profname);
	vals[1][0].length = strlen(profname);

	ret = ldb_add(ldb_ctx, &msg);

	if (ret != LDB_SUCCESS) return MAPI_E_NO_SUPPORT;

	return MAPI_E_SUCCESS;
}

/*
 * delete the current profile
 */
static enum MAPISTATUS ldb_delete_profile(TALLOC_CTX *mem_ctx,
					  struct ldb_context *ldb_ctx,
					  const char *profname)
{
	enum ldb_scope		scope = LDB_SCOPE_SUBTREE;
	struct ldb_result	*res;
	char			*ldb_filter;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	ldb_filter = talloc_asprintf(mem_ctx, "(cn=%s)(cn=Profiles)", profname);
	ret = ldb_search(ldb_ctx, ldb_get_default_basedn(ldb_ctx), scope, ldb_filter, attrs, &res);
	talloc_free(ldb_filter);
	if (!res->msgs) return MAPI_E_NOT_FOUND;

	ret = ldb_delete(ldb_ctx, res->msgs[0]->dn);
	if (ret != LDB_SUCCESS) return MAPI_E_NOT_FOUND;
	
	return MAPI_E_SUCCESS;
}

/*
 * Public interface
 */

/*
 * Add an attribute to the profile
 */
_PUBLIC_ enum MAPISTATUS mapi_profile_add_string_attr(const char *profile, 
						      const char *attr, 
						      const char *value)
{
	TALLOC_CTX			*mem_ctx;
	enum ldb_scope			scope = LDB_SCOPE_SUBTREE;
	struct ldb_message		msg;
	struct ldb_message_element	el[1];
	struct ldb_val			vals[1][1];
	struct ldb_result		*res;
	struct ldb_context		*ldb_ctx;
	struct ldb_dn			*basedn;
	char				*dn;
	char				*ldb_filter;
	int				ret;
	const char * const		attrs[] = { "*", NULL };

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!global_mapi_ctx->ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!profile, MAPI_E_BAD_VALUE, NULL);

	mem_ctx = talloc_init("mapi_profile_add_string_attr");
	ldb_ctx = global_mapi_ctx->ldb_ctx;

	/* Retrieve the profile from the database */
	ldb_filter = talloc_asprintf(mem_ctx, "(cn=%s)(cn=Profiles)", profile);
	ret = ldb_search(ldb_ctx, ldb_get_default_basedn(ldb_ctx), scope, ldb_filter, attrs, &res);
	talloc_free(ldb_filter);
	MAPI_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_BAD_VALUE, mem_ctx);

	/* Preparing for the transaction */
	dn = talloc_asprintf(mem_ctx, "CN=%s,CN=Profiles", profile);
	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, dn);
	talloc_free(dn);
	MAPI_RETVAL_IF(!ldb_dn_validate(basedn), MAPI_E_BAD_VALUE, mem_ctx);

	msg.dn = ldb_dn_copy(mem_ctx, basedn);
	msg.num_elements = 1;
	msg.elements = el;

	el[0].flags = LDB_FLAG_MOD_ADD;
	el[0].name = talloc_strdup(mem_ctx, attr);
	el[0].num_values = 1;
	el[0].values = vals[0];
	vals[0][0].data = (uint8_t *)talloc_strdup(mem_ctx, value);
	vals[0][0].length = strlen(value);

	ret = ldb_modify(ldb_ctx, &msg);
	MAPI_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);
	
	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}

/**
 * Modify an attribute
 */
_PUBLIC_ enum MAPISTATUS mapi_profile_modify_string_attr(const char *profname, 
							 const char *attr, 
							 const char *value)
{
	TALLOC_CTX			*mem_ctx;
	enum ldb_scope			scope = LDB_SCOPE_SUBTREE;
	struct ldb_message		msg;
	struct ldb_message_element	el[1];
	struct ldb_val			vals[1][1];
	struct ldb_result		*res;
	struct ldb_context		*ldb_ctx;
	struct ldb_dn			*basedn;
	char				*dn;
	char				*ldb_filter;
	int				ret;
	const char * const		attrs[] = { "*", NULL };

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!profname, MAPI_E_BAD_VALUE, NULL);

	ldb_ctx = global_mapi_ctx->ldb_ctx;
	mem_ctx = talloc_init("mapi_profile_modify_string_attr");

	/* Retrieve the profile from the database */
	ldb_filter = talloc_asprintf(mem_ctx, "(cn=%s)(cn=Profiles)", profname);
	ret = ldb_search(ldb_ctx, ldb_get_default_basedn(ldb_ctx), scope, ldb_filter, attrs, &res);
	talloc_free(ldb_filter);
	MAPI_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_BAD_VALUE, mem_ctx);

	/* Preparing for the transaction */
	dn = talloc_asprintf(mem_ctx, "CN=%s,CN=Profiles", profname);
	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, dn);
	talloc_free(dn);
	MAPI_RETVAL_IF(!ldb_dn_validate(basedn), MAPI_E_BAD_VALUE, mem_ctx);

	msg.dn = ldb_dn_copy(mem_ctx, basedn);
	msg.num_elements = 1;
	msg.elements = el;

	el[0].flags = LDB_FLAG_MOD_REPLACE;
	el[0].name = talloc_strdup(mem_ctx, attr);
	el[0].num_values = 1;
	el[0].values = vals[0];
	vals[0][0].data = (uint8_t *)talloc_strdup(mem_ctx, value);
	vals[0][0].length = strlen(value);

	ret = ldb_modify(ldb_ctx, &msg);
	MAPI_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}

/**
 * Delete an attribute
 */
_PUBLIC_ enum MAPISTATUS mapi_profile_delete_string_attr(const char *profname, 
							 const char *attr, 
							 const char *value)
{
	TALLOC_CTX			*mem_ctx;
	enum ldb_scope			scope = LDB_SCOPE_SUBTREE;
	struct ldb_message		msg;
	struct ldb_message_element	el[1];
	struct ldb_val			vals[1][1];
	struct ldb_result		*res;
	struct ldb_context		*ldb_ctx;
	struct ldb_dn			*basedn;
	char				*dn;
	char				*ldb_filter;
	int				ret;
	const char * const		attrs[] = { "*", NULL };

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!profname, MAPI_E_BAD_VALUE, NULL);

	ldb_ctx = global_mapi_ctx->ldb_ctx;
	mem_ctx = talloc_init("mapi_profile_delete_string_attr");

	/* Retrieve the profile from the database */
	ldb_filter = talloc_asprintf(mem_ctx, "(cn=%s)(cn=Profiles)", profname);
	ret = ldb_search(ldb_ctx, ldb_get_default_basedn(ldb_ctx), scope, ldb_filter, attrs, &res);
	talloc_free(ldb_filter);
	MAPI_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_BAD_VALUE, mem_ctx);

	/* Preparing for the transaction */
	dn = talloc_asprintf(mem_ctx, "CN=%s,CN=Profiles", profname);
	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, dn);
	talloc_free(dn);
	MAPI_RETVAL_IF(!ldb_dn_validate(basedn), MAPI_E_BAD_VALUE, mem_ctx);

	msg.dn = ldb_dn_copy(mem_ctx, basedn);
	msg.num_elements = 1;
	msg.elements = el;

	el[0].flags = LDB_FLAG_MOD_DELETE;
	el[0].name = talloc_strdup(mem_ctx, attr);
	el[0].num_values = 1;
	el[0].values = vals[0];
	vals[0][0].data = (uint8_t *)talloc_strdup(mem_ctx, value);
	vals[0][0].length = strlen(value);

	ret = ldb_modify(ldb_ctx, &msg);
	MAPI_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}

/**
 * Private function which opens the profile store
 * Should only be called within MAPIInitialize
 */
enum MAPISTATUS OpenProfileStore(TALLOC_CTX *mem_ctx, struct ldb_context **ldb_ctx, 
				 const char *profiledb)
{
	int			ret;
	struct ldb_context	*tmp_ctx;
	
	*ldb_ctx = 0;
	
	/* store path */
	if (!profiledb) return MAPI_E_NOT_FOUND;

	/* init ldb */
	ret = ldb_global_init();
	if (ret != LDB_SUCCESS) return MAPI_E_NOT_ENOUGH_RESOURCES;

	/* connect to the store */
	tmp_ctx = ldb_init(mem_ctx);
	if (!tmp_ctx) return MAPI_E_NOT_ENOUGH_RESOURCES;

	ret = ldb_connect(tmp_ctx, profiledb, 0, NULL);
	if (ret != LDB_SUCCESS) return MAPI_E_NOT_FOUND;

	*ldb_ctx = tmp_ctx;
	return MAPI_E_SUCCESS;
}

/**
 * Create a profile store
 */
_PUBLIC_ enum MAPISTATUS CreateProfileStore(const char *profiledb, const char *ldif_path)
{
	int			ret;
	TALLOC_CTX		*mem_ctx;
	struct ldb_context	*ldb_ctx;
	struct ldb_ldif		*ldif;
	char			*url = NULL;
	char			*filename = NULL;
	FILE			*f;

	MAPI_RETVAL_IF(!profiledb, MAPI_E_CALL_FAILED, NULL);
	MAPI_RETVAL_IF(!ldif_path, MAPI_E_CALL_FAILED, NULL);

	mem_ctx = talloc_init("CreateProfileStore");

	ret = ldb_global_init();
	MAPI_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	ldb_ctx = ldb_init(mem_ctx);
	MAPI_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	url = talloc_asprintf(mem_ctx, "tdb://%s", profiledb);
	ret = ldb_connect(ldb_ctx, url, 0, 0);
	talloc_free(url);
	MAPI_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_ACCESS, mem_ctx);

	filename = talloc_asprintf(mem_ctx, "%s/oc_profiles_init.ldif", ldif_path);
	f = fopen(filename, "r");
	MAPI_RETVAL_IF(!f, MAPI_E_NO_ACCESS, mem_ctx);
	talloc_free(filename);

	while ((ldif = ldb_ldif_read_file(ldb_ctx, f))) {
		ldif->msg = ldb_msg_canonicalize(ldb_ctx, ldif->msg);
		ret = ldb_add(ldb_ctx, ldif->msg);
		if (ret != LDB_SUCCESS) {
			fclose(f);
			MAPI_RETVAL_IF("ldb_ldif_read_file", MAPI_E_NO_ACCESS, mem_ctx);
		}
		ldb_ldif_read_free(ldb_ctx, ldif);
	}
	fclose(f);

	filename = talloc_asprintf(mem_ctx, "%s/oc_profiles_schema.ldif", ldif_path);
	f = fopen(filename, "r");
	MAPI_RETVAL_IF(!f, MAPI_E_NO_ACCESS, mem_ctx);

	talloc_free(filename);
	while ((ldif = ldb_ldif_read_file(ldb_ctx, f))) {
		ldif->msg = ldb_msg_canonicalize(ldb_ctx, ldif->msg);
		ret = ldb_add(ldb_ctx, ldif->msg);
		if (ret != LDB_SUCCESS) {
			fclose(f);
			MAPI_RETVAL_IF("ldb_ldif_read_file", MAPI_E_NO_ACCESS, mem_ctx);
		}

		ldb_ldif_read_free(ldb_ctx, ldif);
	}
	fclose(f);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
 * Open a MAPI Profile
 */

_PUBLIC_ enum MAPISTATUS OpenProfile(struct mapi_profile *profile, const char *profname, const char *password)
{
	TALLOC_CTX	*mem_ctx;
	enum MAPISTATUS	retval;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = (TALLOC_CTX *) global_mapi_ctx->session;
	
	/* find the profile in ldb store */
	retval = ldb_load_profile(mem_ctx, global_mapi_ctx->ldb_ctx, profile, profname, password);

	MAPI_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;	
}

/**
 * Load a MAPI Profile and sets its credentials
 */

_PUBLIC_ enum MAPISTATUS LoadProfile(struct mapi_profile *profile)
{
	TALLOC_CTX *mem_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = (TALLOC_CTX *) global_mapi_ctx->session;

	profile->credentials = cli_credentials_init(mem_ctx);
	MAPI_RETVAL_IF(!profile->credentials, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
	cli_credentials_set_username(profile->credentials, profile->username, CRED_SPECIFIED);
	cli_credentials_set_password(profile->credentials, profile->password, CRED_SPECIFIED);
	cli_credentials_set_workstation(profile->credentials, profile->workstation, CRED_SPECIFIED);
	cli_credentials_set_realm(profile->credentials, profile->realm, CRED_SPECIFIED);
	cli_credentials_set_domain(profile->credentials, profile->domain, CRED_SPECIFIED);

	return MAPI_E_SUCCESS;
}

/**
 * Release a profile
 */
_PUBLIC_ enum MAPISTATUS ShutDown(struct mapi_profile *profile)
{
	MAPI_RETVAL_IF(!profile, MAPI_E_INVALID_PARAMETER, NULL);

	if (profile->credentials) 
		talloc_free(profile->credentials);
	return MAPI_E_SUCCESS;
}

/**
 * Create a MAPI Profile
 */
_PUBLIC_ enum MAPISTATUS CreateProfile(const char *profile, const char *username,
				       const char *password, uint32_t flag)
{
	enum MAPISTATUS retval;
	TALLOC_CTX	*mem_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = talloc_init("CreateProfile");
	retval = ldb_create_profile(mem_ctx, global_mapi_ctx->ldb_ctx, profile);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	retval = mapi_profile_add_string_attr(profile, "username", username);
	if (flag != OC_PROFILE_NOPASSWORD) {
		retval = mapi_profile_add_string_attr(profile, "password", password);
	}
	talloc_free(mem_ctx);

	return retval;
}

/**
 * Delete a MAPI Profile
 */
_PUBLIC_ enum MAPISTATUS DeleteProfile(const char *profile, uint32_t flags)
{
	TALLOC_CTX	*mem_ctx;
	enum MAPISTATUS	retval;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = talloc_init("DeleteProfile");
	retval = ldb_delete_profile(mem_ctx, global_mapi_ctx->ldb_ctx, profile);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);
	talloc_free(mem_ctx);

	return retval;
}

/**
 * Change the profile password
 */
_PUBLIC_ enum MAPISTATUS ChangeProfilePassword(const char *profile, const char *old_password,
					       const char *password, uint32_t flags)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;

	if (!profile || !old_password | !password) return MAPI_E_INVALID_PARAMETER;

	mem_ctx = talloc_init("ChangeProfilePassword");

	retval = ldb_test_password(mem_ctx, profile, password);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	retval = mapi_profile_modify_string_attr(profile, "password", password);
	
	talloc_free(mem_ctx);
	return retval;
}

/**
 * Copies a profile
 */
_PUBLIC_ enum MAPISTATUS CopyProfile(const char *old_profile, const char *password,
				     const char *profile, uint32_t flags)
{
	return MAPI_E_SUCCESS;
	/* return MAPI_E_LOGON_FAILED => auth failure */
	/* return MAPI_E_ACCESS_DENIED => the new profile already exists */
	/* return MAPI_E_NOT_FOUND => old profile doesn't exist */
}

/**
 * Rename a profile
 */
_PUBLIC_ enum MAPISTATUS RenameProfile(const char *old_profile, const char *password,
				       const char *profile, uint32_t flags)
{
	return MAPI_E_SUCCESS;
	/* return MAPI_E_LOGON_FAILED => auth failure*/
}

/**
 * Set default profile
 */
_PUBLIC_ enum MAPISTATUS SetDefaultProfile(const char *profname, uint32_t flags)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_profile	profile;
	enum MAPISTATUS		retval;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!profname, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_init("SetDefaultProfile");

	/* open profile */
	retval = ldb_load_profile(mem_ctx, global_mapi_ctx->ldb_ctx, &profile, profname, NULL);
	MAPI_RETVAL_IF(retval && retval != MAPI_E_INVALID_PARAMETER, retval, mem_ctx);

	/* search any previous default profile and unset it */
	retval = ldb_clear_default_profile(mem_ctx);

	/* set profname as the default profile */
	retval = mapi_profile_modify_string_attr(profname, "PR_DEFAULT_PROFILE", "1");

	talloc_free(mem_ctx);

	return retval;
}

/**
 * Get default profile
 */
_PUBLIC_ enum MAPISTATUS GetDefaultProfile(const char **profname, uint32_t flags)
{
	enum MAPISTATUS		retval;
	struct SRowSet		proftable;
	struct SPropValue	*lpProp;
	uint32_t		i;
	

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	
	retval = GetProfileTable(&proftable, 0);

	for (i = 0; i < proftable.cRows; i++) {
		lpProp = get_SPropValue_SRow(&(proftable.aRow[i]), PR_DEFAULT_PROFILE);
		if (lpProp && (lpProp->value.l == 1)) {
			lpProp = get_SPropValue_SRow(&(proftable.aRow[i]), PR_DISPLAY_NAME);
			if (lpProp) {
				*profname = lpProp->value.lpszA;
				talloc_free(proftable.aRow);
				return MAPI_E_SUCCESS;
			}
		}
	}
	
	MAPI_RETVAL_IF("GetDefaultProfile", MAPI_E_NOT_FOUND, proftable.aRow);
}

/**
 * Retrieve a pointer on the mapi_profile structure used in the given session
 */
_PUBLIC_ enum MAPISTATUS GetProfilePtr(struct mapi_profile *profile)
{
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	profile = global_mapi_ctx->session->profile;

	return MAPI_E_SUCCESS;
}

/**
 * Retrieve a SRowSet containing the profile table with only two
 * columns: PR_DISPLAY_NAME && PR_DEFAULT_PROFILE
 */
_PUBLIC_ enum MAPISTATUS GetProfileTable(struct SRowSet *proftable, uint32_t flags)
{
	TALLOC_CTX			*mem_ctx;
	enum ldb_scope			scope = LDB_SCOPE_SUBTREE;
	struct ldb_context		*ldb_ctx;
	struct ldb_result		*res;
	struct ldb_message		*msg;
	struct ldb_dn			*basedn;
	const char			*attrs[] = {"cn", "PR_DEFAULT_PROFILE", NULL};
	int				ret;
	uint32_t			count;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	ldb_ctx = global_mapi_ctx->ldb_ctx;
	mem_ctx = (TALLOC_CTX *)ldb_ctx;

	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, "CN=Profiles");
	ret = ldb_search(ldb_ctx, basedn, scope, "(cn=*)", attrs, &res);
	MAPI_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, NULL);

	/* Allocate Arow */
	proftable->cRows = res->count;
	proftable->aRow = talloc_array(mem_ctx, struct SRow, res->count);

	/* Build Arow array */
	for (count = 0; count < res->count; count++) {
		msg = res->msgs[count];
		
		proftable->aRow[count].lpProps = talloc_array(mem_ctx, struct SPropValue, 2);
		proftable->aRow[count].cValues = 2;

		proftable->aRow[count].lpProps[0].ulPropTag = PR_DISPLAY_NAME;
		proftable->aRow[count].lpProps[0].value.lpszA = ldb_msg_find_attr_as_string(msg, "cn", NULL);

		proftable->aRow[count].lpProps[1].ulPropTag = PR_DEFAULT_PROFILE;
		proftable->aRow[count].lpProps[1].value.l = ldb_msg_find_attr_as_int(msg, "PR_DEFAULT_PROFILE", 0);
	}

	return MAPI_E_SUCCESS;
}

/**
 * Retrieve an attribute value from the profile
 */
_PUBLIC_ enum MAPISTATUS GetProfileAttr(struct mapi_profile *profile, 
					const char *attribute, 
					unsigned int *count,
					char ***value)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_context		*ldb_ctx;
	struct ldb_result		*res;
	struct ldb_message		*msg;
	struct ldb_message_element	*ldb_element;
	struct ldb_dn			*basedn;
	char				*ldb_filter;
	const char			*attrs[] = {"*", NULL};
	int				ret;
	int				i;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!profile, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!attribute, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = (TALLOC_CTX *)global_mapi_ctx->ldb_ctx;
	ldb_ctx = global_mapi_ctx->ldb_ctx;

	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, "CN=Profiles");
	ldb_filter = talloc_asprintf(mem_ctx, "(cn=%s)", profile->profname);

	ret = ldb_search(ldb_ctx, basedn, LDB_SCOPE_SUBTREE, ldb_filter, attrs, &res);
	talloc_free(ldb_filter);
	MAPI_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, NULL);

	msg = res->msgs[0];
	ldb_element = ldb_msg_find_element(msg, attribute);
	if (!ldb_element) {
		DEBUG(3, ("ldb_msg_find_element: NULL\n"));
		return MAPI_E_NOT_FOUND;
	}

	*count = ldb_element[0].num_values;
	value[0] = talloc_array(mem_ctx, char *, *count);

	if (*count == 1) {
		value[0][0] = talloc_strdup(value[0], 
						ldb_msg_find_attr_as_string(msg, attribute, NULL));
	} else {
		for (i = 0; i < *count; i++) {
			value[0][i] = talloc_strdup(mem_ctx, (char *)ldb_element->values[i].data);
		}
	}

	return MAPI_E_SUCCESS;
}

/**
 * Search the value of an attribute within a given profile
 */
_PUBLIC_ enum MAPISTATUS FindProfileAttr(struct mapi_profile *profile, const char *attribute, const char *value)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_context		*ldb_ctx;
	struct ldb_result		*res;
	struct ldb_message		*msg;
	struct ldb_message_element	*ldb_element;
	struct ldb_val			val;
	struct ldb_dn			*basedn;
	char				*ldb_filter;
	const char			*attrs[] = {"*", NULL};
	int				ret;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!profile, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!attribute, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!value, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = (TALLOC_CTX *)global_mapi_ctx->ldb_ctx;
	ldb_ctx = global_mapi_ctx->ldb_ctx;

	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, "CN=Profiles");
	ldb_filter = talloc_asprintf(mem_ctx, "(CN=%s)", profile->profname);

	ret = ldb_search(ldb_ctx, basedn, LDB_SCOPE_SUBTREE, ldb_filter, attrs, &res);
	talloc_free(ldb_filter);
	MAPI_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, NULL);
	MAPI_RETVAL_IF(!res->count, MAPI_E_NOT_FOUND, NULL);

	msg = res->msgs[0];
	ldb_element = ldb_msg_find_element(msg, attribute);
	MAPI_RETVAL_IF(!ldb_element, MAPI_E_NOT_FOUND, NULL);

	val.data = (uint8_t *)talloc_strdup(mem_ctx, value);
	val.length = strlen(value);
	MAPI_RETVAL_IF(!ldb_msg_find_val(ldb_element, &val), MAPI_E_NOT_FOUND, NULL);

	return MAPI_E_SUCCESS;
}

/**
 * Create the profile 
 */

static bool set_profile_attribute(const char *profname, struct SRowSet rowset, 
				  uint32_t index, uint32_t property, const char *attr)
{
	struct SPropValue	*lpProp;
	enum MAPISTATUS		ret;

	lpProp = get_SPropValue_SRow(&(rowset.aRow[index]), property);

	if (!lpProp) {
		DEBUG(3, ("MAPI Property %s not set\n", attr));
		return true;
	}

	ret = mapi_profile_add_string_attr(profname, attr, lpProp->value.lpszA);

	if (ret != MAPI_E_SUCCESS) {
		DEBUG(3, ("Problem adding attribute %s in profile %s\n", attr, profname));
		return false;
	}
	return true;
}

static bool set_profile_mvstr_attribute(const char *profname, struct SRowSet rowset,
					uint32_t index, uint32_t property, const char *attr)
{
	struct SPropValue	*lpProp;
	enum MAPISTATUS		ret;
	int			i;

	lpProp = get_SPropValue_SRow(&(rowset.aRow[index]), property);

	if (!lpProp) {
		DEBUG(3, ("MAPI Property %s not set\n", attr));
		return true;
	}

	for (i = 0; i < lpProp->value.MVszA.cValues; i++) {
		ret = mapi_profile_add_string_attr(profname, attr, lpProp->value.MVszA.strings[i]->lppszA);
		if (ret != MAPI_E_SUCCESS) {
			DEBUG(3, ("Problem adding attribute %s in profile %s\n", attr, profname));
			return false;
		}
	}
	return true;
}

_PUBLIC_ enum MAPISTATUS ProcessNetworkProfile(struct mapi_session *session, const char *username,
					       mapi_profile_callback_t callback, const void *private)
{
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		rowset;
	struct SPropValue	*lpProp;
	const char		*profname;
	uint32_t		index = 0;

	MAPI_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!session->nspi->ctx, MAPI_E_END_OF_SESSION, NULL);
	
	nspi = (struct nspi_context *) session->nspi->ctx;
	profname = session->profile->profname;
	index = 0;
	
	retval = nspi_GetHierarchyInfo(nspi, &rowset);
	if (retval != MAPI_E_SUCCESS) return retval;

	SPropTagArray = set_SPropTagArray(nspi->mem_ctx, 0xd,
					  PR_DISPLAY_NAME,
					  PR_OFFICE_TELEPHONE_NUMBER,
					  PR_OFFICE_LOCATION,
					  PR_TITLE,
					  PR_COMPANY_NAME,
					  PR_ACCOUNT,
					  PR_ADDRTYPE,
					  PR_ENTRYID,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_INSTANCE_KEY,
					  PR_EMAIL_ADDRESS
					  );
	retval = nspi_GetMatches(nspi, SPropTagArray, &rowset, username);
	if (retval != MAPI_E_SUCCESS) return retval;

	/* if rowset count is superior than 1 an callback is specified, call it */
	if (rowset.cRows > 1 && callback) {
		index = callback(&rowset, private);
		MAPI_RETVAL_IF((index >= rowset.cRows), MAPI_E_USER_CANCEL, NULL);
	}

	lpProp = get_SPropValue_SRow(&(rowset.aRow[index]), PR_INSTANCE_KEY);
	if (lpProp) {
		struct SBinary bin;
		
		bin = lpProp->value.bin;
		nspi->profile_instance_key = *(uint32_t *)bin.lpb;
	} else {
		nspi->profile_instance_key = 0;
	}
	
	lpProp = get_SPropValue_SRow(&(rowset.aRow[index]), PR_EMAIL_ADDRESS);
	if (lpProp) {
		nspi->org = x500_get_dn_element(nspi->mem_ctx, lpProp->value.lpszA, ORG);
		nspi->org_unit = x500_get_dn_element(nspi->mem_ctx, lpProp->value.lpszA, ORG_UNIT);

		MAPI_RETVAL_IF(!nspi->org_unit, MAPI_E_INVALID_PARAMETER, NULL);
		MAPI_RETVAL_IF(!nspi->org, MAPI_E_INVALID_PARAMETER, NULL);

		retval = mapi_profile_add_string_attr(profname, "Organization", nspi->org);
		retval = mapi_profile_add_string_attr(profname, "OrganizationUnit", nspi->org_unit);
	} else {
		MAPI_RETVAL_IF(!lpProp, MAPI_E_NOT_FOUND, NULL);
	}
	
	set_profile_attribute(profname, rowset, index, PR_EMAIL_ADDRESS, "EmailAddress");
	set_profile_attribute(profname, rowset, index, PR_DISPLAY_NAME, "DisplayName");
	set_profile_attribute(profname, rowset, index, PR_ACCOUNT, "Account");
	set_profile_attribute(profname, rowset, index, PR_ADDRTYPE, "AddrType");

	SPropTagArray = set_SPropTagArray(nspi->mem_ctx, 0x8,
					  PR_DISPLAY_NAME,
					  PR_EMAIL_ADDRESS,
					  PR_DISPLAY_TYPE,
					  PR_EMS_AB_HOME_MDB,
					  PR_ATTACH_NUM,
					  PR_PROFILE_HOME_SERVER_ADDRS,
					  PR_EMS_AB_PROXY_ADDRESSES
					  );
	retval = nspi_QueryRows(nspi, SPropTagArray, &rowset);
	if (retval != MAPI_E_SUCCESS) return retval;

	lpProp = get_SPropValue_SRowSet(&rowset, PR_EMS_AB_HOME_MDB);
	MAPI_RETVAL_IF(!lpProp, MAPI_E_NOT_FOUND, NULL);

	nspi->servername = x500_get_servername(nspi->mem_ctx, lpProp->value.lpszA);
	mapi_profile_add_string_attr(profname, "ServerName", nspi->servername);
	set_profile_attribute(profname, rowset, 0, PR_EMS_AB_HOME_MDB, "HomeMDB");
	set_profile_mvstr_attribute(profname, rowset, 0, PR_EMS_AB_PROXY_ADDRESSES, "ProxyAddress");

	retval = nspi_DNToEph(nspi);
	if (retval != MAPI_E_SUCCESS) return retval;

	SPropTagArray = set_SPropTagArray(nspi->mem_ctx, 0x1, PR_EMS_AB_NETWORK_ADDRESS);
	retval = nspi_GetProps(nspi, SPropTagArray, &rowset);
	if (retval != MAPI_E_SUCCESS) return retval;
	set_profile_mvstr_attribute(profname, rowset, 0, PR_EMS_AB_NETWORK_ADDRESS, "NetworkAddress");

	return MAPI_E_SUCCESS;
}
