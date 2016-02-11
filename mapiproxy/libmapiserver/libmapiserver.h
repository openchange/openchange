/*
   libmapiserver - MAPI library for Server side

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009

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

#ifndef	__LIBMAPISERVER_H__
#define	__LIBMAPISERVER_H__

#include <sys/types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <dcerpc.h>
#include <param.h>

#include "libmapi/oc_log.h"
#include "gen_ndr/exchange.h"

#ifndef	__BEGIN_DECLS
#ifdef	__cplusplus
#define	__BEGIN_DECLS		extern "C" {
#define	__END_DECLS		}
#else
#define	__BEGIN_DECLS
#define	__END_DECLS
#endif
#endif

#define	SIZE_DFLT_MAPI_RESPONSE	6

/* Rops default and static size */

/**
   \details OpenFolderRop has fixed response size for
   -# HasRules: uint8_t
   -# IsGhosted: uint8_t   
 */
#define	SIZE_DFLT_ROPOPENFOLDER			2

/**
   \details OpenMessage has fixed response size for
   -# HasNamedProperties: uint8_t
   -# RecipientCount: uint16_t
   -# RowCount: uint8_t
 */
#define	SIZE_DFLT_ROPOPENMESSAGE		4

/**
   \details GetHierarchyTableTop has fixed response size for:
   -# RowCount: uint32_t
 */
#define	SIZE_DFLT_ROPGETHIERARCHYTABLE		4

/**
   \details GetContentsTableRop has fixed response size for:
   -# RowCount: uint32_t
 */
#define	SIZE_DFLT_ROPGETCONTENTSTABLE		4

/**
   \details CreateMessageRop has fixed response size for:
   -# HasMessageId: uint8_t
 */
#define	SIZE_DFLT_ROPCREATEMESSAGE		1

/**
   \details GetPropertiesSpecificRop has fixed response size for:
   -# layout: uint8_t
 */
#define	SIZE_DFLT_ROPGETPROPERTIESSPECIFIC	1

/**
   \details GetPropertiesAllRop has fixed response size for:
   -# cValues: uint16_t
 */
#define	SIZE_DFLT_ROPGETPROPERTIESALL		2

/**
   \details GetPropertiesListRop has fixed response size for:
   -# count: uint16_t
 */
#define	SIZE_DFLT_ROPGETPROPERTIESLIST		2

/**
   \details: SetPropertiesRop has fixed response size for:
   -# PropertyProblemCount: uint16_t
 */
#define	SIZE_DFLT_ROPSETPROPERTIES		2

/**
   \details: DeletePropertiesRop has fixed response size for:
   -# PropertyProblemCount: uint16_t
 */
#define	SIZE_DFLT_ROPDELETEPROPERTIES		2

/**
   \details: CopyToRop has fixed response size for:
   -# PropertyProblemCount: uint16_t
 */
#define	SIZE_DFLT_ROPCOPYTO		2

/**
   \details: SaveChangesMessageRop has fixed response size for:
   -# handle_idx: uint8_t
   -# MessageId: uint64_t
 */
#define	SIZE_DFLT_ROPSAVECHANGESMESSAGE		9

/**
   \details ReloadCachedInformation has fixed response size for:
   -# HasNamedProperties: uint8_t
   -# RecipientCount: uint16_t
   -# RowCount: uint8_t
 */
#define	SIZE_DFLT_ROPRELOADCACHEDINFORMATION	4

/**
   \details: SetMessageReadFlag has fixed response size for:
   -# ReadStatusChanged: uint8_t
 */
#define	SIZE_DFLT_ROPSETMESSAGEREADFLAG		1

/**
   \details: GetMessageStatus has fixed response size for:
   -# MessageStatusFlags: uint32_t
 */
#define	SIZE_DFLT_ROPGETMESSAGESTATUS		4

/**
   \details: CreateAttachRop has fixed response size for:
   -# AttachmentId: uint32_t
 */
#define	SIZE_DFLT_ROPCREATEATTACH		4

/**
   \details: OpenEmbeddedMessage has fixed response size for:
   -# Reserved: uint8_t
   -# MessageId: uint64_t
   -# HasNamedProperties: uint8_t
   -# RecipientCount: uint16_t
   -# RecipientColumns.cValues: uint16_t
   -# RowCount: uint8_t
 */
