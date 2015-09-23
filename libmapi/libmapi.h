/*
   OpenChange MAPI implementation.
   libmapi public header file

   Copyright (C) Julien Kerihuel 2007-2013.

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

#ifndef __LIBMAPI_H__
#define __LIBMAPI_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

/* Samba4 includes */
#include <talloc.h>
#include <dcerpc.h>
#include <param.h>

/* OpenChange includes */
#include <gen_ndr/exchange.h>
#include <gen_ndr/property.h>

#include "libmapi/version.h"
#include "libmapi/oc_log.h"
#include "libmapi/nspi.h"
#include "libmapi/emsmdb.h"
#include "libmapi/mapi_context.h"
#include "libmapi/mapi_provider.h"
#include "libmapi/mapi_object.h"
#include "libmapi/mapi_id_array.h"
#include "libmapi/mapi_notification.h"
#include "libmapi/mapi_profile.h"
#include "libmapi/mapidefs.h"
#include "libmapi/mapicode.h"
#include "libmapi/socket/netif.h"
#include "libmapi/idset.h"
#include "libmapi/property_tags.h"
#include "libmapi/property_altnames.h"
#include "libmapi/fxics.h"

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

/* The following public definitions come from libmapi/nspi.c */
struct nspi_context	*nspi_bind(TALLOC_CTX *, struct dcerpc_pipe *, struct cli_credentials *, uint32_t, uint32_t, uint32_t);
enum MAPISTATUS		nspi_unbind(struct nspi_context *);
enum MAPISTATUS		nspi_UpdateStat(struct nspi_context *, TALLOC_CTX *, int32_t *);
enum MAPISTATUS		nspi_QueryRows(struct nspi_context *, TALLOC_CTX *, struct SPropTagArray *, struct PropertyTagArray_r *MIds, uint32_t, struct PropertyRowSet_r **);
enum MAPISTATUS		nspi_SeekEntries(struct nspi_context *, TALLOC_CTX *, enum TableSortOrders, struct PropertyValue_r *, struct SPropTagArray *, struct PropertyTagArray_r *pMIds, struct PropertyRowSet_r **);
enum MAPISTATUS		nspi_GetMatches(struct nspi_context *, TALLOC_CTX *, struct SPropTagArray *, struct Restriction_r *, uint32_t ulRequested, struct PropertyRowSet_r **, struct PropertyTagArray_r **ppOutMIds);
enum MAPISTATUS		nspi_ResortRestriction(struct nspi_context *, TALLOC_CTX *, enum TableSortOrders, struct PropertyTagArray_r *pInMIds, struct PropertyTagArray_r **ppMIds);
enum MAPISTATUS		nspi_DNToMId(struct nspi_context *, TALLOC_CTX *, struct StringsArray_r *, struct PropertyTagArray_r **ppMIds);
enum MAPISTATUS		nspi_GetPropList(struct nspi_context *, TALLOC_CTX *, bool, uint32_t, struct SPropTagArray **);
enum MAPISTATUS		nspi_GetProps(struct nspi_context *, TALLOC_CTX *, struct SPropTagArray *, struct PropertyTagArray_r *MId, struct PropertyRowSet_r **);
enum MAPISTATUS		nspi_CompareMIds(struct nspi_context *, TALLOC_CTX *, uint32_t, uint32_t, uint32_t *);
enum MAPISTATUS		nspi_ModProps(struct nspi_context *, TALLOC_CTX *, uint32_t, struct SPropTagArray *, struct PropertyRow_r *);
enum MAPISTATUS		nspi_GetSpecialTable(struct nspi_context *, TALLOC_CTX *, uint32_t, struct PropertyRowSet_r **);
enum MAPISTATUS		nspi_GetTemplateInfo(struct nspi_context *, TALLOC_CTX *, uint32_t, uint32_t, char *, struct PropertyRow_r **);
enum MAPISTATUS		nspi_ModLinkAtt(struct nspi_context *, bool, uint32_t, uint32_t, struct BinaryArray_r *);
enum MAPISTATUS		nspi_QueryColumns(struct nspi_context *, TALLOC_CTX *, bool, struct SPropTagArray **);
enum MAPISTATUS		nspi_GetNamesFromIDs(struct nspi_context *, TALLOC_CTX *, struct FlatUID_r *, struct SPropTagArray *, struct SPropTagArray **, struct PropertyNameSet_r **);
enum MAPISTATUS		nspi_GetIDsFromNames(struct nspi_context *, TALLOC_CTX *, bool, uint32_t, struct PropertyName_r *, struct SPropTagArray **);
enum MAPISTATUS		nspi_ResolveNames(struct nspi_context *, TALLOC_CTX *, const char **, struct SPropTagArray *, struct PropertyRowSet_r ***, struct PropertyTagArray_r ***);
enum MAPISTATUS		nspi_ResolveNamesW(struct nspi_context *, TALLOC_CTX *, const char **, struct SPropTagArray *, struct PropertyRowSet_r ***, struct PropertyTagArray_r ***);
void			nspi_dump_STAT(const char *, struct STAT *);

/* The following public definitions come from libmapi/emsmdb.c */
NTSTATUS		emsmdb_transaction_null(struct emsmdb_context *, struct mapi_response **);
NTSTATUS		emsmdb_transaction(struct emsmdb_context *, TALLOC_CTX *, struct mapi_request *, struct mapi_response **);
NTSTATUS		emsmdb_transaction_ext2(struct emsmdb_context *, TALLOC_CTX *, struct mapi_request *, struct mapi_response **);
NTSTATUS		emsmdb_transaction_wrapper(struct mapi_session *, TALLOC_CTX *, struct mapi_request *, struct mapi_response **);
struct emsmdb_info	*emsmdb_get_info(struct mapi_session *);
void			emsmdb_get_SRowSet(TALLOC_CTX *, struct SRowSet *, struct SPropTagArray *, DATA_BLOB *);

