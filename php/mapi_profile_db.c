/*
   OpenChange MAPI PHP bindings

   Copyright (C) 2013 Javier Amor Garcia.

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"
#include "mapi_profile_db.h"
#include "mapi_profile.h"

static zend_function_entry mapi_profile_db_class_functions[] = {
	PHP_ME(MAPIProfileDB,	__construct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIProfileDB,	__destruct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIProfileDB,	path,		NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIProfileDB,	profiles,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIProfileDB,	getProfile,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIProfileDB,	createAndGetProfile,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIProfileDB,	debug,		NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};


static zend_class_entry		*mapi_profile_db_ce;
static zend_object_handlers	mapi_profile_db_object_handlers;


static void mapi_profile_db_free_storage(void *object TSRMLS_DC)
{
	mapi_profile_db_object_t	*obj;

	obj = (mapi_profile_db_object_t *) object;
	if (obj->mapi_ctx) {
		MAPIUninitialize(obj->mapi_ctx);
	}
	if (obj->path) {
		efree(obj->path);
	}
	if (obj->talloc_ctx) {
		talloc_free(obj->talloc_ctx);
	}

	zend_object_std_dtor(&(obj->std) TSRMLS_CC);
	efree(obj);
}

static zend_object_value mapi_profile_db_create_handler(zend_class_entry *type TSRMLS_DC)
{
	zval				*tmp;
	zend_object_value		retval;
	mapi_profile_db_object_t	*obj;

	obj = (mapi_profile_db_object_t*) emalloc(sizeof(mapi_profile_db_object_t));
	memset(obj, 0, sizeof(mapi_profile_db_object_t));

	obj->std.ce = type;

	ALLOC_HASHTABLE(obj->std.properties);
	zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
#if PHP_VERSION_ID < 50399
	zend_hash_copy(obj->std.properties, &type->default_properties,
		       (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
#else
	object_properties_init((zend_object *)&(obj->std), type);
#endif
	retval.handle = zend_objects_store_put(obj, NULL, mapi_profile_db_free_storage,
					       NULL TSRMLS_CC);
	retval.handlers = &mapi_profile_db_object_handlers;

	MAKE_STD_ZVAL(obj->children);
	array_init(obj->children);
	obj->talloc_ctx = talloc_named(NULL, 0, "profile_db");

	return retval;
}

void MAPIProfileDBRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIProfileDB", mapi_profile_db_class_functions);
	mapi_profile_db_ce = zend_register_internal_class(&ce TSRMLS_CC);
	mapi_profile_db_ce->create_object = mapi_profile_db_create_handler;
	memcpy(&mapi_profile_db_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	mapi_profile_db_object_handlers.clone_obj = NULL;
}

static struct mapi_profile *get_profile_ptr(TALLOC_CTX *talloc_ctx,
					    struct mapi_context *mapi_ctx,
					    char *opt_profname)
{
	enum MAPISTATUS		retval;
	const char		*err_str;
	char			*profname;
	struct mapi_profile	*profile;

	profile = talloc(talloc_ctx, struct mapi_profile);

	if (!opt_profname) {
		if ((retval = GetDefaultProfile(mapi_ctx, &profname)) != MAPI_E_SUCCESS) {
			talloc_free(talloc_ctx);
			err_str = mapi_get_errstr(retval);
			php_error(E_ERROR, "Get default profile: %s", err_str);
		}
	} else {
		profname = talloc_strdup(talloc_ctx, (const char *)opt_profname);
	}

	retval = OpenProfile(mapi_ctx, profile, profname, NULL);
	talloc_free(profname);

	if (retval == MAPI_E_NOT_FOUND) {
		talloc_free(talloc_ctx);
		return NULL;
	}
	if (retval && (retval != MAPI_E_INVALID_PARAMETER)) {
		talloc_free(talloc_ctx);
		err_str = mapi_get_errstr(retval);
		php_error(E_ERROR, "Get %s profile: %s", opt_profname, err_str);
	}

	return profile;
}


PHP_METHOD(MAPIProfileDB, __construct)
{
	char				*profdb_path;
	int				profdb_path_len;
	zval				*php_obj;
	mapi_profile_db_object_t	*obj;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",
				  &profdb_path, &profdb_path_len) == FAILURE) {
		RETURN_NULL();
	}

	php_obj = getThis();
	obj = (mapi_profile_db_object_t *) zend_object_store_get_object(php_obj TSRMLS_CC);
	obj->path = estrdup(profdb_path);

	// initialize and check mapi context
	struct mapi_context *ctx = mapi_profile_db_get_mapi_context(php_obj TSRMLS_CC);
	if (!ctx) {
		php_error(E_ERROR, "Cannot initialize mapi context");
	}
}

PHP_METHOD(MAPIProfileDB, __destruct)
{
	mapi_profile_db_object_t *obj = (mapi_profile_db_object_t*) zend_object_store_get_object(getThis() TSRMLS_CC);
	DESTROY_CHILDRENS(obj);
}


PHP_METHOD(MAPIProfileDB, path)
{
	zval				*php_obj;
	mapi_profile_db_object_t	*obj;

	php_obj = getThis();
	obj = (mapi_profile_db_object_t *) zend_object_store_get_object(php_obj TSRMLS_CC);
	RETURN_STRING(obj->path, 1);
}

PHP_METHOD(MAPIProfileDB, debug)
{
	zval			*this_php_obj;
	struct mapi_context	*mapi_ctx;
	long     		lvl = 0;
        bool                    enabled = false;

	this_php_obj = getThis();
	mapi_ctx = mapi_profile_db_get_mapi_context(this_php_obj TSRMLS_CC);
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "bl",
				  &enabled,  &lvl) == FAILURE) {
		RETURN_NULL();
	}

	SetMAPIDumpData(mapi_ctx, enabled);
	SetMAPIDebugLevel(mapi_ctx, lvl);

	RETURN_NULL();
}


struct mapi_context *mapi_context_init(char *profdb)
{
	struct mapi_context	*mapi_ctx;
	enum MAPISTATUS		retval;
	const char		*err_str;

	retval = MAPIInitialize(&mapi_ctx, profdb);
	if (retval != MAPI_E_SUCCESS) {
		err_str = mapi_get_errstr(retval);
		php_error(E_ERROR, "Intialize MAPI: %s", err_str);
	}

	return mapi_ctx;
}


struct mapi_context *mapi_profile_db_get_mapi_context(zval *mapi_profile_db TSRMLS_DC)
{
	mapi_profile_db_object_t	*obj;
	struct mapi_context		*new_ct;

	obj = (mapi_profile_db_object_t *) zend_object_store_get_object(mapi_profile_db TSRMLS_CC);
	if (obj->mapi_ctx != NULL) {
		return obj->mapi_ctx;
	}

	// create new mapi context
	new_ct  =  mapi_context_init(obj->path);
	obj->mapi_ctx = new_ct;
	return new_ct;
}

PHP_METHOD(MAPIProfileDB, profiles)
{
	struct mapi_context	*mapi_ctx;
	struct SRowSet		proftable;
	enum MAPISTATUS		retval;
	const char		*err_str;
	uint32_t		count;

	mapi_ctx = mapi_profile_db_get_mapi_context(getThis() TSRMLS_CC);
	memset(&proftable, 0, sizeof (struct SRowSet));

	if ((retval = GetProfileTable(mapi_ctx, &proftable)) != MAPI_E_SUCCESS) {
		err_str = mapi_get_errstr(retval);
		php_error(E_ERROR, "GetProfileTable: %s", err_str);
	}

	array_init(return_value);

	for (count = 0; count != proftable.cRows; count++) {
		char		*name = NULL;
		uint32_t	dflt = 0;
		zval		*profile;

		/* FIXME: Use accessors instead of direct access */
		name = (char *) proftable.aRow[count].lpProps[0].value.lpszA;
		dflt = proftable.aRow[count].lpProps[1].value.l;

		MAKE_STD_ZVAL(profile);
		array_init(profile);
		add_assoc_string(profile, "name", name, 1);
		add_assoc_bool(profile, "default", dflt);
		add_next_index_zval(return_value, profile);
	}
}

