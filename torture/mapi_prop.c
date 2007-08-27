/*
   OpenChange MAPI implementation testsuite

   Properties related operations torture

   Copyright (C) Julien Kerihuel 2007
   Copyright (C) Fabien Le Mentec 2007
   
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


#include <libmapi/libmapi.h>
#include <gen_ndr/ndr_exchange.h>
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture/torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>
#include <string.h>
#include "oc_test.h"


static bool torture_prop(mapi_object_t *obj_prop)
{
	enum MAPISTATUS	status;
	TALLOC_CTX	*mem;
	struct SPropTagArray *proptags;
	struct SPropValue vals[10];
	struct SPropValue *propvals;
	uint32_t	cn_vals;
	int		cmp;

	mem = talloc_init("local");

	/* GetPropList
	 */
	oc_test_describe("GetPropList");
	proptags = talloc(mem, struct SPropTagArray);
	status = GetPropList(obj_prop, proptags);
	oc_test_assert(status == MAPI_E_SUCCESS);
	oc_test_assert(proptags->cValues != 0);


	/* SetProps
	 */
	oc_test_describe("SetProps");
	vals[0].value.lpszA = "torture_new_name";
	vals[0].ulPropTag = PR_DISPLAY_NAME;
	vals[1].value.lpszA = "torture_new_comment";
	vals[1].ulPropTag = PR_COMMENT;
	cn_vals = 2;
	status = SetProps(obj_prop, vals, cn_vals);
	oc_test_assert(status == MAPI_E_SUCCESS);

	proptags = set_SPropTagArray(mem, 2, PR_DISPLAY_NAME, PR_COMMENT);
	status = GetProps(obj_prop, proptags, &propvals, &cn_vals);
	oc_test_assert(status == MAPI_E_SUCCESS);
	oc_test_assert(cn_vals == 2);

	cmp = (strcmp(propvals[0].value.lpszA, "torture_new_name") ||
	       strcmp(propvals[1].value.lpszA, "torture_new_comment"));
	oc_test_assert(cmp == 0);


	/* DeleteProps
	 */
	oc_test_describe("DeleteProps");
	proptags = set_SPropTagArray(mem, 1, PR_COMMENT);
	status = DeleteProps(obj_prop, proptags);
	oc_test_assert(status == MAPI_E_SUCCESS);

	proptags = set_SPropTagArray(mem, 1, PR_COMMENT);
	status = GetProps(obj_prop, proptags, &propvals, &cn_vals);
	oc_test_assert(status == MAPI_E_SUCCESS || status == MAPI_W_ERRORS_RETURNED);

	return true;
}


bool torture_rpc_mapi_prop(struct torture_context *torture)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_inbox;
	mapi_object_t		obj_child;
	mapi_id_t		id_inbox;
	struct mapi_session	*session;


	/* init torture */
	mem_ctx = talloc_init("torture_rpc_mapi_prop");

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx)) == NULL) return false;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_inbox);
	mapi_object_init(&obj_child);

	/* session::OpenMsgStore() */
	retval = OpenMsgStore(&obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	mapi_object_debug(&obj_store);

	/* id_inbox = store->GeInboxFolder() */
	retval = GetReceiveFolder(&obj_store, &id_inbox);
	mapi_errstr("GetReceiveFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* inbox = store->OpenFolder(id_inbox) */
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	mapi_object_debug(&obj_inbox);

	/* child = inbox->CreateFolder() */
	retval = CreateFolder(&obj_inbox, "torture_name", "torture_comment", &obj_child);
	mapi_errstr("CreateFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	mapi_object_debug(&obj_child);

	/* child->torture() */
	oc_test_begin();
	ret = torture_prop(&obj_child);
	oc_test_end();

	/* inbox->DeleteFolder() */
	retval = DeleteFolder(&obj_inbox, mapi_object_get_id(&obj_child));
	mapi_errstr("DeleteFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* objects->Release()
	 */
	mapi_object_release(&obj_child);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);

	/* uninitialize mapi
	 */
	MAPIUninitialize();
	talloc_free(mem_ctx);

	return ret;
}