/* The following public definitions come from libmapi/cdo_mapi.c */
enum MAPISTATUS		MapiLogonEx(struct mapi_context *, struct mapi_session **, const char *, const char *);
enum MAPISTATUS		MapiLogonProvider(struct mapi_context *, struct mapi_session **, const char *, const char *, enum PROVIDER_ID);
enum MAPISTATUS		MAPIInitialize(struct mapi_context **, const char *);
void			MAPIUninitialize(struct mapi_context *);
enum MAPISTATUS		SetMAPIDumpData(struct mapi_context *, bool);
enum MAPISTATUS		SetMAPIDebugLevel(struct mapi_context *, uint32_t);
enum MAPISTATUS		GetLoadparmContext(struct mapi_context *, struct loadparm_context **);

/* The following public definitions come from libmapi/simple_mapi.c */
enum MAPISTATUS		GetDefaultPublicFolder(mapi_object_t *, uint64_t *, const uint32_t);
enum MAPISTATUS		GetDefaultFolder(mapi_object_t *, uint64_t *, const uint32_t);
enum MAPISTATUS		GetSpecialAdditionalFolder(mapi_object_t *, uint64_t *, const uint32_t);
bool			IsMailboxFolder(mapi_object_t *, uint64_t, uint32_t *);
enum MAPISTATUS		GetFolderItemsCount(mapi_object_t *, uint32_t *, uint32_t *);
enum MAPISTATUS		AddUserPermission(mapi_object_t *, const char *, enum ACLRIGHTS);
enum MAPISTATUS		ModifyUserPermission(mapi_object_t *, const char *, enum ACLRIGHTS);
enum MAPISTATUS		RemoveUserPermission(mapi_object_t *, const char *);
enum MAPISTATUS		GetBestBody(mapi_object_t *, uint8_t *);

/* The following public definitions come from auto-generated libmapi/mapitags.c */
const char		*get_proptag_name(uint32_t);
uint32_t		get_proptag_value(const char *);
uint16_t		get_property_type(uint16_t untypedtag);

/* The following public definitions come from auto-generated libmapi/mapicode.c */
void			mapi_errstr(const char *, enum MAPISTATUS);
const char		*mapi_get_errstr(enum MAPISTATUS);

/* The following public definitions come from libmapi/codepage_lcid.c */
char			*mapi_get_system_locale(void);
bool			mapi_verify_cpid(uint32_t);
uint32_t		mapi_get_cpid_from_lcid(uint32_t);
uint32_t		mapi_get_cpid_from_locale(const char *);
uint32_t		mapi_get_cpid_from_language(const char *);
uint32_t		mapi_get_lcid_from_locale(const char *);
uint32_t		mapi_get_lcid_from_language(const char *);
const char		*mapi_get_locale_from_lcid(uint32_t);
const char		*mapi_get_locale_from_language(const char *);
const char		*mapi_get_language_from_locale(const char *);
const char		*mapi_get_language_from_lcid(uint32_t);
char			**mapi_get_language_from_group(TALLOC_CTX *, uint32_t);

/* The following public definitions come from libmapi/mapidump.c */
void			mapidump_SPropValue(struct SPropValue, const char *);
void			mapidump_SPropTagArray(struct SPropTagArray *);
void			mapidump_SRowSet(struct SRowSet *, const char *);
void			mapidump_SRow(struct SRow *, const char *);
void			mapidump_PAB_entry(struct PropertyRow_r *);
void			mapidump_Recipients(const char **, struct SRowSet *, struct PropertyTagArray_r *flaglist);
void			mapidump_date(struct mapi_SPropValue_array *, uint32_t, const char *);
void			mapidump_date_SPropValue(struct SPropValue, const char *, const char *);
void			mapidump_message_summary(mapi_object_t *);
void			mapidump_message(struct mapi_SPropValue_array *, const char *, mapi_object_t *);
void			mapidump_appointment(struct mapi_SPropValue_array *, const char *);
void			mapidump_contact(struct mapi_SPropValue_array *, const char *);
const char		*get_task_status(uint32_t);
const char		*get_importance(uint32_t);
void			mapidump_task(struct mapi_SPropValue_array *, const char *);
void			mapidump_note(struct mapi_SPropValue_array *, const char *);
void			mapidump_msgflags(uint32_t, const char *);
void			mapidump_newmail(struct NewMailNotification *, const char *);
void			mapidump_tags(enum MAPITAGS *, uint16_t, const char *);
void			mapidump_foldercreated(struct FolderCreatedNotification *, const char *);
void			mapidump_folderdeleted(struct FolderDeletedNotification *, const char *);
void			mapidump_foldermoved(struct FolderMoveCopyNotification *, const char *);
void			mapidump_foldercopied(struct FolderMoveCopyNotification *, const char *);
void			mapidump_messagedeleted(struct MessageDeletedNotification *, const char *);
void			mapidump_messagecreated(struct MessageCreatedNotification *, const char *);
void			mapidump_messagemodified(struct MessageModifiedNotification *, const char *);
void			mapidump_messagemoved(struct MessageMoveCopyNotification *, const char *);
void			mapidump_messagecopied(struct MessageMoveCopyNotification *, const char *);
const char		*mapidump_freebusy_month(uint32_t, uint32_t);
uint32_t		mapidump_freebusy_year(uint32_t, uint32_t);
void			mapidump_freebusy_date(uint32_t, const char *);
void			mapidump_freebusy_event(struct Binary_r *, uint32_t, uint32_t, const char *);
void			mapidump_languages_list(void);

/* The following public definitions come from libmapi/mapi_object.c */
enum MAPISTATUS		mapi_object_init(mapi_object_t *);
void			mapi_object_release(mapi_object_t *);
enum MAPISTATUS		mapi_object_copy(mapi_object_t *, mapi_object_t *);
struct mapi_session	*mapi_object_get_session(mapi_object_t *);
void			mapi_object_set_session(mapi_object_t *, struct mapi_session *);
mapi_id_t		mapi_object_get_id(mapi_object_t *);
void			mapi_object_set_logon_id(mapi_object_t *, uint8_t);
enum MAPISTATUS		mapi_object_get_logon_id(mapi_object_t *, uint8_t *);
void			mapi_object_set_logon_store(mapi_object_t *);
void			mapi_object_debug(mapi_object_t *);
enum MAPISTATUS		mapi_object_bookmark_get_count(mapi_object_t *, uint32_t *);
enum MAPISTATUS		mapi_object_bookmark_debug(mapi_object_t *);

