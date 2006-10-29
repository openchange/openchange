/*
   MAPI Implementation: Containers

   OpenChange Project

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
#include "ndr_exchange.h"
#include "libmapi/include/proto.h"

NTSTATUS GetHierarchyTable(uint32_t flags, struct SRowSet **RowSet)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_context	*ldb_ctx;
	struct ldb_dn		*container_dn;
	struct ldb_result	*container_tree;

	mem_ctx = talloc_init("GetHierarchyTable");

	ldb_ctx = storedb_connect(mem_ctx);
	
	if (!ldb_ctx) {
	  DEBUG(0, ("connection to the store database failed\n"));
	  return NT_STATUS(MAPI_E_CORRUPT_STORE);
	}

	container_dn = storedb_search_dn(ldb_ctx, mem_ctx, NULL,
					 "(&(objectClass=container)(containerID=0x0))");

	if (!container_dn) {
	  DEBUG(0, ("GetHierarchyInfo: Root container not found in storedb, returning MAPI_E_CORRUPT_STORE\n"));
	  return NT_STATUS(MAPI_E_CORRUPT_STORE);
	}

	*RowSet = storedb_search_container_tree(ldb_ctx, mem_ctx, container_dn, &container_tree,
						"(&(objectClass=container))");

	return NT_STATUS(MAPI_E_SUCCESS);
}
