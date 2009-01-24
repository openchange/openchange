/*
   Stand-alone MAPI testsuite

   OpenChange Project - Non connection oriented tests

   Copyright (C) Brad Hards 2008

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
   \file module_noserver.c

   \brief Non connection oriented tests
*/

#define RTF_COMPRESSED1_HEX "d60000000b0100004c5a467556c587aa03000a0072637067313235163200f80b606e0e103033334f01f702a403e3020063680ac073b065743020071302807d0a809d00002a09b009f00490617405b11a520de068098001d020352e4035302e39392e01d031923402805c760890776b0b807464340c606300500b030bb520284a757305407303706520d7129014d003702005a06d07800230390ac0792e0aa20a840a80416eac6f74132005c06c0b806517dbc24f05c074776f2e0ae31a13fa6119132003f018d0086005401b405b026000706b191a11e1001da0"

#define RTF_UNCOMPRESSED1 "{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033{\\fonttbl{\\f0\\fswiss\\fcharset0 Arial;}}\r\n{\\*\\generator Riched20 5.50.99.2014;}\\viewkind4\\uc1\\pard\\f0\\fs20 Just some random commentary.\\par\r\n\\par\r\nAnother line.\\par\r\n\\par\r\nOr two. \\par\r\nOr a line without a blank line.\\par\r\n}}"

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
	DATA_BLOB		uncompressed;
	uint8_t			compressed_hex[1024];
	uint8_t			*compressed;
	uint32_t		compressed_length;

	compressed = talloc_array(mt->mem_ctx, uint8_t, 1024);
	memcpy(compressed_hex, RTF_COMPRESSED1_HEX, 434);
	compressed_length = strhex_to_str((char*)compressed, 1024, (char*)compressed_hex, 434);
	if (compressed_length != 217) {
		mapitest_print(mt, "* %-35s: uncompress RTF - Bad length\n", "LZFU");
		return false;
	}

	retval = uncompress_rtf(mt->mem_ctx, compressed, compressed_length, &uncompressed);
	mapitest_print_retval(mt, "uncompress_rtf");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}	   

	if (!strncmp((char*)uncompressed.data, RTF_UNCOMPRESSED1, uncompressed.length)) {
		mapitest_print(mt, "* %-35s: PASSED\n", "uncompress_rtf");
	} else {
		mapitest_print(mt, "* %-35s: FAILED\n", "uncompress_rtf");
		return false;
	}
	
	return true;
}

#define SROWSET_UNTAGGED "005b4d545d2044756d6d792046726f6d00426f6479206f66206d657373616765203800005b4d545d2044756d6d792046726f6d00426f6479206f66206d657373616765203900005b4d545d2044756d6d792046726f6d00426f6479206f66206d657373616765203700005b4d545d2044756d6d792046726f6d00426f6479206f66206d657373616765203600005b4d545d2044756d6d793400426f6479206f66206d657373616765203400005b4d545d2044756d6d792046726f6d00426f6479206f66206d657373616765203500005b4d545d2044756d6d793300426f6479206f66206d657373616765203300005b4d545d2044756d6d793100426f6479206f66206d657373616765203100005b4d545d2044756d6d793200426f6479206f66206d657373616765203200005b4d545d2044756d6d793000426f6479206f66206d657373616765203000"
#define SROWSET_UNTAGGED_LEN 330

