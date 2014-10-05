/*
   OpenChange MAPI implementation.

   This work is based on libpst-0.5.2, and the author(s) of
   that code will also hold appropriate copyrights.

   Copyright (C) Julien Kerihuel 2007-2011.

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

#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"

#include <ctype.h>
 
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
	enum MAPISTATUS		retval;
	struct mapi_context	*mapi_ctx;
	struct mapi_session	*session;
	TALLOC_CTX		*mem_ctx;
	uint32_t		in_size;
	uint8_t			*rtfcomp;
	uint16_t		read_size;
	unsigned char		buf[0x1000];

	/* sanity check and init */
	OPENCHANGE_RETVAL_IF(!obj_stream, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_stream);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = session->mapi_ctx;
	OPENCHANGE_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = mapi_ctx->mem_ctx;

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

typedef struct _decompression_state {
	uint8_t*	dict;
	uint32_t	dict_writeoffset;
	uint8_t*	compressed_data;
	uint32_t	in_size;
	uint32_t	in_pos;
} decompression_state;

/**
  Initialise the decompression_state
  
  \param mem_ctx the memory context to allocate the decompression state on
  \param dict the resulting decompression_state
*/
static void initialise_decompression_state(TALLOC_CTX *mem_ctx, uint8_t *compressed_data,
					   uint32_t in_size, decompression_state *state)
{
	state->dict = talloc_array(mem_ctx, uint8_t, LZFU_DICTLENGTH);

	memcpy(state->dict, LZFU_INITDICT, LZFU_INITLENGTH);
	state->dict_writeoffset = LZFU_INITLENGTH;
	
	state->compressed_data = compressed_data;
	state->in_size = in_size;
	state->in_pos = LZFU_HEADERLENGTH;
}

static void cleanup_decompression_state(decompression_state *state)
{
	talloc_free(state->dict);
}

typedef struct _output_state {
	uint32_t	out_size;
	uint32_t	out_pos;
	DATA_BLOB	*output_blob;
} output_state;

static void initialise_output_state(TALLOC_CTX *mem_ctx, output_state *state, uint32_t rawSize, DATA_BLOB *output_blob)
{
	state->out_pos = 0;
	state->out_size = rawSize + LZFU_HEADERLENGTH + 4;
	output_blob->data = (uint8_t *) talloc_size(mem_ctx, state->out_size);
	output_blob->length = 0;
	state->output_blob = output_blob;
}

static void parse_header(uint8_t *header_data, lzfuheader *header)
{
	memcpy(header, header_data, sizeof(*header));
	LE32_CPU(header->cbSize);   
	LE32_CPU(header->cbRawSize);
	LE32_CPU(header->dwMagic);  
	LE32_CPU(header->dwCRC);

	/*
	DEBUG(2, ("COMPSIZE = 0x%x\n", header->cbSize));
	DEBUG(2, ("RAWSIZE = 0x%x\n", header->cbRawSize));
	DEBUG(2, ("COMPTYPE = 0x%08x\n", header->dwMagic)); // TODO: make this look like MS-OXRTFCP examples
	DEBUG(2, ("CRC = 0x%08x\n", header->dwCRC));
	*/
}

static enum MAPISTATUS verify_header(uint8_t *header_data, uint32_t in_size, lzfuheader *header)
{
	parse_header(header_data, header);

	if (header->cbSize != in_size - 4) {
		DEBUG(0, ("in_size mismatch:%u\n", in_size));
		OPENCHANGE_RETVAL_ERR(MAPI_E_CORRUPT_DATA, NULL);
	}

	if ((header->dwMagic != LZFU_COMPRESSED) && (header->dwMagic != LZFU_UNCOMPRESSED)) {
		DEBUG(0, ("bad magic: 0x%x\n", header->dwMagic));
		OPENCHANGE_RETVAL_ERR(MAPI_E_CORRUPT_DATA, NULL);
	}
	
	return MAPI_E_SUCCESS;
}

