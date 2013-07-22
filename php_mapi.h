#ifndef PHP_MAPI_H
#define PHP_MAPI_H 1

// from openchange
#include "utils/mapitest/mapitest.h"
#include "utils/openchange-tools.h"
#include "libmapi/mapidefs.h"
//#include "libmapi/libmapi_private.h"
//#include "libmapi/mapi_nameid.h"
#include "talloc.h"

#define PHP_MAPI_VERSION "1.0"
#define PHP_MAPI_EXTNAME "mapi"

#define PROFILE_RESOURCE_NAME "Profile"
#define SESSION_RESOURCE_NAME "Session"

PHP_MINIT_FUNCTION(mapi);
PHP_MSHUTDOWN_FUNCTION(mapi);

PHP_METHOD(MAPIProfileDB, __construct);
PHP_METHOD(MAPIProfileDB, __destruct);
PHP_METHOD(MAPIProfileDB, profiles);
PHP_METHOD(MAPIProfileDB, getProfile);
PHP_METHOD(MAPIProfileDB, folders);

PHP_METHOD(MAPIProfile, __construct);
PHP_METHOD(MAPIProfile, __destruct);
PHP_METHOD(MAPIProfile, dump);

PHP_METHOD(MAPISession, __construct);
PHP_METHOD(MAPISession, folders);



struct entry_w_mem_ctx {
  void* entry;
  TALLOC_CTX* mem_ctx;
};

struct mapi_context* mapi_context_init(char *profdb);
void mapi_context_dtor(void *mapi_ctx);
struct mapi_context* get_mapi_context(zval* object);
static struct mapi_profile* get_profile_ptr(TALLOC_CTX* mem_ctx,  struct mapi_context* mapi_ctx, char* opt_profname);
static zval* get_child_folders(TALLOC_CTX *mem_ctx, mapi_object_t *parent, mapi_id_t folder_id, int count);
static const char *get_container_class(TALLOC_CTX *mem_ctx, mapi_object_t *parent, mapi_id_t folder_id);
static void logon_with_profile(struct mapi_context* mapi_ctx, struct mapi_session** session,  struct mapi_profile* profile);
static void init_message_store(mapi_object_t *store, struct mapi_session* session, bool public_folder, char* username);

extern zend_module_entry mapi_module_entry;
#define phpext_mapi_ptr &mapi_module_entry

//#define OBJ_CTMEM_CTZ(ZVAL) get_mapi_context(ZVAL)->mem_ctx

#define EXPECTED_MAPI_OBJECTS 32

#endif
