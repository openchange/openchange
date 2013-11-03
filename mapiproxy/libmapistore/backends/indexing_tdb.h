#ifndef __INDEXING_TDB_H__
#define __INDEXING_TDB_H__

#include <tdb.h>

#define	MAPISTORE_DB_INDEXING		"indexing.tdb"
#define	MAPISTORE_SOFT_DELETED_TAG	"SOFT_DELETED:"


enum mapistore_error mapistore_indexing_tdb_init(struct mapistore_context *,
						 const char *,
						 struct indexing_context **);


#endif /* __INDEXING_TDB_H__ */