static uint8_t get_next_byte(decompression_state *state)
{
	if (state->in_pos > state->in_size) {
		return 0;
	}  
	uint8_t next_byte = state->compressed_data[state->in_pos];
	state->in_pos += 1;
	return next_byte;
}

static uint8_t get_next_control(decompression_state *state)
{
	uint8_t c = get_next_byte(state);
	/* DEBUG(3, ("control: 0x%02x\n", c)); */
	return c;
}

static uint8_t get_next_literal(decompression_state *state)
{
	uint8_t c = get_next_byte(state);
	/* if (isprint(c)) {
		DEBUG(3, ("literal %c\n", c));
	} else {
		DEBUG(3, ("literal 0x%02x\n", c));
		} */
	return c;
}

typedef struct _dictionaryref {
	uint8_t		length;
	uint16_t	 offset;
} dictionaryref;

static dictionaryref get_next_dictionary_reference(decompression_state *state)
{
	dictionaryref reference;
	uint8_t highbyte = get_next_byte(state);
	uint8_t lowbyte = get_next_byte(state);
	reference.length = lowbyte & 0x0F; /* low 4 bits are length */
	reference.length += 2; /* stored as two less than actual length */
	reference.offset = ((highbyte << 8) + lowbyte);
	reference.offset &= 0xFFF0; /* high 12 bits are offset */
	reference.offset >>= 4; /* shift the offset down */
	return reference;
}

static void append_to_dictionary(decompression_state *state, char c)
{
	state->dict[state->dict_writeoffset] = c;
	state->dict_writeoffset = (state->dict_writeoffset + 1) % LZFU_DICTLENGTH;
}

static void append_to_output(output_state *output, char c)
{
	output->output_blob->data[output->out_pos] = c;
	output->out_pos += 1;
	output->output_blob->length += 1;
}

static char get_latest_literal_in_output(output_state output)
{
        if (!output.out_pos) return 0;

        return output.output_blob->data[output.out_pos-1];
}

static char get_dictionary_entry(decompression_state *state, uint32_t index)
{
	char c = state->dict[index % LZFU_DICTLENGTH];
	/* if (isprint(c)) {
		DEBUG(3, ("dict entry %i: %c\n", index, c));
	} else {
		DEBUG(3, ("dict entry 0x%04x: 0x%02x\n", index, c));
		} */
	return c;
}

static bool output_would_overflow(output_state *output)
{
	bool would_overflow = (output->out_pos > output->out_size);
	if (would_overflow) {
		DEBUG(0, (" overrun on out_pos: %u > %u\n", output->out_pos, output->out_size));
		DEBUG(0, (" overrun data: %s\n", output->output_blob->data));
	}
	return would_overflow;
}

static bool input_would_overflow(decompression_state *state)
{
	bool would_overflow = (state->in_pos > state->in_size);
	if (would_overflow) {
		DEBUG(0, ("input overrun at in_pos: %i (of %i)\n", state->in_pos, state->in_size));
	}
	return would_overflow;
}

