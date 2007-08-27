/*
   Stand-alone MAPI application

   OpenChange Project

   Copyright (C) Julien Kerihuel 2007

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

#ifndef __OPENCHANGECLIENT_H__
#define	__OPENCHANGECLIENT_H__

struct attach {
	const char		*filename;
	struct SBinary		bin;
	int			fd;
};

struct oclient {
	const char		*subject;
	const char		*pr_body;
	const char		*pr_html_inline;
	char			**usernames;
	char			**mapi_to;
	char			**mapi_cc;
	char			**mapi_bcc;
	struct SBinary		pr_html;
	struct attach		*attach;
	uint32_t		attach_num;
	const char		*store_folder;
	const char		*location;
	const char		*dtstart;
	const char		*dtend;
	uint32_t		busystatus;
	uint32_t		taskstatus;
	uint32_t		label;
	uint32_t		priority;
	const char		*email;
	const char		*full_name;
	const char		*card_name;
	/* PF related options */
	BOOL			pf;
	const char		*folder;
};

struct itemfolder {
	const uint32_t		olFolder;
	const char		*container_class;
};

struct itemfolder	defaultFolders[] = {
	{olFolderInbox,		"Mail"},
	{olFolderCalendar,	"Appointment"},
	{olFolderContacts,	"Contact"},
	{olFolderTasks,		"Task"},
	{olFolderNotes,		"Note"},
	{0 , NULL}
};

struct oc_element {
	uint8_t			index;
	const char		*status;
};

struct oc_element	oc_busystatus[] = {
	{BUSY_STATUS_FREE,		"FREE"},
	{BUSY_STATUS_TENTATIVE,		"TENTATIVE"},
	{BUSY_STATUS_BUSY,		"BUSY"},
	{BUSY_STATUS_OUTOFOFFICE,	"OUTOFOFFICE"},
	{0 , NULL}
};

struct oc_element	oc_priority[] = {
	{PRIORITY_LOW,		"LOW"},
	{PRIORITY_NORMAL,	"NORMAL"},
	{PRIORITY_HIGH,		"HIGH"},
	{0, NULL}
};

struct oc_element	oc_taskstatus[] = {
	{olTaskNotStarted,	"NOTSTARTED"},
	{olTaskInProgress,	"PROGRESS"},
	{olTaskComplete,	"COMPLETED"},
	{olTaskWaiting,		"WAITING"},
	{olTaskDeferred,	"DEFERRED"},
	{0, NULL}
};

#define	DEFAULT_PROFDB	"%s/.openchange/profiles.ldb"
#define	DATE_FORMAT	"%Y-%m-%d %H:%M:%S"
#define	CAL_CNPROPS	14
#define	CONTACT_CNPROPS	5
#define	TASK_CNPROPS	7

#endif /* !__OPENCHANGECLIENT_H__ */