/* The following public definitions come from libmapi/mapi_id_array.c */
enum MAPISTATUS		mapi_id_array_init(TALLOC_CTX *, mapi_id_array_t *);
enum MAPISTATUS		mapi_id_array_release(mapi_id_array_t *);
enum MAPISTATUS		mapi_id_array_get(TALLOC_CTX *, mapi_id_array_t *, mapi_id_t **);
enum MAPISTATUS		mapi_id_array_add_obj(mapi_id_array_t *, mapi_object_t *);
enum MAPISTATUS		mapi_id_array_add_id(mapi_id_array_t *, mapi_id_t);
enum MAPISTATUS		mapi_id_array_del_id(mapi_id_array_t *, mapi_id_t);
enum MAPISTATUS		mapi_id_array_del_obj(mapi_id_array_t *, mapi_object_t *);

/* The following public definitions come from libmapi/mapi_nameid.c */
struct mapi_nameid	*mapi_nameid_new(TALLOC_CTX *);
enum MAPISTATUS		mapi_nameid_OOM_add(struct mapi_nameid *, const char *, const char *);
enum MAPISTATUS		mapi_nameid_lid_add(struct mapi_nameid *, uint16_t, const char *);
enum MAPISTATUS		mapi_nameid_string_add(struct mapi_nameid *, const char *, const char *);
enum MAPISTATUS		mapi_nameid_custom_lid_add(struct mapi_nameid *, uint16_t, uint16_t, const char *);
enum MAPISTATUS		mapi_nameid_custom_string_add(struct mapi_nameid *, const char *, uint16_t, const char *);
enum MAPISTATUS		mapi_nameid_canonical_add(struct mapi_nameid *, uint32_t);
enum MAPISTATUS		mapi_nameid_property_lookup(uint32_t);
enum MAPISTATUS		mapi_nameid_OOM_lookup(const char *, const char *, uint16_t *);
enum MAPISTATUS		mapi_nameid_lid_lookup(uint16_t, const char *, uint16_t *);
enum MAPISTATUS		mapi_nameid_lid_lookup_canonical(uint16_t, const char *, uint32_t *);
enum MAPISTATUS		mapi_nameid_string_lookup(const char *, const char *, uint16_t *);
enum MAPISTATUS		mapi_nameid_string_lookup_canonical(const char *, const char *, uint32_t *);
enum MAPISTATUS		mapi_nameid_SPropTagArray(struct mapi_nameid *, struct SPropTagArray *);
enum MAPISTATUS		mapi_nameid_map_SPropTagArray(struct mapi_nameid *, struct SPropTagArray *, struct SPropTagArray *);
enum MAPISTATUS		mapi_nameid_unmap_SPropTagArray(struct mapi_nameid *, struct SPropTagArray *);
enum MAPISTATUS		mapi_nameid_map_SPropValue(struct mapi_nameid *, struct SPropValue *, uint32_t, struct SPropTagArray *);
enum MAPISTATUS		mapi_nameid_unmap_SPropValue(struct mapi_nameid *, struct SPropValue *, uint32_t);
enum MAPISTATUS		mapi_nameid_lookup_SPropTagArray(struct mapi_nameid *, struct SPropTagArray *);
enum MAPISTATUS		mapi_nameid_lookup_SPropValue(struct mapi_nameid *, struct SPropValue *, unsigned long);
enum MAPISTATUS		mapi_nameid_GetIDsFromNames(struct mapi_nameid *, mapi_object_t *, struct SPropTagArray *);
const char *		get_namedid_name(uint32_t proptag);
uint32_t		get_namedid_value(const char *propname);
uint16_t		get_namedid_type(uint16_t untypedtag);

