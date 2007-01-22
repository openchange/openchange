/* 
   MAPI Implementation

   OpenChange Project

   Copyright (C) Julien Kerihuel 2005
   
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

#include "openchange.h"
#include "exchange.h"
#include "libmapi/include/mapi.h"

#define	DFLT_REMAINING "\xFF\xFF\xFF\xFF"
#define DFLT_PADDING	0xFF

/*
  FILL_FIRST_BYTE maps the latest previous byte to the first reqr one:
  previous:	xxxxxxxx [byte n]
  reqr:		[byte 1] FFFFFFF
*/

#define	FILL_FIRST_BYTE(reqr, previous, length) do { \
	reqr[0] = previous[length - 4]; \
	reqr[1] = previous[length - 3]; \
	reqr[2] = previous[length - 2]; \
	reqr[3] = previous[length - 1];	\
} while (0)

#define	FILL_WITH_FIRST_BYTE(reqr, previous) do { \
	reqr[0] = previous[0]; \
	reqr[1] = previous[1]; \
	reqr[2] = previous[2]; \
	reqr[3] = previous[3];	\
} while (0)

/*
  SWAP_BYTES inverts the latest bytes of previous and store them in reqr:
  previous	[byte 1][byte 2]
  reqr		[byte 2][byte 1]
*/

#define	SWAP_BYTES(reqr, previous, length) do {	\
	reqr[0] = previous[length - 4];		\
	reqr[1] = previous[length - 3];		\
	reqr[2] = previous[length - 2];		\
	reqr[3] = previous[length - 1];		\
						\
	reqr[4] = previous[length - 8];		\
	reqr[5] = previous[length - 7];		\
	reqr[6] = previous[length - 6];		\
	reqr[7] = previous[length - 5];		\
} while (0)

/*
  CONCAT_BYTES maps the first and last byte of previous at the same offset in reqr:
  previous:	[byte 1] xxxxxxxx [byte n]
  reqr:		[byte 1] FFFFFFFF [byte n]
*/

#define	CONCAT_BYTES(reqr, previous, length, curlen) do {	\
	reqr = memmove(reqr, previous, 4);			\
	reqr[curlen - 4] = previous[length - 4];		\
	reqr[curlen - 3] = previous[length - 3];		\
	reqr[curlen - 2] = previous[length - 2];		\
	reqr[curlen - 1] = previous[length - 1];		\
} while (0);

uint8_t *remaining_blob_alloc(size_t bnumber)
{
	uint8_t		*blob;
	size_t		length;

	length = bnumber * 4;

	blob = talloc_size(NULL, length);
	memset(blob, DFLT_PADDING, length);

	return (blob);
}

