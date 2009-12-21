/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2009.
   Copyright (C) Fabien Le Mentec 2007.

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

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <credentials.h>
#include <ldb_errors.h>
#include <ldb.h>

/**
   \file IProfAdmin.c

   \brief MAPI Profiles interface
 */

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
	struct ldb_result	*res;
	struct ldb_message	*msg;
	const char * const	attrs[] = { "*", NULL };

	/* fills in profile name */
	profile->profname = talloc_strdup(mem_ctx, profname);
	if (!profile->profname) return MAPI_E_NOT_ENOUGH_RESOURCES;

	/* ldb query */
	ldb_ctx = global_mapi_ctx->ldb_ctx;

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx), scope, attrs, "(cn=%s)(cn=Profiles)", profile->profname);
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
	profile->seal = ldb_msg_find_attr_as_bool(msg, "seal", false);
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
	uint32_t       		i;

	ldb_ctx = global_mapi_ctx->ldb_ctx;

	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, "CN=Profiles");
	ret = ldb_search(ldb_ctx, mem_ctx, &res, basedn, scope, attrs, "(cn=*)");
	
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

	ldb_ctx = global_mapi_ctx->ldb_ctx;
	
	ret = ldb_search(ldb_ctx, mem_ctx, &res, NULL, scope, attrs, 
					 "(cn=%s)(cn=Profiles)", profile);

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
	int				ret;
	const char * const		attrs[] = { "*", NULL };

	/* Sanity check */
	if (profname == NULL) 
		return MAPI_E_BAD_VALUE;

	/* Does the profile already exists? */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx), 
					 scope, attrs, "(cn=%s)(cn=Profiles)", profname);
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
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx), scope, attrs, "(cn=%s)(cn=Profiles)", profname);
	if (!res->msgs) return MAPI_E_NOT_FOUND;

	ret = ldb_delete(ldb_ctx, res->msgs[0]->dn);
	if (ret != LDB_SUCCESS) return MAPI_E_NOT_FOUND;
	
	return MAPI_E_SUCCESS;
}


/**
   \details Rename a profile

   \param mem_ctx pointer on the memory context
   \param old_profname the old profile name string
   \param new_profname the new profile name string

   \return MAPI_E_SUCCESS on success otherwise MAPI error.
 */
static enum MAPISTATUS ldb_rename_profile(TALLOC_CTX *mem_ctx, 
					  const char *old_profname,
					  const char *new_profname)
{
	int			ret;
	struct ldb_context	*ldb_ctx;
	struct ldb_dn		*old_dn;
	struct ldb_dn		*new_dn;
	char			*dn;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!old_profname, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!new_profname, MAPI_E_INVALID_PARAMETER, NULL);

	ldb_ctx = global_mapi_ctx->ldb_ctx;

	dn = talloc_asprintf(mem_ctx, "CN=%s,CN=Profiles", old_profname);
	old_dn = ldb_dn_new(mem_ctx, ldb_ctx, dn);
	talloc_free(dn);

	dn = talloc_asprintf(mem_ctx, "CN=%s,CN=Profiles", new_profname);
	new_dn = ldb_dn_new(mem_ctx, ldb_ctx, dn);
	talloc_free(dn);

	ret = ldb_rename(ldb_ctx, old_dn, new_dn);
	OPENCHANGE_RETVAL_IF(ret, MAPI_E_CORRUPT_STORE, NULL);

	return MAPI_E_SUCCESS;
}


/*
 * Public interface
 */

/**
   \details Add an attribute to the profile
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
	int				ret;
	const char * const		attrs[] = { "*", NULL };

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx->ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!profile, MAPI_E_BAD_VALUE, NULL);
	OPENCHANGE_RETVAL_IF(!value, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "mapi_profile_add_string_attr");
	ldb_ctx = global_mapi_ctx->ldb_ctx;

	/* Retrieve the profile from the database */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx), scope, attrs, "(cn=%s)(cn=Profiles)", profile);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_BAD_VALUE, mem_ctx);

	/* Preparing for the transaction */
	dn = talloc_asprintf(mem_ctx, "CN=%s,CN=Profiles", profile);
	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, dn);
	talloc_free(dn);
	OPENCHANGE_RETVAL_IF(!ldb_dn_validate(basedn), MAPI_E_BAD_VALUE, mem_ctx);

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
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);
	
	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}

