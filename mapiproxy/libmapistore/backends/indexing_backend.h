#ifndef __INDEXING_BACKEND_H__
#define __INDEXING_BACKEND_H__

#include <talloc.h>
#include "../mapistore.h"
#include "../mapistore_private.h"
#include "../mapistore_errors.h"

struct indexing_context {
	enum mapistore_error (*add_fid)(struct indexing_context *, const char *, uint64_t, const char *);
	enum mapistore_error (*del_fid)(struct indexing_context *, const char *, uint64_t, uint8_t);

	enum mapistore_error (*add_mid)(struct indexing_context *, const char *, uint64_t, const char *);
	enum mapistore_error (*del_mid)(struct indexing_context *, const char *, uint64_t, uint8_t);

	enum mapistore_error (*get_uri)(struct indexing_context *, const char *, TALLOC_CTX *, uint64_t, char **, bool *);
	enum mapistore_error (*get_fmid)(struct indexing_context *, const char *, const char *, bool, uint64_t *, bool *);

	const char *backend_type;
	void *data;
};


#endif /* __INDEXING_BACKEND_H__ */
