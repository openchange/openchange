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
};

#define	DEFAULT_PROFDB	"%s/.openchange/profiles.ldb"

#endif /* !__OPENCHANGECLIENT_H__ */
