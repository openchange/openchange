#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"

// from openchange
#include "utils/mapitest/mapitest.h"
#include "utils/openchange-tools.h"

static zend_function_entry mapi_functions[] = {
    PHP_FE(hello_mapi, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry mapi_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_MAPI_EXTNAME,
    mapi_functions,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_MAPI_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_MAPI
ZEND_GET_MODULE(mapi)
#endif

PHP_FUNCTION(hello_mapi)
{
	enum MAPISTATUS		retval;
  TALLOC_CTX		*mem_ctx;
  struct mapitest		mt;
  const char		*opt_profdb = NULL;
  /* Initialize MAPI subsystem */
  if (!opt_profdb) {
    opt_profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
  }

        retval = MAPIInitialize(&(mt.mapi_ctx), opt_profdb);
        if (retval != MAPI_E_SUCCESS) {
          mapi_errstr("MAPIInitialize", retval);
          RETURN_STRING("MAPI FAILED", 1);
        } else {
          RETURN_STRING("JELLO MAPI", 1);
        }

    RETURN_STRING("HELLO MPAI", 1);
}
