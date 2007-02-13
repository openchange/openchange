/*
 *  OpenChange MAPI implementation.
 *  MAPI definitions
 *
 *  Copyright (C) Julien Kerihuel 2005-2007.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __MAPIDEFS_H__
#define __MAPIDEFS_H__

#define PROP_TAG(type, id) (((id << 16))| (type))
#define MV_FLAG			0x1000

/* Property types */
#define	PT_UNSPECIFIED		0x0
#define	PT_NULL			0x1
#define	PT_I2			0x2
#define	PT_SHORT		0x2
#define	PT_LONG			0x3
#define	PT_FLOAT		0x4
#define	PT_DOUBLE		0x5
#define	PT_CURRENCY		0x6
#define	PT_APPTIME		0x7
#define	PT_ERROR		0xa
#define	PT_BOOLEAN		0xb
#define	PT_OBJECT		0xd
#define	PT_I8			0x14
#define	PT_STRING8		0x1e
#define	PT_UNICODE		0x1f
#define	PT_SYSTIME		0x40
#define	PT_CLSID		0x48
#define	PT_BINARY		0x102

/* Multi valued property types */
#define	PT_MV_SHORT		(MV_FLAG | PT_SHORT)
#define	PT_MV_LONG		(MV_FLAG | PT_LONG)
#define	PT_MV_FLOAT		(MV_FLAG | PT_FLOAT)
#define	PT_MV_DOUBLE		(MV_FLAG | PT_DOUBLE)
#define	PT_MV_CURRENCY		(MV_FLAG | PT_CURRENCY)
#define	PT_MV_APPTIME		(MV_FLAG | PT_APPTIME)
#define	PT_MV_I8		(MV_FLAG | PT_I8)
#define	PT_MV_STRING8		(MV_FLAG | PT_STRING8)
#define	PT_MV_UNICODE		(MV_FLAG | PT_UNICODE)
#define	PT_MV_SYSTIME		(MV_FLAG | PT_SYSTIME)
#define PT_MV_CLSID		(MV_FLAG | PT_CLSID)
#define	PT_MV_BINARY		(MV_FLAG | PT_BINARY)

/* Restriction types */
#define RES_AND			0
#define RES_OR			1
#define RES_NOT			2
#define RES_CONTENT		3
#define RES_PROPERTY		4
#define RES_COMPAREPROPS	5
#define RES_BITMASK		6
#define RES_SIZE		7
#define RES_EXIST		8
#define RES_SUBRESTRICTION	9
#define RES_COMMENT		10

/* Object Type */
#define MAPI_STORE		0x1    	/* Message Store */
#define MAPI_ADDRBOOK		0x2    	/* Address Book */
#define MAPI_FOLDER		0x3    	/* Folder */
#define MAPI_ABCONT		0x4    	/* Address Book Container */
#define MAPI_MESSAGE		0x5    	/* Message */
#define MAPI_MAILUSER		0x6    	/* Individual Recipient */
#define MAPI_ATTACH		0x7    	/* Attachment */
#define MAPI_DISTLIST		0x8    	/* Distribution List Recipient */
#define MAPI_PROFSECT		0x9    	/* Profile Section */
#define MAPI_STATUS		0xA    	/* Status Object */
#define MAPI_SESSION		0xB	/* Session */
#define MAPI_FORMINFO		0xC	/* Form Information */

/* Display Type */
#define DT_MAILUSER		0x0
#define DT_DISTLIST		0x1
#define DT_FORUM		0x2
#define DT_AGENT		0x3
#define DT_ORGANIZATION		0x4
#define DT_PRIVATE_DISTLIST	0x5
#define DT_REMOTE_MAILUSER	0x6

/* PR_MESSAGE_FLAGS Flags */
#define MSGFLAG_READ		0x001
#define MSGFLAG_UNMODIFIED	0x002
#define MSGFLAG_SUBMIT		0x004
#define MSGFLAG_UNSENT		0x008
#define MSGFLAG_HASATTACH	0x010
#define MSGFLAG_FROMME		0x020
#define MSGFLAG_ASSOCIATED	0x040
#define MSGFLAG_RESEND		0x080
#define MSGFLAG_RN_PENDING	0x100
#define MSGFLAG_NRN_PENDING	0x200

/* Attachment method */
#define NO_ATTACHMENT		0
#define ATTACH_BY_VALUE		1
#define ATTACH_BY_REFERENCE	2
#define ATTACH_BY_REF_RESOLVE	3
#define ATTACH_BY_REF_ONLY	4
#define ATTACH_EMBEDDED_MSG	5
#define ATTACH_OLE		6

/*
 * ENTRYID flags
 */

/* definition for abFlags[0] */
#define MAPI_SHORTTERM          0x80
#define MAPI_NOTRECIP           0x40
#define MAPI_THISSESSION        0x20
#define MAPI_NOW                0x10
#define MAPI_NOTRESERVED        0x08

/* definition for abFlags[1] */
#define MAPI_COMPOUND           0x80

#endif /*!__MAPIDEFS_H__ */
