#ifndef MAPI_SESSION_H
#define MAPI_SESSION_H

static zval* get_child_folders(TALLOC_CTX *mem_ctx, mapi_object_t *parent, mapi_id_t folder_id, int count);
static const char *get_container_class(TALLOC_CTX *mem_ctx, mapi_object_t *parent, mapi_id_t folder_id);
static void init_message_store(mapi_object_t *store, struct mapi_session* session, bool public_folder, char* username);


PHP_METHOD(MAPISession, __construct);
PHP_METHOD(MAPISession, folders);
PHP_METHOD(MAPISession, fetchmail);
PHP_METHOD(MAPISession, appointments);

void MAPISessionRegisterClass();

#endif