#define SIZE_DFLT_ROPOPENEMBEDDEDMESSAGE        15

/**
   \details SetColumnsRop has fixed response size for:
   -# TableStatus: uint8_t
 */
#define	SIZE_DFLT_ROPSETCOLUMNS			1

/**
   \details SortTableRop has fixed response size for:
   -# TableStatus: uint8_t
 */
#define	SIZE_DFLT_ROPSORTTABLE			1

/**
   \details RestrictRop has fixed response size for:
   -# TableStatus: uint8_t
 */
#define	SIZE_DFLT_ROPRESTRICT			1

/**
   \details QueryRowsRop has fixed size for:
   -# Origin: uint8_t
   -# RowCount: uint16_t
 */
#define	SIZE_DFLT_ROPQUERYROWS			3

/**
   \details QueryPositionRop has fixed response size for:
   -# Numerator: uint32_t
   -# Denominator: uint32_t
 */
#define	SIZE_DFLT_ROPQUERYPOSITION		8

/**
   \details SeekRowRop has fixed response size for:
   -# HasSought: uint8_t
   -# RowsSought: uint32_t
 */
#define	SIZE_DFLT_ROPSEEKROW			5

/**
   \details CreateFolderRop has fixed response size for:
   -# folder_id: uint64_t
   -# isExistingFolder: uint8_t
 */
#define	SIZE_DFLT_ROPCREATEFOLDER		9

/**
   \details DeleteFolderRop has fixed response size for:
   -# PartialCompletion: uint8_t
 */
#define	SIZE_DFLT_ROPDELETEFOLDER		1


/**
   \details OpenStreamRop has fixed response size for:
   -# StreamSize: uint32_t
 */
#define	SIZE_DFLT_ROPOPENSTREAM			4

/**
   \details ReadStreamRop has fixed response size for:
   -# DataSize: uint16_t
 */
#define	SIZE_DFLT_ROPREADSTREAM			2

/**
   \details WriteStreamRop has fixed response size for:
   -# WrittenSize: uint16_t
 */
#define	SIZE_DFLT_ROPWRITESTREAM		2

/**
   \details GetStreamSize has fixed response size for:
   -# StreamSize: uint32_t
 */
#define	SIZE_DFLT_ROPGETSTREAMSIZE		4

/**
   \details SeekStream has fixed response size for:
   -# NewPosition: uint64_t
 */
#define	SIZE_DFLT_ROPSEEKSTREAM 		8

/**
   \details GetReceiveFolder has fixed response size for:
   -# folder_id: uint64_t
 */
#define	SIZE_DFLT_ROPGETRECEIVEFOLDER		8

/**
   \details GetAddressTypes has fixed response size for:
   -# cValues: uint16_t
   -# size: uint16_t
 */
#define	SIZE_DFLT_ROPGETADDRESSTYPES		4

/**
   \details TransportSend  has fixed response size for:
   -# NoPropertiesReturned: uint8_t
 */
#define	SIZE_DFLT_ROPTRANSPORTSEND		1

/**
   \details GetTransportFolder has fixed response size for:
   -# FolderId: uint64_t
 */
#define	SIZE_DFLT_ROPGETTRANSPORTFOLDER		8

/**
   \details OptionsData has fixed response size for:
   -# Reserved: uint8_t
   -# OptionsInfo: uint16_t part of SBinary_short
   -# HelpFileSize: uint16_t
 */
#define	SIZE_DFLT_ROPOPTIONSDATA		5

/**
   \details FindRow has fixed response size for:
   -# RowNoLongerVisible: uint8_t
   -# HasRowData: uint8_t
 */
#define	SIZE_DFLT_ROPFINDROW			2

/**
   \details GetNamesFromIDs has fixed response size for:
   -# PropertyNameCount: uint16_t
 */
#define	SIZE_DFLT_ROPGETNAMESFROMIDS		2

/**
   \details GetPropertyIdsFromNames has fixed response size for:
   -# count: uint16_t
 */
#define	SIZE_DFLT_ROPGETPROPERTYIDSFROMNAMES	2

/**
   \details GetPropertyIdsFromNames has fixed response size for:
   -# PropertyProblemCount: uint16_t
 */
#define SIZE_DFLT_ROPDELETEPROPERTIESNOREPLICATE 2

