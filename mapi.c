#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"

// from openchange
#include "utils/mapitest/mapitest.h"
#include "utils/openchange-tools.h"

static zend_function_entry mapi_functions[] = {
    PHP_FE(profiles, NULL)
    PHP_FE(dump_profile, NULL)
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


struct mapi_context* initialize_mapi()
{
  char *profdb = "/home/jag/.openchange/profiles.ldb";
  struct mapi_context   *mapi_ctx;
  enum MAPISTATUS        retval;
  retval = MAPIInitialize(&mapi_ctx, profdb);
  if (retval != MAPI_E_SUCCESS) {
    char *err_str = mapi_get_errstr(retval);
    php_error(E_ERROR, err_str);
    php_printf("ERROR MAPI: %s\n", err_str); // TMP
    // TODO BAIL OUT

  }
  return mapi_ctx;
}

PHP_FUNCTION(profiles)
{
  struct mapi_context *mapi_ctx = initialize_mapi();

  struct SRowSet proftable;
  memset(&proftable, 0, sizeof (struct SRowSet));
  enum MAPISTATUS               retval;
  if ((retval = GetProfileTable(mapi_ctx, &proftable)) != MAPI_E_SUCCESS) {
    mapi_errstr("GetProfileTable", retval);
    exit (1);
  }

  array_init(return_value);

  uint32_t count;
  for (count = 0; count != proftable.cRows; count++) {
    char* name = NULL;
    uint32_t dflt = 0;
    zval* profile;

    name = proftable.aRow[count].lpProps[0].value.lpszA;
    dflt = proftable.aRow[count].lpProps[1].value.l;

    MAKE_STD_ZVAL(profile);
    array_init(profile);
    add_assoc_string(profile, "name", name, 1);
    add_assoc_bool(profile, "default", dflt);
    add_next_index_zval(return_value, profile);
  }
}

PHP_FUNCTION(dump_profile)
{
    char* opt_profname = NULL;
    int   opt_profname_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &opt_profname, &opt_profname_len) == FAILURE) {
        RETURN_NULL();
    }

    struct mapi_context *mapi_ctx = initialize_mapi();


    TALLOC_CTX              *mem_ctx;
    enum MAPISTATUS         retval;
    struct mapi_profile     *profile;
    char                    *profname;
    char                    *exchange_version = NULL;

    mem_ctx = talloc_named(mapi_ctx->mem_ctx, 0, "mapiprofile_dump");
    profile = talloc(mem_ctx, struct mapi_profile);

    if (!opt_profname) {
      if ((retval = GetDefaultProfile(mapi_ctx, &profname)) != MAPI_E_SUCCESS) {
        mapi_errstr("GetDefaultProfile", retval);
        talloc_free(mem_ctx);
        exit (1);
      }
    } else {
      profname = talloc_strdup(mem_ctx, (const char *)opt_profname);
    }

    retval = OpenProfile(mapi_ctx, profile, profname, NULL);
    talloc_free(profname);

    if (retval && (retval != MAPI_E_INVALID_PARAMETER)) {
      talloc_free(mem_ctx);
      mapi_errstr("OpenProfile", retval);
      exit (1);
    }

    switch (profile->exchange_version) {
    case 0x0:
      exchange_version = talloc_strdup(mem_ctx, "exchange 2000");
      break;
    case 0x1:
      exchange_version = talloc_strdup(mem_ctx, "exchange 2003/2007");
      break;
    case 0x2:
      exchange_version = talloc_strdup(mem_ctx, "exchange 2010");
      break;
    default:
      php_printf("Error: unknown Exchange server\n");
      goto end;
    }

    array_init(return_value);
    add_assoc_string(return_value, "profile", profile->profname, 1);
    add_assoc_string(return_value, "exchange_server", exchange_version, 1);
    add_assoc_string(return_value, "encription", (profile->seal == true) ? "yes" : "no", 1);
    add_assoc_string(return_value, "username", profile->username, 1);
    add_assoc_string(return_value, "password", profile->password, 1);
    add_assoc_string(return_value, "mailbox", profile->mailbox, 1);
    add_assoc_string(return_value, "workstation", profile->workstation, 1);
    add_assoc_string(return_value, "domain", profile->domain, 1);
    add_assoc_string(return_value, "server", profile->server, 1);

 end:
    talloc_free(mem_ctx);
    //    talloc_free(profile); ??? XXX sems not freed
}