_PUBLIC_ bool mapitest_noserver_srowset_untagged(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct loadparm_context	*lp_ctx = NULL;
	DATA_BLOB		rawData;
	uint8_t			rawDataHex[1024];
	struct SRowSet		rowSet;
	struct SRowSet		referenceRowSet;
	struct SPropTagArray	*proptags;
	uint32_t		rowNum;
	int		i;

	retval = GetLoadparmContext(lp_ctx);
	if (retval != MAPI_E_SUCCESS) return false;

	rawData.data = talloc_array(mt->mem_ctx, uint8_t, 1024);
	memcpy(rawDataHex, SROWSET_UNTAGGED, 2*SROWSET_UNTAGGED_LEN);
	rawData.length = strhex_to_str((char*)rawData.data, 1024, (char*)rawDataHex, 2*SROWSET_UNTAGGED_LEN);
	if (rawData.length != SROWSET_UNTAGGED_LEN) {
		mapitest_print(mt, "* %-35s: untagged - Bad length\n", "SRowSet");
		return false;
	}

	proptags = set_SPropTagArray(mt->mem_ctx, 2, PR_SENDER_NAME,  PR_BODY);
	rowSet.cRows = 10;
	rowSet.aRow = talloc_array(mt->mem_ctx, struct SRow, 10);
	emsmdb_get_SRowSet(mt->mem_ctx, lp_ctx, &rowSet, proptags, &rawData);

	/* Check the resulting SRowSet */
	if (rowSet.cRows != 10) {
		mapitest_print(mt, "* %-35s: unexpected row count: %i\n", "SRowSet", rowSet.cRows);
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
	referenceRowSet.aRow[0].lpProps[0].value.lpszA = "[MT] Dummy From";
	referenceRowSet.aRow[0].lpProps[1].value.lpszA = "Body of message 8";
	referenceRowSet.aRow[1].lpProps[0].value.lpszA = "[MT] Dummy From";
	referenceRowSet.aRow[1].lpProps[1].value.lpszA = "Body of message 9";
	referenceRowSet.aRow[2].lpProps[0].value.lpszA = "[MT] Dummy From";
	referenceRowSet.aRow[2].lpProps[1].value.lpszA = "Body of message 7";
	referenceRowSet.aRow[3].lpProps[0].value.lpszA = "[MT] Dummy From";
	referenceRowSet.aRow[3].lpProps[1].value.lpszA = "Body of message 6";
	referenceRowSet.aRow[4].lpProps[0].value.lpszA = "[MT] Dummy4";
	referenceRowSet.aRow[4].lpProps[1].value.lpszA = "Body of message 4";
	referenceRowSet.aRow[5].lpProps[0].value.lpszA = "[MT] Dummy From";
	referenceRowSet.aRow[5].lpProps[1].value.lpszA = "Body of message 5";
	referenceRowSet.aRow[6].lpProps[0].value.lpszA = "[MT] Dummy3";
	referenceRowSet.aRow[6].lpProps[1].value.lpszA = "Body of message 3";
	referenceRowSet.aRow[7].lpProps[0].value.lpszA = "[MT] Dummy1";
	referenceRowSet.aRow[7].lpProps[1].value.lpszA = "Body of message 1";
	referenceRowSet.aRow[8].lpProps[0].value.lpszA = "[MT] Dummy2";
	referenceRowSet.aRow[8].lpProps[1].value.lpszA = "Body of message 2";
	referenceRowSet.aRow[9].lpProps[0].value.lpszA = "[MT] Dummy0";
	referenceRowSet.aRow[9].lpProps[1].value.lpszA = "Body of message 0";


	/* compare result with reference rowset */
	for (rowNum = 0; rowNum < rowSet.cRows; ++rowNum) {
		/* check each row has expected number of properties */
		if (rowSet.aRow[rowNum].cValues != referenceRowSet.aRow[rowNum].cValues) {
			mapitest_print(mt, "* %-35s: unexpected props count, row %i: %i\n", "SRowSet", rowSet.aRow[rowNum].cValues, rowNum);
			return false;
		}
		for (i=0; i < rowSet.aRow[rowNum].cValues; ++i) {
			/* check property tags are as expected */
			if (rowSet.aRow[rowNum].lpProps[i].ulPropTag != referenceRowSet.aRow[rowNum].lpProps[i].ulPropTag) {
				mapitest_print(mt, "* %-35s: unexpected proptag (%i/%i): 0x%08x\n", "SRowSet", rowNum, i, rowSet.aRow[rowNum].lpProps[i].ulPropTag);
				return false;
			}
			/* check property values are as expected */
			if (strcmp(rowSet.aRow[rowNum].lpProps[i].value.lpszA, referenceRowSet.aRow[rowNum].lpProps[i].value.lpszA) != 0) {
				mapitest_print(mt, "* %-35s: unexpected property value (%i/%i): %s\n", "SRowSet", rowNum, i, rowSet.aRow[rowNum].lpProps[i].value.lpszA);
				return false;
			}
		}
	}
	return true;
}

#define SROWSET_TAGGED	"01005b4d545d2044756d6d792046726f6d000a0f010480005b4d545d2044756d6d792046726f6d00426f6479206f66206d657373616765203500005b4d545d2044756d6d792046726f6d00426f6479206f66206d657373616765203600005b4d545d2044756d6d792046726f6d00426f6479206f66206d657373616765203700005b4d545d2044756d6d792046726f6d00426f6479206f66206d657373616765203800005b4d545d2044756d6d792046726f6d00426f6479206f66206d65737361676520390001005b4d545d2044756d6d7930000a0f010480005b4d545d2044756d6d793000426f6479206f66206d65737361676520300001005b4d545d2044756d6d7931000a0f010480005b4d545d2044756d6d793100426f6479206f66206d65737361676520310001005b4d545d2044756d6d7932000a0f010480005b4d545d2044756d6d793200426f6479206f66206d65737361676520320001005b4d545d2044756d6d7933000a0f010480005b4d545d2044756d6d793300426f6479206f66206d65737361676520330001005b4d545d2044756d6d7934000a0f010480005b4d545d2044756d6d793400426f6479206f66206d657373616765203400"
#define SROWSET_TAGGED_LEN 448


_PUBLIC_ bool mapitest_noserver_srowset_tagged(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct loadparm_context	*lp_ctx = NULL;
	DATA_BLOB		rawData;
	uint8_t			rawDataHex[1024];
	struct SRowSet		rowSet;
	struct SRowSet		referenceRowSet;
	struct SPropTagArray	*proptags;
	uint32_t		rowNum;
	int		i;

	retval = GetLoadparmContext(lp_ctx);
	if (retval != MAPI_E_SUCCESS) return false;

	rawData.data = talloc_array(mt->mem_ctx, uint8_t, 1024);
	memcpy(rawDataHex, SROWSET_TAGGED, 2*SROWSET_TAGGED_LEN);
	rawData.length = strhex_to_str((char*)rawData.data, 1024, (char*)rawDataHex, 2*SROWSET_TAGGED_LEN);
	if (rawData.length != SROWSET_TAGGED_LEN) {
		mapitest_print(mt, "* %-35s: tagged - Bad length\n", "SRowSet");
		return false;
	}

	proptags = set_SPropTagArray(mt->mem_ctx, 2, PR_SENDER_NAME,  PR_BODY);
	rowSet.cRows = 16;
	rowSet.aRow = talloc_array(mt->mem_ctx, struct SRow, 16);
	emsmdb_get_SRowSet(mt->mem_ctx, lp_ctx, &rowSet, proptags, &rawData);

	/* Check the resulting SRowSet */
	if (rowSet.cRows != 16) {
		mapitest_print(mt, "* %-35s: unexpected row count: %i\n", "SRowSet", rowSet.cRows);
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
	referenceRowSet.aRow[0].lpProps[0].value.lpszA = "[MT] Dummy From";
	referenceRowSet.aRow[0].lpProps[1].ulPropTag = PR_BODY_ERROR;
	referenceRowSet.aRow[0].lpProps[1].value.err = MAPI_E_NOT_FOUND;
	referenceRowSet.aRow[1].lpProps[0].value.lpszA = "[MT] Dummy From";
	referenceRowSet.aRow[1].lpProps[1].value.lpszA = "Body of message 5";
	referenceRowSet.aRow[2].lpProps[0].value.lpszA = "[MT] Dummy From";
	referenceRowSet.aRow[2].lpProps[1].value.lpszA = "Body of message 6";
	referenceRowSet.aRow[3].lpProps[0].value.lpszA = "[MT] Dummy From";
	referenceRowSet.aRow[3].lpProps[1].value.lpszA = "Body of message 7";
	referenceRowSet.aRow[4].lpProps[0].value.lpszA = "[MT] Dummy From";
	referenceRowSet.aRow[4].lpProps[1].value.lpszA = "Body of message 8";
	referenceRowSet.aRow[5].lpProps[0].value.lpszA = "[MT] Dummy From";
	referenceRowSet.aRow[5].lpProps[1].value.lpszA = "Body of message 9";
	referenceRowSet.aRow[6].lpProps[0].value.lpszA = "[MT] Dummy0";
	referenceRowSet.aRow[6].lpProps[1].ulPropTag = PR_BODY_ERROR;
	referenceRowSet.aRow[6].lpProps[1].value.err = MAPI_E_NOT_FOUND;
	referenceRowSet.aRow[7].lpProps[0].value.lpszA = "[MT] Dummy0";
	referenceRowSet.aRow[7].lpProps[1].value.lpszA = "Body of message 0";
	referenceRowSet.aRow[8].lpProps[0].value.lpszA = "[MT] Dummy1";
	referenceRowSet.aRow[8].lpProps[1].ulPropTag = PR_BODY_ERROR;
	referenceRowSet.aRow[8].lpProps[1].value.err = MAPI_E_NOT_FOUND;
	referenceRowSet.aRow[9].lpProps[0].value.lpszA = "[MT] Dummy1";
	referenceRowSet.aRow[9].lpProps[1].value.lpszA = "Body of message 1";
	referenceRowSet.aRow[10].lpProps[0].value.lpszA = "[MT] Dummy2";
	referenceRowSet.aRow[10].lpProps[1].ulPropTag = PR_BODY_ERROR;
	referenceRowSet.aRow[10].lpProps[1].value.err = MAPI_E_NOT_FOUND;
	referenceRowSet.aRow[11].lpProps[0].value.lpszA = "[MT] Dummy2";
	referenceRowSet.aRow[11].lpProps[1].value.lpszA = "Body of message 2";
	referenceRowSet.aRow[12].lpProps[0].value.lpszA = "[MT] Dummy3";
	referenceRowSet.aRow[12].lpProps[1].ulPropTag = PR_BODY_ERROR;
	referenceRowSet.aRow[12].lpProps[1].value.err = MAPI_E_NOT_FOUND;
	referenceRowSet.aRow[13].lpProps[0].value.lpszA = "[MT] Dummy3";
	referenceRowSet.aRow[13].lpProps[1].value.lpszA = "Body of message 3";
	referenceRowSet.aRow[14].lpProps[0].value.lpszA = "[MT] Dummy4";
	referenceRowSet.aRow[14].lpProps[1].ulPropTag = PR_BODY_ERROR;
	referenceRowSet.aRow[14].lpProps[1].value.err = MAPI_E_NOT_FOUND;
	referenceRowSet.aRow[15].lpProps[0].value.lpszA = "[MT] Dummy4";
	referenceRowSet.aRow[15].lpProps[1].value.lpszA = "Body of message 4";

	/* compare result with reference rowset */
	for (rowNum = 0; rowNum < rowSet.cRows; ++rowNum) {
		/* check each row has expected number of properties */
		if (rowSet.aRow[rowNum].cValues != referenceRowSet.aRow[rowNum].cValues) {
			mapitest_print(mt, "* %-35s: unexpected props count, row %i: %i\n", "SRowSet", rowSet.aRow[rowNum].cValues, rowNum);
			return false;
		}
		for (i=0; i < rowSet.aRow[rowNum].cValues; ++i) {
			/* check property tags are as expected */
			if (rowSet.aRow[rowNum].lpProps[i].ulPropTag != referenceRowSet.aRow[rowNum].lpProps[i].ulPropTag) {
				mapitest_print(mt, "* %-35s: unexpected proptag (%i/%i): 0x%08x\n", "SRowSet", rowNum, i, rowSet.aRow[rowNum].lpProps[i].ulPropTag);
				return false;
			}
			/* check property values are as expected */
			if ((rowSet.aRow[rowNum].lpProps[i].ulPropTag & 0xFFFF) == PT_ERROR) {
				if (rowSet.aRow[rowNum].lpProps[i].value.err != referenceRowSet.aRow[rowNum].lpProps[i].value.err) {
					mapitest_print(mt, "* %-35s: unexpected property error value (%i/%i): 0x%04x\n", "SRowSet", rowNum, i, rowSet.aRow[rowNum].lpProps[i].value.err);
					return false;
				}
			} else {
				if (strcmp(rowSet.aRow[rowNum].lpProps[i].value.lpszA, referenceRowSet.aRow[rowNum].lpProps[i].value.lpszA) != 0) {
					mapitest_print(mt, "* %-35s: unexpected property value (%i/%i): %s\n", "SRowSet", rowNum, i, rowSet.aRow[rowNum].lpProps[i].value.lpszA);
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
		mapitest_print(mt, "* %-35s: [FAILURE]\n", "SRowSet Untagged");
		return false;
	}
	mapitest_print(mt, "* %-35s: [SUCCESS]\n", "SRowSet Untagged");

	result = mapitest_noserver_srowset_tagged(mt);
	if (result == false) {
		/* failure */
		mapitest_print(mt, "* %-35s: [FAILURE]\n", "SRowSet Tagged");
		return false;
	}
	mapitest_print(mt, "* %-35s: [SUCCESS]\n", "SRowSet Tagged");

	return true;
}

