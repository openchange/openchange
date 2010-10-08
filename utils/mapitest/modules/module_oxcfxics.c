/*
   Stand-alone MAPI testsuite

   OpenChange Project - BULK DATA TRANSFER PROTOCOL operations

   Copyright (C) Julien Kerihuel 2008
   Copyright (C) Brad Hards 2010

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
	mapi_object_t		obj_context;
	mapi_object_t		destfolder;
	uint16_t		version[3];
	bool			ret = true;

	/* Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	context = mt->priv;

	/* Create destfolder */
	mapi_object_init(&destfolder);
	mapi_object_init(&obj_context);
	retval = CreateFolder(&(context->obj_test_folder), FOLDER_GENERIC,
			      "DestFolder", NULL /*folder comment*/,
			      OPEN_IF_EXISTS, &destfolder);
	mapitest_print_retval_clean(mt, "Create DestFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = FXDestConfigure(&(context->obj_test_folder), FastTransferDest_CopyTo, &obj_context);
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
	mapi_object_release(&obj_context);
	mapi_object_release(&destfolder);
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return ret;
}

/**
   \details Test the FastTransferCopyFolder (0x4C), FastTransferGetBuffer (0x4E) and TellVersion (0x86) operations

   This function:
   -# Log on private message store
   -# Creates a test folder
   -# Setup source
   -# Sends the "server version"
 */
