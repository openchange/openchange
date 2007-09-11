/*
   MAPI Backup application suite

   OpenChange Project

   Copyright (C) Julien Kerihuel 2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __OPENCHANGEBACKUP_H__
#define	__OPENCHANGEBACKUP_H__

#include <libmapi/libmapi.h>

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Samba4 includes */
#include <util.h>
#include <talloc.h>
#include <ldb_errors.h>
#include <ldb.h>

/* Data structures */

struct ocb_context {
	struct ldb_context	*ldb_ctx;	/* ldb database context */
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
uint32_t		ocb_create_record(struct ocb_context *, const char *, const char *);
uint32_t		ocb_add_attribute(struct ocb_context *, const char *,
					  struct mapi_SPropValue *);
char			*get_record_uuid(TALLOC_CTX *, const struct SBinary_short *);
char			*get_MAPI_uuid(TALLOC_CTX *, const struct SBinary_short *);
char			*get_MAPI_store_guid(TALLOC_CTX *, const struct SBinary_short *);
char			*ocb_ldb_timestring(TALLOC_CTX *, struct FILETIME *);
__END_DECLS


#define	DEFAULT_PROFDB		"%s/.openchange/profiles.ldb"
#define	DEFAULT_OCBCONF		"%s/.openchange/openchangebackup.conf"
#define	DEFAULT_OCBDB		"%s/.openchange/openchangebackup_%s.ldb"

#endif /* __OPENCHANGEBACKUP_H__ */
