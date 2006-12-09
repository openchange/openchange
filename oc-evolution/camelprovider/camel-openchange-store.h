/* camel-exchange-store.h: class for a openchange store */


#ifndef CAMEL_OPENCHANGE_STORE_H
#define CAMEL_OPENCHANGE_STORE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-store.h>
#include <camel/camel-offline-store.h>


typedef struct {
	CamelOfflineStore parent_object;	
	//CamelStub *stub;
	char *storage_path, *base_url;
	char *trash_name;
	GHashTable *folders;
	GMutex *folders_lock;
       	gboolean stub_connected;
	GMutex *connect_lock;
	
} CamelOpenchangeStore;

typedef struct {
	CamelOfflineStoreClass parent_class;
	
} CamelOpenchangeStoreClass;


/* Standard Camel function */
CamelType camel_openchange_store_get_type (void);

gboolean camel_openchange_store_connected (CamelOpenchangeStore *store, CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_OPENCHANGE_STORE_H */
