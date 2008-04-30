/*
   Stand-alone MAPI testsuite

   OpenChange Project - PROPERTY AND STREAM OBJECT PROTOCOL operations

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

#include <libmapi/libmapi.h>
#include "utils/mapitest/mapitest.h"
#include "utils/mapitest/proto.h"

/**
   \file module_oxcprpt.c

   \brief Property and Stream Object Protocol test suite
*/

/**
   \details Test the GetProps (0x7) operation

   This function:
   -# Log on the user private mailbox
   -# Retrieve the properties list using GetPropList
   -# Retrieve their associated values using the GetProps operation

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_GetProps(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		cValues;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}	
	
	/* Step 2. Retrieve the properties list using GetPropList */
	SPropTagArray = talloc_zero(mt->mem_ctx, struct SPropTagArray);
	retval = GetPropList(&obj_store, SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Call the GetProps operation */
	retval = GetProps(&obj_store, SPropTagArray, &lpProps, &cValues);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetProps", GetLastError());	
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	MAPIFreeBuffer(SPropTagArray);

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the GetPropsAll (0x8) operation

   This function:
   -# Log on the user private mailbox
   -# Retrieve the whole set of properties and values associated to the store object

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_GetPropsAll(struct mapitest *mt)
{
	enum MAPISTATUS			retval;
	mapi_object_t			obj_store;
	struct mapi_SPropValue_array	properties_array;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. GetPropsAll operation */
	retval = GetPropsAll(&obj_store, &properties_array);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetPropsAll", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the GetPropList (0x9) operation

   This function:
   -# Log on the user private mailbox
   -# Retrieve the list of properties associated to the store object object

   \param mt pointer on the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_GetPropList(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	struct SPropTagArray	*SPropTagArray;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. GetPropList operation */
	SPropTagArray = talloc_zero(mt->mem_ctx, struct SPropTagArray);
	retval = GetPropList(&obj_store, SPropTagArray);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetPropList", GetLastError());
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the SetProps (0xa) operation

   This function:
   -# Logon Private mailbox
   -# Use GetProps to retrieve the mailbox name
   -# Change it using SetProps
   -# Reset the mailbox name to its original value

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_SetProps(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	struct SPropValue	*lpProps;
	struct SPropValue	lpProp[1];
	struct SPropTagArray	*SPropTagArray;
	const char		*mailbox = NULL;
	const char		*new_mailbox = NULL;
	uint32_t		cValues;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	mapitest_print(mt, "* Step 1. - Logon Private Mailbox\n", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2: GetProps, retrieve mailbox name */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = GetProps(&obj_store, SPropTagArray, &lpProps, &cValues);
	mapitest_print(mt, "* Step 2. - Retrieve the mailbox name\n", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	MAPIFreeBuffer(SPropTagArray);

	if (cValues && lpProps[0].value.lpszA) {
		mailbox = lpProps[0].value.lpszA;
		mapitest_print(mt, "* Step 2. Mailbox name = %s\n", mailbox);
	} else {
		mapitest_print(mt, MT_ERROR, "* Step 2 - GetProps: No mailbox name\n");
		return false;
	}

	/* Step 2.1: SetProps with new value */
	cValues = 1;
	new_mailbox = talloc_asprintf(mt->mem_ctx, "%s [MAPITEST]", mailbox);
	set_SPropValue_proptag(&lpProp[0], PR_DISPLAY_NAME, 
			       (const void *) new_mailbox);
	retval = SetProps(&obj_store, lpProp, cValues);
	mapitest_print(mt, "* Step 2.1 - Set:   NEW mailbox name: 0x%.8x\n", GetLastError());

	/* Step 2.2: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = GetProps(&obj_store, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(new_mailbox, lpProps[0].value.lpszA, strlen(lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 2.2 - Check: NEW mailbox name - [SUCCESS] (%s)\n", lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 2.2 - Check: NEW mailbox name [FAILURE]\n");
		}
	}
	MAPIFreeBuffer((void *)new_mailbox);

	/* Step 3.1: Reset mailbox to its original value */
	cValues = 1;
	set_SPropValue_proptag(&lpProp[0], PR_DISPLAY_NAME, (const void *)mailbox);
	retval = SetProps(&obj_store, lpProp, cValues);
	mapitest_print(mt, "* Step 3.1 - Set:   OLD mailbox name 0x%.8x\n", GetLastError());
	
	/* Step 3.2: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = GetProps(&obj_store, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(mailbox, lpProps[0].value.lpszA, strlen(lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 3.2 - Check: OLD mailbox name [SUCCESS]\n");
		} else {
			mapitest_print(mt, "* Step 3.2 - Check: OLD mailbox name, [FAILURE]\n");
		}
	}

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}



/**
   \details Test Stream operations. This test uses all related stream
   operations: OpenStream (0x2b), SetStreamSize (0x2f), WriteStream
   (0x2d), CommitStream (0x5d), ReadStream (0x2c), SeekStream (0x2e)
   
   This function:
   -# Logon 
   -# Open Outbox folder
   -# Create message
   -# Create attachment and set properties
   -# Open the stream
   -# Set the stream size
   -# Write into the stream
   -# Commit the stream
   -# Save the message
   -# Get stream size and compare values
   -# Open the stream again with different permissions
   -# Read the stream and compare buffers
   -# SeekStream at 0x1000 from the end of the stream
   -# Read the 0x1000 last bytes and check if it matches
   -# Delete the message;

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise -1
 */
_PUBLIC_ bool mapitest_oxcprpt_Stream(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_object_t		obj_attach;
	mapi_object_t		obj_stream;
	mapi_id_t		id_folder;
	DATA_BLOB		data;
	struct SPropValue	attach[3];
	char			*stream = NULL;
	char			*out_stream = NULL;
	uint32_t		stream_len = 0x32146;
	unsigned char		buf[0x4000];
	uint32_t		StreamSize = 0;
	uint32_t		read_size = 0;
	uint16_t		write_len = 0;
	uint32_t		len = 0;
	uint32_t		offset = 0;
	mapi_id_t		id_msgs[1];
	uint32_t		i;
	uint64_t		NewPosition;

	stream = mapitest_common_genblob(mt->mem_ctx, stream_len);
	if (stream == NULL) {
		return false;
	}

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Inbox folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderInbox);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_folder, id_folder, &obj_folder);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create the message */
	mapi_object_init(&obj_message);
	mapitest_common_message_create(mt, &obj_folder, &obj_message, MT_MAIL_SUBJECT);

	/* Step 4. Create the attachment */
	mapi_object_init(&obj_attach);
	retval = CreateAttach(&obj_message, &obj_attach);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "CreateAttach", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	attach[0].ulPropTag = PR_ATTACH_METHOD;
	attach[0].value.l = ATTACH_BY_VALUE;
	attach[1].ulPropTag = PR_RENDERING_POSITION;
	attach[1].value.l = 0;
	attach[2].ulPropTag = PR_ATTACH_FILENAME;
	attach[2].value.lpszA = MT_MAIL_ATTACH;

	retval = SetProps(&obj_attach, attach, 3);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 5. Open the stream */
	mapi_object_init(&obj_stream);
	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 2, &obj_stream);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "OpenStream", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 6. Set the stream size */
	retval = SetStreamSize(&obj_stream, (uint64_t) stream_len);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SetStreamSize", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 7. Write the stream */
	write_len = 0;

	if (stream_len < 0x4000) {
		data.length = stream_len;
		data.data = (uint8_t *) stream;
		retval = WriteStream(&obj_stream, &data, &write_len);
		mapitest_print(mt, "* %-35s: (0x%x bytes written) 0x%.8x\n", "WriteStream", write_len, GetLastError());
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
		}
	} else {
		uint32_t	StreamSize = stream_len;

		for (offset = 0, len = 0x4000, i = 0; StreamSize; i++) {
			data.length = len;
			data.data = (uint8_t *)stream + offset;
			retval = WriteStream(&obj_stream, &data, &write_len);
			mapitest_print(mt, "* %-35s: [%d] (0x%x bytes written) 0x%.8x\n", "WriteStream", i, write_len, GetLastError());

			StreamSize -= write_len;
			if (StreamSize > 0x4000) {
				offset += 0x4000;
			} else {
				offset += write_len;
				len = StreamSize;
			}
		}
	}

 	/* Step 8. Commit the stream */
	retval = CommitStream(&obj_stream);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "CommitStream", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 9. Save the message */
	retval = SaveChanges(&obj_message, &obj_attach, KEEP_OPEN_READONLY);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SaveChanges", GetLastError());
	retval = SaveChangesMessage(&obj_folder, &obj_message);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SaveChangesMessage", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 10. Get stream size */
	retval = GetStreamSize(&obj_stream, &StreamSize);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetStreamSize", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}
	mapitest_print(mt, "* %-35s: %s\n", "StreamSize comparison", 
		       (StreamSize == stream_len) ? "[PASSED]" : "[FAILURE]");

	/* Step 11. Read the stream */
	mapi_object_release(&obj_stream);
	mapi_object_init(&obj_stream);

	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 0, &obj_stream);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "OpenStream", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	offset = 0;
	out_stream = talloc_size(mt->mem_ctx, StreamSize + 1);
	do {
		retval = ReadStream(&obj_stream, buf, 0x4000, &read_size);
		mapitest_print(mt, "* %-35s: (0x%x bytes read) 0x%.8x\n", "ReadStream", read_size, GetLastError());
		memcpy(out_stream + offset, buf, read_size);
		offset += read_size;
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
			break;
		}
	} while (read_size || offset != StreamSize);
	out_stream[offset] = '\0';

	if (offset) {
		if (!strcmp(stream, out_stream)) {
			mapitest_print(mt, "* %-35s: [IN,OUT] stream [PASSED]\n", "Comparison");
		} else {
			mapitest_print(mt, "* %-35s: [IN,OUT] stream [FAILURE]\n", "Comparison");

		}
	}

	/* Step 12. SeekStream from the end of the stream */
	retval = SeekStream(&obj_stream, 0x2, (uint64_t) -0x1000, &NewPosition);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SeekStream", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}


	talloc_free(out_stream);
	out_stream = talloc_size(mt->mem_ctx, 0x1000);
	retval = ReadStream(&obj_stream, (uint8_t *)out_stream, 0x1000, &read_size);
	out_stream[read_size] = '\0';
	mapitest_print(mt, "* %-35s: (0x%x bytes read) 0x%.8x\n", "ReadStream", read_size, GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}
	
	if (read_size && !strcmp(out_stream, stream + StreamSize - read_size)) {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Comparison");
	} else {
		mapitest_print(mt, "* %-35s: [FAILURE]\n", "Comparison");
	}

	/* Step 14. Delete the message */
	errno = 0;
	id_msgs[0] = mapi_object_get_id(&obj_message);
	retval = DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "DeleteMessage", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Release */
	mapi_object_release(&obj_stream);
	mapi_object_release(&obj_attach);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	talloc_free(stream);
	talloc_free(out_stream);

	return ret;
}