uint8_t *fill_remaining_blob(struct MAPI_DATA current, uint8_t *previous, uint32_t length)
{
	uint8_t			*reqr = NULL;
/* 	static uint16_t		opnum; */
/* 	static uint8_t		remcode[3]; */
/* 	static uint8_t		remcode_0x5[3]; */
/* 	uint16_t		copnum; */
/* 	static BOOL    		lock = False; */

/* 	if (!previous) */
/* 	{ */
/* 		reqr = remaining_blob_alloc(1); */
/* 	} */
/* 	else { */
/* /\* 		copnum = (current.content[1] << 8) | current.content[0]; *\/ */
/* 		copnum = current.opnum; */
/* 		printf("current opnum = 0x%.04x\n", copnum); */
/* 		printf("previous remaining length = %d\n", length); */
/* 		switch (copnum) */
/* 		{ */
/* 		case 0x4: */
/* 		{ */
/* 			reqr = remaining_blob_alloc(2); */
/* 			FILL_FIRST_BYTE(reqr, previous, length); */
/* 			FILL_FIRST_BYTE(remcode, previous, length); */
/* /\* 			printf("------------------\n"); *\/ */
/* /\* 			dump_data(1, remcode, 4); *\/ */
/* /\* 			printf("------------------\n"); *\/ */
/* 			break; */
/* 		} */
/* 		case 0x5: */
/* 		{ */
/* 			reqr = remaining_blob_alloc(2); */
/* 			FILL_FIRST_BYTE(reqr, remcode, 4); */
/* 			break; */
/* 		} */
/* 		case 0x12: */
/* 		{ */
/* 			if (opnum == 0x4) { */
/* 				reqr = remaining_blob_alloc(1); */
/* 				FILL_FIRST_BYTE(reqr, previous, length); */
/* 			} */
/* 			if (opnum == 0x5) { */
/* 				reqr = remaining_blob_alloc(3); */
/* 				SWAP_BYTES(reqr, previous, length); */
/* 			} */
/* 			if (opnum == 0x12) { */
/* 				reqr = remaining_blob_alloc(2); */
/* 				SWAP_BYTES(reqr, previous, length); */
/* 			} */
/* 			break; */
/* 		} */
/* 		case 0x18: */
/* 		{ */
/* 			reqr = remaining_blob_alloc(1); */
/* 			FILL_WITH_FIRST_BYTE(reqr, previous); */
/* /\* 			FILL_FIRST_BYTE(reqr, previous, length); *\/ */

/* 			printf("--------0x5----------\n"); */
/* 			dump_data(1, remcode_0x5, 4); */
/* 			printf("------------------\n"); */
/* /\* 			FILL_FIRST_BYTE(reqr, remcode_0x5, 4); *\/ */
/* 			break; */
/* 		} */
/* 		case 0x29: */
/* 		{ */
/* 			if (opnum == 0xFE || opnum == 0x12) { */
/* 				reqr = remaining_blob_alloc(9); */
/* 				FILL_FIRST_BYTE(reqr, previous, length); */
/* 			} */
/* 			else { */
/* 				if (lock != True) { */
/* 					FILL_FIRST_BYTE(remcode_0x5, previous, length); */
/* 					printf("--------0x5----------\n"); */
/* 					dump_data(1, remcode_0x5, 4); */
/* 					printf("------------------\n"); */
/* 					lock = True; */
/* 				} */
/* 				reqr = remaining_blob_alloc(8); */
/* 				CONCAT_BYTES(reqr, previous, length, (8 * 4)); */
/* 				FILL_FIRST_BYTE(remcode, previous, length); */
/* 				printf("------------------\n"); */
/* 				dump_data(1, remcode, 4); */
/* 				printf("------------------\n"); */
/* 			} */
/* 			break; */
/* 		} */
/* 		} */
/* 	} */

/* 	/\* For the moment we store the previous "opnum" in order to define the next remaining data size *\/ */
/* 	printf("opnum = 0x%.04x\n", opnum); */
/* /\* 	opnum = (current.content[1] << 8) | current.content[0]; *\/ */
/* 	opnum = current.opnum; */

	return (reqr);
}

static struct opnums mapi_opnums[] = {
	{OPNUM_MAPI_SETCOLUMNS,		"MAPI_SETCOLUMNS"},
	{OPNUM_MAPI_QUERYROWS,		"MAPI_QUERYROWS"},
	{OPNUM_MAPI_OPEN_MSGSTORE,	"MAPI_OPEN_MSGSTORE"},
	{0x0,				NULL}
};


/*
  retrieves mapi opnum name
 */

const char *get_mapi_opname(uint8_t opnum)
{
	uint32_t		i;

	for (i = 0; mapi_opnums[i].name != NULL; i++) {
		if (mapi_opnums[i].opnum == opnum) {
			return mapi_opnums[i].name;
		}
	}
	return OPNUM_Unknown;
}

/*
  check if the given mapi opnum is supported
*/

BOOL check_mapi_opnum(uint8_t opnum)
{
	uint32_t		i;

	for (i = 0; mapi_opnums[i].name != NULL; i++) {
		if (mapi_opnums[i].opnum == opnum) {
			return True;
		}
	}
	return False;
}

/*
  retrieves mapi opnum
 */

uint8_t get_mapi_opnum(const char *name)
{
	uint32_t		i;

	if (!name) {
		return -1;
	}
	for (i = 0; mapi_opnums[i].name != NULL; i++) {
		if (!strcmp(mapi_opnums[i].name, name)) {
			return mapi_opnums[i].opnum;
		}
	}
	return -1;
}