/**
   \details Modify an attribute
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
	int				ret;
	const char * const		attrs[] = { "*", NULL };

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!profname, MAPI_E_BAD_VALUE, NULL);

	ldb_ctx = global_mapi_ctx->ldb_ctx;
	mem_ctx = talloc_named(NULL, 0, "mapi_profile_modify_string_attr");

	/* Retrieve the profile from the database */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx), scope, attrs, "(cn=%s)(cn=Profiles)", profname);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_BAD_VALUE, mem_ctx);

	/* Preparing for the transaction */
	dn = talloc_asprintf(mem_ctx, "CN=%s,CN=Profiles", profname);
	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, dn);
	talloc_free(dn);
	OPENCHANGE_RETVAL_IF(!ldb_dn_validate(basedn), MAPI_E_BAD_VALUE, mem_ctx);

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
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}

/**
   \details Delete an attribute
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
	int				ret;
	const char * const		attrs[] = { "*", NULL };

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!profname, MAPI_E_BAD_VALUE, NULL);

	ldb_ctx = global_mapi_ctx->ldb_ctx;
	mem_ctx = talloc_named(NULL, 0, "mapi_profile_delete_string_attr");

	/* Retrieve the profile from the database */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx), scope, attrs, "(cn=%s)(cn=Profiles)", profname);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_BAD_VALUE, mem_ctx);

	/* Preparing for the transaction */
	dn = talloc_asprintf(mem_ctx, "CN=%s,CN=Profiles", profname);
	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, dn);
	talloc_free(dn);
	OPENCHANGE_RETVAL_IF(!ldb_dn_validate(basedn), MAPI_E_BAD_VALUE, mem_ctx);

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
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}

/*
 * Private function which opens the profile store
 * Should only be called within MAPIInitialize
 */
enum MAPISTATUS OpenProfileStore(TALLOC_CTX *mem_ctx, struct ldb_context **ldb_ctx, 
				 const char *profiledb)
{
	int			ret;
	struct ldb_context	*tmp_ctx;
	struct tevent_context	*ev;
	
	*ldb_ctx = 0;
	
	/* store path */
	if (!profiledb) return MAPI_E_NOT_FOUND;

	ev = tevent_context_init(mem_ctx);
	if (!ev) return MAPI_E_NOT_ENOUGH_RESOURCES;

	/* connect to the store */
	tmp_ctx = ldb_init(mem_ctx, ev);
	if (!tmp_ctx) return MAPI_E_NOT_ENOUGH_RESOURCES;

	ret = ldb_connect(tmp_ctx, profiledb, 0, NULL);
	if (ret != LDB_SUCCESS) return MAPI_E_NOT_FOUND;

	*ldb_ctx = tmp_ctx;
	return MAPI_E_SUCCESS;
}


/**
   \details Get default ldif_path

   This function returns the path for the default LDIF files.

   \sa CreateProfileStore
*/
_PUBLIC_ const char *mapi_profile_get_ldif_path(void)
{
	return DEFAULT_LDIF;
}


