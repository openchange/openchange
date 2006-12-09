/* camel-openchange-transport.c: Openchange transport class. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <camel/camel-data-wrapper.h>
#include <camel/camel-exception.h>
#include <camel/camel-mime-filter-crlf.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-session.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-stream-mem.h>


#include "camel-openchange-transport.h"



static gboolean openchange_send_to (CamelTransport *transport,
				  CamelMimeMessage *message,
				  CamelAddress *from,
				  CamelAddress *recipients,
				  CamelException *ex);


static void	camel_openchange_transport_class_init (CamelOpenchangeTransportClass *camel_openchange_transport_class)
{
 
}

static void	camel_openchange_transport_init (CamelTransport *transport)
{
}

CamelType	camel_openchange_transport_get_type (void)
{
  static CamelType camel_openchange_transport_type = CAMEL_INVALID_TYPE;
  
  if (camel_openchange_transport_type == CAMEL_INVALID_TYPE) {
    camel_openchange_transport_type =
      camel_type_register (CAMEL_TRANSPORT_TYPE,
			   "CamelOpenchangeTransport",
			   sizeof (CamelOpenchangeTransport),
			   sizeof (CamelOpenchangeTransportClass),
			   (CamelObjectClassInitFunc) camel_openchange_transport_class_init,
			   NULL,
			   (CamelObjectInitFunc) camel_openchange_transport_init,
			   NULL);
  }
  
  return camel_openchange_transport_type;
}


