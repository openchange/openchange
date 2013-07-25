#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"
#include "mapi_session.h"
//#include "utils/openchange-tools.h"

extern int session_resource_id;

static zend_function_entry mapi_session_class_functions[] = {
  PHP_ME(MAPISession, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
  // PHP_ME(MAPISession, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
  PHP_ME(MAPISession, folders, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(MAPISession, fetchmail, NULL, ZEND_ACC_PUBLIC)
  { NULL, NULL, NULL }
};

void MAPISessionRegisterClass()
{
  zend_class_entry ce;
  INIT_CLASS_ENTRY(ce, "MAPISession", mapi_session_class_functions);
  zend_register_internal_class(&ce TSRMLS_CC);
}

struct mapi_session* get_session(zval* session_obj)
{
  zval** session_resource;
  struct mapi_session* session;

  if (zend_hash_find(Z_OBJPROP_P(session_obj),
                     "session", sizeof("session"), (void**)&session_resource) == FAILURE) {
    php_error(E_ERROR, "session attribute not found");
  }

  ZEND_FETCH_RESOURCE_NO_RETURN(session, struct mapi_session**, session_resource, -1, SESSION_RESOURCE_NAME, session_resource_id);
  if (session  == NULL) {
    php_error(E_ERROR, "session resource not correctly fetched");
  }

  return session;
}

struct mapi_profile* session_obj_get_profile(zval* session_obj)
{
  zval** profile_obj;
  if (zend_hash_find(Z_OBJPROP_P(session_obj),
                     "profile", sizeof("profile"), (void**)&profile_obj) == FAILURE) {
    php_error(E_ERROR, "profile attribute not found");
  }
  return get_profile(*profile_obj);
}


static void init_message_store(mapi_object_t *store, struct mapi_session* session, bool public_folder, char* username)
{
  enum MAPISTATUS retval;

  mapi_object_init(store);
  if (public_folder == true) {
    retval = OpenPublicFolder(session, store);
    if (retval != MAPI_E_SUCCESS) {
      php_error(E_ERROR, "Open public folder: %s", mapi_get_errstr(retval));
    }
  } else if (username) {
    retval = OpenUserMailbox(session, username, store);
    if (retval != MAPI_E_SUCCESS) {
      php_error(E_ERROR, "Open user mailbox: %s", mapi_get_errstr(retval));
    }
  } else {
    retval = OpenMsgStore(session, store);
    if (retval != MAPI_E_SUCCESS) {
      php_error(E_ERROR, "Open message store: %s",  mapi_get_errstr(retval));
    }
  }
}


PHP_METHOD(MAPISession, __construct)
{
  zval* this_obj;
  zval* profile_object;
  zval* session_resource;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "or", &profile_object, &session_resource) == FAILURE ) {
    RETURN_NULL();
  }

  if (strncmp(Z_OBJCE_P(profile_object)->name, "MAPIProfile", sizeof("MAPIProfile")+1) != 0) {
    php_error(E_ERROR, "The object must be of the class MAPIProfile");
  }

  this_obj = getThis();
  add_property_zval(this_obj, "profile", profile_object);
  add_property_zval(this_obj, "session", session_resource);
}

PHP_METHOD(MAPISession, folders)
{
   enum MAPISTATUS retval;
  zval* this_obj;
  TALLOC_CTX *mem_ctx;
  struct mapi_session* session = NULL;
  struct mapi_profile* profile;
  mapi_object_t obj_store;

  this_obj = getThis();
  //  mapi_ctx = get_mapi_context(this_obj);

  //  TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "openchangeclient");
  /* zval** profile_obj; */
  /* if (zend_hash_find(Z_OBJPROP_P(this_obj), */
  /*                    "profile", sizeof("profile"), (void**)&profile_obj) == FAILURE) { */
  /*   php_error(E_ERROR, "profile attribute not found"); */
  /* } */
  /* profile = get_profile(*profile_obj); */
  profile = session_obj_get_profile(this_obj);
  session = get_session(this_obj);



  // open user mailbox
  init_message_store(&obj_store, session, false, (char*) profile->username);

  mapi_id_t                     id_mailbox;
  struct SPropTagArray          *SPropTagArray;
  struct SPropValue             *lpProps;
  uint32_t                      cValues;
  const char                    *mailbox_name;

  mem_ctx = talloc_named(NULL, 0, "MAPISession::folders"); // TODO XXX destroy somehow in php_error

  /* Retrieve the mailbox folder name */
  SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_DISPLAY_NAME_UNICODE);
  retval = GetProps(&obj_store, MAPI_UNICODE, SPropTagArray, &lpProps, &cValues);
  MAPIFreeBuffer(SPropTagArray);
  if (retval != MAPI_E_SUCCESS) {
    php_error(E_ERROR, "Get mailbox properties: %s", mapi_get_errstr(retval));
  }

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
  add_assoc_zval(return_value, "childs", get_child_folders(mem_ctx, &obj_store, id_mailbox, 0));

  talloc_free(mem_ctx);
}