_PUBLIC_ bool mapitest_oxcfxics_CopyFolder(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct mt_common_tf_ctx	*context;
	mapi_object_t		obj_htable;
	mapi_object_t		obj_context;
	mapi_object_t		sourcefolder;
	uint16_t		version[3];
	bool			ret = true;
	enum TransferStatus	transferStatus;
	uint16_t		progress;
	uint16_t		totalSteps;
	DATA_BLOB		transferdata;

	/* Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	context = mt->priv;

	/* Create source folder */
	mapi_object_init(&sourcefolder);
	mapi_object_init(&obj_context);
	retval = CreateFolder(&(context->obj_test_folder), FOLDER_GENERIC,
			      "SourceCopyFolder", NULL /*folder comment*/,
			      OPEN_IF_EXISTS, &sourcefolder);
	mapitest_print_retval_clean(mt, "Create SourceCopyFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = FXCopyFolder(&(context->obj_test_folder), FastTransferCopyFolder_CopySubfolders, FastTransfer_Unicode, &obj_context);
	mapitest_print_retval_clean(mt, "FXCopyFolder", retval);
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

	retval = FXGetBuffer(&obj_context, 0, &transferStatus, &progress, &totalSteps, &transferdata);
	mapitest_print_retval_clean(mt, "FXGetBuffer", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	if (transferStatus != TransferStatus_Done) {
		mapitest_print(mt, "unexpected transferStatus 0x%04x\n", transferStatus);
		ret = false;
		goto cleanup;
	}
	if (progress != totalSteps) {
		mapitest_print(mt, "unexpected final step count, progress 0x%04x, total 0x%04x\n", progress, totalSteps);
		ret = false;
		goto cleanup;
	}
	// TODO: verify that the buffer is as expected
cleanup:
	/* Cleanup and release */
	mapi_object_release(&obj_context);
	mapi_object_release(&sourcefolder);
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return ret;
}

/**
   \details Test the FastTransferCopyMessages (0x4B) and FastTransferGetBuffer (0x4E) operations

   This function:
   -# Log on private message store
   -# Creates a test folder
   -# Setup source
   -# Get data
 */
_PUBLIC_ bool mapitest_oxcfxics_CopyMessages(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct mt_common_tf_ctx	*context;
	mapi_object_t		obj_htable;
	mapi_object_t		obj_context;
	mapi_object_t		sourcefolder;
	mapi_id_array_t		mids;
	int			i;
	bool			ret = true;
	enum TransferStatus	transferStatus;
	uint16_t		progress;
	uint16_t		totalSteps;
	DATA_BLOB		transferdata;

	/* Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	context = mt->priv;

	/* Create source folder */
	mapi_object_init(&sourcefolder);
	mapi_object_init(&obj_context);

	retval = CreateFolder(&(context->obj_test_folder), FOLDER_GENERIC,
			      "SourceCopyMessages", NULL /*folder comment*/,
			      OPEN_IF_EXISTS, &sourcefolder);
	mapitest_print_retval_clean(mt, "Create SourceCopyMessages", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = mapi_id_array_init(&mids);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval_clean(mt, "mapi_id_array_init", retval);
		ret = false;
		goto cleanup;
	}
	for (i = 0; i < 5; ++i) {
		retval = mapi_id_array_add_obj(&mids, &(context->obj_test_msg[i]));
		if (retval != MAPI_E_SUCCESS) {
			mapitest_print_retval_clean(mt, "mapi_id_array_add_obj", retval);
			ret = false;
			goto cleanup;
		}
	}

	retval = FXCopyMessages(&(context->obj_test_folder), &mids, FastTransferCopyMessage_BestBody, FastTransfer_Unicode, &obj_context);
	mapitest_print_retval_clean(mt, "FXCopyMessages", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = FXGetBuffer(&obj_context, 0, &transferStatus, &progress, &totalSteps, &transferdata);
	mapitest_print_retval_clean(mt, "FXGetBuffer", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	if (transferStatus != TransferStatus_Done) {
		mapitest_print(mt, "unexpected transferStatus 0x%04x\n", transferStatus);
		ret = false;
		goto cleanup;
	}
	if (progress != totalSteps) {
		mapitest_print(mt, "unexpected final step count, progress 0x%04x, total 0x%04x\n", progress, totalSteps);
		ret = false;
		goto cleanup;
	}
	// TODO: verify that the buffer is as expected
cleanup:
	/* Cleanup and release */
	mapi_object_release(&obj_context);
	mapi_object_release(&sourcefolder);
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return ret;
}

/**
   \details Test the FastTransferCopyTo (0x4D) and FastTransferGetBuffer (0x4E) operations

   This function:
   -# Log on private message store
   -# Creates a test folder
   -# Setup source
   -# Get data
 */
_PUBLIC_ bool mapitest_oxcfxics_CopyTo(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct mt_common_tf_ctx	*context;
	mapi_object_t		obj_htable;
	mapi_object_t		obj_context;
	mapi_object_t		sourcefolder;
	mapi_id_array_t		mids;
	struct SPropTagArray	*propsToExclude;
	int			i;
	bool			ret = true;
	enum TransferStatus	transferStatus;
	uint16_t		progress;
	uint16_t		totalSteps;
	DATA_BLOB		transferdata;

	/* Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	context = mt->priv;

	/* Create source folder */
	mapi_object_init(&sourcefolder);
	mapi_object_init(&obj_context);

	retval = CreateFolder(&(context->obj_test_folder), FOLDER_GENERIC,
			      "SourceCopyTo", NULL /*folder comment*/,
			      OPEN_IF_EXISTS, &sourcefolder);
	mapitest_print_retval_clean(mt, "Create SourceCopyTo", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = mapi_id_array_init(&mids);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval_clean(mt, "mapi_id_array_init", retval);
		ret = false;
		goto cleanup;
	}
	for (i = 0; i < 5; ++i) {
		retval = mapi_id_array_add_obj(&mids, &(context->obj_test_msg[i]));
		if (retval != MAPI_E_SUCCESS) {
			mapitest_print_retval_clean(mt, "mapi_id_array_add_obj", retval);
			ret = false;
			goto cleanup;
		}
	}

	propsToExclude = talloc_zero(mt->mem_ctx, struct SPropTagArray);
	retval = FXCopyTo(&(context->obj_test_folder), 0, FastTransferCopyTo_BestBody, FastTransfer_Unicode, propsToExclude, &obj_context);
	mapitest_print_retval_clean(mt, "FXCopyTo", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = FXGetBuffer(&obj_context, 0, &transferStatus, &progress, &totalSteps, &transferdata);
	mapitest_print_retval_clean(mt, "FXGetBuffer", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	if (transferStatus != TransferStatus_Done) {
		mapitest_print(mt, "unexpected transferStatus 0x%04x\n", transferStatus);
		ret = false;
		goto cleanup;
	}
	if (progress != totalSteps) {
		mapitest_print(mt, "unexpected final step count, progress 0x%04x, total 0x%04x\n", progress, totalSteps);
		ret = false;
		goto cleanup;
	}
	// TODO: verify that the buffer is as expected
cleanup:
	/* Cleanup and release */
	mapi_object_release(&obj_context);
	mapi_object_release(&sourcefolder);
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return ret;
}

/**
   \details Test the FastTransferCopyProperties (0x69) and FastTransferSourceGetBuffer (0x4e) operations

   This function:
   -# Log on private message store
   -# Creates a test folder
   -# Setup source
   -# Get data
 */
_PUBLIC_ bool mapitest_oxcfxics_CopyProperties(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct mt_common_tf_ctx	*context;
	mapi_object_t		obj_htable;
	mapi_object_t		obj_context;
	mapi_object_t		sourcefolder;
	struct SPropTagArray	*props;
	bool			ret = true;
	enum TransferStatus	transferStatus;
	uint16_t		progress;
	uint16_t		totalSteps;
	DATA_BLOB		transferdata;

	/* Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	context = mt->priv;

	/* Create source folder */
	mapi_object_init(&sourcefolder);
	mapi_object_init(&obj_context);

	retval = CreateFolder(&(context->obj_test_folder), FOLDER_GENERIC,
			      "SourceCopyProperties", NULL /*folder comment*/,
			      OPEN_IF_EXISTS, &sourcefolder);
	mapitest_print_retval_clean(mt, "Create SourceCopyProperties", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	props = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = FXCopyProperties(&(context->obj_test_folder), 0 /* level */, 0 /*copyflags */, FastTransfer_Unicode, props, &obj_context);
	mapitest_print_retval_clean(mt, "FXCopyProperties", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	
	retval = FXGetBuffer(&obj_context, 0, &transferStatus, &progress, &totalSteps, &transferdata);
	mapitest_print_retval_clean(mt, "FXGetBuffer", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	if (transferStatus != TransferStatus_Done) {
		mapitest_print(mt, "unexpected transferStatus 0x%04x\n", transferStatus);
		ret = false;
		goto cleanup;
	}
	if (progress != totalSteps) {
		mapitest_print(mt, "unexpected final step count, progress 0x%04x, total 0x%04x\n", progress, totalSteps);
		ret = false;
		goto cleanup;
	}
	// TODO: verify that the buffer is as expected
cleanup:
	/* Cleanup and release */
	mapi_object_release(&obj_context);
	mapi_object_release(&sourcefolder);
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return ret;
}