/**
   \details EmptyFolder has fixed response size for:
   -# PartialCompletion: uint8_t
 */
#define SIZE_DFLT_ROPEMPTYFOLDER                1

/**
   \details MoveCopyMessages Rop has fixed response size for:
   -#: PartialCompletion: uint8_t

*/
#define SIZE_DFLT_ROPMOVECOPYMESSAGES           1 

/**
   \details MoveFolder Rop has fixed response size for:
   -#: PartialCompletion: uint8_t

*/
#define SIZE_DFLT_ROPMOVEFOLDER                 1 

/**
   \details CopyFolder Rop has fixed response size for:
   -#: PartialCompletion: uint8_t

*/
#define SIZE_DFLT_ROPCOPYFOLDER                 1 

/**
   \details DeleteMessage Rop has fixed response size for:
   -# PartialCompletion: uint8_t
 */
#define	SIZE_DFLT_ROPDELETEMESSAGE		1

/**
   \details Notify Rop has non-default fixed response size for:
   -# RopId: uint8_t
   -# NotificationHandle: uint32_t
   -# LogonId: uint8_t
   -# NotificationType: uint16_t
 */
#define	SIZE_DFLT_ROPNOTIFY                     8

/**
   \details GetSearchCriteria Rop has fixed response size for:
   -# RestrictionDataSize: uint16_t
   -# LogonId: uint8_t
   -# FolderIdCount: uint16_t
   -# SearchFlags: uint32_t
 */
#define	SIZE_DFLT_ROPGETSEARCHCRITERIA		9

/**
   \details LongTermIdFromId Rop has fixed response size for:
   -# DatabaseGuid: 16 * uint8_t
   -# LongTermId: 6 * uint8_t
   -# Padding: uint16_t
*/
#define SIZE_DFLT_ROPLONGTERMIDFROMID		24;

/**
   \details IdFromLongTermId Rop has fixed response size for:
   -# Id: 8 * uint8_t
*/
#define SIZE_DFLT_ROPIDFROMLONGTERMID		8;

/**
   \details GetPerUserLongTermIds has fixed response size for:
   -# LongTermIdCount: uint16_t
 */
#define	SIZE_DFLT_ROPGETPERUSERLONGTERMIDS	2

/**
   \details ReadPerUserInformation has fixed response size for:
   -# HasFinished: uint8_t
   -# DataSize: uint16_t
 */
#define	SIZE_DFLT_ROPREADPERUSERINFORMATION	3

/**
   \details GetPerUserGuid has fixed response size for:
   -# DatabaseGuid: uint8_t * 16
 */
#define	SIZE_DFLT_ROPGETPERUSERGUID		16

/**
   \details GetStoreState has fixed response size for:
   -# StoreState: uin32_t
 */
#define SIZE_DFLT_ROPGETSTORESTATE		4

/**
   \details LogonRop has a fixed size for mailbox:
   -# LogonFlags: uint8_t
   -# FolderIDs: uint64_t * 13
   -# ResponseFlags: uint8_t
   -# MailboxGUID: sizeof (struct GUID)
   -# ReplID: uint16_t
   -# ReplGUID: sizeof (struct GUID)
   -# LogonTime: uint8_t * 6 + uint16_t
   -# GwartTime: uint64_t
   -# StoreState: uint32_t
 */
#define	SIZE_DFLT_ROPLOGON_MAILBOX	160

/**
   \details LogonRop has a fixed size for public folder logon:
   -# LogonFlags: uint8_t
   -# FolderIDs: uint64_t * 13
   -# ReplId: uint16_t
   -# ReplGuid: sizeof (struct GUID) = 16 bytes
   -# PerUserGuid: sizeof (struct GUID) = 16 bytes
 */
#define	SIZE_DFLT_ROPLOGON_PUBLICFOLDER	139

/**
   \details LogonRop has a fixed size for redirect response:
   -# LogonFlags: uint8_t
   -# ServerNameSize: uint8_t
 */
#define	SIZE_DFLT_ROPLOGON_REDIRECT	2

#define	SIZE_NULL_TRANSACTION		2

/**
   \details LongTermId structure is fixed size:
   -# DatabaseGUID: uint8_t * 16
   -# GlobalCounter: uint8_t * 6
   -# padding: uint16_t
 */