static zval* get_child_folders(TALLOC_CTX *mem_ctx, mapi_object_t *parent, mapi_id_t folder_id, int count)
{
  enum MAPISTATUS       retval;
  bool                  ret;
  mapi_object_t         obj_folder;
  mapi_object_t         obj_htable;
  struct SPropTagArray  *SPropTagArray;
  struct SRowSet        rowset;
  const char            *name;
  const char            *comment;
  const uint32_t        *total;
  const uint32_t        *unread;
  const uint32_t        *child;
  uint32_t              index;
  const uint64_t        *fid;
  int                   i;

  zval* folders;
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


  while (((retval = QueryRows(&obj_htable, 0x32, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND) && rowset.cRows) {
    for (index = 0; index < rowset.cRows; index++) {
      zval* current_folder;
      MAKE_STD_ZVAL(current_folder);
      array_init(current_folder);


      long n_total, n_unread;

      fid = (const uint64_t *)find_SPropValue_data(&rowset.aRow[index], PR_FID);
      name = (const char *)find_SPropValue_data(&rowset.aRow[index], PR_DISPLAY_NAME_UNICODE);
      comment = (const char *)find_SPropValue_data(&rowset.aRow[index], PR_COMMENT_UNICODE);
      if (comment == NULL) {
        comment = "";
      }
      total = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_CONTENT_COUNT);
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


      child = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_FOLDER_CHILD_COUNT);

      const char* container = get_container_class(mem_ctx, parent, *fid);

      add_assoc_string(current_folder, "name",(char*)  name, 1);
      add_assoc_long(current_folder, "fid", (long)(*fid)); // this must be cast to unsigned long in reading
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
  return lpProps[0].value.lpszA;
}

////////fetchmail

struct oclient {
	struct mapi_context	*mapi_ctx;
	struct oc_property	*props;
	const char     		*update;
	const char		*delete;
	const char		*subject;
	const char		*pr_body;
	const char		*pr_html_inline;
	char			**usernames;
	char			**mapi_to;
	char			**mapi_cc;
	char			**mapi_bcc;
	struct Binary_r		pr_html;
	struct attach		*attach;
	uint32_t		attach_num;
	const char		*store_folder;
	const char		*location;
	const char		*dtstart;
	const char		*dtend;
	int			busystatus;
	int			taskstatus;
	int			label;
	bool			private;
	int			importance;
	int			color;
	int			width;
	int			height;
	const char		*email;
	const char		*full_name;
	const char		*card_name;
	const char		*folder_name;
	const char		*folder_comment;
	const char		*freebusy;
	bool			force;
	bool			summary;
	/* PF related options */
	bool			pf;
	const char		*folder;
	/* OCPF related options */
	struct ocpf_file	*ocpf_files;
	const char		*ocpf_dump;
};


/**
 * fetch the user INBOX
 */

#define	MAX_READ_SIZE	0x1000

static bool store_attachment(mapi_object_t obj_attach, const char *filename, uint32_t size, struct oclient *oclient)
{
	TALLOC_CTX	*mem_ctx;
	bool		ret = true;
	enum MAPISTATUS	retval;
	char		*path;
	mapi_object_t	obj_stream;
	uint16_t	read_size;
	int		fd;
	DIR		*dir;
	unsigned char  	buf[MAX_READ_SIZE];

	if (!filename || !size) return false;

	mapi_object_init(&obj_stream);

	mem_ctx = talloc_named(NULL, 0, "store_attachment");

	if (!(dir = opendir(oclient->store_folder))) {
		if (mkdir(oclient->store_folder, 0700) == -1) {
			ret = false;
			goto error_close;
		}
	} else {
		closedir(dir);
	}

	path = talloc_asprintf(mem_ctx, "%s/%s", oclient->store_folder, filename);
	if ((fd = open(path, O_CREAT|O_WRONLY, S_IWUSR|S_IRUSR)) == -1) {
		ret = false;
		talloc_free(path);
		goto error_close;
	}
	talloc_free(path);

	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 0, &obj_stream);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto error;
	}

	read_size = 0;
	do {
		retval = ReadStream(&obj_stream, buf, MAX_READ_SIZE, &read_size);
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
			goto error;
		}
		write(fd, buf, read_size);
	} while (read_size);

