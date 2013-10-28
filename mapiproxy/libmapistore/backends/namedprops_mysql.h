#ifndef __NAMEDPROPS_MYSQL_H_
#define __NAMEDPROPS_MYSQL_H_

#include "namedprops_backend.h"

enum mapistore_error mapistore_namedprops_mysql_init(TALLOC_CTX *, const char *, struct namedprops_context **);

#endif /* __NAMEDPROPS_MYSQL_H_ */
