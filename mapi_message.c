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
	php_printf("message free\n");

	mapi_message_object_t	*obj;

	obj = (mapi_message_object_t *) object;
	if (obj->talloc_ctx) {
		talloc_free(obj->talloc_ctx);
	}
	if (obj->message) {
		php_printf("messag release %p\n", obj->message);
		mapi_object_release(obj->message);
		efree(obj->message);
	}

//	S_PARENT_DELREF_P(obj);
//	Z_OBJ_HT_P(obj->parent)->del_ref(obj->parent TSRMLS_CC);
//	Z_DELREF_P(obj->parent);

	zend_object_std_dtor(&(obj->std) TSRMLS_CC);
	efree(obj);

	php_printf("END message free\n");
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
}

zval *create_message_object(char *class, zval *parent, mapi_object_t *message, mapi_id_t id TSRMLS_DC)
{
	enum MAPISTATUS		retval;
	zval 			*new_php_obj;
	mapi_message_object_t 	*new_obj;
	zend_class_entry	**ce;

	MAKE_STD_ZVAL(new_php_obj);
	if (zend_hash_find(EG(class_table), class, strlen(class)+1,(void**)&ce) == FAILURE) {
		php_error(E_ERROR, "create_message_object: class '%s' does not exist.", class);
	}
	object_init_ex(new_php_obj, *ce);

	new_obj = (mapi_message_object_t *) zend_object_store_get_object(new_php_obj TSRMLS_CC);
	new_obj->message = message;
	new_obj->talloc_ctx = talloc_named(NULL, 0, "message");
	new_obj->parent = parent;
	new_obj->id = id;
//	Z_ADDREF_P(new_obj->parent);
//	Z_OBJ_HT_P(new_obj->parent)->add_ref(new_obj->parent TSRMLS_CC);
//	S_PARENT_ADDREF_P(new_obj);

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

	/* zval			*php_this_obj; */
	/* mapi_message_object_t	*this_obj; */
	/* php_this_obj = getThis(); */
	/* this_obj = (mapi_message_object_t *) zend_object_store_get_object(php_this_obj TSRMLS_CC); */

}

PHP_METHOD(MAPIMessage, getID)
{
	zval			*this_php_obj;
	mapi_message_object_t 	*this_obj;
	char 			*str_id;

	this_php_obj = getThis();
	this_obj = STORE_OBJECT(mapi_message_object_t*, this_php_obj);
	str_id = mapi_id_to_str(this_obj->id);
	RETURN_STRING(str_id, 0);
}

zval* mapi_message_get_property(mapi_message_object_t* msg, mapi_id_t prop_id)
{
	zval *zprop;
	void *prop_value;
	uint32_t prop_type;

	prop_value  = (void*) find_mapi_SPropValue_data(&(msg->properties), prop_id);
	if (prop_value == NULL) {
		return NULL;
	}

	MAKE_STD_ZVAL(zprop);
	prop_type =  prop_id & 0xFFFF;
	if (prop_type == PT_UNICODE) {
		ZVAL_STRING(zprop, (char *) prop_value , 1);
	} else if (prop_type == PT_LONG) {
		ZVAL_LONG(zprop, *((long *) prop_value));
	} else if (prop_type == PT_BOOLEAN) {
		ZVAL_BOOL(zprop, *((bool *) prop_value));
	} else if (prop_type == PT_SYSTIME) {
		const struct FILETIME *filetime_value = (const struct FILETIME *) prop_value;
		const char		*date;
		if (filetime_value) {
			NTTIME			time;
			time = filetime_value->dwHighDateTime;
			time = time << 32;
			time |= filetime_value->dwLowDateTime;
			date = nt_time_string(msg->talloc_ctx, time);
		}
		if (date) {
			ZVAL_STRING(zprop, date, 1);
			talloc_free((char*) date);
		} else {
			return NULL;
		}

	} else {
// TODO : PT_ERROR PT_MV_BINARY PT_OBJECT PT_BINARY	  PT_STRING8  PT_MV_UNICODE   PT_CLSID PT_SYSTIME  PT_SVREID  PT_I8
		php_error(E_ERROR, "Property type %i is unknow or unsupported", prop_type);
	}

	return zprop;
}

