#include <php_mapi.h>

static zend_function_entry mapi_appointment_class_functions[] = {
	PHP_ME(MAPIAppointment,	__construct,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIAppointment,	__destruct,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)

	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_appointment_ce;
static zend_object_handlers	mapi_appointment_object_handlers;

void MAPIAppointmentRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIAppointment", mapi_appointment_class_functions);
	mapi_appointment_ce = zend_register_internal_class_ex(&ce, NULL, "mapimessage" TSRMLS_CC);

	mapi_appointment_ce->create_object = mapi_message_create_handler;

	memcpy(&mapi_appointment_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	mapi_appointment_object_handlers.clone_obj = NULL;
}

zval *create_appointment_object(zval *folder, mapi_object_t  *message TSRMLS_DC)
{
	return create_message_object("mapiappointment", folder, message TSRMLS_CC);
}

PHP_METHOD(MAPIAppointment, __construct)
{
	php_error(E_ERROR, "The appointment object should not created directly.");
}


PHP_METHOD(MAPIAppointment, __destruct)
{
	/* zval			*php_this_obj; */
	/* mapi_appointment_object_t	*this_obj; */
	/* php_this_obj = getThis(); */
	/* this_obj = (mapi_appointment_object_t *) zend_object_store_get_object(php_this_obj TSRMLS_CC); */

}
