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

#ifndef PHP_MAPI_H
#define PHP_MAPI_H 1

#include <php.h>

#include <libmapi/libmapi.h>
#include <libmapi/mapi_nameid.h>

#include <mapi_profile.h>
#include <mapi_profile_db.h>
#include <mapi_session.h>
#include <mapi_mailbox.h>

#define PHP_MAPI_VERSION "1.0"
#define PHP_MAPI_EXTNAME "mapi"

#define PROFILE_RESOURCE_NAME "Profile"
#define SESSION_RESOURCE_NAME "Session"
#define TALLOC_RESOURCE_NAME "TALLOC_CTX"

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

PHP_MINIT_FUNCTION(mapi);
PHP_MSHUTDOWN_FUNCTION(mapi);

__END_DECLS

extern zend_module_entry mapi_module_entry;

#define phpext_mapi_ptr &mapi_module_entry
#define EXPECTED_MAPI_OBJECTS 32
#define OBJ_GET_TALLOC_CTX(objType, obj) ((objType) zend_object_store_get_object(obj TSRMLS_CC))->mem_ctx;
#define OBJ_GET_TALLOC_CTX_TMP(objType, obj) ((objType) zend_object_store_get_object(obj TSRMLS_CC))->talloc_ctx;
#define add_assoc_mapi_id_t(zv, name, value) add_assoc_long(zv, name, (long) value)
#define CHECK_MAPI_RETVAL(rv, desc)		\
  if (rv != MAPI_E_SUCCESS)			\
	  php_error(E_ERROR, "%s: %s", desc, mapi_get_errstr(rv))


#endif /*! PHP_MAPI_H */
