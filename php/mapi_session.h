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

#ifndef MAPI_SESSION_H
#define MAPI_SESSION_H

typedef struct mapi_session_object
{
	zend_object		std;
	char			*path;
	TALLOC_CTX		*talloc_ctx;
	zval			*parent;
	zval			*children;
	struct mapi_session	*session;
} mapi_session_object_t;

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

PHP_METHOD(MAPISession, __construct);
PHP_METHOD(MAPISession, __destruct);
PHP_METHOD(MAPISession, mailbox);

void MAPISessionRegisterClass(TSRMLS_D);
zval *create_session_object(struct mapi_session *, zval *, TALLOC_CTX * TSRMLS_DC);
void mapi_mailbox_remove_children_mailbox(zval *mapi_mailbox, zend_object_handle mailbox_handle TSRMLS_DC);
struct mapi_session *mapi_session_get_session(zval *php_obj TSRMLS_DC);

__END_DECLS

#endif
