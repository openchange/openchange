/*
   OpenChange MAPI PHP bindings

   Copyright (C) 2013 Zentyal S.L.

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


/* static void init_message_store(mapi_object_t *store, */
/* 			       struct mapi_session *session, */
/* 			       bool public_folder, char *username) */
/* { */
/* 	enum MAPISTATUS retval; */

/* 	mapi_object_init(store); */
/* 	if (public_folder == true) { */
/* 		retval = OpenPublicFolder(session, store); */
/* 		if (retval != MAPI_E_SUCCESS) { */
/* 			php_error(E_ERROR, "Open public folder: %s", mapi_get_errstr(retval)); */
/* 		} */
/* 	} else if (username) { */
/* 		retval = OpenUserMailbox(session, username, store); */
/* 		if (retval != MAPI_E_SUCCESS) { */
/* 			php_error(E_ERROR, "Open user mailbox: %s", mapi_get_errstr(retval)); */
/* 		} */
/* 	} else { */
/* 		retval = OpenMsgStore(session, store); */
/* 		if (retval != MAPI_E_SUCCESS) { */
/* 			php_error(E_ERROR, "Open message store: %s",  mapi_get_errstr(retval)); */
/* 		} */
/* 	} */
/* } */


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



/* static zval *get_child_folders(TALLOC_CTX *talloc_ctx, */
/* 			       mapi_object_t *parent, */
/* 			       mapi_id_t folder_id, int count) */
/* { */
/* 	enum MAPISTATUS		retval; */
/* 	bool			ret; */
/* 	mapi_object_t		obj_folder; */
/* 	mapi_object_t		obj_htable; */
/* 	struct SPropTagArray	*SPropTagArray; */
/* 	struct SRowSet		rowset; */
/* 	const char		*name; */
/* 	const char		*comment; */
/* 	const uint32_t		*total; */
/* 	const uint32_t		*unread; */
/* 	const uint32_t		*child; */
/* 	uint32_t		index; */
/* 	const uint64_t		*fid; */
/* 	int			i; */
/* 	zval			*folders; */

/* 	MAKE_STD_ZVAL(folders); */
/* 	array_init(folders); */

/* 	mapi_object_init(&obj_folder); */
/* 	retval = OpenFolder(parent, folder_id, &obj_folder); */
/* 	if (retval != MAPI_E_SUCCESS) return folders; */

/* 	mapi_object_init(&obj_htable); */
/* 	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, NULL); */
/* 	if (retval != MAPI_E_SUCCESS) return folders; */

/* 	SPropTagArray = set_SPropTagArray(talloc_ctx, 0x6, */
/* 					  PR_DISPLAY_NAME_UNICODE, */
/* 					  PR_FID, */
/* 					  PR_COMMENT_UNICODE, */
/* 					  PR_CONTENT_UNREAD, */
/* 					  PR_CONTENT_COUNT, */
/* 					  PR_FOLDER_CHILD_COUNT); */
/* 	retval = SetColumns(&obj_htable, SPropTagArray); */
/* 	MAPIFreeBuffer(SPropTagArray); */
/* 	if (retval != MAPI_E_SUCCESS) return folders; */


/* 	while (((retval = QueryRows(&obj_htable, 0x32, */
/* 				    TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND) && rowset.cRows) { */
/* 		for (index = 0; index < rowset.cRows; index++) { */
/* 			zval	*current_folder; */

/* 			MAKE_STD_ZVAL(current_folder); */
/* 			array_init(current_folder); */


/* 			long n_total, n_unread; */

/* 			fid = (const uint64_t *)find_SPropValue_data(&rowset.aRow[index], PR_FID); */
/* 			name = (const char *)find_SPropValue_data(&rowset.aRow[index], */
/* 								  PR_DISPLAY_NAME_UNICODE); */
/* 			comment = (const char *)find_SPropValue_data(&rowset.aRow[index], */
/* 								     PR_COMMENT_UNICODE); */
/* 			if (comment == NULL) comment = ""; */

/*       /\*** XXX temporary workaround for child folders problem ***\/ */
/* 			{ */
/* 				mapi_object_t obj_child; */
/* 				mapi_object_t obj_ctable; */
/* 				uint32_t      count = 0; */
/* 				uint32_t      count2 = 0; */

/* 				mapi_object_init(&obj_child); */
/* 				mapi_object_init(&obj_ctable); */

/* 				retval = OpenFolder(&obj_folder, *fid, &obj_child); */

/* 				if (retval != MAPI_E_SUCCESS) return false; */
/* 				retval = GetContentsTable(&obj_child, &obj_ctable, 0, &count); */
/* 				total = &count; */

/* 				mapi_object_release(&obj_ctable); */
/* 				mapi_object_init(&obj_ctable); */

