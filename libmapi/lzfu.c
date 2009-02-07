/*
   OpenChange MAPI implementation.

   This work is based on libpst-0.5.2, and the author(s) of
   that code will also hold appropriate copyrights.

   Copyright (C) Julien Kerihuel 2007-2008.

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
#include <libmapi/proto_private.h>

/**
   \file lzfu.c
   
   \brief Compressed RTF related functions
*/


#if BYTE_ORDER == BIG_ENDIAN
#define LE32_CPU(x)		    \
  x = ((((x) & 0xff000000) >> 24) | \
       (((x) & 0x00ff0000) >> 8 ) | \
       (((x) & 0x0000ff00) << 8 ) | \
       (((x) & 0x000000ff) << 24));
#define LE16_CPU(x)	       \
  x = ((((x) & 0xff00) >> 8) | \
       (((x) & 0x00ff) << 8));
#elif BYTE_ORDER == LITTLE_ENDIAN
#define	LE32_CPU(x) {}
#define	LE16_CPU(x) {}
#else
#error Byte order not supported
#endif /* BYTE_ORDER */

#define	LZFU_COMPRESSED		0x75465a4c
#define	LZFU_UNCOMPRESSED	0x414c454d

/* Initial directory */
#define LZFU_INITDICT					\
  "{\\rtf1\\ansi\\mac\\deff0\\deftab720{\\fonttbl;}"	\
  "{\\f0\\fnil \\froman \\fswiss \\fmodern \\fscrip"	\
  "t \\fdecor MS Sans SerifSymbolArialTimes Ne"		\
  "w RomanCourier{\\colortbl\\red0\\green0\\blue0"	\
  "\r\n\\par \\pard\\plain\\f0\\fs20\\b\\i\\u\\tab"	\
  "\\tx"

/* initial length of dictionary */
#define LZFU_INITLENGTH		207

#define	LZFU_DICTLENGTH		0x1000
#define	LZFU_HEADERLENGTH	0x10

/* header for compressed rtf */
typedef struct _lzfuheader {
	uint32_t	cbSize;
	uint32_t	cbRawSize;
	uint32_t	dwMagic;
	uint32_t	dwCRC;
} lzfuheader;


/**
   \details creates a DATA_BLOB in uncompressed Rich Text Format (RTF)
   from the compressed format used in the PR_RTF_COMPRESSED property
   opened in the stream.

   \param obj_stream stream object with RTF stream content
   \param rtf the output blob with uncompressed content

   \return MAPI_E_SUCCESS on success, otherwise MAPI error. Possible
   MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_stream is not a valid pointer
   - MAPI_E_CORRUPT_DATA: a problem was encountered while
     decompressing the RTF compressed data
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code.
 
   \note rtf->data needs to be freed with MAPIFreeBuffer

   \sa OpenStream
*/
_PUBLIC_ enum MAPISTATUS WrapCompressedRTFStream(mapi_object_t *obj_stream, 
						 DATA_BLOB *rtf)
{
	enum MAPISTATUS	retval;
	TALLOC_CTX	*mem_ctx;
	uint32_t	in_size;
	uint8_t		*rtfcomp;
	uint16_t	read_size;
	unsigned char	buf[0x1000];

	/* sanity check and init */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_stream, MAPI_E_INVALID_PARAMETER, NULL);
	mem_ctx = global_mapi_ctx->mem_ctx;

	/* Read the stream pointed by obj_stream */
	read_size = 0;
	in_size = 0;
	rtfcomp = talloc_zero(mem_ctx, uint8_t);
	do {
		retval = ReadStream(obj_stream, buf, 0x1000, &read_size);
		OPENCHANGE_RETVAL_IF(retval, GetLastError(), rtf->data);
		if (read_size) {
			rtfcomp = talloc_realloc(mem_ctx, rtfcomp, uint8_t, 
						   in_size + read_size);
			memcpy(&(rtfcomp[in_size]), buf, read_size);
			in_size += read_size;
		}
	} while (read_size);

	return uncompress_rtf(mem_ctx, rtfcomp, in_size, rtf);
}

