/*
   Stand-alone MAPI testsuite

   OpenChange Project - BULK DATA TRANSFER PROTOCOL operations

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
   \file module_oxcfxics.c

   \brief Bulk Data Transfer Protocol test suite
*/

/**
   \details Test the GetLocalReplicaIds (0x7f) operation

   This function:
   -# Log on private message store
   -# Reserve a range of Ids
 */
_PUBLIC_ bool mapitest_oxcfxics_GetLocalReplicaIds(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	struct GUID		ReplGuid;
	uint8_t			GlobalCount[6];
	char			*guid;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Reserve a range of IDs */
	retval = GetLocalReplicaIds(&obj_store, 0x1000, &ReplGuid, GlobalCount);
	mapitest_print_retval(mt, "GetLocalReplicaIds");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}
	guid = GUID_string(mt->mem_ctx, &ReplGuid);
	mapitest_print(mt, "* %-35s: %s\n", "ReplGuid", guid);
	mapitest_print(mt, "* %-35s: %x %x %x %x %x %x\n", "GlobalCount", GlobalCount[0],
		       GlobalCount[1], GlobalCount[2], GlobalCount[3], GlobalCount[4],
		       GlobalCount[5]);
	talloc_free(guid);

	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the FastTransferDestinationConfigure (0x53) and TellVersion (0x86) operations

   This function:
   -# Log on private message store
   -# Creates a test folder
   -# Setup destination
   -# Sends the "server version"
 */
_PUBLIC_ bool mapitest_oxcfxics_DestConfigure(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct mt_common_tf_ctx	*context;
	mapi_object_t		obj_htable;
	mapi_object_t		destfolder;
	uint16_t		version[3];
	bool			ret = true;

	/* Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	context = mt->priv;

	/* Create destfolders */
	mapi_object_init(&destfolder);
	retval = CreateFolder(&(context->obj_test_folder), FOLDER_GENERIC,
			      "DestFolder", NULL /*folder comment*/,
			      OPEN_IF_EXISTS, &destfolder);
	mapitest_print_retval_clean(mt, "Create DestFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = FXDestConfigure(&(context->obj_test_folder), FastTransferDest_CopyTo);
	mapitest_print_retval_clean(mt, "FXDestConfigure", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Send server version */
	version[0] = 3;
	version[1] = 1;
	version[2] = 4;
	retval = TellVersion(&(context->obj_test_folder), version);
	mapitest_print_retval_clean(mt, "TellVersion", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

cleanup:
	/* Cleanup and release */
	mapi_object_release(&destfolder);
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return ret;
}
