#ifndef MAPI_PROFILE_H
#define MAPI_PROFILE_H

PHP_METHOD(MAPIProfile, __construct);
PHP_METHOD(MAPIProfile, __destruct);
PHP_METHOD(MAPIProfile, dump);

void MAPIProfileRegisterClass();

#endif
