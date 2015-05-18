/*
   OpenChange MAPI implementation.
   libmapi private header file

   Copyright (C) Julien Kerihuel 2010-2011.

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

#ifndef __LIBMAPI_PRIVATE_H__
#define __LIBMAPI_PRIVATE_H__

#include "config.h"

#include "utils/dlinklist.h"

#ifndef	__STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS	1
#endif

#include <inttypes.h>

#if defined(HAVE_PTHREADS)
#include <pthread.h>
#elif defined(HAVE_GTHREAD)
#include <gthread.h>
#endif

#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2) PRINTF_ATTRIBUTE(a1, a2)

/* This provides a "we need to fix this problem" signal
   in development builds, but not in release builds */
#if SNAPSHOT == yes
  #include <assert.h>
  #define OPENCHANGE_ASSERT() assert(0)
#else
  #define OPENCHANGE_ASSERT()
#endif

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

/* The following private definitions come from ndr_mapi.c */
void obfuscate_data(uint8_t *, uint32_t, uint8_t);
enum ndr_err_code ndr_pull_lzxpress_decompress(struct ndr_pull *, struct ndr_pull **, ssize_t);
enum ndr_err_code ndr_push_lzxpress_compress(struct ndr_push *, struct ndr_push *);
enum ndr_err_code ndr_push_ExtendedException(struct ndr_push *, int, uint16_t, const struct ExceptionInfo *, const struct ExtendedException *);
enum ndr_err_code ndr_pull_ExtendedException(struct ndr_pull *, int, uint16_t, const struct ExceptionInfo *, struct ExtendedException *);
enum ndr_err_code ndr_push_AppointmentRecurrencePattern(struct ndr_push *, int, const struct AppointmentRecurrencePattern *);
enum ndr_err_code ndr_pull_AppointmentRecurrencePattern(struct ndr_pull *, int, struct AppointmentRecurrencePattern *);
enum ndr_err_code ndr_push_PersistDataArray(struct ndr_push *ndr, int ndr_flags, const struct PersistDataArray *r);
enum ndr_err_code ndr_pull_PersistDataArray(struct ndr_pull *ndr, int ndr_flags, struct PersistDataArray *r);
enum ndr_err_code ndr_push_PersistElementArray(struct ndr_push *ndr, int ndr_flags, const struct PersistElementArray *r);
enum ndr_err_code ndr_pull_PersistElementArray(struct ndr_pull *ndr, int ndr_flags, struct PersistElementArray *r);

/* The following private definitions come from libmapi/nspi.c */
int nspi_disconnect_dtor(void *);

/* The following private definitions come from libmapi/emsmdb.c */
struct emsmdb_context	*emsmdb_connect(TALLOC_CTX *, struct mapi_session *, struct dcerpc_pipe *, struct cli_credentials *, int *);
struct emsmdb_context	*emsmdb_connect_ex(TALLOC_CTX *, struct mapi_session *, struct dcerpc_pipe *, struct cli_credentials *, int *);
int			emsmdb_disconnect_dtor(void *);
enum MAPISTATUS		emsmdb_disconnect(struct emsmdb_context *);
struct mapi_notify_ctx	*emsmdb_bind_notification(struct mapi_context *, TALLOC_CTX *);
NTSTATUS		emsmdb_register_notification(struct mapi_session *, struct NOTIFKEY *);
void			free_emsmdb_property(struct SPropValue *, void *);
const void		*pull_emsmdb_property(TALLOC_CTX *, uint32_t *, enum MAPITAGS, DATA_BLOB *);
enum MAPISTATUS		emsmdb_get_SPropValue(TALLOC_CTX *, DATA_BLOB *, struct SPropTagArray *, struct SPropValue **, uint32_t *, uint8_t);
enum MAPISTATUS		emsmdb_get_SPropValue_offset(TALLOC_CTX *, DATA_BLOB *, struct SPropTagArray *, struct SPropValue **, uint32_t *, uint32_t *);
void			emsmdb_get_SRow(TALLOC_CTX *, struct SRow *, struct SPropTagArray *, uint16_t, DATA_BLOB *, uint8_t, uint8_t);
enum MAPISTATUS		emsmdb_async_connect(struct emsmdb_context *);
bool 			server_version_at_least(struct emsmdb_context *, uint16_t, uint16_t, uint16_t, uint16_t);

/* The following private definition comes from libmapi/async_emsmdb.c */
enum MAPISTATUS emsmdb_async_waitex(struct emsmdb_context *, uint32_t, uint32_t *);

/* The following private definitions come from auto-generated libmapi/mapicode.c */
void			set_errno(enum MAPISTATUS);

/* The following private definitions come from libmapi/codepage_lcid.c */
void			mapi_get_language_list(void);

/* The following private definitions come from libmapi/mapi_object.c */
int			mapi_object_is_invalid(mapi_object_t *);
void			mapi_object_set_id(mapi_object_t *, mapi_id_t);
mapi_handle_t		mapi_object_get_handle(mapi_object_t *);
void			mapi_object_set_handle(mapi_object_t *, mapi_handle_t);
void			mapi_object_table_init(TALLOC_CTX *, mapi_object_t *);
enum MAPISTATUS		mapi_object_bookmark_find(mapi_object_t *, uint32_t,struct SBinary_short *);

/* The following private definitions come from libmapi/property.c */
enum MAPITAGS		*get_MAPITAGS_SRow(TALLOC_CTX *, struct SRow *, uint32_t *);
uint32_t		MAPITAGS_delete_entries(enum MAPITAGS *, uint32_t, uint32_t, ...);
size_t			get_utf8_utf16_conv_length(const char *);

/* The following private definitions come from libmapi/IProfAdmin.c */
enum MAPISTATUS		OpenProfileStore(TALLOC_CTX *, struct ldb_context **, const char *);

/* The following private definitions come from libmapi/IMAPISupport.c  */
enum MAPISTATUS		ProcessNotification(struct mapi_notify_ctx *, struct mapi_response *);

/* The following private definitions come from libmapi/IMAPITable.c  */
uint32_t		get_mapi_SRestriction_size(struct mapi_SRestriction *);

/* The following private definitions come from libmapi/IMSProvider.c  */
enum MAPISTATUS		Logon(struct mapi_session *, struct mapi_provider *, enum PROVIDER_ID);
enum MAPISTATUS		GetNewLogonId(struct mapi_session *, uint8_t *);

/* The following private definitions come from libmapi/IMessage.c */
uint8_t			mapi_recipients_get_org_length(struct mapi_profile *);
uint16_t		mapi_recipients_RecipientFlags(struct SRow *);

/* The following private definitions come from libmapi/socket/interface.c  */
void			openchange_load_interfaces(TALLOC_CTX *, const char **, struct interface **);
int			iface_count(struct interface *);
const char		*iface_n_ip(struct interface *, int);
const char		*iface_n_bcast(struct interface *, int);
const char		*iface_n_netmask(struct interface *, int);
const char		*iface_best_ip(struct interface *, const char *);
bool			iface_is_local(struct interface *, const char *);
bool			iface_same_net(const char *, const char *, const char *);

__END_DECLS

#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2)

#endif /* !__LIBMAPI_PRIVATE_H__ */