/* The following public definitions come from libmapi/property.c */
struct SPropTagArray	*set_SPropTagArray(TALLOC_CTX *, uint32_t, ...);
enum MAPISTATUS		SPropTagArray_add(TALLOC_CTX *, struct SPropTagArray *, enum MAPITAGS);
enum MAPISTATUS		SPropTagArray_delete(TALLOC_CTX *, struct SPropTagArray *, uint32_t);
enum MAPISTATUS		SPropTagArray_find(struct SPropTagArray, enum MAPITAGS, uint32_t *);
int                     get_email_address_index_SPropTagArray(struct SPropTagArray*);
int                     get_display_name_index_SPropTagArray(struct SPropTagArray*);
const void		*get_SPropValue(struct SPropValue *, enum MAPITAGS);
struct SPropValue	*get_SPropValue_SRowSet(struct SRowSet *, uint32_t);
const void		*get_SPropValue_SRowSet_data(struct SRowSet *, uint32_t);
enum MAPISTATUS		set_default_error_SPropValue_SRow(struct SRow *, enum MAPITAGS, void *);
struct SPropValue	*get_SPropValue_SRow(struct SRow *, uint32_t);
const void		*get_SPropValue_SRow_data(struct SRow *, uint32_t);
const void		*find_SPropValue_data(struct SRow *, uint32_t);
const void		*find_mapi_SPropValue_data(struct mapi_SPropValue_array *, uint32_t);
const void		*get_mapi_SPropValue_data(struct mapi_SPropValue *);
const void		*get_SPropValue_data(struct SPropValue *);
bool			set_SPropValue_proptag(struct SPropValue *, enum MAPITAGS, const void *);
struct SPropValue	*add_SPropValue(TALLOC_CTX *, struct SPropValue *, uint32_t *, enum MAPITAGS, const void *);
struct mapi_SPropValue	*add_mapi_SPropValue(TALLOC_CTX *, struct mapi_SPropValue *, uint16_t *, uint32_t, const void *);
bool			set_SPropValue(struct SPropValue *, const void *);
uint32_t		get_mapi_property_size(struct mapi_SPropValue *);
void			mapi_copy_spropvalues(TALLOC_CTX *, struct SPropValue *, struct SPropValue *, uint32_t);
uint32_t		cast_mapi_SPropValue(TALLOC_CTX *, struct mapi_SPropValue *, struct SPropValue *);
uint32_t		cast_SPropValue(TALLOC_CTX *, struct mapi_SPropValue *, struct SPropValue *);
enum MAPISTATUS		SRow_addprop(struct SRow *, struct SPropValue);
uint32_t		SRowSet_propcpy(TALLOC_CTX *, struct SRowSet *, struct SPropValue);
void			mapi_SPropValue_array_named(mapi_object_t *, struct mapi_SPropValue_array *);
enum MAPISTATUS		get_mapi_SPropValue_array_date_timeval(struct timeval *, struct mapi_SPropValue_array *, uint32_t);
enum MAPISTATUS		get_mapi_SPropValue_date_timeval(struct timeval *t, struct SPropValue);
bool			set_SPropValue_proptag_date_timeval(struct SPropValue *, enum MAPITAGS, const struct timeval *);
struct RecurrencePattern *get_RecurrencePattern(TALLOC_CTX *, struct Binary_r *);
struct AppointmentRecurrencePattern *get_AppointmentRecurrencePattern(TALLOC_CTX *mem_ctx, struct Binary_r *);
struct Binary_r *set_RecurrencePattern(TALLOC_CTX *, const struct RecurrencePattern *);
struct Binary_r *set_AppointmentRecurrencePattern(TALLOC_CTX *mem_ctx, const struct AppointmentRecurrencePattern *);
struct TimeZoneStruct	*get_TimeZoneStruct(TALLOC_CTX *, struct Binary_r *);
struct Binary_r		*set_TimeZoneStruct(TALLOC_CTX *, const struct TimeZoneStruct *);
struct Binary_r		*set_TimeZoneDefinition(TALLOC_CTX *, const struct TimeZoneDefinition *);
struct PtypServerId	*get_PtypServerId(TALLOC_CTX *, struct Binary_r *);
struct GlobalObjectId	*get_GlobalObjectId(TALLOC_CTX *, struct Binary_r *);
struct MessageEntryId	*get_MessageEntryId(TALLOC_CTX *, struct Binary_r *);
struct FolderEntryId	*get_FolderEntryId(TALLOC_CTX *, struct Binary_r *);
struct AddressBookEntryId *get_AddressBookEntryId(TALLOC_CTX *, struct Binary_r *);
struct OneOffEntryId	*get_OneOffEntryId(TALLOC_CTX *, struct Binary_r *);
struct PersistData      *get_PersistData(TALLOC_CTX *, struct Binary_r *);
struct PersistDataArray *get_PersistDataArray(TALLOC_CTX *, struct Binary_r *);
struct SizedXid         *get_SizedXidArray(TALLOC_CTX *, struct Binary_r *, uint32_t *);
const char		*get_TypedString(struct TypedString *);
bool			set_mapi_SPropValue(TALLOC_CTX *, struct mapi_SPropValue *, const void *);
bool			set_mapi_SPropValue_proptag(TALLOC_CTX *, struct mapi_SPropValue *, uint32_t, const void *);

struct PropertyValue_r	*get_PropertyValue_PropertyRow(struct PropertyRow_r *, enum MAPITAGS);
struct PropertyValue_r	*get_PropertyValue_PropertyRowSet(struct PropertyRowSet_r *, enum MAPITAGS);
const void		*get_PropertyValue_PropertyRowSet_data(struct PropertyRowSet_r *, uint32_t);
bool			set_PropertyValue(struct PropertyValue_r *, const void *);
const void		*get_PropertyValue(struct PropertyValue_r *, enum MAPITAGS);
const void		*get_PropertyValue_data(struct PropertyValue_r *);
enum MAPISTATUS		PropertyRow_addprop(struct PropertyRow_r *, struct PropertyValue_r);
const void		*find_PropertyValue_data(struct PropertyRow_r *, uint32_t);
uint32_t		PropertyRowSet_propcpy(TALLOC_CTX *, struct PropertyRowSet_r *, struct PropertyValue_r);
void			cast_PropertyValue_to_SPropValue(struct PropertyValue_r *, struct SPropValue *);
void			cast_PropertyRow_to_SRow(TALLOC_CTX *, struct PropertyRow_r *, struct SRow *);
void			cast_PropertyRowSet_to_SRowSet(TALLOC_CTX *, struct PropertyRowSet_r *, struct SRowSet *);

/* The following public definitions come from libmapi/IABContainer.c */
enum MAPISTATUS		ResolveNames(struct mapi_session *, const char **, struct SPropTagArray *, struct PropertyRowSet_r **,  struct PropertyTagArray_r **, uint32_t);
enum MAPISTATUS		GetGALTable(struct mapi_session *, struct SPropTagArray *, struct PropertyRowSet_r **, uint32_t, uint8_t);
enum MAPISTATUS		GetGALTableCount(struct mapi_session *, uint32_t *);
enum MAPISTATUS		GetABRecipientInfo(struct mapi_session *, const char *, struct SPropTagArray *, struct PropertyRowSet_r **);

/* The following public definitions come from libmapi/IProfAdmin.c */
enum MAPISTATUS		mapi_profile_add_string_attr(struct mapi_context *, const char *, const char *, const char *);
enum MAPISTATUS		mapi_profile_modify_string_attr(struct mapi_context *, const char *, const char *, const char *);
enum MAPISTATUS		mapi_profile_delete_string_attr(struct mapi_context *, const char *, const char *, const char *);
const char		*mapi_profile_get_ldif_path(void);
enum MAPISTATUS		CreateProfileStore(const char *, const char *);
enum MAPISTATUS		OpenProfile(struct mapi_context *, struct mapi_profile *, const char *, const char *);
enum MAPISTATUS		LoadProfile(struct mapi_context *, struct mapi_profile *);
enum MAPISTATUS		ShutDown(struct mapi_profile *);
enum MAPISTATUS		CreateProfile(struct mapi_context *, const char *, const char *, const char *, uint32_t);
enum MAPISTATUS		DeleteProfile(struct mapi_context *, const char *);
enum MAPISTATUS		ChangeProfilePassword(struct mapi_context *, const char *, const char *, const char *);
enum MAPISTATUS		CopyProfile(struct mapi_context *, const char *, const char *);
enum MAPISTATUS		DuplicateProfile(struct mapi_context *, const char *, const char *, const char *);
enum MAPISTATUS		RenameProfile(struct mapi_context *, const char *, const char *);
enum MAPISTATUS		SetDefaultProfile(struct mapi_context *, const char *);
enum MAPISTATUS		GetDefaultProfile(struct mapi_context *, char **);
enum MAPISTATUS		GetProfileTable(struct mapi_context *, struct SRowSet *);
enum MAPISTATUS		GetProfileAttr(struct mapi_profile *, const char *, unsigned int *, char ***);
enum MAPISTATUS		FindProfileAttr(struct mapi_profile *, const char *, const char *);
enum MAPISTATUS		ProcessNetworkProfile(struct mapi_session *, const char *, mapi_profile_callback_t, const void *);

