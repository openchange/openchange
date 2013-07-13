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
    PHP_FE(print_profiles, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry mapi_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_MAPI_EXTNAME,
    mapi_functions,
    NULL,//    PHP_MINIT(mapi),
    NULL, //PHP_MSHUTDOWN(mapi),
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
/*
PHP_MINIT(mapi)
{
  int i = 0;

}

PHP_MSHUTDOWN(mapi)
{
  int i = 0;
}
*/

static char * mapiprofile_list(struct mapi_context *mapi_ctx, const char *profdb)
{
	enum MAPISTATUS		retval;
	struct SRowSet		proftable;
	uint32_t		count = 0;

	memset(&proftable, 0, sizeof (struct SRowSet));
	if ((retval = GetProfileTable(mapi_ctx, &proftable)) != MAPI_E_SUCCESS) {
		mapi_errstr("GetProfileTable", retval);
		exit (1);
	}


	printf("We have %u profiles in the database:\n", proftable.cRows);



	for (count = 0; count != proftable.cRows; count++) {
		const char	*name = NULL;
		uint32_t	dflt = 0;

		name = proftable.aRow[count].lpProps[0].value.lpszA;
		dflt = proftable.aRow[count].lpProps[1].value.l;

		if (dflt) {
			printf("\tProfile = %s [default]\n", name);
		} else {
			printf("\tProfile = %s\n", name);
		}

	}
}




PHP_FUNCTION(print_profiles)
{
  struct mapi_context	*mapi_ctx;	/*!< mapi context */
  char *profdb = "/home/jag/.openchange/profiles.ldb";
  enum MAPISTATUS retval;
  retval = MAPIInitialize(&mapi_ctx, profdb);
  if (retval != MAPI_E_SUCCESS) {
    mapi_errstr("MAPIInitialize", retval);
    RETURN_STRING("MAPI FAILED XXX", 1);
  }

  struct SRowSet proftable;
  uint32_t count = 0;

  memset(&proftable, 0, sizeof (struct SRowSet));

	if ((retval = GetProfileTable(mapi_ctx, &proftable)) != MAPI_E_SUCCESS) {
		mapi_errstr("GetProfileTable", retval);
		exit (1);
	}


	php_printf("We have %u profiles in the database:\n", proftable.cRows);

	for (count = 0; count != proftable.cRows; count++) {
		const char	*name = NULL;
		uint32_t	dflt = 0;

		name = proftable.aRow[count].lpProps[0].value.lpszA;
		dflt = proftable.aRow[count].lpProps[1].value.l;

		if (dflt) {
			php_printf("\tProfile = %s [default]\n", name);
		} else {
			php_printf("\tProfile = %s\n", name);
		}

	}
}


PHP_FUNCTION(hello_mapi)
{
	enum MAPISTATUS		retval;
  TALLOC_CTX		*mem_ctx;
  struct mapitest		mt;
   const char		*opt_profdb = NULL;
   // Initialize MAPI subsystem *\/ */
   if (!opt_profdb) {
     opt_profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
   }



        retval = MAPIInitialize(&(mt.mapi_ctx), opt_profdb);
        if (retval != MAPI_E_SUCCESS) {
          mapi_errstr("MAPIInitialize", retval);
          RETURN_STRING("MAPI FAILED", 1);
        } else {
          RETURN_STRING("HELLO MaPI OK", 1);
        }

    RETURN_STRING("HELLO MPAI", 1);
}



