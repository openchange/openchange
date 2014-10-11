/*
   Stand-alone MAPI testsuite

   OpenChange Project - Non connection oriented tests

   Copyright (C) Brad Hards 2008-2009

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
   \file module_noserver.c

   \brief Non connection oriented tests
*/

/* From MS-OXRTFCP, Section 4.1.1 */
#define RTF_COMPRESSED1_HEX "2d0000002b0000004c5a4675f1c5c7a703000a007263706731323542320af32068656c090020627705b06c647d0a800fa0"
#define RTF_UNCOMPRESSED1 "{\\rtf1\\ansi\\ansicpg1252\\pard hello world}\r\n"

/* From MS-OXRTFCP, Section 4.1.2 */
#define RTF_COMPRESSED2_HEX "1a0000001c0000004c5a4675e2d44b51410004205758595a0d6e7d010eb0"
#define RTF_UNCOMPRESSED2 "{\\rtf1 WXYZWXYZWXYZWXYZWXYZ}"


/**
     \details Test the Compressed RTF decompression routine.

   This function:
   -# Loads some test data and checks it
   -# Decompresses the test data
   -# Checks that the decompressed data matches the expected result

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
*/ 
_PUBLIC_ bool mapitest_noserver_lzfu(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	DATA_BLOB		uncompressed1;
	DATA_BLOB		uncompressed2;
	uint8_t			compressed_hex[1024];
	uint8_t			*compressed;
	uint32_t		compressed_length;

	compressed = talloc_array(mt->mem_ctx, uint8_t, 1024);

	memcpy(compressed_hex, RTF_COMPRESSED1_HEX, 98);
	compressed_length = strhex_to_str((char*)compressed, 1024, (char*)compressed_hex, 98);
	if (compressed_length != 49) {
		mapitest_print(mt, "* %-40s: uncompress RTF - Bad length\n", "LZFU");
		return false;
	}

	uint32_t crc = calculateCRC(compressed, 0x10, (49-0x10));
	if (crc == 0xA7C7C5F1) {
		  mapitest_print(mt, "* CRC pass\n");
	} else {
		  mapitest_print(mt, "* CRC failure, expected 0xA7C7C5F1, but got 0x%08X\n", crc);
	}

	retval = uncompress_rtf(mt->mem_ctx, compressed, compressed_length, &uncompressed1);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "uncompress_rtf - step 1 (bad retval)");
		return false;
	}	   

	if (sizeof(RTF_UNCOMPRESSED1) != uncompressed1.length) {
		mapitest_print(mt, "* %-40s: FAILED (bad length: %i vs %i)\n", "uncompress_rtf - step 1",
			       sizeof(RTF_UNCOMPRESSED1), uncompressed1.length);
		return false;
	}
	if (!strncmp((char*)uncompressed1.data, RTF_UNCOMPRESSED1, uncompressed1.length)) {
		mapitest_print(mt, "* %-40s: PASSED\n", "uncompress_rtf - step 1");
	} else {
		mapitest_print(mt, "* %-40s: FAILED - %s\n", "uncompress_rtf - step 1", (char*)uncompressed1.data);
		return false;
	}

	memcpy(compressed_hex, RTF_COMPRESSED2_HEX, 60);
	compressed_length = strhex_to_str((char*)compressed, 1024, (char*)compressed_hex, 60);
	if (compressed_length != 30) {
		mapitest_print(mt, "* %-40s: uncompress RTF - Bad length\n", "LZFU");
		return false;
	}
	retval = uncompress_rtf(mt->mem_ctx, compressed, compressed_length, &uncompressed2);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "uncompress_rtf - step 2 (bad retval)");
		return false;
	}	   

	if (!strncmp((char*)uncompressed2.data, RTF_UNCOMPRESSED2, uncompressed2.length)) {
		mapitest_print(mt, "* %-40s: PASSED\n", "uncompress_rtf - step 2");
	} else {
		mapitest_print(mt, "* %-40s: FAILED - %s\n", "uncompress_rtf - step 2", (char*)uncompressed2.data);
		return false;
	}
	
	/* TODO: add an uncompressed test here */

	return true;
}

/**
     \details Test the Compressed RTF compression routine.

   This function:
   -# Loads some test data and checks it
   -# Compresses the test data
   -# Checks that the compressed data matches the expected result

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
*/ 
_PUBLIC_ bool mapitest_noserver_rtfcp(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	uint8_t			*compressed;
	size_t			compressed_length;
	char 			*compressed_hex;

	/* compress the test data */
	retval = compress_rtf(mt->mem_ctx, RTF_UNCOMPRESSED1, sizeof(RTF_UNCOMPRESSED1) - 1, &compressed, &compressed_length);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "compress_rtf - step 1 (bad retval)");
		return false;
	}

	/* Check the compressed result matches the compressed array */
	compressed_hex = hex_encode_talloc(mt->mem_ctx, compressed, compressed_length);
	if (strncasecmp(compressed_hex, RTF_COMPRESSED1_HEX, (size_t)98) != 0) {
		mapitest_print(mt, "* %-40s: compare results RTF_COMPRESSED1 - mismatch\n", "RTFCP");
		mapitest_print(mt, "- %s\n", RTF_COMPRESSED1_HEX);
		mapitest_print(mt, "- %s\n", compressed_hex);
		return false;
	} else {
		mapitest_print(mt, "* %-40s: compare results RTF_COMPRESSED1 - match\n", "RTFCP");
		mapitest_print(mt, "- %s\n", RTF_COMPRESSED1_HEX);
		mapitest_print(mt, "- %s\n", compressed_hex);
	}
	talloc_free(compressed_hex);
	talloc_free(compressed);

	/* compress the test data */
	retval = compress_rtf(mt->mem_ctx, RTF_UNCOMPRESSED2, sizeof(RTF_UNCOMPRESSED2) - 1, &compressed, &compressed_length);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "compress_rtf - step 2 (bad retval)");
		return false;
	}

	// Check the compressed result matches the compressed array.
	compressed_hex = hex_encode_talloc(mt->mem_ctx, compressed, compressed_length);
	if (strncasecmp(compressed_hex, RTF_COMPRESSED2_HEX, sizeof(RTF_COMPRESSED2_HEX)) != 0) {
		mapitest_print(mt, "* %-40s: compare results RTF_COMPRESSED2 - mismatch\n", "RTFCP");
		mapitest_print(mt, "- %s\n", RTF_COMPRESSED2_HEX);
		mapitest_print(mt, "- %s\n", compressed_hex);
		return false;
	} else {
		mapitest_print(mt, "* %-40s: compare results RTF_COMPRESSED2 - match\n", "RTFCP");
		mapitest_print(mt, "- %s\n", RTF_COMPRESSED2_HEX);
		mapitest_print(mt, "- %s\n", compressed_hex);
	}
	talloc_free(compressed_hex);
	talloc_free(compressed);
	return true;
}

