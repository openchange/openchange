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

#ifndef MAPI_FOLDER_H
#define MAPI_FOLDER_H

typedef enum mapi_folder_type {
	TASK,
	CONTACT,
	APPOINTMENT,
	NOTE
} mapi_folder_type_t;


typedef struct mapi_folder_object
{
	zend_object	std;
	TALLOC_CTX	*talloc_ctx;
	zval		*parent;
	zval		*children;
	mapi_object_t	store;
	mapi_folder_type_t type;
} mapi_folder_object_t;




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

PHP_METHOD(MAPIFolder, __construct);
PHP_METHOD(MAPIFolder, __destruct);
PHP_METHOD(MAPIFolder, getName);
PHP_METHOD(MAPIFolder, getID);
PHP_METHOD(MAPIFolder, getFolderType);
PHP_METHOD(MAPIFolder, getFolderTable);
PHP_METHOD(MAPIFolder, getMessageTable);
PHP_METHOD(MAPIFolder, openMessage);
PHP_METHOD(MAPIFolder, createMessage);
PHP_METHOD(MAPIFolder, deleteMessages);

void MAPIFolderRegisterClass(TSRMLS_D);
zval *create_folder_object(zval *php_mailbox, mapi_id_t id, mapi_folder_type_t type TSRMLS_DC);
zval* mapi_folder_open_message(zval *folder, mapi_id_t message_id, char open_mode TSRMLS_DC);
mapi_folder_type_t mapi_folder_type_from_string(char *folder_type_str);


__END_DECLS

#endif
