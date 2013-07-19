#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"

zend_class_entry *mapi_class;

static zend_function_entry mapi_class_functions[] = {
     PHP_ME(MAPI, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
     PHP_ME(MAPI, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
     PHP_ME(MAPI, profiles, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(MAPI, dump_profile, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(MAPI, folders, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(MAPI, fetchmail, NULL, ZEND_ACC_PUBLIC)

    { NULL, NULL, NULL }
};

static zend_function_entry mapi_functions[] = {
    {NULL, NULL, NULL}
};

zend_module_entry mapi_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_MAPI_EXTNAME,
    mapi_functions,
    PHP_MINIT(mapi),
    PHP_MSHUTDOWN(mapi),
    NULL,
    NULL,
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_MAPI_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_MAPI
ZEND_GET_MODULE(mapi)
#endif

static HashTable *mapi_context_by_object;


PHP_MINIT_FUNCTION(mapi)
{
  // register class
  zend_class_entry ce;
  INIT_CLASS_ENTRY(ce, MAPI_CLASS_NAME, mapi_class_functions);
  mapi_class =
    zend_register_internal_class(&ce TSRMLS_CC);

  // initialize mpai contexts hash
  ALLOC_HASHTABLE(mapi_context_by_object);
  zend_hash_init(mapi_context_by_object, EXPECTED_MAPI_OBJECTS,
                 NULL,
                 mapi_context_dtor, 1);
  return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(mapi)
{
  zend_hash_destroy(mapi_context_by_object);
}

struct mapi_context* mapi_context_init(char *profdb)
{
  struct mapi_context   *mapi_ctx;
  enum MAPISTATUS        retval;
  retval = MAPIInitialize(&mapi_ctx, profdb);
  if (retval != MAPI_E_SUCCESS) {
    const char *err_str = mapi_get_errstr(retval);
    php_error(E_ERROR, "Intialize MAPI: %s", err_str);
  }

  return mapi_ctx;
}

void mapi_context_dtor(void *ptr)
{
  struct mapi_context** ctx =  ptr;
  MAPIUninitialize(*ctx);
}

PHP_METHOD(MAPI, __construct)
{
  char* profdb_path;
  int   profdb_path_len;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &profdb_path, &profdb_path_len) == FAILURE) {
    RETURN_NULL();
  }

  zval *object = getThis();
  add_property_string(object, "__profdb", profdb_path, 0);
}

PHP_METHOD(MAPI, __destruct)
{
  ulong key = (ulong) getThis();
  if (zend_hash_index_exists(mapi_context_by_object, key)) {
    zend_hash_index_del(mapi_context_by_object, key);
  }
}

struct mapi_context* get_mapi_context(zval* object)
{
  struct mapi_context** ct;
  int res;

  if (object == NULL) {
    php_error(E_ERROR, "Must be called inside of a method");
  }

  ulong key = (ulong) object; // XXX check this is true in all cases

  res= zend_hash_index_find(mapi_context_by_object, key, (void**) &ct);
  if (res == SUCCESS) {
    return *ct;
  }

  // new mapi context
  zval **profdb;
  if (zend_hash_find(Z_OBJPROP_P(object),
                     "__profdb", sizeof("__profdb"), (void**)&profdb) == FAILURE) {
    php_error(E_ERROR, "__profdb attribute not found");
  }

  struct mapi_context* new_ct  =  mapi_context_init(Z_STRVAL_P(*profdb));
  ct = &new_ct;

  res = zend_hash_index_update(mapi_context_by_object, key,
                                ct, sizeof(struct mapi_context**), NULL);
  if (res == FAILURE) {
    php_error(E_ERROR, "Adding to MAPI contexts hash");
  }

  return *ct;
}

PHP_METHOD(MAPI, profiles)
{
  struct mapi_context *mapi_ctx = get_mapi_context(getThis());

  struct SRowSet proftable;
  memset(&proftable, 0, sizeof (struct SRowSet));
  enum MAPISTATUS               retval;
  if ((retval = GetProfileTable(mapi_ctx, &proftable)) != MAPI_E_SUCCESS) {
    const char *err_str = mapi_get_errstr(retval);
    php_error(E_ERROR, "GetProfileTable: %s", err_str);
  }

  array_init(return_value);

  uint32_t count;
  for (count = 0; count != proftable.cRows; count++) {
    char* name = NULL;
    uint32_t dflt = 0;
    zval* profile;

    name = (char*) proftable.aRow[count].lpProps[0].value.lpszA;
    dflt = proftable.aRow[count].lpProps[1].value.l;

    MAKE_STD_ZVAL(profile);
    array_init(profile);
    add_assoc_string(profile, "name", name, 1);
    add_assoc_bool(profile, "default", dflt);
    add_next_index_zval(return_value, profile);
  }
}

static struct mapi_profile* get_profile(TALLOC_CTX* mem_ctx,  struct mapi_context* mapi_ctx, char* opt_profname)
{
    enum MAPISTATUS      retval;
    char*                profname;
    struct mapi_profile* profile;

    profile = talloc(mem_ctx, struct mapi_profile);

    if (!opt_profname) {
      if ((retval = GetDefaultProfile(mapi_ctx, &profname)) != MAPI_E_SUCCESS) {
        talloc_free(mem_ctx);
        const char *err_str = mapi_get_errstr(retval);
        php_error(E_ERROR, "Get default profile: %s", err_str);
      }
    } else {
      profname = talloc_strdup(mem_ctx, (const char *)opt_profname);
    }

    retval = OpenProfile(mapi_ctx, profile, profname, NULL);
    talloc_free(profname);

    if (retval && (retval != MAPI_E_INVALID_PARAMETER)) {
      talloc_free(mem_ctx);
      const char *err_str = mapi_get_errstr(retval);
      php_error(E_ERROR, "Get %s profile: %s", opt_profname, err_str);
    }

    return profile;
}

PHP_METHOD(MAPI, dump_profile)
{
    struct mapi_profile* profile;
    TALLOC_CTX*          mem_ctx;
    struct mapi_context* mapi_ctx = get_mapi_context(getThis());
    char*                exchange_version = NULL;

    char* opt_profname = NULL;
    int   opt_profname_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &opt_profname, &opt_profname_len) == FAILURE) {
        RETURN_NULL();
    }

    mapi_ctx = get_mapi_context(getThis());
    mem_ctx = talloc_named(mapi_ctx->mem_ctx, 0, "mapi_dump_profile");
    profile = get_profile(mem_ctx, mapi_ctx, opt_profname);

    switch (profile->exchange_version) {
    case 0x0:
      exchange_version = "exchange 2000";
      break;
    case 0x1:
      exchange_version = "exchange 2003/2007";
      break;
    case 0x2:
      exchange_version = "exchange 2010";
      break;
    default:
      talloc_free(mem_ctx);
      php_error(E_ERROR, "Error: unknown Exchange server: %i\n", profile->exchange_version);
    }

    array_init(return_value);
    add_assoc_string(return_value, "profile", (char *) profile->profname, 1);
    add_assoc_string(return_value, "exchange_server", exchange_version, 1);
    add_assoc_string(return_value, "encription", (profile->seal == true) ? "yes" : "no", 1);
    add_assoc_string(return_value, "username", (char*) profile->username, 1);
    add_assoc_string(return_value, "password", (char*) profile->password, 1);
    add_assoc_string(return_value, "mailbox", (char*) profile->mailbox, 1);
    add_assoc_string(return_value, "workstation", (char*) profile->workstation, 1);
    add_assoc_string(return_value, "domain", (char*) profile->domain, 1);
    add_assoc_string(return_value, "server", (char*) profile->server, 1);

    talloc_free(mem_ctx);
}

void init_message_store(mapi_object_t *store, struct mapi_session* session, bool public_folder, char* username)
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



//openchangeclient_mailbox(mem_ctx, &obj_store);
PHP_METHOD(MAPI, folders)
{
  enum MAPISTATUS retval;
  zval* thisObject;
  TALLOC_CTX *mem_ctx;

  struct mapi_session* session = NULL;
  struct mapi_context* mapi_ctx;
  static struct mapi_profile* profile;
  mapi_object_t obj_store;

  char* opt_profname = NULL;
  int   opt_profname_len;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &opt_profname, &opt_profname_len) == FAILURE) {
    RETURN_NULL();
  }

  thisObject = getThis();
  mapi_ctx = get_mapi_context(thisObject);
  mem_ctx = talloc_named(mapi_ctx->mem_ctx, 0, "mapi_folders"); // we hang mem_ctx from MAPI bz is free in object destruction
  //  TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "openchangeclient");
  profile = get_profile(mem_ctx, mapi_ctx, opt_profname);


  // login
  retval = MapiLogonEx(mapi_ctx, &session, profile->profname, profile->password);
  if (retval != MAPI_E_SUCCESS) {
    const char *err_str = mapi_get_errstr(retval);
    php_error(E_ERROR, "MapiLogonEx: %s", err_str);
  }

  // open user mailbox
  init_message_store(&obj_store, session, false, profile->username);

  mapi_id_t                     id_mailbox;
  struct SPropTagArray          *SPropTagArray;
  struct SPropValue             *lpProps;
  uint32_t                      cValues;
  const char                    *mailbox_name;

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
  add_assoc_string(return_value, "name", mailbox_name, 1);
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

      add_assoc_string(current_folder, "name", name, 1);
      add_assoc_long(current_folder, "fid", (long)(*fid)); // this must be cast to unsigned long in reading
      add_assoc_string(current_folder, "comment", comment, 1);
      add_assoc_string(current_folder, "container", container, 1);

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


static enum MAPISTATUS openchangeclient_fetchmail(
						  bool public_folder,
                                                  const char* folder);

PHP_METHOD(MAPI, fetchmail)
{


  //  openchangeclient_fetchmail(&obj_store, 0, NULL);
  //  openchangeclient_fetchmail(false, NULL);

}



/* static enum MAPISTATUS openchangeclient_fetchmail( */
/* 						  bool public_folder, */
/*                                                   const char* folder) */
/* { */
/*   mapi_object_t obj_store; */
/* 	enum MAPISTATUS			retval; */
/* 	bool				status; */
/* 	TALLOC_CTX			*mem_ctx; */
/* 	mapi_object_t			obj_tis; */
/* 	mapi_object_t			obj_inbox; */
/* 	mapi_object_t			obj_message; */
/* 	mapi_object_t			obj_table; */
/* 	mapi_object_t			obj_tb_attach; */
/* 	mapi_object_t			obj_attach; */
/* 	uint64_t			id_inbox; */
/* 	struct SPropTagArray		*SPropTagArray; */
/* 	struct SRowSet			rowset; */
/* 	struct SRowSet			rowset_attach; */
/* 	uint32_t			i, j; */
/* 	uint32_t			count; */
/* 	const uint8_t			*has_attach; */
/* 	const uint32_t			*attach_num; */
/* 	const char			*attach_filename; */
/* 	const uint32_t			*attach_size; */

/* 	mem_ctx = talloc_named(NULL, 0, "openchangeclient_fetchmail"); */

/* 	mapi_object_init(&obj_tis); */
/* 	mapi_object_init(&obj_inbox); */
/* 	mapi_object_init(&obj_table); */
/*         mapi_object_init(&obj_store); */

/* 	if (public_folder == true) { */
/* 		retval = openchangeclient_getpfdir(mem_ctx, obj_store, &obj_inbox, oclient->folder); */
/* 		MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx); */
/* 	} else { */
/* 		if (folder == NULL) { */
/* 			retval = GetDefaultFolder(obj_store, &id_inbox, olFolderTopInformationStore); */
/* 			MAPI_RETVAL_IF(retval, retval, mem_ctx); */

/* 			retval = OpenFolder(obj_store, id_inbox, &obj_tis); */
/* 			MAPI_RETVAL_IF(retval, retval, mem_ctx); */

/* 			retval = openchangeclient_getdir(mem_ctx, &obj_tis, &obj_inbox, oclient->folder); */
/* 			MAPI_RETVAL_IF(retval, retval, mem_ctx); */
/* 		} else { */
/* 			retval = GetReceiveFolder(obj_store, &id_inbox, NULL); */
/* 			MAPI_RETVAL_IF(retval, retval, mem_ctx); */

/* 			retval = OpenFolder(obj_store, id_inbox, &obj_inbox); */
/* 			MAPI_RETVAL_IF(retval, retval, mem_ctx); */
/* 		} */
/* 	} */

/* 	retval = GetContentsTable(&obj_inbox, &obj_table, 0, &count); */
/* 	MAPI_RETVAL_IF(retval, retval, mem_ctx); */

/* 	printf("MAILBOX (%u messages)\n", count); */
/* 	if (!count) goto end; */

/* 	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5, */
/* 					  PR_FID, */
/* 					  PR_MID, */
/* 					  PR_INST_ID, */
/* 					  PR_INSTANCE_NUM, */
/* 					  PR_SUBJECT_UNICODE); */
/* 	retval = SetColumns(&obj_table, SPropTagArray); */
/* 	MAPIFreeBuffer(SPropTagArray); */
/* 	MAPI_RETVAL_IF(retval, retval, mem_ctx); */

/* 	while ((retval = QueryRows(&obj_table, count, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) { */
/* 		count -= rowset.cRows; */
/* 		for (i = 0; i < rowset.cRows; i++) { */
/* 			mapi_object_init(&obj_message); */
/* 			retval = OpenMessage(obj_store, */
/* 					     rowset.aRow[i].lpProps[0].value.d, */
/* 					     rowset.aRow[i].lpProps[1].value.d, */
/* 					     &obj_message, 0); */
/* 			if (GetLastError() == MAPI_E_SUCCESS) { */
/* 				if (oclient->summary) { */
/* 					mapidump_message_summary(&obj_message); */
/* 				} else { */
/* 					struct SPropValue	*lpProps; */
/* 					struct SRow		aRow; */

/* 					SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_HASATTACH); */
/* 					lpProps = NULL; */
/* 					retval = GetProps(&obj_message, 0, SPropTagArray, &lpProps, &count); */
/* 					MAPIFreeBuffer(SPropTagArray); */
/* 					if (retval != MAPI_E_SUCCESS) return retval; */

/* 					aRow.ulAdrEntryPad = 0; */
/* 					aRow.cValues = count; */
/* 					aRow.lpProps = lpProps; */

/* 					retval = octool_message(mem_ctx, &obj_message); */

/* 					has_attach = (const uint8_t *) get_SPropValue_SRow_data(&aRow, PR_HASATTACH); */

/* 					/\* If we have attachments, retrieve them *\/ */
/* 					if (has_attach && *has_attach) { */
/* 						mapi_object_init(&obj_tb_attach); */
/* 						retval = GetAttachmentTable(&obj_message, &obj_tb_attach); */
/* 						if (retval == MAPI_E_SUCCESS) { */
/* 							SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_ATTACH_NUM); */
/* 							retval = SetColumns(&obj_tb_attach, SPropTagArray); */
/* 							if (retval != MAPI_E_SUCCESS) return retval; */
/* 							MAPIFreeBuffer(SPropTagArray); */

/* 							retval = QueryRows(&obj_tb_attach, 0xa, TBL_ADVANCE, &rowset_attach); */
/* 							if (retval != MAPI_E_SUCCESS) return retval; */

/* 							for (j = 0; j < rowset_attach.cRows; j++) { */
/* 								attach_num = (const uint32_t *)find_SPropValue_data(&(rowset_attach.aRow[j]), PR_ATTACH_NUM); */
/* 								retval = OpenAttach(&obj_message, *attach_num, &obj_attach); */
/* 								if (retval == MAPI_E_SUCCESS) { */
/* 									struct SPropValue	*lpProps2; */
/* 									uint32_t		count2; */

/* 									SPropTagArray = set_SPropTagArray(mem_ctx, 0x4, */
/* 													  PR_ATTACH_FILENAME, */
/* 													  PR_ATTACH_LONG_FILENAME, */
/* 													  PR_ATTACH_SIZE, */
/* 													  PR_ATTACH_CONTENT_ID); */
/* 									lpProps2 = NULL; */
/* 									retval = GetProps(&obj_attach, MAPI_UNICODE, SPropTagArray, &lpProps2, &count2); */
/* 									MAPIFreeBuffer(SPropTagArray); */
/* 									if (retval != MAPI_E_SUCCESS) return retval; */

/* 									aRow.ulAdrEntryPad = 0; */
/* 									aRow.cValues = count2; */
/* 									aRow.lpProps = lpProps2; */

/* 									attach_filename = get_filename(octool_get_propval(&aRow, PR_ATTACH_LONG_FILENAME)); */
/* 									if (!attach_filename || (attach_filename && !strcmp(attach_filename, ""))) { */
/* 										attach_filename = get_filename(octool_get_propval(&aRow, PR_ATTACH_FILENAME)); */
/* 									} */
/* 									attach_size = (const uint32_t *) octool_get_propval(&aRow, PR_ATTACH_SIZE); */
/* 									printf("[%u] %s (%u Bytes)\n", j, attach_filename, attach_size ? *attach_size : 0); */
/* 									fflush(0); */
/* 									if (oclient->store_folder) { */
/* 										status = store_attachment(obj_attach, attach_filename, attach_size ? *attach_size : 0, oclient); */
/* 										if (status == false) { */
/* 											printf("A Problem was encountered while storing attachments on the filesystem\n"); */
/* 											talloc_free(mem_ctx); */
/* 											return MAPI_E_UNABLE_TO_COMPLETE; */
/* 										} */
/* 									} */
/* 									MAPIFreeBuffer(lpProps2); */
/* 								} */
/* 							} */
/* 							errno = 0; */
/* 						} */
/* 						MAPIFreeBuffer(lpProps); */
/* 					} */
/* 				} */
/* 			} */
/* 			mapi_object_release(&obj_message); */
/* 		} */
/* 	} */
/*  end: */
/* 	mapi_object_release(&obj_table); */
/* 	mapi_object_release(&obj_inbox); */
/* 	mapi_object_release(&obj_tis); */

/* 	talloc_free(mem_ctx); */

/* 	errno = 0; */
/* 	return MAPI_E_SUCCESS; */
/* } */
