/*
   OpenChange Tools includes

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

#ifndef	__OPENCHANGETOOLS_H__
#define	__OPENCHANGETOOLS_H__

#include <popt.h>
#include <util/debug.h>


#define	DEFAULT_PROFDB	"%s/.openchange/profiles.ldb"
#define	DEFAULT_TDB	"%s/.openchange/index.tdb"
#define	DEFAULT_MBOX	"%s/.openchange/mbox"
#define	DEFAULT_ICAL	"%s/.openchange/ical"
#define	DEFAULT_VCF	"%s/.openchange/vcf"
#define	DEFAULT_DIR	"%s/.openchange"

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS		extern "C" {
#define __END_DECLS		}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

/* Common popt structures for tool */
extern struct poptOption popt_openchange_version[];

#define	POPT_OPENCHANGE_VERSION { NULL, 0, POPT_ARG_INCLUDE_TABLE, popt_openchange_version, 0, "Common openchange options:", NULL },

#ifndef _PUBLIC_
#define _PUBLIC_
#endif

__BEGIN_DECLS
_PUBLIC_ enum MAPISTATUS octool_message(TALLOC_CTX *, mapi_object_t *);
_PUBLIC_ void *octool_get_propval(struct SRow *, uint32_t);
_PUBLIC_ enum MAPISTATUS octool_get_body(TALLOC_CTX *, mapi_object_t *,
					 struct SRow *, DATA_BLOB *);
_PUBLIC_ enum MAPISTATUS octool_get_stream(TALLOC_CTX *mem_ctx,
					 mapi_object_t *obj_stream, 
					 DATA_BLOB *body);
_PUBLIC_ struct mapi_session *octool_init_mapi(struct mapi_context *, const char *, const char *, uint32_t);
__END_DECLS

#endif /*!__OPENCHANGETOOLS_H__ */
