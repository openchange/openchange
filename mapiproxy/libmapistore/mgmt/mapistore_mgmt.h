/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2010

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

#ifndef	__MAPISTORE_MGMT_H
#define	__MAPISTORE_MGMT_H

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mqueue.h>
#include <stdint.h>
#include <signal.h>
#include <stdbool.h>

#include <dlinklist.h>

#include "gen_ndr/mapistore_mgmt.h"

/* forward declaration */
struct mapistore_context;

struct mapistore_mgmt_users_list {
	uint32_t	count;
	const char	**user;
};

struct mapistore_mgmt_users {
	struct mapistore_mgmt_user_cmd	*info;
	uint32_t			ref_count;
	struct mapistore_mgmt_users	*prev;
	struct mapistore_mgmt_users	*next;
};

struct mapistore_mgmt_context {
	struct mapistore_context	*mstore_ctx;
	struct mapistore_mgmt_users	*users;
	mqd_t				mq_users;
	bool				verbose;
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

/* definitions from mapistore_mgmt.c */
struct mapistore_mgmt_context *mapistore_mgmt_init(struct mapistore_context *);
int mapistore_mgmt_release(struct mapistore_mgmt_context *);
int mapistore_mgmt_registered_backend(struct mapistore_mgmt_context *, const char *);
struct mapistore_mgmt_users_list *mapistore_mgmt_registered_users(struct mapistore_mgmt_context *, const char *, const char *);
int mapistore_mgmt_backend_register_user(struct mapistore_connection_info *, const char *, const char *);
int mapistore_mgmt_backend_unregister_user(struct mapistore_connection_info *, const char *, const char *);
int mapistore_mgmt_set_verbosity(struct mapistore_mgmt_context *, bool);

__END_DECLS

#endif /* ! __MAPISTORE_MGMT_H */
