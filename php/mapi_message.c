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

static zend_function_entry mapi_message_class_functions[] = {
	PHP_ME(MAPIMessage,	__construct,	       NULL,		ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIMessage,	__destruct,	       NULL,		ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_ME(MAPIMessage,	getID,	               NULL,	 	ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMessage,	get,	               NULL,	 	ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMessage,	getAsBase64,           NULL,	 	ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMessage,	set,	       	       NULL,	 	ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMessage,	setAsBase64,           NULL,	 	ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMessage,	save,	      	       NULL,           	ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMessage,	getBodyContentFormat,  NULL,           	ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMessage,	getAttachment,         NULL,           	ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMessage,	getAttachmentTable,    NULL,           	ZEND_ACC_PUBLIC)
	PHP_ME(MAPIMessage,	dump,		       NULL,		ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)

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

void mapi_message_request_all_properties(zval *z_message TSRMLS_DC)
{
	enum MAPISTATUS		retval;
	mapi_message_object_t 	*obj;

	obj = (mapi_message_object_t *) zend_object_store_get_object(z_message TSRMLS_CC);
	retval = GetPropsAll(obj->message, MAPI_UNICODE, &(obj->properties));
        CHECK_MAPI_RETVAL(retval, "Getting message properties");
	mapi_SPropValue_array_named(obj->message,  &(obj->properties));
}

void mapi_message_so_request_properties(mapi_message_object_t *obj, struct SPropTagArray *SPropTagArray TSRMLS_DC)
{
	enum MAPISTATUS		retval;
	struct SPropValue 	*lpProps;
	int			i;
	int			count;

	retval = GetProps(obj->message, MAPI_UNICODE, SPropTagArray, &lpProps, &count);
        CHECK_MAPI_RETVAL(retval, "Getting appointment properties");

	obj->properties.cValues = count;
	obj->properties.lpProps =  talloc_array(obj->talloc_ctx, struct mapi_SPropValue, count);
	for (i=0; i < count; i++) {
		cast_mapi_SPropValue(obj->talloc_ctx, &(obj->properties.lpProps[i]), &lpProps[i]);
	}
	MAPIFreeBuffer(lpProps);

	mapi_SPropValue_array_named(obj->message,  &(obj->properties));
}

zval *create_message_object(char *class, zval *folder, mapi_object_t *message, char open_mode TSRMLS_DC)
{

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

zval* mapi_message_get_base64_binary_property(mapi_message_object_t* msg, mapi_id_t prop_id)
{
	zval 		*result;
	uint32_t 	prop_type;
	struct Binary_r *bin;
	char *		base64;
	DATA_BLOB 	blob;

	prop_type =  prop_id & 0xFFFF;
	if (prop_type != PT_BINARY) {
		php_error(E_ERROR, "This function should be only used upon binary properties");
	}

	bin  = (struct Binary_r*) find_mapi_SPropValue_data(&(msg->properties), prop_id);
	blob = data_blob_talloc_named(msg->talloc_ctx, bin->lpb, bin->cb, "blob to hex");

	base64 = base64_encode_data_blob(msg->talloc_ctx, blob);

	MAKE_STD_ZVAL(result);
	ZVAL_STRING(result, base64, dup);

	data_blob_free(&blob);

	return result;
}

zval* mapi_message_set_base64_binary_property(mapi_message_object_t* msg, mapi_id_t prop_id, char *base64 TSRMLS_DC)
{
	zval 		*result;
	uint32_t 	prop_type;
	struct Binary_r *bin;
	DATA_BLOB 	blob;


	prop_type =  prop_id & 0xFFFF;
	if (prop_type != PT_BINARY) {
		php_error(E_ERROR, "This function should be only used upon binary properties");
	}

	blob = base64_decode_data_blob_talloc(msg->talloc_ctx, base64);
	bin  = emalloc(sizeof(struct Binary_r));
	bin->cb = blob.length;
	bin->lpb = blob.data;

	mapi_message_so_set_prop(msg->talloc_ctx, msg->message, prop_id, (void*) bin TSRMLS_CC);

	efree(bin);
	data_blob_free(&blob);
}

zval* mapi_message_property_to_zval(TALLOC_CTX *talloc_ctx, mapi_id_t prop_id, void *prop_value)
{
	zval *zprop;
	uint32_t prop_type;


	MAKE_STD_ZVAL(zprop);
	if (prop_value == NULL) {
		ZVAL_NULL(zprop);
		return zprop;
	}

	prop_type =  prop_id & 0xFFFF;
	if ((prop_type == PT_UNICODE) || (prop_type == PT_STRING8)) {
		ZVAL_STRING(zprop, (char *) prop_value , 1);
	} else if (prop_type == PT_LONG) {
		ZVAL_LONG(zprop, *((long *) prop_value));
	} else if (prop_type == PT_BOOLEAN) {
		ZVAL_BOOL(zprop, *((bool *) prop_value));
	} else if (prop_type == PT_I8) {
		mapi_id_t id = *((mapi_id_t*) prop_value);
		char *str_id = mapi_id_to_str(id);
		ZVAL_STRING(zprop, str_id, 0);
	} else if (prop_type == PT_DOUBLE) {
		ZVAL_DOUBLE(zprop, *((double*) prop_value));
	} else if (prop_type == PT_MV_UNICODE) {
		int i;
		struct StringArrayW_r *data = (struct StringArrayW_r*) prop_value;
		array_init(zprop);
		for (i=0; i < data->cValues; i++) {
			add_next_index_string(zprop, data->lppszW[i], 1);
		}
	} else if (prop_type == PT_MV_STRING8) {
		php_error(E_ERROR, "Not implemented PT_MV_STRING8");
	} else if (prop_type == PT_BINARY) {
		int i;
		struct Binary_r *bin = ((struct Binary_r*) prop_value);
		array_init(zprop);
		for (i=0; i < bin->cb; i++) {
			uint8_t value = bin->lpb[i];
			add_next_index_long(zprop, value);
		}
	} else if (prop_type == PT_SYSTIME) {
		const struct FILETIME *filetime_value = (const struct FILETIME *) prop_value;
		time_t		      unix_time = 0;
		if (filetime_value) {
			NTTIME			nt_time;
			nt_time = filetime_value->dwHighDateTime;
			nt_time = nt_time << 32;
			nt_time |= filetime_value->dwLowDateTime;
			unix_time = nt_time_to_unix(nt_time);
		}
		ZVAL_LONG(zprop, unix_time);
	} else {
// TODO : PT_ERROR PT_MV_BINARY PT_OBJECT PT_BINARY	  PT_STRING8    PT_CLSID PT_SYSTIME  PT_SVREID  PT_I8
		php_error(E_ERROR, "Property 0x%" PRIX64 " has a type unknow or unsupported", prop_id);
	}

	return zprop;
}

// XXX mapi_message_check_*_array should be inside fill_array function
void mapi_message_check_binary_array_zval(zval *zv)
{
	int       arrayLen;
	int	  i;
	HashTable *ht = zv->value.ht;
	arrayLen = zend_hash_num_elements(ht);
	for (i=0; i < arrayLen; i++) {
		int found;
		long **value;
		found = zend_hash_index_find(ht, i, (void**) &value);
		if (found != SUCCESS) {
			php_error(E_ERROR, "Binary array has not element in position %i", i);

		}

		long bval = **value;
		if ((bval < 0) || (bval > 255)) {
			php_error(E_ERROR, "Binary array has bad value %ld at position %i", bval, i);
		}
	}
}

void mapi_message_check_string_array_zval(zval *zv)
{
	int       arrayLen;
	int	  i;
	HashTable *ht = zv->value.ht;
	arrayLen = zend_hash_num_elements(ht);
	for (i=0; i < arrayLen; i++) {
		int found;
		zval **value;
		found = zend_hash_index_find(ht, i, (void**) &value);
		if (found != SUCCESS) {
			php_error(E_ERROR, "String array has not element in position %i", i);

		}
		if (Z_TYPE_PP(value) != IS_STRING) {
			php_error(E_ERROR, "String array has bad value at position %i", i);
		}
	}
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
		case PT_SYSTIME:
		case PT_DOUBLE:
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
		default:
			return false;
		}
	case IS_BOOL:
		return (mapi_type == PT_BOOLEAN) ? true : false;
	case IS_ARRAY:
		if (mapi_type == PT_BINARY) {
			mapi_message_check_binary_array_zval(zv);
			return true;
		} else if (mapi_type == PT_MV_UNICODE) {
			mapi_message_check_string_array_zval(zv);
			return true;
		}
		return false;
	case IS_OBJECT:
		return false;
	case IS_RESOURCE:
		return false;
	default:
		return false;
	}

	return false;
}

void mapi_message_fill_binary_array(TALLOC_CTX *mem_ctx, zval *src, struct Binary_r *bi)
{
	int 		i;
	HashTable 	*ht;
	int             nBits;
	ht = src->value.ht;
	nBits = zend_hash_num_elements(ht);
	bi->cb = nBits;
        bi->lpb =  talloc_named (mem_ctx, nBits*sizeof(uint8_t), "mapi_message_fill_binary_array");

	for(i=0; i < bi->cb; i++) {
		int found;
		long **value;
		found = zend_hash_index_find(ht, i, (void**) &value);
		if (found != SUCCESS) {
			php_error(E_ERROR, "Cannot find byte in array position %i", i);
		}

		bi->lpb[i] = **value;
	}
}

void *mapi_message_fill_unicode_array(TALLOC_CTX *mem_ctx, zval *src, struct StringArrayW_r *string_array)
{
	int 		i;
	HashTable 	*ht;
	int             nStrs;
	ht = src->value.ht;
	nStrs = zend_hash_num_elements(ht);
	string_array->cValues = nStrs;
	string_array->lppszW = talloc_named(mem_ctx, nStrs*sizeof(const char*), "mapi_message_fill_unicode_array");

	for (i=0; i < nStrs; i++) {
		int found;
		zval **value;
		found = zend_hash_index_find(ht, i, (void**) &value);
		if (found != SUCCESS) {
			php_error(E_ERROR, "Cannot find string in array position %i", i);
		}
		string_array->lppszW[i] = talloc_strdup(mem_ctx, Z_STRVAL_PP(value));
	}
}

void *mapi_message_zval_to_mapi_value(TALLOC_CTX *mem_ctx, mapi_id_t mapi_type, zval *val)
{
	void* data = NULL;
	int type = Z_TYPE_P(val);
	if (type == IS_NULL) {
		data = NULL;
	} else if (type == IS_LONG) {
		if (mapi_type == PT_LONG) {
			long *ldata = talloc_ptrtype(mem_ctx, ldata);
			*ldata = Z_LVAL_P(val);
			data = (void*) ldata;
		} else if (mapi_type == PT_DOUBLE) {
			long value = Z_LVAL_P(val);
			if ((value > DBL_MAX) || (value < DBL_MIN_EXP)) {
				php_error(E_ERROR, "Value out of bonds for property of type double: %ld", value);
			}
			double *ddata =  talloc_ptrtype(mem_ctx, ddata);
			*ddata = value;
			data = (void*) ddata;
		} else if (mapi_type == PT_SYSTIME) {
			struct FILETIME		*date;
			long 			*ldata;
			NTTIME                  nttime;
			ldata =  talloc_ptrtype(mem_ctx, ldata);
			*ldata = Z_LVAL_P(val);

			unix_to_nt_time(&nttime, (time_t) *ldata);

			date = talloc(mem_ctx, struct FILETIME);
			date->dwLowDateTime  = (nttime << 32) >> 32;
			date->dwHighDateTime = (nttime >> 32);
			data = (void*) date;
		} else {
			php_error(E_ERROR, "Incorrect PHP long variable for mapi type  0x%" PRIX64 "", mapi_type);
		}

	} else if (type == IS_DOUBLE) {
		if (mapi_type == PT_DOUBLE) {
			double *ddata =  talloc_ptrtype(mem_ctx, ddata);
			*ddata = Z_DVAL_P(val);
			data = (void*) ddata;
		} else {
			php_error(E_ERROR, "Incorrect PHP double variable for mapi type  0x%" PRIX64 "", mapi_type);
		}
	} else if (type == IS_STRING) {
		data = (void*)talloc_strdup(mem_ctx, Z_STRVAL_P(val));
	} else if (type == IS_BOOL) {
		bool *bdata =  talloc_ptrtype(mem_ctx, bdata);
		*bdata = Z_BVAL_P(val);
		data = (void*) bdata;
	} else if ((type == IS_ARRAY) && (mapi_type == PT_BINARY)) {
		struct Binary_r *bidata = talloc_ptrtype(mem_ctx, bidata);
		mapi_message_fill_binary_array(mem_ctx, val, bidata);
		data = (void*) bidata;
	} else if ((type == IS_ARRAY) && (mapi_type == PT_MV_UNICODE)) {
		struct StringArrayW_r *sawdata =talloc_ptrtype(mem_ctx, sawdata);
		mapi_message_fill_unicode_array(mem_ctx, val, sawdata);
		data = (void *) sawdata;
	} else {
		php_error(E_ERROR, "ZVAL type %i for MAPI ID type 0x%" PRIX64 "  not expected. Skipped\n", type, mapi_type);
	}

	return data;
}

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

void mapi_message_so_set_prop(TALLOC_CTX *mem_ctx, mapi_object_t *message, mapi_id_t id, void *data TSRMLS_DC)
{
	enum MAPISTATUS		retval;
	struct SPropValue	*lpProps = NULL;
	uint32_t		cValues;

	/* Pushing the property with SetProps */
	cValues = 0;
	lpProps = talloc_array(mem_ctx, struct SPropValue, 1);
	lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, id, (const void *) data);
	retval = SetProps(message, 0, lpProps, cValues);
	CHECK_MAPI_RETVAL(retval, "SetProps");
	MAPIFreeBuffer(lpProps);
	lpProps = NULL;
}

