/*
** mapitables.h for OpenChange
** 
** Copyright (C) Gregory Schiro
** Mail   <g.schiro@openchange.org>
** 
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
** 
** Started on  Tue Apr 11 17:11:29 2006 root
** Last update Wed Apr 12 04:43:25 2006 root
*/

#ifndef _MAPITABLES_H_
# define _MAPITABLES_H_


struct GUID_users {
	uint8_t			guid[16];
	struct GUID_users	*next;
};


struct GUID_users	*GUID_user_list = NULL;

#endif /* !_MAPITABLES_H_ */