_PUBLIC_ enum MAPISTATUS uncompress_rtf(TALLOC_CTX *mem_ctx, 
					 uint8_t *rtfcomp, uint32_t in_size,
					 DATA_BLOB *rtf)
{
	lzfuheader		lzfuhdr;
	decompression_state	state;
	uint8_t			bitmask_pos;
	output_state		output;

	enum MAPISTATUS		retval;

	if (in_size < sizeof(lzfuhdr)+1) {
		OPENCHANGE_RETVAL_ERR(MAPI_E_CORRUPT_DATA, NULL);
	}

	initialise_decompression_state(mem_ctx, rtfcomp, in_size, &state);

	retval = verify_header(rtfcomp, state.in_size, &lzfuhdr);
	if (retval != MAPI_E_SUCCESS) {
		cleanup_decompression_state(&state);
		return retval;
	}

	if (lzfuhdr.dwMagic == LZFU_UNCOMPRESSED) {
		// TODO: handle uncompressed case
	}

	initialise_output_state(mem_ctx, &output, lzfuhdr.cbRawSize, rtf);

	while ((state.in_pos + 1) < state.in_size) {
		uint8_t control = get_next_control(&state);
		for(bitmask_pos = 0; bitmask_pos < 8; ++bitmask_pos) {
			if (control & ( 1 << bitmask_pos)) { /* its a dictionary reference */
				dictionaryref dictref;
				int i;
				dictref = get_next_dictionary_reference(&state);
				if (dictref.offset == state.dict_writeoffset) {
					DEBUG(4, ("matching offset - done\n"));
                                        /* Do not add \0 twice */
                                        if (get_latest_literal_in_output(output)) {
                                                append_to_output(&output, '\0');
                                        }
					cleanup_decompression_state(&state);
					return MAPI_E_SUCCESS;
				}
				for (i = 0; i < dictref.length; ++i) {
					if (output_would_overflow(&output)) {
						cleanup_decompression_state(&state);
						return MAPI_E_CORRUPT_DATA;
					}
					char c = get_dictionary_entry(&state, (dictref.offset + i));
					append_to_output(&output, c);
					append_to_dictionary(&state, c);
				}
			} else { /* its a literal */
				if ( output_would_overflow(&output) || input_would_overflow(&state) ) {
					cleanup_decompression_state(&state);
					talloc_free(rtf->data);
					return MAPI_E_CORRUPT_DATA;
				}
				char c = get_next_literal(&state);
				append_to_output(&output, c);
				append_to_dictionary(&state, c);
			}
		}
	}
	
	cleanup_decompression_state(&state);

	OPENCHANGE_RETVAL_ERR(MAPI_E_SUCCESS, NULL);
}

