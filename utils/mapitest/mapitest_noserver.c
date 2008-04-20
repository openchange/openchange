/*
   Stand-alone MAPI testsuite

   OpenChange Project

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
#include <samba/popt.h>
#include <param.h>

#include <utils/openchange-tools.h>
#include <utils/mapitest/mapitest.h>

#define RTF_COMPRESSED1_HEX "d60000000b0100004c5a467556c587aa03000a0072637067313235163200f80b606e0e103033334f01f702a403e3020063680ac073b065743020071302807d0a809d00002a09b009f00490617405b11a520de068098001d020352e4035302e39392e01d031923402805c760890776b0b807464340c606300500b030bb520284a757305407303706520d7129014d003702005a06d07800230390ac0792e0aa20a840a80416eac6f74132005c06c0b806517dbc24f05c074776f2e0ae31a13fa6119132003f018d0086005401b405b026000706b191a11e1001da0"

#define RTF_UNCOMPRESSED1 "{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033{\\fonttbl{\\f0\\fswiss\\fcharset0 Arial;}}\r\n{\\*\\generator Riched20 5.50.99.2014;}\\viewkind4\\uc1\\pard\\f0\\fs20 Just some random commentary.\\par\r\n\\par\r\nAnother line.\\par\r\n\\par\r\nOr two. \\par\r\nOr a line without a blank line.\\par\r\n}}"

static void mapitest_call_test_lzfu(struct mapitest *mt)
{
	enum MAPISTATUS retval;
	TALLOC_CTX *mem_ctx;
	DATA_BLOB uncompressed;
	uint8_t compressed_hex[1024];
	uint8_t *compressed;
	uint32_t compressed_length;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "lzfu decompression");
	mem_ctx = talloc_init("mapitest_call_test_lzfu");

	compressed = talloc_array(mem_ctx, uint8_t, 1024);
	memcpy(compressed_hex, RTF_COMPRESSED1_HEX, 434);
	compressed_length = strhex_to_str((char*)compressed, 434, (char*)compressed_hex);
	if (compressed_length !=217) {
		mapitest_print(mt, MT_HDR_FMT, "uncompress_rtf", "bad length");
		goto cleanup;
	}

	retval = uncompress_rtf(mem_ctx, compressed, compressed_length, &uncompressed);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_subcall(mt,  "uncompress_rtf", GetLastError());
		goto cleanup;
	}	   

	if (0 == strncmp((char*)uncompressed.data, RTF_UNCOMPRESSED1, uncompressed.length)) {
		mapitest_print(mt, MT_HDR_FMT, "uncompress_rtf", "passed");
	} else {
		mapitest_print(mt, MT_HDR_FMT, "uncompress_rtf", "failed comparison");
	}
 cleanup:
	talloc_free(mem_ctx);
}


void mapitest_calls_noserver(struct mapitest *mt)
{
	mapitest_print(mt, MT_HDR_START);

	mapitest_print_interface_start(mt, "Unit tests that do not require a server");

	mapitest_call_test_lzfu(mt);

	mapitest_print_interface_end(mt);

	mapitest_print(mt, MT_HDR_END);
}
