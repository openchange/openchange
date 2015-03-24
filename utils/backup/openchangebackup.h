/*
   MAPI Backup application suite

   OpenChange Project

   Copyright (C) Julien Kerihuel 2007

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

#ifndef __OPENCHANGEBACKUP_H__
#define	__OPENCHANGEBACKUP_H__

#include "libmapi/libmapi.h"

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

/* Samba4 includes */
#include <talloc.h>
#include <ldb_errors.h>
#include <ldb.h>

/* Data structures */

struct ocb_context {
	struct ldb_context	*ldb_ctx;	/* ldb database context */
	struct ldb_message	*msg;		/* pointer on record msg */
};

/* Prototypes */
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
struct ocb_context	*ocb_init(TALLOC_CTX *, const char *);
uint32_t		ocb_release(struct ocb_context *);

int			ocb_record_init(struct ocb_context *, const char *, 
					const char *, const char *, struct mapi_SPropValue_array *);
uint32_t		ocb_record_commit(struct ocb_context *);
uint32_t		ocb_record_add_property(struct ocb_context *, struct mapi_SPropValue *);

char			*get_record_uuid(TALLOC_CTX *, const struct SBinary_short *);
char			*get_MAPI_uuid(TALLOC_CTX *, const struct SBinary_short *);
char			*get_MAPI_store_guid(TALLOC_CTX *, const struct SBinary_short *);
char			*ocb_ldb_timestring(TALLOC_CTX *, struct FILETIME *);
__END_DECLS

#define	OCB_RETVAL_IF_CODE(x, m, c, r)	       	\
do {						\
	if (x) {				\
		OC_DEBUG(3, "[OCB] %s\n", m);	\
		if (c) {			\
			talloc_free(c);		\
		}				\
		return r;			\
	}					\
} while (0);

#define	OCB_RETVAL_IF(x, m, c) OCB_RETVAL_IF_CODE(x, m, c, -1)


#define	DEFAULT_PROFDB		"%s/.openchange/profiles.ldb"
#define	DEFAULT_OCBCONF		"%s/.openchange/openchangebackup.conf"
#define	DEFAULT_OCBDB		"%s/.openchange/openchangebackup_%s.ldb"

/* objectClass */
#define	OCB_OBJCLASS_CONTAINER	"container"
#define	OCB_OBJCLASS_MESSAGE	"message"
#define	OCB_OBJCLASS_ATTACHMENT	"attachment"

#endif /* __OPENCHANGEBACKUP_H__ */