/**
     \details Test the Compressed RTF compression / decompression routines on a larger file

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
*/ 
_PUBLIC_ bool mapitest_noserver_rtfcp_large(struct mapitest *mt)
{
	enum MAPISTATUS		retval;

	char			*filename = NULL;
	char			*original_uncompressed_data;
	size_t			original_uncompressed_length;
	char			*original_uncompressed_hex;

	uint8_t			*compressed;
	size_t			compressed_length;

	DATA_BLOB		decompressed;
	char 			*decompressed_hex;

	/* load the test file */
	filename = talloc_asprintf(mt->mem_ctx, "%s/testcase.rtf", LZFU_DATADIR);
	original_uncompressed_data = file_load(filename, &original_uncompressed_length, 0, mt->mem_ctx);
	if (!original_uncompressed_data) {
		perror(filename);
		mapitest_print(mt, "%s: Error while loading %s\n", __FUNCTION__, filename);
		talloc_free(filename);
		return false;
	}
	talloc_free(filename);
	original_uncompressed_hex = hex_encode_talloc(mt->mem_ctx, (const unsigned char*)original_uncompressed_data, original_uncompressed_length);

	/* compress it */
	retval = compress_rtf(mt->mem_ctx, original_uncompressed_data, original_uncompressed_length, &compressed, &compressed_length);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval_clean(mt, "mapitest_noserver_rtfcp_large - step 1 (bad retval)", retval);
		return false;
	}

	/* decompress it */
	retval = uncompress_rtf(mt->mem_ctx, compressed, compressed_length, &decompressed);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval_clean(mt, "mapitest_noserver_rtfcp_large - step 2 (bad retval)", retval);
		return false;
	}

	mapitest_print(mt, "Original data size = 0x%zx\n", original_uncompressed_length);
	mapitest_print(mt, "Decompressed size  = 0x%zx\n", decompressed.length);
	{
		int i;
		int min;

		min = (original_uncompressed_length >= decompressed.length) ? decompressed.length : original_uncompressed_length;
		mapitest_print(mt, "Comparing data over 0x%x bytes\n", min);
		for (i = 0; i < min; i++) {
			if (decompressed.data[i] != original_uncompressed_data[i]) {
				mapitest_print(mt, "Bytes differ at offset 0x%x: original (0x%.2x) decompressed (0x%.2x)\n", 
						i, original_uncompressed_data[i], decompressed.data[i]);
			}
		}
		
	}

	/* check the uncompressed version (less trailing null) matches the original test file contents */
	decompressed_hex = hex_encode_talloc(mt->mem_ctx, decompressed.data, decompressed.length -1);
	if (strncasecmp(original_uncompressed_hex, decompressed_hex, original_uncompressed_length) != 0) {
		mapitest_print(mt, "* %-40s: compare results - mismatch\n", "RTFCP_LARGE");
		return false;
	} else {
		mapitest_print(mt, "* %-40s: compare results - match\n", "RTFCP_LARGE");
		// mapitest_print(mt, "- %s\n", original_uncompressed_hex);
		// mapitest_print(mt, "- %s\n", decompressed_hex);
	}
	
	/* clean up */
	talloc_free(decompressed_hex);
	talloc_free(compressed);
	talloc_free(original_uncompressed_hex);

	return true;
}

#define SROWSET_UNTAGGED "004d542044756d6d792046726f6d00426f6479206f66206d657373616765203800004d542044756d6d792046726f6d00426f6479206f66206d657373616765203900004d542044756d6d792046726f6d00426f6479206f66206d657373616765203700004d542044756d6d792046726f6d00426f6479206f66206d657373616765203600004d542044756d6d793400426f6479206f66206d657373616765203400004d542044756d6d792046726f6d00426f6479206f66206d657373616765203500004d542044756d6d793300426f6479206f66206d657373616765203300004d542044756d6d793100426f6479206f66206d657373616765203100004d542044756d6d793200426f6479206f66206d657373616765203200004d542044756d6d793000426f6479206f66206d657373616765203000"
#define SROWSET_UNTAGGED_LEN 310