/* The following public definitions come from libmapi/IMAPIContainer.c */
enum MAPISTATUS		GetContentsTable(mapi_object_t *, mapi_object_t *, uint8_t, uint32_t *);
enum MAPISTATUS		GetHierarchyTable(mapi_object_t *, mapi_object_t *, uint8_t, uint32_t *);
enum MAPISTATUS		GetPermissionsTable(mapi_object_t *, uint8_t, mapi_object_t *);
enum MAPISTATUS		GetRulesTable(mapi_object_t *, mapi_object_t *, uint8_t);
enum MAPISTATUS		ModifyPermissions(mapi_object_t *, uint8_t, struct mapi_PermissionsData *);
enum MAPISTATUS		SetSearchCriteria(mapi_object_t *, struct mapi_SRestriction *, uint32_t, mapi_id_array_t *);
enum MAPISTATUS		GetSearchCriteria(mapi_object_t *, struct mapi_SRestriction *, uint32_t *, uint16_t *, uint64_t **);

/* The following public definitions come from libmapi/IMAPIFolder.c */
enum MAPISTATUS		CreateMessage(mapi_object_t *, mapi_object_t *);
enum MAPISTATUS		DeleteMessage(mapi_object_t *, mapi_id_t *, uint32_t);
enum MAPISTATUS		HardDeleteMessage(mapi_object_t *, mapi_id_t *, uint16_t);
enum MAPISTATUS		GetMessageStatus(mapi_object_t *, mapi_id_t, uint32_t *);
enum MAPISTATUS		SetMessageStatus(mapi_object_t *, mapi_id_t, uint32_t, uint32_t, uint32_t *);
enum MAPISTATUS		MoveCopyMessages(mapi_object_t *, mapi_object_t *, mapi_id_array_t *, bool);
enum MAPISTATUS		CreateFolder(mapi_object_t *, enum FOLDER_TYPE, const char *, const char *, uint32_t, mapi_object_t *);
enum MAPISTATUS		EmptyFolder(mapi_object_t *);
enum MAPISTATUS		DeleteFolder(mapi_object_t *, mapi_id_t, uint8_t, bool *);
enum MAPISTATUS		MoveFolder(mapi_object_t *, mapi_object_t *, mapi_object_t *, char *, bool);
enum MAPISTATUS		CopyFolder(mapi_object_t *, mapi_object_t *, mapi_object_t *, char *, bool, bool);
enum MAPISTATUS		SetReadFlags(mapi_object_t *, uint8_t, uint16_t, uint64_t *);
enum MAPISTATUS		HardDeleteMessagesAndSubfolders(mapi_object_t *);

/* The following public definitions come from libmapi/IMAPIProp.c */
enum MAPISTATUS		GetProps(mapi_object_t *, uint32_t, struct SPropTagArray *, struct SPropValue **, uint32_t *);
enum MAPISTATUS		SetProps(mapi_object_t *, uint32_t, struct SPropValue *, unsigned long);
enum MAPISTATUS		SaveChangesAttachment(mapi_object_t *, mapi_object_t *, enum SaveFlags);
enum MAPISTATUS		GetPropList(mapi_object_t *, struct SPropTagArray *);
enum MAPISTATUS		GetPropsAll(mapi_object_t *, uint32_t, struct mapi_SPropValue_array *);
enum MAPISTATUS		DeleteProps(mapi_object_t *, struct SPropTagArray *);
enum MAPISTATUS		SetPropertiesNoReplicate(mapi_object_t *, uint32_t, struct SPropValue *, unsigned long);
enum MAPISTATUS		DeletePropertiesNoReplicate(mapi_object_t *, struct SPropTagArray *);
enum MAPISTATUS		GetNamesFromIDs(mapi_object_t *, enum MAPITAGS, uint16_t *, struct MAPINAMEID **);
enum MAPISTATUS		GetIDsFromNames(mapi_object_t *, uint16_t, struct MAPINAMEID *, uint32_t, struct SPropTagArray **);
enum MAPISTATUS		QueryNamedProperties(mapi_object_t *, uint8_t, struct GUID *, uint16_t *, uint16_t **, struct MAPINAMEID **);
enum MAPISTATUS		CopyProps(mapi_object_t *, mapi_object_t *, struct SPropTagArray *, uint8_t, uint16_t *, struct PropertyProblem **);
enum MAPISTATUS		CopyTo(mapi_object_t *, mapi_object_t *, struct SPropTagArray *, uint8_t, uint16_t *, struct PropertyProblem **);

/* The following public definitions come from libmapi/IMAPISession.c */
enum MAPISTATUS		OpenPublicFolder(struct mapi_session *, mapi_object_t *);
enum MAPISTATUS		OpenMsgStore(struct mapi_session *, mapi_object_t *);
enum MAPISTATUS		OpenUserMailbox(struct mapi_session *, const char *, mapi_object_t *);

/* The following public definitions come from libmapi/IMAPISupport.c */
enum MAPISTATUS		Subscribe(mapi_object_t *, uint32_t *, uint16_t, bool, mapi_notify_callback_t, void *);
enum MAPISTATUS		Unsubscribe(struct mapi_session *, uint32_t);
enum MAPISTATUS		DispatchNotifications(struct mapi_session *);
enum MAPISTATUS		MonitorNotification(struct mapi_session *, void *, struct mapi_notify_continue_callback_data *);