error:
	close(fd);
error_close:
	mapi_object_release(&obj_stream);
	talloc_free(mem_ctx);
	return ret;
}


/* /\* */
/*  * Retrieve the property value for a given SRow and property tag. */
/*  * */
/*  * If the property type is a string: fetch PT_UNICODE then PT_STRING8 */
/*  * in case the desired property is not available in first choice. */
/*  * */
/*  * Fetch property normally for any others properties */
/*  *\/ */
/* _PUBLIC_ void *octool_get_propval(struct SRow *aRow, uint32_t proptag) */
/* { */
/* 	const char	*str; */

/* 	if (((proptag & 0xFFFF) == PT_STRING8) || */
/* 	    ((proptag & 0xFFFF) == PT_UNICODE)) { */
/* 		proptag = (proptag & 0xFFFF0000) | PT_UNICODE; */
/* 		str = (const char *) find_SPropValue_data(aRow, proptag); */
/* 		if (str) return (void *)str; */

/* 		proptag = (proptag & 0xFFFF0000) | PT_STRING8; */
/* 		str = (const char *) find_SPropValue_data(aRow, proptag); */
/* 		return (void *)str; */
/* 	} */

/* 	return (void *)find_SPropValue_data(aRow, proptag); */
/* } */


/* /\* */
/*  * Read a stream and store it in a DATA_BLOB */
/*  *\/ */
/* _PUBLIC_ enum MAPISTATUS octool_get_stream(TALLOC_CTX *mem_ctx, */
/* 					 mapi_object_t *obj_stream, */
/* 					 DATA_BLOB *body) */
/* { */
/* 	enum MAPISTATUS	retval; */
/* 	uint16_t	read_size; */
/* 	uint8_t		buf[0x1000]; */

/* 	body->length = 0; */
/* 	body->data = talloc_zero(mem_ctx, uint8_t); */

/* 	do { */
/* 		retval = ReadStream(obj_stream, buf, 0x1000, &read_size); */
/* 		MAPI_RETVAL_IF(retval, GetLastError(), body->data); */
/* 		if (read_size) { */
/* 			body->data = talloc_realloc(mem_ctx, body->data, uint8_t, */
/* 						    body->length + read_size); */
/* 			memcpy(&(body->data[body->length]), buf, read_size); */
/* 			body->length += read_size; */
/* 		} */
/* 	} while (read_size); */

/* 	errno = 0; */
/* 	return MAPI_E_SUCCESS; */
/* } */


/* /\* */
/*  * Fetch the body given PR_MSG_EDITOR_FORMAT property value */
/*  *\/ */
/* _PUBLIC_ enum MAPISTATUS octool_get_body(TALLOC_CTX *mem_ctx, */
/* 				       mapi_object_t *obj_message, */
/* 				       struct SRow *aRow, */
/* 				       DATA_BLOB *body) */
/* { */
/* 	enum MAPISTATUS			retval; */
/* 	const struct SBinary_short	*bin; */
/* 	mapi_object_t			obj_stream; */
/* 	char				*data; */
/* 	uint8_t				format; */

/* 	/\* Sanity checks *\/ */
/* 	MAPI_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL); */

/* 	/\* initialize body DATA_BLOB *\/ */
/* 	body->data = NULL; */
/* 	body->length = 0; */

/* 	retval = GetBestBody(obj_message, &format); */
/* 	MAPI_RETVAL_IF(retval, retval, NULL); */

