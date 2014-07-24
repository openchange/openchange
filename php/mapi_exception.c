#include <php_mapi.h>

static zend_class_entry *mapi_exception_ce;

void MAPIExceptionRegisterClass(TSRMLS_D) {
	zend_class_entry ex_entry;
	INIT_CLASS_ENTRY(ex_entry, "MAPIException", NULL);
	mapi_exception_ce = zend_register_internal_class_ex(&ex_entry, NULL, "exception" TSRMLS_CC);
}

zend_class_entry *mapi_exception_get_default(){
	return mapi_exception_ce;
}
