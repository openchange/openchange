#include <php_mapi.h>

static zend_class_entry *mapi_exception_ce;

void MAPIExceptionRegisterClasss(TSRMLS_D) {
	zend_class_entry e;

	INIT_CLASS_ENTRY(e, "MAPIException", NULL);
	mapi_exception_ce = zend_register_internal_class_ex(&e, (zend_class_entry*)zend_exception_get_default(TSRMLS_C), NULL TSRMLS_CC);
}

zend_class_entry *mapi_exception_get_default(){
	return mapi_exception_ce;
}
