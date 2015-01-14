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

#include <php_mapi.h>

static zend_function_entry mapi_table_class_functions[] = {
 	PHP_ME(MAPITable,	__construct,	 NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
 	PHP_ME(MAPITable,	__destruct,	 NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
 	PHP_ME(MAPITable,	count,		 NULL, ZEND_ACC_PUBLIC)
 	PHP_ME(MAPITable,	getParentFolder, NULL, ZEND_ACC_PUBLIC)
 	PHP_ME(MAPITable,	getParent,       NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_table_ce;
static zend_object_handlers	mapi_table_object_handlers;

void mapi_table_free_storage(void *object TSRMLS_DC)
{
	mapi_table_object_t	*obj;
	obj = (mapi_table_object_t *) object;
	if (obj->table) {
		mapi_object_release(obj->table);
		efree(obj->table);
	}
	if (obj->tag_array) {
		enum MAPISTATUS		retval;
		retval = MAPIFreeBuffer(obj->tag_array);
		CHECK_MAPI_RETVAL(retval, "Freeing table tag_array");
	}
	if (obj->talloc_ctx) {
		talloc_free(obj->talloc_ctx);
	}

	zend_object_std_dtor(&(obj->std) TSRMLS_CC);
	efree(obj);
}

zend_object_value mapi_table_create_handler(zend_class_entry *type TSRMLS_DC)
{
	zval			*tmp;
	zend_object_value	retval;
	mapi_table_object_t	*obj;

	obj = (mapi_table_object_t *) emalloc(sizeof(mapi_table_object_t));
	memset(obj, 0, sizeof(mapi_table_object_t));

	obj->std.ce = type;

	ALLOC_HASHTABLE(obj->std.properties);
	zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
#if PHP_VERSION_ID < 50399
	zend_hash_copy(obj->std.properties, &type->default_properties,
		       (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
#else
	object_properties_init((zend_object *) &(obj->std), type);
#endif
	retval.handle = zend_objects_store_put(obj, NULL, mapi_table_free_storage,
					       NULL TSRMLS_CC);
	retval.handlers = &mapi_table_object_handlers;

	return retval;
}

void MAPITableClassSetObjectHandlers(zend_object_handlers *handlers)
{
	handlers->clone_obj = NULL;
}


void MAPITableRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPITable", mapi_table_class_functions);
	mapi_table_ce = zend_register_internal_class(&ce TSRMLS_CC);
	mapi_table_ce->create_object = mapi_table_create_handler;
	memcpy(&mapi_table_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	MAPITableClassSetObjectHandlers(&mapi_table_object_handlers);
}



zval *create_table_object(char *class, zval* folder_php_obj, mapi_object_t *table, TALLOC_CTX *talloc_ctx, struct SPropTagArray *tag_array, uint count TSRMLS_DC)
{
	zval *new_php_obj;
	mapi_table_object_t *new_obj;
	zend_class_entry	**ce;
	enum MAPISTATUS		retval;

	MAKE_STD_ZVAL(new_php_obj);
	/* create the MapiTable instance in return_value zval */
	if (zend_hash_find(EG(class_table), class, strlen(class)+1, (void**)&ce) == FAILURE) {
		php_error(E_ERROR, "create_table_object: class '%s' does not exist.", class);
	}
	object_init_ex(new_php_obj, *ce);

	new_obj = (mapi_table_object_t *) zend_object_store_get_object(new_php_obj TSRMLS_CC);
	new_obj->parent = folder_php_obj;
	new_obj->talloc_ctx = talloc_ctx;

	new_obj->count = count;
	new_obj->table = table;
	new_obj->tag_array = tag_array;

	retval = SetColumns(new_obj->table, tag_array);
	CHECK_MAPI_RETVAL(retval, "Create table objects");

	return new_php_obj;
}

struct SRowSet* mapi_table_next_row_set(mapi_object_t* table, struct SRowSet *row_set, uint32_t count TSRMLS_DC)
{
	enum MAPISTATUS		retval;
	if (count == 0) {
		php_error(E_ERROR, "Bad count parameter, must be greater than zero");
	}

	retval = QueryRows(table, count, TBL_ADVANCE, row_set);
	if ((retval !=  MAPI_E_NOT_FOUND) && (row_set->cRows == 0)) {
		return NULL;
	}
	CHECK_MAPI_RETVAL(retval, "Next row set");

	return row_set;
}


PHP_METHOD(MAPITable, __construct)
{
	php_error(E_ERROR, "The base table object should not be created directly.");
}


PHP_METHOD(MAPITable, __destruct)
{
	zval		               	*this_zval;
	mapi_table_object_t	*this_obj;
	this_zval = getThis();
	this_obj = (mapi_table_object_t *) zend_object_store_get_object(this_zval TSRMLS_CC);
	mapi_folder_object_t* obj_parent =  (mapi_folder_object_t*) zend_object_store_get_object(this_obj->parent TSRMLS_CC);
	REMOVE_CHILD(obj_parent, this_zval);
}

PHP_METHOD(MAPITable, count)
{
	mapi_table_object_t *this_obj = THIS_STORE_OBJECT(mapi_table_object_t*);
	RETURN_LONG(this_obj->count);
}

// XXX to be deprecated , sue getParent instead
PHP_METHOD(MAPITable, getParentFolder)
{
	mapi_table_object_t *this_obj = THIS_STORE_OBJECT(mapi_table_object_t*);
	RETURN_ZVAL(this_obj->parent, 0, 0);
}

PHP_METHOD(MAPITable, getParent)
{
	mapi_table_object_t *this_obj = THIS_STORE_OBJECT(mapi_table_object_t*);
	RETURN_ZVAL(this_obj->parent, 0, 0);
}