/**
   \details Create a profile database
   
   This function creates a new profile database, including doing an
   initial setup.

   \param profiledb the absolute path to the profile database intended
   to be created
   \param ldif_path the absolute path to the LDIF information to use
   for initial setup.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
 
   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_CALL_FAILED: profiledb or ldif_path is not set
   - MAPI_E_NOT_ENOUGH_RESOURCES: ldb subsystem initialization failed
   - MAPI_E_NO_ACCESS: connection or ldif add failed

   \sa GetLastError, mapi_profile_get_ldif_path
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
	struct tevent_context	*ev;

	OPENCHANGE_RETVAL_IF(!profiledb, MAPI_E_CALL_FAILED, NULL);
	OPENCHANGE_RETVAL_IF(!ldif_path, MAPI_E_CALL_FAILED, NULL);

	mem_ctx = talloc_named(NULL, 0, "CreateProfileStore");

	ev = tevent_context_init(mem_ctx);
	OPENCHANGE_RETVAL_IF(!ev, MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	ldb_ctx = ldb_init(mem_ctx, ev);
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	url = talloc_asprintf(mem_ctx, "tdb://%s", profiledb);
	ret = ldb_connect(ldb_ctx, url, 0, 0);
	talloc_free(url);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_ACCESS, mem_ctx);

	filename = talloc_asprintf(mem_ctx, "%s/oc_profiles_init.ldif", ldif_path);
	f = fopen(filename, "r");
	OPENCHANGE_RETVAL_IF(!f, MAPI_E_NO_ACCESS, mem_ctx);
	talloc_free(filename);

	while ((ldif = ldb_ldif_read_file(ldb_ctx, f))) {
		ldif->msg = ldb_msg_canonicalize(ldb_ctx, ldif->msg);
		ret = ldb_add(ldb_ctx, ldif->msg);
		if (ret != LDB_SUCCESS) {
			fclose(f);
			OPENCHANGE_RETVAL_ERR(MAPI_E_NO_ACCESS, mem_ctx);
		}
		ldb_ldif_read_free(ldb_ctx, ldif);
	}
	fclose(f);

	filename = talloc_asprintf(mem_ctx, "%s/oc_profiles_schema.ldif", ldif_path);
	f = fopen(filename, "r");
	OPENCHANGE_RETVAL_IF(!f, MAPI_E_NO_ACCESS, mem_ctx);

	talloc_free(filename);
	while ((ldif = ldb_ldif_read_file(ldb_ctx, f))) {
		ldif->msg = ldb_msg_canonicalize(ldb_ctx, ldif->msg);
		ret = ldb_add(ldb_ctx, ldif->msg);
		if (ret != LDB_SUCCESS) {
			fclose(f);
			OPENCHANGE_RETVAL_ERR(MAPI_E_NO_ACCESS, mem_ctx);
		}

		ldb_ldif_read_free(ldb_ctx, ldif);
	}
	fclose(f);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Load a profile from the database

   This function opens a named profile from the database, and fills
   the mapi_profile structure with common profile information.

   \param profile the resulting profile
   \param profname the name of the profile to open
   \param password the password to use with the profile

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_ENOUGH_RESOURCES: ldb subsystem initialization failed
   - MAPI_E_NOT_FOUND: the profile was not found in the profile
     database
   - MAPI_E_COLLISION: profname matched more than one entry

   \sa MAPIInitialize, GetLastError
*/
_PUBLIC_ enum MAPISTATUS OpenProfile(struct mapi_profile *profile, const char *profname, const char *password)
{
	TALLOC_CTX	*mem_ctx;
	enum MAPISTATUS	retval;

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = (TALLOC_CTX *) global_mapi_ctx->session;
	
	/* find the profile in ldb store */
	retval = ldb_load_profile(mem_ctx, global_mapi_ctx->ldb_ctx, profile, profname, password);

	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;	
}

/**
   \details Load a MAPI Profile and sets its credentials

   This function loads a named MAPI profile and sets the MAPI session
   credentials.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: The profile parameter is not
     initialized
   - MAPI_E_NOT_ENOUGH_RESOURCES: MAPI subsystem failed to allocate
     the necessary resources to perform the operation

   \sa OpenProfile, GetLastError
 */