/* 	switch (format) { */
/* 	case olEditorText: */
/* 		data = octool_get_propval(aRow, PR_BODY_UNICODE); */
/* 		if (data) { */
/* 			body->data = talloc_memdup(mem_ctx, data, strlen(data)); */
/* 			body->length = strlen(data); */
/* 		} else { */
/* 			mapi_object_init(&obj_stream); */
/* 			retval = OpenStream(obj_message, PR_BODY_UNICODE, 0, &obj_stream); */
/* 			MAPI_RETVAL_IF(retval, GetLastError(), NULL); */

/* 			retval = octool_get_stream(mem_ctx, &obj_stream, body); */
/* 			MAPI_RETVAL_IF(retval, GetLastError(), NULL); */

/* 			mapi_object_release(&obj_stream); */
/* 		} */
/* 		break; */
/* 	case olEditorHTML: */
/* 		bin = (const struct SBinary_short *) octool_get_propval(aRow, PR_HTML); */
/* 		if (bin) { */
/* 			body->data = talloc_memdup(mem_ctx, bin->lpb, bin->cb); */
/* 			body->length = bin->cb; */
/* 		} else { */
/* 			mapi_object_init(&obj_stream); */
/* 			retval = OpenStream(obj_message, PR_HTML, 0, &obj_stream); */
/* 			MAPI_RETVAL_IF(retval, GetLastError(), NULL); */

/* 			retval = octool_get_stream(mem_ctx, &obj_stream, body); */
/* 			MAPI_RETVAL_IF(retval, GetLastError(), NULL); */

/* 			mapi_object_release(&obj_stream); */
/* 		} */
/* 		break; */
/* 	case olEditorRTF: */
/* 		mapi_object_init(&obj_stream); */

/* 		retval = OpenStream(obj_message, PR_RTF_COMPRESSED, 0, &obj_stream); */
/* 		MAPI_RETVAL_IF(retval, GetLastError(), NULL); */

/* 		retval = WrapCompressedRTFStream(&obj_stream, body); */
/* 		MAPI_RETVAL_IF(retval, GetLastError(), NULL); */

/* 		mapi_object_release(&obj_stream); */
/* 		break; */
/* 	default: */
/* 		DEBUG(0, ("Undefined Body\n")); */
/* 		break; */
/* 	} */

/* 	return MAPI_E_SUCCESS; */
/* } */


/* /\* */
/*  * Optimized dump message routine (use GetProps rather than GetPropsAll) */
/*  *\/ */
/* _PUBLIC_ enum MAPISTATUS octool_message(TALLOC_CTX *mem_ctx, */
/* 					mapi_object_t *obj_message) */
/* { */
/* 	enum MAPISTATUS			retval; */
/* 	struct SPropTagArray		*SPropTagArray; */
/* 	struct SPropValue		*lpProps; */
/* 	struct SRow			aRow; */
/* 	uint32_t			count; */
/* 	/\* common email fields *\/ */
/* 	const char			*msgid; */
/* 	const char			*from, *to, *cc, *bcc; */
/* 	const char			*subject; */
/* 	DATA_BLOB			body; */
/* 	const uint8_t			*has_attach; */
/* 	const uint32_t			*cp; */
/* 	const char			*codepage; */

/* 	/\* Build the array of properties we want to fetch *\/ */
/* 	SPropTagArray = set_SPropTagArray(mem_ctx, 0x13, */
/* 					  PR_INTERNET_MESSAGE_ID, */
/* 					  PR_INTERNET_MESSAGE_ID_UNICODE, */
/* 					  PR_CONVERSATION_TOPIC, */
/* 					  PR_CONVERSATION_TOPIC_UNICODE, */
/* 					  PR_MSG_EDITOR_FORMAT, */
/* 					  PR_BODY_UNICODE, */
/* 					  PR_HTML, */
/* 					  PR_RTF_IN_SYNC, */
/* 					  PR_RTF_COMPRESSED, */
/* 					  PR_SENT_REPRESENTING_NAME, */
/* 					  PR_SENT_REPRESENTING_NAME_UNICODE, */
/* 					  PR_DISPLAY_TO, */
/* 					  PR_DISPLAY_TO_UNICODE, */
/* 					  PR_DISPLAY_CC, */
/* 					  PR_DISPLAY_CC_UNICODE, */
/* 					  PR_DISPLAY_BCC, */
/* 					  PR_DISPLAY_BCC_UNICODE, */
/* 					  PR_HASATTACH, */
/* 					  PR_MESSAGE_CODEPAGE); */
/* 	lpProps = NULL; */
/* 	retval = GetProps(obj_message, MAPI_UNICODE, SPropTagArray, &lpProps, &count); */
/* 	MAPIFreeBuffer(SPropTagArray); */
/* 	MAPI_RETVAL_IF(retval, retval, NULL); */

