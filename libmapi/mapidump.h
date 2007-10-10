/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007.

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

#ifndef __MAPIDUMP_H__
#define	__MAPIDUMP_H__

struct mdump_msgflags {
	uint16_t	flag;
	const char	*value;
};

struct mdump_msgflags mdump_msgflags[] = {
	{0x1,	"MSGFLAG_READ"},
	{0x2,	"MSGFLAG_UNMODIFIED"},
	{0x4,	"MSGFLAG_SUBMIT"},
	{0x8,	"MSGFLAG_UNSENT"},
	{0x10,	"MSGFLAG_HASATTACH"},
	{0x20,	"MSGFLAG_FROMME"},
	{0x40,	"MSGFLAG_ASSOCIATED"},
	{0x80,	"MSGFLAG_RESEND"},
	{0x100,	"MSGFLAG_RN_PENDING"},
	{0x200,	"MSGFLAG_NRN_PENDING"},
	{0, NULL}
};

#endif	/* __MAPIDUMP_H__ */
