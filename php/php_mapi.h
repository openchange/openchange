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

#ifndef PHP_OPENCHANGE_H
#define PHP_OPENCHANGE_H 1


#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <float.h>

#include <php.h>

#include <libmapi/libmapi.h>
#include <libmapi/mapi_nameid.h>

#include <mapi_profile.h>
#include <mapi_profile_db.h>
#include <mapi_session.h>
#include <mapi_mailbox.h>
#include <mapi_folder.h>
#include <mapi_message.h>
#include <mapi_contact.h>
#include <mapi_appointment.h>
#include <mapi_task.h>
#include <mapi_attachment.h>
#include <mapi_table.h>
#include <mapi_message_table.h>
#include <mapi_folder_table.h>
#include <mapi_attachment_table.h>
#include <mapi_exception.h>

#define PHP_OPENCHANGE_VERSION "1.0"
#define PHP_OPENCHANGE_EXTNAME "openchange"

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

PHP_MINIT_FUNCTION(openchange);
PHP_MSHUTDOWN_FUNCTION(openchange);

__END_DECLS

extern zend_module_entry openchange_module_entry;

#define phpext_openchange_ptr &openchange_module_entry
#define EXPECTED_MAPI_OBJECTS 32
#define OBJ_GET_TALLOC_CTX(objType, obj) ((objType) zend_object_store_get_object(obj TSRMLS_CC))->talloc_ctx;
#define add_assoc_mapi_id_t(zv, name, value) add_assoc_long(zv, name, (long) value)
#define CHECK_MAPI_RETVAL(rv, desc)		\
  if (rv != MAPI_E_SUCCESS)			\
	  php_error(E_ERROR, "%s: %s", desc, mapi_get_errstr(rv))

#define STORE_OBJECT(type, zv) (type) zend_object_store_get_object(zv TSRMLS_CC)
#define THIS_STORE_OBJECT(type) STORE_OBJECT(type, getThis())

#define STORE_OBJECT_CHILDREN(store_object)  store_object ? (store_object->children ? store_object->children : NULL) : NULL
#define ADD_CHILD(parent_store, zchild)
#define REMOVE_CHILD(parent_store, zchild)
//#define DESTROY_CHILDRENS(obj)
//#define ADD_CHILD(parent_store, zchild) add_index_zval(STORE_OBJECT_CHILDREN(parent_store), (ulong) Z_OBJ_HANDLE_P(zchild), zchild)
//#define REMOVE_CHILD(parent_store, zchild) zval *_tmp_children = STORE_OBJECT_CHILDREN(parent_store); \
//                                           if (_tmp_children) zend_hash_index_del(_tmp_children->value.ht, (long) Z_OBJ_HANDLE_P(zchild))
#define DESTROY_CHILDRENS(obj)  if (obj->children) { \
                                       zval *tmp_children = obj->children;	     \
					obj->children = NULL;		\
					zval_dtor(tmp_children);	\
					FREE_ZVAL(tmp_children);	\
	                        }

void exception_from_status(enum MAPISTATUS status, const char *failed_action TSRMLS_DC);
#define MAPI_ID_STR_SIZE  19*sizeof(char) // 0x + 64/4 + NUL char

char *mapi_id_to_str(mapi_id_t id);
mapi_id_t str_to_mapi_id(const char *str);



ZEND_BEGIN_ARG_INFO_EX(php_method_one_args, 0, 0, 1)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(php_method_two_args, 0, 0, 2)
ZEND_END_ARG_INFO()

#define UNUSED_PARAM -1 // macro for non-used parameter in zend_get_parameters_array


#endif /*! PHP_OPENCHANGE_H */
