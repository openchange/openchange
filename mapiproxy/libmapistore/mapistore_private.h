/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef	__MAPISTORE_PRIVATE_H__
#define	__MAPISTORE_PRIVATE_H__

#ifndef	ISDOT
#define ISDOT(path) ( \
			*((const char *)(path)) == '.' && \
			*(((const char *)(path)) + 1) == '\0' \
		    )
#endif

#ifndef	ISDOTDOT
#define ISDOTDOT(path)	( \
			    *((const char *)(path)) == '.' && \
			    *(((const char *)(path)) + 1) == '.' && \
			    *(((const char *)(path)) + 2) == '\0' \
			)
#endif


struct tdb_wrap {
	struct tdb_context	*tdb;
	const char		*name;
	struct tdb_wrap		*prev;
	struct tdb_wrap		*next;
};

/**
   Identifier mapping context.

   This structure stores PR_MID and PR_FID identifiers to backend
   identifiers mapping. It points on 2 databases, one with "in use"
   identifiers and another one with a list of "free identifiers" which
   are added when an object is deleted, moved, etc.

   The last_id structure member references the last identifier value
   which got created. There is no identifier available with a value
   higher than last_id.
 */
struct id_mapping_context {
	struct tdb_wrap		*used_ctx;
	struct tdb_wrap		*free_ctx;
	uint64_t		last_id;
};


/**
   Free context identifier list

   This structure is a double chained list storing unused context
   identifiers.
 */
struct context_id_list {
	uint32_t		context_id;
	struct context_id_list	*prev;
	struct context_id_list	*next;
};


struct processing_context {
	struct id_mapping_context	*mapping_ctx;
	struct context_id_list		*free_ctx;
	uint32_t			last_context_id;
	uint64_t			dflt_start_id;
};


/**
   The database name where in use ID mappings are stored
 */
#define	MAPISTORE_DB_LAST_ID_KEY	"mapistore_last_id"
#define	MAPISTORE_DB_LAST_ID_VAL	0x15000

#define	MAPISTORE_DB_NAME_USED_ID	"mapistore_id_mapping_used.tdb"
#define	MAPISTORE_DB_NAME_FREE_ID	"mapistore_id_mapping_free.tdb"

__BEGIN_DECLS

/* definitions from mapistore_processing.c */
const char *mapistore_get_mapping_path(void);
int mapistore_init_mapping_context(struct processing_context *);
int mapistore_get_context_id(struct processing_context *, uint32_t *);
int mapistore_free_context_id(struct processing_context *, uint32_t);


/* definitions from mapistore_backend.c */
int mapistore_backend_init(TALLOC_CTX *, const char *);
struct backend_context *mapistore_backend_create_context(TALLOC_CTX *, const char *, const char *);
int mapistore_backend_delete_context(struct backend_context *);

/* definitions from mapistore_tdb_wrap.c */
struct tdb_wrap *tdb_wrap_open(TALLOC_CTX *, const char *, int, int, int, mode_t);

__END_DECLS

#endif	/* ! __MAPISTORE_PRIVATE_H__ */
