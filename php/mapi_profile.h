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

#ifndef MAPI_PROFILE_H
#define MAPI_PROFILE_H

typedef struct mapi_profile_object
{
	zend_object		std;
	char			*path;
	TALLOC_CTX		*talloc_ctx;
	zval			*parent;
	zval			*children;
	struct mapi_profile	*profile;
} mapi_profile_object_t;

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

PHP_METHOD(MAPIProfile, __construct);
PHP_METHOD(MAPIProfile, __destruct);
PHP_METHOD(MAPIProfile, dump);
PHP_METHOD(MAPIProfile, logon);

void MAPIProfileRegisterClass(TSRMLS_D);
zval *create_profile_object(struct mapi_profile *, zval *, TALLOC_CTX *  TSRMLS_DC);
struct mapi_profile *mapi_profile_get_profile(zval * TSRMLS_DC);
struct mapi_context *profile_get_mapi_context(zval * TSRMLS_DC);
zval *mapi_profile_logon(zval *z_profile TSRMLS_DC);

__END_DECLS

#endif
