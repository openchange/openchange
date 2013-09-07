/*
   OpenChange MAPI implementation.
   FXICS header file

   Copyright (C) Julien Kerihuel 2013.

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

#ifndef	__FXICS_H__
#define	__FXICS_H__

/* Folders Markers [MS-OXCFXICS] - 2.2.4.1.4 */
#define	StartTopFld				0x40090003
#define	StartSubFld				0x400A0003
#define	EndFolder				0x400B0003

/* Messages and their parts Markers [MS-OXCFXICS] - 2.2.4.1.4 */
#define	StartMessage				0x400C0003
#define	EndMessage				0x400D0003
#define	StartFAIMsg				0x40100003 
#define	StartEmbed				0x40010003 
#define	EndEmbed				0x40020003
#define	StartRecip				0x40030003 
#define	EndToRecip				0x40040003
#define	NewAttach				0X40000003 
#define	EndAttach				0x400E0003

/* Synchronization method Markers [MS-OXCFXICS] - 2.2.4.1.4 */
#define	IncrSyncChg				0x40120003
#define	IncrSyncChgPartial			0x407D0003
#define	IncrSyncDel				0x40130003
#define	IncrSyncEnd				0x40140003
#define	IncrSyncRead				0x402F0003
#define	IncrSyncStateBegin			0x403A0003
#define	IncrSyncStateEnd			0x403B0003
#define	IncrSyncProgressMode			0x4074000B
#define	IncrSyncProgressPerMsg			0x4075000B
#define	IncrSyncMessage				0x40150003
#define	IncrSyncGroupInfo			0x407B0102

/* Special */
#define	FXErrorInfo				0x40180003

/* Meta-Properties [MS-OXCFXICS] - 2.2.4.1.5 */
#define	MetaTagFXDelProp			0x40160003
#define	MetaTagEcWarning			0x400F0003
#define	MetaTagNewFXFolder			0x40110102
#define	MetaTagIncrSyncGroupId			0x407C0003
#define	MetaTagIncrementalSyncMessagePartial	0x407A0003
#define	MetaTagDnPrefix				0x4008001E

/* ICS State Properties [MS-OXCFXICS] - 2.2.1.1 */
#define	MetaTagIdsetGiven			0x40170003
#define	MetaTagCnsetSeen			0x67960102
#define	MetaTagCnsetSeenFAI			0x67DA0102
#define	MetaTagCnsetRead			0x67D20102

/* Meta-Properties for Encoding Differences in Replica Content
   [MS-OXCFXICS] - 2.2.1.3 */
#define	MetaTagIdsetDeleted			0x67E50102
#define	MetaTagIdsetNoLongerInScope		0x40210102
#define	MetaTagIdsetExpired			0x67930102
#define	MetaTagIdsetRead			0x402D0102
#define	MetaTagIdsetUnread			0x402E0102

#endif /* ! __FXICS_H__ */
