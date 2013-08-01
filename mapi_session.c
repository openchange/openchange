#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"
#include "mapi_session.h"
#include "mapi_profile.h"
#include "utils/openchangeclient.h" // for defaultFolders
#include <inttypes.h>

extern int session_resource_id;

static zend_function_entry mapi_session_class_functions[] = {
  PHP_ME(MAPISession, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
  // PHP_ME(MAPISession, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
  PHP_ME(MAPISession, folders, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(MAPISession, fetchmail, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(MAPISession, appointments, NULL, ZEND_ACC_PUBLIC)
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

  // XX removed double pointer which was likely an error
  ZEND_FETCH_RESOURCE_NO_RETURN(session, struct mapi_session*, session_resource, -1, SESSION_RESOURCE_NAME, session_resource_id);
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
  profile = session_obj_get_profile(this_obj);
  session = get_session(this_obj);

  // open user mailbox
  init_message_store(&obj_store, session, false, (char*) profile->username);

  mapi_id_t                     id_mailbox;
  struct SPropTagArray          *SPropTagArray;
  struct SPropValue             *lpProps;
  uint32_t                      cValues;
  const char                    *mailbox_name;

  mem_ctx = talloc_named(object_talloc_ctx(this_obj), 0, "MAPISession::folders");

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

      /*** XXX tmeporal workaround for child folders problem ***/
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
      /*** ***/

      // XXX commetned by workaround of child problem
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

      const char* container = get_container_class(mem_ctx, parent, *fid);

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

/**
 * fetch the user INBOX
 */

/* #define MAX_READ_SIZE   0x1000 */
/* static bool store_attachment(mapi_object_t obj_attach, const char *filename, uint32_t size, struct oclient *oclient) */
/* { */
/*         TALLOC_CTX      *mem_ctx; */
/*         bool            ret = true; */
/*         enum MAPISTATUS retval; */
/*         char            *path; */
/*         mapi_object_t   obj_stream; */
/*         uint16_t        read_size; */
/*         int             fd; */
/*         DIR             *dir; */
/*         unsigned char   buf[MAX_READ_SIZE]; */

/*         if (!filename || !size) return false; */

/*         mapi_object_init(&obj_stream); */

/*         mem_ctx = talloc_named(NULL, 0, "store_attachment"); */

/*         if (!(dir = opendir(oclient->store_folder))) { */
/*                 if (mkdir(oclient->store_folder, 0700) == -1) { */
/*                         ret = false; */
/*                         goto error_close; */
/*                 } */
/*         } else { */
/*                 closedir(dir); */
/*         } */

/*         path = talloc_asprintf(mem_ctx, "%s/%s", oclient->store_folder, filename); */
/*         if ((fd = open(path, O_CREAT|O_WRONLY, S_IWUSR|S_IRUSR)) == -1) { */
/*                 ret = false; */
/*                 talloc_free(path); */
/*                 goto error_close; */
/*         } */
/*         talloc_free(path); */

/*         retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 0, &obj_stream); */
/*         if (retval != MAPI_E_SUCCESS) { */
/*                 ret = false; */
/*                 goto error; */
/*         } */

/*         read_size = 0; */
/*         do { */
/*                 retval = ReadStream(&obj_stream, buf, MAX_READ_SIZE, &read_size); */
/*                 if (retval != MAPI_E_SUCCESS) { */
/*                         ret = false; */
/*                         goto error; */
/*                 } */
/*                 write(fd, buf, read_size); */
/*         } while (read_size); */

/* error: */
/*         close(fd); */
/* error_close: */
/*         mapi_object_release(&obj_stream); */
/*         talloc_free(mem_ctx); */
/*         return ret; */
/* } */

/*
 * Optimized dump message routine (use GetProps rather than GetPropsAll)
 */
_PUBLIC_ zval* dump_message_to_zval(TALLOC_CTX *mem_ctx,
                                        mapi_object_t *obj_message)
{
  zval* message = NULL;

        enum MAPISTATUS                 retval;
        struct SPropTagArray            *SPropTagArray;
        struct SPropValue               *lpProps;
        struct SRow                     aRow;
        uint32_t                        count;
        /* common email fields */
        const char                      *msgid;
        const char                      *from, *to, *cc, *bcc;
        const char                      *subject;
        DATA_BLOB                       body;
        const uint8_t                   *has_attach;
        const uint32_t                  *cp;
        const char                      *codepage;

        /* Build the array of properties we want to fetch */
        SPropTagArray = set_SPropTagArray(mem_ctx, 0x13,
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
        MAPI_RETVAL_IF(retval, retval, NULL);

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
          add_assoc_string(message, "msgid", msgid ? (char*) msgid : "", 1);
          add_assoc_long(message, "Error", (long) retval);
          char* err_str=  (char*) mapi_get_errstr(retval);
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

static uint32_t open_default_folder(mapi_object_t* obj_store, mapi_object_t* obj_inbox, mapi_object_t* obj_table)
{
  enum MAPISTATUS  retval;
  uint64_t   id_inbox;
  uint32_t count;

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


static zval* do_fetchmail(TALLOC_CTX* mem_ctx, mapi_object_t *obj_store)
{
  zval* messages = NULL;

  enum MAPISTATUS                 retval;
  bool                            status;
  mapi_object_t                   obj_tis;
  mapi_object_t                   obj_inbox;
  mapi_object_t                   obj_message;
  mapi_object_t                   obj_table;
  mapi_object_t                   obj_tb_attach;
  mapi_object_t                   obj_attach;
  uint64_t                        id_inbox;
  struct SPropTagArray            *SPropTagArray;
  struct SRowSet                  rowset;
  struct SRowSet                  rowset_attach;
  uint32_t                        i, j;
  uint32_t                        count;
  const uint8_t                   *has_attach;
  const uint32_t                  *attach_num;
  const char                      *attach_filename;
  const uint32_t                  *attach_size;
  bool summary = false;// TODO: support summary
  const char* store_folder = NULL; // TODO: support store_folder




  mapi_object_init(&obj_tis);
  mapi_object_init(&obj_inbox);
  mapi_object_init(&obj_table);

  // XXX Only supported get all mails from session
  // TODO: public folder; get mails from a folder
  count = open_default_folder(obj_store, &obj_inbox, &obj_table);

  if (retval != MAPI_E_SUCCESS) {
    php_error(E_ERROR, "Get contents of folder: %s", mapi_get_errstr(retval));
  }

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

  MAKE_STD_ZVAL(messages);
  array_init(messages);

  while ((retval = QueryRows(&obj_table, count, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
    count -= rowset.cRows;
    for (i = 0; i < rowset.cRows; i++) {
      zval* message = NULL;
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

              zval *attachments;
              if (rowset_attach.cRows > 0) {
                MAKE_STD_ZVAL(attachments);
                array_init(attachments);
              }

              for (j = 0; j < rowset_attach.cRows; j++) {
                attach_num = (const uint32_t *)find_SPropValue_data(&(rowset_attach.aRow[j]), PR_ATTACH_NUM);
                retval = OpenAttach(&obj_message, *attach_num, &obj_attach);
                if (retval == MAPI_E_SUCCESS) {
                  zval *attachment;
                  MAKE_STD_ZVAL(attachment);
                  array_init(attachment);

                  struct SPropValue       *lpProps2;
                  uint32_t                count2;

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
                      php_printf("A Problem was encountered while storing attachments on the filesystem\n");
                      talloc_free(mem_ctx);
                      return MAPI_E_UNABLE_TO_COMPLETE;
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

static void open_user_mailbox(struct mapi_profile* profile, struct mapi_session* session, mapi_object_t* obj_store)
{
  enum MAPISTATUS retval = OpenUserMailbox(session, (char*) profile->username, obj_store);
  if (retval != MAPI_E_SUCCESS) {
    php_error(E_ERROR, "OpenUserMailbox %s", mapi_get_errstr(retval));
  }

  init_message_store(obj_store, session, false, (char*) profile->username);
}



PHP_METHOD(MAPISession, fetchmail)
{
  zval* this_obj = getThis();
  mapi_object_t user_mbox;
  mapi_object_init(&user_mbox);
  struct mapi_session* session = get_session(this_obj);
  struct mapi_profile* profile = session_obj_get_profile(this_obj);

  open_user_mailbox(profile, session, &user_mbox);

  TALLOC_CTX* mem_ctx = talloc_named(object_talloc_ctx(this_obj), 0, "do_fetchmail");
  zval* messages = do_fetchmail(mem_ctx, &user_mbox);
  RETURN_ZVAL(messages, 0, 0);

}


const char* mapi_date(TALLOC_CTX *parent_ctx, struct mapi_SPropValue_array *properties, uint32_t mapitag)
{
  const char* date = NULL;
  TALLOC_CTX *mem_ctx;
  NTTIME time;
  const struct FILETIME *filetime;

  filetime = (const struct FILETIME *) find_mapi_SPropValue_data(properties, mapitag);
  if (filetime) {
    const char* nt_date_str;
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
   \details This function dumps the properties relating to an appointment to standard output

   The expected way to obtain the properties array is to use OpenMessage() to obtain the
   appointment object, then to use GetPropsAll() to obtain all the properties.

   \param properties array of appointment properties
   \param id identification to display for the appointment (can be NULL)

   \sa mapidump_message, mapidump_contact, mapidump_task, mapidump_note
*/
static zval* appointment_zval (TALLOC_CTX *mem_ctx, struct mapi_SPropValue_array *properties, const char *id)
{
  zval* appointment;

  const struct mapi_SLPSTRArray *contacts = NULL;
  const char* subject = NULL;
  const char* location= NULL;
  const char* timezone = NULL;
  const uint32_t *status;
  const uint8_t  *priv = NULL;
  uint32_t        i;

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
  //  add_assoc_double(appointment, "status", *status);
  add_assoc_mapi_id_t(appointment, "status", *status);

  zval* contacts_list;
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



static zval* openchangeclient_fetchitems2(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, const char *item)
{
  zval *items;

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
	uint32_t       			i;
	char				*id;

	if (!item) return false;

	mapi_object_init(&obj_tis);
	mapi_object_init(&obj_folder);

	for (i = 0; defaultFolders[i].olFolder; i++) {
		if (!strncasecmp(defaultFolders[i].container_class, item, strlen(defaultFolders[i].container_class))) {
			olFolder = defaultFolders[i].olFolder;
		}
	}

	if (!olFolder) return false;

        // XXX open default folder for the type
			retval = GetDefaultFolder(obj_store, &fid, olFolder);
			if (retval != MAPI_E_SUCCESS) return false;

			/* We now open the folder */
			retval = OpenFolder(obj_store, fid, &obj_folder);
			if (retval != MAPI_E_SUCCESS) return false;

	/* Operations on the  folder */
	mapi_object_init(&obj_table);
	retval = GetContentsTable(&obj_folder, &obj_table, 0, &count);
	if (retval != MAPI_E_SUCCESS) return false;

	printf("MAILBOX (%u messages)\n", count);
	if (!count) {
		mapi_object_release(&obj_table);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_tis);

		return NULL;
	}

        MAKE_STD_ZVAL(items);
        array_init(items);


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
	if (retval != MAPI_E_SUCCESS) return false;

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
                          if (0) {//				if (oclient->summary) {
					mapidump_message_summary(&obj_message);
				} else {
					retval = GetPropsAll(&obj_message, MAPI_UNICODE, &properties_array);
					if (retval == MAPI_E_SUCCESS) {
                                          id = talloc_asprintf(mem_ctx, ": %" PRIx64 "/%" PRIx64,
								     SRowSet.aRow[i].lpProps[0].value.d,
								     SRowSet.aRow[i].lpProps[1].value.d);
						mapi_SPropValue_array_named(&obj_message,
									    &properties_array);
						switch (olFolder) {
						case olFolderInbox:
						  mapidump_message(&properties_array, id, NULL);
							break;
						case olFolderCalendar:
							mapidump_appointment(&properties_array, id);
                                                        zval* app = appointment_zval(mem_ctx, &properties_array, id);
                                                        add_next_index_zval(items, app);
							break;
						case olFolderContacts:
							mapidump_contact(&properties_array, id);
							break;
						case olFolderTasks:
							mapidump_task(&properties_array, id);
							break;
						case olFolderNotes:
							mapidump_note(&properties_array, id);
							break;
						}
						talloc_free(id);
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
  zval* this_obj = getThis();
  mapi_object_t user_mbox;
  mapi_object_init(&user_mbox);
  struct mapi_session* session = get_session(this_obj);
  struct mapi_profile* profile = session_obj_get_profile(this_obj);

  open_user_mailbox(profile, session, &user_mbox);

  TALLOC_CTX* mem_ctx = talloc_named(object_talloc_ctx(this_obj), 0, "appointments");

  zval* appointments = openchangeclient_fetchitems2(mem_ctx, &user_mbox,  "Appointment");
  RETURN_ZVAL(appointments, 0, 0);
}