/* The following public definitions come from libmapi/IMAPITable.c */
enum MAPISTATUS		SetColumns(mapi_object_t *, struct SPropTagArray *);
enum MAPISTATUS		QueryPosition(mapi_object_t *, uint32_t *, uint32_t *);
enum MAPISTATUS		QueryRows(mapi_object_t *, uint16_t, enum QueryRowsFlags, enum ForwardRead, struct SRowSet *);
enum MAPISTATUS		QueryColumns(mapi_object_t *, struct SPropTagArray *);
enum MAPISTATUS		SeekRow(mapi_object_t *, enum BOOKMARK, int32_t, uint32_t *);
enum MAPISTATUS		SeekRowBookmark(mapi_object_t *, uint32_t, uint32_t, uint32_t *);
enum MAPISTATUS		SeekRowApprox(mapi_object_t *, uint32_t, uint32_t);
enum MAPISTATUS		CreateBookmark(mapi_object_t *, uint32_t *);
enum MAPISTATUS		FreeBookmark(mapi_object_t *, uint32_t);
enum MAPISTATUS		SortTable(mapi_object_t *, struct SSortOrderSet *);
enum MAPISTATUS		Reset(mapi_object_t *);
enum MAPISTATUS		Restrict(mapi_object_t *, struct mapi_SRestriction *, uint8_t *);
enum MAPISTATUS		FindRow(mapi_object_t *, struct mapi_SRestriction *, enum BOOKMARK, enum FindRow_ulFlags, struct SRowSet *);
enum MAPISTATUS		GetStatus(mapi_object_t *, uint8_t *);
enum MAPISTATUS		Abort(mapi_object_t *, uint8_t *);
enum MAPISTATUS		ExpandRow(mapi_object_t *, uint64_t, uint16_t, struct SRowSet *, uint32_t *);
enum MAPISTATUS		CollapseRow(mapi_object_t *, uint64_t, uint32_t *);
enum MAPISTATUS		GetCollapseState(mapi_object_t *, uint64_t, uint32_t, struct SBinary_short *);
enum MAPISTATUS		SetCollapseState(mapi_object_t *, struct SBinary_short *);

/* The following public definitions come from libmapi/IMSProvider.c */
enum MAPISTATUS		RfrGetNewDSA(struct mapi_context *, struct mapi_session *, const char *, const char *, char **);
enum MAPISTATUS		RfrGetFQDNFromLegacyDN(struct mapi_context *, struct mapi_session *, const char **);
enum MAPISTATUS		Logoff(mapi_object_t *);
enum MAPISTATUS		RegisterNotification(struct mapi_session *);
enum MAPISTATUS		RegisterAsyncNotification(struct mapi_session *, uint32_t *);

/* The following public definitions come from libmapi/IMessage.c */
enum MAPISTATUS		CreateAttach(mapi_object_t *, mapi_object_t *);
enum MAPISTATUS		DeleteAttach(mapi_object_t *, uint32_t);
enum MAPISTATUS		GetAttachmentTable(mapi_object_t *, mapi_object_t *);
enum MAPISTATUS		GetValidAttach(mapi_object_t *, uint16_t *, uint32_t **);
enum MAPISTATUS		OpenAttach(mapi_object_t *, uint32_t,mapi_object_t *);
enum MAPISTATUS		SetRecipientType(struct SRow *, enum ulRecipClass);
enum MAPISTATUS		ModifyRecipients(mapi_object_t *, struct SRowSet *);
enum MAPISTATUS		ReadRecipients(mapi_object_t *, uint32_t, uint8_t *, struct ReadRecipientRow **);
enum MAPISTATUS		RemoveAllRecipients(mapi_object_t *);
enum MAPISTATUS		SubmitMessage(mapi_object_t *);
enum MAPISTATUS		AbortSubmit(mapi_object_t *, mapi_object_t *, mapi_object_t *);
enum MAPISTATUS		SaveChangesMessage(mapi_object_t *, mapi_object_t *, uint8_t);
enum MAPISTATUS		TransportSend(mapi_object_t *, struct mapi_SPropValue_array *);
enum MAPISTATUS		GetRecipientTable(mapi_object_t *, struct SRowSet *, struct SPropTagArray *);
enum MAPISTATUS		SetMessageReadFlag(mapi_object_t *, mapi_object_t *, uint8_t);
enum MAPISTATUS		OpenEmbeddedMessage(mapi_object_t *, mapi_object_t *, enum OpenEmbeddedMessage_OpenModeFlags);

/* The following public definitions come from libmapi/IMsgStore.c */
enum MAPISTATUS		OpenFolder(mapi_object_t *, mapi_id_t,mapi_object_t *);
enum MAPISTATUS		PublicFolderIsGhosted(mapi_object_t *, mapi_object_t *, bool *);
enum MAPISTATUS		OpenPublicFolderByName(mapi_object_t *, mapi_object_t *, const char *);
enum MAPISTATUS		SetReceiveFolder(mapi_object_t *, mapi_object_t *, const char *);
enum MAPISTATUS		GetReceiveFolder(mapi_object_t *, mapi_id_t *, const char *);
enum MAPISTATUS		GetReceiveFolderTable(mapi_object_t *, struct SRowSet *);
enum MAPISTATUS		GetTransportFolder(mapi_object_t *, mapi_id_t *);
enum MAPISTATUS		GetOwningServers(mapi_object_t *, mapi_object_t *, uint16_t *, uint16_t *, char **);
enum MAPISTATUS		GetStoreState(mapi_object_t *, uint32_t *);
enum MAPISTATUS		GetOutboxFolder(mapi_object_t *, mapi_id_t *);
enum MAPISTATUS		TransportNewMail(mapi_object_t *, mapi_object_t *, mapi_object_t *,const char *, uint32_t);

/* The following public definitions come from libmapi/IStoreFolder.c */
enum MAPISTATUS		OpenMessage(mapi_object_t *, mapi_id_t, mapi_id_t, mapi_object_t *, uint8_t);
enum MAPISTATUS		ReloadCachedInformation(mapi_object_t *);

