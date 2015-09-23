/*
   Stand-alone MAPI testsuite

   OpenChange Project - BULK DATA TRANSFER PROTOCOL operations

   Copyright (C) Julien Kerihuel 2008
   Copyright (C) Brad Hards 2010-2011
   Copyright (C) Enrique J. Hernández 2015

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
	mapitest_print(mt, "* %-35s: %02x %02x %02x %02x %02x %02x\n", "GlobalCount", GlobalCount[0],
		       GlobalCount[1], GlobalCount[2], GlobalCount[3], GlobalCount[4],
		       GlobalCount[5]);
	talloc_free(guid);

	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the FastTransferDestinationConfigure (0x53), TellVersion (0x86) and
   FastTransferDestinationPutBuffer (0x54) operations

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
	DATA_BLOB		put_buffer_data;
	uint16_t		usedSize;
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
	retval = TellVersion(&obj_context, mt->info.rgwServerVersion);
	mapitest_print_retval_clean(mt, "TellVersion", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* start top folder, followed by end folder */
	put_buffer_data = data_blob_named("\x03\x00\x09\x40\x03\x00\x0b\x40", 8, "putbuffer");
	retval = FXPutBuffer(&obj_context, &put_buffer_data, &usedSize);
	mapitest_print_retval_clean(mt, "FXPutBuffer", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	data_blob_free(&put_buffer_data);

	if (usedSize != 8) {
		mapitest_print(mt, "unexpected used count 0x%04x\n", usedSize);
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
	enum MAPISTATUS			retval;
	struct mt_common_tf_ctx		*context;
	mapi_object_t			obj_htable;
	mapi_object_t			obj_context;
	mapi_object_t			sourcefolder;
	bool				ret = true;
	enum TransferStatus		transferStatus;
	uint16_t			progress;
	uint16_t			totalSteps;
	DATA_BLOB			transferdata;
	struct fx_parser_context	*parser;

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
	retval = TellVersion(&obj_context, mt->info.rgwServerVersion);
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
	parser = fxparser_init(mt->mem_ctx, NULL);
	fxparser_parse(parser, &transferdata);
	talloc_free(parser);
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
   -# Cremapidump_fx_bufferates a test folder
   -# Setup source
   -# Get data
 */
_PUBLIC_ bool mapitest_oxcfxics_CopyMessages(struct mapitest *mt)
{
	enum MAPISTATUS			retval;
	struct mt_common_tf_ctx		*context;
	mapi_object_t			obj_htable;
	mapi_object_t			obj_context;
	mapi_object_t			sourcefolder;
	mapi_id_array_t			mids;
	int				i;
	bool				ret = true;
	enum TransferStatus		transferStatus;
	uint16_t			progress;
	uint16_t			totalSteps;
	DATA_BLOB			transferdata;
	struct fx_parser_context	*parser;

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

	retval = mapi_id_array_init(mt->mapi_ctx->mem_ctx, &mids);
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
	parser = fxparser_init(mt->mem_ctx, NULL);
	fxparser_parse(parser, &transferdata);
	talloc_free(parser);
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
	enum MAPISTATUS			retval;
	struct mt_common_tf_ctx		*context;
	mapi_object_t			obj_htable;
	mapi_object_t			obj_context;
	mapi_object_t			sourcefolder;
	mapi_id_array_t			mids;
	struct SPropTagArray		*propsToExclude;
	int				i;
	bool				ret = true;
	enum TransferStatus		transferStatus;
	uint16_t			progress;
	uint16_t			totalSteps;
	DATA_BLOB			transferdata;
	struct fx_parser_context	*parser;

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

	retval = mapi_id_array_init(mt->mapi_ctx->mem_ctx, &mids);
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
	parser = fxparser_init(mt->mem_ctx, NULL);
	fxparser_parse(parser, &transferdata);
	talloc_free(parser);
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
	enum MAPISTATUS			retval;
	struct mt_common_tf_ctx		*context;
	mapi_object_t			obj_htable;
	mapi_object_t			obj_context;
	mapi_object_t			sourcefolder;
	struct SPropTagArray		*props;
	bool				ret = true;
	enum TransferStatus		transferStatus;
	uint16_t			progress;
	uint16_t			totalSteps;
	DATA_BLOB			transferdata;
	struct fx_parser_context	*parser;
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

	props = set_SPropTagArray(mt->mem_ctx, 0x3, PR_DISPLAY_NAME, PR_FOLDER_TYPE, PidTagLastModificationTime);
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
	parser = fxparser_init(mt->mem_ctx, NULL);
	fxparser_parse(parser, &transferdata);
	talloc_free(parser);
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
   \details Test the RopSynchronizationConfigure (0x70),
   RopSynchronizationUploadStateStreamBegin (0x75),
   RopSynchronizationUploadStateStreamContinue (0x76),
   RopSynchronizationUploadStateStreamEnd (0x77),
   RopFastTransferSourceGetBuffer (0x4e) and
   RopSynchronizationGetTransferState (0x82) operations
   for hierarchy synchronisation.

   This function:
   -# Log on private message store
   -# Create a test folder
   -# Set up sync configure context for downloading changes in hierarchy
   -# Upload an empty ICS state
   -# Download current state
   -# Clean up.
 */
_PUBLIC_ bool mapitest_oxcfxics_SyncConfigureHierarchy(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct mt_common_tf_ctx	*context;
	mapi_object_t		obj_htable;
	mapi_object_t		obj_sync_context;
	mapi_object_t		download_folder;
	mapi_object_t		obj_transfer_state;
	DATA_BLOB		restriction;
	DATA_BLOB		ics_state;
	struct SPropTagArray	*property_tags;
	bool			ret = true;
	DATA_BLOB		transfer_data;
	struct fx_parser_context	*parser;
	enum TransferStatus	transfer_status;
	uint16_t		progress, total_steps;

	/* Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	context = mt->priv;

	/* Create destfolder */
	mapi_object_init(&download_folder);
	mapi_object_init(&obj_sync_context);
	mapi_object_init(&obj_transfer_state);
	retval = CreateFolder(&(context->obj_test_folder), FOLDER_GENERIC,
			      "ICSDownloadFolder", NULL /*folder comment*/,
			      OPEN_IF_EXISTS, &download_folder);
	mapitest_print_retval_clean(mt, "Create ICS Download Folder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	property_tags = set_SPropTagArray(mt->mem_ctx, 0x0);
	restriction.length = 0;
	restriction.data = NULL;
	retval = ICSSyncConfigure(&(context->obj_test_folder), Hierarchy,
				  FastTransfer_Unicode,
				  SynchronizationFlag_Unicode,
				  Eid | Cn, restriction, property_tags, &obj_sync_context);
	mapitest_print_retval_clean(mt, "ICSSyncConfigure", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	ics_state.length = 0;
	ics_state.data = NULL;

	retval = ICSSyncUploadStateBegin(&obj_sync_context, MetaTagIdsetGiven, ics_state.length);
	mapitest_print_retval_clean(mt, "ICSSyncUploadStateBegin", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = ICSSyncUploadStateContinue(&obj_sync_context, ics_state);
	mapitest_print_retval_clean(mt, "ICSSyncUploadStateContinue", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = ICSSyncUploadStateEnd(&obj_sync_context);
	mapitest_print_retval_clean(mt, "ICSSyncUploadStateEnd", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = FXGetBuffer(&obj_sync_context, 0, &transfer_status, &progress, &total_steps,
			     &transfer_data);
	mapitest_print_retval_clean(mt, "FXGetBuffer", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Check the transfer is done */
	mapitest_print_assert(mt, "Hierarchy changes download transfer done",
			      transfer_status == TransferStatus_Done);
	if (transfer_status != TransferStatus_Done) {
		ret = false;
	}

	parser = fxparser_init(mt->mem_ctx, NULL);
	retval = fxparser_parse(parser, &transfer_data);
	mapitest_print_retval_clean(mt, "FX buffer parse", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}
	talloc_free(parser);
	// TODO: verify that the buffer is as expected

	retval = ICSSyncGetTransferState(&obj_sync_context, &obj_transfer_state);
	mapitest_print_retval_clean(mt, "ICSSyncGetTransferState", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

cleanup:
	/* Cleanup and release */
	mapi_object_release(&obj_transfer_state);
	mapi_object_release(&obj_sync_context);
	mapi_object_release(&download_folder);
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return ret;
}

/**
   \details Test the RopSynchronizationConfigure (0x70),
   RopSynchronizationUploadStateStreamBegin (0x75),
   RopSynchronizationUploadStateStreamContinue (0x76),
   RopSynchronizationUploadStateStreamEnd (0x77),
   RopFastTransferSourceGetBuffer (0x4e) and
   RopSynchronizationGetTransferState (0x82) operations
   for content synchronisation.

   This function:
   -# Log on private message store
   -# Creates a test folder
   -# Sets up sync configure context
   -# Uploads an empty ICS state
   -# cleans up.
 */
_PUBLIC_ bool mapitest_oxcfxics_SyncConfigureContents(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct mt_common_tf_ctx	*context;
	mapi_object_t		obj_htable;
	mapi_object_t		obj_sync_context;
	mapi_object_t		download_folder;
	mapi_object_t		obj_transfer_state;
	DATA_BLOB		restriction;
	DATA_BLOB		ics_state;
	struct SPropTagArray	*property_tags;
	bool			ret = true;
	DATA_BLOB		transfer_data;
	struct fx_parser_context	*parser;
	enum TransferStatus	transfer_status;
	uint16_t		progress, total_steps;

	/* Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	context = mt->priv;

	/* Create destfolder */
	mapi_object_init(&download_folder);
	mapi_object_init(&obj_sync_context);
	mapi_object_init(&obj_transfer_state);
	retval = CreateFolder(&(context->obj_test_folder), FOLDER_GENERIC,
			      "ICSDownloadFolder", NULL /*folder comment*/,
			      OPEN_IF_EXISTS, &download_folder);
	mapitest_print_retval_clean(mt, "Create ICS Download Folder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	property_tags = set_SPropTagArray(mt->mem_ctx, 0x0);
	restriction.length = 0;
	restriction.data = NULL;
	retval = ICSSyncConfigure(&download_folder, Contents,
				  FastTransfer_Unicode,
				  SynchronizationFlag_Unicode | SynchronizationFlag_Normal | SynchronizationFlag_NoForeignIdentifiers | SynchronizationFlag_BestBody,
				  Eid | Cn | OrderByDeliveryTime,
				  restriction, property_tags, &obj_sync_context);
	mapitest_print_retval_clean(mt, "ICSSyncConfigure", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	ics_state.length = 0;
	ics_state.data = NULL;

	retval = ICSSyncUploadStateBegin(&obj_sync_context, MetaTagIdsetGiven, ics_state.length);
	mapitest_print_retval_clean(mt, "ICSSyncUploadStateBegin", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = ICSSyncUploadStateContinue(&obj_sync_context, ics_state);
	mapitest_print_retval_clean(mt, "ICSSyncUploadStateContinue", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = ICSSyncUploadStateEnd(&obj_sync_context);
	mapitest_print_retval_clean(mt, "ICSSyncUploadStateEnd", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = FXGetBuffer(&obj_sync_context, 0, &transfer_status, &progress, &total_steps,
			     &transfer_data);
	mapitest_print_retval_clean(mt, "FXGetBuffer", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Check the transfer is done */
	mapitest_print_assert(mt, "Content changes download transfer done",
			      transfer_status == TransferStatus_Done);
	if (transfer_status != TransferStatus_Done) {
		ret = false;
	}

	parser = fxparser_init(mt->mem_ctx, NULL);
	retval = fxparser_parse(parser, &transfer_data);
	mapitest_print_retval_clean(mt, "FX buffer parse", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}
	talloc_free(parser);
	// TODO: verify that the buffer is as expected

	retval = ICSSyncGetTransferState(&obj_sync_context, &obj_transfer_state);
	mapitest_print_retval_clean(mt, "ICSSyncGetTransferState", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

cleanup:
	/* Cleanup and release */
	mapi_object_release(&obj_transfer_state);
	mapi_object_release(&obj_sync_context);
	mapi_object_release(&download_folder);
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return ret;
}

/**
   \details Test the GetLocalReplicaId (0x7f) and SetLocalReplicaMidsetDeleted (0x93)
   operations

   This function:
   -# Log on private message store
   -# Creates a test folder
   -# Gets a local replica ID range
   -# Sets the local replica ID range as deleted (on the test folder)
   -# cleans up
 */
_PUBLIC_ bool mapitest_oxcfxics_SetLocalReplicaMidsetDeleted(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct mt_common_tf_ctx	*context;
	mapi_object_t		obj_htable;
	struct GUID		ReplGuid;
	uint8_t			GlobalCount[6];
	uint8_t			GlobalCountHigh[6];
	char			*guid;
	mapi_object_t		testfolder;
	bool			ret = true;

	/* Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	context = mt->priv;

	/* Create test folder */
	mapi_object_init(&testfolder);
	retval = CreateFolder(&(context->obj_test_folder), FOLDER_GENERIC,
			      "ReplicaTestFolder", NULL /*folder comment*/,
			      OPEN_IF_EXISTS, &testfolder);
	mapitest_print_retval_clean(mt, "Create ReplicaTestFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Get the local ID range */
	retval = GetLocalReplicaIds(&(context->obj_store), 0x101, &ReplGuid, GlobalCount);
	mapitest_print_retval_clean(mt, "GetLocalReplicaIds", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	guid = GUID_string(mt->mem_ctx, &ReplGuid);
	mapitest_print(mt, "* %-35s: %s\n", "ReplGuid", guid);
	mapitest_print(mt, "* %-35s: %02x %02x %02x %02x %02x %02x\n", "GlobalCount", GlobalCount[0],
		       GlobalCount[1], GlobalCount[2], GlobalCount[3], GlobalCount[4],
		       GlobalCount[5]);
	talloc_free(guid);
	
	/* copy the returned global count range, and increment it by the range we asked for, less 1,
	   since these ranges are inclusive */
	memcpy(GlobalCountHigh, GlobalCount, 6);
	GlobalCountHigh[4] += 1; /* 0x100 */
	
	/* delete the local ID range */
	retval = SetLocalReplicaMidsetDeleted(&testfolder, ReplGuid, GlobalCount, GlobalCountHigh);
	mapitest_print_retval_clean(mt, "SetLocalReplicaMidsetDeleted", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

cleanup:
	/* Cleanup and release */
	mapi_object_release(&testfolder);
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return ret;
}

/**
   \details Test the RopSynchronizationOpenCollector (0x7e),
   operation.

   This function:
   -# Log on private message store
   -# Creates a test folder
   -# Opens a sync collector context for content
   -# Opens a sync collector context for hierachy
   -# cleans up.
 */
_PUBLIC_ bool mapitest_oxcfxics_SyncOpenCollector(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct mt_common_tf_ctx	*context;
	mapi_object_t		obj_htable;
	mapi_object_t		obj_sync_collector;
	mapi_object_t		obj_sync_hierachy_collector;
	mapi_object_t		collector_folder;
	bool			ret = true;

	/* Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	context = mt->priv;

	/* Create destfolder */
	mapi_object_init(&collector_folder);
	mapi_object_init(&obj_sync_collector);
	mapi_object_init(&obj_sync_hierachy_collector);
	retval = CreateFolder(&(context->obj_test_folder), FOLDER_GENERIC,
			      "ICSSyncCollector", NULL /*folder comment*/,
			      OPEN_IF_EXISTS, &collector_folder);
	mapitest_print_retval_clean(mt, "Create ICS SyncCollector Folder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = ICSSyncOpenCollector(&collector_folder, true, &obj_sync_collector);
	mapitest_print_retval_clean(mt, "ICSSyncOpenCollector - Contents", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = ICSSyncOpenCollector(&collector_folder, false, &obj_sync_hierachy_collector);
	mapitest_print_retval_clean(mt, "ICSSyncOpenCollector - Hierarchy", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

cleanup:
	/* Cleanup and release */
	mapi_object_release(&obj_sync_hierachy_collector);
	mapi_object_release(&obj_sync_collector);
	mapi_object_release(&collector_folder);
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return ret;
}

/**
   \details Test the RopSynchronizationImportReadStateChanges (0x80)
   operation.

   This function:
   -# Log on private message store
   -# Create a test folder
   -# Create a message and save it
   -# Get the PidTagSourceKey and PidTagChangeKey
   -# Open a sync collector context for contents
   -# Import Read State Changes to set that message as read
   -# Check the PidTagMessageFlags has changed and PidTagChangeKey could have increased for that message
   -# Clean up
 */
_PUBLIC_ bool mapitest_oxcfxics_SyncImportReadStateChanges(struct mapitest *mt)
{
	struct Binary_r		*source_key, *change_key, *new_change_key;
	bool			ret = true;
	bool			read_states[1];
	enum MAPISTATUS		retval;
	mapi_object_t		obj_htable;
	mapi_object_t		obj_sync_collector;
	mapi_object_t		collector_folder;
	mapi_object_t		obj_message;
	struct mt_common_tf_ctx	*context;
	struct SPropValue	*lpProps;
	struct SPropTagArray	*SPropTagArray;
	uint32_t		c_values, msg_flags;

	/* Step 1. Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	context = mt->priv;

	/* Step 2. Create destfolder */
	mapi_object_init(&collector_folder);
	mapi_object_init(&obj_sync_collector);
	mapi_object_init(&obj_message);
	retval = CreateFolder(&(context->obj_test_folder), FOLDER_GENERIC,
			      "ImportReadStateChanges", NULL /*folder comment*/,
			      OPEN_IF_EXISTS, &collector_folder);
	mapitest_print_retval_clean(mt, "Create Folder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3. Create the message and save it */
	ret = mapitest_common_message_create(mt, &collector_folder, &obj_message,
					     MT_MAIL_SUBJECT);
	mapitest_print_assert(mt, "mapitest_common_message_create", ret);
	if (!ret) {
		goto cleanup;
	}

	retval = SaveChangesMessage(&collector_folder, &obj_message, KeepOpenReadWrite);
	mapitest_print_retval_clean(mt, "SaveChangesMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 4. Get the PidTagSourceKey and PidTagChangeKey */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_SOURCE_KEY, PR_CHANGE_KEY);
	retval = GetProps(&obj_message, 0, SPropTagArray, &lpProps, &c_values);
	mapitest_print_retval_clean(mt, "GetProps - Source Key", retval);
	if (c_values && (lpProps[0].ulPropTag & 0xFFFF) != PT_ERROR
            && (lpProps[1].ulPropTag & 0xFFFF) != PT_ERROR) {
		source_key = (struct Binary_r *)get_SPropValue_data(&lpProps[0]);
		change_key = (struct Binary_r *)get_SPropValue_data(&lpProps[1]);
	} else {
		ret = false;
		goto cleanup;
	}
	read_states[0] = true;

	/* Step 5. Open the collector */
	retval = ICSSyncOpenCollector(&collector_folder, true, &obj_sync_collector);
	mapitest_print_retval_clean(mt, "ICSSyncOpenCollector - Contents", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 6. Import read state change */
	retval = SyncImportReadStateChanges(&obj_sync_collector, source_key, read_states, 1);
	mapitest_print_retval_clean(mt, "SyncImportReadStateChanges", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = OpenMessage(&context->obj_store, mapi_object_get_id(&collector_folder),
			     mapi_object_get_id(&obj_message), &obj_message, 0x0);
	mapitest_print_retval_clean(mt, "OpenMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 7. Get again PidTagChangeKey and PidTagMessageFlags */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2,
					  PR_CHANGE_KEY, PidTagMessageFlags);
	retval = GetProps(&obj_message, 0, SPropTagArray, &lpProps, &c_values);
	mapitest_print_retval_clean(mt, "GetProps - MessageFlags", retval);
	if (c_values && (lpProps[0].ulPropTag & 0xFFFF) != PT_ERROR
            && (lpProps[1].ulPropTag & 0xFFFF) != PT_ERROR) {
		new_change_key = (struct Binary_r *)get_SPropValue_data(&lpProps[0]);
		/* Perform the check to see if new change key is greater than old one */
		ret = (memcmp(change_key->lpb, new_change_key->lpb, change_key->cb) < 0);
		/* This is failing in Exchange 2010 */
		mapitest_print_assert(mt, "New Change Key is greater than old one", ret);
		msg_flags = *(uint32_t *)get_SPropValue_data(&lpProps[1]);
		ret = msg_flags & MSGFLAG_READ;
		mapitest_print_assert(mt, "Marked as read", ret);
	} else {
		ret = false;
	}

cleanup:
	/* Cleanup and release */
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_sync_collector);
	mapi_object_release(&collector_folder);
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return ret;
}
