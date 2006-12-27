/*
   OpenChange Project

   ems address book provider implementation

   Copyright (C) Julien Kerihuel 2006
   Copyright (C) Pauline Khun 2006

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

#ifndef __EMSABP_H
#define __EMSABP_H

#include "libmapi/include/property.h"

/*
 * ENTRY_ID structure for ADDRESS BOOK provider
 * uint8_t		abFlags[4]	// bitmask of flags that provide information describing 
 *					// the object set to 0 (LONGTERM?)
 * struct MAPIUID	provider_uid;	// the address book provider guid
 * uint8_t		entry_id_end[7]
 * const		char *exchange_dn;
 *
 */

#define	PACKED_AB_GUID	"guid=%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X"

static const uint8_t AB_MAPIUID[] = {
	0xdc, 0xa7, 0x40, 0xc8, 0xc0, 0x42, 0x10, 0x1a,
	0xb4, 0xb9, 0x08, 0x00, 0x2b, 0x2f, 0xe1, 0x82
};

static const uint8_t ABENTRYID_END[] = {
	0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00
};

struct entry_id {
	uint8_t			abFlags;	/* bitmask of flags that provide information describing the object. */
	uint8_t			ab;		/* specifies the binary data used by service providers */
	struct GUID		guid;
	uint32_t		instance_key;
	struct ldb_message	*msg;
	struct entry_id		*next;
};

struct emsabp_ctx {
	void		*conf_ctx;
	void		*users_ctx;
	void		*ldb_ctx;
	TALLOC_CTX	*mem_ctx;
	struct entry_id	*entry_ids;
};

struct emsabp_containerID {
	const char	*name;
	uint32_t	id;
};

#define	EMSABP_CTX	"emsabp context"
#define	EMSABP_ADDRTYPE	"EX"

/* 
 * Display Type values 
 */

/*  For address book contents tables */
#define	DT_MAILUSER		0x0
#define DT_DISTLIST		0x1
#define DT_FORUM		0x2
#define DT_AGENT		0x3
#define DT_ORGANIZATION		0x4
#define DT_PRIVATE_DISTLIST	0x5
#define DT_REMOTE_MAILUSER	0x6

/*  For address book hierarchy tables */
#define DT_MODIFIABLE		0x10000
#define DT_GLOBAL		0x20000
#define DT_LOCAL		0x30000
#define DT_WAN			0x40000
#define DT_NOT_SPECIFIC		0x50000

/*  For folder hierarchy tables */
#define DT_FOLDER		0x01000000
#define DT_FOLDER_LINK		0x02000000
#define DT_FOLDER_SPECIAL	0x04000000

/* Container flags */
#define	AB_RECIPIENTS		0x1	/* The container CAN hold recipients */
#define	AB_SUBCONTAINERS	0x2	/* The container CAN hold child containers */
#define	AB_MODIFIABLE		0x4	/* Entries can be added to and removed from the container */
#define	AB_UNMODIFIABLE		0x8	/* Entries cannot be added to or removed from the container */
#define	AB_FIND_ON_OPEN		0x10	/* Displays a dialog box to request a restriction before diplaying any contents of the container */
#define	AB_NOT_DEFAULT		0x20	/* The container is not the default one */

/* GetHierarchyInfo flags */
#define	MAPI_UNICODE		0x80000000
#define	MAPI_DEFERRED_ERRORS	0x00000008
#define	CONVENIENT_DEPTH	0x00000001

#endif /* __EMSABP_H */
