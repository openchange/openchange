#include <php_mapi.h>

static zend_function_entry mapi_contact_class_functions[] = {
	PHP_ME(MAPIContact,	__construct,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIContact,	__destruct,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_ME(MAPIContact,	__get,	        php_method_one_arg, 	ZEND_ACC_PUBLIC)

	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_contact_ce;
static zend_object_handlers	mapi_contact_object_handlers;

static void mapi_contact_free_storage(void *object TSRMLS_DC)
{
	mapi_contact_object_t	*obj;

	obj = (mapi_contact_object_t *) object;
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


static zend_object_value mapi_contact_create_handler(zend_class_entry *type TSRMLS_DC)
{
	zval			*tmp;
	zend_object_value	retval;
	mapi_contact_object_t	*obj;

	obj = (mapi_contact_object_t *) emalloc(sizeof(mapi_contact_object_t));
	memset(obj, 0, sizeof(mapi_contact_object_t));

	obj->std.ce = type;

	ALLOC_HASHTABLE(obj->std.properties);
	zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
#if PHP_VERSION_ID < 50399
	zend_hash_copy(obj->std.properties, &type->default_properties,
		       (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
#else
	object_properties_init((zend_object *) &(obj->std), type);
#endif
	retval.handle = zend_objects_store_put(obj, NULL, mapi_contact_free_storage,
					       NULL TSRMLS_CC);
	retval.handlers = &mapi_contact_object_handlers;

	return retval;
}



void MAPIContactRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIContact", mapi_contact_class_functions);
	mapi_contact_ce = zend_register_internal_class(&ce TSRMLS_CC);
	mapi_contact_ce->create_object = mapi_contact_create_handler;
	memcpy(&mapi_contact_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	mapi_contact_object_handlers.clone_obj = NULL;
}



zval *create_contact_object(mapi_object_t  *message TSRMLS_DC)
{
	enum MAPISTATUS		retval;
	zval 			*new_php_obj;
	mapi_contact_object_t 	*new_obj;
	zend_class_entry	**ce;
	struct mapi_session 	*session;

	MAKE_STD_ZVAL(new_php_obj);
	/* create the MapiContact instance in return_value zval */
	if (zend_hash_find(EG(class_table),"mapicontact", sizeof("mapicontact"),(void**)&ce) == FAILURE) {
		php_error(E_ERROR, "Class MAPIContact does not exist.");
	}
	object_init_ex(new_php_obj, *ce);

	new_obj = (mapi_contact_object_t *) zend_object_store_get_object(new_php_obj TSRMLS_CC);
	new_obj->message = message;
	new_obj->talloc_ctx = talloc_named(NULL, 0, "contact");

	retval = GetPropsAll(new_obj->message, MAPI_UNICODE, &(new_obj->properties));
        CHECK_MAPI_RETVAL(retval, "Getting contact properties");

	mapi_SPropValue_array_named(new_obj->message,  &(new_obj->properties));

	return new_php_obj;
}

PHP_METHOD(MAPIContact, __construct)
{
	php_error(E_ERROR, "The contact object should not created directly.");
}


PHP_METHOD(MAPIContact, __destruct)
{
	/* zval			*php_this_obj; */
	/* mapi_contact_object_t	*this_obj; */
	/* php_this_obj = getThis(); */
	/* this_obj = (mapi_contact_object_t *) zend_object_store_get_object(php_this_obj TSRMLS_CC); */

}

// 	const char	*card_name = (const char *)find_mapi_SPropValue_data(&(this_obj->properties), PidLidFileUnder);
// 	const char	*email = (const char *)find_mapi_SPropValue_data(&(this_obj->properties), PidLidEmail1OriginalDisplayName);
PHP_METHOD(MAPIContact, __get)
{
	char 			*prop_name;
	size_t 			prop_len;
	size_t 			full_prop_name_len;
	char 			*full_prop_name;
	mapi_contact_object_t 	*this_obj;
        uint32_t 		prop_id;
	uint32_t 		prop_type;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",
				  &prop_name, &prop_len) == FAILURE) {
		php_error(E_ERROR, "Missing property name");
	}

	full_prop_name_len = strlen("PidLid") + strlen(prop_name) + 1;
	full_prop_name = emalloc(full_prop_name_len);
	strlcpy(full_prop_name, "PidLid", full_prop_name_len);
	strlcat(full_prop_name, prop_name, full_prop_name_len);

	prop_id = get_namedid_value(full_prop_name);
	/* prop_id = get_proptag_value(full_prop_name); */
	if (prop_id == 0) {
		php_error(E_ERROR, "Invalid contact property: %s", prop_name);
	}
	efree(full_prop_name);

//	prop_type =  get_namedid_type(prop_id);
	prop_type =  get_namedid_type(prop_id >> 16); // do it with a bit rotatinon
		//get_property_type(prop_id);

	this_obj = THIS_STORE_OBJECT(mapi_contact_object_t*);
	if (prop_type == PT_UNICODE) {
		char *str_val = (char *)find_mapi_SPropValue_data(&(this_obj->properties), prop_id);
		RETURN_STRING(str_val, 1);
	} else if (prop_type == PT_LONG) {
		long *long_val = (long *)find_mapi_SPropValue_data(&(this_obj->properties), prop_id);
		RETVAL_LONG(*long_val);
	} else if (prop_type == PT_BOOLEAN) {
		bool *bool_val = (bool *)find_mapi_SPropValue_data(&(this_obj->properties), prop_id);
		RETVAL_BOOL(*bool_val);
	} else {
// TODO : PT_ERROR PT_MV_BINARY PT_OBJECT PT_BINARY	  PT_STRING8  PT_MV_UNICODE   PT_CLSID PT_SYSTIME  PT_SVREID  PT_I8
		php_error(E_ERROR, "Property type %i is unknow or unsupported", prop_type);
	}
}