_PUBLIC_ enum MAPISTATUS LoadProfile(struct mapi_profile *profile)
{
	TALLOC_CTX *mem_ctx;

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!profile, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = (TALLOC_CTX *) global_mapi_ctx->session;

	profile->credentials = cli_credentials_init(mem_ctx);
	OPENCHANGE_RETVAL_IF(!profile->credentials, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
	cli_credentials_set_username(profile->credentials, profile->username, CRED_SPECIFIED);
	cli_credentials_set_password(profile->credentials, profile->password, CRED_SPECIFIED);
	cli_credentials_set_workstation(profile->credentials, profile->workstation, CRED_SPECIFIED);
	cli_credentials_set_realm(profile->credentials, profile->realm, CRED_SPECIFIED);
	cli_credentials_set_domain(profile->credentials, profile->domain, CRED_SPECIFIED);

	return MAPI_E_SUCCESS;
}

/**
   \details Release a profile

   This function releases the credentials associated with the profile.

   \param profile the profile to release.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: The profile parameter was not set or
     not valid
 */
_PUBLIC_ enum MAPISTATUS ShutDown(struct mapi_profile *profile)
{
	OPENCHANGE_RETVAL_IF(!profile, MAPI_E_INVALID_PARAMETER, NULL);

	if (profile->credentials) 
		talloc_free(profile->credentials);
	return MAPI_E_SUCCESS;
}


/**
   \details Create a profile in the MAPI profile database

   This function creates a profile named \a profile in the MAPI profile
   database and sets the specified username in that profile.

   This function may also set the password. If the flags include
   OC_PROFILE_NOPASSWORD then the password will not be set. Otherwise,
   the specified password argument will also be saved to the profile.

   \param profile the name of the profile
   \param username the username of the profile
   \param password the password for the profile (if used)
   \param flag the union of the flags. 

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been
     initialized. The MAPI subsystem must be initialized (using
     MAPIInitialize) prior to creating a profile.
   - MAPI_E_NOT_ENOUGH_RESOURCES: MAPI subsystem failed to allocate
     the necessary resources to operate properly
   - MAPI_E_NO_SUPPORT: An error was encountered while setting the
     MAPI profile attributes in the database.

   \note profile information (including the password, if saved to the
   profile) is stored unencrypted.

   \sa DeleteProfile, SetDefaultProfile, GetDefaultProfile,
   ChangeProfilePassword, GetProfileTable, ProcessNetworkProfile,
   GetLastError
 */
_PUBLIC_ enum MAPISTATUS CreateProfile(const char *profile, const char *username,
				       const char *password, uint32_t flag)
{
	enum MAPISTATUS retval;
	TALLOC_CTX	*mem_ctx;

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = talloc_named(NULL, 0, "CreateProfile");
	retval = ldb_create_profile(mem_ctx, global_mapi_ctx->ldb_ctx, profile);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	retval = mapi_profile_add_string_attr(profile, "username", username);
	if (flag != OC_PROFILE_NOPASSWORD) {
		retval = mapi_profile_add_string_attr(profile, "password", password);
	}
	talloc_free(mem_ctx);

	return retval;
}


/**
   \details Delete a profile from the MAPI profile database

   \param profile the name of the profile to delete

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been
     initialized. The MAPI subsystem must be initialized (using
     MAPIInitialize) prior to creating a profile.
   - MAPI_E_NOT_FOUND: The profile was not found in the database.

   \sa CreateProfile, ChangeProfilePassword, GetProfileTable,
   GetProfileAttr, ProcessNetworkProfile, GetLastError
*/
_PUBLIC_ enum MAPISTATUS DeleteProfile(const char *profile)
{
	TALLOC_CTX	*mem_ctx;
	enum MAPISTATUS	retval;

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = talloc_named(NULL, 0, "DeleteProfile");
	retval = ldb_delete_profile(mem_ctx, global_mapi_ctx->ldb_ctx, profile);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
	talloc_free(mem_ctx);

	return retval;
}


/**
   \details Change the profile password of an existing MAPI profile

   \param profile the name of the profile to have its password changed
   \param old_password the old password
   \param password the new password

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: One of the following argument was not
     set: profile, old_password, password
   - MAPI_E_NOT_FOUND: The profile was not found in the database

   \sa CreateProfile, GetProfileTable, GetProfileAttr,
   ProcessNetworkProfile, GetLastError
 */
_PUBLIC_ enum MAPISTATUS ChangeProfilePassword(const char *profile, 
					       const char *old_password,
					       const char *password)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;

	if (!profile || !old_password | !password) return MAPI_E_INVALID_PARAMETER;

	mem_ctx = talloc_named(NULL, 0, "ChangeProfilePassword");

	retval = ldb_test_password(mem_ctx, profile, password);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	retval = mapi_profile_modify_string_attr(profile, "password", password);
	
	talloc_free(mem_ctx);
	return retval;
}


/**
   \details Rename a profile

   \param old_profile old profile name
   \param profile new profile name

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
 */
