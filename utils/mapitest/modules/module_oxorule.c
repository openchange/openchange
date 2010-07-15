/*
   Stand-alone MAPI testsuite

   OpenChange Project - E-MAIL RULES PROTOCOL operations

   Copyright (C) Julien Kerihuel 2008

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

#include "utils/mapitest/mapitest.h"
#include "utils/mapitest/proto.h"

/**
   \file module_oxorule.c

   \brief E-Mail Rules Protocol test suite
 */


/**
   \details Test the GetRulesTable (0x3f) operation

   This function:
	-# Log on the user private mailbox
	-# Open the inbox folder
	-# Retrieve the rules table
   
   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxorule_GetRulesTable(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_rtable;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_folder);
	mapi_object_init(&obj_rtable);

	retval = OpenMsgStore(mt->session, &obj_store);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 2. Open the Inbox folder */
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_folder, olFolderInbox);
	if (ret == false) {
		goto cleanup;
	}

	/* Step 3. Retrieve the rules table */
	retval = GetRulesTable(&obj_folder, &obj_rtable, RulesTableFlags_Unicode);
	mapitest_print_retval(mt, "GetRulesTable");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

 cleanup:
	/* Release */
	mapi_object_release(&obj_rtable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return ret;
}
