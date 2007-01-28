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
#include <torture/torture.h>

BOOL torture_rpc_mapi_fetchmail(struct torture_context *);
BOOL torture_rpc_emsmdb(struct torture_context *);
BOOL torture_rpc_exchange(struct torture_context *);
BOOL torture_rpc_nspi(struct torture_context *);
BOOL torture_rpc_nspi_profile(struct torture_context *);
BOOL torture_rpc_scantags(struct torture_context *);

NTSTATUS init_module(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "OPENCHANGE");

	DEBUG(0, ("Loading openchange torture test\n"));

	/* OpenChange torture tests */
	torture_suite_add_simple_test(suite, "EMSMDB", torture_rpc_emsmdb);
	torture_suite_add_simple_test(suite, "EXCHANGE", torture_rpc_exchange);
	torture_suite_add_simple_test(suite, "NSPI", torture_rpc_nspi);
	torture_suite_add_simple_test(suite, "NSPI-PROFILE", torture_rpc_nspi_profile);
	torture_suite_add_simple_test(suite, "NSPI-SCANTAGS", torture_rpc_scantags);
	/* MAPI torture tests */
	torture_suite_add_simple_test(suite, "MAPI-FETCHMAIL", torture_rpc_mapi_fetchmail);

	suite->description = talloc_strdup(suite, "Exchange protocols tests (NSPI and EMSMDB)");

	torture_register_suite(suite);
	return NT_STATUS_OK;
}
