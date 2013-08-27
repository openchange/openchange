/*
   OpenChange MAPI PHP bindings

   Copyright (C) 2013 Zentyal S.L.

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include "php_mapi.h"

/* struct itemfolder { */
/* 	const uint32_t		olFolder; */
/* 	const char		*container_class; */
/* }; */

/* struct itemfolder	defaultFolders[] = { */
/* 	{olFolderInbox,		"Mail"}, */
/* 	{olFolderCalendar,	"Appointment"}, */
/* 	{olFolderContacts,	"Contact"}, */
/* 	{olFolderTasks,		"Task"}, */
/* 	{olFolderNotes,		"Note"}, */
/* 	{0 , NULL} */
/* }; */

static zend_function_entry mapi_session_class_functions[] = {
	PHP_ME(MAPISession,	__construct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPISession,	__destruct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_ME(MAPISession,	folders,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPISession,	fetchmail,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPISession,	appointments,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPISession,	contacts,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(MAPISession,	mailbox,	NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_session_ce;
static zend_object_handlers	mapi_session_object_handlers;


static void mapi_session_free_storage(void *object TSRMLS_DC)
{
	mapi_session_object_t	*obj;

	obj = (mapi_session_object_t *) object;
	if (obj->mem_ctx) {
		talloc_free(obj->mem_ctx);
	}

	zend_hash_destroy(obj->std.properties);
	FREE_HASHTABLE(obj->std.properties);

	if (obj->children_mailboxes) {
		zval_dtor(obj->children_mailboxes);
		FREE_ZVAL(obj->children_mailboxes);
	}

	efree(obj);
}


static zend_object_value mapi_session_create_handler(zend_class_entry *type TSRMLS_DC)
{
	zval			*tmp;
	zend_object_value	retval;
	mapi_session_object_t	*obj;

	obj = (mapi_session_object_t *) emalloc(sizeof(mapi_session_object_t));
	memset(obj, 0, sizeof(mapi_session_object_t));

	obj->std.ce = type;

	ALLOC_HASHTABLE(obj->std.properties);
	zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
#if PHP_VERSION_ID < 50399
	zend_hash_copy(obj->std.properties, &type->default_properties,
		       (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
#else
	object_properties_init((zend_object *) &(obj->std), type);
#endif
	retval.handle = zend_objects_store_put(obj, NULL, mapi_session_free_storage,
					       NULL TSRMLS_CC);
	retval.handlers = &mapi_session_object_handlers;

	return retval;
}


void MAPISessionRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPISession", mapi_session_class_functions);
	mapi_session_ce = zend_register_internal_class(&ce TSRMLS_CC);
	mapi_session_ce->create_object = mapi_session_create_handler;
	memcpy(&mapi_session_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	mapi_session_object_handlers.clone_obj = NULL;
}


zval *create_session_object(struct mapi_session *session,
			    zval *profile, TALLOC_CTX *mem_ctx TSRMLS_DC)
{
	zval			*php_obj;
	zend_class_entry	**ce;
	mapi_session_object_t	*obj;

	MAKE_STD_ZVAL(php_obj);

	/* create the MapiProfile instance in return_value zval */
	if (zend_hash_find(EG(class_table),"mapisession", sizeof("mapisession"),(void**)&ce) == FAILURE) {
		php_error(E_ERROR,"Class MAPISession does not exist.");
	}

	object_init_ex(php_obj, *ce);

	obj = (mapi_session_object_t *) zend_object_store_get_object(php_obj TSRMLS_CC);
	obj->session = session;
	obj->parent = profile;
	obj->mem_ctx = mem_ctx;
	MAKE_STD_ZVAL(obj->children_mailboxes);
	array_init(obj->children_mailboxes);

	return php_obj;
}


struct mapi_session *mapi_session_get_session(zval *php_obj TSRMLS_DC)
{
	mapi_session_object_t *obj;

	obj = (mapi_session_object_t *) zend_object_store_get_object(php_obj TSRMLS_CC);
	return obj->session;
}

struct mapi_profile *session_get_profile(zval *php_obj TSRMLS_DC)
{
	mapi_session_object_t	*obj ;

	obj = (mapi_session_object_t*) zend_object_store_get_object(php_obj TSRMLS_CC);
	return get_profile(obj->parent TSRMLS_CC);
}


/* static void init_message_store(mapi_object_t *store, */
/* 			       struct mapi_session *session, */
/* 			       bool public_folder, char *username) */
/* { */
/* 	enum MAPISTATUS retval; */

/* 	mapi_object_init(store); */
/* 	if (public_folder == true) { */
/* 		retval = OpenPublicFolder(session, store); */
/* 		if (retval != MAPI_E_SUCCESS) { */
/* 			php_error(E_ERROR, "Open public folder: %s", mapi_get_errstr(retval)); */
/* 		} */
/* 	} else if (username) { */
/* 		retval = OpenUserMailbox(session, username, store); */
/* 		if (retval != MAPI_E_SUCCESS) { */
/* 			php_error(E_ERROR, "Open user mailbox: %s", mapi_get_errstr(retval)); */
/* 		} */
/* 	} else { */
/* 		retval = OpenMsgStore(session, store); */
/* 		if (retval != MAPI_E_SUCCESS) { */
/* 			php_error(E_ERROR, "Open message store: %s",  mapi_get_errstr(retval)); */
/* 		} */
/* 	} */
/* } */


PHP_METHOD(MAPISession, __construct)
{
	php_error(E_ERROR, "The session object should not created directly.\n" \
		  "Use the 'logon' method in the profile object");
}


PHP_METHOD(MAPISession, __destruct)
{
	zval			*php_this;
	mapi_session_object_t	*this;

	php_this = getThis();
	this = (mapi_session_object_t *) zend_object_store_get_object(php_this TSRMLS_CC);

	if (zend_hash_num_elements(this->children_mailboxes->value.ht) > 0) {
		php_error(E_ERROR, "This MapiSession object has active mailboxes");
	}

	mapi_profile_remove_children_session(this->parent, Z_OBJ_HANDLE_P(php_this) TSRMLS_CC);
}

static const char *get_container_class(TALLOC_CTX *mem_ctx, mapi_object_t *parent, mapi_id_t folder_id)
{
	enum MAPISTATUS       retval;
	mapi_object_t         obj_folder;
	struct SPropTagArray  *SPropTagArray;
	struct SPropValue     *lpProps;
	uint32_t              count;

	mapi_object_init(&obj_folder);
	retval = OpenFolder(parent, folder_id, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_CONTAINER_CLASS);
	retval = GetProps(&obj_folder, MAPI_UNICODE, SPropTagArray, &lpProps, &count);
	MAPIFreeBuffer(SPropTagArray);
	mapi_object_release(&obj_folder);
	if ((lpProps[0].ulPropTag != PR_CONTAINER_CLASS) || (retval != MAPI_E_SUCCESS)) {
		errno = 0;
		return IPF_NOTE;
	}

	/* FIXME: Use accessor instead of direct access */
	return lpProps[0].value.lpszA;
}


static zval *get_child_folders(TALLOC_CTX *mem_ctx,
			       mapi_object_t *parent,
			       mapi_id_t folder_id, int count)
{
	enum MAPISTATUS		retval;
	bool			ret;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		rowset;
	const char		*name;
	const char		*comment;
	const uint32_t		*total;
	const uint32_t		*unread;
	const uint32_t		*child;
	uint32_t		index;
	const uint64_t		*fid;
	int			i;
	zval			*folders;

	MAKE_STD_ZVAL(folders);
	array_init(folders);

	mapi_object_init(&obj_folder);
	retval = OpenFolder(parent, folder_id, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return folders;

	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, NULL);
	if (retval != MAPI_E_SUCCESS) return folders;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x6,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_FID,
					  PR_COMMENT_UNICODE,
					  PR_CONTENT_UNREAD,
					  PR_CONTENT_COUNT,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return folders;


	while (((retval = QueryRows(&obj_htable, 0x32,
				    TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND) && rowset.cRows) {
		for (index = 0; index < rowset.cRows; index++) {
			zval	*current_folder;

			MAKE_STD_ZVAL(current_folder);
			array_init(current_folder);


			long n_total, n_unread;

			fid = (const uint64_t *)find_SPropValue_data(&rowset.aRow[index], PR_FID);
			name = (const char *)find_SPropValue_data(&rowset.aRow[index],
								  PR_DISPLAY_NAME_UNICODE);
			comment = (const char *)find_SPropValue_data(&rowset.aRow[index],
								     PR_COMMENT_UNICODE);
			if (comment == NULL) comment = "";

      /*** XXX temporary workaround for child folders problem ***/
			{
				mapi_object_t obj_child;
				mapi_object_t obj_ctable;
				uint32_t      count = 0;
				uint32_t      count2 = 0;

				mapi_object_init(&obj_child);
				mapi_object_init(&obj_ctable);

				retval = OpenFolder(&obj_folder, *fid, &obj_child);

				if (retval != MAPI_E_SUCCESS) return false;
				retval = GetContentsTable(&obj_child, &obj_ctable, 0, &count);
				total = &count;

				mapi_object_release(&obj_ctable);
				mapi_object_init(&obj_ctable);

				retval = GetHierarchyTable(&obj_child, &obj_ctable, 0, &count2);
				child = &count2;

				mapi_object_release(&obj_ctable);
				mapi_object_release(&obj_child);
			}

			// XXX commented by workaround of child problem
			/* total = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_CONTENT_COUNT); */
			if (!total) {
				n_total = 0;
			} else {
				n_total = (long) *total;
			}

			unread = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_CONTENT_UNREAD);
			if (!unread) {
				n_unread =0;
			} else {
				n_unread =  (long) *unread;
			}

			// XXX commented by workaround of child problems
			//      child = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_FOLDER_CHILD_COUNT);

			const char *container = get_container_class(mem_ctx, parent, *fid);

			add_assoc_string(current_folder, "name",(char*)  name, 1);
			add_assoc_mapi_id_t(current_folder, "fid", *fid);
			add_assoc_string(current_folder, "comment", (char*) comment, 1);
			add_assoc_string(current_folder, "container", (char*) container, 1);

			add_assoc_long(current_folder, "total", n_total);
			add_assoc_long(current_folder, "uread", n_unread);

			if (child && *child) {
				zval *child_folders  = get_child_folders(mem_ctx, &obj_folder, *fid, count + 1);
				add_assoc_zval(current_folder, "childs", child_folders);
			}

			add_next_index_zval(folders, current_folder);
		}
	}

	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_folder);

	return folders;
}



PHP_METHOD(MAPISession, mailbox)
{
	char			*username = NULL;
	int			username_len;
	zval                    *mailbox_obj;
        struct mapi_profile	*profile;
	TALLOC_CTX		*talloc_ctx;
	mapi_session_object_t   *this_obj;
	zval			*this_php_obj;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s",
				  &username, &username_len) == FAILURE) {
		RETURN_NULL();
	}

	this_php_obj = getThis();

        if (username == NULL) {
		struct mapi_profile *profile = session_get_profile(this_php_obj TSRMLS_CC);
                username  = (char *) profile->username;
        }

	mailbox_obj = create_mailbox_object(this_php_obj, username TSRMLS_CC);

	this_obj = (mapi_session_object_t *) zend_object_store_get_object(this_php_obj TSRMLS_CC);
	add_index_zval(this_obj->children_mailboxes, (long) Z_OBJ_HANDLE_P(mailbox_obj), mailbox_obj);

	RETURN_ZVAL(mailbox_obj, 0, 0);
}

PHP_METHOD(MAPISession, folders)
{
	enum MAPISTATUS		retval;
	zval			*this_obj;
	TALLOC_CTX		*this_talloc_ctx;
	TALLOC_CTX		*talloc_ctx;
	struct mapi_session	*session = NULL;
	struct mapi_profile	*profile;
	mapi_object_t		obj_store;
	mapi_id_t		id_mailbox;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		cValues;
	const char		*mailbox_name;


	this_obj = getThis();
	profile = session_get_profile(this_obj TSRMLS_CC);
	session = mapi_session_get_session(this_obj TSRMLS_CC);

	// open user mailbox
	init_message_store(&obj_store, session, false, (char *) profile->username);
	this_talloc_ctx = OBJ_GET_TALLOC_CTX(mapi_session_object_t *, this_obj);
	talloc_ctx = talloc_named(this_talloc_ctx, 0, "MAPISession::folders");

	/* Retrieve the mailbox folder name */
	SPropTagArray = set_SPropTagArray(talloc_ctx, 0x1, PR_DISPLAY_NAME_UNICODE);
	retval = GetProps(&obj_store, MAPI_UNICODE, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		php_error(E_ERROR, "Get mailbox properties: %s", mapi_get_errstr(retval));
	}

	/* FIXME: Use accessor instead of direct access */
	if (lpProps[0].value.lpszW) {
		mailbox_name = lpProps[0].value.lpszW;
	} else {
		php_error(E_ERROR, "Get mailbox name");
	}

	/* Prepare the directory listing */
	retval = GetDefaultFolder(&obj_store, &id_mailbox, olFolderTopInformationStore);
	if (retval != MAPI_E_SUCCESS) {
		php_error(E_ERROR, "Get default folder: %s", mapi_get_errstr(retval));
	}

	array_init(return_value);
	add_assoc_string(return_value, "name", (char*) mailbox_name, 1);
	add_assoc_zval(return_value, "childs", get_child_folders(talloc_ctx, &obj_store, id_mailbox, 0));

	talloc_free(talloc_ctx);
}

/**
 * fetch the user INBOX
 */

/*
 * Optimized dump message routine (use GetProps rather than GetPropsAll)
 */
_PUBLIC_ zval* dump_message_to_zval(TALLOC_CTX *mem_ctx,
                                        mapi_object_t *obj_message)
{
	zval				*message = NULL;
        enum MAPISTATUS			retval;
        struct SPropTagArray		*SPropTagArray;
        struct SPropValue		*lpProps;
        struct SRow			aRow;
        uint32_t			count;
	char				*err_str;
        /* common email fields */
        const char			*msgid;
        const char			*from, *to, *cc, *bcc;
        const char			*subject;
        DATA_BLOB			body;
        const uint8_t			*has_attach;
        const uint32_t			*cp;
        const char			*codepage;

        /* Build the array of properties we want to fetch */
        SPropTagArray = set_SPropTagArray(mem_ctx, 19,
                                          PR_INTERNET_MESSAGE_ID,
                                          PR_INTERNET_MESSAGE_ID_UNICODE,
                                          PR_CONVERSATION_TOPIC,
                                          PR_CONVERSATION_TOPIC_UNICODE,
                                          PR_MSG_EDITOR_FORMAT,
                                          PR_BODY_UNICODE,
                                          PR_HTML,
                                          PR_RTF_IN_SYNC,
                                          PR_RTF_COMPRESSED,
                                          PR_SENT_REPRESENTING_NAME,
                                          PR_SENT_REPRESENTING_NAME_UNICODE,
                                          PR_DISPLAY_TO,
                                          PR_DISPLAY_TO_UNICODE,
                                          PR_DISPLAY_CC,
                                          PR_DISPLAY_CC_UNICODE,
                                          PR_DISPLAY_BCC,
                                          PR_DISPLAY_BCC_UNICODE,
                                          PR_HASATTACH,
                                          PR_MESSAGE_CODEPAGE);
        lpProps = NULL;
        retval = GetProps(obj_message, MAPI_UNICODE, SPropTagArray, &lpProps, &count);
        MAPIFreeBuffer(SPropTagArray);
        CHECK_MAPI_RETVAL(retval, "Get message props");

        /* Build a SRow structure */
        aRow.ulAdrEntryPad = 0;
        aRow.cValues = count;
        aRow.lpProps = lpProps;

        msgid = (const char *) octool_get_propval(&aRow, PR_INTERNET_MESSAGE_ID);
        subject = (const char *) octool_get_propval(&aRow, PR_CONVERSATION_TOPIC);

        retval = octool_get_body(mem_ctx, obj_message, &aRow, &body);

        if (retval != MAPI_E_SUCCESS) {
		MAKE_STD_ZVAL(message);
		array_init(message);
		add_assoc_string(message, "msgid", msgid ? (char *) msgid : "", 1);
		add_assoc_long(message, "Error", (long) retval);
		err_str = (char *) mapi_get_errstr(retval);
		add_assoc_string(message, "ErrorString",  err_str ? err_str : "Unknown" , 1);
		return message;
        }

        from = (const char *) octool_get_propval(&aRow, PR_SENT_REPRESENTING_NAME);
        to = (const char *) octool_get_propval(&aRow, PR_DISPLAY_TO_UNICODE);
        cc = (const char *) octool_get_propval(&aRow, PR_DISPLAY_CC_UNICODE);
        bcc = (const char *) octool_get_propval(&aRow, PR_DISPLAY_BCC_UNICODE);

        has_attach = (const uint8_t *) octool_get_propval(&aRow, PR_HASATTACH);
        cp = (const uint32_t *) octool_get_propval(&aRow, PR_MESSAGE_CODEPAGE);
        switch (cp ? *cp : 0) {
        case CP_USASCII:
                codepage = "CP_USASCII";
                break;
        case CP_UNICODE:
                codepage = "CP_UNICODE";
                break;
        case CP_JAUTODETECT:
                codepage = "CP_JAUTODETECT";
                break;
        case CP_KAUTODETECT:
                codepage = "CP_KAUTODETECT";
                break;
        case CP_ISO2022JPESC:
                codepage = "CP_ISO2022JPESC";
                break;
        case CP_ISO2022JPSIO:
                codepage = "CP_ISO2022JPSIO";
                break;
        default:
                codepage = "";
                break;
        }

        MAKE_STD_ZVAL(message);
        array_init(message);
        add_assoc_string(message, "msgid", msgid ? (char*) msgid : "", 1);
        add_assoc_string(message, "subject", subject ? (char*) subject : "", 1);
        add_assoc_string(message, "from", from ? (char*) from : "", 1);
        add_assoc_string(message, "to", to ? (char*) to : "", 1);
        add_assoc_string(message, "cc", cc ? (char*) cc : "", 1);
        add_assoc_string(message, "bcc", bcc ? (char*) bcc : "", 1);
        // attachments added later
        add_assoc_string(message, "codepage",  codepage ? (char*) codepage : "", 1);
        add_assoc_string(message, "body", body.data, 1);

        talloc_free(body.data);

        return message;
}

static const char *get_filename(const char *filename)
{
        const char *substr;

        if (!filename) return NULL;

        substr = rindex(filename, '/');
        if (substr) return substr + 1;

        return filename;
}

static uint32_t open_default_inbox_folder(mapi_object_t *obj_store,
					  mapi_object_t *obj_inbox,
					  mapi_object_t *obj_table)
{
	enum MAPISTATUS  retval;
	uint64_t	id_inbox;
	uint32_t	count;

	retval = GetReceiveFolder(obj_store, &id_inbox, NULL);
	if (retval != MAPI_E_SUCCESS) {
		php_error(E_ERROR, "Get receive default folder: %s", mapi_get_errstr(retval));
	}

	retval = OpenFolder(obj_store, id_inbox, obj_inbox);
	if (retval != MAPI_E_SUCCESS) {
		php_error(E_ERROR, "Open receive default folder: %s", mapi_get_errstr(retval));
	}

	retval = GetContentsTable(obj_inbox, obj_table, 0, &count);
	if (retval != MAPI_E_SUCCESS) {
		php_error(E_ERROR, "Get contents of folder: %s", mapi_get_errstr(retval));
	}

	return count;
}

static zval *do_fetchmail(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store)
{
	zval			*messages = NULL;
	enum MAPISTATUS		retval;
	bool			status;
	mapi_object_t		obj_tis;
	mapi_object_t		obj_inbox;
	mapi_object_t		obj_message;
	mapi_object_t		obj_table;
	mapi_object_t		obj_tb_attach;
	mapi_object_t		obj_attach;
	uint64_t		id_inbox;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		rowset;
	struct SRowSet		rowset_attach;
	uint32_t		i, j;
	uint32_t		count;
	const uint8_t		*has_attach;
	const uint32_t		*attach_num;
	const char		*attach_filename;
	const uint32_t		*attach_size;
	bool			summary = false;// TODO: support summary
	const char		*store_folder = NULL; // TODO: support store_folder

	mapi_object_init(&obj_tis);
	mapi_object_init(&obj_inbox);
	mapi_object_init(&obj_table);

	MAKE_STD_ZVAL(messages);
	array_init(messages);

	// XXX Only supported get all mails from session
	// TODO: public folder; get mails from a folder
	count = open_default_inbox_folder(obj_store, &obj_inbox, &obj_table);
	if (!count) goto end;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT_UNICODE);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	CHECK_MAPI_RETVAL(retval, "SetColumns");

	while ((retval = QueryRows(&obj_table, count, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		count -= rowset.cRows;
		for (i = 0; i < rowset.cRows; i++) {
			zval	*message = NULL;

			mapi_object_init(&obj_message);
			retval = OpenMessage(obj_store,
					     rowset.aRow[i].lpProps[0].value.d,
					     rowset.aRow[i].lpProps[1].value.d,
					     &obj_message, 0);
			if (GetLastError() == MAPI_E_SUCCESS) {
				if (summary) {
					// TODO suport summary
					mapidump_message_summary(&obj_message);
				} else {
					struct SPropValue       *lpProps;
					struct SRow             aRow;

					SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_HASATTACH);
					lpProps = NULL;
					retval = GetProps(&obj_message, 0, SPropTagArray, &lpProps, &count);
					MAPIFreeBuffer(SPropTagArray);
					if (retval != MAPI_E_SUCCESS) {
						php_error(E_ERROR, "%s", mapi_get_errstr(retval));
					}

					aRow.ulAdrEntryPad = 0;
					aRow.cValues = count;
					aRow.lpProps = lpProps;

					message = dump_message_to_zval(mem_ctx, &obj_message);
					has_attach = (const uint8_t *) get_SPropValue_SRow_data(&aRow, PR_HASATTACH);

					/* If we have attachments, retrieve them */
					if (has_attach && *has_attach) {
						mapi_object_init(&obj_tb_attach);
						retval = GetAttachmentTable(&obj_message, &obj_tb_attach);
						if (retval == MAPI_E_SUCCESS) {
							SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_ATTACH_NUM);
							retval = SetColumns(&obj_tb_attach, SPropTagArray);
							if (retval != MAPI_E_SUCCESS) {
								php_error(E_ERROR, "%s", mapi_get_errstr(retval));
							}
							MAPIFreeBuffer(SPropTagArray);

							retval = QueryRows(&obj_tb_attach, 0xa, TBL_ADVANCE, &rowset_attach);
							if (retval != MAPI_E_SUCCESS) {
								php_error(E_ERROR, "%s", mapi_get_errstr(retval));
							}

							zval	*attachments;

							if (rowset_attach.cRows > 0) {
								MAKE_STD_ZVAL(attachments);
								array_init(attachments);
							}

							for (j = 0; j < rowset_attach.cRows; j++) {
								attach_num = (const uint32_t *)find_SPropValue_data(&(rowset_attach.aRow[j]), PR_ATTACH_NUM);
								retval = OpenAttach(&obj_message, *attach_num, &obj_attach);
								if (retval == MAPI_E_SUCCESS) {
									zval			*attachment;
									struct SPropValue       *lpProps2;
									uint32_t                count2;

									MAKE_STD_ZVAL(attachment);
									array_init(attachment);

									SPropTagArray = set_SPropTagArray(mem_ctx, 0x4,
													  PR_ATTACH_FILENAME,
													  PR_ATTACH_LONG_FILENAME,
													  PR_ATTACH_SIZE,
													  PR_ATTACH_CONTENT_ID);
									lpProps2 = NULL;
									retval = GetProps(&obj_attach, MAPI_UNICODE, SPropTagArray, &lpProps2, &count2);
									MAPIFreeBuffer(SPropTagArray);
									if (retval != MAPI_E_SUCCESS) {
										php_error(E_ERROR, "%s", mapi_get_errstr(retval));
									}

									aRow.ulAdrEntryPad = 0;
									aRow.cValues = count2;
									aRow.lpProps = lpProps2;

									attach_filename = get_filename(octool_get_propval(&aRow, PR_ATTACH_LONG_FILENAME));
									if (!attach_filename || (attach_filename && !strcmp(attach_filename, ""))) {
										attach_filename = get_filename(octool_get_propval(&aRow, PR_ATTACH_FILENAME));
									}
									attach_size = (const uint32_t *) octool_get_propval(&aRow, PR_ATTACH_SIZE);
									php_printf("[%u] %s (%u Bytes)\n", j, attach_filename, attach_size ? *attach_size : 0);
									fflush(0);
									if (store_folder) {
										// TODO: store_folder
										//                    status = store_attachment(obj_attach, attach_filename, attach_size ? *attach_size : 0, oclient);
										if (status == false) {
											talloc_free(mem_ctx);
											//                      return MAPI_E_UNABLE_TO_COMPLETE;
											php_error(E_ERROR, "A Problem was encountered while storing attachments on the filesystem\n");
										}
									}
									MAPIFreeBuffer(lpProps2);
								}
							} // end for attachments
							if (rowset_attach.cRows > 0) {
								add_assoc_zval(message, "Attachments", attachments);
							}
							errno = 0;
						}
						MAPIFreeBuffer(lpProps);
					}
				}
			}
			mapi_object_release(&obj_message);
			if (message) {
				add_next_index_zval(messages, message);
			}
		}
	}
end:
	mapi_object_release(&obj_table);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_tis);

	talloc_free(mem_ctx);

	errno = 0;
	return messages;
}

static void open_user_mailbox(struct mapi_profile *profile,
			      struct mapi_session *session,
			      mapi_object_t *obj_store)
{
	enum MAPISTATUS	retval;

	retval = OpenUserMailbox(session, (char *) profile->username, obj_store);
	if (retval != MAPI_E_SUCCESS) {
		php_error(E_ERROR, "OpenUserMailbox %s", mapi_get_errstr(retval));
	}

	init_message_store(obj_store, session, false, (char *) profile->username);
}



PHP_METHOD(MAPISession, fetchmail)
{
	zval			*this_obj;
	zval			*messages;
	TALLOC_CTX		*this_mem_ctx;
	TALLOC_CTX		*mem_ctx;
	mapi_object_t		obj_store;
	mapi_object_t		user_mbox;
	struct mapi_session	*session;
	struct mapi_profile	*profile;
	mapi_id_t		id_mailbox;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		cValues;
	const char		*mailbox_name;

	this_obj = getThis();
	mapi_object_init(&user_mbox);
	session = mapi_session_get_session(this_obj TSRMLS_CC);
	profile = session_get_profile(this_obj TSRMLS_CC);

	open_user_mailbox(profile, session, &user_mbox);

	this_obj = getThis();
	profile = session_get_profile(this_obj TSRMLS_CC);
	session = mapi_session_get_session(this_obj TSRMLS_CC);

	// open user mailbox
	init_message_store(&obj_store, session, false, (char *) profile->username);

	this_mem_ctx = OBJ_GET_TALLOC_CTX(mapi_session_object_t *, this_obj);
	mem_ctx = talloc_named(this_mem_ctx, 0, "do_fetchmail");

	messages = do_fetchmail(mem_ctx, &user_mbox);
	RETURN_ZVAL(messages, 1, 1);
}


const char *mapi_date(TALLOC_CTX *parent_ctx,
		      struct mapi_SPropValue_array *properties,
		      uint32_t mapitag)
{
	const char		*date = NULL;
	TALLOC_CTX		*mem_ctx;
	NTTIME			time;
	const struct FILETIME	*filetime;

	filetime = (const struct FILETIME *) find_mapi_SPropValue_data(properties, mapitag);
	if (filetime) {
		const char *nt_date_str;

		time = filetime->dwHighDateTime;
		time = time << 32;
		time |= filetime->dwLowDateTime;
		mem_ctx = talloc_named(parent_ctx, 0, "mapi_date_string");
		nt_date_str = nt_time_string(mem_ctx, time);
		date = estrdup(nt_date_str);
		talloc_free(mem_ctx);
	}

	return date;
}


/**
   \details This function dumps the properties relating to an appointment to a zval array

   The expected way to obtain the properties array is to use OpenMessage() to obtain the
   appointment object, then to use GetPropsAll() to obtain all the properties.

   \param talloc memory context to be used
   \param properties array of appointment properties
   \param id identification to display for the appointment (can be NULL)

   \sa mapidump_message, mapidump_contact, mapidump_task, mapidump_note
*/
static zval *appointment_zval (TALLOC_CTX *mem_ctx,
			       struct mapi_SPropValue_array *properties,
			       const char *id)
{
	zval				*appointment;
	zval				*contacts_list;
	const struct mapi_SLPSTRArray	*contacts = NULL;
	const char			*subject = NULL;
	const char			*location= NULL;
	const char			*timezone = NULL;
	const uint32_t			*status;
	const uint8_t			*priv = NULL;
	uint32_t			i;

	contacts = (const struct mapi_SLPSTRArray *)find_mapi_SPropValue_data(properties, PidLidContacts);
	subject = find_mapi_SPropValue_data(properties, PR_CONVERSATION_TOPIC);
	timezone = find_mapi_SPropValue_data(properties, PidLidTimeZoneDescription);
	location = find_mapi_SPropValue_data(properties, PidLidLocation);
	status = (const uint32_t *)find_mapi_SPropValue_data(properties, PidLidBusyStatus);
	priv = (const uint8_t *)find_mapi_SPropValue_data(properties, PidLidPrivate);

	MAKE_STD_ZVAL(appointment);
	array_init(appointment);
	add_assoc_string(appointment, "id", id ? (char*) id : "", 1);
	add_assoc_string(appointment, "subject", subject ? (char*) subject : "", 1);
	add_assoc_string(appointment, "location", location ? (char*) location : "", 1);
	add_assoc_string(appointment, "startDate", (char*) mapi_date(mem_ctx, properties, PR_START_DATE), 0);
	add_assoc_string(appointment, "endDate", (char*) mapi_date(mem_ctx, properties, PR_END_DATE), 0);
	add_assoc_string(appointment, "timezone",  timezone ? (char*) timezone : "", 1);
	add_assoc_bool(appointment, "private",  (priv && (*priv == true)) ? true : false);
	//maybe return strign directly? get_task_status(*status));
	add_assoc_mapi_id_t(appointment, "status", *status);

	MAKE_STD_ZVAL(contacts_list);
	array_init(contacts_list);
	if (contacts) {
		for (i = 0; i < contacts->cValues; i++) {
			add_next_index_string(contacts_list,  contacts->strings[i].lppszA, 1);
		}
	}
	add_assoc_zval(appointment, "contacts", contacts_list);
	return appointment;
}

static zval *contact_zval (TALLOC_CTX *mem_ctx,
			   struct mapi_SPropValue_array *properties,
			   const char *id)
{
	zval		*contact;
	const char	*card_name = (const char *)find_mapi_SPropValue_data(properties, PidLidFileUnder);
	const char	*topic = (const char *)find_mapi_SPropValue_data(properties, PR_CONVERSATION_TOPIC);
	const char	*company = (const char *)find_mapi_SPropValue_data(properties, PR_COMPANY_NAME);
	const char	*title = (const char *)find_mapi_SPropValue_data(properties, PR_TITLE);
	const char	*full_name = (const char *)find_mapi_SPropValue_data(properties, PR_DISPLAY_NAME);
	const char	*given_name = (const char *)find_mapi_SPropValue_data(properties, PR_GIVEN_NAME);
	const char	*surname = (const char *)find_mapi_SPropValue_data(properties, PR_SURNAME);
	const char	*department = (const char *)find_mapi_SPropValue_data(properties, PR_DEPARTMENT_NAME);
	const char	*email = (const char *)find_mapi_SPropValue_data(properties, PidLidEmail1OriginalDisplayName);
	const char	*office_phone = (const char *)find_mapi_SPropValue_data(properties, PR_OFFICE_TELEPHONE_NUMBER);
	const char	*home_phone = (const char *)find_mapi_SPropValue_data(properties, PR_HOME_TELEPHONE_NUMBER);
	const char	*mobile_phone = (const char *)find_mapi_SPropValue_data(properties, PR_MOBILE_TELEPHONE_NUMBER);
	const char	*business_fax = (const char *)find_mapi_SPropValue_data(properties, PR_BUSINESS_FAX_NUMBER);
	const char	*business_home_page = (const char *)find_mapi_SPropValue_data(properties, PR_BUSINESS_HOME_PAGE);
	const char	*postal_address = (const char*)find_mapi_SPropValue_data(properties, PR_POSTAL_ADDRESS);
	const char	*street_address = (const char*)find_mapi_SPropValue_data(properties, PR_STREET_ADDRESS);
	const char	*locality = (const char*)find_mapi_SPropValue_data(properties, PR_LOCALITY);
	const char	*state = (const char*)find_mapi_SPropValue_data(properties, PR_STATE_OR_PROVINCE);
	const char	*country = (const char*)find_mapi_SPropValue_data(properties, PR_COUNTRY);

	MAKE_STD_ZVAL(contact);
	array_init(contact);

	add_assoc_string(contact, "id", id ? (char*) id : "", 1);
	if (card_name) {
		add_assoc_string(contact, "cardName", card_name ? (char*) card_name : "", 1);
	}
	if (topic) {
		add_assoc_string(contact, "topic", topic ? (char*) topic : "", 1);
	}
	if (full_name) {
		add_assoc_string(contact, "fullName", full_name ? (char*) full_name : "", 1);
	} else if (given_name && surname) {
		size_t full_name_len = strlen(given_name) + strlen(surname) + 1;
		full_name = (const char*) emalloc(full_name_len);
		snprintf((char*) full_name, full_name_len, "%s %s", given_name, surname);
		add_assoc_string(contact, "fullName", (char*) full_name, 0);
	}
	if (title) {
		add_assoc_string(contact, "title", title ? (char*) title : "", 1);
	}
	if (department) {
		add_assoc_string(contact, "department", department ? (char*) department : "", 1);
	}
	if (company) {
		add_assoc_string(contact, "company", company ? (char*) company : "", 1);
	}
	if (email) {
		add_assoc_string(contact, "email", email ? (char*) email : "", 1);
	}
	if (office_phone) {
		add_assoc_string(contact, "officePhone", office_phone ? (char*) office_phone : "", 1);
	}
	if (home_phone) {
		add_assoc_string(contact, "homePhone", home_phone ? (char*) home_phone : "", 1);
	}
	if (mobile_phone) {
		add_assoc_string(contact, "mobilePhone", mobile_phone ? (char*) mobile_phone : "", 1);
	}
	if (business_fax) {
		add_assoc_string(contact, "businessFax", business_fax ? (char*) business_fax : "", 1);
	}
	if (business_home_page) {
		add_assoc_string(contact, "businessHomePage", business_home_page ? (char*) business_home_page : "", 1);
	}
	if (postal_address) {
		add_assoc_string(contact, "postalAddress", postal_address ? (char*) postal_address : "", 1);
	}
	if (street_address) {
		add_assoc_string(contact, "homePhone", street_address ? (char*) street_address : "", 1);
	}
	if (locality) {
		add_assoc_string(contact, "locality", locality ? (char*) locality : "", 1);
	}
	if (state) {
		add_assoc_string(contact, "state", state ? (char*) state : "", 1);
	}
	if (country) {
		add_assoc_string(contact, "country", country ? (char*) country : "", 1);
	}

	return contact;
}

// not works longer
static zval *fetch_items(TALLOC_CTX *mem_ctx,
			 mapi_object_t *obj_store,
			 const char *item)
{
	zval				*items;
	enum MAPISTATUS			retval;
	mapi_object_t			obj_tis;
	mapi_object_t			obj_folder;
	mapi_object_t			obj_table;
	mapi_object_t			obj_message;
	mapi_id_t			fid;
	uint32_t			olFolder = 0;
	struct SRowSet			SRowSet;
	struct SPropTagArray		*SPropTagArray;
	struct mapi_SPropValue_array	properties_array;
	uint32_t			count;
	uint32_t			i;
	char				*id;

	if (!item) {
		php_error(E_ERROR, "Missing item parameter");
	}

	mapi_object_init(&obj_tis);
	mapi_object_init(&obj_folder);

	/* for (i = 0; defaultFolders[i].olFolder; i++) { */
	/* 	if (!strncasecmp(defaultFolders[i].container_class, item, strlen(defaultFolders[i].container_class))) { */
	/* 		olFolder = defaultFolders[i].olFolder; */
	/* 	} */
	/* } */
	if (!olFolder) {
		php_error(E_ERROR, "Cannot found defualt folder for items of type %s", item);
	}

	// XXX open default folder for the type
	// MACRO form api erros
	retval = GetDefaultFolder(obj_store, &fid, olFolder);
	CHECK_MAPI_RETVAL(retval, "GetDefaultFolder for item type");

	/* We now open the folder */
	retval = OpenFolder(obj_store, fid, &obj_folder);
	CHECK_MAPI_RETVAL(retval, "OpenFolder");

	/* Operations on the  folder */
	mapi_object_init(&obj_table);
	retval = GetContentsTable(&obj_folder, &obj_table, 0, &count);
	CHECK_MAPI_RETVAL(retval, "GetContentsTable");

	MAKE_STD_ZVAL(items);
	array_init(items);

	if (!count) {
		mapi_object_release(&obj_table);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_tis);

		return items;
	}

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x8,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT_UNICODE,
					  PR_MESSAGE_CLASS_UNICODE,
					  PR_RULE_MSG_PROVIDER,
					  PR_RULE_MSG_NAME);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	CHECK_MAPI_RETVAL(retval, "SetColumns");

	while ((retval = QueryRows(&obj_table, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND && SRowSet.cRows) {
		count -= SRowSet.cRows;
		for (i = 0; i < SRowSet.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(&obj_folder,
					     SRowSet.aRow[i].lpProps[0].value.d,
					     SRowSet.aRow[i].lpProps[1].value.d,
					     &obj_message, 0);
			if (retval != MAPI_E_NOT_FOUND) {
				// XXX not summary for now
				//                            if (oclient->summary) {
				/*   mapidump_message_summary(&obj_message); */
				/* } else { */
				retval = GetPropsAll(&obj_message, MAPI_UNICODE, &properties_array);
				if (retval == MAPI_E_SUCCESS) {
					zval	*item = NULL;

					id = talloc_asprintf(mem_ctx, "%" PRIx64 "/%" PRIx64,
							     SRowSet.aRow[i].lpProps[0].value.d,
							     SRowSet.aRow[i].lpProps[1].value.d);
					mapi_SPropValue_array_named(&obj_message,
								    &properties_array);
					switch (olFolder) {
					case olFolderCalendar:
						item = appointment_zval(mem_ctx, &properties_array, id);
						break;
					case olFolderInbox:
						php_error(E_ERROR, "Inbox folder case not implemented");
						break;
					case olFolderContacts:
						item = contact_zval(mem_ctx, &properties_array, id);
						break;
					case olFolderTasks:
						php_error(E_ERROR, "Tasks folder case not implemented");
						break;
					case olFolderNotes:
						php_error(E_ERROR, "Notes folder case not implemented");
						break;
					}
					talloc_free(id);
					if (item) {
						add_next_index_zval(items, item);
					}
				}
				mapi_object_release(&obj_message);
			}
		}
	}

	mapi_object_release(&obj_table);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_tis);

	return items;
}


PHP_METHOD(MAPISession, appointments)
{
	zval			*this_obj;
	zval			*appointments;
	mapi_object_t		user_mbox;
	struct mapi_session	*session;
	struct mapi_profile	*profile;
	TALLOC_CTX		*mem_ctx;
	TALLOC_CTX		*this_mem_ctx;

	mapi_object_init(&user_mbox);

	this_obj =  getThis();
	session = mapi_session_get_session(this_obj TSRMLS_CC);
	profile = session_get_profile(this_obj TSRMLS_CC);

	open_user_mailbox(profile, session, &user_mbox);

	this_mem_ctx = OBJ_GET_TALLOC_CTX(mapi_session_object_t *, this_obj);
	mem_ctx = talloc_named(this_mem_ctx, 0, "appointments");

	appointments = fetch_items(mem_ctx, &user_mbox,  "Appointment");
	talloc_free(mem_ctx);
	RETURN_ZVAL(appointments, 1, 1);
}

PHP_METHOD(MAPISession, contacts)
{
	zval			*this_obj;
	zval			*contacts;
	mapi_object_t		user_mbox;
	struct mapi_session	*session;
	struct mapi_profile	*profile;
	TALLOC_CTX		*mem_ctx;
	TALLOC_CTX		*this_mem_ctx;

	mapi_object_init(&user_mbox);

	this_obj = getThis();
	session = mapi_session_get_session(this_obj TSRMLS_CC);
	profile = session_get_profile(this_obj TSRMLS_CC);

	open_user_mailbox(profile, session, &user_mbox);

	this_mem_ctx = OBJ_GET_TALLOC_CTX(mapi_session_object_t*, this_obj);
	mem_ctx = talloc_named(this_mem_ctx, 0, "contacts");

	contacts = fetch_items(mem_ctx, &user_mbox,  "Contact");
	talloc_free(mem_ctx);
	RETURN_ZVAL(contacts, 1, 1);
}


void mapi_session_remove_children_mailbox(zval *mapi_session, zend_object_handle mailbox_handle TSRMLS_DC)
{
	mapi_session_object_t	*this_obj;

	this_obj = (mapi_session_object_t*) zend_object_store_get_object(mapi_session TSRMLS_CC);
	zend_hash_index_del(this_obj->children_mailboxes->value.ht, (long) mailbox_handle);
}
