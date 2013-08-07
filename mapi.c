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

void talloc_ctx_res_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
  TALLOC_CTX* ctx = (TALLOC_CTX*) rsrc->ptr;
  talloc_free(ctx);
}

void do_nothing_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
}

//int profile_resource_id;
int session_resource_id;
int talloc_resource_id;

PHP_MINIT_FUNCTION(mapi)
{
  // register classes
  MAPIProfileDBRegisterClass();
  MAPIProfileRegisterClass();
  MAPISessionRegisterClass();

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
  ctx = talloc_named(NULL, 0, "object talloc context");

  MAKE_STD_ZVAL(ctx_res);
  ZEND_REGISTER_RESOURCE(ctx_res, ctx, talloc_resource_id);
  add_property_zval(obj, "__talloc_cxt", ctx_res);

  return ctx;
}