static bool mapitest_noserver_srowset_untagged(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct loadparm_context	*lp_ctx = NULL;
	DATA_BLOB		rawData;
	uint8_t			rawDataHex[1024];
	struct SRowSet		rowSet;
	struct SRowSet		referenceRowSet;
	struct SPropTagArray	*proptags;
	uint32_t		rowNum;

	retval = GetLoadparmContext(mt->mapi_ctx, &lp_ctx);
	if (retval != MAPI_E_SUCCESS) return false;

	rawData.data = talloc_array(mt->mem_ctx, uint8_t, 1024);
	memcpy(rawDataHex, SROWSET_UNTAGGED, 2*SROWSET_UNTAGGED_LEN);
	rawData.length = strhex_to_str((char*)rawData.data, 1024, (char*)rawDataHex, 2*SROWSET_UNTAGGED_LEN);
	if (rawData.length != SROWSET_UNTAGGED_LEN) {
		mapitest_print(mt, "* %-40s: untagged - Bad length\n", "SRowSet");
		return false;
	}

	proptags = set_SPropTagArray(mt->mem_ctx, 2, PR_SENDER_NAME,  PR_BODY);
	rowSet.cRows = 10;
	rowSet.aRow = talloc_array(mt->mem_ctx, struct SRow, 10);
	emsmdb_get_SRowSet(mt->mem_ctx, &rowSet, proptags, &rawData);

	/* Check the resulting SRowSet */
	if (rowSet.cRows != 10) {
		mapitest_print(mt, "* %-40s: unexpected row count: %i\n", "SRowSet", rowSet.cRows);
		return false;
	}

	/* Build reference RowSet */
	referenceRowSet.cRows = rowSet.cRows;
	referenceRowSet.aRow = talloc_array(mt->mem_ctx, struct SRow, rowSet.cRows);
	for (rowNum = 0; rowNum < rowSet.cRows; ++rowNum) {
		referenceRowSet.aRow[rowNum].ulAdrEntryPad = 0;
		referenceRowSet.aRow[rowNum].cValues = 2;
		referenceRowSet.aRow[rowNum].lpProps = talloc_array(mt->mem_ctx, struct SPropValue, 2);
		referenceRowSet.aRow[rowNum].lpProps[0].ulPropTag = PR_SENDER_NAME;
		referenceRowSet.aRow[rowNum].lpProps[0].dwAlignPad = 0;
		referenceRowSet.aRow[rowNum].lpProps[1].ulPropTag = PR_BODY;
		referenceRowSet.aRow[rowNum].lpProps[1].dwAlignPad = 0;
	}
	referenceRowSet.aRow[0].lpProps[0].value.lpszA = "MT Dummy From";
	referenceRowSet.aRow[0].lpProps[1].value.lpszA = "Body of message 8";
	referenceRowSet.aRow[1].lpProps[0].value.lpszA = "MT Dummy From";
	referenceRowSet.aRow[1].lpProps[1].value.lpszA = "Body of message 9";
	referenceRowSet.aRow[2].lpProps[0].value.lpszA = "MT Dummy From";
	referenceRowSet.aRow[2].lpProps[1].value.lpszA = "Body of message 7";
	referenceRowSet.aRow[3].lpProps[0].value.lpszA = "MT Dummy From";
	referenceRowSet.aRow[3].lpProps[1].value.lpszA = "Body of message 6";
	referenceRowSet.aRow[4].lpProps[0].value.lpszA = "MT Dummy4";
	referenceRowSet.aRow[4].lpProps[1].value.lpszA = "Body of message 4";
	referenceRowSet.aRow[5].lpProps[0].value.lpszA = "MT Dummy From";
	referenceRowSet.aRow[5].lpProps[1].value.lpszA = "Body of message 5";
	referenceRowSet.aRow[6].lpProps[0].value.lpszA = "MT Dummy3";
	referenceRowSet.aRow[6].lpProps[1].value.lpszA = "Body of message 3";
	referenceRowSet.aRow[7].lpProps[0].value.lpszA = "MT Dummy1";
	referenceRowSet.aRow[7].lpProps[1].value.lpszA = "Body of message 1";
	referenceRowSet.aRow[8].lpProps[0].value.lpszA = "MT Dummy2";
	referenceRowSet.aRow[8].lpProps[1].value.lpszA = "Body of message 2";
	referenceRowSet.aRow[9].lpProps[0].value.lpszA = "MT Dummy0";
	referenceRowSet.aRow[9].lpProps[1].value.lpszA = "Body of message 0";


	/* compare result with reference rowset */
	for (rowNum = 0; rowNum < rowSet.cRows; ++rowNum) {
		uint32_t	i;
		/* check each row has expected number of properties */
		if (rowSet.aRow[rowNum].cValues != referenceRowSet.aRow[rowNum].cValues) {
			mapitest_print(mt, "* %-40s: unexpected props count, row %i: %i\n", "SRowSet", rowSet.aRow[rowNum].cValues, rowNum);
			return false;
		}
		for (i=0; i < rowSet.aRow[rowNum].cValues; ++i) {
			/* check property tags are as expected */
			if (rowSet.aRow[rowNum].lpProps[i].ulPropTag != referenceRowSet.aRow[rowNum].lpProps[i].ulPropTag) {
				mapitest_print(mt, "* %-40s: unexpected proptag (%i/%i): 0x%08x\n", "SRowSet", rowNum, i, rowSet.aRow[rowNum].lpProps[i].ulPropTag);
				return false;
			}
			/* check property values are as expected */
			if (strcmp(rowSet.aRow[rowNum].lpProps[i].value.lpszA, referenceRowSet.aRow[rowNum].lpProps[i].value.lpszA) != 0) {
				mapitest_print(mt, "* %-40s: unexpected property value (%i/%i): %s\n", "SRowSet", rowNum, i, rowSet.aRow[rowNum].lpProps[i].value.lpszA);
				return false;
			}
		}
	}
	return true;
}

#define SROWSET_TAGGED	"01004d542044756d6d792046726f6d000a0f010480004d542044756d6d792046726f6d00426f6479206f66206d657373616765203500004d542044756d6d792046726f6d00426f6479206f66206d657373616765203600004d542044756d6d792046726f6d00426f6479206f66206d657373616765203700004d542044756d6d792046726f6d00426f6479206f66206d657373616765203800004d542044756d6d792046726f6d00426f6479206f66206d65737361676520390001004d542044756d6d7930000a0f010480004d542044756d6d793000426f6479206f66206d65737361676520300001004d542044756d6d7931000a0f010480004d542044756d6d793100426f6479206f66206d65737361676520310001004d542044756d6d7932000a0f010480004d542044756d6d793200426f6479206f66206d65737361676520320001004d542044756d6d7933000a0f010480004d542044756d6d793300426f6479206f66206d65737361676520330001004d542044756d6d7934000a0f010480004d542044756d6d793400426f6479206f66206d657373616765203400"
#define SROWSET_TAGGED_LEN 416