void mapi_message_set_properties(zval *message_zval, int argc, zval **args TSRMLS_DC)
{
	mapi_message_object_t 	*message_obj;
	int 			i;

	message_obj = (mapi_message_object_t *) zend_object_store_get_object(message_zval TSRMLS_CC);
	for (i=0; i<argc; i+=2) {
		mapi_id_t id = Z_LVAL_P(args[i]);
		mapi_id_t  prop_type =  id & 0xFFFF;
		zval *val = args[i+1];

		void *data;
		if (mapi_message_types_compatibility(val, prop_type) == false) {
			php_log_err("mapi_message_set_properties: trying to set invalid zval type to a property" TSRMLS_CC);
			continue;
		}

		data = mapi_message_zval_to_mapi_value(message_obj->talloc_ctx, prop_type, val);
		mapi_message_so_set_prop(message_obj->talloc_ctx, message_obj->message, id, data TSRMLS_CC);
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

PHP_METHOD(MAPIMessage, getBodyContentFormat)
{
	enum MAPISTATUS	      ret;
	mapi_message_object_t *this_obj;
	uint8_t  	      format;

	this_obj    = (mapi_message_object_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
	ret = GetBestBody(this_obj->message, &format);
	CHECK_MAPI_RETVAL(ret, "getBodyContentFormat");

	switch(format) {
	case olEditorText:
		RETURN_STRING("txt", 1);
		break;
	case olEditorHTML:
		RETURN_STRING("html", 1);
		break;
	case olEditorRTF:
		RETURN_STRING("rtf", 1);
		break;
	default:
		php_error(E_ERROR, "Unknown body content format: %i", format);
	}
}

zval *mapi_message_get_attachment(zval *z_message, const uint32_t attach_num TSRMLS_DC)
{
	mapi_message_object_t 	*message_obj;
	zval 	   	 	*z_attachment;
	mapi_object_t 		*attachment_obj;
	enum MAPISTATUS		retval;

	message_obj    = (mapi_message_object_t *) zend_object_store_get_object(z_message TSRMLS_CC);
	attachment_obj = emalloc(sizeof(mapi_object_t));
	mapi_object_init(attachment_obj);
	retval = OpenAttach(message_obj->message, attach_num, attachment_obj);
	CHECK_MAPI_RETVAL(retval, "Open attachment");

	z_attachment = create_attachment_object(z_message, attachment_obj TSRMLS_CC);
	return z_attachment;
}

PHP_METHOD(MAPIMessage, getAttachment)
{
	long attach_num;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &attach_num  ) == FAILURE) {
		php_error(E_ERROR, "getAttachment invalid arguments. Must be: attachment number");
	}

	zval *attch = mapi_message_get_attachment(getThis(), (uint32_t) attach_num TSRMLS_CC);
	RETURN_ZVAL(attch, 0, 1);
}

PHP_METHOD(MAPIMessage, getAttachmentTable)
{
	zval			*z_this_obj;
	mapi_message_object_t 	*this_obj;
	enum MAPISTATUS		retval;
	mapi_object_t           *obj_table;
	zval			*z_table;

	z_this_obj     = getThis();
	this_obj    = (mapi_message_object_t *) zend_object_store_get_object(z_this_obj TSRMLS_CC);

	obj_table = emalloc(sizeof(mapi_object_t));
	mapi_object_init(obj_table);
	retval = GetAttachmentTable(this_obj->message, obj_table);
	CHECK_MAPI_RETVAL(retval, "GetAttachmentTable");

	z_table = create_attachment_table_object(z_this_obj, obj_table  TSRMLS_CC);

	RETVAL_ZVAL(z_table, 0, 1);
}

// TODO generalize to all kinds of messages
zval *mapi_message_dump(zval *z_message TSRMLS_DC)
{
	mapi_message_object_t		*message_obj;
	zval				*dump;
	int				i;

	MAKE_STD_ZVAL(dump);
	array_init(dump);

	message_obj = (mapi_message_object_t *) zend_object_store_get_object(z_message TSRMLS_CC);

	for (i = 0; i < message_obj->properties.cValues; i++) {
		zval  *prop;
		mapi_id_t prop_id =  message_obj->properties.lpProps[i].ulPropTag;
		prop = mapi_message_get_property(message_obj, prop_id);
		if (prop == NULL) {
			continue;
		} else if (ZVAL_IS_NULL(prop)) {
			zval_ptr_dtor(&prop);
			continue;
		}
	        add_index_zval(dump, prop_id, prop);
	}


	return dump;
}


PHP_METHOD(MAPIMessage, dump)
{
	zval *dump = mapi_message_dump(getThis() TSRMLS_CC);
	RETURN_ZVAL(dump, 0, 1);
}

PHP_METHOD(MAPIMessage, getAsBase64)
{
	zval 		      *base64;
	long		      prop_id;
	mapi_message_object_t *this_obj;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &prop_id  ) == FAILURE) {
		php_error(E_ERROR, "getAsBase64 invalid arguments. Must be: property ID number");
	}

	this_obj    = (mapi_message_object_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
	base64 = mapi_message_get_base64_binary_property(this_obj, (mapi_id_t) prop_id);
	RETURN_ZVAL(base64, 0, 1);
}

PHP_METHOD(MAPIMessage, setAsBase64)
{
	char 		      *base64 = NULL;
	size_t	              base64_len;
	long		      prop_id;
	mapi_message_object_t *this_obj;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ls", &prop_id, &base64, &base64_len  ) == FAILURE) {
		php_error(E_ERROR, "setAsBase64 invalid arguments. Must be: property ID number, base 64 string");
	}
	if (base64 == NULL) {
		php_error(E_ERROR, "setAsBase64 was passed a NULL string");
	}

	this_obj    = (mapi_message_object_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
	mapi_message_set_base64_binary_property(this_obj, (mapi_id_t) prop_id, base64 TSRMLS_CC);
}
