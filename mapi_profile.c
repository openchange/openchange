#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"
#include "mapi_profile.h"
#include "mapi_profile_db.h"
#include "mapi_session.h"

static zend_function_entry mapi_profile_class_functions[] = {
  PHP_ME(MAPIProfile, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
  PHP_ME(MAPIProfile, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
  PHP_ME(MAPIProfile, dump, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(MAPIProfile, logon, NULL, ZEND_ACC_PUBLIC)
  { NULL, NULL, NULL }
};

static zend_class_entry* mapi_profile_ce;
static zend_object_handlers mapi_profile_object_handlers;

static void mapi_profile_free_storage(void *object TSRMLS_DC)
{
    mapi_profile_object_t* obj = (mapi_profile_object_t*) object;
    if (obj->talloc_ctx)
      talloc_free(obj->talloc_ctx);
    zend_hash_destroy(obj->std.properties);
    FREE_HASHTABLE(obj->std.properties);

    if (obj->children_sessions) {
      zval_dtor(obj->children_sessions);
      FREE_ZVAL(obj->children_sessions);
    }

    efree(obj);
}

static zend_object_value mapi_profile_create_handler(zend_class_entry *type TSRMLS_DC)
{
    zval *tmp;
    zend_object_value retval;

    mapi_profile_object_t* obj = (mapi_profile_object_t*) emalloc(sizeof(mapi_profile_object_t));
    memset(obj, 0, sizeof(mapi_profile_object_t));

    obj->std.ce = type;

    ALLOC_HASHTABLE(obj->std.properties);
    zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
#if PHP_VERSION_ID < 50399
    zend_hash_copy(obj->std.properties, &type->default_properties,
        (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
#else
    object_properties_init((zend_object *) &(obj->std), type);
#endif
    retval.handle = zend_objects_store_put(obj, NULL,
        mapi_profile_free_storage, NULL TSRMLS_CC);
    retval.handlers = &mapi_profile_object_handlers;

    return retval;
}

void MAPIProfileRegisterClass(TSRMLS_D)
{
  zend_class_entry ce;
  INIT_CLASS_ENTRY(ce, "MAPIProfile", mapi_profile_class_functions);
  mapi_profile_ce = zend_register_internal_class(&ce TSRMLS_CC);
  mapi_profile_ce->create_object = mapi_profile_create_handler;
  memcpy(&mapi_profile_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  mapi_profile_object_handlers.clone_obj = NULL;
}

zval* create_profile_object(struct mapi_profile* profile, zval* profile_db, TALLOC_CTX* talloc_ctx TSRMLS_DC)
{
  zval* php_obj;
  MAKE_STD_ZVAL(php_obj);

  /* create the MapiProfile instance in return_value zval */
  zend_class_entry **ce;
  if (zend_hash_find(EG(class_table),"mapiprofile", sizeof("mapiprofile"),(void**)&ce) == FAILURE) {
    php_error(E_ERROR,"Class MAPIProfile does not exist.");
  }

  object_init_ex(php_obj, *ce);

  mapi_profile_object_t* obj = (mapi_profile_object_t*) zend_object_store_get_object(php_obj TSRMLS_CC);
  obj->profile = profile;
  obj->parent = profile_db;
  obj->talloc_ctx =  talloc_ctx;
  MAKE_STD_ZVAL(obj->children_sessions);
  array_init(obj->children_sessions);

  return php_obj;
}

struct mapi_context* profile_get_mapi_context(zval* profile_obj TSRMLS_DC)
{
  mapi_profile_object_t* obj = (mapi_profile_object_t*) zend_object_store_get_object(profile_obj TSRMLS_CC);
  return mapi_profile_db_get_mapi_context(obj->parent  TSRMLS_CC);
}

struct mapi_profile* get_profile(zval* php_profile_obj TSRMLS_DC)
{
  mapi_profile_object_t* obj = (mapi_profile_object_t*) zend_object_store_get_object(php_profile_obj TSRMLS_CC);
  return obj->profile;
}

PHP_METHOD(MAPIProfile, __construct)
{
  php_error(E_ERROR, "This class cannot be instatiated. Use the getProfile class from MapiProfileDB");
}

PHP_METHOD(MAPIProfile, __destruct)
{
  zval* php_this = getThis();

  mapi_profile_object_t* this = (mapi_profile_object_t*) zend_object_store_get_object(php_this TSRMLS_CC);
  if (zend_hash_num_elements(this->children_sessions->value.ht) > 0) {
    php_error(E_ERROR, "This MapiProfile object has active sessions");
  }

  // remove this instance form parent
  zval* parent = this->parent;
  mapi_profile_db_remove_children_profile(parent, Z_OBJ_HANDLE_P(php_this) TSRMLS_CC);
}

PHP_METHOD(MAPIProfile, dump)
{
  struct mapi_profile* profile;
  char* exchange_version = NULL;

  profile = get_profile(getThis() TSRMLS_CC);

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
  struct mapi_profile* profile;
  struct mapi_session* session = NULL;
  TALLOC_CTX*          talloc_ctx;

  zval* this_php_obj = getThis();
  mapi_ctx = profile_get_mapi_context(this_php_obj TSRMLS_CC);
  profile = get_profile(this_php_obj TSRMLS_CC);

  enum MAPISTATUS logon_status = MapiLogonEx(mapi_ctx, &session, profile->profname, profile->password);
  if (logon_status != MAPI_E_SUCCESS) {
    php_error(E_ERROR, "MapiLogonEx: %s",  mapi_get_errstr(logon_status));
  }

  talloc_ctx = talloc_named(NULL, 0, "session");
  zval* php_obj = create_session_object(session, this_php_obj, talloc_ctx TSRMLS_CC);

  mapi_profile_object_t* this_obj = (mapi_profile_object_t*) zend_object_store_get_object(this_php_obj TSRMLS_CC);
  add_index_zval(this_obj->children_sessions, (long) Z_OBJ_HANDLE_P(php_obj), php_obj);

  RETURN_ZVAL(php_obj, 0, 0);
}

void mapi_profile_remove_children_session(zval* mapi_profile, zend_object_handle session_handle TSRMLS_DC)
{
    mapi_profile_object_t* this_obj = (mapi_profile_object_t*) zend_object_store_get_object(mapi_profile TSRMLS_CC);
    zend_hash_index_del(this_obj->children_sessions->value.ht, (long) session_handle);
}