#define	SIZE_DFLT_LONGTERMID		24

/**
   \details PropertyName structure is fixed size:
   -# Kind: uint8_t
   -# GUID: uint8_t * 16
 */
#define	SIZE_DFLT_PROPERTYNAME		17

/**
   \details FastTransferSourceGetBuffer has a fixed size for:
   -# TransferStatus: uint16_t
   -# InProgressCount: uint16_t
   -# TotalStepCount: uint16_t
   -# Reserved (1 byte): uint8_t
   -# TransferBufferSize (2 bytes): uint16_t
 */
#define SIZE_DFLT_ROPFASTTRANSFERSOURCEGETBUFFER 9

/**
   \details SyncImportMessageChange has a fixed size for:
   -# FolderId: uint64_t
*/
#define SIZE_DFLT_ROPSYNCIMPORTMESSAGECHANGE 8

/**
   \details SyncImportHierarchyChange has a fixed size for:
   -# FolderId: uint64_t
*/
#define SIZE_DFLT_ROPSYNCIMPORTHIERARCHYCHANGE 8

/**
   \details SyncImportMessageMove has a fixed size for:
   -# MessageId: uint64_t
*/
#define SIZE_DFLT_ROPSYNCIMPORTMESSAGEMOVE 8

/**
   \details GetLocalReplicaIds has a fixed size for:
   -# ReplGuid: sizeof (struct GUID)
   -# GlobalCount: uint8_t * 6
 */
#define SIZE_DFLT_ROPGETLOCALREPLICAIDS 22

__BEGIN_DECLS

/* definitions from libmapiserver_oxcfold.c */
uint16_t libmapiserver_RopOpenFolder_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetHierarchyTable_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetContentsTable_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopCreateFolder_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopDeleteFolder_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopDeleteMessage_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSetSearchCriteria_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetSearchCriteria_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopEmptyFolder_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopMoveCopyMessages_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopMoveFolder_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopCopyFolder_size(struct EcDoRpc_MAPI_REPL *);


/* definitions from libmapiserver_oxcmsg.c */
uint16_t libmapiserver_RopOpenMessage_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopCreateMessage_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSaveChangesMessage_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopRemoveAllRecipients_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopModifyRecipients_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopReloadCachedInformation_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSetMessageReadFlag_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetMessageStatus_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetAttachmentTable_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopOpenAttach_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopCreateAttach_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSaveChangesAttachment_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopOpenEmbeddedMessage_size(struct EcDoRpc_MAPI_REPL *response);

/* definitions from libmapiserver_oxcnotif.c */
uint16_t libmapiserver_RopRegisterNotification_size(void);
uint16_t libmapiserver_RopNotify_size(struct EcDoRpc_MAPI_REPL *);

/* definitions from libmapiserver_oxcdata.c */
uint16_t libmapiserver_TypedString_size(struct TypedString);
uint16_t libmapiserver_RecipientRow_size(struct RecipientRow);
uint16_t libmapiserver_LongTermId_size(void);
uint16_t libmapiserver_PropertyName_size(struct MAPINAMEID *);
uint16_t libmapiserver_mapi_SPropValue_size(uint16_t, struct mapi_SPropValue *);

