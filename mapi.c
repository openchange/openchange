#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"
#include "mapi_profile_db.h"
#include "mapi_profile.h"
#include "mapi_session.h"

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

void entry_w_mem_ctx_res_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
  struct entry_w_mem_ctx* ptr = (struct entry_w_mem_ctx*) rsrc->ptr;
  if (ptr->mem_ctx != NULL) {
    talloc_free(ptr->mem_ctx);
  }

  efree(ptr);
}

void do_nothing_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
}

void talloc_ctx_res_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
  TALLOC_CTX* ctx = (TALLOC_CTX*) rsrc->ptr;
  talloc_free(ctx);
}


int profile_resource_id;
int session_resource_id;
int talloc_resource_id;

PHP_MINIT_FUNCTION(mapi)
{
  // register classes
  MAPIProfileDBRegisterClass();
  MAPIProfileRegisterClass();
  MAPISessionRegisterClass();

  // initialize mpai contexts hash
  ALLOC_HASHTABLE(mapi_context_by_object);
  zend_hash_init(mapi_context_by_object, EXPECTED_MAPI_OBJECTS,
                 NULL,
                 mapi_context_dtor, 1);

  profile_resource_id = zend_register_list_destructors_ex(
                entry_w_mem_ctx_res_dtor, NULL, PROFILE_RESOURCE_NAME,
                module_number);
  session_resource_id = zend_register_list_destructors_ex(
                do_nothing_dtor, NULL, SESSION_RESOURCE_NAME,
                module_number);
  talloc_resource_id = zend_register_list_destructors_ex(
                talloc_ctx_res_dtor, NULL, TALLOC_RESOURCE_NAME,
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
  MAPIUninitialize(*ctx);
}


TALLOC_CTX* object_talloc_ctx(zval* obj)
{
  TALLOC_CTX* ctx;

  zval** stored_ctx;
  if (zend_hash_find(Z_OBJPROP_P(obj), "_talloc_ctx", sizeof("_talloc_ctx"), (void**)&stored_ctx) == SUCCESS) {
    ZEND_FETCH_RESOURCE_NO_RETURN(ctx, struct TALLOC_CTX**, stored_ctx, -1, TALLOC_RESOURCE_NAME, talloc_resource_id);
    if (ctx  == NULL) {
      php_error(E_ERROR, "Talloc context not correctly fetched");
    }
    return ctx;
  }

  zval* ctx_res;
  char *name = "temp_name";
  ctx = talloc_named(NULL, 0, name);

  MAKE_STD_ZVAL(ctx_res);
  ZEND_REGISTER_RESOURCE(ctx_res, ctx, talloc_resource_id);
  add_property_zval(obj, "__talloc_cxt", ctx_res);

  return ctx;
}

