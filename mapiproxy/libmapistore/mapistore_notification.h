/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2015

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

#ifndef	MAPISTORE_NOTIFICATION_H
#define	MAPISTORE_NOTIFICATION_H

#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/gen_ndr/ndr_mapistore_notification.h"

#define	MSTORE_MEMC_DFLT_HOST	"127.0.0.1"

/* Define format strings used for key storage in memcached */
#define	MSTORE_MEMC_FMT_SESSION	"session:%s"
#define	MSTORE_MEMC_FMT_RESOLVER "resolver:%s"
#define	MSTORE_MEMC_FMT_SUBSCRIPTION "subscription:%s"
#define	MSTORE_MEMC_FMT_DELIVER "deliver:%s"

#endif /* MAPISTORE_NOTIFICATION_H */
