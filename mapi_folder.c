#include <php_mapi.h>

static zend_function_entry mapi_folder_class_functions[] = {
	PHP_ME(MAPIFolder,	__construct,		NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIFolder,	__destruct,		NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_ME(MAPIFolder,	getFolderType,		NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIFolder,	getName,		NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIFolder,	getID,			NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIFolder,	getFolderTable,		NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIFolder,	getMessageTable,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIFolder,	openMessage,		NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPIFolder,	createMessage,		NULL, ZEND_ACC_PUBLIC)

	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_folder_ce;
static zend_object_handlers	mapi_folder_object_handlers;

static void mapi_folder_free_storage(void *object TSRMLS_DC)
{
	mapi_folder_object_t	*obj;

	obj = (mapi_folder_object_t *) object;
	if (obj->talloc_ctx) {
		talloc_free(obj->talloc_ctx);
	}

	mapi_object_release(&(obj->store));

	zend_object_std_dtor(&(obj->std) TSRMLS_CC);
	efree(obj);
}


static zend_object_value mapi_folder_create_handler(zend_class_entry *type TSRMLS_DC)
{
	zval			*tmp;
	zend_object_value	retval;
	mapi_folder_object_t	*obj;

	obj = (mapi_folder_object_t *) emalloc(sizeof(mapi_folder_object_t));
	memset(obj, 0, sizeof(mapi_folder_object_t));

	obj->std.ce = type;

	ALLOC_HASHTABLE(obj->std.properties);
	zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
#if PHP_VERSION_ID < 50399
	zend_hash_copy(obj->std.properties, &type->default_properties,
		       (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
#else
	object_properties_init((zend_object *) &(obj->std), type);
#endif
	retval.handle = zend_objects_store_put(obj, NULL, mapi_folder_free_storage,
					       NULL TSRMLS_CC);
	retval.handlers = &mapi_folder_object_handlers;

	MAKE_STD_ZVAL(obj->children);
	array_init(obj->children);

	return retval;
}

void MAPIFolderRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIFolder", mapi_folder_class_functions);
	mapi_folder_ce = zend_register_internal_class(&ce TSRMLS_CC);
	mapi_folder_ce->create_object = mapi_folder_create_handler;
	memcpy(&mapi_folder_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	mapi_folder_object_handlers.clone_obj = NULL;
}

zval *create_folder_object(zval *php_mailbox, mapi_id_t id, char *folder_type TSRMLS_DC)
{
	enum MAPISTATUS		retval;
	zval			*new_php_obj;
	mapi_mailbox_object_t	*mailbox;
	mapi_folder_object_t	*new_obj;
	zend_class_entry	**ce;

	MAKE_STD_ZVAL(new_php_obj);
	if (zend_hash_find(EG(class_table),"mapifolder", sizeof("mapifolder"),(void**)&ce) == FAILURE) {
		php_error(E_ERROR, "Class MAPIFolder does not exist.");
	}
	object_init_ex(new_php_obj, *ce);

	new_obj = (mapi_folder_object_t *) zend_object_store_get_object(new_php_obj TSRMLS_CC);

	// first try to open the folder
	mapi_object_init(&(new_obj->store));
	mailbox = (mapi_mailbox_object_t *) zend_object_store_get_object(php_mailbox TSRMLS_CC);
	retval = OpenFolder(&(mailbox->store), id, &(new_obj->store));
	if (retval == MAPI_E_NOT_FOUND) {
		mapi_object_release(&(new_obj->store));
		return NULL;
	}
	CHECK_MAPI_RETVAL(retval, "Open folder");

	new_obj->parent = php_mailbox;
	new_obj->talloc_ctx = talloc_named(NULL, 0, "folder");

	if (strncmp(folder_type, "IPF.Contact", 20) == 0) {
		new_obj->type = CONTACT;
	} else if (strncmp(folder_type, "IPF.Task", 20) == 0) {
		new_obj->type = TASK;
	} else if (strncmp(folder_type, "IPF.Appointment", 20) == 0) {
		new_obj->type = APPOINTMENT;
	} else if (strncmp(folder_type, "IPF.Note", 20) == 0) {
		new_obj->type = NOTE;
	} else {
		php_error(E_ERROR, "Unknow folder type: %i", new_obj->type);
	}

	return new_php_obj;
}

PHP_METHOD(MAPIFolder, __construct)
{
	php_error(E_ERROR, "The folder object should not created directly.\n" \
		  "Use the  methods in the mailbox object");
}

PHP_METHOD(MAPIFolder, __destruct)
{
	zval *this_zval = getThis();
	mapi_folder_object_t *obj = (mapi_folder_object_t*) zend_object_store_get_object(this_zval TSRMLS_CC);
	DESTROY_CHILDRENS(obj);
	mapi_mailbox_object_t* obj_parent =  (mapi_mailbox_object_t*) zend_object_store_get_object(obj->parent TSRMLS_CC);
	REMOVE_CHILD(obj_parent, this_zval);
}

PHP_METHOD(MAPIFolder, getName)
{
	enum MAPISTATUS		retval;
	const char		*folder_name;
	TALLOC_CTX		*this_obj_talloc_ctx;
	TALLOC_CTX		*talloc_ctx;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		cValues;
	zval                    *this_php_obj;
	mapi_folder_object_t	*this_obj;

	this_php_obj = getThis();
	this_obj = (mapi_folder_object_t *) zend_object_store_get_object(this_php_obj TSRMLS_CC);
	this_obj_talloc_ctx = OBJ_GET_TALLOC_CTX(mapi_folder_object_t *, this_php_obj);
	talloc_ctx = talloc_named(this_obj_talloc_ctx, 0, "MAPIFolder::getName");

	/* Retrieve the folder folder name */
	SPropTagArray = set_SPropTagArray(talloc_ctx, 0x1, PR_DISPLAY_NAME_UNICODE);
	retval = GetProps(&this_obj->store, MAPI_UNICODE, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	CHECK_MAPI_RETVAL(retval, "Get folder properties");

	/* FIXME: Use accessor instead of direct access */
	if (lpProps[0].value.lpszW) {
		folder_name = lpProps[0].value.lpszW;
	} else {
		talloc_free(talloc_ctx);
		RETURN_NULL();
	}

        RETVAL_STRING(folder_name, 1);
	talloc_free(talloc_ctx);

	return;
}

PHP_METHOD(MAPIFolder, getID)
{
	char *str_id;
	mapi_folder_object_t *obj = THIS_STORE_OBJECT(mapi_folder_object_t*);
	str_id = mapi_id_to_str(obj->store.id);
	RETURN_STRING(str_id, 0);
}


PHP_METHOD(MAPIFolder, getFolderType)
{
	zval                    *this_php_obj;
	mapi_folder_object_t	*this_obj;

	this_php_obj = getThis();
	this_obj = (mapi_folder_object_t *) zend_object_store_get_object(this_php_obj TSRMLS_CC);
	switch (this_obj->type) {
	case TASK:
		RETURN_STRING("IPF.Task", 1);
	case CONTACT:
		RETURN_STRING("IPF.Contact", 1);
	case APPOINTMENT:
		RETURN_STRING("IPF.Appointment", 1);
	case NOTE:
		RETURN_STRING("IPF.Note", 1);
	}

	php_error(E_ERROR, "Unknown folder type: %i", this_obj->type);
}

PHP_METHOD(MAPIFolder, getFolderTable)
{
	php_error(E_ERROR, "Not implemented");
}

PHP_METHOD(MAPIFolder, getMessageTable)
{
	enum MAPISTATUS		retval;
	mapi_object_t		*obj_table;
	zval                    *this_php_obj;
	mapi_folder_object_t	*this_obj;
	uint32_t		count;
	zval			*res;

	this_php_obj = getThis();
	this_obj = (mapi_folder_object_t *) zend_object_store_get_object(this_php_obj TSRMLS_CC);

	obj_table = emalloc(sizeof(mapi_object_t));
	mapi_object_init(obj_table);
	retval = GetContentsTable(&(this_obj->store), obj_table, 0, &count);
	CHECK_MAPI_RETVAL(retval, "getMessageTable");

	res = create_message_table_object(this_obj->type, this_php_obj, obj_table, count TSRMLS_CC);
	RETURN_ZVAL(res, 0, 1);
}

PHP_METHOD(MAPIFolder, openMessage)
{
	enum MAPISTATUS		retval;
	zval			*php_this_obj;
	mapi_folder_object_t	*this_obj;
	mapi_id_t		message_id;
	char			*id_str;
	size_t			id_str_len;
	mapi_object_t		*message;
	zval                    *php_message;
	long                    open_mode = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l",
				  &id_str, &id_str_len, &open_mode) == FAILURE) {
		php_error(E_ERROR, "Missing message ID");
	}
	if ((open_mode != 0) && (open_mode != 1)) {
		php_error(E_ERROR, "Invalid open mode:%i", (int) open_mode);
	}

	message_id = str_to_mapi_id(id_str);

	php_this_obj = getThis();
	this_obj = (mapi_folder_object_t *) zend_object_store_get_object(php_this_obj TSRMLS_CC);
	message = (mapi_object_t*) emalloc(sizeof(mapi_object_t));
	mapi_object_init(message);

	retval = OpenMessage(&(this_obj->store), this_obj->store.id, message_id, message, open_mode);
	if (retval == MAPI_E_NOT_FOUND) {
		mapi_object_release(message);
		efree(message);
		RETURN_NULL();
	}
	CHECK_MAPI_RETVAL(retval, "Open message");

	switch(this_obj->type) {
	case CONTACT:
		php_message = create_contact_object(php_this_obj, message, (char) open_mode TSRMLS_CC);
		break;
	case TASK:
		php_message = create_task_object(php_this_obj, message, (char) open_mode TSRMLS_CC);
		break;
	case APPOINTMENT:
		php_message = create_appointment_object(php_this_obj, message, (char) open_mode TSRMLS_CC);
		break;
	default:
		php_error(E_ERROR, "Unknow folder type: %i", this_obj->type);
	}

	ADD_CHILD(this_obj, php_message);

	RETURN_ZVAL(php_message, 0, 1);
}

PHP_METHOD(MAPIFolder, createMessage)
{
	int 			argc = ZEND_NUM_ARGS();
	zval 			***args;
	enum MAPISTATUS		retval;
	zval 			*this_zval;
	zval			*message_zval;
	mapi_folder_object_t	*this_obj;
	mapi_object_t		*message;
	char 			open_mode = 3;

	args = (zval ***)safe_emalloc(argc, sizeof(zval **), 0);
	if (ZEND_NUM_ARGS() == 0 ||
	    zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	this_zval = getThis();
	this_obj = (mapi_folder_object_t *) zend_object_store_get_object(this_zval TSRMLS_CC);
	message = (mapi_object_t*) emalloc(sizeof(mapi_object_t));
	mapi_object_init(message);

	retval = CreateMessage(&(this_obj->store), message);
	CHECK_MAPI_RETVAL(retval, "Create message");

	switch(this_obj->type) {
	case CONTACT:
		message_zval = create_contact_object(this_zval, message, (char) open_mode TSRMLS_CC);
		break;
	case TASK:
		message_zval = create_task_object(this_zval, message, (char) open_mode TSRMLS_CC);
		break;
	case APPOINTMENT:
		message_zval = create_appointment_object(this_zval, message, (char) open_mode TSRMLS_CC);
		break;
	default:
		php_error(E_ERROR, "Unknow folder type: %i", this_obj->type);
	}

	mapi_message_set_properties(message_zval, argc, args TSRMLS_CC);

	mapi_mailbox_object_t *mailbox_obj = (mapi_mailbox_object_t*)  zend_object_store_get_object(this_obj->parent TSRMLS_CC);
	retval = SaveChangesMessage(&(mailbox_obj->store), message, KeepOpenReadOnly);
	CHECK_MAPI_RETVAL(retval, "Saving new message");

	efree(args);

	ADD_CHILD(this_obj, message_zval);
	RETURN_ZVAL(message_zval, 0, 1);
}


