#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"

// from openchange
#include "utils/mapitest/mapitest.h"
#include "utils/openchange-tools.h"


zend_class_entry *mapi_class;

static zend_function_entry mapi_class_functions[] = {
     PHP_ME(MAPI, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
     PHP_ME(MAPI, profiles, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(MAPI, dump_profile, NULL, ZEND_ACC_PUBLIC)

    { NULL, NULL, NULL }
};

static zend_function_entry mapi_functions[] = {
    {NULL, NULL, NULL}
};

zend_module_entry mapi_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_MAPI_EXTNAME,
    mapi_functions,
    PHP_MINIT(mapi),
    PHP_MSHUTDOWN(mapi),
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

static HashTable *mapi_context_by_object;


PHP_MINIT_FUNCTION(mapi)
{
  // register class
  zend_class_entry ce;
  INIT_CLASS_ENTRY(ce, MAPI_CLASS_NAME, mapi_class_functions);
  mapi_class =
    zend_register_internal_class(&ce TSRMLS_CC);

  // initialize mpai contexts hash
  ALLOC_HASHTABLE(mapi_context_by_object);
  zend_hash_init(mapi_context_by_object, EXPECTED_MAPI_OBJECTS,
                 NULL,
                 mapi_context_dtor, 1);
  return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(mapi)
{
  php_printf("SHUTDOWN\n" );
  zend_hash_destroy(mapi_context_by_object);
}

struct mapi_context* mapi_context_init(char *profdb)
{
  struct mapi_context   *mapi_ctx;
  enum MAPISTATUS        retval;
  php_printf("MAPI ctx init:%s\n", profdb);
  retval = MAPIInitialize(&mapi_ctx, profdb);
  if (retval != MAPI_E_SUCCESS) {
    const char *err_str = mapi_get_errstr(retval);
    php_error(E_ERROR, "Intialize MAPI: %s", err_str);
  }

  char *str = "macaco";
  php_printf("Adding macaco\n");
  zend_hash_index_update(mapi_context_by_object, 11,
                         str, sizeof(char *), NULL);
  php_printf("Getting macaco\n");
  zend_hash_index_find(mapi_context_by_object, 11, (void**) &str);

  php_printf("Rerieved macaco: %s\n", str);




  return mapi_ctx;
}

void mapi_context_dtor(void *ptr)
{
  php_printf("MAPI ctx to be destroyd\n" );

  //  struct mapi_context* ctx =  (struct mapi_context**)  *ptr;
  MAPIUninitialize(ptr);
}

PHP_METHOD(MAPI, __construct)
{
  char* profdb_path;
  int   profdb_path_len;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &profdb_path, &profdb_path_len) == FAILURE) {
    RETURN_NULL();
  }

  zval *object = getThis();
  add_property_string(object, "__profdb", profdb_path, 0);
}


struct mapi_context* get_mapi_context(zval* object)
{
  struct mapi_context* ct;
  int res;

  if (object == NULL) {
    php_error(E_ERROR, "Must be called inside of a method");
  }

  ulong key = (ulong) object; // XXX check this is true in all cases
  php_printf("KEy %u\n", key);


  // mapi context retrieval
  php_printf("before FIMD\n");


  res= zend_hash_index_find(mapi_context_by_object, key, (void**) &ct);
  if (res == SUCCESS) {
    php_printf("FOUND CT\n");
    php_printf("POinter restored %p %p\n", ct->mem_ctx, ct->session);
    return ct;
  }

  // new mapi context
  zval **profdb;
  if (zend_hash_find(Z_OBJPROP_P(object),
                     "__profdb", sizeof("__profdb"), (void**)&profdb) == FAILURE) {
    php_error(E_ERROR, "__profdb attribute not found");
  }

  php_printf("NOT FOUND CT -> before UPDATE\n");
  ct  =  mapi_context_init(Z_STRVAL_P(*profdb));
  php_printf("POinter %p %p\n", ct->mem_ctx, ct->session);

  res = zend_hash_index_update(mapi_context_by_object, key,
                                ct, sizeof(struct mapi_context*), NULL);
  if (res == FAILURE) {
    php_error(E_ERROR, "Adding to MAPI contexts hash");
  }

  return ct;


}

PHP_METHOD(MAPI, profiles)
{
  struct mapi_context *mapi_ctx = get_mapi_context(getThis());

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

    name = (char*) proftable.aRow[count].lpProps[0].value.lpszA;
    dflt = proftable.aRow[count].lpProps[1].value.l;

    MAKE_STD_ZVAL(profile);
    array_init(profile);
    add_assoc_string(profile, "name", name, 1);
    add_assoc_bool(profile, "default", dflt);
    add_next_index_zval(return_value, profile);
  }

  //  MAPIUninitialize(mapi_ctx);
}

PHP_METHOD(MAPI, dump_profile)
{
    char* opt_profname = NULL;
    int   opt_profname_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &opt_profname, &opt_profname_len) == FAILURE) {
        RETURN_NULL();
    }

    struct mapi_context *mapi_ctx = get_mapi_context(getThis());

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
        const char *err_str = mapi_get_errstr(retval);
        php_error(E_ERROR, "Get default profile: %s", err_str);
        exit (1);
      }
    } else {
      profname = talloc_strdup(mem_ctx, (const char *)opt_profname);
    }

    retval = OpenProfile(mapi_ctx, profile, profname, NULL);
    talloc_free(profname);

    if (retval && (retval != MAPI_E_INVALID_PARAMETER)) {
      talloc_free(mem_ctx);
      const char *err_str = mapi_get_errstr(retval);
      php_error(E_ERROR, "Get %s profile: %s", opt_profname, err_str);
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
    add_assoc_string(return_value, "profile", (char *) profile->profname, 1);
    add_assoc_string(return_value, "exchange_server", exchange_version, 1);
    add_assoc_string(return_value, "encription", (profile->seal == true) ? "yes" : "no", 1);
    add_assoc_string(return_value, "username", (char*) profile->username, 1);
    add_assoc_string(return_value, "password", (char*) profile->password, 1);
    add_assoc_string(return_value, "mailbox", (char*) profile->mailbox, 1);
    add_assoc_string(return_value, "workstation", (char*) profile->workstation, 1);
    add_assoc_string(return_value, "domain", (char*) profile->domain, 1);
    add_assoc_string(return_value, "server", (char*) profile->server, 1);

 end:
    talloc_free(mem_ctx);
    //    MAPIUninitialize(mapi_ctx);
}