zval* mapi_profile_db_get_profile(zval *profile_db, char *profname TSRMLS_DC)
{
	zval				*new_profile;
	mapi_profile_db_object_t	*this_obj;
	struct mapi_profile		*profile;
	TALLOC_CTX			*mem_ctx;
	struct mapi_context		*mapi_ctx;

	mapi_ctx = mapi_profile_db_get_mapi_context(profile_db TSRMLS_CC);
	mem_ctx = talloc_named(NULL, 0, "profile");
	profile = get_profile_ptr(mem_ctx, mapi_ctx, profname);
	if (profile == NULL) {
		return NULL;
	}

	new_profile = create_profile_object(profile, profile_db, mem_ctx TSRMLS_CC);
	this_obj = (mapi_profile_db_object_t *) zend_object_store_get_object(profile_db TSRMLS_CC);
	ADD_CHILD(this_obj, new_profile);
	return new_profile;
}


static uint32_t nop_callback(struct SRowSet *rowset, void *private)
{
}

static bool create_mapi_profile(struct mapi_context *mapi_ctx,
			       const char *profdb, const char *profname,
			       const char *username,
			       const char *password, const char *address,
			       const char *language, const char *workstation,
			       const char *domain, const char *realm,
			       uint32_t flags, bool seal,
			       uint8_t exchange_version, const char *kerberos TSRMLS_DC)
{
	enum MAPISTATUS		retval;
	struct mapi_session	*session = NULL;
	TALLOC_CTX		*mem_ctx;
	struct mapi_profile	*profile;
	const char		*locale;
	uint32_t		cpid = 0;
	uint32_t		lcid = 0;
	char			*exchange_version_str;
	char			*cpid_str;
	char			*lcid_str;

	mem_ctx = talloc_named(mapi_ctx->mem_ctx, 0, "create_mapi_profile");
	profile = talloc(mem_ctx, struct mapi_profile);

	retval = CreateProfile(mapi_ctx, profname, username, password, flags);
	if (retval != MAPI_E_SUCCESS) {
		php_log_err("CreateProfile failed" TSRMLS_CC);
		talloc_free(mem_ctx);
		return false;
	}

	mapi_profile_add_string_attr(mapi_ctx, profname, "binding", address);
	mapi_profile_add_string_attr(mapi_ctx, profname, "workstation", workstation);
	mapi_profile_add_string_attr(mapi_ctx, profname, "domain", domain);
	mapi_profile_add_string_attr(mapi_ctx, profname, "seal", (seal == true) ? "true" : "false");

	exchange_version_str = talloc_asprintf(mem_ctx, "%d", exchange_version);

	mapi_profile_add_string_attr(mapi_ctx, profname, "exchange_version", exchange_version_str);
	talloc_free(exchange_version_str);

	if (realm) {
		mapi_profile_add_string_attr(mapi_ctx, profname, "realm", realm);
	}

	locale = (const char *) (language) ? mapi_get_locale_from_language(language) : mapi_get_system_locale();

	if (locale) {
		cpid = mapi_get_cpid_from_locale(locale);
		lcid = mapi_get_lcid_from_locale(locale);
	}

	if (!locale || !cpid || !lcid) {
		php_log_err("Invalid Language supplied or unknown system language. Deleting profile" TSRMLS_CC);
		if ((retval = DeleteProfile(mapi_ctx, profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("DeleteProfile", retval);
		}
		talloc_free(mem_ctx);
		return false;
	}

	cpid_str = talloc_asprintf(mem_ctx, "%d", cpid);
	lcid_str = talloc_asprintf(mem_ctx, "%d", lcid);

	mapi_profile_add_string_attr(mapi_ctx, profname, "codepage", cpid_str);
	mapi_profile_add_string_attr(mapi_ctx, profname, "language", lcid_str);
	mapi_profile_add_string_attr(mapi_ctx, profname, "method", lcid_str);

	talloc_free(cpid_str);
	talloc_free(lcid_str);

	if (kerberos) {
		mapi_profile_add_string_attr(mapi_ctx, profname, "kerberos", kerberos);
	}

	retval = MapiLogonProvider(mapi_ctx, &session, profname, password, PROVIDER_ID_NSPI);
	if (retval != MAPI_E_SUCCESS) {
		php_log_err("MAPILogon provider failed. Deleting profile" TSRMLS_CC);
		if ((retval = DeleteProfile(mapi_ctx, profname)) != MAPI_E_SUCCESS) {
			php_log_err("DeleteProfile" TSRMLS_CC);
		}
		talloc_free(mem_ctx);
		return false;
	}

	retval = ProcessNetworkProfile(session, username, (mapi_profile_callback_t) nop_callback, "Select a user id");
	if (retval != MAPI_E_SUCCESS && retval != 0x1) {
		php_log_err("ProcessNetworkProfile failed. Deleting profile" TSRMLS_CC);
		if ((retval = DeleteProfile(mapi_ctx, profname)) != MAPI_E_SUCCESS) {
			php_log_err("DeleteProfile" TSRMLS_CC);
		}
		talloc_free(mem_ctx);
		return false;
	}

	php_log_err("Profile  completed and added to database " TSRMLS_CC);

	talloc_free(mem_ctx);

	return true;
}



PHP_METHOD(MAPIProfileDB, getProfile)
{
	zval *new_profile;
	char *opt_profname = NULL;
	int  opt_profname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s",
				  &opt_profname, &opt_profname_len) == FAILURE) {
		php_error(E_ERROR, "getProfile invalid parameters (need profile name or none for default profile");
	}

	new_profile = mapi_profile_db_get_profile(getThis(), opt_profname TSRMLS_CC);
	if (new_profile == NULL) {
		RETURN_NULL();
	}


	RETURN_ZVAL(new_profile, 0, 1);
}