_PUBLIC_ enum MAPISTATUS RenameProfile(const char *old_profile, 
				       const char *profile)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct SRowSet		proftable;
	struct SPropValue	*lpProp;
	struct ldb_message	*msg = NULL;
	bool			found = false;
	char			*dn = NULL;
	int			ret;
	int			i;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!old_profile, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!profile, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = global_mapi_ctx->mem_ctx;

	retval = GetProfileTable(&proftable);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Step 1. Check if old profile exists */
	for (found = false, i = 0; i < proftable.cRows; i++) {
		lpProp = get_SPropValue_SRow(&(proftable.aRow[i]), PR_DISPLAY_NAME);
		if (lpProp && !strcmp(lpProp->value.lpszA, old_profile)) {
			found = true;
		}
	}
	OPENCHANGE_RETVAL_IF(found == false, MAPI_E_NOT_FOUND, proftable.aRow);

	/* Step 2. Check if new profile already exists */
	for (found = false, i = 0; i < proftable.cRows; i++) {
		lpProp = get_SPropValue_SRow(&(proftable.aRow[i]), PR_DISPLAY_NAME);
		if (lpProp && !strcmp(lpProp->value.lpszA, profile)) {
			found = true;
		}
	}
	talloc_free(proftable.aRow);
	OPENCHANGE_RETVAL_IF(found == true, MAPI_E_INVALID_PARAMETER, NULL);

	/* Step 3. Rename the profile */
	retval = ldb_rename_profile(mem_ctx, old_profile, profile);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Step 4. Change name and cn */
	msg = ldb_msg_new(mem_ctx);
	dn = talloc_asprintf(mem_ctx, "CN=%s,CN=Profiles", profile);
	msg->dn = ldb_dn_new(mem_ctx, global_mapi_ctx->ldb_ctx, dn);
	talloc_free(dn);

	ret = ldb_msg_add_string(msg, "cn", profile);
	ret = ldb_msg_add_string(msg, "name", profile);
	for (i = 0; i < msg->num_elements; i++) {
		msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
	}

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_CORRUPT_STORE, NULL);
	ret = ldb_modify(global_mapi_ctx->ldb_ctx, msg);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_CORRUPT_STORE, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Set a default profile for the database

   \param profname the name of the profile to make the default profile

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: The profile parameter was not set
     properly.
   - MAPI_E_NOT_FOUND: The profile was not found in the database

   \sa GetDefaultProfile, GetProfileTable, GetLastError
 */
_PUBLIC_ enum MAPISTATUS SetDefaultProfile(const char *profname)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_profile	profile;
	enum MAPISTATUS		retval;

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!profname, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "SetDefaultProfile");

	/* open profile */
	retval = ldb_load_profile(mem_ctx, global_mapi_ctx->ldb_ctx, &profile, profname, NULL);
	OPENCHANGE_RETVAL_IF(retval && retval != MAPI_E_INVALID_PARAMETER, retval, mem_ctx);

	/* search any previous default profile and unset it */
	retval = ldb_clear_default_profile(mem_ctx);

	/* set profname as the default profile */
	retval = mapi_profile_modify_string_attr(profname, "PR_DEFAULT_PROFILE", "1");

	talloc_free(mem_ctx);

	return retval;
}


/**
   \details Get the default profile from the database

   \param profname the result of the function (name of the default
   profile)

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_NOT_FOUND: The profile was not found in the database

   \note On success GetDefaultProfile profname string is allocated. It
   is up to the developer to free it when not needed anymore.

   \sa SetDefaultProfile, GetProfileTable, GetLastError
*/
_PUBLIC_ enum MAPISTATUS GetDefaultProfile(char **profname)
{
	enum MAPISTATUS		retval;
	struct SRowSet		proftable;
	struct SPropValue	*lpProp;
	uint32_t		i;
	

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	
	retval = GetProfileTable(&proftable);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	for (i = 0; i < proftable.cRows; i++) {
		lpProp = get_SPropValue_SRow(&(proftable.aRow[i]), PR_DEFAULT_PROFILE);
		if (lpProp && (lpProp->value.l == 1)) {
			lpProp = get_SPropValue_SRow(&(proftable.aRow[i]), PR_DISPLAY_NAME);
			if (lpProp) {
			  *profname = talloc_strdup(global_mapi_ctx->mem_ctx, lpProp->value.lpszA);
				talloc_free(proftable.aRow);
				return MAPI_E_SUCCESS;
			}
		}
	}
	
	OPENCHANGE_RETVAL_ERR(MAPI_E_NOT_FOUND, proftable.aRow);
}


