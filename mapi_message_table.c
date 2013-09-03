#include <php_mapi.h>

static zend_function_entry mapi_message_table_class_functions[] = {
 	PHP_ME(MAPIMessageTable,	__construct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIMessageTable,	summary,	NULL, ZEND_ACC_PUBLIC)
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

zval *create_message_table_object(char *type, mapi_object_t* folder, mapi_object_t* message_table, uint32_t count TSRMLS_DC)
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

/* struct SRowSet* next_row_set(zval *obj TSRMLS_DC) */
/* { */
/* 	mapi_message_table_object_t	*store_obj; */
/* 	enum MAPISTATUS		retval; */
/* 	struct SRowSet 		*row_set; */
/* 	row_set = emalloc(sizeof(struct SRowSet)); */

/* 	store_obj = STORE_OBJECT(mapi_message_table_object_t*, obj); */

/* 	retval = QueryRows(store_obj->message_table, 0x32, TBL_ADVANCE, row_set); */
/* 	if ((retval !=  MAPI_E_NOT_FOUND) || (!row_set->cRows)) { */
/* 		return NULL; */
/* 	} */
/* 	CHECK_MAPI_RETVAL(retval, "Next row set"); */

/* 	return row_set; */
/* } */


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
	add_assoc_string(appointment, "subject", subject ? (char*) subject : "", 1);
	add_assoc_string(appointment, "startDate", (char*) mapi_date(talloc_ctx, properties, PR_START_DATE), 0);
	add_assoc_string(appointment, "endDate", (char*) mapi_date(talloc_ctx, properties, PR_END_DATE), 0);

	return appointment;
}

static zval *task_summary_zval (struct mapi_SPropValue_array *properties, const char *id)
{
	php_error(E_ERROR, "Task summary not implemented yet");
}

PHP_METHOD(MAPIMessageTable, summary)
{
	struct SRowSet		row;
	enum MAPISTATUS		retval;
	zval 			*php_this_obj;
	zval 			*res;
	zval			*summary;
	mapi_table_object_t	*this_obj;
	mapi_object_t		obj_message;
	uint32_t		i;
	char			*id;
	struct mapi_SPropValue_array	properties_array;

	php_this_obj = getThis();
	this_obj = STORE_OBJECT(mapi_table_object_t*, php_this_obj);

	MAKE_STD_ZVAL(res);
	array_init(res);
	while (next_row_set(this_obj->table,  &row TSRMLS_CC)) {
		for (i = 0; i < row.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(this_obj->folder,
					     row.aRow[i].lpProps[0].value.d,
					     row.aRow[i].lpProps[1].value.d,
					     &obj_message, 0);
			if (retval != MAPI_E_NOT_FOUND) {
				retval = GetPropsAll(&obj_message, MAPI_UNICODE, &properties_array);
				if (retval == MAPI_E_SUCCESS) {
					id = talloc_asprintf(this_obj->talloc_ctx, "%" PRIx64 "/%" PRIx64,
						     row.aRow[i].lpProps[0].value.d,
						     row.aRow[i].lpProps[1].value.d);
					mapi_SPropValue_array_named(&obj_message,
								    &properties_array);
					if (this_obj->type == CONTACTS) {
						summary = contact_summary_zval(&properties_array, id);
					} else if (this_obj->type == APPOINTMENTS) {
						summary = appointment_summary_zval(this_obj->talloc_ctx, &properties_array, id);
					} else if (this_obj->type == TASKS) {
						summary = task_summary_zval(&properties_array, id);
					} else {
						php_printf("Unknown folder type %i\n", this_obj->type);
					}

					if (summary) {
						add_next_index_zval(res, summary);
					}
					talloc_free(id);

				}
			}

		}

		// end while
	}

	RETURN_ZVAL(res, 0, 0);
}

