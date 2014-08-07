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

static zend_function_entry mapi_attachment_table_class_functions[] = {
 	PHP_ME(MAPIAttachmentTable,	__construct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIAttachmentTable,	getAttachments,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIAttachmentTable,	getParentFolder,NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIAttachmentTable,   count,          NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_attachment_table_ce;
static zend_object_handlers	mapi_attachment_table_object_handlers;

void MAPIAttachmentTableRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIAttachmentTable", mapi_attachment_table_class_functions);
	mapi_attachment_table_ce = zend_register_internal_class_ex(&ce, NULL, "mapitable" TSRMLS_CC);
	mapi_attachment_table_ce->create_object = mapi_table_create_handler;
	memcpy(&mapi_attachment_table_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	MAPITableClassSetObjectHandlers(&mapi_attachment_table_object_handlers);
	mapi_attachment_table_object_handlers.clone_obj = NULL;
}

zval *create_attachment_table_object(zval* parent, mapi_object_t* attachment_table TSRMLS_DC)
{
	int			i;
	struct SPropTagArray	*tag_array;
	mapi_table_object_t	*new_obj;
	TALLOC_CTX         	*mem_ctx;

	mem_ctx =   talloc_named(NULL, 0, "attachment_table");
	tag_array = set_SPropTagArray(mem_ctx, 1,  PR_ATTACH_NUM);

	zval *new_php_obj = create_table_object("mapiattachmenttable", parent, attachment_table,
						mem_ctx, tag_array, 0 TSRMLS_CC);
	new_obj = (mapi_table_object_t*)  zend_object_store_get_object(new_php_obj TSRMLS_CC);
	new_obj->type = ATTACHMENTS;

	return new_php_obj;
}

PHP_METHOD(MAPIAttachmentTable, __construct)
{
	php_error(E_ERROR, "The attachment message object should not be created directly.");
}



PHP_METHOD(MAPIAttachmentTable, getParentFolder)
{
	php_error(E_ERROR, "This method cannot be called in an attachment table");
}


PHP_METHOD(MAPIAttachmentTable, getAttachments)
{
	struct SRowSet		row_set;
	enum MAPISTATUS		retval;
	zval 			*res;
	uint32_t		i;
	const uint32_t		*attach_num;
	long 			countParam;
	uint32_t		count;
	zval                    *message;
	zval			*attachment;
	mapi_table_object_t     *this_obj;


	countParam = -1;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
			       "|l", &countParam) == FAILURE) {
		RETURN_NULL();
	}

	count = (countParam > 0) ? (uint32_t) countParam : 50;
	this_obj = THIS_STORE_OBJECT(mapi_table_object_t*);
	message  = this_obj->parent;

	MAKE_STD_ZVAL(res);
	array_init(res);

	while (mapi_table_next_row_set(this_obj->table, &row_set, count TSRMLS_CC)) {
		for (i = 0; i < row_set.cRows; i++) {
			attach_num = (const uint32_t *)find_SPropValue_data(&(row_set.aRow[i]), PR_ATTACH_NUM);
			attachment = mapi_message_get_attachment(message, *attach_num TSRMLS_CC);
			if (attachment) {
				add_next_index_zval(res, attachment);
			}
		}

		if (countParam > 0) {
			break;	// no unlimited
		}
	}

	RETURN_ZVAL(res, 0, 1);
}

PHP_METHOD(MAPIAttachmentTable, count)
{
	php_error(E_ERROR, "This method is not available for attachments tables");
}
