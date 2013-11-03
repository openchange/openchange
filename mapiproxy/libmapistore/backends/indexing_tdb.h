#ifndef __INDEXING_TDB_H__
#define __INDEXING_TDB_H__

#include "indexing_backend.h"
#include <tdb.h>

#define	MAPISTORE_DB_NAMED		"named_properties.ldb"
#define	MAPISTORE_DB_INDEXING		"indexing.tdb"
#define	MAPISTORE_SOFT_DELETED_TAG	"SOFT_DELETED:"


struct tdb_context_list {
	struct tdb_wrap			*index_ctx;
	char				*username;
	struct tdb_context_list	*prev;
	struct tdb_context_list	*next;
};

enum mapistore_error mapistore_indexing_tdb_init(struct mapistore_context *,
						 const char *,
						 struct indexing_context **);


#endif /* __INDEXING_TDB_H__ */
