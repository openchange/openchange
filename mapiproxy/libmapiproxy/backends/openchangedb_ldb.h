#ifndef __OPENCHANGEDB_LDB_H__
#define __OPENCHANGEDB_LDB_H__

#include "openchangedb_backends.h"

enum MAPISTATUS openchangedb_ldb_initialize(TALLOC_CTX *, struct loadparm_context *lp_ctx, struct openchangedb_context **);

#endif /* __OPENCHANGEDB_LDB_H__ */