static bool mapitest_noserver_srowset_tagged(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct loadparm_context	*lp_ctx = NULL;
	DATA_BLOB		rawData;
	uint8_t			rawDataHex[1024];
	struct SRowSet		rowSet;
	struct SRowSet		referenceRowSet;
	struct SPropTagArray	*proptags;
	uint32_t		rowNum;

	retval = GetLoadparmContext(mt->mapi_ctx, &lp_ctx);
	if (retval != MAPI_E_SUCCESS) return false;

	rawData.data = talloc_array(mt->mem_ctx, uint8_t, 1024);
	memcpy(rawDataHex, SROWSET_TAGGED, 2*SROWSET_TAGGED_LEN);
	rawData.length = strhex_to_str((char*)rawData.data, 1024, (char*)rawDataHex, 2*SROWSET_TAGGED_LEN);
	if (rawData.length != SROWSET_TAGGED_LEN) {
		mapitest_print(mt, "* %-40s: tagged - Bad length\n", "SRowSet");
		return false;
	}

	proptags = set_SPropTagArray(mt->mem_ctx, 2, PR_SENDER_NAME,  PR_BODY);
	rowSet.cRows = 16;
	rowSet.aRow = talloc_array(mt->mem_ctx, struct SRow, 16);
	emsmdb_get_SRowSet(mt->mem_ctx, &rowSet, proptags, &rawData);

	/* Check the resulting SRowSet */
	if (rowSet.cRows != 16) {
		mapitest_print(mt, "* %-40s: unexpected row count: %i\n", "SRowSet", rowSet.cRows);
		return false;
	}

	/* Build reference RowSet */
	referenceRowSet.cRows = rowSet.cRows;
	referenceRowSet.aRow = talloc_array(mt->mem_ctx, struct SRow, rowSet.cRows);
	for (rowNum = 0; rowNum < rowSet.cRows; ++rowNum) {
		referenceRowSet.aRow[rowNum].ulAdrEntryPad = 0;
		referenceRowSet.aRow[rowNum].cValues = 2;
		referenceRowSet.aRow[rowNum].lpProps = talloc_array(mt->mem_ctx, struct SPropValue, 2);
		referenceRowSet.aRow[rowNum].lpProps[0].ulPropTag = PR_SENDER_NAME;
		referenceRowSet.aRow[rowNum].lpProps[0].dwAlignPad = 0;
		referenceRowSet.aRow[rowNum].lpProps[1].ulPropTag = PR_BODY;
		referenceRowSet.aRow[rowNum].lpProps[1].dwAlignPad = 0;
	}
	referenceRowSet.aRow[0].lpProps[0].value.lpszA = "MT Dummy From";
	referenceRowSet.aRow[0].lpProps[1].ulPropTag = PR_BODY_ERROR;
	referenceRowSet.aRow[0].lpProps[1].value.err = MAPI_E_NOT_FOUND;
	referenceRowSet.aRow[1].lpProps[0].value.lpszA = "MT Dummy From";
	referenceRowSet.aRow[1].lpProps[1].value.lpszA = "Body of message 5";
	referenceRowSet.aRow[2].lpProps[0].value.lpszA = "MT Dummy From";
	referenceRowSet.aRow[2].lpProps[1].value.lpszA = "Body of message 6";
	referenceRowSet.aRow[3].lpProps[0].value.lpszA = "MT Dummy From";
	referenceRowSet.aRow[3].lpProps[1].value.lpszA = "Body of message 7";
	referenceRowSet.aRow[4].lpProps[0].value.lpszA = "MT Dummy From";
	referenceRowSet.aRow[4].lpProps[1].value.lpszA = "Body of message 8";
	referenceRowSet.aRow[5].lpProps[0].value.lpszA = "MT Dummy From";
	referenceRowSet.aRow[5].lpProps[1].value.lpszA = "Body of message 9";
	referenceRowSet.aRow[6].lpProps[0].value.lpszA = "MT Dummy0";
	referenceRowSet.aRow[6].lpProps[1].ulPropTag = PR_BODY_ERROR;
	referenceRowSet.aRow[6].lpProps[1].value.err = MAPI_E_NOT_FOUND;
	referenceRowSet.aRow[7].lpProps[0].value.lpszA = "MT Dummy0";
	referenceRowSet.aRow[7].lpProps[1].value.lpszA = "Body of message 0";
	referenceRowSet.aRow[8].lpProps[0].value.lpszA = "MT Dummy1";
	referenceRowSet.aRow[8].lpProps[1].ulPropTag = PR_BODY_ERROR;
	referenceRowSet.aRow[8].lpProps[1].value.err = MAPI_E_NOT_FOUND;
	referenceRowSet.aRow[9].lpProps[0].value.lpszA = "MT Dummy1";
	referenceRowSet.aRow[9].lpProps[1].value.lpszA = "Body of message 1";
	referenceRowSet.aRow[10].lpProps[0].value.lpszA = "MT Dummy2";
	referenceRowSet.aRow[10].lpProps[1].ulPropTag = PR_BODY_ERROR;
	referenceRowSet.aRow[10].lpProps[1].value.err = MAPI_E_NOT_FOUND;
	referenceRowSet.aRow[11].lpProps[0].value.lpszA = "MT Dummy2";
	referenceRowSet.aRow[11].lpProps[1].value.lpszA = "Body of message 2";
	referenceRowSet.aRow[12].lpProps[0].value.lpszA = "MT Dummy3";
	referenceRowSet.aRow[12].lpProps[1].ulPropTag = PR_BODY_ERROR;
	referenceRowSet.aRow[12].lpProps[1].value.err = MAPI_E_NOT_FOUND;
	referenceRowSet.aRow[13].lpProps[0].value.lpszA = "MT Dummy3";
	referenceRowSet.aRow[13].lpProps[1].value.lpszA = "Body of message 3";
	referenceRowSet.aRow[14].lpProps[0].value.lpszA = "MT Dummy4";
	referenceRowSet.aRow[14].lpProps[1].ulPropTag = PR_BODY_ERROR;
	referenceRowSet.aRow[14].lpProps[1].value.err = MAPI_E_NOT_FOUND;
	referenceRowSet.aRow[15].lpProps[0].value.lpszA = "MT Dummy4";
	referenceRowSet.aRow[15].lpProps[1].value.lpszA = "Body of message 4";

	/* compare result with reference rowset */
	for (rowNum = 0; rowNum < rowSet.cRows; ++rowNum) {
		uint32_t	i;
		/* check each row has expected number of properties */
		if (rowSet.aRow[rowNum].cValues != referenceRowSet.aRow[rowNum].cValues) {
			mapitest_print(mt, "* %-40s: unexpected props count, row %i: %i\n", "SRowSet", rowSet.aRow[rowNum].cValues, rowNum);
			return false;
		}
		for (i=0; i < rowSet.aRow[rowNum].cValues; ++i) {
			/* check property tags are as expected */
			if (rowSet.aRow[rowNum].lpProps[i].ulPropTag != referenceRowSet.aRow[rowNum].lpProps[i].ulPropTag) {
				mapitest_print(mt, "* %-40s: unexpected proptag (%i/%i): 0x%08x\n", "SRowSet", rowNum, i, rowSet.aRow[rowNum].lpProps[i].ulPropTag);
				return false;
			}
			/* check property values are as expected */
			if ((rowSet.aRow[rowNum].lpProps[i].ulPropTag & 0xFFFF) == PT_ERROR) {
				if (rowSet.aRow[rowNum].lpProps[i].value.err != referenceRowSet.aRow[rowNum].lpProps[i].value.err) {
					mapitest_print(mt, "* %-40s: unexpected property error value (%i/%i): 0x%04x\n", "SRowSet", rowNum, i, rowSet.aRow[rowNum].lpProps[i].value.err);
					return false;
				}
			} else {
				if (strcmp(rowSet.aRow[rowNum].lpProps[i].value.lpszA, referenceRowSet.aRow[rowNum].lpProps[i].value.lpszA) != 0) {
					mapitest_print(mt, "* %-40s: unexpected property value (%i/%i): %s\n", "SRowSet", rowNum, i, rowSet.aRow[rowNum].lpProps[i].value.lpszA);
					return false;
				}
			}
		}
	}
	return true;
}

