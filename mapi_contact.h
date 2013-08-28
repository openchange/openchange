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

#ifndef MAPI_CONTACT_H
#define MAPI_CONTACT_H

typedef struct mapi_contact_object
{
	zend_object	std;
	mapi_object_t	*message;
	TALLOC_CTX	*talloc_ctx;
	struct mapi_SPropValue_array	properties;
} mapi_contact_object_t;

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

PHP_METHOD(MAPIContact, __construct);
PHP_METHOD(MAPIContact, __destruct);
PHP_METHOD(MAPIContact, __get);

void MAPIContactRegisterClass(TSRMLS_D);
zval *create_contact_object(mapi_object_t *message TSRMLS_DC);

__END_DECLS

#endif
