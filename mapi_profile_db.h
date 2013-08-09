#ifndef MAPI_PROFILE_DB_H
#define MAPI_PROFILE_DB_H

PHP_METHOD(MAPIProfileDB, __construct);
PHP_METHOD(MAPIProfileDB, __destruct);
PHP_METHOD(MAPIProfileDB, path);
PHP_METHOD(MAPIProfileDB, profiles);
PHP_METHOD(MAPIProfileDB, getProfile);

void MAPIProfileDBRegisterClass(TSRMLS_D);
struct mapi_context* mapi_profile_db_get_mapi_context(zval* mapiProfileDB TSRMLS_DC);
void mapi_profile_db_remove_children_profile(zval* mapi_profile_db, zend_object_handle profile_handle TSRMLS_DC);

struct mapi_profile_db_object
{
  zend_object std;
  char* path;
  TALLOC_CTX* talloc_ctx;
  zval* children_profiles;
  struct mapi_context* mapi_ctx;
};
typedef struct mapi_profile_db_object mapi_profile_db_object_t;


#endif
