#include <php_mapi.h>

static zend_function_entry mapi_attachment_class_functions[] = {
	PHP_ME(MAPIAttachment,	__construct,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIAttachment,	__destruct,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_ME(MAPIAttachment,	dump,	NULL,			ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)

	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_attachment_ce;
static zend_object_handlers	mapi_attachment_object_handlers;

void MAPIAttachmentRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIAttachment", mapi_attachment_class_functions);
	mapi_attachment_ce = zend_register_internal_class_ex(&ce, NULL, "mapimessage" TSRMLS_CC);

	mapi_attachment_ce->create_object = mapi_message_create_handler;

	memcpy(&mapi_attachment_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	mapi_attachment_object_handlers.clone_obj = NULL;
}

zval *create_attachment_object(zval *message, mapi_object_t *attachment TSRMLS_DC)
{
	zval *z_attachment = create_message_object("mapiattachment", message, attachment, 0 TSRMLS_CC);
	mapi_message_request_all_properties(z_attachment TSRMLS_CC);
	return z_attachment;
}

PHP_METHOD(MAPIAttachment, __construct)
{
	php_error(E_ERROR, "The attachment object should not created directly.");
}


PHP_METHOD(MAPIAttachment, __destruct)
{
	/* zval			*php_this_obj; */
	/* mapi_message_object_t	*this_obj; */
	/* php_this_obj = getThis(); */
	/* this_obj = (mapi_attachment_object_t *) zend_object_store_get_object(php_this_obj TSRMLS_CC); */

}


// XXXX tyr to generalize to all kinds of messages
zval *mapi_attachment_dump(zval *z_attachment TSRMLS_DC)
{
	mapi_message_object_t		*attachment_obj;
	zval				*dump;
	int				i;

	MAKE_STD_ZVAL(dump);
	array_init(dump);

	attachment_obj = (mapi_message_object_t *) zend_object_store_get_object(z_attachment TSRMLS_CC);

	for (i = 0; i < attachment_obj->properties.cValues; i++) {
		zval  *prop;
		mapi_id_t prop_id =  attachment_obj->properties.lpProps[i].ulPropTag;
		prop = mapi_message_get_property(attachment_obj, prop_id);
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


PHP_METHOD(MAPIAttachment, dump)
{
	zval *dump = mapi_attachment_dump(getThis() TSRMLS_CC);
	RETURN_ZVAL(dump, 0, 1);
}

