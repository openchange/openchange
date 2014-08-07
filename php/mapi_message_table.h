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

#ifndef MAPI_MESSAGE_TABLE_H
#define MAPI_MESSAGE_TABLE_H

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
PHP_METHOD(MAPIMessageTable, summary);
PHP_METHOD(MAPIMessageTable, getMessages);

void MAPIMessageTableRegisterClass(TSRMLS_D);

zval *create_message_table_object(mapi_folder_type_t type, zval* folder, mapi_object_t* message_table, uint32_t count, int nProp, zval **props TSRMLS_DC);

__END_DECLS

#endif
