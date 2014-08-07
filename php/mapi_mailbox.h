/*
   OpenChange MAPI PHP bindings

   Copyright (C) 2013 Javier Amor Garcia

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
	mapi_object_t	store;
	TALLOC_CTX	*talloc_ctx;
	const char      *username;
	zval		*parent;
	zval		*children;
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
PHP_METHOD(MAPIMailbox, openFolder);

void MAPIMailboxRegisterClass(TSRMLS_D);
zval *create_mailbox_object(zval *session, char *username TSRMLS_DC);

void init_message_store(mapi_object_t *store,
			       struct mapi_session *session,
			bool public_folder, char *username);

__END_DECLS

#endif
