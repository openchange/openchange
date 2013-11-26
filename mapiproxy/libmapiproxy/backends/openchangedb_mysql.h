#ifndef __OPENCHANGEDB_MYSQL_H__
#define __OPENCHANGEDB_MYSQL_H__

#include "openchangedb_backends.h"

enum MAPISTATUS openchangedb_mysql_initialize(TALLOC_CTX *, const char *, struct openchangedb_context **);

#endif /* __OPENCHANGEDB_MYSQL_H__ */