/**
   \details Retrieve the profile table

   This function retrieves the profile table. Two fields are returned:
   - PR_DISPLAY_NAME: The profile name stored as a UTF8 string
   - PR_DEFAULT_PROFILE: Whether the profile is the default one(1) or
     not(0), stored as an integer

   \param proftable the result of the call

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_NOT_FOUND: The profile was not found in the database

   \sa SetDefaultProfile, GetLastError
 */
_PUBLIC_ enum MAPISTATUS GetProfileTable(struct SRowSet *proftable)
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

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	ldb_ctx = global_mapi_ctx->ldb_ctx;
	mem_ctx = talloc_autofree_context();

	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, "CN=Profiles");
	ret = ldb_search(ldb_ctx, mem_ctx, &res, basedn, scope, attrs, "(cn=*)");
	talloc_free(basedn);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, NULL);

	/* Allocate Arow */
	proftable->cRows = res->count;
	proftable->aRow = talloc_array(global_mapi_ctx->mem_ctx, struct SRow, res->count);

	/* Build Arow array */
	for (count = 0; count < res->count; count++) {
		msg = res->msgs[count];
		
		proftable->aRow[count].lpProps = talloc_array((TALLOC_CTX *)proftable->aRow, struct SPropValue, 2);
		proftable->aRow[count].cValues = 2;

		proftable->aRow[count].lpProps[0].ulPropTag = PR_DISPLAY_NAME;
		proftable->aRow[count].lpProps[0].value.lpszA = talloc_strdup((TALLOC_CTX *)proftable->aRow, 
									      ldb_msg_find_attr_as_string(msg, "cn", NULL));
									      
		proftable->aRow[count].lpProps[1].ulPropTag = PR_DEFAULT_PROFILE;
		proftable->aRow[count].lpProps[1].value.l = ldb_msg_find_attr_as_int(msg, "PR_DEFAULT_PROFILE", 0);
	}

	talloc_free(res);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve attribute values from a profile

   This function retrieves all the attribute values from the given
   profile.  The number of results is stored in \a count and values
   are stored in an allocated string array in the \a value parameter
   that needs to be free'd using MAPIFreeBuffer().
   
   \param profile the name of the profile to retrieve attributes from
   \param attribute the attribute(s) to search for
   \param count the number of results
   \param value the resulting values
   
   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: Either profile or attribute was not set
     properly
   - MAPI_E_NOT_FOUND: The profile was not found in the database

   \sa SetDefaultProfile, GetDefaultProfile, MAPIFreeBuffer,
   GetProfileTable, GetLastError
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
	const char			*attrs[] = {"*", NULL};
	int				ret;
	uint32_t       			i;

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!profile, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!attribute, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = (TALLOC_CTX *)global_mapi_ctx->ldb_ctx;
	ldb_ctx = global_mapi_ctx->ldb_ctx;

	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, "CN=Profiles");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, basedn, LDB_SCOPE_SUBTREE, attrs, "(cn=%s)", profile->profname);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, NULL);

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
   \details Search the value of an attribute within a given profile
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
	const char			*attrs[] = {"*", NULL};
	int				ret;

	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!profile, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!attribute, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!value, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = (TALLOC_CTX *)global_mapi_ctx->ldb_ctx;
	ldb_ctx = global_mapi_ctx->ldb_ctx;

	basedn = ldb_dn_new(ldb_ctx, ldb_ctx, "CN=Profiles");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, basedn, LDB_SCOPE_SUBTREE, attrs, "(CN=%s)", profile->profname);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, NULL);
	OPENCHANGE_RETVAL_IF(!res->count, MAPI_E_NOT_FOUND, NULL);

	msg = res->msgs[0];
	ldb_element = ldb_msg_find_element(msg, attribute);
	OPENCHANGE_RETVAL_IF(!ldb_element, MAPI_E_NOT_FOUND, NULL);

	val.data = (uint8_t *)talloc_strdup(mem_ctx, value);
	val.length = strlen(value);
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_val(ldb_element, &val), MAPI_E_NOT_FOUND, NULL);

	return MAPI_E_SUCCESS;
}