/**
     \details Test the SRowSet parsing / assembly code

   This function:
   -# Loads some test data and checks it
   -# Parses the test data 
   -# Checks that the parsed data matches the expected result

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
*/
_PUBLIC_ bool mapitest_noserver_srowset(struct mapitest *mt) 
{
	bool result;

	result = mapitest_noserver_srowset_untagged(mt);
	if (result == false) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SRowSet Untagged");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SRowSet Untagged");

	result = mapitest_noserver_srowset_tagged(mt);
	if (result == false) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SRowSet Tagged");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SRowSet Tagged");

	return true;
}

static bool mapitest_no_server_props_i2(struct mapitest *mt)
{
	struct SPropValue propvalue;
	uint16_t i2 = 0x5693; /* just a random, not zero value */
	const uint16_t *i2get;

	set_SPropValue_proptag(&propvalue, PT_SHORT, &i2);
	i2get = (const uint16_t*)get_SPropValue_data(&propvalue);
	if (!i2get || (*i2get != i2)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_SHORT");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_SHORT");
	return true;
}

static bool mapitest_no_server_props_i4(struct mapitest *mt)
{
	struct SPropValue propvalue;
	uint32_t i4 = 0x33870911;
	const uint32_t *i4get;

	set_SPropValue_proptag(&propvalue, PT_LONG, &i4);
	i4get = (const uint32_t*)get_SPropValue_data(&propvalue);
	if (!i4get || (*i4get != i4)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_LONG");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_LONG");

	return true;
}

static bool mapitest_no_server_props_i8(struct mapitest *mt)
{
	struct SPropValue	propvalue;
	int64_t			i8 = 0x098763aa;
	const int64_t		*i8get;

	set_SPropValue_proptag(&propvalue, PT_I8, &i8);
	i8get = (const int64_t*)get_SPropValue_data(&propvalue);
	if (!i8get || (*i8get != i8)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_I8");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_I8");

	return true;
}


static bool mapitest_no_server_props_bool(struct mapitest *mt)
{
	struct SPropValue propvalue;
	bool b = false;
	const bool *boolget;

	set_SPropValue_proptag(&propvalue, PT_BOOLEAN, &b);
	boolget = (const bool*)get_SPropValue_data(&propvalue);
	if (!boolget || (*boolget != b)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_BOOLEAN");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_BOOLEAN");

	return true;
}

static bool mapitest_no_server_props_err(struct mapitest *mt)
{
	struct SPropValue propvalue;
	enum MAPISTATUS err = MAPI_E_NOT_ME;
	enum MAPISTATUS *errget;

	set_SPropValue_proptag(&propvalue, PT_ERROR, &err);
	errget = (enum MAPISTATUS*)get_SPropValue_data(&propvalue);
	if (!errget || (*errget != err)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_ERROR");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_ERROR");

	return true;
}

/* this seems strange... */
static bool mapitest_no_server_props_null(struct mapitest *mt)
{
	struct SPropValue propvalue;
	uint32_t nullval = 0;
	const uint32_t *nullget;

	set_SPropValue_proptag(&propvalue, PT_NULL, &nullval);
	nullget = (const uint32_t*)get_SPropValue_data(&propvalue);
	if (!nullget) {
		printf("null nullget\n");
	}
	if (!nullget || (*nullget != nullval)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_NULL");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_NULL");

	return true;
}


static bool mapitest_no_server_props_string8(struct mapitest *mt)
{
	struct SPropValue propvalue;
	char *string = "OpenChange Project";
	const char **stringget;

	set_SPropValue_proptag(&propvalue, PT_STRING8, &string);
	stringget = (const char**)get_SPropValue_data(&propvalue);
	if (!stringget || (strncmp(*stringget, string, strlen(*stringget)) != 0)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_STRING8");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_STRING8");

	return true;
}

static bool mapitest_no_server_props_unicode(struct mapitest *mt)
{
	struct SPropValue propvalue;
	char *string = "OpenChange Project"; /* OK, so it probably isn't valid UTF16.. */
	const char **stringget;

	set_SPropValue_proptag(&propvalue, PT_UNICODE, &string);
	stringget = (const char**)get_SPropValue_data(&propvalue);
	if (!stringget || (strncmp(*stringget, string, strlen(*stringget)) != 0)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_UNICODE");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_UNICODE");

	return true;
}

