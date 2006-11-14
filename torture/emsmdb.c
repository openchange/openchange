/* 
   Unix EMSMDB implementation

   test suite for emsmdb rpc operations

   Copyright (C) Julien Kerihuel 2005
   
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
#include "ndr_exchange.h"
#include "ndr_exchange_c.h"
#include "libmapi/mapicode.h"
#include "libmapi/include/proto.h"

/* FIXME: Should be part of Samba's data: */
NTSTATUS torture_rpc_connection(TALLOC_CTX *parent_ctx, 
				struct dcerpc_pipe **p, 
				const struct dcerpc_interface_table *table);


#define TEST_NAME	"/o=OpenChange Organization/ou=First Administrative Group/cn=Recipients/cn=Administrator"
#define TEST_NAME_0	""
#define TEST_NAME_1	"/o=fo Organization/ou=First Administrative Group/cn=Recipients/cn=Administrator"
#define TEST_NAME_2	"/o=OpenChange Organization/ou=Third Administrative Group/cn=Recipients/cn=Administrator"
#define TEST_NAME_3	"/o=OpenChange Organization/ou=First Administrative Group/cn=Recipients_foooo/cn=Administrator"
#define TEST_NAME_4	"/o=OpenChange Organization/ou=First Administrative Group/cn=Recipients/cn="
#define TEST_NAME_5	"/o=OpenChange Organization/ou=First Administrative Group/cn=Recipients/cn=foobar"


BOOL test_EcDoConnect(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	struct EcDoConnect	r;
	NTSTATUS		status;
	BOOL			ret = True;
	int			n = 0;
	struct ndr_print       	*ndr_print;

	for (n = 0; n < 0xFFFF; n++)
	{
		r.in.name = TEST_NAME;
		r.in.unknown1[0] = 0x0;
		r.in.unknown1[1] = 0xf99dfde6;
		r.in.unknown1[2] = 0x0;

		r.in.code_page = 0x4e4;
		r.in.input_locale.language = 0x40c;
		r.in.input_locale.method = 0x409;

		r.in.unknown2 = 0xffffffff;
		r.in.unknown3 = 0x1;

		r.in.emsmdb_client_version[0] = 0xa;
		r.in.emsmdb_client_version[1] = 0x0;
		r.in.emsmdb_client_version[2] = 0x1013;
		*r.in.alloc_space = 0x0163000;

		r.out.unknown4[0] = talloc(mem_ctx, uint32_t);
		r.out.unknown4[1] = talloc(mem_ctx, uint32_t);
		r.out.unknown4[2] = talloc(mem_ctx, uint32_t);
		r.out.session_nb = talloc(mem_ctx, uint16_t);
		r.out.handle = talloc(mem_ctx, struct policy_handle);

		status = dcerpc_EcDoConnect(p, mem_ctx, &r);

		if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
			mapi_errstr("EcDoConnect", r.out.result);
			ret = False;
			return ret;
		}
		else {
		  	ndr_print = talloc(mem_ctx, struct ndr_print);
		  	ndr_print->depth = 1;
		  	ndr_print->print = ndr_print_debug_helper;
		  	ndr_print_EcDoConnect(ndr_print, "EcDoConnect", NDR_OUT, &r);
			
			ret = True;
		}
}
	return ret;
}

BOOL torture_rpc_emsmdb(struct torture_context *torture)
{
	NTSTATUS		status;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	BOOL			ret = True;

	mem_ctx = talloc_init("torture_rpc_emsmdb");

	status = torture_rpc_connection(mem_ctx, &p, &dcerpc_table_exchange_emsmdb);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);

		return False;
	}

	ret &= test_EcDoConnect(p, mem_ctx);

	talloc_free(mem_ctx);

	return ret;
}
