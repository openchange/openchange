#ifndef PHP_MAPI_H
#define PHP_MAPI_H 1

#define PHP_MAPI_VERSION "1.0"
#define PHP_MAPI_EXTNAME "mapi"
#define MAPI_CLASS_NAME "MAPI"

struct mapi_context* mapi_context_init(char *profdb);
void mapi_context_dtor(void *mapi_ctx);


PHP_MINIT_FUNCTION(mapi);
PHP_MSHUTDOWN_FUNCTION(mapi);

PHP_METHOD(MAPI, __construct);
PHP_METHOD(MAPI, __destruct);
PHP_METHOD(MAPI, profiles);
PHP_METHOD(MAPI, dump_profile);

extern zend_module_entry mapi_module_entry;
#define phpext_mapi_ptr &mapi_module_entry

#define EXPECTED_MAPI_OBJECTS 32

#endif