/**
   Create the profile 
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
	uint32_t       		i;

	lpProp = get_SPropValue_SRow(&(rowset.aRow[index]), property);

	if (!lpProp) {
		DEBUG(3, ("MAPI Property %s not set\n", attr));
		return true;
	}

	for (i = 0; i < lpProp->value.MVszA.cValues; i++) {
		ret = mapi_profile_add_string_attr(profname, attr, lpProp->value.MVszA.lppszA[i]);
		if (ret != MAPI_E_SUCCESS) {
			DEBUG(3, ("Problem adding attribute %s in profile %s\n", attr, profname));
			return false;
		}
	}
	return true;
}


/**
   \details Process a full and automated MAPI profile creation

   This function process a full and automated MAPI profile creation
   using the \a username pattern passed as a parameter. The functions
   takes a callback parameter which will be called when the username
   checked matches several usernames. Private data needed by the
   callback can be supplied using the private_data pointer.

   \code
   typedef int (*mapi_callback_t) callback(struct SRowSet *, void *private_data);
   \endcode
   
   The callback returns the SRow element index within the SRowSet
   structure.  If the user cancels the operation the callback return
   value should be SRowSet->cRows or more.

   \param session the session context
   \param username the username for the network profile
   \param callback function pointer callback function
   \param private_data context data that will be provided to the callback

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:

   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been
     initialized. The MAPI subsystem must be initialized (using
     MAPIInitialize) prior to creating a profile.
   - MAPI_E_END_OF_SESSION: The NSPI session has not been initialized
   - MAPI_E_CANCEL_USER: The user has aborted the operation
   - MAPI_E_INVALID_PARAMETER: The profile parameter was not set
     properly.
   - MAPI_E_NOT_FOUND: One of the mandatory field was not found during
     the profile creation process.

   \sa OpenProfileStore, MAPILogonProvider, GetLastError
*/
_PUBLIC_ enum MAPISTATUS ProcessNetworkProfile(struct mapi_session *session, const char *username,
					       mapi_profile_callback_t callback, const void *private_data)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct nspi_context	*nspi;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct Restriction_r	Filter;
	struct SRowSet		*SRowSet;
	struct SPropValue	*lpProp = NULL;
	struct SPropTagArray	*MIds = NULL;
	struct SPropTagArray	MIds2;
	struct SPropTagArray	*MId_server = NULL;
	struct StringsArray_r	pNames;
	const char		*profname;
	uint32_t		instance_key = 0;
	uint32_t		index = 0;

	OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!session->nspi->ctx, MAPI_E_END_OF_SESSION, NULL);
	
	mem_ctx = talloc_named(NULL, 0, "ProcessNetworkProfile");
	nspi = (struct nspi_context *) session->nspi->ctx;
	profname = session->profile->profname;
	index = 0;

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	retval = nspi_GetSpecialTable(nspi, mem_ctx, 0x0, &SRowSet);
	MAPIFreeBuffer(SRowSet);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	SPropTagArray = set_SPropTagArray(mem_ctx, 0xc,
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

	/* Retrieve the username to match */
	if (!username) {
		username = cli_credentials_get_username(nspi->cred);
		OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, mem_ctx);
	}

	/* Build the restriction we want for NspiGetMatches */
	lpProp = talloc_zero(mem_ctx, struct SPropValue);
	lpProp->ulPropTag = PR_ANR_UNICODE;
	lpProp->dwAlignPad = 0;
	lpProp->value.lpszW = username;

	Filter.rt = RES_PROPERTY;
	Filter.res.resProperty.relop = RES_PROPERTY;
	Filter.res.resProperty.ulPropTag = PR_ANR_UNICODE;
	Filter.res.resProperty.lpProp = lpProp;

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	MIds = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = nspi_GetMatches(nspi, mem_ctx, SPropTagArray, &Filter, &SRowSet, &MIds);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(lpProp);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		MAPIFreeBuffer(SRowSet);
		talloc_free(mem_ctx);
		return retval;
	}

	/* if there's no match */
	OPENCHANGE_RETVAL_IF(!SRowSet, MAPI_E_NOT_FOUND, mem_ctx);
	OPENCHANGE_RETVAL_IF(!SRowSet->cRows, MAPI_E_NOT_FOUND, mem_ctx);
	OPENCHANGE_RETVAL_IF(!MIds, MAPI_E_NOT_FOUND, mem_ctx);

	/* if SRowSet count is superior than 1 an callback is specified, call it */
	if (SRowSet->cRows > 1 && callback) {
		index = callback(SRowSet, private_data);
		OPENCHANGE_RETVAL_IF((index >= SRowSet->cRows), MAPI_E_USER_CANCEL, mem_ctx);
		instance_key = MIds->aulPropTag[index];
	} else {
		instance_key = MIds->aulPropTag[0];
	}
	MAPIFreeBuffer(MIds);
	
	MIds2.cValues = 0x1;
	MIds2.aulPropTag = (enum MAPITAGS *) &instance_key;

	set_profile_attribute(profname, *SRowSet, index, PR_EMAIL_ADDRESS, "EmailAddress");
	set_profile_attribute(profname, *SRowSet, index, PR_DISPLAY_NAME, "DisplayName");
	set_profile_attribute(profname, *SRowSet, index, PR_ACCOUNT, "Account");
	set_profile_attribute(profname, *SRowSet, index, PR_ADDRTYPE, "AddrType");

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x7,
					  PR_DISPLAY_NAME,
					  PR_EMAIL_ADDRESS,
					  PR_DISPLAY_TYPE,
					  PR_EMS_AB_HOME_MDB,
					  PR_ATTACH_NUM,
					  PR_PROFILE_HOME_SERVER_ADDRS,
					  PR_EMS_AB_PROXY_ADDRESSES
					  );

	nspi->pStat->CurrentRec = 0x0;
	nspi->pStat->Delta = 0x0;
	nspi->pStat->NumPos = 0x0;
	nspi->pStat->TotalRecs = 0x1;

	MAPIFreeBuffer(SRowSet);
	SRowSet = talloc(mem_ctx, struct SRowSet);
	retval = nspi_QueryRows(nspi, mem_ctx, SPropTagArray, &MIds2, 1, &SRowSet);
	MAPIFreeBuffer(SPropTagArray);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	lpProp = get_SPropValue_SRowSet(SRowSet, PR_EMS_AB_HOME_MDB);
	OPENCHANGE_RETVAL_IF(!lpProp, MAPI_E_NOT_FOUND, mem_ctx);

	nspi->org = x500_get_dn_element(mem_ctx, lpProp->value.lpszA, ORG);
	nspi->org_unit = x500_get_dn_element(mem_ctx, lpProp->value.lpszA, ORG_UNIT);
	
	OPENCHANGE_RETVAL_IF(!nspi->org_unit, MAPI_E_INVALID_PARAMETER, mem_ctx);
	OPENCHANGE_RETVAL_IF(!nspi->org, MAPI_E_INVALID_PARAMETER, mem_ctx);

	retval = mapi_profile_add_string_attr(profname, "Organization", nspi->org);
	retval = mapi_profile_add_string_attr(profname, "OrganizationUnit", nspi->org_unit);

	nspi->servername = x500_get_servername(lpProp->value.lpszA);
	mapi_profile_add_string_attr(profname, "ServerName", nspi->servername);
	set_profile_attribute(profname, *SRowSet, 0, PR_EMS_AB_HOME_MDB, "HomeMDB");
	set_profile_mvstr_attribute(profname, *SRowSet, 0, PR_EMS_AB_PROXY_ADDRESSES, "ProxyAddress");

	pNames.Count = 0x1;
	pNames.Strings = (const char **) talloc_array(mem_ctx, char **, 1);
	pNames.Strings[0] = (const char *) talloc_asprintf(mem_ctx, SERVER_DN, 
							   nspi->org, nspi->org_unit, 
							   nspi->servername);
	MId_server = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = nspi_DNToMId(nspi, mem_ctx, &pNames, &MId_server);
	MAPIFreeBuffer((char *)pNames.Strings[0]);
	MAPIFreeBuffer((char **)pNames.Strings);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	MAPIFreeBuffer(SRowSet);
	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_EMS_AB_NETWORK_ADDRESS);
	retval = nspi_GetProps(nspi, mem_ctx, SPropTagArray, MId_server, &SRowSet);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(MId_server);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	set_profile_mvstr_attribute(profname, *SRowSet, 0, PR_EMS_AB_NETWORK_ADDRESS, "NetworkAddress");
	MAPIFreeBuffer(SRowSet);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
