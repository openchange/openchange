#ifndef MAPI_SESSION_H
#define  MAPI_SESSION_H

PHP_METHOD(MAPISession, __construct);
PHP_METHOD(MAPISession, login);
PHP_METHOD(MAPISession, folders);

void MAPISessionRegisterClass();

#endif
