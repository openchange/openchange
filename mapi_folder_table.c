#include <php_mapi.h>

static zend_function_entry mapi_folder_table_class_functions[] = {
 	PHP_ME(MAPIFolderTable,	__construct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIFolderTable,	summary,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIFolderTable,	getFolders,	NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_folder_table_ce;
static zend_object_handlers	mapi_folder_table_object_handlers;

void MAPIFolderTableRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIFolderTable", mapi_folder_table_class_functions);
	mapi_folder_table_ce = zend_register_internal_class_ex(&ce, NULL, "mapitable" TSRMLS_CC);
	mapi_folder_table_ce->create_object = mapi_table_create_handler;
	memcpy(&mapi_folder_table_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	MAPITableClassSetObjectHandlers(&mapi_folder_table_object_handlers);
	mapi_folder_table_object_handlers.clone_obj = NULL;
}

zval *create_folder_table_object(zval* parent, mapi_object_t* folder_table, uint32_t count TSRMLS_DC)
{
	int			i;
	struct SPropTagArray	*tag_array;
	mapi_table_object_t	*new_obj;
	TALLOC_CTX         	*mem_ctx;

	mem_ctx =   talloc_named(NULL, 0, "directory_table");
	tag_array = set_SPropTagArray(mem_ctx, 6,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_FID,
					  PR_COMMENT_UNICODE,
					  PR_CONTENT_UNREAD,
					  PR_CONTENT_COUNT,
					  PR_FOLDER_CHILD_COUNT);


	zval *new_php_obj = create_table_object("mapifoldertable", parent, folder_table,
						mem_ctx, tag_array, count TSRMLS_CC);
	new_obj->type = FOLDERS;

	return new_php_obj;
}

PHP_METHOD(MAPIFolderTable, __construct)
{
	php_error(E_ERROR, "The directory message object should not be created directly.");
}

PHP_METHOD(MAPIFolderTable, summary)
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
	while (mapi_table_next_row_set(this_obj->table,  &row_set, count TSRMLS_CC)) {
		for (i = 0; i < row_set.cRows; i++) {
			zval *obj_summary;
			MAKE_STD_ZVAL(obj_summary);
			array_init(obj_summary);

			// start  at 2 because two first are fid and mid
			for (j=2; j < this_obj->tag_array->cValues; j++) {
				zval 		*zprop;
				void 		*prop_value;
				uint32_t 	prop_id = this_obj->tag_array->aulPropTag[j];
				uint32_t        prop_id_row =  row_set.aRow[i].lpProps[j].ulPropTag;
				if (prop_id_row != prop_id) {
					// The type has changed, probably not found or error, use null
					ZVAL_NULL(zprop);
				} else {
					prop_value = (void*) find_SPropValue_data(&(row_set.aRow[i]), prop_id_row);
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

PHP_METHOD(MAPIFolderTable, getFolders)
{
	php_error(E_ERROR, "Not implemented");

	long countParam = -1;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
			       "|l", &countParam) == FAILURE) {
		RETURN_NULL();
	}

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


	uint32_t count = (countParam > 0) ? (uint32_t) countParam : 50;
	this_obj = STORE_OBJECT(mapi_table_object_t*, getThis());
	folder  = this_obj->parent;
	folder_obj = STORE_OBJECT(mapi_folder_object_t*, folder);

	MAKE_STD_ZVAL(res);
	array_init(res);
	while (mapi_table_next_row_set(this_obj->table,  &row, count TSRMLS_CC)) {
		for (i = 0; i < row.cRows; i++) {
			mapi_id_t message_id = row.aRow[i].lpProps[1].value.d; // message_id
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
