/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2011

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

/**
   \file mapistore_private.h

   \brief internal / private API for mapistore.
 */

#ifndef	__MAPISTORE_PRIVATE_H__
#define	__MAPISTORE_PRIVATE_H__

#define	__STDC_FORMAT_MACROS	1
#include <inttypes.h>

#include "mapiproxy/libmapistore/indexing/gen_ndr/mapistore_indexing_db.h"

#include <talloc.h>

void mapistore_set_errno(int);

#define	MAPISTORE_RETVAL_IF(x,e,c)	\
do {					\
	if (x) {			\
		mapistore_set_errno(e);	\
		if (c) {		\
			talloc_free(c);	\
		}			\
		return (e);		\
	}				\
} while (0);

#define	MAPISTORE_SANITY_CHECKS(x,c)						\
MAPISTORE_RETVAL_IF(!x, MAPISTORE_ERR_NOT_INITIALIZED, c);			\
MAPISTORE_RETVAL_IF(!x->processing_ctx, MAPISTORE_ERR_NOT_INITIALIZED, c);	\
MAPISTORE_RETVAL_IF(!x->context_list, MAPISTORE_ERR_NOT_INITIALIZED, c);

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

struct ldb_wrap_context {
	const char		*url;
	struct tevent_context	*ev;
	unsigned int		flags;
};

struct ldb_wrap {
  struct ldb_wrap			*next;
	struct ldb_wrap			*prev;
	struct ldb_wrap_context 	context;
	struct ldb_context		*ldb;
};


/**
   mapistore database context.

   This structure stores parameters for mapistore database
   initialization and backend initialization.
 */
#define	DFLT_MDB_FIRSTORG	"First Organization"
#define	DFLT_MDB_FIRSTOU	"First Organization Unit"
#define	TMPL_MDB_SERVERDN	"CN=%s,%s"
#define	TMPL_MDB_FIRSTORGDN	"CN=%s,CN=%s,%s"

struct mapistoredb_conf {
	char		*db_path;
	char		*mstore_path;
	char		*netbiosname;
	char		*dnsdomain;
	char		*domain;
	char		*domaindn;
	char		*serverdn;
	char		*firstorg;
	char		*firstou;
	char		*firstorgdn;
};

struct mapistoredb_context {
	struct loadparm_context		*lp_ctx;
	struct mapistoredb_conf		*param;
	struct mapistore_context	*mstore_ctx;
};

#define	MDB_INIT_LDIF_TMPL		\
	"dn: @OPTIONS\n"		\
	"checkBaseOnSearch: TRUE\n\n"	\
	"dn: @INDEXLIST\n"		\
	"@IDXATTR: cn\n\n"		\
	"dn: @ATTRIBUTES@\n"		\
	"cn: CASE_INSENSITIVE\n"	\
	"dn: CASE_INSENSITIVE\n\n"

#define	MDB_ROOTDSE_LDIF_TMPL						\
	"dn: @ROOTDSE\n"						\
	"defaultNamingContext: CN=%s,%s,%s\n"				\
	"rootDomainNamingContext: %s\n"					\
	"vendorName: OpenChange Project (http://www.openchange.org)\n\n"

#define	MDB_SERVER_LDIF_TMPL	       	\
	"dn: %s\n"			\
	"objectClass: top\n"		\
	"objectClass: server\n"		\
	"cn: %s\n"			\
	"GlobalCount: 0x1\n"		\
	"ReplicaID: 0x1\n\n"		\
					\
	"dn: CN=%s,%s\n"		\
	"objectClass: top\n"		\
	"objectClass: org\n"		\
	"cn: %s\n\n"			\
					\
	"dn: CN=%s,CN=%s,%s\n"		\
	"objectClass: top\n"		\
	"objectClass: ou\n"		\
	"cn: %s\n"				
						
