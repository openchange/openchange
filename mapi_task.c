#include <php_mapi.h>

static zend_function_entry mapi_task_class_functions[] = {
	PHP_ME(MAPITask,	__construct,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPITask,	__destruct,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)

	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_task_ce;
static zend_object_handlers	mapi_task_object_handlers;

void MAPITaskRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPITask", mapi_task_class_functions);
	mapi_task_ce = zend_register_internal_class_ex(&ce, NULL, "mapimessage" TSRMLS_CC);

	mapi_task_ce->create_object = mapi_message_create_handler;

	memcpy(&mapi_task_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	mapi_task_object_handlers.clone_obj = NULL;
}

zval *create_task_object(mapi_object_t  *message TSRMLS_DC)
{
	return create_message_object("mapitask", message TSRMLS_CC);
}

PHP_METHOD(MAPITask, __construct)
{
	php_error(E_ERROR, "The task object should not created directly.");
}


PHP_METHOD(MAPITask, __destruct)
{
	/* zval			*php_this_obj; */
	/* mapi_task_object_t	*this_obj; */
	/* php_this_obj = getThis(); */
	/* this_obj = (mapi_task_object_t *) zend_object_store_get_object(php_this_obj TSRMLS_CC); */

}
