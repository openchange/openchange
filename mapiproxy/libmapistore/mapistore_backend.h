/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2011
   Copyright (C) Brad Hards <bradh@openchange.org> 2010-2011

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
   \file mapistore_backend.h

   \brief MAPISTORE backend storage API
   
   This header contains the API that a backend storage / provider
   needs to implement.
 */

#ifndef	__MAPISTORE_BACKEND_H
#define	__MAPISTORE_BACKEND_H

#include <libmapi/libmapi.h>

/**
  \brief Backend provider interface
  
  This is the primary structure you need to create and register (using mapistore_backend_register()) to
  create a backend storage provider.

 */
struct mapistore_backend {
	const char	*name;			/**< The short name of the provider */
	const char	*description;		/**< A longer (but one line) description of the provider */
	const char	*uri_namespace;		/**< The namespace that this backend provider will use (for example "my_name://") */

	enum MAPISTORE_ERROR (*init)(void);
	enum MAPISTORE_ERROR (*create_context)(struct mapistore_backend_context *ctx, const char *, const char *, const char *, void **);
	enum MAPISTORE_ERROR (*delete_context)(void *);
	enum MAPISTORE_ERROR (*release_record)(void *, uint64_t, uint8_t);
	enum MAPISTORE_ERROR (*get_path)(void *, uint64_t, uint8_t, char **);
	/* folders semantic */
	enum MAPISTORE_ERROR (*op_mkdir)(void *, uint64_t, uint64_t, struct SRow *);
	enum MAPISTORE_ERROR (*op_rmdir)(void *, uint64_t, uint64_t);
	enum MAPISTORE_ERROR (*op_opendir)(void *, uint64_t, uint64_t);
	enum MAPISTORE_ERROR (*op_closedir)(void *);
	enum MAPISTORE_ERROR (*op_readdir_count)(void *, uint64_t, enum MAPISTORE_TABLE_TYPE, uint32_t *);
	enum MAPISTORE_ERROR (*op_get_table_property)(void *, uint64_t, enum MAPISTORE_TABLE_TYPE, uint32_t, enum MAPITAGS, void **);
	/* message semantics */
	enum MAPISTORE_ERROR (*op_openmessage)(void *, uint64_t, uint64_t, struct mapistore_message *);
	enum MAPISTORE_ERROR (*op_createmessage)(void *, uint64_t, uint64_t);
	enum MAPISTORE_ERROR (*op_savechangesmessage)(void *, uint64_t, uint8_t);
	enum MAPISTORE_ERROR (*op_submitmessage)(void *, uint64_t, uint8_t);
	enum MAPISTORE_ERROR (*op_getprops)(void *, uint64_t, uint8_t, struct SPropTagArray *, struct SRow *);
	enum MAPISTORE_ERROR (*op_get_fid_by_name)(void *, uint64_t, const char *, uint64_t *);
	enum MAPISTORE_ERROR (*op_setprops)(void *, uint64_t, uint8_t, struct SRow *);
	enum MAPISTORE_ERROR (*op_deletemessage)(void *, uint64_t mid, enum MAPISTORE_DELETION_TYPE deletion_type);
	/* mapistoredb/store semantics */
	enum MAPISTORE_ERROR (*op_db_create_uri)(TALLOC_CTX *, uint32_t, const char *, char **);
	enum MAPISTORE_ERROR (*op_db_provision_namedprops)(TALLOC_CTX *, char **, enum MAPISTORE_NAMEDPROPS_PROVISION_TYPE *);
	enum MAPISTORE_ERROR (*op_db_mkdir)(void *, enum MAPISTORE_DFLT_FOLDERS, const char *, const char *);
  
};

__BEGIN_DECLS

/*
  \brief Register a backend
 
  This function registers a backend with mapistore.

  The general approach is to create a mapistore_backend object within the 
  mapistore_init_backend() entry point, fill in the various structure elements
  and function pointers, and then call mapistore_backend_register().
 
  \code
  int mapistore_init_backend(void)
  {
    struct mapistore_backend demo_backend;
    
    demo_backend.name = "demo";
    demo_backend.description = "this is just a demostration of the mapistore backend API";
    demo_backend.uri_namespace = "demo://";
    demo.init = demo_init; // calls demo_init() on startup
    ... // more function pointers here.
    
    mapistore_backend_register(&demo_backend);
    if (ret != MAPISTORE_SUCCESS) {
        DEBUG(0, ("Failed to register the '%s' mapistore backend\n", demo_backend.name));
        return ret;
    }

    return MAPISTORE_SUCCESS;
  }
  \endcode
 */
extern enum MAPISTORE_ERROR	mapistore_backend_register(const struct mapistore_backend *);

__END_DECLS

#endif /* __MAPISTORE_BACKEND_H */
