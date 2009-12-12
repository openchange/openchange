/*
   OpenChange MAPI implementation.
   MAPI definitions

   Copyright (C) Julien Kerihuel 2005 - 2007.

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

#ifndef __MAPIDEFS_H__
#define __MAPIDEFS_H__

#define PROP_TAG(type, id) (((id << 16))| (type))
#define MV_FLAG			0x1000

/* UNICODE flags */
#define	MAPI_UNICODE		0x80000000

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
#define	PT_SVREID		0xFB
#define	PT_SRESTRICT		0xFD
#define	PT_ACTIONS		0xFE
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

/* Resolve types */
#define MAPI_UNRESOLVED		0x0
#define MAPI_AMBIGUOUS		0x1
#define MAPI_RESOLVED		0x2

/* Positioning Minimal Entry IDs */
#define	MID_BEGINNING_OF_TABLE	0x0
#define	MID_CURRENT		0x1
#define	MID_END_OF_TABLE	0x2

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
#define	DT_CONTAINER		0x100
#define	DT_TEMPLATE		0x101
#define	DT_ADDRESS_TEMPLATE	0x102
#define	DT_SEARCH		0x200

/* Attachment method */
#define NO_ATTACHMENT		0
#define ATTACH_BY_VALUE		1
#define ATTACH_BY_REFERENCE	2
#define ATTACH_BY_REF_RESOLVE	3
#define ATTACH_BY_REF_ONLY	4
#define ATTACH_EMBEDDED_MSG	5
#define ATTACH_OLE		6

/* Creation flags */
#define	MAPI_CREATE		0x2

/* SaveChanges flags */
#define KEEP_OPEN_READONLY	0x1
#define KEEP_OPEN_READWRITE	0x2
#define FORCE_SAVE		0x4
/* #define MAPI_DEFERRED_ERRORS 0x8 (already defined in the IDL */

/* OpenMessage flags */
#define	MAPI_MODIFY             0x1
/* see MAPI_CREATE above */


/* GetGALTable flags */
#define	TABLE_START		0x0
#define	TABLE_CUR		0x1

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

/*
 * Priority
 */

#define	PRIORITY_LOW		-1
#define	PRIORITY_NORMAL		0
#define	PRIORITY_HIGH		1

/*
 * Importance
 */

#define	IMPORTANCE_LOW		0
#define	IMPORTANCE_NORMAL	1
#define	IMPORTANCE_HIGH		2

/*
 * Color
 */

#define	olBlue			0
#define	olGreen			1
#define	olPink			2
#define	olYellow		3
#define	olWhite			4


/*
 * Appointment flags with PR_APPOINTMENT_BUSY_STATUS
 */

#define	BUSY_STATUS_FREE	0
#define	BUSY_STATUS_TENTATIVE	1
#define	BUSY_STATUS_BUSY	2
#define	BUSY_STATUS_OUTOFOFFICE	3

/*
 * Appointment meeting status
 */
#define MEETING_STATUS_NONMEETING	0
#define MEETING_STATUS_MEETING		1

/*
 * Task status
 */

#define	olTaskNotStarted		0
#define	olTaskInProgress		1
#define	olTaskComplete			2
#define	olTaskWaiting			3
#define	olTaskDeferred			4

/*
 * Task OwnerShip
 */
#define	olNewTask			0
#define	olDelegatedTask			1
#define	olOwnTask			2

/*
 * PR_MESSAGE_EDITOR_FORMAT type
 */
#define	EDITOR_FORMAT_PLAINTEXT		1
#define	EDITOR_FORMAT_HTML		2
#define	EDITOR_FORMAT_RTF		3

#define	olEditorText			1
#define	olEditorHTML			2
#define	olEditorRTF			3
#define	olEditorWord			4

/*
 * Default folders
 */
#define	olFolderTopInformationStore	1
#define	olFolderDeletedItems		3
#define	olFolderOutbox			4
#define	olFolderSentMail		5
#define	olFolderInbox			6
#define	olFolderCommonView		8
#define	olFolderCalendar		9
#define	olFolderContacts		10
#define	olFolderJournal			11
#define	olFolderNotes			12
#define	olFolderTasks			13
#define	olFolderDrafts			16
#define	olPublicFoldersAllPublicFolders	18
#define	olFolderConflicts		19
#define	olFolderSyncIssues		20
#define	olFolderLocalFailures		21
#define	olFolderServerFailures		22
#define	olFolderJunk			23
#define	olFolderFinder			24
#define	olFolderPublicRoot		25
#define	olFolderPublicIPMSubtree	26
#define	olFolderPublicNonIPMSubtree	27
#define	olFolderPublicEFormsRoot	28
#define	olFolderPublicFreeBusyRoot	29
#define	olFolderPublicOfflineAB		30
#define	olFolderPublicEFormsRegistry	31
#define	olFolderPublicLocalFreeBusy	32
#define	olFolderPublicLocalOfflineAB	33
#define	olFolderPublicNNTPArticle	34

/*
 * IPF container class
 */

#define	IPF_APPOINTMENT			"IPF.Appointment"
#define	IPF_CONTACT			"IPF.Contact"
#define	IPF_JOURNAL			"IPF.Journal"
#define	IPF_NOTE			"IPF.Note"
#define	IPF_STICKYNOTE			"IPF.StickyNote"
#define	IPF_TASK			"IPF.Task"
#define	IPF_POST			"IPF.Post"

/*
 * Common OLEGUID - see MS-OXPROPS, Section 1.3.2
 */

#define	PSETID_Appointment	"00062002-0000-0000-c000-000000000046"
#define	PSETID_Task		"00062003-0000-0000-c000-000000000046"
#define	PSETID_Address		"00062004-0000-0000-c000-000000000046"
#define	PSETID_Common		"00062008-0000-0000-c000-000000000046"
#define	PSETID_Note		"0006200e-0000-0000-c000-000000000046"
#define	PSETID_Log		"0006200a-0000-0000-c000-000000000046"
#define	PSETID_Sharing		"00062040-0000-0000-c000-000000000046"
#define	PSETID_PostRss		"00062041-0000-0000-c000-000000000046"
#define	PSETID_UnifiedMessaging	"4442858e-a9e3-4e80-b900-317a210cc15b"
#define	PSETID_Meeting		"6ed8da90-450b-101b-98da-00aa003f1305"
#define	PSETID_AirSync		"71035549-0739-4dcb-9163-00f0580dbbdf"
#define	PSETID_Attachment	"96357f7f-59e1-47d0-99a7-46515c183b54"
#define	PS_PUBLIC_STRINGS	"00020329-0000-0000-c000-000000000046"
#define	PS_INTERNET_HEADERS	"00020386-0000-0000-c000-000000000046"
#define	PS_MAPI			"00020328-0000-0000-c000-000000000046"

/* FreeBusy strings for Exchange 2003 and below */
#define	FREEBUSY_FOLDER		"EX:/o=%s/ou=%s"
#define	FREEBUSY_USER		"USER-/CN=RECIPIENTS/CN=%s"

#endif /*!__MAPIDEFS_H__ */
