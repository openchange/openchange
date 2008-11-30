/*
   Stand-alone MAPI application

   OpenChange Project

   Copyright (C) Julien Kerihuel 2007

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

#ifndef __OPENCHANGECLIENT_H__
#define	__OPENCHANGECLIENT_H__

struct oc_property {
	struct oc_property	*prev;
	struct oc_property	*next;
	uint32_t		ulPropTag;
	const void		*data;
	bool			named;
};

struct ocpf_file {
	struct ocpf_file	*prev;
	struct ocpf_file	*next;
	const char		*filename;
};

struct attach {
	const char		*filename;
	struct Binary_r		bin;
	int			fd;
};

struct oclient {
	struct oc_property	*props;
	const char     		*update;
	const char		*delete;
	const char		*subject;
	const char		*pr_body;
	const char		*pr_html_inline;
	char			**usernames;
	char			**mapi_to;
	char			**mapi_cc;
	char			**mapi_bcc;
	struct Binary_r		pr_html;
	struct attach		*attach;
	uint32_t		attach_num;
	const char		*store_folder;
	const char		*location;
	const char		*dtstart;
	const char		*dtend;
	int			busystatus;
	int			taskstatus;
	int			label;
	bool			private;
	int			importance;
	int			color;
	int			width;
	int			height;
	const char		*email;
	const char		*full_name;
	const char		*card_name;
	const char		*folder_name;
	const char		*folder_comment;
	const char		*freebusy;
	bool			force;
	/* PF related options */
	bool			pf;
	const char		*folder;
	/* OCPF related options */
	struct ocpf_file	*ocpf_files;
	const char		*ocpf_dump;
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

struct oc_element	oc_importance[] = {
	{IMPORTANCE_LOW,	"LOW"},
	{IMPORTANCE_NORMAL,	"NORMAL"},
	{IMPORTANCE_HIGH,	"HIGH"},
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

struct oc_element	oc_label[] = {
	{0,			"NONE"},
	{1,			"IMPORTANT"},
	{2,			"BUSINESS"},
	{3,			"PERSONAL"},
	{4,			"VACATION"},
	{5,			"MUST_ATTEND"},
	{6,			"TRAVEL_REQUIRED"},
	{7,			"NEEDS_PREPARATION"},
	{8,			"BIRTHDAY"},
	{9,			"ANNIVERSARY"},
	{10,			"PHONE_CALL"},
	{0, NULL}
};

struct oc_element	oc_color[] = {
	{olBlue,		"Blue"},
	{olGreen,		"Green"},
	{olPink,		"Pink"},
	{olYellow,		"Yellow"},
	{olWhite,		"White"},
	{0, NULL}
};

#define	DATE_FORMAT	"%Y-%m-%d %H:%M:%S"

#endif /* !__OPENCHANGECLIENT_H__ */
