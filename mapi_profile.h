#ifndef MAPI_PROFILE_H
#define MAPI_PROFILE_H

PHP_METHOD(MAPIProfile, __construct);
PHP_METHOD(MAPIProfile, __destruct);
PHP_METHOD(MAPIProfile, dump);
PHP_METHOD(MAPIProfile, logon);

void MAPIProfileRegisterClass(TSRMLS_D);

zval* create_profile_object(struct mapi_profile* profile, zval* profile_db, TALLOC_CTX* talloc_ctx  TSRMLS_DC);

struct mapi_profile* get_profile(zval* profileObject  TSRMLS_DC);
struct mapi_context* profile_get_mapi_context(zval* object TSRMLS_DC);
void mapi_profile_remove_children_session(zval* mapi_profile, zend_object_handle session_handle TSRMLS_DC);

struct mapi_profile_object
{
  zend_object std;
  char* path;
  TALLOC_CTX* talloc_ctx;
  zval* parent;
  zval* children_sessions;
  struct mapi_profile* profile;
};
typedef struct mapi_profile_object mapi_profile_object_t;



#endif
