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

#ifndef MAPI_MESSAGE_TABLE_H
#define MAPI_MESSAGE_TABLE_H

/* typedef struct mapi_message_table_object */
/* { */
/* 	zend_object	std; */
/* 	mapi_object_t	*message_table; */
/* 	TALLOC_CTX	*talloc_ctx; */
/* } mapi_message_table_object_t; */

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

PHP_METHOD(MAPIMessageTable, __construct);
/* PHP_METHOD(MAPIMessage_Table, __destruct); */
 PHP_METHOD(MAPIMessageTable, summary);

void MAPIMessageTableRegisterClass(TSRMLS_D);

zval *create_message_table_object(char *type, zval* folder, mapi_object_t* message_table, uint32_t count TSRMLS_DC);

//struct SRowSet* next_row_set(zval *obj  TSRMLS_DC);


__END_DECLS

#endif
