/*
   OpenChange MAPI PHP bindings

   Copyright (C) Zentyal SL. 2013.

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

	{ NULL, NULL, NULL }
};


static zend_class_entry		*mapi_profile_db_ce;
static zend_object_handlers	mapi_profile_db_object_handlers;

static void  mapi_profile_db_add_ref(zval *object TSRMLS_DC)
{
	php_printf("profile_db add ref count: %i -> %i\n", Z_REFCOUNT_P(object),  Z_REFCOUNT_P(object) + 1);
//	Z_ADDREF_P(object);
}

static void mapi_profile_db_del_ref(zval *object TSRMLS_DC)
{
	if (Z_REFCOUNT_P(object) == 0) return;
	php_printf("profile_db del ref count: %i => %i\n", Z_REFCOUNT_P(object),  Z_REFCOUNT_P(object) - 1);

	Z_DELREF_P(object);
}

static void mapi_profile_db_free_storage(void *object TSRMLS_DC)
{
	php_printf("profile_db free\n");

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

	return retval;
}

void MAPIProfileDBRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIProfileDB", mapi_profile_db_class_functions);
	mapi_profile_db_ce = zend_register_internal_class(&ce TSRMLS_CC);
	mapi_profile_db_ce->create_object = mapi_profile_db_create_handler;
	memcpy(&mapi_profile_db_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

//	mapi_profile_db_object_handlers.add_ref = mapi_profile_db_add_ref;
//	mapi_profile_db_object_handlers.del_ref = mapi_profile_db_del_ref;
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
	obj->talloc_ctx = talloc_named(NULL, 0, "profile_db");
}

PHP_METHOD(MAPIProfileDB, __destruct)
{
	php_printf("ProfileDB Destruct\n\n");
        php_printf("Referecne count: %u\n", Z_REFCOUNT_P(getThis()));

//	mapi_profileDB_object_t *obj = THIS_STORE_OBJECT(mapi_profile_db_object_t*);
//	S_PARENT_DELREF_P(obj);
	php_printf("END ProfileDB Destruct\n\n");
//	Z_DTOR_GUARD_P(getThis(), "MAPIProfileDB object");
}


PHP_METHOD(MAPIProfileDB, path)
{
	zval				*php_obj;
	mapi_profile_db_object_t	*obj;

	php_obj = getThis();
	obj = (mapi_profile_db_object_t *) zend_object_store_get_object(php_obj TSRMLS_CC);
	RETURN_STRING(obj->path, 1);
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

PHP_METHOD(MAPIProfileDB, getProfile)
{
	zval				*this_php_obj;
	zval				*php_obj;
	mapi_profile_db_object_t	*this_obj;
	struct mapi_profile		*profile;
	TALLOC_CTX			*talloc_ctx;
	struct mapi_context		*mapi_ctx;
	char				*opt_profname = NULL;
	int				opt_profname_len;

	this_php_obj= getThis();
	mapi_ctx = mapi_profile_db_get_mapi_context(this_php_obj TSRMLS_CC);

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s",
				  &opt_profname, &opt_profname_len) == FAILURE) {
		RETURN_NULL();
	}

	talloc_ctx = talloc_named(NULL, 0, "profile");
	profile = get_profile_ptr(talloc_ctx, mapi_ctx, opt_profname);

	php_obj = create_profile_object(profile, this_php_obj, talloc_ctx TSRMLS_CC);
	this_obj = (mapi_profile_db_object_t *) zend_object_store_get_object(this_php_obj TSRMLS_CC);
	RETURN_ZVAL(php_obj, 0, 1);
}