static bool mapitest_no_server_props_filetime(struct mapitest *mt)
{
	struct SPropValue propvalue;
	struct FILETIME ft;
	const struct FILETIME *ftget;

	ft.dwLowDateTime = 0x12345678;
	ft.dwHighDateTime = 0x87653491;
	set_SPropValue_proptag(&propvalue, PT_SYSTIME, &ft);
	ftget = (const struct FILETIME *)get_SPropValue_data(&propvalue);
	if (!ftget || (ft.dwLowDateTime != ftget->dwLowDateTime) || (ft.dwHighDateTime != ftget->dwHighDateTime)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_SYSTIME");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_SYSTIME");

	return true;
}
#if 0
static bool mapitest_no_server_props_flatuid(struct mapitest *mt)
{
	bool res;
	struct SPropValue propvalue;
	struct FlatUID_r uid;
	const struct FlatUID_r *uidget;
	int i;

	// initialise uid
	for (i = 0; i < 16; ++i) {
		uid.ab[i] = 0x40 + i;
	}
	printf("uid:");
	for (i = 0; i < 16; ++i) {
		printf("%c", uid.ab[i]);
	}
	printf("\n");

	res = set_SPropValue_proptag(&propvalue, PT_CLSID, &uid);
	if (res == false) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue set with PT_CLSID");
		return false;
	}
	uidget = (const struct FlatUID_r*)get_SPropValue_data(&propvalue);
	if (!uidget) {
		printf("null uidget\n");
		return false;
	}
	printf("uidget:");
	for (i = 0; i < 16; ++i) {
		printf("%c", uidget->ab[i]);
	}
	printf("\n");
	if (!uidget || (memcmp(uid.ab, uidget->ab, 16))) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_CLSID");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_CLSID");

	return true;
}
#endif

static bool mapitest_no_server_props_bin(struct mapitest *mt)
{
	bool res;
	struct SPropValue propvalue;

	struct Binary_r bin;
	const struct Binary_r *binget;
	uint32_t	i;

	// initialise bin
	bin.cb = 8;
	bin.lpb = talloc_array(mt->mem_ctx, uint8_t, bin.cb);
	for (i = 0; i < bin.cb; ++i) {
		bin.lpb[i] = 0xF0 + i;
	}

	res = set_SPropValue_proptag(&propvalue, PT_BINARY, &bin);
	if (res == false) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue set with PT_BINARY");
		return false;
	}
	binget = (const struct Binary_r *)get_SPropValue_data(&propvalue);
	if (!binget || (bin.cb != binget->cb) || (bin.lpb != binget->lpb)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_BINARY");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_BINARY");

	talloc_free(bin.lpb);

	return true;
}


static bool mapitest_no_server_props_mv_i2(struct mapitest *mt)
{
	bool res;
	struct SPropValue propvalue;
	struct ShortArray_r shortarray;
	const struct ShortArray_r *shortarrayget;

	// create and initialise shortarray
	shortarray.cValues = 3;
	shortarray.lpi = talloc_array(mt->mem_ctx, uint16_t, shortarray.cValues);
	shortarray.lpi[0] = 0x1245;
	shortarray.lpi[1] = 0x3498;
	shortarray.lpi[2] = 0x5675;
	res = set_SPropValue_proptag(&propvalue, PT_MV_SHORT, &shortarray);
	if (res == false) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue set with PT_MV_SHORT");
		return false;
	}
	shortarrayget = (const struct ShortArray_r *)get_SPropValue_data(&propvalue);
	if (!shortarrayget || (shortarray.cValues != shortarrayget->cValues) || (shortarray.lpi != shortarrayget->lpi)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_MV_SHORT");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_MV_SHORT");

	talloc_free(shortarray.lpi);

	return true;
}

static bool mapitest_no_server_props_mv_i4(struct mapitest *mt)
{
	bool res;
	struct SPropValue propvalue;
	struct LongArray_r longarray;
	const struct LongArray_r *longarrayget;

	// create and initialise longarray
	longarray.cValues = 2;
	longarray.lpl = talloc_array(mt->mem_ctx, uint32_t, longarray.cValues);
	longarray.lpl[0] = 0x34124543;
	longarray.lpl[1] = 0x88567576;
	res = set_SPropValue_proptag(&propvalue, PT_MV_LONG, &longarray);
	if (res == false) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue set with PT_MV_LONG");
		return false;
	}
	longarrayget = (const struct LongArray_r *)get_SPropValue_data(&propvalue);
	if (!longarrayget || (longarray.cValues != longarrayget->cValues) || (longarray.lpl != longarrayget->lpl)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_MV_LONG");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_MV_LONG");

	talloc_free(longarray.lpl);

	return true;
}

static bool mapitest_no_server_props_mv_bin(struct mapitest *mt)
{
	bool res;
	struct SPropValue propvalue;
	struct BinaryArray_r binarray;
	const struct BinaryArray_r *binarrayget;

	// create and initialise binarray
	struct Binary_r bin1, bin2;
	uint32_t i;

	// initialise bin
	bin1.cb = 8;
	bin1.lpb = talloc_array(mt->mem_ctx, uint8_t, bin1.cb);
	for (i = 0; i < bin1.cb; ++i) {
		bin1.lpb[i] = 0xC0 + i;
	}
	bin2.cb = 12;
	bin2.lpb = talloc_array(mt->mem_ctx, uint8_t, bin2.cb);
	for (i = 0; i < bin2.cb; ++i) {
		bin2.lpb[i] = 0xA0 + i;
	}

	binarray.cValues = 2;
	binarray.lpbin = talloc_array(mt->mem_ctx, struct Binary_r, binarray.cValues);
	binarray.lpbin[0] = bin1;
	binarray.lpbin[1] = bin2;

	res = set_SPropValue_proptag(&propvalue, PT_MV_BINARY, &binarray);
	if (res == false) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue set with PT_MV_BINARY");
		return false;
	}
	binarrayget = (const struct BinaryArray_r *)get_SPropValue_data(&propvalue);
	if (!binarrayget || (binarray.cValues != binarrayget->cValues) || (binarray.lpbin != binarrayget->lpbin)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_MV_BINARY");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_MV_BINARY");

	talloc_free(binarray.lpbin);
	talloc_free(bin1.lpb);
	talloc_free(bin2.lpb);

	return true;
}

static bool mapitest_no_server_props_mv_string8(struct mapitest *mt)
{
	bool res;
	struct SPropValue propvalue;
	struct StringArray_r stringarray;
	const struct StringArray_r *stringarrayget;

	// create and initialise stringarray
	stringarray.cValues = 4;
	stringarray.lppszA = talloc_array(mt->mem_ctx, const char*, stringarray.cValues);
	stringarray.lppszA[0] = "Fedora";
	stringarray.lppszA[1] = "FreeBSD";
	stringarray.lppszA[2] = "OpenSolaris";
	stringarray.lppszA[3] = "Debian";

	res = set_SPropValue_proptag(&propvalue, PT_MV_STRING8, &stringarray);
	if (res == false) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue set with PT_MV_STRING8");
		return false;
	}
	stringarrayget = (const struct StringArray_r *)get_SPropValue_data(&propvalue);
	if (!stringarrayget || (stringarray.cValues != stringarrayget->cValues) || (stringarray.lppszA != stringarrayget->lppszA)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_MV_STRING8");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_MV_STRING8");

	talloc_free(stringarray.lppszA);

	return true;
}