/* The following public definitions come from libmapi/IUnknown.c */
enum MAPISTATUS		MAPIAllocateBuffer(struct mapi_context *, uint32_t, void **);
enum MAPISTATUS		MAPIFreeBuffer(void *);
enum MAPISTATUS		Release(mapi_object_t *);
enum MAPISTATUS		GetLastError(void);
enum MAPISTATUS		GetLongTermIdFromId(mapi_object_t *, mapi_id_t, struct LongTermId *);
enum MAPISTATUS		GetIdFromLongTermId(mapi_object_t *, struct LongTermId, mapi_id_t *);

/* The following public definitions come from libmapi/IStream.c */
enum MAPISTATUS		OpenStream(mapi_object_t *, enum MAPITAGS, enum OpenStream_OpenModeFlags, mapi_object_t *);
enum MAPISTATUS		ReadStream(mapi_object_t *, unsigned char *, uint16_t, uint16_t *);
enum MAPISTATUS		WriteStream(mapi_object_t *, DATA_BLOB *, uint16_t *);
enum MAPISTATUS		CommitStream(mapi_object_t *);
enum MAPISTATUS		GetStreamSize(mapi_object_t *, uint32_t *);
enum MAPISTATUS		SeekStream(mapi_object_t *, uint8_t, uint64_t, uint64_t *);
enum MAPISTATUS		SetStreamSize(mapi_object_t *, uint64_t);
enum MAPISTATUS		CopyToStream(mapi_object_t *, mapi_object_t *, uint64_t, uint64_t *, uint64_t *);
enum MAPISTATUS		LockRegionStream(mapi_object_t *, uint64_t, uint64_t, uint32_t);
enum MAPISTATUS		UnlockRegionStream(mapi_object_t *, uint64_t, uint64_t, uint32_t);
enum MAPISTATUS		CloneStream(mapi_object_t *, mapi_object_t *);
enum MAPISTATUS		WriteAndCommitStream(mapi_object_t *, DATA_BLOB *, uint16_t *);

/* The following public definitions come from libmapi/IXPLogon.c */
enum MAPISTATUS		AddressTypes(mapi_object_t *, uint16_t *, struct mapi_LPSTR **);
enum MAPISTATUS		SetSpooler(mapi_object_t *);
enum MAPISTATUS		SpoolerLockMessage(mapi_object_t *, mapi_object_t *, enum LockState);
enum MAPISTATUS		OptionsData(mapi_object_t *, const char *, uint8_t **, uint16_t *, uint8_t **, uint16_t *, const char** );

/* The following public definitions come from libmapi/FXICS.c */
enum MAPISTATUS		GetLocalReplicaIds(mapi_object_t *, uint32_t, struct GUID *, uint8_t [6]);
enum MAPISTATUS		TellVersion(mapi_object_t *, uint16_t version[3]);
enum MAPISTATUS		FXDestConfigure(mapi_object_t *, enum FastTransferDestConfig_SourceOperation, mapi_object_t *);
enum MAPISTATUS		FXCopyFolder(mapi_object_t *, uint8_t, uint8_t, mapi_object_t *);
enum MAPISTATUS		FXCopyMessages(mapi_object_t *, mapi_id_array_t *, uint8_t, uint8_t, mapi_object_t *);
enum MAPISTATUS		FXCopyTo(mapi_object_t *, uint8_t, uint32_t, uint8_t, struct SPropTagArray *, mapi_object_t *);
enum MAPISTATUS		FXCopyProperties(mapi_object_t *, uint8_t, uint32_t, uint8_t, struct SPropTagArray *, mapi_object_t *);
enum MAPISTATUS		FXGetBuffer(mapi_object_t *obj_source_context, uint16_t maxSize, enum TransferStatus *, uint16_t *, uint16_t *, DATA_BLOB *);
enum MAPISTATUS		FXPutBuffer(mapi_object_t *obj_dest_context, DATA_BLOB *blob, uint16_t *usedSize);
enum MAPISTATUS		ICSSyncConfigure(mapi_object_t *, enum SynchronizationType, uint8_t, uint16_t, uint32_t, DATA_BLOB, struct SPropTagArray*, mapi_object_t *);
enum MAPISTATUS		ICSSyncUploadStateBegin(mapi_object_t *, enum StateProperty, uint32_t);
enum MAPISTATUS		ICSSyncUploadStateContinue(mapi_object_t *, DATA_BLOB);
enum MAPISTATUS		ICSSyncUploadStateEnd(mapi_object_t *);
enum MAPISTATUS		SetLocalReplicaMidsetDeleted(mapi_object_t *, const struct GUID, const uint8_t GlobalCountLow[6], const uint8_t GlobalCountHigh[6]);
enum MAPISTATUS		ICSSyncOpenCollector(mapi_object_t *, bool, mapi_object_t *);
enum MAPISTATUS		ICSSyncGetTransferState(mapi_object_t *, mapi_object_t *);
enum MAPISTATUS		SyncImportReadStateChanges(mapi_object_t *, struct Binary_r *, bool *, uint16_t);

/* The following public definitions come from libmapi/freebusy.c */
enum MAPISTATUS		GetUserFreeBusyData(mapi_object_t *, const char *, struct SRow *);
enum MAPISTATUS		IsFreeBusyConflict(mapi_object_t *, struct FILETIME *, bool *);
int			GetFreeBusyYear(const uint32_t *);

/* The following public definitions come from libmapi/x500.c */
char			*x500_get_dn_element(TALLOC_CTX *, const char *, const char *);
char			*x500_truncate_dn_last_elements(TALLOC_CTX *, const char *, uint32_t);
char			*x500_get_servername(const char *);

/* The following public definitions come from libmapi/lzfu.c */
enum MAPISTATUS		WrapCompressedRTFStream(mapi_object_t *, DATA_BLOB *);
enum MAPISTATUS		uncompress_rtf(TALLOC_CTX *, uint8_t *, uint32_t, DATA_BLOB *);
uint32_t		calculateCRC(uint8_t *, uint32_t, uint32_t);
enum MAPISTATUS		compress_rtf(TALLOC_CTX *, const char*, const size_t, uint8_t **, size_t *);

