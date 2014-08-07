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

static zend_function_entry mapi_attachment_class_functions[] = {
	PHP_ME(MAPIAttachment,	__construct,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIAttachment,	__destruct,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_ME(MAPIAttachment,	save,		NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)

	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_attachment_ce;
static zend_object_handlers	mapi_attachment_object_handlers;

void MAPIAttachmentRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIAttachment", mapi_attachment_class_functions);
	mapi_attachment_ce = zend_register_internal_class_ex(&ce, NULL, "mapimessage" TSRMLS_CC);

	mapi_attachment_ce->create_object = mapi_message_create_handler;

	memcpy(&mapi_attachment_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	mapi_attachment_object_handlers.clone_obj = NULL;
}

zval *create_attachment_object(zval *message, mapi_object_t *attachment TSRMLS_DC)
{
	zval *z_attachment;
	mapi_message_object_t   *parent;

	parent = (mapi_message_object_t *) zend_object_store_get_object(message TSRMLS_CC);
	z_attachment = create_message_object("mapiattachment", message, attachment, parent->open_mode TSRMLS_CC);
	mapi_message_request_all_properties(z_attachment TSRMLS_CC);
	return z_attachment;
}

PHP_METHOD(MAPIAttachment, __construct)
{
	php_error(E_ERROR, "The attachment object should not created directly.");
}


PHP_METHOD(MAPIAttachment, __destruct)
{
}

PHP_METHOD(MAPIAttachment, save)
{
	php_error(E_ERROR, "Save attachment not yet supported in the backend");

	zval			*z_this_obj;
	mapi_message_object_t	*this_obj;
	zval			*z_parent;
	mapi_message_object_t   *parent;
	enum MAPISTATUS         res;

	z_this_obj = getThis();
	this_obj = (mapi_message_object_t *) zend_object_store_get_object(z_this_obj TSRMLS_CC);
	if ((this_obj->open_mode != 1) && (this_obj->open_mode != 3)) {
		php_error(E_ERROR, "Trying to save a attachment from a non-RW message");
	}

	z_parent = this_obj->parent;
	parent = (mapi_message_object_t *) zend_object_store_get_object(z_parent TSRMLS_CC);

	res = SaveChangesAttachment(parent->message, this_obj->message, KeepOpenReadWrite);
	CHECK_MAPI_RETVAL(res, "SaveChangesAttachment");
}

