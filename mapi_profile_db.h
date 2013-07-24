#ifndef MAPI_PROFILE_DB_H
#define MAPI_PROFILE_DB_H

PHP_METHOD(MAPIProfileDB, __construct);
PHP_METHOD(MAPIProfileDB, __destruct);
PHP_METHOD(MAPIProfileDB, profiles);
PHP_METHOD(MAPIProfileDB, getProfile);

void MAPIProfileDBRegisterClass();
struct mapi_context* get_mapi_context(zval* mapiProfileDB);

#endif
