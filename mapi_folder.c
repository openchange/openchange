#include <php_mapi.h>

static zend_function_entry mapi_folder_class_functions[] = {
	PHP_ME(MAPIFolder,	__construct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIFolder,	__destruct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_ME(MAPIFolder,	getItemType,	NULL, ZEND_ACC_PUBLIC)

	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_folder_ce;
static zend_object_handlers	mapi_folder_object_handlers;

static void mapi_folder_free_storage(void *object TSRMLS_DC)
{
	mapi_folder_object_t	*obj;

	obj = (mapi_folder_object_t *) object;
	if (obj->talloc_ctx) {
		talloc_free(obj->talloc_ctx);
	}
	if (obj->item_type) {
		efree(obj->item_type);
	}

	zend_hash_destroy(obj->std.properties);
	FREE_HASHTABLE(obj->std.properties);


	efree(obj);
}


static zend_object_value mapi_folder_create_handler(zend_class_entry *type TSRMLS_DC)
{
	zval			*tmp;
	zend_object_value	retval;
	mapi_folder_object_t	*obj;

	obj = (mapi_folder_object_t *) emalloc(sizeof(mapi_folder_object_t));
	memset(obj, 0, sizeof(mapi_folder_object_t));

	obj->std.ce = type;

	ALLOC_HASHTABLE(obj->std.properties);
	zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
#if PHP_VERSION_ID < 50399
	zend_hash_copy(obj->std.properties, &type->default_properties,
		       (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
#else
	object_properties_init((zend_object *) &(obj->std), type);
#endif
	retval.handle = zend_objects_store_put(obj, NULL, mapi_folder_free_storage,
					       NULL TSRMLS_CC);
	retval.handlers = &mapi_folder_object_handlers;

	return retval;
}



void MAPIFolderRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIFolder", mapi_folder_class_functions);
	mapi_folder_ce = zend_register_internal_class(&ce TSRMLS_CC);
	mapi_folder_ce->create_object = mapi_folder_create_handler;
	memcpy(&mapi_folder_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	mapi_folder_object_handlers.clone_obj = NULL;
}

zval *create_folder_object(zval *php_mailbox, uint64_t id, char *item_type TSRMLS_DC)
{
	enum MAPISTATUS		retval;
	zval			*new_php_obj;
	mapi_mailbox_object_t	*mailbox;
	mapi_folder_object_t	*new_obj;
	zend_class_entry	**ce;

	MAKE_STD_ZVAL(new_php_obj);
	if (zend_hash_find(EG(class_table),"mapifolder", sizeof("mapifolder"),(void**)&ce) == FAILURE) {
		php_error(E_ERROR, "Class MAPIFolder does not exist.");
	}
	object_init_ex(new_php_obj, *ce);

	new_obj = (mapi_folder_object_t *) zend_object_store_get_object(new_php_obj TSRMLS_CC);
	new_obj->id = id;
	new_obj->parent_mailbox = php_mailbox;
	new_obj->talloc_ctx = talloc_named(NULL, 0, "folder");
	new_obj->item_type = estrdup(item_type);

	mapi_object_init(&(new_obj->store));
	mailbox = (mapi_mailbox_object_t *) zend_object_store_get_object(php_mailbox TSRMLS_CC);
	retval = OpenFolder(&(mailbox->store), id, &(new_obj->store));
	CHECK_MAPI_RETVAL(retval, "Open folder");

	return new_php_obj;
}

PHP_METHOD(MAPIFolder, __construct)
{
	php_error(E_ERROR, "The folder object should not created directly.\n" \
		  "Use the  methods in the mailbox object");
}


PHP_METHOD(MAPIFolder, __destruct)
{
	zval			*php_this_obj;
	mapi_folder_object_t	*this_obj;
	php_this_obj = getThis();
	this_obj = (mapi_folder_object_t *) zend_object_store_get_object(php_this_obj TSRMLS_CC);

	mapi_mailbox_remove_children_folder(this_obj->parent_mailbox, Z_OBJ_HANDLE_P(php_this_obj) TSRMLS_CC);
}

PHP_METHOD(MAPIFolder, getItemType)
{
	zval                    *this_php_obj;
	mapi_folder_object_t	*this_obj;

	this_php_obj = getThis();
	this_obj = (mapi_folder_object_t *) zend_object_store_get_object(this_php_obj TSRMLS_CC);
	RETURN_STRING(this_obj->item_type, 1);
}




