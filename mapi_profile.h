#ifndef MAPI_PROFILE_H
#define MAPI_PROFILE_H

PHP_METHOD(MAPIProfile, __construct);
PHP_METHOD(MAPIProfile, __destruct);
PHP_METHOD(MAPIProfile, dump);
PHP_METHOD(MAPIProfile, logon);

void MAPIProfileRegisterClass();
struct mapi_profile* get_profile(zval* profileObject);
struct mapi_context* get_profile_mapi_context(zval* object);

#endif
