#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"

extern int profile_resource_id;

PHP_METHOD(MAPIProfile, __construct)
{
  zval* thisObject;
  zval *profile_resource;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r",
                            &profile_resource) == FAILURE ) {
    RETURN_NULL();
  }

  thisObject = getThis();
  add_property_zval(thisObject, "profile", profile_resource);


  /* char* propname; */
  /* int propname_len; */
  /* zend_mangle_property_name(&propname, &propname_len, */
  /*                           "MAPIProfile",sizeof("MAPIProfile")-1, */
  /*                           "profile", sizeof("profile")-1, 0); */
  /* add_property_zval_ex(thisObject, propname, propname_len, */
  /*                      profile_resource); */
  /* efree(propname); */

}

PHP_METHOD(MAPIProfile, __destruct)
{
  php_printf("MAPIPROFILE destruct\n"
             );

}

PHP_METHOD(MAPIProfile, dump)
{
  zval** profile_resource;
  struct entry_w_mem_ctx* profile_w_mem_ctx;
  struct mapi_profile* profile;
  char* exchange_version = NULL;

  if (zend_hash_find(Z_OBJPROP_P(getThis()),
                     "profile", sizeof("profile"), (void**)&profile_resource) == FAILURE) {
    php_error(E_ERROR, "profile attribute not found");
  }

  ZEND_FETCH_RESOURCE(profile_w_mem_ctx, struct entry_w_mem_ctx*, profile_resource, -1, PROFILE_RESOURCE_NAME, profile_resource_id);
  profile = (struct mapi_profile*) profile_w_mem_ctx->entry;

  switch (profile->exchange_version) {
  case 0x0:
    exchange_version = "exchange 2000";
    break;
  case 0x1:
    exchange_version = "exchange 2003/2007";
    break;
  case 0x2:
    exchange_version = "exchange 2010";
    break;
  default:
    php_error(E_ERROR, "Error: unknown Exchange server: %i\n", profile->exchange_version);
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
}

