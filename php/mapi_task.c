/*
   OpenChange MAPI PHP bindings

   Copyright (C) 2013 Javier Amor Garcia.

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

#include <php_mapi.h>

static zend_function_entry mapi_task_class_functions[] = {
	PHP_ME(MAPITask,	__construct,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPITask,	__destruct,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)

	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_task_ce;
static zend_object_handlers	mapi_task_object_handlers;

void MAPITaskRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPITask", mapi_task_class_functions);
	mapi_task_ce = zend_register_internal_class_ex(&ce, NULL, "mapimessage" TSRMLS_CC);

	mapi_task_ce->create_object = mapi_message_create_handler;

	memcpy(&mapi_task_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	mapi_task_object_handlers.clone_obj = NULL;
}

zval *create_task_object(zval *folder, mapi_object_t *message, char open_mode TSRMLS_DC)
{
	zval *task = create_message_object("mapitask", folder, message, open_mode TSRMLS_CC);
	mapi_message_request_all_properties(task TSRMLS_CC);
	return task;
}

PHP_METHOD(MAPITask, __construct)
{
	php_error(E_ERROR, "The task object should not created directly.");
}


PHP_METHOD(MAPITask, __destruct)
{
}
