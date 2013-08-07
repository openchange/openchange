#ifndef PHP_MAPI_H
#define PHP_MAPI_H 1

// from openchange
#include "utils/mapitest/mapitest.h"
#include "utils/openchange-tools.h"
#include "libmapi/mapidefs.h"
//#include "libmapi/libmapi_private.h"
//#include "libmapi/mapi_nameid.h"
#include "talloc.h"

#define PHP_MAPI_VERSION "1.0"
#define PHP_MAPI_EXTNAME "mapi"

#define PROFILE_RESOURCE_NAME "Profile"
#define SESSION_RESOURCE_NAME "Session"
#define TALLOC_RESOURCE_NAME "TALLOC_CTX"

PHP_MINIT_FUNCTION(mapi);
PHP_MSHUTDOWN_FUNCTION(mapi);

static struct mapi_profile* get_profile_ptr(TALLOC_CTX* mem_ctx,  struct mapi_context* mapi_ctx, char* opt_profname);

extern zend_module_entry mapi_module_entry;
#define phpext_mapi_ptr &mapi_module_entry

#define EXPECTED_MAPI_OBJECTS 32
#define OBJ_GET_TALLOC_CTX(objType, obj) ((objType) zend_object_store_get_object(obj TSRMLS_CC))->talloc_ctx;

#define add_assoc_mapi_id_t(zv, name, value) add_assoc_long(zv, name, (long) value)
#define CHECK_MAPI_RETVAL(rv, desc) if (rv != MAPI_E_SUCCESS) php_error(E_ERROR, "%s: %s", desc, mapi_get_errstr(rv))


#endif
