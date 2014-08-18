/*
   OpenChange MAPI PHP bindings

   Copyright (C) 2013 Javier Amor Garcia

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

#include <inttypes.h>
#include "php_mapi.h"

static zend_function_entry mapi_session_class_functions[] = {
	PHP_ME(MAPISession,	__construct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPISession,	__destruct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_ME(MAPISession,	mailbox,	NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_session_ce;
static zend_object_handlers	mapi_session_object_handlers;

static void mapi_session_free_storage(void *object TSRMLS_DC)
{
	mapi_session_object_t	*obj;

	obj = (mapi_session_object_t *) object;
	if (obj->talloc_ctx) {
		talloc_free(obj->talloc_ctx);
	}

	zend_object_std_dtor(&(obj->std) TSRMLS_CC);
	efree(obj);
}


static zend_object_value mapi_session_create_handler(zend_class_entry *type TSRMLS_DC)
{
	zval			*tmp;
	zend_object_value	retval;
	mapi_session_object_t	*obj;

	obj = (mapi_session_object_t *) emalloc(sizeof(mapi_session_object_t));
	memset(obj, 0, sizeof(mapi_session_object_t));

	obj->std.ce = type;

	ALLOC_HASHTABLE(obj->std.properties);
	zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
#if PHP_VERSION_ID < 50399
	zend_hash_copy(obj->std.properties, &type->default_properties,
		       (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
#else
	object_properties_init((zend_object *) &(obj->std), type);
#endif
	retval.handle = zend_objects_store_put(obj, NULL, mapi_session_free_storage,
					       NULL TSRMLS_CC);
	retval.handlers = &mapi_session_object_handlers;

	MAKE_STD_ZVAL(obj->children);
	array_init(obj->children);

	return retval;
}

void MAPISessionRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPISession", mapi_session_class_functions);
	mapi_session_ce = zend_register_internal_class(&ce TSRMLS_CC);
	mapi_session_ce->create_object = mapi_session_create_handler;
	memcpy(&mapi_session_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	mapi_session_object_handlers.clone_obj = NULL;
}


zval *create_session_object(struct mapi_session *session,
			    zval *profile, TALLOC_CTX *talloc_ctx TSRMLS_DC)
{
	zval			*php_obj;
	zend_class_entry	**ce;
	mapi_session_object_t	*obj;

	MAKE_STD_ZVAL(php_obj);

	/* create the MapiProfile instance in return_value zval */
	if (zend_hash_find(EG(class_table),"mapisession", sizeof("mapisession"),(void**)&ce) == FAILURE) {
		php_error(E_ERROR,"Class MAPISession does not exist.");
	}

	object_init_ex(php_obj, *ce);

	obj = (mapi_session_object_t *) zend_object_store_get_object(php_obj TSRMLS_CC);
	obj->session = session;
	obj->parent = profile;
	obj->talloc_ctx = talloc_ctx;

	return php_obj;
}


struct mapi_session *mapi_session_get_session(zval *php_obj TSRMLS_DC)
{
	mapi_session_object_t *obj;

	obj = (mapi_session_object_t *) zend_object_store_get_object(php_obj TSRMLS_CC);
	return obj->session;
}

struct mapi_profile *session_get_profile(zval *php_obj TSRMLS_DC)
{
	mapi_session_object_t	*obj ;

	obj = (mapi_session_object_t*) zend_object_store_get_object(php_obj TSRMLS_CC);
	return mapi_profile_get_profile(obj->parent TSRMLS_CC);
}


PHP_METHOD(MAPISession, __construct)
{
	php_error(E_ERROR, "The session object should not created directly.\n" \
		  "Use the 'logon' method in the profile object");
}

PHP_METHOD(MAPISession, __destruct)
{
	zval *this_zval = getThis();
	mapi_session_object_t *obj = (mapi_session_object_t*) zend_object_store_get_object(this_zval TSRMLS_CC);
	DESTROY_CHILDRENS(obj);
	mapi_profile_object_t* obj_parent =  (mapi_profile_object_t*) zend_object_store_get_object(obj->parent TSRMLS_CC);
	REMOVE_CHILD(obj_parent, this_zval);
}

PHP_METHOD(MAPISession, mailbox)
{
	char			*username = NULL;
	int			username_len;
	zval                    *new_mailbox;
        struct mapi_profile	*profile;
	TALLOC_CTX		*talloc_ctx;
	mapi_session_object_t   *this_obj;
	zval			*this_zval;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s",
				  &username, &username_len) == FAILURE) {
		RETURN_NULL();
	}

	this_zval = getThis();

        if (username == NULL) {
		struct mapi_profile *profile = session_get_profile(this_zval TSRMLS_CC);
                username  = (char *) profile->username;
        }

	new_mailbox = create_mailbox_object(this_zval, username TSRMLS_CC);

	this_obj = STORE_OBJECT(mapi_session_object_t*, this_zval);
	ADD_CHILD(this_obj, new_mailbox);

	RETURN_ZVAL(new_mailbox, 0, 1);
}
