#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"
#include "mapi_session.h"

zend_class_entry *mapi_profile_db_class;
static zend_function_entry mapi_profile_db_class_functions[] = {
     PHP_ME(MAPIProfileDB, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
     PHP_ME(MAPIProfileDB, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
     PHP_ME(MAPIProfileDB, profiles, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(MAPIProfileDB, getProfile, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(MAPIProfileDB, folders, NULL, ZEND_ACC_PUBLIC)

    { NULL, NULL, NULL }
};

zend_class_entry *mapi_profile_class;
static zend_function_entry mapi_profile_class_functions[] = {
  PHP_ME(MAPIProfile, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
  PHP_ME(MAPIProfile, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
  PHP_ME(MAPIProfile, dump, NULL, ZEND_ACC_PUBLIC)
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

HashTable *mapi_context_by_object; // no static sine profile_Db needs access

static void entry_w_mem_ctx_res_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
  struct entry_w_mem_ctx* ptr = (struct entry_w_mem_ctx*) rsrc->ptr;
  php_printf("profile %p mem_ctx %p \n", ptr->entry, ptr->mem_ctx);

  if (ptr->mem_ctx != NULL) {
    php_printf("bfore talloc_free\n");
    talloc_free(ptr->mem_ctx);
  }

  php_printf("bfore efree\n");


  efree(ptr);
  php_printf("end dtor\n");
}

int profile_resource_id;

PHP_MINIT_FUNCTION(mapi)
{
  // register classes
  zend_class_entry ce;
  INIT_CLASS_ENTRY(ce, "MAPIProfileDB", mapi_profile_db_class_functions);
  mapi_profile_db_class =
    zend_register_internal_class(&ce TSRMLS_CC);

  INIT_CLASS_ENTRY(ce, "MAPIProfile", mapi_profile_class_functions);
  mapi_profile_db_class =
    zend_register_internal_class(&ce TSRMLS_CC);

  MAPISessionRegisterClass();

  // initialize mpai contexts hash
  ALLOC_HASHTABLE(mapi_context_by_object);
  zend_hash_init(mapi_context_by_object, EXPECTED_MAPI_OBJECTS,
                 NULL,
                 mapi_context_dtor, 1);

  profile_resource_id = zend_register_list_destructors_ex(
                entry_w_mem_ctx_res_dtor, NULL, PROFILE_RESOURCE_NAME,
                module_number);

  return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(mapi)
{
  zend_hash_destroy(mapi_context_by_object);
}

struct mapi_context* mapi_context_init(char *profdb)
{
  struct mapi_context   *mapi_ctx;
  enum MAPISTATUS        retval;
  retval = MAPIInitialize(&mapi_ctx, profdb);
  if (retval != MAPI_E_SUCCESS) {
    const char *err_str = mapi_get_errstr(retval);
    php_error(E_ERROR, "Intialize MAPI: %s", err_str);
  }

  return mapi_ctx;
}

void mapi_context_dtor(void *ptr)
{
  struct mapi_context** ctx =  ptr;
  php_printf("MAPI ctx dtor\n");

  MAPIUninitialize(*ctx);
}

