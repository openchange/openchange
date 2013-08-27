/*
   OpenChange MAPI PHP bindings

   Copyright (C) 2013 Zentyal S.L.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAPI_MAILBOX_H
#define MAPI_MAILBOX_H

typedef struct mapi_mailbox_object
{
	zend_object	std;
	mapi_object_t	obj_store;
	TALLOC_CTX	*talloc_ctx;
	const char      *username;
	zval		*parent_session;
} mapi_mailbox_object_t;

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS		extern "C" {
#define __END_DECLS		}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

__BEGIN_DECLS

PHP_METHOD(MAPIMailbox, __construct);
PHP_METHOD(MAPIMailbox, __destruct);
PHP_METHOD(MAPIMailbox, getName);
PHP_METHOD(MAPIMailbox, setName);
PHP_METHOD(MAPIMailbox, inbox);
PHP_METHOD(MAPIMailbox, calendar);
PHP_METHOD(MAPIMailbox, contacts);
PHP_METHOD(MAPIMailbox, tasks);

void MAPIMailboxRegisterClass(TSRMLS_D);
zval *create_mailbox_object(zval *session, char *username TSRMLS_DC);

__END_DECLS

#endif