static bool mapitest_no_server_props_mv_unicode(struct mapitest *mt)
{
	bool res;
	struct SPropValue propvalue;
	struct StringArrayW_r unicodearray;
	const struct StringArrayW_r *unicodearrayget;

	// create and initialise unicodearray
	unicodearray.cValues = 4;
	unicodearray.lppszW = talloc_array(mt->mem_ctx, const char*, unicodearray.cValues);
	unicodearray.lppszW[0] = "Fedora";  /* not valid UTF16, but should still be OK */
	unicodearray.lppszW[1] = "FreeBSD";
	unicodearray.lppszW[2] = "OpenSolaris";
	unicodearray.lppszW[3] = "Debian";

	res = set_SPropValue_proptag(&propvalue, PT_MV_UNICODE, &unicodearray);
	if (res == false) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue set with PT_MV_UNICODE");
		return false;
	}
	unicodearrayget = (const struct StringArrayW_r *)get_SPropValue_data(&propvalue);
	if (!unicodearrayget || (unicodearray.cValues != unicodearrayget->cValues) || (unicodearray.lppszW != unicodearrayget->lppszW)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_MV_UNICODE");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_MV_UNICODE");

	talloc_free(unicodearray.lppszW);

	return true;
}


static bool mapitest_no_server_props_mv_filetime(struct mapitest *mt)
{
	struct SPropValue propvalue;
	struct FILETIME ft1, ft2, ft3;
	struct DateTimeArray_r ftarray;
	const struct DateTimeArray_r *ftarrayget;

	ft1.dwLowDateTime = 0x12345678;
	ft1.dwHighDateTime = 0x87653491;
	ft2.dwLowDateTime = 0x10234324;
	ft2.dwHighDateTime = 0x98748756;
	ft3.dwLowDateTime = 0x53245324;
	ft3.dwHighDateTime = 0x54324633;

	ftarray.cValues = 3;
	ftarray.lpft = talloc_array(mt->mem_ctx, struct FILETIME, ftarray.cValues);
	ftarray.lpft[0] = ft1;
	ftarray.lpft[1] = ft2;
	ftarray.lpft[2] = ft3;

	set_SPropValue_proptag(&propvalue, PT_MV_SYSTIME, &ftarray);
	ftarrayget = (const struct DateTimeArray_r *)get_SPropValue_data(&propvalue);
	if (!ftarrayget || (ftarray.cValues != ftarrayget->cValues) || (ftarray.lpft != ftarrayget->lpft)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "SPropValue get/set with PT_MV_SYSTIME");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "SPropValue get/set with PT_MV_SYSTIME");

	talloc_free(ftarray.lpft);

	return true;
}

/**
     \details Test the property setter / getter code

   This function:
   -# Checks setting / getting on an SPropValue

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
*/
_PUBLIC_ bool mapitest_noserver_properties(struct mapitest *mt) 
{
	if (! mapitest_no_server_props_i2(mt)) {
		return false;
	}

	if (! mapitest_no_server_props_i4(mt)) {
		return false;
	}

	if (! mapitest_no_server_props_i8(mt)) {
		return false;
	}

	if (! mapitest_no_server_props_bool(mt)) {
		return false;
	}

	if (! mapitest_no_server_props_err(mt)) {
		return false;
	}

	if (! mapitest_no_server_props_null(mt)) {
		return false;
	}

	if (! mapitest_no_server_props_string8(mt)) {
		return false;
	}

	if (! mapitest_no_server_props_unicode(mt)) {
		return false;
	}

	if (! mapitest_no_server_props_filetime(mt)) {
		return false;
	}

#if 0
	if (! mapitest_no_server_props_flatuid(mt)) {
		return false;
	}
#endif

	if (! mapitest_no_server_props_bin(mt)) {
		return false;
	}

	if (! mapitest_no_server_props_mv_i2(mt)) {
		return false;
	}

	if (! mapitest_no_server_props_mv_i4(mt)) {
		return false;
	}

	if (! mapitest_no_server_props_mv_string8(mt)) {
		return false;
	}

	if (! mapitest_no_server_props_mv_unicode(mt)) {
		return false;
	}

	if (! mapitest_no_server_props_mv_filetime(mt)) {
		return false;
	}

	if (! mapitest_no_server_props_mv_bin(mt)) {
		return false;
	}

	/*
	    Types we don't test yet:
	 */
	// should this be a double_t?
	// int64_t dbl;/* [case(0x0005)] */

	// struct FlatUID_r *lpguid;/* [unique,case(0x0048)] */
	// struct FlatUIDArray_r MVguid;/* [case(0x1048)] */
	// uint32_t object;/* [case(0x000d)] */


	return true;
}

