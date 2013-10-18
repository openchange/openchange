#ifndef __NAMEDPROPS_LDB_H_
#define __NAMEDPROPS_LDB_H_

#include "namedprops_backend.h"

enum mapistore_error mapistore_namedprops_ldb_init(TALLOC_CTX *, const char *, struct namedprops_context **);

#endif /* __NAMEDPROPS_LDB_H_ */