static uint32_t CRCTable[] = {
0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t calculateCRC(uint8_t *input, uint32_t offset, uint32_t length)
{
	uint32_t crc = 0;
	uint32_t i;
	for (i = offset; i < offset+length; ++i) {
		DEBUG(5, ("input at %i: 0x%02x\n", i, input[i]));
		uint8_t table_position = (crc ^ input[i]) & 0xFF;
		DEBUG(5, ("table_position: 0x%02x\n", table_position));
		uint32_t intermediateValue = crc >> 8;
		DEBUG(5, ("intermediateValue: 0x%08x\n", intermediateValue));
		crc = CRCTable[table_position] ^ intermediateValue;
		DEBUG(5, ("tableValue: 0x%08x\n", CRCTable[table_position]));
		DEBUG(5, ("crc: 0x%08x\n", crc));
	}
	return crc;
}
#define MIN(a,b) ((a) < (b) ? (a) : (b))

static size_t longest_match(const char *rtf, const size_t rtf_size, size_t input_idx, uint8_t *dict, size_t *dict_write_idx, size_t *dict_match_offset, size_t *dict_match_length)
{
	size_t best_match_length = 0;
	size_t dict_iterator;
	for (dict_iterator = 0; dict_iterator < MIN(*dict_write_idx, LZFU_DICTLENGTH); ++dict_iterator) {
		size_t match_length_from_this_pos = 0;
		while ((rtf[input_idx + match_length_from_this_pos] == dict[dict_iterator + match_length_from_this_pos]) &&
		       ((dict_iterator + match_length_from_this_pos) < ((*dict_write_idx) % LZFU_DICTLENGTH)) && /* does this line need to have % LZFU_DICTLENGTH? */
		       ((input_idx + match_length_from_this_pos) < rtf_size) && 
		       (match_length_from_this_pos < 17)) {
			match_length_from_this_pos += 1;
			if (match_length_from_this_pos > best_match_length) {
				best_match_length = match_length_from_this_pos;
				dict[(*dict_write_idx) % LZFU_DICTLENGTH] = rtf[input_idx + match_length_from_this_pos - 1];
				*dict_write_idx += 1;
				*dict_match_offset = dict_iterator;
			}
		}
	}
	*dict_match_length = best_match_length;
	return best_match_length;
}

_PUBLIC_ enum MAPISTATUS compress_rtf(TALLOC_CTX *mem_ctx, const char *rtf, const size_t rtf_size,
				      uint8_t **rtfcomp, size_t *rtfcomp_size)
{
	size_t			input_idx = 0;
	lzfuheader		header;
	uint8_t			*dict;
	size_t			output_idx = 0;
	size_t			control_byte_idx = 0;
	uint8_t			control_bit = 0x01;
	size_t			dict_write_idx = 0;

	/* as an upper bound, assume that the output is no larger than 9/8 of the input size, plus the header size */
	*rtfcomp = (uint8_t *) talloc_size(mem_ctx, 9 * rtf_size / 8 + sizeof(lzfuheader));
	control_byte_idx = sizeof(lzfuheader);
	(*rtfcomp)[control_byte_idx] = 0x00;
	output_idx = control_byte_idx + 1;

	/* allocate and initialise the dictionary */
	dict = talloc_zero_array(mem_ctx, uint8_t, LZFU_DICTLENGTH);
	memcpy(dict, LZFU_INITDICT, LZFU_INITLENGTH);
	dict_write_idx = LZFU_INITLENGTH;

	while (input_idx < rtf_size) {
		size_t dict_match_length = 0;
		size_t dict_match_offset = 0;
		DEBUG(4, ("compressing byte %zi of %zi\n", input_idx, rtf_size));
		if (longest_match(rtf, rtf_size, input_idx, dict, &dict_write_idx, &dict_match_offset, &dict_match_length) > 1) {
			uint16_t dict_ref = dict_match_offset << 4;
			dict_ref += (dict_match_length - 2);
			input_idx += dict_match_length;
			(*rtfcomp)[control_byte_idx] |= control_bit;
			/* append dictionary reference to output */
			(*rtfcomp)[output_idx] = (dict_ref & 0xFF00) >> 8;
			output_idx += 1;
			(*rtfcomp)[output_idx] = (dict_ref & 0xFF);
			output_idx += 1;
		} else {
			if (dict_match_length == 0) {
				/* we haven't written a literal to the dict yet */
				dict[dict_write_idx % LZFU_DICTLENGTH] = rtf[input_idx];
				dict_write_idx += 1;
			}
			/* append to output, and increment the output position */
			(*rtfcomp)[output_idx] = rtf[input_idx];
			output_idx += 1;
			DEBUG(5, ("new output_idx = 0x%08zx (for char value 0x%02x)\n", output_idx, rtf[input_idx]));
			/* increment the input position */
			input_idx += 1;
		}
		if (control_bit == 0x80) {
			control_bit = 0x01;
			control_byte_idx = output_idx;
			(*rtfcomp)[control_byte_idx] = 0x00;
			output_idx = control_byte_idx + 1;
			DEBUG(5, ("new output_idx cb = 0x%08zx\n", output_idx));
		} else {
			control_bit = control_bit << 1;
		}
	}
	{
		/* append final marker dictionary reference to output */
		uint16_t dict_ref = (dict_write_idx % LZFU_DICTLENGTH) << 4;
		// printf("dict ref: 0x%04x at 0x%08zx\n", dict_ref, output_idx);
		(*rtfcomp)[control_byte_idx] |= control_bit;
		// printf("dict ref hi: 0x%02x\n", (dict_ref & 0xFF00) >> 8);
		(*rtfcomp)[output_idx] = (dict_ref & 0xFF00) >> 8;
		output_idx += 1;
		// printf("dict ref lo: 0x%02x\n", dict_ref & 0xFF);
		(*rtfcomp)[output_idx] = (dict_ref & 0xFF);
		output_idx += 1;
	}

	header.cbSize = output_idx - sizeof(lzfuheader) + 12;
	header.cbRawSize = rtf_size;
	header.dwMagic = LZFU_COMPRESSED;
	header.dwCRC = calculateCRC(*rtfcomp, sizeof(lzfuheader), output_idx - sizeof(lzfuheader));
	memcpy(*rtfcomp, &header, sizeof(lzfuheader));
	*rtfcomp_size = output_idx;
	*rtfcomp = (uint8_t *) talloc_realloc_size(mem_ctx, *rtfcomp, *rtfcomp_size);

	return MAPI_E_SUCCESS;
}