/**
     \details Test the mapi_SPropValue_array handling

   This function:
   -# Builds a mapi_SPropValue_array
   -# Checks that appropriate values can be retrieved

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
*/
_PUBLIC_ bool mapitest_noserver_mapi_properties(struct mapitest *mt) 
{
	struct mapi_SPropValue_array valarray;
	const uint16_t *i2get;
	const uint32_t *i4get;
	const uint32_t *i8get;
	const uint8_t *boolget;
	const char *stringget;
	const struct FILETIME *ftget;
	const struct SBinary_short *binget;
	const struct mapi_MV_LONG_STRUCT *mvi4get;

	valarray.cValues = 8;
	valarray.lpProps = talloc_array(mt->mem_ctx, struct mapi_SPropValue, valarray.cValues);

	valarray.lpProps[0].ulPropTag = PR_GENDER;
	valarray.lpProps[0].value.i = 0x0001;  /* Female */
	valarray.lpProps[1].ulPropTag = PR_ORIGINAL_SENSITIVITY;
	valarray.lpProps[1].value.l = 0x00000002; /* Private */
	valarray.lpProps[2].ulPropTag = PR_ALTERNATE_RECIPIENT_ALLOWED;
	valarray.lpProps[2].value.b = false; 
	valarray.lpProps[3].ulPropTag = PR_CONVERSATION_TOPIC;
	valarray.lpProps[3].value.lpszA = "Elizabeth's Birthday"; 
	valarray.lpProps[4].ulPropTag = PR_MESSAGE_SIZE_EXTENDED;
	valarray.lpProps[4].value.d = 43857;
	valarray.lpProps[5].ulPropTag = PR_RECORD_KEY;
	valarray.lpProps[5].value.bin.cb = 4;
	valarray.lpProps[5].value.bin.lpb = talloc_array(mt->mem_ctx, uint8_t, 4);
	valarray.lpProps[5].value.bin.lpb[0] = 0x44;
	valarray.lpProps[5].value.bin.lpb[1] = 0x00;
	valarray.lpProps[5].value.bin.lpb[2] = 0x20;
	valarray.lpProps[5].value.bin.lpb[3] = 0x00;
	valarray.lpProps[6].ulPropTag = PR_SCHDINFO_MONTHS_BUSY;
	valarray.lpProps[6].value.MVl.cValues = 2;
	valarray.lpProps[6].value.MVl.lpl = talloc_array(mt->mem_ctx, uint32_t, 2);
	valarray.lpProps[6].value.MVl.lpl[0] = 32130;
	valarray.lpProps[6].value.MVl.lpl[1] = 32131;
	valarray.lpProps[7].ulPropTag = PidLidAppointmentEndWhole;
	valarray.lpProps[7].value.ft.dwLowDateTime  = 0x12975909;
	valarray.lpProps[7].value.ft.dwHighDateTime  = 0x98989204;

	/* now start pulling the values back out */
	i2get = find_mapi_SPropValue_data(&valarray, PR_GENDER);
	if (!i2get || (*i2get != 0x0001)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "mapi_SPropValue find with PT_SHORT");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "mapi_SPropValue find with PT_SHORT");


	i4get = find_mapi_SPropValue_data(&valarray, PR_ORIGINAL_SENSITIVITY);
	if (!i4get || (*i4get != 0x00000002)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "mapi_SPropValue find with PT_LONG");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "mapi_SPropValue find with PT_LONG");


	i8get = find_mapi_SPropValue_data(&valarray, PR_MESSAGE_SIZE_EXTENDED);
	if (!i8get || (*i8get != 43857)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "mapi_SPropValue find with PT_I8");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "mapi_SPropValue find with PT_I8");


	boolget = find_mapi_SPropValue_data(&valarray, PR_ALTERNATE_RECIPIENT_ALLOWED);
	if (!boolget || (*boolget != false)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "mapi_SPropValue find with PT_BOOLEAN");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "mapi_SPropValue find with PT_BOOLEAN");


	stringget = find_mapi_SPropValue_data(&valarray, PR_CONVERSATION_TOPIC);
	if (!stringget || (strcmp(stringget, "Elizabeth's Birthday") !=0 )) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "mapi_SPropValue find with PT_STRING");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "mapi_SPropValue find with PT_STRING");

	ftget = find_mapi_SPropValue_data(&valarray, PidLidAppointmentEndWhole);
	if (!ftget || (ftget->dwLowDateTime != 0x12975909) || (ftget->dwHighDateTime != 0x98989204)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "mapi_SPropValue find with PT_FILETIME");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "mapi_SPropValue find with PT_FILETIME");

	binget = find_mapi_SPropValue_data(&valarray, PR_RECORD_KEY);
	if (!binget || (binget->cb != 4 ) || (binget->lpb[0] != 0x44) || (binget->lpb[1] != 0x00)
	    || (binget->lpb[2] != 0x20) || (binget->lpb[3] != 0x00)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "mapi_SPropValue find with PT_BINARY");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "mapi_SPropValue find with PT_BINARY");


	mvi4get = find_mapi_SPropValue_data(&valarray, PR_SCHDINFO_MONTHS_BUSY);
	if (!mvi4get || (mvi4get->cValues != 2 ) || (mvi4get->lpl[0] != 32130) || (mvi4get->lpl[1] != 32131)) {
		/* failure */
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "mapi_SPropValue find with PT_MV_LONG");
		return false;
	}
	mapitest_print(mt, "* %-40s: [SUCCESS]\n", "mapi_SPropValue find with PT_MV_LONG");

#if 0
	// Types to still to test:
        int64_t dbl;/* [case(0x0005)] */
        uint32_t err;/* [case(0x000a)] */
        const char * lpszW;/* [flag(LIBNDR_FLAG_STR_NULLTERM),case(0x001f)] */
        struct GUID lpguid;/* [case(0x0048)] */
        struct mapi_SRestriction_wrap Restrictions;/* [case(0x00fd)] */
        struct RuleAction RuleAction;/* [case(0x00fe)] */
        struct mapi_SLPSTRArray MVszA;/* [case(0x101e)] */
        struct mapi_SPLSTRArrayW MVszW;/* [case(0x101f)] */
        struct mapi_SGuidArray MVguid;/* [case(0x1048)] */
        struct mapi_SBinaryArray MVbin;/* [case(0x1102)] */
#endif
	return true;
}

/**
     \details Test the get_proptag_value() function

   This function:
   -# Checks the first value in the list
   -# Checks a random value from the list
   -# Checks the last value in the list
   -# Checks a value that doesn't exist

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
*/
_PUBLIC_ bool mapitest_noserver_proptagvalue(struct mapitest *mt) 
{
	uint32_t proptag;
	
	proptag = get_proptag_value("PidTagTemplateData");
	if (proptag != PidTagTemplateData) {
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "get_proptag_value with PidTagTemplateData");
		return false;
	}

	proptag = get_proptag_value("PidTagDelegatedByRule");
	if (proptag != PidTagDelegatedByRule) {
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "get_proptag_value with PidTagDelegatedByRule");
		return false;
	}

	proptag = get_proptag_value("PidTagAddressBookContainerId_Error");
	if (proptag != PidTagAddressBookContainerId_Error) {
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "get_proptag_value with PidTagAddressBookContainerId_Error");
		return false;
	}

	proptag = get_proptag_value("No such tag, ok?");
	if (proptag != 0) {
		mapitest_print(mt, "* %-40s: [FAILURE]\n", "get_proptag_value with non-existent tag");
		return false;
	}

	return true;
}
