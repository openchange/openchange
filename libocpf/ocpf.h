/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2008-2010.

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

#ifndef	__OCPF_H_
#define	__OCPF_H_

#include "libmapi/libmapi.h"
#include "libmapi/mapi_nameid.h"

#define	OCPF_SUCCESS	0x0
#define	OCPF_ERROR	0x1
#define	OCPF_E_EXIST	0x2

#define	OCPF_FLAGS_RDWR			0
#define	OCPF_FLAGS_READ			1
#define	OCPF_FLAGS_WRITE		2
#define	OCPF_FLAGS_CREATE		3

enum ocpf_recipClass {
	OCPF_MAPI_TO = 0x1,
	OCPF_MAPI_CC,
	OCPF_MAPI_BCC
};

extern struct ocpf	*ocpf;

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

#ifndef _PUBLIC_
#define _PUBLIC_
#endif

__BEGIN_DECLS

/* The following public definitions come from libocpf/ocpf_public.c */
int ocpf_init(void);
int ocpf_release(void);
int ocpf_new_context(const char *, uint32_t *, uint8_t);
int ocpf_del_context(uint32_t);
int ocpf_parse(uint32_t);
enum MAPISTATUS ocpf_get_recipients(TALLOC_CTX *, uint32_t, struct SRowSet **);
enum MAPISTATUS	ocpf_set_SPropValue(TALLOC_CTX *, uint32_t, mapi_object_t *, mapi_object_t *);
struct SPropValue *ocpf_get_SPropValue(uint32_t, uint32_t *);
enum MAPISTATUS ocpf_OpenFolder(uint32_t, mapi_object_t *, mapi_object_t *);
enum MAPISTATUS ocpf_set_Recipients(TALLOC_CTX *, uint32_t, mapi_object_t *);
enum MAPISTATUS ocpf_clear_props (uint32_t context_id);

/* The following public definitions come from libocpf/ocpf_server.c */
enum MAPISTATUS ocpf_server_set_type(uint32_t, const char *);
enum MAPISTATUS ocpf_server_set_folderID(uint32_t, mapi_id_t);
enum MAPISTATUS ocpf_server_set_SPropValue(TALLOC_CTX *, uint32_t);
enum MAPISTATUS ocpf_server_add_SPropValue(uint32_t, struct SPropValue *);
enum MAPISTATUS ocpf_server_sync(uint32_t);

/* The following public definitions come from libocpf/ocpf_dump.c */
void ocpf_dump_type(uint32_t);
void ocpf_dump_folder(uint32_t);
void ocpf_dump_recipients(uint32_t);
void ocpf_dump_oleguid(uint32_t);
void ocpf_dump_variable(uint32_t);
void ocpf_dump_property(uint32_t);
void ocpf_dump_named_property(uint32_t);
void ocpf_dump(uint32_t);

/* The following public definitions come from libocpf/ocpf_write.c */
int ocpf_write_init(uint32_t, mapi_id_t);
int ocpf_write_auto(uint32_t, mapi_object_t *, struct mapi_SPropValue_array *);
int ocpf_write_commit(uint32_t);

__END_DECLS

#endif /* ! __OCPF_H_ */
