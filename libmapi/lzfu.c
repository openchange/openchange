/*
   OpenChange MAPI implementation.

   This work is based on libpst-0.5.2, and the author(s) of
   that code will also hold appropriate copyrights.

   Copyright (C) Julien Kerihuel 2007.

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

/**
   \file lzfu.c
   
   \brief RTF Compressed related functions
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

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_stream is not a valid pointer
   - MAPI_E_CORRUPT_DATA: a problem was encountered while
     decompressing the RTF compressed data
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction

   \note rtf->data needs to be freed with MAPIFreeBuffer

   \sa OpenStream
*/
_PUBLIC_ enum MAPISTATUS WrapCompressedRTFStream(mapi_object_t *obj_stream, 
						 DATA_BLOB *rtf)
{
	enum MAPISTATUS	retval;
	TALLOC_CTX	*mem_ctx;
	uint8_t		dict[4096];
	uint32_t	dict_length = 0;
	lzfuheader	lzfuhdr;
	uint8_t		flags;
	uint8_t		flag_mask;
	uint8_t		i;
	uint32_t	in_size;
	uint8_t		*rtfcomp;
	uint8_t		*out_buf;
	uint32_t	out_ptr = 0;
	uint32_t	read_size;
	unsigned char	buf[0x1000];

	/* sanity check and init */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_stream, MAPI_E_INVALID_PARAMETER, NULL);
	mem_ctx = global_mapi_ctx->mem_ctx;

	/* Read the stream pointed by obj_stream */
	read_size = 0;
	in_size = 0;
	rtfcomp = talloc_zero(mem_ctx, uint8_t);
	do {
		retval = ReadStream(obj_stream, buf, 0x1000, &read_size);
		MAPI_RETVAL_IF(retval, GetLastError(), rtf->data);
		if (read_size) {
			rtfcomp = talloc_realloc(mem_ctx, rtfcomp, uint8_t, 
						   in_size + read_size);
			memcpy(&(rtfcomp[in_size]), buf, read_size);
			in_size += read_size;
		}
	} while (read_size);

	memcpy(dict, LZFU_INITDICT, LZFU_INITLENGTH);
	dict_length = LZFU_INITLENGTH;

	memcpy(&lzfuhdr, rtfcomp, sizeof(lzfuhdr));
	LE32_CPU(lzfuhdr.cbSize);   
	LE32_CPU(lzfuhdr.cbRawSize);
	LE32_CPU(lzfuhdr.dwMagic);  
	LE32_CPU(lzfuhdr.dwCRC);

	DEBUG(3, ("lzfuhdr.cbSize = %d\n", lzfuhdr.cbSize));
	DEBUG(3, ("lzfuhdr.cbRawSize = %d\n", lzfuhdr.cbRawSize));
	DEBUG(3, ("lzfuhdr.dwMagic = 0x%x\n", lzfuhdr.dwMagic));
	DEBUG(3, ("lzfuhdr.dwCRC = 0x%x\n", lzfuhdr.dwCRC));

	out_buf = (unsigned char *) talloc_size(mem_ctx, lzfuhdr.cbRawSize + 
						LZFU_HEADERLENGTH + 4);
	MAPI_RETVAL_IF(!out_buf, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);

	in_size = LZFU_HEADERLENGTH;

	while ((in_size < (lzfuhdr.cbSize - 1)) && 
	       (out_ptr < (lzfuhdr.cbRawSize + LZFU_HEADERLENGTH + 4))) {
		memcpy(&flags, &(rtfcomp[in_size]), 1);
		in_size += 1;

		flag_mask = 1;
		while (flag_mask != 0 && (in_size < (lzfuhdr.cbSize - 1)) &&
		       (out_ptr < (lzfuhdr.cbRawSize + LZFU_HEADERLENGTH + 4))) {
			if (flag_mask & flags) {
				/* read 2 bytes from input */
				unsigned short int blkhdr;
				unsigned short int offset;
				unsigned short int length;

				memcpy(&blkhdr, &(rtfcomp[in_size]), 2);
				LE16_CPU(blkhdr);	
				in_size += 2;

				/* swap the upper and lower bytes of blkhdr */
				blkhdr = (((blkhdr & 0xFF00) >> 8) +
					  ((blkhdr & 0x00FF) << 8));

				/* the offset is the first 24 bits of the 32 bit value */
				offset = (blkhdr & 0xFFF0) >> 4;

				/* the length of the dict entry are the last 8 bits */
				length = (blkhdr & 0x000F) + 2;

				/* add the value we are about to print to the dictionary */
				for (i = 0; i < length; i++) {
					unsigned char c1;
					c1 = dict[(offset + i) % 4096];
					dict[dict_length] = c1;
					dict_length = (dict_length + 1) % 4096;
					out_buf[out_ptr++] = c1;
				}
			} else {
				/* uncompressed chunk (single byte) */
				char c1 = rtfcomp[in_size];
				in_size++;
				dict[dict_length] = c1;
				dict_length = (dict_length + 1) % 4096;
				out_buf[out_ptr++] = c1;
			}
			flag_mask <<= 1;
		}
	}

	talloc_free(rtfcomp);
	
	/* the compressed version doesn't appear to drop the closing braces onto the doc.
	 * we should do that
	 */
	out_buf[out_ptr++] = '}';
	out_buf[out_ptr++] = '}';
	out_buf[out_ptr++] = '\0';

	/* check if out_ptr matches with expected size */
	MAPI_RETVAL_IF(out_ptr != lzfuhdr.cbRawSize, 
		       MAPI_E_CORRUPT_DATA, out_buf);

	rtf->data = out_buf;
	rtf->length = out_ptr;

	return MAPI_E_SUCCESS;
}