bool mapi_message_types_compatibility(zval *zv, int mapi_type)
{
	int	type;

	php_printf("Z_TYPE_P(zv) = %d\n", Z_TYPE_P(zv));

	switch (Z_TYPE_P(zv)) {
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
			return true;
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

	php_printf("Incorrect zval for MAPI type  0x%" PRIX64  "\n", mapi_type);
	return false;


}

void *mapi_message_zval_to_mapi_value(TALLOC_CTX *talloc_ctx, zval *val)
{
	char* data = NULL;
	int type = Z_TYPE_P(val);
	if (type == IS_NULL) {
		data = NULL;

		php_printf("NULL value DDD\n");
//					continue;
/* 		} else if (type == IS_LONG) { */
/* 			data = emalloc(sizeof(long)); */
/* //			*data = Z_LVAL_P(ppzval); */
/* 			*data = Z_LVAL_P(*ppzval); */
		/* } else if (type == IS_DOUBLE) { */
		/* 	data = emalloc(sizeof(double)); */
		/* 	*data =Z_DVAL_P(ppzval); */
		} else if (type == IS_STRING) {
			data = (void*)talloc_strdup(talloc_ctx, Z_STRVAL_P(val));
			php_printf("STRING %s value DDD\n", (char*) data);

		/* } else if (type == IS_BOOL) { */
		/* 	data = emalloc(sizeof(bool)); */
		/* 	*data = Z_STRBool_P(ppzval); */
		} else {
			php_printf("Type not expected: '%i'. Skipped\n", type);
		}

	return data;
}



uint32_t find_mapi_SPropValue_pos(struct mapi_SPropValue_array *properties, uint32_t mapitag)
{
	uint32_t i;
	for (i = 0; i < properties->cValues; i++) {
		if (properties->lpProps[i].ulPropTag == mapitag) {
			return i;
		}
	}
	return i;
}



// 	const char	*card_name = (const char *)find_mapi_SPropValue_data(&(this_obj->properties), PidLidFileUnder);
// 	const char	*email = (const char *)find_mapi_SPropValue_data(&(this_obj->properties), PidLidEmail1OriginalDisplayName);
PHP_METHOD(MAPIMessage, get)
{
	int 			i, argc = ZEND_NUM_ARGS();
	zval 			***args;
	zval			*this_php_obj;
	mapi_message_object_t 	*this_obj;
	zval *result;

	args = (zval ***)safe_emalloc(argc, sizeof(zval **), 0);

	if (ZEND_NUM_ARGS() == 0 ||
	    zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	this_php_obj = getThis();
	this_obj = STORE_OBJECT(mapi_message_object_t*, this_php_obj);

	if (argc > 1) {
		MAKE_STD_ZVAL(result);
		array_init(result);
	}

	for (i=0; i<argc; i++) {
		zval *prop;
		zval **temp_prop;
		mapi_id_t prop_id = (mapi_id_t) Z_LVAL_PP(args[i]);
		php_printf("get 0x%" PRIX64  "\n", prop_id);
		prop  = mapi_message_get_property(this_obj, prop_id);

		if (argc == 1) {
			result = prop;
		} else {
			if (prop == NULL) {
				MAKE_STD_ZVAL(prop);
				ZVAL_NULL(prop);
			}
			add_next_index_zval(result, prop);			}
	}

	efree(args);
	if (result) {
		RETURN_ZVAL(result, 0, 1);
	} else {
		RETURN_NULL();
	}
}

// TODO: for other types, now it only works for strings
PHP_METHOD(MAPIMessage, set)
{
	int 			i, argc;
	zval 			***args;
	zval			*php_this_obj;
	mapi_message_object_t 	*this_obj;
	zval *result;

	argc = ZEND_NUM_ARGS();
	if ((argc == 0) || ((argc % 2) == 1)) {
		WRONG_PARAM_COUNT;
	}

	args = (zval ***)safe_emalloc(argc, sizeof(zval **), 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	php_this_obj = getThis();
	this_obj = (mapi_message_object_t *) zend_object_store_get_object(php_this_obj TSRMLS_CC);

	struct mapi_SPropValue_array *properties = &(this_obj->properties);
	for (i=0; i<argc; i+=2) {
		mapi_id_t id = Z_LVAL_PP(args[i]);
		uint32_t  prop_type =  id & 0xFFFF;
		zval *val = *(args[i+1]);
		void *data;
		php_printf("set 0x%" PRIX64  "\n", id);
		if (mapi_message_types_compatibility(val, prop_type) == false) {
			php_printf("Incorrect type for property " PRIX64  ". Skipping\n", id);
			continue;
		}

		data = mapi_message_zval_to_mapi_value(this_obj->talloc_ctx, val);

		/* Pushing the property with SetProps */
		{
			enum MAPISTATUS		retval;
			struct SPropValue	*lpProps;
			uint32_t		cValues;

			cValues = 0;
			lpProps = talloc_array(this_obj->talloc_ctx, struct SPropValue, 2);
			lpProps = add_SPropValue(this_obj->talloc_ctx, lpProps, &cValues, id, (const void *) data);
			retval = SetProps(this_obj->message, 0, lpProps, cValues);
			MAPIFreeBuffer(lpProps);
		}

		uint32_t prop_pos = find_mapi_SPropValue_pos(properties, id);
		if (prop_pos >=  properties->cValues) {
			php_printf("Adding property\n");

			add_mapi_SPropValue(this_obj->talloc_ctx,  properties->lpProps, &(properties->cValues), id ,data);
		} else {
			php_printf("Saving existent property in pos %i\n", prop_pos);
			set_mapi_SPropValue(this_obj->talloc_ctx,  &(properties->lpProps[prop_pos]), data);
		}
	}

	efree(args);
}


/* struct SPropValue { */
/* 	enum MAPITAGS ulPropTag; */
/* 	uint32_t dwAlignPad; */
/* 	union SPropValue_CTR value;/\* [switch_is(ulPropTag&0xFFFF)] *\/ */
/* }/\* [noprint,nopush,public,nopull] *\/; */

PHP_METHOD(MAPIMessage, save)
{
	zval*			php_this_obj;
	mapi_message_object_t 	*this_obj;
	enum MAPISTATUS	      	retval;
	zval                		*mailbox;
	struct mapi_mailbox_object	*mailbox_obj;
	TALLOC_CTX                      *talloc_ctx;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O",
                                  &mailbox, mapi_mailbox_ce) == FAILURE) {
		php_error(E_ERROR, "Missing mailbox object");
	}

	php_this_obj = getThis();
	this_obj = (mapi_message_object_t *) zend_object_store_get_object(php_this_obj TSRMLS_CC);

	mailbox_obj = (struct mapi_mailbox_object*) zend_object_store_get_object(mailbox TSRMLS_CC);
	retval = SaveChangesMessage(&(mailbox_obj->store),
				    this_obj->message,
				     KeepOpenReadWrite);
	CHECK_MAPI_RETVAL(retval, "Saving properties");
}

