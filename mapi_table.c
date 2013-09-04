#include <php_mapi.h>

static zend_function_entry mapi_table_class_functions[] = {
 	PHP_ME(MAPITable,	__construct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
 	PHP_ME(MAPITable,	count,		NULL, ZEND_ACC_PUBLIC)
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
	if (obj->talloc_ctx) {
		talloc_free(obj->talloc_ctx);
	}

	Z_DELREF_P(obj->parent);

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

void MAPITableRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPITable", mapi_table_class_functions);
	mapi_table_ce = zend_register_internal_class(&ce TSRMLS_CC);
	mapi_table_ce->create_object = mapi_table_create_handler;
	memcpy(&mapi_table_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	mapi_table_object_handlers.clone_obj = NULL;
}

zval *create_table_object(char *class, zval* folder_php_obj, mapi_object_t *table, uint32_t count TSRMLS_DC)
{
	zval *new_php_obj;
	mapi_table_object_t *new_obj;
	zend_class_entry	**ce;
	struct SPropTagArray	*SPropTagArray;
	enum MAPISTATUS		retval;

	MAKE_STD_ZVAL(new_php_obj);
	/* create the MapiTable instance in return_value zval */
	if (zend_hash_find(EG(class_table), class, strlen(class)+1, (void**)&ce) == FAILURE) {
		php_error(E_ERROR, "create_table_object: class '%s' does not exist.", class);
	}
	object_init_ex(new_php_obj, *ce);

	new_obj = (mapi_table_object_t *) zend_object_store_get_object(new_php_obj TSRMLS_CC);
	new_obj->parent = folder_php_obj;
	Z_ADDREF_P(new_obj->parent);

	mapi_folder_object_t *folder_obj = STORE_OBJECT(mapi_folder_object_t*, folder_php_obj);
	mapi_object_t* folder =  &(folder_obj->store);
	new_obj->folder = folder;
	new_obj->talloc_ctx = talloc_named(NULL, 0, "table=%p", new_obj);

	SPropTagArray = set_SPropTagArray(new_obj->talloc_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT_UNICODE);
	new_obj->count = count;
	new_obj->table = table;

	retval = SetColumns(new_obj->table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	CHECK_MAPI_RETVAL(retval, "Create table objects");

	return new_php_obj;
}

struct SRowSet* next_row_set(mapi_object_t* table, struct SRowSet *row_set, uint32_t count TSRMLS_DC)
{
	mapi_table_object_t	*store_obj;
	enum MAPISTATUS		retval;

	if (count == 0) {
		php_error(E_ERROR, "Bad count parameter, msut be greater than zero");
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


/* PHP_METHOD(MAPITable, __destruct) */
/* { */
/* 	/\* zval			*php_this_obj; *\/ */
/* 	/\* mapi_table_object_t	*this_obj; *\/ */
/* 	/\* php_this_obj = getThis(); *\/ */
/* 	/\* this_obj = (mapi_table_object_t *) zend_object_store_get_object(php_this_obj TSRMLS_CC); *\/ */

/* } */


PHP_METHOD(MAPITable, count)
{
	mapi_table_object_t *this_obj = THIS_STORE_OBJECT(mapi_table_object_t*);
	RETURN_LONG(this_obj->count);
}
