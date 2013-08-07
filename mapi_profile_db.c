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
     PHP_ME(MAPIProfileDB, path, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(MAPIProfileDB, profiles, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(MAPIProfileDB, getProfile, NULL, ZEND_ACC_PUBLIC)

    { NULL, NULL, NULL }
};


static zend_class_entry* mapi_profile_db_ce;
static zend_object_handlers mapi_profile_db_object_handlers;

static void mapi_profile_db_free_storage(void *object TSRMLS_DC)
{
    mapi_profile_db_object_t* obj = (mapi_profile_db_object_t*) object;
    if (obj->mapi_ctx)
      MAPIUninitialize(obj->mapi_ctx);
    if (obj->path)
      efree(obj->path);
    if (obj->talloc_ctx)
      talloc_free(obj->talloc_ctx);

    zend_hash_destroy(obj->std.properties);
    FREE_HASHTABLE(obj->std.properties);

    efree(obj);
}

static zend_object_value mapi_profile_db_create_handler(zend_class_entry *type TSRMLS_DC)
{
    zval *tmp;
    zend_object_value retval;

    mapi_profile_db_object_t* obj = (mapi_profile_db_object_t*) emalloc(sizeof(mapi_profile_db_object_t));
    memset(obj, 0, sizeof(mapi_profile_db_object_t));

    obj->std.ce = type;

    ALLOC_HASHTABLE(obj->std.properties);
    zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_copy(obj->std.properties, &type->default_properties,
        (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));

    retval.handle = zend_objects_store_put(obj, NULL,
        mapi_profile_db_free_storage, NULL TSRMLS_CC);
    retval.handlers = &mapi_profile_db_object_handlers;

    return retval;
}

void MAPIProfileDBRegisterClass()
{
  zend_class_entry ce;
  INIT_CLASS_ENTRY(ce, "MAPIProfileDB", mapi_profile_db_class_functions);
  mapi_profile_db_ce = zend_register_internal_class(&ce TSRMLS_CC);
  mapi_profile_db_ce->create_object = mapi_profile_db_create_handler;
  memcpy(&mapi_profile_db_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  mapi_profile_db_object_handlers.clone_obj = NULL;
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

  zval* php_obj = getThis();
  mapi_profile_db_object_t* obj = (mapi_profile_db_object_t*) zend_object_store_get_object(php_obj TSRMLS_CC);
  obj->path = estrdup(profdb_path);
}

PHP_METHOD(MAPIProfileDB, __destruct)
{
}

PHP_METHOD(MAPIProfileDB, path)
{
  zval* php_obj = getThis();
  mapi_profile_db_object_t* obj = (mapi_profile_db_object_t*) zend_object_store_get_object(php_obj TSRMLS_CC);
  RETURN_STRING(obj->path, 1);
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


struct mapi_context* mapi_profile_db_get_mapi_context(zval* mapi_profile_db)
{
  mapi_profile_db_object_t* obj = (mapi_profile_db_object_t*) zend_object_store_get_object(mapi_profile_db TSRMLS_CC);
  if (obj->mapi_ctx != NULL)
    return obj->mapi_ctx;

  // create new mapi context
  struct mapi_context* new_ct  =  mapi_context_init(obj->path);
  obj->mapi_ctx = new_ct;
  return new_ct;
}

PHP_METHOD(MAPIProfileDB, profiles)
{
  struct mapi_context *mapi_ctx = mapi_profile_db_get_mapi_context(getThis());

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
  mapi_ctx = mapi_profile_db_get_mapi_context(this_obj);

  char* opt_profname = NULL;
  int   opt_profname_len;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &opt_profname, &opt_profname_len) == FAILURE) {
    RETURN_NULL();
  }

  mapi_ctx = mapi_profile_db_get_mapi_context(this_obj);
  mem_ctx = talloc_named(object_talloc_ctx(this_obj), 0, "getProfile");
  profile = get_profile_ptr(mem_ctx, mapi_ctx, opt_profname);

  zval* php_obj = create_profile_object(profile, this_obj, mem_ctx);
  RETURN_ZVAL(php_obj, 0, 0);
}
