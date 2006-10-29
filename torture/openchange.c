/* 
   Unix EMSMDB implementation

   test suite for openchange

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

#include "openchange.h"
#include <torture.h>

BOOL torture_rpc_emsmdb(struct torture_context *);
BOOL torture_rpc_exchange(struct torture_context *);
BOOL torture_rpc_nspi(struct torture_context *);
BOOL torture_rpc_nspi_profile(struct torture_context *);
BOOL torture_rpc_scantags(struct torture_context *);

NTSTATUS init_module(void)
{
	DEBUG(0, ("Loading openchange torture test\n"));
	register_torture_op("RPC-EMSMDB", torture_rpc_emsmdb);
	register_torture_op("RPC-EXCHANGE", torture_rpc_exchange);
	register_torture_op("RPC-NSPI", torture_rpc_nspi);
	register_torture_op("RPC-NSPI-PROFILE", torture_rpc_nspi_profile);
	register_torture_op("RPC-NSPI-SCANTAGS", torture_rpc_scantags);

	return NT_STATUS_OK;
}
