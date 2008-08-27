/*
   MAPI Proxy

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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

#ifndef	__DCESRV_MAPIPROXY_H__
#define	__DCESRV_MAPIPROXY_H__

#define	_GNU_SOURCE 1

#include <sys/types.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <talloc.h>
#include <dcerpc.h>
#include <samba/session.h>

#include <dcerpc_server.h>
#include <util.h>
#include <param.h>
#include <credentials.h>

#include <gen_ndr/exchange.h>
#include <gen_ndr/ndr_exchange.h>
#include <mapiproxy/libmapiproxy.h>

struct dcesrv_mapiproxy_private {
	struct dcerpc_pipe	*c_pipe;
	char			*exchname;
};

#define MAXHOSTNAMELEN	255
#define	SERVERNAME      "/cn=Servers/cn="

#endif	/* !__DCESRV_MAPIPROXY_H__ */