/* 	/\* Build a SRow structure *\/ */
/* 	aRow.ulAdrEntryPad = 0; */
/* 	aRow.cValues = count; */
/* 	aRow.lpProps = lpProps; */

/* 	msgid =	(const char *) octool_get_propval(&aRow, PR_INTERNET_MESSAGE_ID); */
/* 	subject = (const char *) octool_get_propval(&aRow, PR_CONVERSATION_TOPIC); */

/* 	retval = octool_get_body(mem_ctx, obj_message, &aRow, &body); */

/* 	if (retval != MAPI_E_SUCCESS) { */
/* 		printf("Invalid Message: %s\n", msgid ? msgid : ""); */
/* 		return GetLastError(); */
/* 	} */

/* 	from = (const char *) octool_get_propval(&aRow, PR_SENT_REPRESENTING_NAME); */
/* 	to = (const char *) octool_get_propval(&aRow, PR_DISPLAY_TO_UNICODE); */
/* 	cc = (const char *) octool_get_propval(&aRow, PR_DISPLAY_CC_UNICODE); */
/* 	bcc = (const char *) octool_get_propval(&aRow, PR_DISPLAY_BCC_UNICODE); */

/* 	has_attach = (const uint8_t *) octool_get_propval(&aRow, PR_HASATTACH); */
/* 	cp = (const uint32_t *) octool_get_propval(&aRow, PR_MESSAGE_CODEPAGE); */
/* 	switch (cp ? *cp : 0) { */
/* 	case CP_USASCII: */
/* 		codepage = "CP_USASCII"; */
/* 		break; */
/* 	case CP_UNICODE: */
/* 		codepage = "CP_UNICODE"; */
/* 		break; */
/* 	case CP_JAUTODETECT: */
/* 		codepage = "CP_JAUTODETECT"; */
/* 		break; */
/* 	case CP_KAUTODETECT: */
/* 		codepage = "CP_KAUTODETECT"; */
/* 		break; */
/* 	case CP_ISO2022JPESC: */
/* 		codepage = "CP_ISO2022JPESC"; */
/* 		break; */
/* 	case CP_ISO2022JPSIO: */
/* 		codepage = "CP_ISO2022JPSIO"; */
/* 		break; */
/* 	default: */
/* 		codepage = ""; */
/* 		break; */
/* 	} */

/* 	printf("+-------------------------------------+\n"); */
/* 	printf("message id: %s\n", msgid ? msgid : ""); */
/* 	printf("subject: %s\n", subject ? subject : ""); */
/* 	printf("From: %s\n", from ? from : ""); */
/* 	printf("To:  %s\n", to ? to : ""); */
/* 	printf("Cc:  %s\n", cc ? cc : ""); */
/* 	printf("Bcc: %s\n", bcc ? bcc : ""); */
/* 	if (has_attach) { */
/* 		printf("Attachment: %s\n", *has_attach ? "True" : "False"); */
/* 	} */
/* 	printf("Codepage: %s\n", codepage); */
/* 	printf("Body:\n"); */
/* 	fflush(0); */
/* 	if (body.length) { */
/* 		write(1, body.data, body.length); */
/* 		write(1, "\n", 1); */
/* 		fflush(0); */
/* 		talloc_free(body.data); */
/* 	} */
/* 	return MAPI_E_SUCCESS; */
/* } */


static const char *get_filename(const char *filename)
{
	const char *substr;

	if (!filename) return NULL;

	substr = rindex(filename, '/');
	if (substr) return substr + 1;

	return filename;
}




