#ifndef PHP_MAPI_H
#define PHP_MAPI_H 1

#define PHP_MAPI_VERSION "1.0"
#define PHP_MAPI_EXTNAME "mapi"

PHP_FUNCTION(profiles);
PHP_FUNCTION(dump_profile);

extern zend_module_entry mapi_module_entry;
#define phpext_mapi_ptr &mapi_module_entry

#endif