_PUBLIC_ enum MAPISTATUS uncompress_rtf(TALLOC_CTX *mem_ctx, 
					 uint8_t *rtfcomp, uint32_t in_size,
					 DATA_BLOB *rtf)
{
	lzfuheader	lzfuhdr;
	uint8_t		dict[4096];
	uint32_t	out_size = 0;
	uint32_t	in_pos = 16;
	uint32_t	out_pos = 0;
	uint32_t	dict_writeoffset = LZFU_INITLENGTH;
	uint8_t		bitmask_pos;

	if (in_size < sizeof(lzfuhdr)+1) {
		OPENCHANGE_RETVAL_ERR(MAPI_E_CORRUPT_DATA, NULL);
	}

	memcpy(dict, LZFU_INITDICT, LZFU_INITLENGTH);

	memcpy(&lzfuhdr, rtfcomp, sizeof(lzfuhdr));
	LE32_CPU(lzfuhdr.cbSize);   
	LE32_CPU(lzfuhdr.cbRawSize);
	LE32_CPU(lzfuhdr.dwMagic);  
	LE32_CPU(lzfuhdr.dwCRC);

#if 0
	printf("lzfuhdr.cbSize = %u\n", lzfuhdr.cbSize);
	printf("lzfuhdr.cbRawSize = %u\n", lzfuhdr.cbRawSize);
	printf("lzfuhdr.dwMagic = 0x%x\n", lzfuhdr.dwMagic);
	printf("lzfuhdr.dwCRC = 0x%x\n", lzfuhdr.dwCRC);
#endif

	if (lzfuhdr.cbSize != in_size - 4) {
		printf("in_size mismatch:%u\n", in_size);
		OPENCHANGE_RETVAL_ERR(MAPI_E_CORRUPT_DATA, NULL);
	}

	if ((lzfuhdr.dwMagic != LZFU_COMPRESSED) && (lzfuhdr.dwMagic != LZFU_UNCOMPRESSED)) {
		printf("bad magic: 0x%x\n", lzfuhdr.dwMagic);
		OPENCHANGE_RETVAL_ERR(MAPI_E_CORRUPT_DATA, NULL);
	}

	if (lzfuhdr.dwMagic == LZFU_UNCOMPRESSED) {
		// TODO: handle uncompressed case
	}

	out_size = lzfuhdr.cbRawSize + LZFU_HEADERLENGTH + 4;
	rtf->data = talloc_size(mem_ctx, out_size);

	while ((in_pos + 1) < in_size) {
		uint8_t control;
		control = rtfcomp[in_pos];
		++ in_pos;
		// printf("control: 0x%x\n", control);

		for(bitmask_pos = 0; bitmask_pos < 8; ++bitmask_pos) {
			if (in_pos > in_size) {
				break;
			}
			if (control & ( 1 << bitmask_pos)) {
				// it is a dictionary reference
				uint8_t dictreflength;
				uint16_t dictrefoffset;
				int i;
				dictrefoffset= (rtfcomp[in_pos] << 8) + rtfcomp[in_pos + 1];
				in_pos += 2; /* for the two bytes we just consumed */
				dictreflength = dictrefoffset & 0x000F; /* low 4 bits */
				dictreflength += 2; /* stored as two less than actual length */
				dictrefoffset &= 0xFFF0; /* high twelve bits */
				dictrefoffset >>= 4;
				if (dictrefoffset == dict_writeoffset) {
					rtf->length = out_pos;
					OPENCHANGE_RETVAL_ERR(MAPI_E_SUCCESS, NULL);
				}
				for (i = 0; i < dictreflength; ++i) {
					if (out_pos > out_size ) {
						printf(" overrun on out_pos: %u > %u\n", out_pos, out_size);
						printf(" overrun data: %s\n", rtf->data);
						OPENCHANGE_RETVAL_ERR(MAPI_E_CORRUPT_DATA, rtf->data);
					}
					char c = dict[(dictrefoffset + i) % LZFU_DICTLENGTH];
					rtf->data[out_pos] = c;
					dict[dict_writeoffset] = c;
					dict_writeoffset = (dict_writeoffset + 1) % LZFU_DICTLENGTH;
					++ out_pos;
				}
			} else {
				// its a literal
				if ( (out_pos > out_size) || (in_pos > in_size) ) {
					OPENCHANGE_RETVAL_ERR(MAPI_E_CORRUPT_DATA, rtf->data);
				}
				char c = rtfcomp[in_pos];
				++ in_pos;
				rtf->data[out_pos] = c;
				dict[dict_writeoffset] = c;
				dict_writeoffset = (dict_writeoffset + 1) % LZFU_DICTLENGTH;
				++ out_pos;
			}
		}
	}

	rtf->length = lzfuhdr.cbRawSize;

	OPENCHANGE_RETVAL_ERR(MAPI_E_SUCCESS, NULL);

}
