#ifndef __NAMEDPROPS_BACKEND_H__
#define __NAMEDPROPS_BACKEND_H__

#include <talloc.h>
#include <stdint.h>
#include "../mapistore_errors.h"

struct MAPINAMEID;

struct namedprops_backend {
	enum mapistore_error (*get_mapped_id)(struct namedprops_backend *, struct MAPINAMEID, uint16_t *);
	uint16_t (*next_unused_id)(struct namedprops_backend *);
	enum mapistore_error (*create_id)(struct namedprops_backend *, struct MAPINAMEID, uint16_t);
	enum mapistore_error (*get_nameid)(struct namedprops_backend *, uint16_t, TALLOC_CTX *mem_ctx, struct MAPINAMEID **);
	enum mapistore_error (*get_nameid_type)(struct namedprops_backend *, uint16_t, uint16_t *);

	const char *backend_type;
	void *data;
};

// Implemented on mapistore_namedprops
enum mapistore_error mapistore_namedprops_init(TALLOC_CTX *, const char *, struct namedprops_backend **);

#endif /* __NAMEDPROPS_BACKEND_H__ */