/* 				retval = GetHierarchyTable(&obj_child, &obj_ctable, 0, &count2); */
/* 				child = &count2; */

/* 				mapi_object_release(&obj_ctable); */
/* 				mapi_object_release(&obj_child); */
/* 			} */

/* 			// XXX commented by workaround of child problem */
/* 			/\* total = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_CONTENT_COUNT); *\/ */
/* 			if (!total) { */
/* 				n_total = 0; */
/* 			} else { */
/* 				n_total = (long) *total; */
/* 			} */

/* 			unread = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_CONTENT_UNREAD); */
/* 			if (!unread) { */
/* 				n_unread =0; */
/* 			} else { */
/* 				n_unread =  (long) *unread; */
/* 			} */

/* 			// XXX commented by workaround of child problems */
/* 			//      child = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_FOLDER_CHILD_COUNT); */

/* 			const char *container = get_container_class(talloc_ctx, parent, *fid); */

/* 			add_assoc_string(current_folder, "name",(char*)  name, 1); */
/* 			add_assoc_mapi_id_t(current_folder, "fid", *fid); */
/* 			add_assoc_string(current_folder, "comment", (char*) comment, 1); */
/* 			add_assoc_string(current_folder, "container", (char*) container, 1); */

/* 			add_assoc_long(current_folder, "total", n_total); */
/* 			add_assoc_long(current_folder, "uread", n_unread); */

/* 			if (child && *child) { */
/* 				zval *child_folders  = get_child_folders(talloc_ctx, &obj_folder, *fid, count + 1); */
/* 				add_assoc_zval(current_folder, "childs", child_folders); */
/* 			} */

/* 			add_next_index_zval(folders, current_folder); */
/* 		} */
/* 	} */

/* 	mapi_object_release(&obj_htable); */
/* 	mapi_object_release(&obj_folder); */

/* 	return folders; */
/* } */


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

// TO RMEOVWE
/* PHP_METHOD(MAPISession, folders) */
/* { */
/* 	enum MAPISTATUS		retval; */
/* 	zval			*this_obj; */
/* 	TALLOC_CTX		*this_talloc_ctx; */
/* 	TALLOC_CTX		*talloc_ctx; */
/* 	struct mapi_session	*session = NULL; */
/* 	struct mapi_profile	*profile; */
/* 	mapi_object_t		obj_store; */
/* 	mapi_id_t		id_mailbox; */
/* 	struct SPropTagArray	*SPropTagArray; */
/* 	struct SPropValue	*lpProps; */
/* 	uint32_t		cValues; */
/* 	const char		*mailbox_name; */


/* 	this_obj = getThis(); */
/* 	profile = session_get_profile(this_obj TSRMLS_CC); */
/* 	session = mapi_session_get_session(this_obj TSRMLS_CC); */

/* 	// open user mailbox */
/* 	init_message_store(&obj_store, session, false, (char *) profile->username); */
/* 	this_talloc_ctx = OBJ_GET_TALLOC_CTX(mapi_session_object_t *, this_obj); */
/* 	talloc_ctx = talloc_named(this_talloc_ctx, 0, "MAPISession::folders"); */

/* 	/\* Retrieve the mailbox folder name *\/ */
/* 	SPropTagArray = set_SPropTagArray(talloc_ctx, 0x1, PR_DISPLAY_NAME_UNICODE); */
/* 	retval = GetProps(&obj_store, MAPI_UNICODE, SPropTagArray, &lpProps, &cValues); */
/* 	MAPIFreeBuffer(SPropTagArray); */
/* 	if (retval != MAPI_E_SUCCESS) { */
/* 		php_error(E_ERROR, "Get mailbox properties: %s", mapi_get_errstr(retval)); */
/* 	} */

/* 	/\* FIXME: Use accessor instead of direct access *\/ */
/* 	if (lpProps[0].value.lpszW) { */
/* 		mailbox_name = lpProps[0].value.lpszW; */
/* 	} else { */
/* 		php_error(E_ERROR, "Get mailbox name"); */
/* 	} */

/* 	/\* Prepare the directory listing *\/ */
/* 	retval = GetDefaultFolder(&obj_store, &id_mailbox, olFolderTopInformationStore); */
/* 	if (retval != MAPI_E_SUCCESS) { */
/* 		php_error(E_ERROR, "Get default folder: %s", mapi_get_errstr(retval)); */
/* 	} */

/* 	array_init(return_value); */
/* 	add_assoc_string(return_value, "name", (char*) mailbox_name, 1); */
/* 	add_assoc_zval(return_value, "childs", get_child_folders(talloc_ctx, &obj_store, id_mailbox, 0)); */

/* 	talloc_free(talloc_ctx); */
/* } */