/* The following public definitions come from libmapi/utils.c */
char			*guid_delete_dash(TALLOC_CTX *, const char *);
struct Binary_r		*generate_recipient_entryid(TALLOC_CTX *, const char *);
enum MAPISTATUS		GetFIDFromEntryID(uint16_t, uint8_t *, uint64_t, uint64_t *);
enum MAPISTATUS		EntryIDFromSourceIDForMessage(TALLOC_CTX *, mapi_object_t *, mapi_object_t *, mapi_object_t *, struct SBinary_short *);

/* The following public definitions come from libmapi/notif.c */
enum MAPISTATUS		SyncOpenAdvisor(mapi_object_t *, mapi_object_t *);
enum MAPISTATUS		SetSyncNotificationGuid(mapi_object_t *, const struct GUID);

/* The following public definitions come from libmapi/socket/netif.c */
int			get_interfaces_oc(struct iface_struct *, int);

/* The following public definitions come from libmapi/fxparser.c */
struct fx_parser_context;
typedef enum MAPISTATUS (*fxparser_marker_callback_t)(uint32_t, void *);
typedef enum MAPISTATUS (*fxparser_delprop_callback_t)(uint32_t, void *);
typedef enum MAPISTATUS (*fxparser_namedprop_callback_t)(uint32_t, struct MAPINAMEID, void *);
typedef enum MAPISTATUS (*fxparser_property_callback_t)(struct SPropValue, void *);

struct fx_parser_context *fxparser_init(TALLOC_CTX *, void *);
void 			fxparser_set_marker_callback(struct fx_parser_context *, fxparser_marker_callback_t);
void 			fxparser_set_delprop_callback(struct fx_parser_context *, fxparser_delprop_callback_t);
void 			fxparser_set_namedprop_callback(struct fx_parser_context *, fxparser_namedprop_callback_t);
void 			fxparser_set_property_callback(struct fx_parser_context *, fxparser_property_callback_t);
enum MAPISTATUS		fxparser_parse(struct fx_parser_context *, DATA_BLOB *);

/* The following public definitions come from libmapi/idset.c */
uint64_t		exchange_globcnt(uint64_t);

struct rawidset *	RAWIDSET_make(TALLOC_CTX *, bool, bool);
void			RAWIDSET_push_eid(struct rawidset *, uint64_t);
void			RAWIDSET_push_guid_glob(struct rawidset *, const struct GUID *, uint64_t);
struct idset *		RAWIDSET_convert_to_idset(TALLOC_CTX *, const struct rawidset *);

struct idset *		IDSET_parse(TALLOC_CTX *, DATA_BLOB, bool);
struct idset *		IDSET_merge_idsets(TALLOC_CTX *mem_ctx, const struct idset *, const struct idset *);
struct Binary_r *	IDSET_serialize(TALLOC_CTX *, const struct idset *);
bool			IDSET_includes_guid_glob(const struct idset *, struct GUID *, uint64_t);
bool			IDSET_includes_eid(const struct idset *, uint64_t);
void			IDSET_remove_rawidset(struct idset *, const struct rawidset *);
void			IDSET_dump(const struct idset *, const char *);
enum MAPISTATUS	IDSET_check_ranges(const struct idset *);
void			ndr_push_idset(struct ndr_push *, struct idset *);

struct globset_range *	GLOBSET_parse(TALLOC_CTX *, DATA_BLOB, uint32_t *, uint32_t *);

/* Size functions */

/**
   \details struct RecurrencePattern has fixed response size for:
   -# ReaderVersion: uint16_t
   -# WriterVersion: uint16_t
   -# RecurFrequency: uint16_t
   -# PatternType: uint16_t
   -# CalendarType: uint16_t
   -# FirstDateTime: uint32_t
   -# Period: uint32_t
   -# SlidingFlag: uint32_t
   -# EndType: uint32_t
   -# OccurrenceCount: uint32_t
   -# FirstDOW: uint32_t
   -# DeletedInstanceCount: uint32_t
   -# ModifiedInstanceCount: uint32_t
   -# StartDate: uint32_t
   -# EndDate: uint32_t
 */
#define	SIZE_DFLT_RECURRENCEPATTERN 50
size_t set_RecurrencePattern_size(const struct RecurrencePattern *);

/**
   \details struct AppointmentRecurrencePattern has fixed response size for:
   -# ReaderVersion2: uint32_t
   -# WriterVersion2: uint32_t
   -# StartTimeOffset: uint32_t
   -# EndTimeOffset: uint32_t
   -# ExceptionCount: uint16_t
   -# ReservedBlock1Size: uint32_t
   -# ReservedBlock2Size: uint32_t
 */
#define	SIZE_DFLT_APPOINTMENTRECURRENCEPATTERN 26
size_t set_AppointmentRecurrencePattern_size(const struct AppointmentRecurrencePattern *);

/**
   \details struct ExceptionInfo has fixed response size for:
   -# StartDateTime: uint32_t
   -# EndDateTime: uint32_t
   -# OriginalStartDate: uint32_t
   -# OverrideFlags: uint16_t
   -# AppointmentColor uint32_t
   -# ReservedBlock1Size uint32_t
 */
#define	SIZE_DFLT_EXCEPTIONINFO 22
size_t set_ExceptionInfo_size(const struct ExceptionInfo *);

/**
   \details struct ExceptionInfo has not fixed response size
 */
#define	SIZE_DFLT_EXTENDEDEXCEPTION 0

size_t set_ExtendedException_size(uint32_t, enum OverrideFlags, const struct ExtendedException *);

/**
   \details struct ExtendedException has fixed response size for:
   -# ReservedBlockEE1Size: uint32_t
   -# ReservedBlockEE2Size: uint32_t
 */
// #define	SIZE_DFLT_EXTENDEDEXCEPTION 8
// size_t set_ExtendedException_size(const struct ExtendedException *);

__END_DECLS

#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2)

#endif /* __LIBMAPI_H__ */
