#ifndef __NAMEDPROPS_BACKEND_H__
#define __NAMEDPROPS_BACKEND_H__

#include <talloc.h>
#include <stdint.h>
#include "../mapistore_errors.h"

struct MAPINAMEID;

struct namedprops_context {
	enum mapistore_error (*get_mapped_id)(struct namedprops_context *, struct MAPINAMEID, uint16_t *);
	uint16_t (*next_unused_id)(struct namedprops_context *);
	enum mapistore_error (*create_id)(struct namedprops_context *, struct MAPINAMEID, uint16_t);
	enum mapistore_error (*get_nameid)(struct namedprops_context *, uint16_t, TALLOC_CTX *mem_ctx, struct MAPINAMEID **);
	enum mapistore_error (*get_nameid_type)(struct namedprops_context *, uint16_t, uint16_t *);
	enum mapistore_error (*transaction_start)(struct namedprops_context *);
	enum mapistore_error (*transaction_commit)(struct namedprops_context *);

	const char *backend_type;
	void *data;
};


// Implemented on mapistore_namedprops
enum mapistore_error mapistore_namedprops_init(TALLOC_CTX *, const char *, struct namedprops_context **);

/**
   \details Path to the ldif file with initial named properties to populate the
   database.
 */
const char *mapistore_namedprops_get_ldif_path(void)
{
	return MAPISTORE_LDIF; // Defined on compilation time
}

#endif /* __NAMEDPROPS_BACKEND_H__ */
