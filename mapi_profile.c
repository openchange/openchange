#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"
#include "mapi_profile.h"
#include "mapi_profile_db.h"

extern int profile_resource_id;
extern int session_resource_id;

static zend_function_entry mapi_profile_class_functions[] = {
  PHP_ME(MAPIProfile, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
  PHP_ME(MAPIProfile, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
  PHP_ME(MAPIProfile, dump, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(MAPIProfile, logon, NULL, ZEND_ACC_PUBLIC)
  { NULL, NULL, NULL }
};


void MAPIProfileRegisterClass()
{
  zend_class_entry ce;
  INIT_CLASS_ENTRY(ce, "MAPIProfile", mapi_profile_class_functions);
  zend_register_internal_class(&ce TSRMLS_CC);
}

struct mapi_context* get_profile_mapi_context(zval* profile_obj)
{
  zval** parent_db;
  struct mapi_context* ctx;

  char* exchange_version = NULL;

  if (zend_hash_find(Z_OBJPROP_P(profile_obj),
                     "profiles_db", sizeof("profiles_db"), (void**)&parent_db) == FAILURE) {
    php_error(E_ERROR, "profiles_db attribute not found");
  }

  return mapi_profile_db_get_mapi_context(*parent_db);
}

struct mapi_profile* get_profile(zval* profileObject)
{
  zval** profile_resource;
  struct entry_w_mem_ctx* profile_w_mem_ctx;

  if (zend_hash_find(Z_OBJPROP_P(profileObject),
                     "profile", sizeof("profile"), (void**)&profile_resource) == FAILURE) {
    php_error(E_ERROR, "profile attribute not found");
  }

  ZEND_FETCH_RESOURCE_NO_RETURN(profile_w_mem_ctx, struct entry_w_mem_ctx*, profile_resource, -1, PROFILE_RESOURCE_NAME, profile_resource_id);
  if (profile_w_mem_ctx == NULL) {
    php_error(E_ERROR, "profile resource not correctly fetched");
  }

  return (struct mapi_profile*) profile_w_mem_ctx->entry;
}

PHP_METHOD(MAPIProfile, __construct)
{
  zval* thisObject;
  zval* profile_resource;
  zval* db_object;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "or", &db_object, &profile_resource) == FAILURE ) {
    RETURN_NULL();
  }
  if (strncmp(Z_OBJCE_P(db_object)->name, "MAPIProfileDB", sizeof("MAPIProfileDB")+1) != 0) {
    php_error(E_ERROR, "The object must be of the class MAPIProfileDB instead of %s", Z_OBJCE_P(db_object)->name);
  }

  thisObject = getThis();
  add_property_zval(thisObject, "profile", profile_resource);
  add_property_zval(thisObject, "profiles_db", db_object);


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
  // nothing for now
}

PHP_METHOD(MAPIProfile, dump)
{
  zval** profile_resource;
  struct entry_w_mem_ctx* profile_w_mem_ctx;
  struct mapi_profile* profile;
  char* exchange_version = NULL;

  profile = get_profile(getThis());

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

PHP_METHOD(MAPIProfile, logon)
{
  struct mapi_context* mapi_ctx;
  struct entry_w_mem_ctx* profile_w_mem_ctx;
  struct mapi_profile* profile;
  struct mapi_session* session = NULL;
  zval** profile_resource;
  zval*  session_resource;

  zval* this_obj = getThis();
  mapi_ctx = get_profile_mapi_context(this_obj);
  profile = get_profile(this_obj);

  enum MAPISTATUS logon_status = MapiLogonEx(mapi_ctx, &session, profile->profname, profile->password);
  if (logon_status != MAPI_E_SUCCESS) {
    php_error(E_ERROR, "MapiLogonEx: %s",  mapi_get_errstr(logon_status));
  }

  MAKE_STD_ZVAL(session_resource);
  ZEND_REGISTER_RESOURCE(session_resource, session, session_resource_id);

  /* now create the MapiSession instance */
  zend_class_entry **ce;
  if (zend_hash_find(EG(class_table),"mapisession", sizeof("mapisession"),(void**)&ce) == FAILURE) {
    php_error(E_ERROR,"Class MAPISession does not exist.");
  }

  object_init_ex(return_value, *ce);

  // call contructor with the session resource
  zval* args[2];
  zval retval, str, funcname;
  args[0]  = this_obj;
  args[1]  = session_resource;
  ZVAL_STRING(&funcname, "__construct", 0);
  call_user_function(&((*ce)->function_table), &return_value, &funcname, &retval, 2, args TSRMLS_CC);
}