#define	MDB_MAILBOX_LDIF_TMPL		\
	"dn: CN=Folders,CN=%s,%s\n"    	\
	"objectClass: top\n"	       	\
	"objectClass: container\n"     	\
	"cn: Folders\n\n"	       	\
					\
	"dn: %s\n"			\
	"cn: %s\n"			\
	"objectClass: mailbox\n"	\
	"objectClass: container\n"	\
	"MailboxGUID: %s\n"		\
	"ReplicaGUID: %s\n"		\
	"ReplicaID: 1\n"		\
	"SystemIdx: %d\n"		\
	"mapistore_uri: %s\n"		\
	"distinguishedName: %s\n"
	
/**
   Identifier mapping context.

   This structure stores PR_MID and PR_FID identifiers to backend
   identifiers mapping. It points to a database containing the used
   identifiers.

   The last_id structure member references the last identifier value
   which got created. There is no identifier available with a value
   higher than last_id.
 */
struct id_mapping_context {
	/* MAPISTORE v1 */
	struct tdb_wrap		*used_ctx;
	uint64_t		last_id;
	/* !MAPISTORE_v1 */

	/* MAPISTORE v2 */
	struct tevent_context	*ev;
	struct ldb_context	*ldb_ctx;
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
   Indexing identifier list
 */
struct indexing_context_list {
	struct tdb_wrap			*index_ctx;
	char				*username;
	uint32_t			ref_count;
	struct indexing_context_list	*prev;
	struct indexing_context_list	*next;
};

/**
   MAPIStore indexing context list
 */
struct mapistore_indexing_context_list {
	struct tdb_wrap				*tdb_ctx;
	const char				*username;
	uint32_t				ref_count;
	struct mapistore_indexing_context_list	*prev;
	struct mapistore_indexing_context_list	*next;
};

#define	MAPISTORE_INDEXING_DBPATH_TMPL	"%s/mapistore_indexing_%s.tdb"
#define	MAPISTORE_INDEXING_URI		"URI/%s"
#define	MAPISTORE_INDEXING_FMID		"FMID/0x%.16"PRIx64

#define	MAPISTORE_DB_NAMED		"named_properties.ldb"
#define	MAPISTORE_DB_INDEXING		"indexing.tdb"
#define	MAPISTORE_SOFT_DELETED_TAG	"SOFT_DELETED:"

/**
   The database name where in use ID mappings are stored
 */
#define	MAPISTORE_DB_LAST_ID_KEY	"mapistore_last_id"
#define	MAPISTORE_DB_LAST_ID_VAL	0x15000

#define	MAPISTORE_DB_NAME_USED_ID	"mapistore_id_mapping_used.tdb"

/**
   Mapistore opaque context to pass down to backends

   Semantically, encapsulating the mstore_ctx within a container's
   structure help abstracting mapistore logic from backend's
   perspective and write a specific API dealing exclusively with this
   particular context.
 */
struct mapistore_backend_context {
	struct mapistore_context	*mstore_ctx;
};

__BEGIN_DECLS

/* definitions from mapistore_processing.c */
enum MAPISTORE_ERROR mapistore_init_mapping_context(struct processing_context *);
enum MAPISTORE_ERROR mapistore_get_new_fmid(struct processing_context *, uint64_t *);
enum MAPISTORE_ERROR mapistore_get_new_allocation_range(struct processing_context *, uint64_t, uint64_t *, uint64_t *);
enum MAPISTORE_ERROR mapistore_get_context_id(struct processing_context *, uint32_t *);
enum MAPISTORE_ERROR mapistore_free_context_id(struct processing_context *, uint32_t);
/* mapistore_v2 */
enum MAPISTORE_ERROR mapistore_write_ldif_string_to_store(struct processing_context *, const char *);
enum MAPISTORE_ERROR mapistore_get_next_fmid(struct processing_context *, uint64_t, uint64_t *);

/* definitions from mapistore_backend.c */
enum MAPISTORE_ERROR mapistore_backend_init(TALLOC_CTX *, const char *);
struct backend_context *mapistore_backend_create_context(TALLOC_CTX *, const char *, const char *);
enum MAPISTORE_ERROR mapistore_backend_add_ref_count(struct backend_context *);
enum MAPISTORE_ERROR mapistore_backend_delete_context(struct backend_context *);
enum MAPISTORE_ERROR mapistore_backend_create_uri(TALLOC_CTX *, enum MAPISTORE_DFLT_FOLDERS, const char *, const char *, char **);
enum MAPISTORE_ERROR mapistore_backend_release_record(struct backend_context *, uint64_t, uint8_t);
enum MAPISTORE_ERROR mapistore_get_path(struct backend_context *, uint64_t, uint8_t, char **);
enum MAPISTORE_ERROR mapistore_backend_opendir(struct backend_context *, uint64_t, uint64_t);
enum MAPISTORE_ERROR mapistore_backend_mkdir(struct backend_context *, uint64_t, uint64_t, struct SRow *);
enum MAPISTORE_ERROR mapistore_backend_readdir_count(struct backend_context *, uint64_t, enum MAPISTORE_TABLE_TYPE, uint32_t *);
enum MAPISTORE_ERROR mapistore_backend_rmdir(struct backend_context *, uint64_t, uint64_t);
enum MAPISTORE_ERROR mapistore_backend_get_table_property(struct backend_context *, uint64_t, enum MAPISTORE_TABLE_TYPE, uint32_t, 
							  enum MAPITAGS, void **);
enum MAPISTORE_ERROR mapistore_backend_openmessage(struct backend_context *, uint64_t, uint64_t, struct mapistore_message *);
enum MAPISTORE_ERROR mapistore_backend_createmessage(struct backend_context *, uint64_t, uint64_t);
enum MAPISTORE_ERROR mapistore_backend_savechangesmessage(struct backend_context *, uint64_t, uint8_t);
enum MAPISTORE_ERROR mapistore_backend_submitmessage(struct backend_context *, uint64_t, uint8_t);
enum MAPISTORE_ERROR mapistore_backend_getprops(struct backend_context *, uint64_t, uint8_t, 
						struct SPropTagArray *, struct SRow *);
enum MAPISTORE_ERROR mapistore_backend_setprops(struct backend_context *, uint64_t, uint8_t, struct SRow *);
enum MAPISTORE_ERROR mapistore_backend_get_fid_by_name(struct backend_context *, uint64_t, const char *, uint64_t *);
enum MAPISTORE_ERROR mapistore_backend_deletemessage(struct backend_context *, uint64_t, enum MAPISTORE_DELETION_TYPE);

/* definitions from mapistore_tdb_wrap.c */
struct tdb_wrap *tdb_wrap_open(TALLOC_CTX *, const char *, int, int, int, mode_t);

/* definitions from mapistore_ldb_wrap.c */
struct ldb_context *mapistore_ldb_wrap_connect(TALLOC_CTX *, struct tevent_context *, const char *, unsigned int);

/* definitions from mapistore_indexing.c */

/* MAPISTORE_v1 */
struct indexing_context_list *mapistore_indexing_search(struct mapistore_context *, const char *);
enum MAPISTORE_ERROR mapistore_indexing_search_existing_fmid(struct indexing_context_list *, uint64_t, bool *);
enum MAPISTORE_ERROR mapistore_indexing_record_add_fmid(struct mapistore_context *, uint32_t, uint64_t, uint8_t);
enum MAPISTORE_ERROR mapistore_indexing_record_del_fmid(struct mapistore_context *, uint32_t, uint64_t, enum MAPISTORE_DELETION_TYPE);
enum MAPISTORE_ERROR mapistore_indexing_add_ref_count(struct indexing_context_list *);
enum MAPISTORE_ERROR mapistore_indexing_del_ref_count(struct indexing_context_list *);
/* !MAPISTORE_v1 */

/* MAPISTORE_v2 */
enum MAPISTORE_ERROR mapistore_indexing_context_add_ref(struct mapistore_context *, const char *);
/* !MAPISTORE_v2 */

/* definitions from mapistore_namedprops.c */
enum MAPISTORE_ERROR mapistore_namedprops_init(TALLOC_CTX *, void **);

__END_DECLS

#endif	/* ! __MAPISTORE_PRIVATE_H__ */
