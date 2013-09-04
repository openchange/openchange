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
	mapi_message_table_object_handlers.clone_obj = NULL;
}

zval *create_message_table_object(char *type, zval* folder, mapi_object_t* message_table, uint32_t count TSRMLS_DC)
{
	mapi_table_object_t *new_obj;
	zval *new_php_obj = create_table_object("mapimessagetable", folder, message_table, count TSRMLS_CC);

	new_obj = (mapi_table_object_t *) zend_object_store_get_object(new_php_obj TSRMLS_CC);
	if (strcmp(type, "IPF.Contact") == 0) {
		new_obj->type = CONTACTS;
	} else if (strcmp(type, "IPF.Appointment") == 0) {
		new_obj->type = APPOINTMENTS;
	} else if (strcmp(type, "IPF.Task") == 0) {
		new_obj->type = TASKS;
	} else {
		php_error(E_ERROR, "Message table of unknown type: %s", type);
	}

	return new_php_obj;
}

PHP_METHOD(MAPIMessageTable, __construct)
{
	php_error(E_ERROR, "The table message object should not be created directly.");
}


/* PHP_METHOD(MAPIMessageTable, __destruct) */
/* { */
/* 	/\* zval			*php_this_obj; *\/ */
/* 	/\* mapi_message_table_object_t	*this_obj; *\/ */
/* 	/\* php_this_obj = getThis(); *\/ */
/* 	/\* this_obj = (mapi_message_table_object_t *) zend_object_store_get_object(php_this_obj TSRMLS_CC); *\/ */

/* } */

static zval *contact_summary_zval (struct mapi_SPropValue_array *properties, const char *id)
{
	zval		*contact;
	const char	*card_name = (const char *)find_mapi_SPropValue_data(properties, PidLidFileUnder);
	const char	*email = (const char *)find_mapi_SPropValue_data(properties, PidLidEmail1OriginalDisplayName);

	MAKE_STD_ZVAL(contact);
	array_init(contact);

	add_assoc_string(contact, "id", id ? (char*) id : "", 1);
	if (card_name) {
		add_assoc_string(contact, "cardName", card_name ? (char*) card_name : "", 1);
	}
	if (email) {
		add_assoc_string(contact, "email", email ? (char*) email : "", 1);
	}
	return contact;
}

static zval *appointment_summary_zval (TALLOC_CTX *talloc_ctx, struct mapi_SPropValue_array *properties, const char *id)
{
	zval		*appointment;

	MAKE_STD_ZVAL(appointment);
	array_init(appointment);
	const char 	*subject = find_mapi_SPropValue_data(properties, PR_CONVERSATION_TOPIC);
	add_assoc_string(appointment, "id", id ? (char*) id : "", 1);
	add_assoc_string(appointment, "subject", subject ? (char*) subject : "", 1);
	add_assoc_string(appointment, "startDate", (char*) mapi_date(talloc_ctx, properties, PR_START_DATE), 0);
	add_assoc_string(appointment, "endDate", (char*) mapi_date(talloc_ctx, properties, PR_END_DATE), 0);

	return appointment;
}

static zval *task_summary_zval (struct mapi_SPropValue_array *properties, const char *id)
{
	php_error(E_ERROR, "Task summary not implemented yet");
}

zval *fetch_items(zval *php_this_obj, long countParam, bool summary TSRMLS_DC)
{
	struct SRowSet		row;
	enum MAPISTATUS		retval;
	zval 			*res;
	zval			*message;
	mapi_table_object_t	*this_obj;
	mapi_object_t		obj_message;
	uint32_t		i;
	char			*id = NULL;
	struct mapi_SPropValue_array	properties_array;

	uint32_t count = (countParam > 0) ? (uint32_t) countParam : 50;
	this_obj = STORE_OBJECT(mapi_table_object_t*, php_this_obj);

	MAKE_STD_ZVAL(res);
	array_init(res);
	while (next_row_set(this_obj->table,  &row, count TSRMLS_CC)) {
		for (i = 0; i < row.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(this_obj->folder,
					     row.aRow[i].lpProps[0].value.d,
					     row.aRow[i].lpProps[1].value.d,
					     &obj_message, 0);
			if (retval != MAPI_E_NOT_FOUND) {
				retval = GetPropsAll(&obj_message, MAPI_UNICODE, &properties_array);
				if (retval == MAPI_E_SUCCESS) {
					if (summary) {
						id = talloc_asprintf(this_obj->talloc_ctx, "%" PRIx64 "/%" PRIx64,
						     row.aRow[i].lpProps[0].value.d,
						     row.aRow[i].lpProps[1].value.d);
						mapi_SPropValue_array_named(&obj_message,
								    &properties_array);
					}

					if (this_obj->type == CONTACTS) {
						if (summary) {
							message = contact_summary_zval(&properties_array, id);
						} else {
							message = create_contact_object(php_this_obj, &obj_message TSRMLS_CC);
						}
					} else if (this_obj->type == APPOINTMENTS) {
						if (summary) {
							message = appointment_summary_zval(this_obj->talloc_ctx, &properties_array, id);
						} else {
							php_error(E_ERROR, "not implemented yet");
						}

					} else if (this_obj->type == TASKS) {
						if (summary) {
							message = task_summary_zval(&properties_array, id);
						} else {
							message = create_task_object(php_this_obj, &obj_message TSRMLS_CC);
						}

					} else {
						php_printf("Unknown folder type %i\n", this_obj->type);
					}

					if (message) {
						add_next_index_zval(res, message);
					}
					if (id) {
						talloc_free(id);
						id = NULL;
					}
				}
			}
		}

		if (countParam > 0) {
			break;	// no unlimited
		}
	}

	return res;

}

PHP_METHOD(MAPIMessageTable, summary)
{
	long countParam = -1;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
			       "|l", &countParam) == FAILURE) {
		RETURN_NULL();
	}

	zval *summary = fetch_items(getThis(), countParam, true TSRMLS_CC);
	RETURN_ZVAL(summary, 0, 1);
}

PHP_METHOD(MAPIMessageTable, getMessages)
{
	long countParam = -1;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
			       "|l", &countParam) == FAILURE) {
		RETURN_NULL();
	}

	zval *messages = fetch_items(getThis(), countParam, false TSRMLS_CC);
	RETURN_ZVAL(messages, 0, 1);
}
