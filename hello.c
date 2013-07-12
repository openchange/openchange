#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_hello.h"

// from openchange
#include "utils/mapitest/mapitest.h"
#include "utils/openchange-tools.h"

static zend_function_entry hello_functions[] = {
    PHP_FE(hello_world, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry hello_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_HELLO_WORLD_EXTNAME,
    hello_functions,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_HELLO_WORLD_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_HELLO
ZEND_GET_MODULE(hello)
#endif

PHP_FUNCTION(hello_world)
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

    RETURN_STRING("Hello World", 1);
}
