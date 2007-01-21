/*
 *  OpenChange MAPI implementation.
 *  Status codes returned my MAPI
 *
 *  Copyright (C) Julien Kerihuel 2005.
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

#ifndef	_MAPI_H
#define	_MAPI_H

/*
  MAPI opnums
 */

#define	OPNUM_Unknown		"Unknown opnum"
#define	OPNUM_LEN		1

#define	OPNUM_MAPI_RPC_LOGON	0xFE
#define	OPNUM_MAPI_SETCOLUMNS	0x12
#define	OPNUM_MAPI_QUERYROWS	0x15

struct	opnums {
	uint8_t		opnum;
	const char	*name;
};

#endif /*!_MAPI_H */
