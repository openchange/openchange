#include <php_mapi.h>

static zend_function_entry mapi_message_class_functions[] = {
	PHP_ME(MAPIMessage,	__construct,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIMessage,	__destruct,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_ME(MAPIMessage,	getID,	        NULL,		 	ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMessage,	get,	        NULL,		 	ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMessage,	set,	        NULL,		 	ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMessage,	save,	        NULL,           	ZEND_ACC_PUBLIC)

	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_message_ce;
static zend_object_handlers	mapi_message_object_handlers;

extern zend_class_entry		*mapi_mailbox_ce;

static void mapi_message_free_storage(void *object TSRMLS_DC)
{
	mapi_message_object_t	*obj;

	obj = (mapi_message_object_t *) object;
	if (obj->talloc_ctx) {
		talloc_free(obj->talloc_ctx);
	}
	if (obj->message) {
		mapi_object_release(obj->message);
		efree(obj->message);
	}

	zend_object_std_dtor(&(obj->std) TSRMLS_CC);
	efree(obj);
}

zend_object_value mapi_message_create_handler(zend_class_entry *type TSRMLS_DC)
{
	zval			*tmp;
	zend_object_value	retval;
	mapi_message_object_t	*obj;

	obj = (mapi_message_object_t *) emalloc(sizeof(mapi_message_object_t));
	memset(obj, 0, sizeof(mapi_message_object_t));

	obj->std.ce = type;

	ALLOC_HASHTABLE(obj->std.properties);
	zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
#if PHP_VERSION_ID < 50399
	zend_hash_copy(obj->std.properties, &type->default_properties,
		       (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
#else
	object_properties_init((zend_object *) &(obj->std), type);
#endif
	retval.handle = zend_objects_store_put(obj, NULL, mapi_message_free_storage,
					       NULL TSRMLS_CC);
	retval.handlers = &mapi_message_object_handlers;

	return retval;
}

void MAPIMessageRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIMessage", mapi_message_class_functions);
	mapi_message_ce = zend_register_internal_class(&ce TSRMLS_CC);
	mapi_message_ce->create_object = mapi_message_create_handler;
	memcpy(&mapi_message_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	mapi_message_object_handlers.clone_obj = NULL;

	zend_declare_class_constant_long(mapi_message_ce, ZEND_STRL("RO"), 1 TSRMLS_CC);
	zend_declare_class_constant_long(mapi_message_ce, ZEND_STRL("RW"), 1 TSRMLS_CC);

}

zval *create_message_object(char *class, zval *folder, mapi_object_t *message, char open_mode TSRMLS_DC)
{
	enum MAPISTATUS		retval;
	zval 			*new_php_obj;
	mapi_message_object_t 	*new_obj;
	zend_class_entry	**ce;

	MAKE_STD_ZVAL(new_php_obj);
	if (zend_hash_find(EG(class_table), class, strlen(class)+1,(void**)&ce) == FAILURE) {
		php_error(E_ERROR, "Create_message_object: class '%s' does not exist.", class);
	}
	object_init_ex(new_php_obj, *ce);

	new_obj = (mapi_message_object_t *) zend_object_store_get_object(new_php_obj TSRMLS_CC);
	new_obj->message = message;
	new_obj->talloc_ctx = talloc_named(NULL, 0, "message");
	new_obj->parent = folder;
	new_obj->open_mode = open_mode;

	retval = GetPropsAll(new_obj->message, MAPI_UNICODE, &(new_obj->properties));
        CHECK_MAPI_RETVAL(retval, "Getting message properties");

	mapi_SPropValue_array_named(new_obj->message,  &(new_obj->properties));

	return new_php_obj;
}

PHP_METHOD(MAPIMessage, __construct)
{
	php_error(E_ERROR, "This a base class and cannot created directly.");
}


PHP_METHOD(MAPIMessage, __destruct)
{
	zval *this_zval = getThis();
	mapi_message_object_t *this_obj = (mapi_message_object_t *) zend_object_store_get_object(this_zval TSRMLS_CC);
	mapi_folder_object_t* obj_parent =  (mapi_folder_object_t*) zend_object_store_get_object(this_obj->parent TSRMLS_CC);
	REMOVE_CHILD(obj_parent, this_zval);
}

PHP_METHOD(MAPIMessage, getID)
{
	char 			*str_id;
	mapi_id_t               mid;

	mid    = mapi_message_get_id(getThis() TSRMLS_CC);
	str_id = mapi_id_to_str(mid);
	RETURN_STRING(str_id, 0);
}

mapi_id_t mapi_message_get_id(zval *message TSRMLS_DC)
{
	uint32_t		count;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	mapi_id_t		*mid;
	mapi_message_object_t *msg_obj = STORE_OBJECT(mapi_message_object_t*, message);

	SPropTagArray = set_SPropTagArray(msg_obj->talloc_ctx, 0x1, PR_MID);
	GetProps(msg_obj->message, 0, SPropTagArray, &lpProps, &count);
	MAPIFreeBuffer(SPropTagArray);
	CHECK_MAPI_RETVAL(GetLastError(), "getID");

	mid = (mapi_id_t*) get_SPropValue_data(lpProps);
	return *mid;
}

zval* mapi_message_get_property(mapi_message_object_t* msg, mapi_id_t prop_id)
{
	zval *zprop;
	void *prop_value;
	prop_value  = (void*) find_mapi_SPropValue_data(&(msg->properties), prop_id);
	zprop = mapi_message_property_to_zval(msg->talloc_ctx, prop_id, prop_value);

	return zprop;
}

zval* mapi_message_property_to_zval(TALLOC_CTX *talloc_ctx, mapi_id_t prop_id, void *prop_value)
{
	zval *zprop;
	uint32_t prop_type;

	php_printf("Property 0x%" PRIX64 " value %p\n", prop_id, prop_value); //DDD



	MAKE_STD_ZVAL(zprop);
	if (prop_value == NULL) {
		php_printf("Property 0x%" PRIX64 " NULL\n", prop_id); //DDD
		ZVAL_NULL(zprop);
		return zprop;
	}

	prop_type =  prop_id & 0xFFFF;
	if ((prop_type == PT_UNICODE) || (prop_type == PT_STRING8)) {
		ZVAL_STRING(zprop, (char *) prop_value , 1);
	} else if (prop_type == PT_LONG) {
		ZVAL_LONG(zprop, *((long *) prop_value));
		php_printf("Property 0x%" PRIX64 " long value %i\n", prop_id, *((long *) prop_value)); //DDD
	} else if (prop_type == PT_BOOLEAN) {
		php_printf("Property 0x%" PRIX64 " bool value %i\n", prop_id, *((bool *) prop_value)); //DDD
		ZVAL_BOOL(zprop, *((bool *) prop_value));
	} else if (prop_type == PT_I8) {
		mapi_id_t id = *((mapi_id_t*) prop_value);
		char *str_id = mapi_id_to_str(id);
		ZVAL_STRING(zprop, str_id, 0);
	} else if (prop_type == PT_SYSTIME) {
		const struct FILETIME *filetime_value = (const struct FILETIME *) prop_value;
		const char		*date;
		if (filetime_value) {
			NTTIME			time;
			time = filetime_value->dwHighDateTime;
			time = time << 32;
			time |= filetime_value->dwLowDateTime;
			date = nt_time_string(talloc_ctx, time);
		}
		if (date) {
			ZVAL_STRING(zprop, date, 1);
			talloc_free((char*) date);
		} else {
			php_error(E_ERROR, "Cannot create  zval from PT_SYSTIME");
		}

	} else {
// TODO : PT_ERROR PT_MV_BINARY PT_OBJECT PT_BINARY	  PT_STRING8  PT_MV_UNICODE   PT_CLSID PT_SYSTIME  PT_SVREID  PT_I8
		php_error(E_ERROR, "Property 0x%" PRIX64 " has a type unknow or unsupported", prop_id);
	}

	return zprop;
}


const char *mapi_date(TALLOC_CTX *parent_ctx,
		      struct mapi_SPropValue_array *properties,
		      uint32_t mapitag)
{
	const char		*date = NULL;
	TALLOC_CTX		*talloc_ctx;
	NTTIME			time;
	const struct FILETIME	*filetime;

	filetime = (const struct FILETIME *) find_mapi_SPropValue_data(properties, mapitag);
	if (filetime) {
		const char *nt_date_str;

		time = filetime->dwHighDateTime;
		time = time << 32;
		time |= filetime->dwLowDateTime;
		talloc_ctx = talloc_named(parent_ctx, 0, "mapi_date_string");
		nt_date_str = nt_time_string(talloc_ctx, time);
		date = estrdup(nt_date_str);
		talloc_free(talloc_ctx);
	}

	return date;
}

bool mapi_message_types_compatibility(zval *zv, mapi_id_t mapi_type)
{
	int type = Z_TYPE_P(zv);
	switch (type) {
	case IS_NULL:
		break;
	case IS_LONG:
		switch (mapi_type) {
		case PT_LONG:
			return true;
		default:
			return false;
		}
		break;
	case IS_DOUBLE:
		return (mapi_type == PT_DOUBLE) ? true : false;
		break;
	case IS_STRING:
		switch (mapi_type) {
		case PT_STRING8:
		case PT_UNICODE:
		case PT_SYSTIME:
			return true;
		default:
			return false;
		}
	case IS_BOOL:
		return (mapi_type == PT_BOOLEAN) ? true : false;
	case IS_ARRAY:
		return false;
	case IS_OBJECT:
		return false;
	case IS_RESOURCE:
		return false;
	default:
		php_printf("Type not expected: '%x'. Skipped\n", type);
		return false;
	}

	php_printf("Incorrect zval type %i for MAPI type  0x%" PRIX64  "\n", type, mapi_type);
	return false;


}

void *mapi_message_zval_to_mapi_value(TALLOC_CTX *talloc_ctx, zval *val)
{
	void* data = NULL;
	int type = Z_TYPE_P(val);
	if (type == IS_NULL) {
		// XXX TO CHECK
		data = NULL;
	} else if (type == IS_LONG) {
		// XXX TO CHECK
		long *ldata = talloc_ptrtype(talloc_ctx, ldata);
		*ldata = Z_LVAL_P(val);
		data = (void*) ldata;
		php_printf( "zval to long: %p -> %i\n", data, *((long*)data));
	} else if (type == IS_DOUBLE) {
		// XXX TO CHECK
		double *ddata =  talloc_ptrtype(talloc_ctx, ddata);
		*ddata = Z_DVAL_P(val);
		data = (void*) ddata;
	} else if (type == IS_STRING) {
		data = (void*)talloc_strdup(talloc_ctx, Z_STRVAL_P(val));
	} else if (type == IS_BOOL) {
		bool *bdata =  talloc_ptrtype(talloc_ctx, bdata);
		*bdata = Z_BVAL_P(val);
		data = (void*) bdata;
		php_printf( "zval to bdata: %p -> %i\n", data, *((bool*)data));
	} else {
		php_printf("Type not expected: '%i'. Skipped\n", type);
	}

	return data;
}

/* uint32_t find_mapi_SPropValue_pos(struct mapi_SPropValue_array *properties, uint32_t mapitag) */
/* { */
/* 	uint32_t i; */
/* 	for (i = 0; i < properties->cValues; i++) { */
/* 		if (properties->lpProps[i].ulPropTag == mapitag) { */
/* 			return i; */
/* 		} */
/* 	} */
/* 	return i; */
/* } */

// 	const char	*card_name = (const char *)find_mapi_SPropValue_data(&(this_obj->properties), PidLidFileUnder);
// 	const char	*email = (const char *)find_mapi_SPropValue_data(&(this_obj->properties), PidLidEmail1OriginalDisplayName);
PHP_METHOD(MAPIMessage, get)
{

	int 			argc;
	zval 			**args;
	zval			*this_php_obj;
	mapi_message_object_t 	*this_obj;
	zval *result;
	int 			i;

	argc = ZEND_NUM_ARGS();
	if (ZEND_NUM_ARGS() == 0) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	args = (zval**) safe_emalloc(argc, sizeof(zval **), 0);
	if ( zend_get_parameters_array(UNUSED_PARAM, argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	this_php_obj = getThis();
	this_obj = STORE_OBJECT(mapi_message_object_t*, this_php_obj);

	if (argc > 1) {
		MAKE_STD_ZVAL(result);
		array_init(result);
	}
	for (i=0; i < argc; i++) {
		if (Z_TYPE_P(args[i]) != IS_LONG) {
			php_error(E_ERROR, "Argument %i is not a numeric property id", i);
		}

	}

	for (i=0; i<argc; i++) {
		zval *prop;
		zval **temp_prop;
		mapi_id_t prop_id = (mapi_id_t) Z_LVAL_P(args[i]);
		php_printf("get 0x%" PRIX64  "\n", prop_id); //DDD
		prop  = mapi_message_get_property(this_obj, prop_id);

		if (argc == 1) {
			result = prop;
		} else {
			if (prop == NULL) {
				continue;
			} else if (ZVAL_IS_NULL(prop)) {
				zval_ptr_dtor(&prop);
				continue;
			}

			add_index_zval(result, prop_id, prop);
		}
	}

	efree(args);
	if (result) {
		RETURN_ZVAL(result, 0, 1);
	} else {
		RETURN_NULL();
	}
}

void mapi_message_set_properties(zval *message_zval, int argc, zval **args TSRMLS_DC)
{
	mapi_message_object_t 	*message_obj;
	enum MAPISTATUS		retval;
	struct SPropValue	*lpProps = NULL;
	uint32_t		cValues;
	int 			i;

	message_obj = (mapi_message_object_t *) zend_object_store_get_object(message_zval TSRMLS_CC);
	for (i=0; i<argc; i+=2) {
		mapi_id_t id = Z_LVAL_P(args[i]);
		mapi_id_t  prop_type =  id & 0xFFFF;
		zval *val = args[i+1];

		void *data;
		if (mapi_message_types_compatibility(val, prop_type) == false) {
			php_printf("Property with id 0x%" PRIX64 " with type 0x%" PRIX64  " has a incorrect zval type. Skipping\n", id, prop_type);
			continue;
		}

		php_printf("TO SET with id 0x%" PRIX64 " with type 0x%" PRIX64  " \n", id, prop_type); // DDD

		data = mapi_message_zval_to_mapi_value(message_obj->talloc_ctx, val);

		/* Pushing the property with SetProps */
		cValues = 0;
		lpProps = talloc_array(message_obj->talloc_ctx, struct SPropValue, 2);
		lpProps = add_SPropValue(message_obj->talloc_ctx, lpProps, &cValues, id, (const void *) data);
		retval = SetProps(message_obj->message, 0, lpProps, cValues);
		CHECK_MAPI_RETVAL(retval, "SetProps");
		MAPIFreeBuffer(lpProps);
		lpProps = NULL;
	}

}

// TODO: for other types, now it only works for strings
PHP_METHOD(MAPIMessage, set)
{
	int 			argc;
	zval 			**args;
	int			i;

	argc = ZEND_NUM_ARGS();
	if ((argc == 0) || ((argc % 2) == 1)) {
		WRONG_PARAM_COUNT;
	}

	args = (zval **)safe_emalloc(argc, sizeof(zval **), 0);
	if (zend_get_parameters_array(UNUSED_PARAM, argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}
	for (i=0; i < argc; i+=2) {
		if (Z_TYPE_P(args[i]) != IS_LONG) {
			php_error(E_ERROR, "Argument %i is not a numeric property id", i);
		}

	}

	mapi_message_set_properties(getThis(), argc, args TSRMLS_CC);

	efree(args);
}

/* struct SPropValue { */
/* 	enum MAPITAGS ulPropTag; */
/* 	uint32_t dwAlignPad; */
/* 	union SPropValue_CTR value;/\* [switch_is(ulPropTag&0xFFFF)] *\/ */
/* }/\* [noprint,nopush,public,nopull] *\/; */

PHP_METHOD(MAPIMessage, save)
{
	mapi_message_object_t *this_obj;
	mapi_folder_object_t  *folder_obj;
	mapi_mailbox_object_t *mailbox_obj;
	enum MAPISTATUS	      retval;

	this_obj    = (mapi_message_object_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
	if ((this_obj->open_mode != 1) && (this_obj->open_mode != 3)) {
		php_error(E_ERROR, "Trying to save a non-RW message");
	}

	folder_obj  = (mapi_folder_object_t*)   zend_object_store_get_object(this_obj->parent TSRMLS_CC);
	mailbox_obj = (mapi_mailbox_object_t*)  zend_object_store_get_object(folder_obj->parent TSRMLS_CC);

	retval = SaveChangesMessage(&(mailbox_obj->store),
				    this_obj->message,
				     KeepOpenReadWrite);

	CHECK_MAPI_RETVAL(retval, "Saving properties");
}