/* definitions from libmapiserver_oxcprpt.c */
uint16_t libmapiserver_RopSetProperties_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopDeleteProperties_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetPropertiesSpecific_size(struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetPropertiesAll_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetPropertiesList_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopOpenStream_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopReadStream_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopWriteStream_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopCommitStream_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetStreamSize_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSeekStream_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSetStreamSize_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetNamesFromIDs_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetPropertyIdsFromNames_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopDeletePropertiesNoReplicate_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopCopyTo_size(struct EcDoRpc_MAPI_REPL *);

/**
   \details Add a property value to a DATA blob. This convenient
   function should be used when creating a GetPropertiesSpecific reply
   response blob.

   \param mem_ctx pointer to the memory context
   \param property the property tag which value is meant to be
   appended to the blob
   \param value generic pointer on the property value
   \param blob the data blob the function uses to return the blob
   \param layout whether values should be prefixed by a layout
   \param flagged define if the properties are flagged or not
   \param untyped Whether the property type was originally untyped or not
   and in that case we will push the property type as well

   \note blob.length must be set to 0 before this function is called
   the first time. Also the function only supports a limited set of
   property types at the moment.

   \return 0 on success, -1 otherwise
 */
int libmapiserver_push_property(TALLOC_CTX *mem_ctx, uint32_t property,
	const void *value, DATA_BLOB *blob, uint8_t layout, uint8_t flagged,
	uint8_t untyped);

/**
   \details Add properties values to a DATA blob.

   \param mem_ctx pointer to the memory context
   \param num_props number of properties to push
   \param properties array of property tags which values are meant to be
   appended to the blob
   \param value array with generic pointers on the properties values
   \param retvals array of error codes for the properties to push
   \param blob the data blob the function uses to return the blob
   \param layout whether values should be prefixed by a layout
   \param flagged define whether the properties are flagged or not
   \param untyped Whether to push property type or not

   \see libmapiserver_push_property
 */
void libmapiserver_push_properties(TALLOC_CTX *mem_ctx, size_t num_props,
	enum MAPITAGS *properties, void **values, enum MAPISTATUS *retvals,
	DATA_BLOB *blob, uint8_t layout, uint8_t flagged, bool untyped);

/**
   \details Add properties values to a DATA blob.

   \param mem_ctx pointer to the memory context
   \param num_props number of properties to push
   \param properties array of property tags which values are meant to be
   appended to the blob
   \param value array with generic pointers on the properties values
   \param retvals array of error codes for the properties to push
   \param blob the data blob the function uses to return the blob
   \param layout whether values should be prefixed by a layout
   \param flagged define whether the properties are flagged or not
   \param untyped Array of booleans whether to push each property type or not

   \note retvals array will be check and if the value is different of
   MAPI_E_SUCCESS the property will be pushed as an error

   \see libmapiserver_push_property
 */
void libmapiserver_push_properties_with_untyped(TALLOC_CTX *mem_ctx,
	size_t num_props, enum MAPITAGS *properties, void **values,
	enum MAPISTATUS *retvals, DATA_BLOB *blob, uint8_t layout,
	uint8_t flagged, bool *untyped);

struct SRow *libmapiserver_ROP_request_to_properties(TALLOC_CTX *, void *, uint8_t);

/* definitions from libmapiserver_oxcstor.c */
uint16_t libmapiserver_RopLogon_size(struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopRelease_size(void);
uint16_t libmapiserver_RopSetReceiveFolder_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetReceiveFolder_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopLongTermIdFromId_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopIdFromLongTermId_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetPerUserLongTermIds_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopReadPerUserInformation_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetPerUserGuid_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetStoreState_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetReceiveFolderTable_size(struct EcDoRpc_MAPI_REPL *);

/* definitions from libmapiserver_oxctabl.c */
uint16_t libmapiserver_RopSetColumns_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSortTable_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopRestrict_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopQueryRows_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopQueryPosition_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSeekRow_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopFindRow_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopResetTable_size(struct EcDoRpc_MAPI_REPL *);

/* definitions from libmapiserver_oxomsg.c */
uint16_t libmapiserver_RopSubmitMessage_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSetSpooler_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetAddressTypes_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopTransportSend_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetTransportFolder_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopOptionsData_size(struct EcDoRpc_MAPI_REPL *);

/* definitions from libmapiserver_oxorule.c */
uint16_t libmapiserver_RopGetRulesTable_size(void);
uint16_t libmapiserver_RopModifyRules_size(void);

/* definitions from libmapiserver_oxcperm.c */
uint16_t libmapiserver_RopGetPermissionsTable_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopModifyPermissions_size(struct EcDoRpc_MAPI_REPL *);

/* definitions from libmapiserver_oxcfxics.c */
uint16_t libmapiserver_RopFastTransferSourceCopyTo_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopFastTransferSourceGetBuffer_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSyncConfigure_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSyncImportMessageChange_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSyncImportHierarchyChange_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSyncImportDeletes_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSyncUploadStateStreamBegin_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSyncUploadStateStreamContinue_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSyncUploadStateStreamEnd_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSyncImportMessageMove_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSyncOpenCollector_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetLocalReplicaIds_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSyncImportReadStateChanges_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSyncGetTransferState_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSetLocalReplicaMidsetDeleted_size(struct EcDoRpc_MAPI_REPL *response);

__END_DECLS

#endif /* ! __LIBMAPISERVER_H__ */
