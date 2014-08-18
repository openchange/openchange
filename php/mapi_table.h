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
#ifndef MAPI_TABLE_H
#define MAPI_TABLE_H


enum table_type {CONTACTS, APPOINTMENTS, TASKS, FOLDERS, ATTACHMENTS};

typedef struct mapi_table_object
{
	zend_object	std;
	mapi_object_t	*table;
	zval		*parent;
	uint32_t count;
	enum table_type	type;
	TALLOC_CTX	*talloc_ctx;
	struct SPropTagArray	*tag_array;
} mapi_table_object_t;

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

PHP_METHOD(MAPITable, __construct);
PHP_METHOD(MAPITable, __destruct);
PHP_METHOD(MAPITable, count);
PHP_METHOD(MAPITable, getParent);
PHP_METHOD(MAPITable, getParentFolder); // XXX will be deprecated in the future

void MAPITableRegisterClass(TSRMLS_D);
void mapi_table_free_storage(void *object TSRMLS_DC);
zend_object_value mapi_table_create_handler(zend_class_entry *type TSRMLS_DC);
void MAPITableClassSetObjectHandlers(zend_object_handlers *handler);
zval *create_table_object(char *class, zval* folder_php_obj, mapi_object_t *table, TALLOC_CTX *talloc_ctx, struct SPropTagArray *tag_array, uint count TSRMLS_DC);
struct SRowSet* mapi_table_next_row_set(mapi_object_t* table, struct SRowSet *row_set, uint32_t count TSRMLS_DC);


__END_DECLS

#endif
