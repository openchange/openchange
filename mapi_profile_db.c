#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"
#include "mapi_profile_db.h"

extern int profile_resource_id;
extern HashTable *mapi_context_by_object;

static zend_function_entry mapi_profile_db_class_functions[] = {
     PHP_ME(MAPIProfileDB, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
     PHP_ME(MAPIProfileDB, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
     PHP_ME(MAPIProfileDB, profiles, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(MAPIProfileDB, getProfile, NULL, ZEND_ACC_PUBLIC)

    { NULL, NULL, NULL }
};

void MAPIProfileDBRegisterClass()
{
  zend_class_entry ce;
  INIT_CLASS_ENTRY(ce, "MAPIProfileDB", mapi_profile_db_class_functions);
  zend_register_internal_class(&ce TSRMLS_CC);
}


static struct mapi_profile* get_profile_ptr(TALLOC_CTX* mem_ctx,  struct mapi_context* mapi_ctx, char* opt_profname)
{
    enum MAPISTATUS      retval;
    char*                profname;
    struct mapi_profile* profile;

    profile = talloc(mem_ctx, struct mapi_profile);

    if (!opt_profname) {
      if ((retval = GetDefaultProfile(mapi_ctx, &profname)) != MAPI_E_SUCCESS) {
        talloc_free(mem_ctx);
        const char *err_str = mapi_get_errstr(retval);
        php_error(E_ERROR, "Get default profile: %s", err_str);
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

    return profile;
}


PHP_METHOD(MAPIProfileDB, __construct)
{
  char* profdb_path;
  int   profdb_path_len;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &profdb_path, &profdb_path_len) == FAILURE) {
    RETURN_NULL();
  }

  zval *object = getThis();
  add_property_string(object, "__profdb", profdb_path, 0);
}

PHP_METHOD(MAPIProfileDB, __destruct)
{
  ulong key = (ulong) getThis();
  if (zend_hash_index_exists(mapi_context_by_object, key)) {
    zend_hash_index_del(mapi_context_by_object, key);
  }
}

struct mapi_context* get_mapi_context(zval* mapiProfileDB)
{
  struct mapi_context** ct;
  int res;

  ulong key = (ulong) mapiProfileDB; // XXX check this is true in all cases

  res= zend_hash_index_find(mapi_context_by_object, key, (void**) &ct);
  if (res == SUCCESS) {
    return *ct;
  }

  // new mapi context
  zval **profdb;
  if (zend_hash_find(Z_OBJPROP_P(mapiProfileDB),
                     "__profdb", sizeof("__profdb"), (void**)&profdb) == FAILURE) {
    php_error(E_ERROR, "__profdb attribute not found");
  }

  struct mapi_context* new_ct  =  mapi_context_init(Z_STRVAL_P(*profdb));
  ct = &new_ct;

  res = zend_hash_index_update(mapi_context_by_object, key,
                                ct, sizeof(struct mapi_context**), NULL);
  if (res == FAILURE) {
    php_error(E_ERROR, "Adding to MAPI contexts hash");
  }

  return *ct;
}

PHP_METHOD(MAPIProfileDB, profiles)
{
  struct mapi_context *mapi_ctx = get_mapi_context(getThis());

  struct SRowSet proftable;
  memset(&proftable, 0, sizeof (struct SRowSet));
  enum MAPISTATUS               retval;
  if ((retval = GetProfileTable(mapi_ctx, &proftable)) != MAPI_E_SUCCESS) {
    const char *err_str = mapi_get_errstr(retval);
    php_error(E_ERROR, "GetProfileTable: %s", err_str);
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
}



PHP_METHOD(MAPIProfileDB, getProfile)
{
  zval *profile_resource;
  struct mapi_profile* profile;
  TALLOC_CTX*          mem_ctx;
  struct mapi_context* mapi_ctx;

  zval* this_obj= getThis();
  mapi_ctx = get_mapi_context(this_obj);

  char* opt_profname = NULL;
  int   opt_profname_len;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &opt_profname, &opt_profname_len) == FAILURE) {
    RETURN_NULL();
  }

  mapi_ctx = get_mapi_context(this_obj);
  mem_ctx = talloc_named(object_talloc_ctx(this_obj), 0, "getProfile");
  profile = get_profile_ptr(mem_ctx, mapi_ctx, opt_profname);

  struct entry_w_mem_ctx* profile_w_mem_ctx;
  profile_w_mem_ctx = emalloc(sizeof(struct entry_w_mem_ctx));
  profile_w_mem_ctx->entry = profile;
  profile_w_mem_ctx->mem_ctx = mem_ctx;

  MAKE_STD_ZVAL(profile_resource);
  ZEND_REGISTER_RESOURCE(profile_resource, profile_w_mem_ctx, profile_resource_id);

  /* now create the MapiProfile instance */
  zend_class_entry **ce;
  if (zend_hash_find(EG(class_table),"mapiprofile", sizeof("mapiprofile"),(void**)&ce) == FAILURE) {
    php_error(E_ERROR,"Class MAPIProfile does not exist.");
  }

  object_init_ex(return_value, *ce);

  // call contructor with the profile resource
  zval* args[2];
  zval retval, str, funcname;
  args[0]  = this_obj;
  args[1]  = profile_resource;
  ZVAL_STRING(&funcname, "__construct", 0);
  call_user_function(&((*ce)->function_table), &return_value, &funcname, &retval, 2, args TSRMLS_CC);
}
