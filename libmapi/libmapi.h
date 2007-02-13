/*
 *  OpenChange MAPI implementation.
 *  libmapi public header file
 *
 *  Copyright (C) Julien Kerihuel 2007.
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

#ifndef __LIBMAPI_H__
#define __LIBMAPI_H__

#define _GNU_SOURCE 1

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

/* Samba4 includes */
#include <talloc.h>
#include <dcerpc.h>

/* OpenChange includes */
#include <gen_ndr/exchange.h>

#include <libmapi/nspi.h>
#include <libmapi/emsmdb.h>
#include <libmapi/mapidefs.h>
#include <libmapi/mapicode.h>
#include <libmapi/property.h>
#include <libmapi/proto.h>

#endif /* __LIBMAPI_H__ */
