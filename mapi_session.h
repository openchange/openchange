#ifndef MAPI_SESSION_H
#define MAPI_SESSION_H

static zval* get_child_folders(TALLOC_CTX *mem_ctx, mapi_object_t *parent, mapi_id_t folder_id, int count);
static const char *get_container_class(TALLOC_CTX *mem_ctx, mapi_object_t *parent, mapi_id_t folder_id);
static void init_message_store(mapi_object_t *store, struct mapi_session* session, bool public_folder, char* username);

PHP_METHOD(MAPISession, __construct);
PHP_METHOD(MAPISession, __destruct);
PHP_METHOD(MAPISession, folders);
PHP_METHOD(MAPISession, fetchmail);
PHP_METHOD(MAPISession, appointments);
PHP_METHOD(MAPISession, contacts);

void MAPISessionRegisterClass(TSRMLS_D);
zval* create_session_object(struct mapi_session* session, zval* profile, TALLOC_CTX* talloc_ctx TSRMLS_DC);

struct mapi_session_object
{
  zend_object std;
  char* path;
  TALLOC_CTX* talloc_ctx;
  zval* parent;
  struct mapi_session* session;
};
typedef struct mapi_session_object mapi_session_object_t;

#endif