/**
   \details Test the CopyToStream (0x3a) operation

   This function:
   -# Logon the mailbox
   -# Open the inbox folder
   -# Create a sample messages with an attachment
   -# Create 2 streams
   -# Fill the first stream with random data
   -# Seek stream positions to the beginning
   -# CopyToStream data from first stream to the second stream
   -# Read dst stream and compare with src stream
   -# Delete the message
   
   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_CopyToStream(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_object_t		obj_attach;
	mapi_object_t		obj_attach2;
	mapi_object_t		obj_stream;
	mapi_object_t		obj_stream2;
	mapi_id_t		id_folder;
	mapi_id_t		id_msgs[1];
	struct SPropValue	attach[3];
	DATA_BLOB		data;
	char			*stream = NULL;
	char			*dst_stream = NULL;
	uint32_t		stream_len = 0x32146;
	unsigned char		buf[0x4000];
	uint32_t		StreamSize = 0;
	uint16_t		write_len = 0;
	uint32_t		read_size = 0;
	uint32_t		len = 0;
	uint32_t		offset = 0;
	uint32_t		i;
	uint64_t		ReadByteCount = 0;
	uint64_t		WrittenByteCount = 0;
	uint64_t		NewPosition = 0;

	stream = mapitest_common_genblob(mt->mem_ctx, stream_len);
	if (stream == NULL) {
		return false;
	}

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Inbox folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderInbox);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_folder, id_folder, &obj_folder);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create the message */
	mapi_object_init(&obj_message);
	mapitest_common_message_create(mt, &obj_folder, &obj_message, MT_MAIL_SUBJECT);

	/* Step 4. Create the first attachment */
	mapi_object_init(&obj_attach);
	retval = CreateAttach(&obj_message, &obj_attach);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "CreateAttach", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	attach[0].ulPropTag = PR_ATTACH_METHOD;
	attach[0].value.l = ATTACH_BY_VALUE;
	attach[1].ulPropTag = PR_RENDERING_POSITION;
	attach[1].value.l = 0;
	attach[2].ulPropTag = PR_ATTACH_FILENAME;
	attach[2].value.lpszA = MT_MAIL_ATTACH;

	retval = SetProps(&obj_attach, attach, 3);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 5. Open the stream */
	mapi_object_init(&obj_stream);
	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 2, &obj_stream);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "OpenStream", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 6. Set the stream size */
	retval = SetStreamSize(&obj_stream, (uint64_t) stream_len);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SetStreamSize", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 7. Write the stream */
	write_len = 0;

	if (stream_len < 0x4000) {
		data.length = stream_len;
		data.data = (uint8_t *) stream;
		retval = WriteStream(&obj_stream, &data, &write_len);
		mapitest_print(mt, "* %-35s: (0x%x bytes written) 0x%.8x\n", "WriteStream", write_len, GetLastError());
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
		}
	} else {
		uint32_t	StreamSize = stream_len;

		for (offset = 0, len = 0x4000, i = 0; StreamSize; i++) {
			data.length = len;
			data.data = (uint8_t *)stream + offset;
			retval = WriteStream(&obj_stream, &data, &write_len);
			mapitest_print(mt, "* %-35s: [%d] (0x%x bytes written) 0x%.8x\n", "WriteStream", i, write_len, GetLastError());

			StreamSize -= write_len;
			if (StreamSize > 0x4000) {
				offset += 0x4000;
			} else {
				offset += write_len;
				len = StreamSize;
			}
		}
	}

 	/* Step 8. Commit the stream */
	retval = CommitStream(&obj_stream);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "CommitStream", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 9. Save the attachment */
	retval = SaveChanges(&obj_message, &obj_attach, KEEP_OPEN_READONLY);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SaveChanges", GetLastError());

	/* Step 10. Create the second attachment */
	mapi_object_init(&obj_attach2);
	retval = CreateAttach(&obj_message, &obj_attach2);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "CreateAttach", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	attach[0].ulPropTag = PR_ATTACH_METHOD;
	attach[0].value.l = ATTACH_BY_VALUE;
	attach[1].ulPropTag = PR_RENDERING_POSITION;
	attach[1].value.l = 0;
	attach[2].ulPropTag = PR_ATTACH_FILENAME;
	attach[2].value.lpszA = MT_MAIL_ATTACH2;

	retval = SetProps(&obj_attach2, attach, 3);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 11. Open the dst stream */
	mapi_object_init(&obj_stream2);
	retval = OpenStream(&obj_attach2, PR_ATTACH_DATA_BIN, 2, &obj_stream2);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "OpenStream", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 12. Get src stream size */
	retval = GetStreamSize(&obj_stream, &StreamSize);
	mapitest_print(mt, "* %-35s: (0x%x) 0x%.8x\n", "GetStreamSize (Src)", StreamSize, GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 13. Reset streams positions to the beginning */
	retval = SeekStream(&obj_stream, 0, 0, &NewPosition);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SeekStream (Src)", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	retval = SeekStream(&obj_stream2, 0, 0, &NewPosition);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SeekStream (Drc)", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 14. Copy src to dst stream */
	retval = CopyToStream(&obj_stream, &obj_stream2, StreamSize, &ReadByteCount, &WrittenByteCount);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "CopyToStream", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 14. Save the message */
	retval = SaveChanges(&obj_message, &obj_attach2, KEEP_OPEN_READONLY);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SaveChanges", GetLastError());
	retval = SaveChangesMessage(&obj_folder, &obj_message);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SaveChangesMessage", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 15. Compare values */
	mapitest_print(mt, "* %-35s: 0x%llx - 0x%llx %s\n", "Read/Write bytes comparison",
		       ReadByteCount, WrittenByteCount, 
		       (ReadByteCount == WrittenByteCount) ? "[SUCCESS]" : "[FAILURE]");


	/* Step 16. Get dst stream size */
	retval = GetStreamSize(&obj_stream2, &StreamSize);
	mapitest_print(mt, "* %-35s: (0x%x) 0x%.8x\n", "GetStreamSize (Dst)", StreamSize, GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	retval = SeekStream(&obj_stream2, 0, 0, &NewPosition);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SeekStream (Drc)", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 17. Read the dst stream */
	offset = 0;
	dst_stream = talloc_size(mt->mem_ctx, StreamSize + 1);
	do {
		retval = ReadStream(&obj_stream2, buf, 0x4000, &read_size);
		mapitest_print(mt, "* %-35s: (0x%x bytes read) 0x%.8x\n", "ReadStream", read_size, GetLastError());
		memcpy(dst_stream + offset, buf, read_size);
		offset += read_size;
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
			break;
		}
	} while (read_size || offset != StreamSize);
	dst_stream[offset] = '\0';

	/* Step 18. Compare streams */
	if (!strcmp(stream, dst_stream)) {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Comparison");
	} else {
		mapitest_print(mt, "* %-35s: [FAILURE]\n", "Comparison");
	}
	

	/* Step 19. Delete Message */
	errno = 0;
	id_msgs[0] = mapi_object_get_id(&obj_message);
	retval = DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "DeleteMessage", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Release */
	mapi_object_release(&obj_stream2);
	mapi_object_release(&obj_stream);
	mapi_object_release(&obj_attach2);
	mapi_object_release(&obj_attach);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	talloc_free(stream);
	talloc_free(dst_stream);

	return ret;
}
