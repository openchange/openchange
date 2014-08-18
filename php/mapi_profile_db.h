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

#ifndef MAPI_PROFILE_DB_H
#define MAPI_PROFILE_DB_H

typedef struct mapi_profile_db_object
{
	zend_object		std;
	char			*path;
	TALLOC_CTX		*talloc_ctx;
	struct mapi_context	*mapi_ctx;
	zval *children;
} mapi_profile_db_object_t;

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

PHP_METHOD(MAPIProfileDB, __construct);
PHP_METHOD(MAPIProfileDB, __destruct);
PHP_METHOD(MAPIProfileDB, path);
PHP_METHOD(MAPIProfileDB, profiles);
PHP_METHOD(MAPIProfileDB, getProfile);
PHP_METHOD(MAPIProfileDB, createAndGetProfile);
PHP_METHOD(MAPIProfileDB, debug);


void MAPIProfileDBRegisterClass(TSRMLS_D);
struct mapi_context *mapi_profile_db_get_mapi_context(zval * TSRMLS_DC);
void mapi_profile_db_remove_children_profile(zval *, zend_object_handle TSRMLS_DC);

__END_DECLS

#endif /* !MAPI_PROFILE_DB_H */