PHP_METHOD(MAPIProfileDB, createAndGetProfile)
{
	zval				*z_profile;
	struct mapi_context 		*mapi_ctx;
	zval				*this_php;
	mapi_profile_db_object_t	*this_obj;

	// method parameters
	char 	*opt_profname;
	int  	opt_profname_len;
	char    *opt_username;
	int     opt_username_len;
	char    *opt_password;
	int     opt_password_len;
	char    *opt_domain;
	int     opt_domain_len;
	char    *opt_realm;
	int     opt_realm_len;
	char   	*opt_address;
	int     opt_address_len;

	// new profile default values
	char 				*kerberos = NULL;
	uint8_t 			exchange_version = 0;
	char 				*language = NULL;
	char 				*workstation = "localhost";
	bool 				seal = false;
	uint32_t 			no_pass =0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssssss",
				  &opt_profname, &opt_profname_len,
				  &opt_username, &opt_username_len,
				  &opt_password, &opt_password_len,
				  &opt_domain, &opt_domain_len,
				  &opt_realm, &opt_realm_len,
				  &opt_address, &opt_address_len
		    ) == FAILURE) {
		php_error(E_ERROR, "createAndGetProfile invalid arguments. Must be: profile, user, passwd, domain, realm, address");
	}

	z_profile = mapi_profile_db_get_profile(getThis(), opt_profname TSRMLS_CC);
	if (z_profile != NULL) {
		RETURN_ZVAL(z_profile, 0, 1);
	}

	this_php = getThis();
	this_obj = (mapi_profile_db_object_t *) zend_object_store_get_object(this_php TSRMLS_CC);
	mapi_ctx = mapi_profile_db_get_mapi_context(this_php TSRMLS_CC);

	create_mapi_profile(mapi_ctx, this_obj->path, opt_profname,
			   opt_username, opt_password, opt_address,
			   language, workstation, opt_domain, opt_realm,
			   no_pass, seal, exchange_version, kerberos TSRMLS_CC);


	z_profile = mapi_profile_db_get_profile(getThis(), opt_profname TSRMLS_CC);
	if (z_profile != NULL) {
		RETURN_ZVAL(z_profile, 0, 1);
	} else {
		RETURN_NULL();
	}
}



