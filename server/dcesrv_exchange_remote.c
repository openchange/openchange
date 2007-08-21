/*
   Exchange interface registration module

   OpenChange Project

   Copyright (C) Julien Kerihuel 2006

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
  This module registers the exchange interfaces prior loading the
  dcesrv_remote endpoint.
 */

#include <libmapi/libmapi.h>
#include <dcerpc_server.h>
#include <core/nterr.h>
#include <gen_ndr/ndr_exchange.h>
#include "server/dcesrv_proto.h"

NTSTATUS librpc_register_interface(const struct ndr_interface_table *interface);

NTSTATUS init_module(void)
{
	NTSTATUS status;

	status = librpc_register_interface(&ndr_table_exchange_nsp);
	if (NT_STATUS_IS_ERR(status)) return status;

	status = librpc_register_interface(&ndr_table_exchange_emsmdb);
	if (NT_STATUS_IS_ERR(status)) return status;

	return NT_STATUS_OK;
}
