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

#ifndef	__MAPISTORE_H
#define	__MAPISTORE_H

/**
   \file mapistore.h

   \brief MAPISTORE general API
   
   This header contains general functions, primarily for
   users of the store (rather than storage providers).
 */

#ifndef	_GNU_SOURCE
#define	_GNU_SOURCE
#endif

#ifndef	_PUBLIC_
#define	_PUBLIC_
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include <tdb.h>
#include <ldb.h>
#include <talloc.h>
#include <util/debug.h>

#include "libmapi/libmapi.h"

#include "mapistore_defs.h"

typedef	int (*init_backend_fn) (void);

#define	MAPISTORE_INIT_MODULE	"mapistore_init_backend"

/* Forward declaration */
struct mapistoredb_context;

/* MAPISTORE_v1 */
struct indexing_context_list;
/* !MAPISTORE_v1 */

/* MAPISTORE_v2 */
struct mapistore_indexing_context_list;
enum MAPISTORE_NAMEDPROPS_TYPE;
/* MAPISTORE_v2 */

struct backend_context {
	const struct mapistore_backend	*backend;
	void				*private_data;
	struct indexing_context_list	*indexing;
	char				*username;
	uint32_t			context_id;
	uint32_t			ref_count;
	char				*uri;
};

struct backend_context_list {
	struct backend_context		*ctx;
	struct backend_context_list	*prev;
	struct backend_context_list	*next;
};

struct processing_context;

struct mapistore_context {
	struct processing_context		*processing_ctx;
	struct backend_context_list		*context_list;
	/* MAPISTORE v1 */
	struct indexing_context_list		*indexing_list;
	/* !MAPISTORE_v1 */

	/* MAPISTORE_v2 */
	struct mapistore_indexing_context_list	*mapistore_indexing_list;
	struct ldb_context			*mapistore_nprops_ctx;
	struct loadparm_context			*lp_ctx;
};

