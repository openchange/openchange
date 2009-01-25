/*
   OpenChange MAPI torture suite implementation.

   Test Restrictions

   Copyright (C) Julien Kerihuel 2007 - 2008.

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

#include <libmapi/libmapi.h>
#include <libmapi/defs_private.h>
#include <gen_ndr/ndr_exchange.h>
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>

/*
  1. Create a search folder within Top Information Store
  2. SetCriteria with RES_CONTENT PR_SUBJECT FL_SUBSTRING criteria
  3. QueryRows and check if it worked ;-)
 */

bool torture_rpc_mapi_criteria(struct torture_context *torture)
{
	NTSTATUS			ntstatus;
	enum MAPISTATUS			retval;
	struct dcerpc_pipe		*p;
	TALLOC_CTX			*mem_ctx;
	struct mapi_session		*session;
	mapi_object_t			obj_store;
	mapi_object_t			obj_searchdir;
	mapi_object_t			obj_search;
	mapi_id_t			id_search;
	mapi_id_t			id_tis;
	mapi_id_array_t			id;
	struct mapi_SRestriction	res;
	struct SPropValue		lpProps[1];
	uint32_t			ulSearchFlags;
	uint16_t			count;
	mapi_id_t			*fid;
	uint32_t			i;

	/* init torture */
	mem_ctx = talloc_named(NULL, 0, "torture_rpc_mapi_criteria");
	ntstatus = torture_rpc_connection(torture, &p, &ndr_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(ntstatus)) {
		talloc_free(mem_ctx);
		return false;
	}
	
	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx, torture->lp_ctx)) == NULL) return false;

	/* Open Message Store */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(session, &obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	
	/* Get Top Information Store ID */
	retval = GetDefaultFolder(&obj_store, &id_tis, olFolderTopInformationStore);
	mapi_errstr("GetDefaultFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Open Search folder */
	retval = GetDefaultFolder(&obj_store, &id_search, olFolderFinder);
	mapi_errstr("GetDefaultFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_init(&obj_search);
	retval = OpenFolder(&obj_store, id_search, &obj_search);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Create the Search folder */
	mapi_object_init(&obj_searchdir);
	retval = CreateFolder(&obj_search, FOLDER_SEARCH, "torture_search", 
			      "Torture Search Folder", OPEN_IF_EXISTS, 
			      &obj_searchdir);
	mapi_errstr("CreateFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Set Props */
	lpProps[0].ulPropTag = PR_CONTAINER_CLASS;
	lpProps[0].value.lpszA = "IPF.Note";
	retval = SetProps(&obj_searchdir, lpProps, 1);
	mapi_errstr("SetProps", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Set Search criteria on this folder */
	mapi_id_array_init(&id);
	mapi_id_array_add_id(&id, id_tis);

	res.rt = RES_CONTENT;
	res.res.resContent.fuzzy = FL_SUBSTRING;
	res.res.resContent.ulPropTag = PR_SUBJECT;
	res.res.resContent.lpProp.ulPropTag = PR_SUBJECT;
	res.res.resContent.lpProp.value.lpszA = "criteria";

	retval = SetSearchCriteria(&obj_searchdir, &res, 
				   BACKGROUND_SEARCH|RECURSIVE_SEARCH,
				   &id);
	mapi_errstr("SetSearchCriteria", GetLastError());
	mapi_id_array_release(&id);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Get Search criteria for this folder */
	retval = GetSearchCriteria(&obj_searchdir, &res, &ulSearchFlags, &count, &fid);
	mapi_errstr("GetSearchCriteria", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	printf("ulSearchFlags = 0x%x\n", ulSearchFlags);
	printf("res.rt = %d\n", res.rt);
	printf("count = %d\n", count);
	for (i = 0; i < count; i++) {
	  printf("lpContainerList[%d] = 0x%"PRIx64"\n", i, fid[i]);
	}

	/* Delete folder */
	retval = DeleteFolder(&obj_search, mapi_object_get_id(&obj_searchdir),
			      DEL_MESSAGES|DEL_FOLDERS|DELETE_HARD_DELETE, NULL);
	mapi_errstr("DeleteFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_release(&obj_searchdir);
	mapi_object_release(&obj_search);
	mapi_object_release(&obj_store);
	talloc_free(mem_ctx);

	return true;
}
