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

#ifndef	_GNU_SOURCE
#define	_GNU_SOURCE 1
#endif

#include <sys/types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include <dcerpc.h>
#include <param.h>

#include <gen_ndr/exchange.h>

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
   \details: SetPropertiesRop has fixed response size for:
   -# PropertyProblemCount: uint16_t
 */
#define	SIZE_DFLT_ROPSETPROPERTIES		2

/**
   \details: SaveChangesMessageRop has fixed response size for:
   -# handle_idx: uint8_t
   -# MessageId: uint64_t
 */
#define	SIZE_DFLT_ROPSAVECHANGESMESSAGE		9

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
   \details GetReceiveFolder has fixed response size for:
   -# folder_id: uint64_t
 */
#define	SIZE_DFLT_ROPGETRECEIVEFOLDER		8

/**
   \details FindRow has fixed response size for:
   -# RowNoLongerVisible: uint8_t
   -# HasRowData: uint8_t
 */
#define	SIZE_DFLT_ROPFINDROW			2

/**
   \details GetPropertyIdsFromNames has fixed response size for:
   -# count: uint16_t
 */
#define	SIZE_DFLT_ROPGETPROPERTYIDSFROMNAMES	2

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
   \details LogonRop has a fixed size for redirect response:
   -# LogonFlags: uint8_t
   -# ServerNameSize: uint8_t
 */
#define	SIZE_DFLT_ROPLOGON_REDIRECT	2

#define	SIZE_NULL_TRANSACTION		2

__BEGIN_DECLS

/* definitions from libmapiserver_oxcfold.c */
uint16_t libmapiserver_RopOpenFolder_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetHierarchyTable_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetContentsTable_size(struct EcDoRpc_MAPI_REPL *);

/* definitions from libmapiserver_oxcmsg.c */
uint16_t libmapiserver_RopCreateMessage_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSaveChangesMessage_size(struct EcDoRpc_MAPI_REPL *);

/* definitions from libmapiserver_oxcnotif.c */
uint16_t libmapiserver_RopRegisterNotification_size(void);

/* definitions from libmapiserver_oxcprpt.c */
uint16_t libmapiserver_RopSetProperties_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetPropertiesSpecific_size(struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopGetPropertyIdsFromNames_size(struct EcDoRpc_MAPI_REPL *);
int libmapiserver_push_property(TALLOC_CTX *, struct smb_iconv_convenience *, uint32_t, const void *, DATA_BLOB *, uint8_t, uint8_t);

/* definitions from libmapiserver_oxcstor.c */
uint16_t libmapiserver_RopLogon_size(struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopRelease_size(void);
uint16_t libmapiserver_RopGetReceiveFolder_size(struct EcDoRpc_MAPI_REPL *);

/* definitions from libmapiserver_oxctabl.c */
uint16_t libmapiserver_RopSetColumns_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSortTable_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopRestrict_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopQueryRows_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopQueryPosition_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopSeekRow_size(struct EcDoRpc_MAPI_REPL *);
uint16_t libmapiserver_RopFindRow_size(struct EcDoRpc_MAPI_REPL *);

/* definitions from libmapiserver_oxorule.c */
uint16_t libmapiserver_RopGetRulesTable_size(void);

__END_DECLS

#endif /* ! __LIBMAPISERVER_H__ */
