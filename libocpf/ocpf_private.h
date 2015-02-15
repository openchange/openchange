/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2009.

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

#ifndef	__OCPF_PRIVATE_H_
#define	__OCPF_PRIVATE_H_

#include "libocpf/ocpf.h"
#include "config.h"
#include <stdlib.h>
#include <libocpf/ocpf.tab.h>
#ifndef _PUBLIC_ /* For util/debug.h */
#define _PUBLIC_
#endif
#include <util/debug.h>
#include "utils/dlinklist.h"

#ifndef HAVE_COMPARISON_FN_T
#define HAVE_COMPARISON_FN_T
typedef int (*comparison_fn_t)(const void *, const void *);
#else
# ifndef comparison_fn_t
typedef __compar_fn_t comparison_fn_t;
# endif
#endif

#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2) PRINTF_ATTRIBUTE(a1, a2)

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

/* The following private definitions from from libocpf/ocpf_api.c */
void ocpf_do_debug(struct ocpf_context *, const char *, ...);
int ocpf_propvalue_var(struct ocpf_context *, const char *, uint32_t, const char *, bool, int);
int ocpf_set_propvalue(TALLOC_CTX *, struct ocpf_context *, const void **, uint16_t, uint16_t, union SPropValue_CTR, bool);
int ocpf_propvalue_free(union SPropValue_CTR, uint16_t);
int ocpf_propvalue(struct ocpf_context *, uint32_t, union SPropValue_CTR, uint16_t, bool, int);
void ocpf_propvalue_s(struct ocpf_context *, const char *, union SPropValue_CTR, uint16_t, bool, int);
int ocpf_new_recipient(struct ocpf_context *);
int ocpf_recipient_set_class(struct ocpf_context *, enum ulRecipClass);
int ocpf_nproperty_add(struct ocpf_context *, struct ocpf_nprop *, union SPropValue_CTR, const char *, uint16_t, bool);
int ocpf_type_add(struct ocpf_context *, const char *);
int ocpf_folder_add(struct ocpf_context *, const char *, uint64_t, const char *);
int ocpf_oleguid_add(struct ocpf_context *, const char *, const char *);
int ocpf_oleguid_check(struct ocpf_context *, const char *, const char **);
int ocpf_add_filetime(const char *, struct FILETIME *);
int ocpf_variable_add(struct ocpf_context *, const char *, union SPropValue_CTR, uint16_t, bool);
int ocpf_binary_add(struct ocpf_context *, const char *, struct Binary_r *);

/* The following private definitions come from libocpf/ocpf_write.c */
char *ocpf_write_unescape_string(TALLOC_CTX *, const char *);

/* The following private definitions come from libocpf/ocpf_context.c */
struct ocpf_context *ocpf_context_init(TALLOC_CTX *, const char *, uint8_t, uint32_t);
struct ocpf_context *ocpf_context_add(struct ocpf *, const char *, uint32_t *, uint8_t, bool *);
int ocpf_context_delete(struct ocpf *, struct ocpf_context *);
struct ocpf_context *ocpf_context_search_by_filename(struct ocpf_context *, const char *);
struct ocpf_context *ocpf_context_search_by_context_id(struct ocpf_context *, uint32_t);

__END_DECLS

#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2)

#endif /* ! __OCPF_PRIVATE_H_ */
