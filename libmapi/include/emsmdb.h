/*
 *  OpenChange EMSMDB implementation.
 *
 *  Copyright (C) Jelmer Vernooij 2005.
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

#ifndef __EMSMDB_H__
#define	__EMSMDB_H__

struct emsmdb_context {
  struct dcerpc_pipe		*rpc_connection;
  struct policy_handle		handle;
  TALLOC_CTX			*mem_ctx;
  struct EcDoRpc_MAPI_REQ	**cache_requests;
  uint32_t			cache_size;
  uint8_t			cache_count;
  uint16_t			prop_count;
  enum MAPITAGS			*properties;
};

#endif /* __EMSMDB_H__ */
