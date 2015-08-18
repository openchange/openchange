/*
   Memcached util functions

   OpenChange Project

   Copyright (C) Jesús García Sáez 2015

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

#ifndef __OC_MEMCACHED_H__
#define __OC_MEMCACHED_H__

#include <libmemcached/memcached.h>

#define MSTORE_MEMC_DFLT_HOST "127.0.0.1"
#define MSTORE_MEMC_DFLT_PORT 11211


memcached_st *oc_memcached_new_connection(const char *config_string, bool shared);
void oc_memcached_release_connection(memcached_st *memc_ctx, bool shared);


#endif /* __OC_MEMCACHED_H_ */
