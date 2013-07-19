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
  mapi_object_init(&obj_store);

  // login
  retval = MapiLogonEx(mapi_ctx, &session, profile->profname, profile->password);
  if (retval != MAPI_E_SUCCESS) {
    const char *err_str = mapi_get_errstr(retval);
    php_error(E_ERROR, "MapiLogonEx: %s", err_str);
  }

  // open user mailbox
  retval = OpenUserMailbox(session, profile->username, &obj_store);
  if (retval != MAPI_E_SUCCESS) {
    const char *err_str = mapi_get_errstr(retval);
    php_error(E_ERROR, "OpenUserMailbox: %s", err_str);
  }

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
