#ifndef MAPI_EXCEPTION_H
#define MAPI_EXCEPTION_H

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS		extern "C" {
#define __END_DECLS		}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

void MAPIExceptionRegisterClass(TSRMLS_D);
zend_class_entry *mapi_exception_get_default();

__BEGIN_DECLS
__END_DECLS

#endif
