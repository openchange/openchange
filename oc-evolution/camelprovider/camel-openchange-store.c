/* camel-openchange-store.c: class for a openchange store */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <camel/camel-i18n.h>
#include <camel/camel-session.h>

#include "camel-openchange-store.h"



static void	class_init (CamelOpenchangeStoreClass *camel_openchange_store_class)
{
	
}

static void	init (CamelOpenchangeStore *exch, CamelOpenchangeStoreClass *klass)
{

}

static void	finalize (CamelOpenchangeStore *exch)
{

}

CamelType	camel_openchange_store_get_type (void)
{
	static CamelType camel_openchange_store_type = CAMEL_INVALID_TYPE;
	
	if (!camel_openchange_store_type) {
		camel_openchange_store_type = camel_type_register (
								   camel_offline_store_get_type (),
								   "CamelOpenchangeStore",
								   sizeof (CamelOpenchangeStore),
								   sizeof (CamelOpenchangeStoreClass),
								   (CamelObjectClassInitFunc) class_init,
								   NULL,
								   (CamelObjectInitFunc) init,
								   (CamelObjectFinalizeFunc) finalize);
	}
	
	return camel_openchange_store_type;
}



