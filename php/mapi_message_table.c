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

static zend_function_entry mapi_message_table_class_functions[] = {
 	PHP_ME(MAPIMessageTable,	__construct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIMessageTable,	summary,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMessageTable,	getMessages,	NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_message_table_ce;
static zend_object_handlers	mapi_message_table_object_handlers;

void MAPIMessageTableRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIMessageTable", mapi_message_table_class_functions);
	mapi_message_table_ce = zend_register_internal_class_ex(&ce, NULL, "mapitable" TSRMLS_CC);
	mapi_message_table_ce->create_object = mapi_table_create_handler;
	memcpy(&mapi_message_table_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	MAPITableClassSetObjectHandlers(&mapi_message_table_object_handlers);
	mapi_message_table_object_handlers.clone_obj = NULL;
}

zval *create_message_table_object(mapi_folder_type_t type, zval* folder, mapi_object_t* message_table, uint32_t count, int nProp, zval **props TSRMLS_DC)
{
	int			i;
	struct SPropTagArray	*tag_array;
	mapi_table_object_t	*new_obj;
	TALLOC_CTX         	*talloc_ctx;

	talloc_ctx =   talloc_named(NULL, 0, "message_table");
	tag_array = set_SPropTagArray(talloc_ctx, 1, PR_MID);
	for(i=0; i < nProp; i++) {
		mapi_id_t id = Z_LVAL_P(props[i]);
		SPropTagArray_add(talloc_ctx, tag_array, id);
	}

	zval *new_php_obj = create_table_object("mapimessagetable", folder, message_table,
						talloc_ctx, tag_array, count TSRMLS_CC);
	if (type == CONTACT) {
		new_obj->type = CONTACTS;
	} else if (type == APPOINTMENT) {
		new_obj->type = APPOINTMENTS;
	} else if (type == TASK) {
		new_obj->type = TASKS;
	} else {
		php_error(E_ERROR, "Message table of unknown type: %i", type);
	}

	return new_php_obj;
}

PHP_METHOD(MAPIMessageTable, __construct)
{
	php_error(E_ERROR, "The table message object should not be created directly.");
}

PHP_METHOD(MAPIMessageTable, summary)
{
	struct SRowSet		row_set;
	enum MAPISTATUS		retval;
	zval 			*summary;
	zval			*message;
	mapi_table_object_t	*this_obj;
	mapi_object_t		*obj_message;
	uint32_t		i, j;
	char			*id = NULL;
	uint32_t   		count;
	struct mapi_SPropValue_array	properties_array;

	long countParam = -1;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
			       "|l", &countParam) == FAILURE) {
		RETURN_NULL();
	}

	count = (countParam > 0) ? (uint32_t) countParam : 50;
	this_obj = (mapi_table_object_t*)  zend_object_store_get_object(getThis() TSRMLS_CC);

	MAKE_STD_ZVAL(summary);
	array_init(summary);
	while (mapi_table_next_row_set(this_obj->table, &row_set, count TSRMLS_CC)) {
		for (i = 0; i < row_set.cRows; i++) {
			zval *obj_summary;
			MAKE_STD_ZVAL(obj_summary);
			array_init(obj_summary);

			for (j=0; j < this_obj->tag_array->cValues; j++) {
				zval 		*zprop;
				void 		*prop_value;
				uint32_t 	prop_id = this_obj->tag_array->aulPropTag[j];
				uint32_t        prop_id_row =  row_set.aRow[i].lpProps[j].ulPropTag;
				if (prop_id_row != prop_id) {
					// The type has changed, probably not found or error, ignore
					continue;
				} else {
					prop_value = (void*) find_SPropValue_data(&(row_set.aRow[i]), prop_id);
					zprop      = mapi_message_property_to_zval(this_obj->talloc_ctx, prop_id, prop_value);
				}
				add_index_zval(obj_summary, prop_id, zprop);
			}

			add_next_index_zval(summary, obj_summary);
		}
		if (countParam > 0) {
			break;	// no unlimited
		}
	}

	RETURN_ZVAL(summary, 0, 1);
}

PHP_METHOD(MAPIMessageTable, getMessages)
{
	struct SRowSet		row;
	enum MAPISTATUS		retval;
	zval 			*res;
	zval			*message;
	zval			*folder;
	mapi_folder_object_t	*folder_obj;
	mapi_table_object_t	*this_obj;
	mapi_object_t		*obj_message;
	uint32_t		i;
	unsigned char open_mode = 0; //XXX

	long countParam = -1;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
			       "|l", &countParam) == FAILURE) {
		RETURN_NULL();
	}

	uint32_t count = (countParam > 0) ? (uint32_t) countParam : 50;
	this_obj = STORE_OBJECT(mapi_table_object_t*, getThis());
	folder  = this_obj->parent;
	folder_obj = STORE_OBJECT(mapi_folder_object_t*, folder);

	MAKE_STD_ZVAL(res);
	array_init(res);
	while (mapi_table_next_row_set(this_obj->table,  &row, count TSRMLS_CC)) {
		for (i = 0; i < row.cRows; i++) {
			mapi_id_t message_id = row.aRow[i].lpProps[0].value.d; // message_id
			message = mapi_folder_open_message(folder, message_id, open_mode TSRMLS_CC);
			if (message) {
				add_next_index_zval(res, message);
			}
		}

		if (countParam > 0) {
			break;	// no unlimited
		}
	}

	RETURN_ZVAL(res, 0, 1);
}
