/* camel-openchange-transport.h: Openchange-based transport class */


#ifndef CAMEL_OPENCHANGE_TRANSPORT_H
#define CAMEL_OPENCHANGE_TRANSPORT_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-transport.h>


typedef struct {
	CamelTransport parent_object;	
} CamelOpenchangeTransport;


typedef struct {
	CamelTransportClass parent_class;
} CamelOpenchangeTransportClass;


/* Standard Camel function */
CamelType camel_openchange_transport_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_OPENCHANGE_TRANSPORT_H */
