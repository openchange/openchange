#ifndef MAPI_PROFILE_DB_H
#define MAPI_PROFILE_DB_H

PHP_METHOD(MAPIProfileDB, __construct);
PHP_METHOD(MAPIProfileDB, login);
PHP_METHOD(MAPIProfileDB, folders);

void MAPIProfileDBRegisterClass();

#endif