struct indexing_folders_list {
	uint64_t			*folderID;
	uint32_t			count;
};

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS		extern "C" {
#define __END_DECLS		}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

__BEGIN_DECLS

/* definitions from mapistore_interface.c */
struct mapistore_context *mapistore_init(TALLOC_CTX *, const char *);
enum MAPISTORE_ERROR mapistore_release(struct mapistore_context *);
enum MAPISTORE_ERROR mapistore_set_debuglevel(struct mapistore_context *, uint32_t);
enum MAPISTORE_ERROR mapistore_add_context(struct mapistore_context *, const char *, const char *, const char *, uint32_t *);
enum MAPISTORE_ERROR mapistore_add_context_ref_count(struct mapistore_context *, uint32_t);
enum MAPISTORE_ERROR mapistore_del_context(struct mapistore_context *, uint32_t);
enum MAPISTORE_ERROR mapistore_create_uri(struct mapistore_context *, uint32_t, const char *, const char *, char **);
enum MAPISTORE_ERROR mapistore_create_context_uri(struct mapistore_context *, uint32_t, enum MAPISTORE_DFLT_FOLDERS, char **);
enum MAPISTORE_ERROR mapistore_set_mapistore_uri(struct mapistore_context *, uint32_t, enum MAPISTORE_DFLT_FOLDERS, const char *);
enum MAPISTORE_ERROR mapistore_get_folder_identifier_from_uri(struct mapistore_context *, uint32_t, const char *, uint64_t *);
enum MAPISTORE_ERROR mapistore_get_next_backend(const char **, const char **, const char **, uint32_t *);
enum MAPISTORE_ERROR mapistore_get_backend_ldif(struct mapistore_context *, const char *, char **,enum MAPISTORE_NAMEDPROPS_PROVISION_TYPE *);
enum MAPISTORE_ERROR mapistore_create_root_folder(struct mapistore_context *, uint32_t, enum MAPISTORE_DFLT_FOLDERS, 
						  enum MAPISTORE_DFLT_FOLDERS, const char *);
enum MAPISTORE_ERROR mapistore_release_record(struct mapistore_context *, uint32_t, uint64_t, uint8_t);
enum MAPISTORE_ERROR mapistore_search_context_by_uri(struct mapistore_context *, const char *, uint32_t *);
const char *mapistore_errstr(enum MAPISTORE_ERROR);
enum MAPISTORE_ERROR mapistore_add_context_indexing(struct mapistore_context *, const char *, uint32_t);
enum MAPISTORE_ERROR mapistore_opendir(struct mapistore_context *, uint32_t, uint64_t, uint64_t);
enum MAPISTORE_ERROR mapistore_closedir(struct mapistore_context *, uint32_t, uint64_t);
enum MAPISTORE_ERROR mapistore_mkdir(struct mapistore_context *, uint32_t, uint64_t, const char *, const char *, enum FOLDER_TYPE, uint64_t *);
enum MAPISTORE_ERROR mapistore_rmdir(struct mapistore_context *, uint32_t, uint64_t, uint64_t, uint8_t);
enum MAPISTORE_ERROR mapistore_get_folder_count(struct mapistore_context *, uint32_t, uint64_t, uint32_t *);
enum MAPISTORE_ERROR mapistore_get_message_count(struct mapistore_context *, uint32_t, uint64_t, uint32_t *);
enum MAPISTORE_ERROR mapistore_get_table_property(struct mapistore_context *, uint32_t, enum MAPISTORE_TABLE_TYPE, uint64_t, 
						  enum MAPITAGS, uint32_t, void **);
enum MAPISTORE_ERROR mapistore_openmessage(struct mapistore_context *, uint32_t, uint64_t, uint64_t, struct mapistore_message *);
enum MAPISTORE_ERROR mapistore_createmessage(struct mapistore_context *, uint32_t, uint64_t);
enum MAPISTORE_ERROR mapistore_savechangesmessage(struct mapistore_context *, uint32_t, uint64_t *, uint8_t);
enum MAPISTORE_ERROR mapistore_submitmessage(struct mapistore_context *, uint32_t, uint64_t *, uint8_t);
enum MAPISTORE_ERROR mapistore_getprops(struct mapistore_context *, uint32_t, uint64_t, uint8_t, struct SPropTagArray *, struct SRow *);
enum MAPISTORE_ERROR mapistore_get_fid_by_name(struct mapistore_context *, uint32_t, uint64_t, const char *, uint64_t*);
enum MAPISTORE_ERROR mapistore_setprops(struct mapistore_context *, uint32_t, uint64_t, uint8_t, struct SRow *);
enum MAPISTORE_ERROR mapistore_get_child_fids(struct mapistore_context *, uint32_t, uint64_t, uint64_t **, uint32_t *);
enum MAPISTORE_ERROR mapistore_deletemessage(struct mapistore_context *, uint32_t, uint64_t, enum MAPISTORE_DELETION_TYPE);

/* definitions from mapistore_processing.c */
enum MAPISTORE_ERROR	mapistore_set_mapping_path(const char *);
enum MAPISTORE_ERROR	mapistore_set_database_path(const char *);
enum MAPISTORE_ERROR	mapistore_set_named_properties_database_path(const char *);
enum MAPISTORE_ERROR	mapistore_set_firstorgdn(const char *, const char *, const char *);


/* definitions from mapistore_backend.c */
const char	*mapistore_backend_get_installdir(void);
init_backend_fn	*mapistore_backend_load(TALLOC_CTX *, const char *);
struct backend_context *mapistore_backend_lookup(struct backend_context_list *, uint32_t);
struct backend_context *mapistore_backend_lookup_by_uri(struct backend_context_list *, const char *);
bool		mapistore_backend_run_init(init_backend_fn *);

/* definitions from mapistoredb.c */
struct mapistoredb_context *mapistoredb_init(TALLOC_CTX *, const char *);
void mapistoredb_release(struct mapistoredb_context *);
enum MAPISTORE_ERROR mapistoredb_provision(struct mapistoredb_context *);
enum MAPISTORE_ERROR mapistoredb_get_mapistore_uri(struct mapistoredb_context *, enum MAPISTORE_DFLT_FOLDERS, const char *, const char *, char **);
enum MAPISTORE_ERROR mapistoredb_get_new_fmid(struct mapistoredb_context *, const char *, uint64_t *);
enum MAPISTORE_ERROR mapistoredb_get_new_allocation_range(struct mapistoredb_context *, const char *, uint64_t, uint64_t *, uint64_t *);
enum MAPISTORE_ERROR mapistoredb_register_new_mailbox(struct mapistoredb_context *, const char *, const char *);
enum MAPISTORE_ERROR mapistoredb_register_new_mailbox_allocation_range(struct mapistoredb_context *, const char *, uint64_t, uint64_t);

/* definitions from mapistoredb_conf.c */
void				mapistoredb_dump_conf(struct mapistoredb_context *);
enum MAPISTORE_ERROR		mapistoredb_set_netbiosname(struct mapistoredb_context *, const char *);
enum MAPISTORE_ERROR		mapistoredb_set_firstorg(struct mapistoredb_context *, const char *);
enum MAPISTORE_ERROR		mapistoredb_set_firstou(struct mapistoredb_context *, const char *);
const char*			mapistoredb_get_netbiosname(struct mapistoredb_context *);
const char*			mapistoredb_get_firstorg(struct mapistoredb_context *);
const char*			mapistoredb_get_firstou(struct mapistoredb_context *);

/* definitions from mapistoredb_namedprops.c */
enum MAPISTORE_ERROR		mapistoredb_namedprops_provision(struct mapistoredb_context *);
enum MAPISTORE_ERROR		mapistoredb_namedprops_provision_user(struct mapistoredb_context *, const char *);
enum MAPISTORE_ERROR		mapistoredb_namedprops_register_application(struct mapistoredb_context *, const char *,
									    const char *, const char *, const char *);
enum MAPISTORE_ERROR		mapistoredb_namedprops_unregister_application(struct mapistoredb_context *, const char *,
									      const char *, const char *, const char *);

/* definitions from mapistore_indexing.c */

/* MAPISTORE_v1 */
enum MAPISTORE_ERROR mapistore_indexing_add(struct mapistore_context *, const char *);
enum MAPISTORE_ERROR mapistore_indexing_del(struct mapistore_context *, const char *);
enum MAPISTORE_ERROR mapistore_indexing_get_folder_list(struct mapistore_context *, const char *, uint64_t, struct indexing_folders_list **);
enum MAPISTORE_ERROR mapistore_indexing_record_add_fid(struct mapistore_context *, uint32_t, uint64_t);
enum MAPISTORE_ERROR mapistore_indexing_record_del_fid(struct mapistore_context *, uint32_t, uint64_t, enum MAPISTORE_DELETION_TYPE);
enum MAPISTORE_ERROR mapistore_indexing_record_add_mid(struct mapistore_context *, uint32_t, uint64_t);
enum MAPISTORE_ERROR mapistore_indexing_record_del_mid(struct mapistore_context *, uint32_t, uint64_t, enum MAPISTORE_DELETION_TYPE);
/* !MAPISTORE_v1 */

/* MAPISTORE_v2 */
enum MAPISTORE_ERROR mapistore_indexing_context_add(struct mapistore_context *, const char *, struct mapistore_indexing_context_list **);
enum MAPISTORE_ERROR mapistore_indexing_context_del(struct mapistore_context *, const char *);
enum MAPISTORE_ERROR mapistore_indexing_add_fmid_record(struct mapistore_indexing_context_list *, uint64_t, const char *, uint64_t, uint8_t);
enum MAPISTORE_ERROR mapistore_indexing_add_folder_record_allocation_range(struct mapistore_indexing_context_list *, uint64_t, uint64_t, uint64_t);
enum MAPISTORE_ERROR mapistore_indexing_update_mapistore_uri(struct mapistore_indexing_context_list *, uint64_t, const char *);
/* !MAPISTORE_v2 */

/* definitions from mapistore_namedprops.c */
enum MAPISTORE_ERROR mapistore_namedprops_get_default_id(struct mapistore_context *, enum MAPISTORE_NAMEDPROPS_TYPE, uint32_t *);
enum MAPISTORE_ERROR mapistore_namedprops_check_id(struct mapistore_context *, enum MAPISTORE_NAMEDPROPS_TYPE, uint32_t);
enum MAPISTORE_ERROR mapistore_namedprops_user_exist(struct mapistore_context *, const char *);
enum MAPISTORE_ERROR mapistoredb_namedprops_provision_backends(struct mapistoredb_context *);
int mapistore_namedprops_get_mapped_id(void *ldb_ctx, struct MAPINAMEID, uint16_t *);

__END_DECLS

#endif	/* ! __MAPISTORE_H */
