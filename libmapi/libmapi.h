/*
   OpenChange MAPI implementation.
   libmapi public header file

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

#ifndef __LIBMAPI_H__
#define __LIBMAPI_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

/* Samba4 includes */
#include <talloc.h>
#include <dcerpc.h>
#include <util/debug.h>
#include <tevent.h>
#include <param.h>

/* OpenChange includes */
#include <gen_ndr/exchange.h>
#include <gen_ndr/property.h>

#include <libmapi/dlinklist.h>
#include <libmapi/version.h>
#include <libmapi/nspi.h>
#include <libmapi/emsmdb.h>
#include <libmapi/mapi_ctx.h>
#include <libmapi/mapi_provider.h>
#include <libmapi/mapi_object.h>
#include <libmapi/mapi_id_array.h>
#include <libmapi/mapi_notification.h>
#include <libmapi/mapi_profile.h>
#include <libmapi/mapi_nameid.h>
#include <libmapi/mapidefs.h>
#include <libmapi/mapicode.h>
#include <libmapi/socket/netif.h>
#include <libmapi/proto.h>

extern struct mapi_ctx *global_mapi_ctx;

#endif /* __LIBMAPI_H__ */
