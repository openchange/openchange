/*
   Public Folders Administration Tool

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

#ifndef __OPENCHANGEPFADMIN_H__
#define	__OPENCHANGEPFADMIN_H__

#define	DEFAULT_PROFDB	"%s/.openchange/profiles.ldb"

struct aclrights {
	const char	*name;
	uint32_t	value;
};

const struct aclrights	aclrights[] = {
	{"RightsNone",			0x00000000},
        {"RightsReadItems",		0x00000001},
        {"RightsCreateItems",		0x00000002},
        {"RightsEditOwn",		0x00000008},
        {"RightsDeleteOwn",		0x00000010},
        {"RightsEditAll",		0x00000020},
        {"RightsDeleteAll",		0x00000040},
        {"RightsCreateSubfolders",	0x00000080},
        {"RightsFolderOwner",		0x00000100},
        {"RightsFolderContact",		0x00000200},
        {"RoleNone",			0x00000400},
        {"RoleReviewer",		0x00000401},
        {"RoleContributor",		0x00000402},
        {"RoleNoneditingAuthor",	0x00000413},
        {"RoleAuthor",			0x0000041B},
        {"RoleEditor",			0x0000047B},
        {"RolePublishAuthor",		0x0000049B},
        {"RolePublishEditor",		0x000004FB},
        {"RightsAll",			0x000005FB},
        {"RoleOwner",			0x000007FB},
	{NULL, 0}
};

const char * IPF_class[] = { IPF_APPOINTMENT, 
			     IPF_CONTACT, 
			     IPF_JOURNAL, 
			     IPF_NOTE, 
			     IPF_STICKYNOTE, 
			     IPF_TASK, 
			     IPF_POST, 
			     NULL};

#endif /* __OPENCHANGEPFADMIN_H__ */