static enum MAPISTATUS openchangeclient_fetchmail(mapi_object_t *obj_store,
						  struct oclient *oclient)
{
	enum MAPISTATUS			retval;
	bool				status;
	TALLOC_CTX			*mem_ctx;
	mapi_object_t			obj_tis;
	mapi_object_t			obj_inbox;
	mapi_object_t			obj_message;
	mapi_object_t			obj_table;
	mapi_object_t			obj_tb_attach;
	mapi_object_t			obj_attach;
	uint64_t			id_inbox;
	struct SPropTagArray		*SPropTagArray;
	struct SRowSet			rowset;
	struct SRowSet			rowset_attach;
	uint32_t			i, j;
	uint32_t			count;
	const uint8_t			*has_attach;
	const uint32_t			*attach_num;
	const char			*attach_filename;
	const uint32_t			*attach_size;

	mem_ctx = talloc_named(NULL, 0, "openchangeclient_fetchmail");

	mapi_object_init(&obj_tis);
	mapi_object_init(&obj_inbox);
	mapi_object_init(&obj_table);

	if (oclient->pf == true) {
          php_error(E_ERROR, "Shold not be reached\n");

		/* retval = openchangeclient_getpfdir(mem_ctx, obj_store, &obj_inbox, oclient->folder); */
		/* MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx); */
	} else {
		if (oclient->folder) {
                  php_error(E_ERROR, "Shold not be reached\n");
			/* retval = GetDefaultFolder(obj_store, &id_inbox, olFolderTopInformationStore); */
			/* MAPI_RETVAL_IF(retval, retval, mem_ctx); */

			/* retval = OpenFolder(obj_store, id_inbox, &obj_tis); */
			/* MAPI_RETVAL_IF(retval, retval, mem_ctx); */

			/* retval = openchangeclient_getdir(mem_ctx, &obj_tis, &obj_inbox, oclient->folder); */
			/* MAPI_RETVAL_IF(retval, retval, mem_ctx); */
		} else {
			retval = GetReceiveFolder(obj_store, &id_inbox, NULL);
			MAPI_RETVAL_IF(retval, retval, mem_ctx);

			retval = OpenFolder(obj_store, id_inbox, &obj_inbox);
			MAPI_RETVAL_IF(retval, retval, mem_ctx);
		}
	}

        php_printf("Before GetContentsTable\n");

	retval = GetContentsTable(&obj_inbox, &obj_table, 0, &count);
        php_printf("After GetContentsTable\n");
        //	MAPI_RETVAL_IF(retval, retval, mem_ctx);
    if (retval != MAPI_E_SUCCESS) {
      php_error(E_ERROR, "Open public folder: %s", mapi_get_errstr(retval));
    }
        php_printf("Afer MAPI_RETVAL_IF\n");

	php_printf("MAILBOX (%u messages)\n", count);
	if (!count) goto end;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT_UNICODE);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	while ((retval = QueryRows(&obj_table, count, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		count -= rowset.cRows;
		for (i = 0; i < rowset.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(obj_store,
					     rowset.aRow[i].lpProps[0].value.d,
					     rowset.aRow[i].lpProps[1].value.d,
					     &obj_message, 0);
			if (GetLastError() == MAPI_E_SUCCESS) {
				if (oclient->summary) {
					mapidump_message_summary(&obj_message);
				} else {
					struct SPropValue	*lpProps;
					struct SRow		aRow;

					SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_HASATTACH);
					lpProps = NULL;
					retval = GetProps(&obj_message, 0, SPropTagArray, &lpProps, &count);
					MAPIFreeBuffer(SPropTagArray);
					if (retval != MAPI_E_SUCCESS) return retval;

					aRow.ulAdrEntryPad = 0;
					aRow.cValues = count;
					aRow.lpProps = lpProps;

					retval = octool_message(mem_ctx, &obj_message);

					has_attach = (const uint8_t *) get_SPropValue_SRow_data(&aRow, PR_HASATTACH);

					/* If we have attachments, retrieve them */
					if (has_attach && *has_attach) {
						mapi_object_init(&obj_tb_attach);
						retval = GetAttachmentTable(&obj_message, &obj_tb_attach);
						if (retval == MAPI_E_SUCCESS) {
							SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_ATTACH_NUM);
							retval = SetColumns(&obj_tb_attach, SPropTagArray);
							if (retval != MAPI_E_SUCCESS) return retval;
							MAPIFreeBuffer(SPropTagArray);

							retval = QueryRows(&obj_tb_attach, 0xa, TBL_ADVANCE, &rowset_attach);
							if (retval != MAPI_E_SUCCESS) return retval;

							for (j = 0; j < rowset_attach.cRows; j++) {
								attach_num = (const uint32_t *)find_SPropValue_data(&(rowset_attach.aRow[j]), PR_ATTACH_NUM);
								retval = OpenAttach(&obj_message, *attach_num, &obj_attach);
								if (retval == MAPI_E_SUCCESS) {
									struct SPropValue	*lpProps2;
									uint32_t		count2;

									SPropTagArray = set_SPropTagArray(mem_ctx, 0x4,
													  PR_ATTACH_FILENAME,
													  PR_ATTACH_LONG_FILENAME,
													  PR_ATTACH_SIZE,
													  PR_ATTACH_CONTENT_ID);
									lpProps2 = NULL;
									retval = GetProps(&obj_attach, MAPI_UNICODE, SPropTagArray, &lpProps2, &count2);
									MAPIFreeBuffer(SPropTagArray);
									if (retval != MAPI_E_SUCCESS) return retval;

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
									if (oclient->store_folder) {
										status = store_attachment(obj_attach, attach_filename, attach_size ? *attach_size : 0, oclient);
										if (status == false) {
											php_printf("A Problem was encountered while storing attachments on the filesystem\n");
											talloc_free(mem_ctx);
											return MAPI_E_UNABLE_TO_COMPLETE;
										}
									}
									MAPIFreeBuffer(lpProps2);
								}
							}
							errno = 0;
						}
						MAPIFreeBuffer(lpProps);
					}
				}
			}
			mapi_object_release(&obj_message);
		}
	}
 end:
	mapi_object_release(&obj_table);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_tis);

	talloc_free(mem_ctx);

	errno = 0;
	return MAPI_E_SUCCESS;
}



static void init_oclient(struct oclient *oclient)
{
	oclient->mapi_ctx = NULL;

	/* update and delete parameter */
	oclient->update = NULL;
	oclient->delete = NULL;

	/* properties list */
	oclient->props = NULL;

	/* email related parameter */
	oclient->subject = NULL;
	oclient->pr_body = NULL;
	oclient->pr_html_inline = NULL;
	oclient->attach = NULL;
	oclient->attach_num = 0;
	oclient->store_folder = NULL;

	/* appointment related parameters */
	oclient->location = NULL;
	oclient->dtstart = NULL;
	oclient->dtend = NULL;
	oclient->busystatus = -1;
	oclient->label = -1;
	oclient->private = false;
	oclient->freebusy = NULL;
	oclient->force = false;
	oclient->summary = false;

	/* contact related parameters */
	oclient->email = NULL;
	oclient->full_name = NULL;
	oclient->card_name = NULL;

	/* task related parameters */
	oclient->importance = -1;
	oclient->taskstatus = -1;

	/* note related parameters */
	oclient->color = -1;
	oclient->width = -1;
	oclient->height = -1;

	/* pf related parameters */
	oclient->pf = false;

	/* folder related parameters */
	oclient->folder = NULL;
	oclient->folder_name = NULL;
	oclient->folder_comment = NULL;

	/* ocpf related parameters */
	oclient->ocpf_files = NULL;
	oclient->ocpf_dump = NULL;
}



PHP_METHOD(MAPISession, fetchmail)
{
  zval* this_obj = getThis();
  mapi_object_t         obj_store;
  mapi_object_init(&obj_store);
  struct mapi_session* session = get_session(this_obj);
  struct mapi_profile* profile = session_obj_get_profile(this_obj);

  enum MAPISTATUS retval = OpenUserMailbox(session, (char*) profile->username, &obj_store);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("OpenUserMailbox", GetLastError());
			exit (1);
		}

  struct oclient oclient;
  init_oclient(&oclient);


  oclient.pf = false;
  oclient.folder = NULL;
  oclient.summary = false;
  oclient.store_folder = NULL;


  // open user mailbox
  init_message_store(&obj_store, session, false, (char*) profile->username);

  openchangeclient_fetchmail(&obj_store, &oclient);
  php_printf("After openchangeclient_fetchmail\n");

}

