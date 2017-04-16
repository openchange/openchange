/*
   OpenChange implementation.

   libndr mapi support

   Copyright (C) Julien Kerihuel 2005-2014

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

#include <ctype.h>
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include <ndr.h>
#include "gen_ndr/ndr_exchange.h"
#include "gen_ndr/ndr_property.h"

#define MIN(a,b) ((a)<(b)?(a):(b))

_PUBLIC_ void obfuscate_data(uint8_t *data, uint32_t size, uint8_t salt)
{
	uint32_t i;

	for (i=0; i<size; i++) {
		data[i] ^= salt;
	}
}

ssize_t lzxpress_compress(const uint8_t *input,
			  uint32_t input_size,
			  uint8_t *output,
			  uint32_t max_output_size);

ssize_t lzxpress_decompress(const uint8_t *input,
			    uint32_t input_size,
			    uint8_t *output,
			    uint32_t max_output_size);

/**
   \details Compress a LZXPRESS chunk

   \param ndrpush pointer to the compressed data to return
   \param ndrpull pointer to the uncompressed data used for compression
   \param last pointer on boolean to define whether or not this is the
   last chunk

   \return NDR_ERR_SUCCESS on success, otherwise NDR error
 */
static enum ndr_err_code ndr_push_lxpress_chunk(struct ndr_push *ndrpush,
						struct ndr_pull *ndrpull,
						bool *last)
{
	DATA_BLOB	comp_chunk;
	DATA_BLOB	plain_chunk;
	uint32_t	plain_chunk_size;
	uint32_t	plain_chunk_offset;
	uint32_t	max_plain_size = 0x00010000;
	uint32_t	max_comp_size = 0x00020000 + 2; /* TODO: use the correct value here */
	ssize_t		ret;

	/* Step 1. Retrieve the uncompressed buf */
	plain_chunk_size = MIN(max_plain_size, ndrpull->data_size - ndrpull->offset);
	plain_chunk_offset = ndrpull->offset;
	NDR_CHECK(ndr_pull_advance(ndrpull, plain_chunk_size));

	plain_chunk.data = ndrpull->data + plain_chunk_offset;
	plain_chunk.length = plain_chunk_size;

	if (plain_chunk_size < max_plain_size) {
		*last = true;
	};

	NDR_CHECK(ndr_push_expand(ndrpush, max_comp_size));

	comp_chunk.data = ndrpush->data + ndrpush->offset;
	comp_chunk.length = max_comp_size;

	/* Compressing the buffer using LZ Xpress algorithm */
	ret = lzxpress_compress(plain_chunk.data,
				plain_chunk.length,
				comp_chunk.data,
				comp_chunk.length);

	if (ret < 0) {
		return ndr_pull_error(ndrpull, NDR_ERR_COMPRESSION,
				      "XPRESS lzxpress_compress() returned %d\n",
				      (int)ret);
	}
	comp_chunk.length = ret;

	ndrpush->offset += comp_chunk.length;
	return NDR_ERR_SUCCESS;
}

static enum ndr_err_code ndr_pull_lxpress_chunk(struct ndr_pull *ndrpull,
						struct ndr_push *ndrpush,
						ssize_t decompressed_len,
						bool *last)
{
	DATA_BLOB	comp_chunk;
	DATA_BLOB	plain_chunk;
	uint32_t	plain_chunk_offset;
	int		ret;

	/* Step 1. Retrieve the compressed buf */
	comp_chunk.length = ndrpull->data_size;
	comp_chunk.data = ndrpull->data;

	plain_chunk_offset = ndrpush->offset;
	NDR_CHECK(ndr_push_zero(ndrpush, decompressed_len));
	plain_chunk.length = decompressed_len;
	plain_chunk.data = ndrpush->data + plain_chunk_offset;

	ret = lzxpress_decompress(comp_chunk.data,
				  comp_chunk.length,
				  plain_chunk.data,
				  plain_chunk.length);
	if (ret < 0) {
		return ndr_pull_error(ndrpull, NDR_ERR_COMPRESSION,
				      "XPRESS lzxpress_decompress() returned %d\n",
				      (int)ret);
	}
	plain_chunk.length = ret;
	ndrpush->offset = ret;

	if ((decompressed_len < 0x00010000) || (ndrpull->offset+4 >= ndrpull->data_size)) {
		/* this is the last chunk */
		*last = true;
	}

	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_lzxpress_decompress(struct ndr_pull *subndr,
							struct ndr_pull **_comndr,
							ssize_t decompressed_len)
{
	struct ndr_push *ndrpush;
	struct ndr_pull *comndr;
	DATA_BLOB	uncompressed;
	bool		last = false;

	ndrpush = ndr_push_init_ctx(subndr);
	NDR_ERR_HAVE_NO_MEMORY(ndrpush);

	while (!last) {
		NDR_CHECK(ndr_pull_lxpress_chunk(subndr, ndrpush, decompressed_len, &last));
	}

	uncompressed = ndr_push_blob(ndrpush);
	if (uncompressed.length != decompressed_len) {
		return ndr_pull_error(subndr, NDR_ERR_COMPRESSION,
				      "Bad uncompressed_len [%u] != [%u](0x%08X) (PULL)",
				      (int)uncompressed.length,
				      (int)decompressed_len,
				      (int)decompressed_len);
	}

	comndr = talloc_zero(subndr, struct ndr_pull);
	NDR_ERR_HAVE_NO_MEMORY(comndr);
	comndr->flags = subndr->flags;
	comndr->current_mem_ctx = subndr->current_mem_ctx;
	comndr->data = uncompressed.data;
	comndr->data_size = uncompressed.length;
	comndr->offset = 0;

	*_comndr = comndr;
	return NDR_ERR_SUCCESS;
}

/**
   \details Push a compressed LZXPRESS blob

   \param subndr pointer to the compressed blob the function returns
   \param _uncomndr pointer on pointer to the uncompressed DATA blob

   \return NDR_ERR_SUCCESS on success, otherwise NDR error
 */
_PUBLIC_ enum ndr_err_code ndr_push_lzxpress_compress(struct ndr_push *subndr,
						      struct ndr_push *uncomndr)
{
	struct ndr_pull	*ndrpull;
	bool		last = false;

	ndrpull = talloc_zero(uncomndr, struct ndr_pull);
	NDR_ERR_HAVE_NO_MEMORY(ndrpull);
	ndrpull->flags = uncomndr->flags;
	ndrpull->data = uncomndr->data;
	ndrpull->data_size = uncomndr->offset;
	ndrpull->offset = 0;

	while (!last) {
		NDR_CHECK(ndr_push_lxpress_chunk(subndr, ndrpull, &last));
	}

	return NDR_ERR_SUCCESS;
}


_PUBLIC_ enum ndr_err_code ndr_pull_mapi2k7_request(struct ndr_pull *ndr, int ndr_flags, struct mapi2k7_request *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_RPC_HEADER_EXT(ndr, NDR_SCALARS, &r->header));
		{
			uint32_t _flags_save_mapi_request = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_REMAINING|LIBNDR_FLAG_NOALIGN);
			{
				struct ndr_pull *_ndr_buffer;

				if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
					NDR_PULL_ALLOC(ndr, r->mapi_request);
				}

				NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_buffer, 0, -1));
				{
					if ((r->header.Flags == (RHEF_Compressed|RHEF_XorMagic)) ||
					    (r->header.Flags == (RHEF_Compressed|RHEF_XorMagic|RHEF_Last))) {
						struct ndr_pull *_ndr_data_compressed = NULL;

						obfuscate_data(_ndr_buffer->data, _ndr_buffer->data_size, 0xA5);
						NDR_CHECK(ndr_pull_lzxpress_decompress(_ndr_buffer, &_ndr_data_compressed, r->header.SizeActual));
						NDR_CHECK(ndr_pull_mapi_request(_ndr_data_compressed, NDR_SCALARS|NDR_BUFFERS, r->mapi_request));
						_ndr_buffer->offset = _ndr_buffer->data_size;
					} else if ((r->header.Flags  == RHEF_Compressed) ||
						   (r->header.Flags == (RHEF_Compressed|RHEF_Last))) {
						struct ndr_pull *_ndr_data_compressed = NULL;

						NDR_CHECK(ndr_pull_lzxpress_decompress(_ndr_buffer, &_ndr_data_compressed, r->header.SizeActual));
						NDR_CHECK(ndr_pull_mapi_request(_ndr_data_compressed, NDR_SCALARS|NDR_BUFFERS, r->mapi_request));
						_ndr_buffer->offset = _ndr_buffer->data_size;
					} else if ((r->header.Flags == RHEF_XorMagic) ||
						   (r->header.Flags == (RHEF_XorMagic|RHEF_Last))) {
						obfuscate_data(_ndr_buffer->data, _ndr_buffer->data_size, 0xA5);
						NDR_CHECK(ndr_pull_mapi_request(_ndr_buffer, NDR_SCALARS|NDR_BUFFERS, r->mapi_request));
					} else {
						NDR_CHECK(ndr_pull_mapi_request(_ndr_buffer, NDR_SCALARS|NDR_BUFFERS, r->mapi_request));

					}
				}
				NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_buffer, 0, -1));
			}
			ndr->flags = _flags_save_mapi_request;
		}
	}

	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_push_mapi2k7_request(struct ndr_push *ndr, int ndr_flags, const struct mapi2k7_request *r)
{
	NDR_PUSH_CHECK_FLAGS(ndr, ndr_flags);
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 5));

		if ((r->header.Flags & RHEF_Last) == 0) {
			return ndr_push_error(ndr, NDR_ERR_VALIDATE, "RPC_HEADER_EXT.Flags indicates this isn't the last header block.");
		}

		/* Make a local copy so the flags can be reset, compression and obfuscation are not currently supported. */
		struct RPC_HEADER_EXT tempRPC_HEADER_EXT;
		tempRPC_HEADER_EXT = r->header;
		tempRPC_HEADER_EXT.Size = tempRPC_HEADER_EXT.SizeActual;  /* Original size is not valid, use actual size */
		tempRPC_HEADER_EXT.Flags = RHEF_Last;

		NDR_CHECK(ndr_push_RPC_HEADER_EXT(ndr, NDR_SCALARS, &tempRPC_HEADER_EXT));

		if (r->mapi_request) {
			struct ndr_push *_ndr_mapi_request;
			NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_mapi_request, 0, -1));
			NDR_CHECK(ndr_push_mapi_request(_ndr_mapi_request, NDR_SCALARS|NDR_BUFFERS, r->mapi_request));
			NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_mapi_request, 0, -1));
		}
	}
	return NDR_ERR_SUCCESS;
}


_PUBLIC_ enum ndr_err_code ndr_pull_mapi2k7_response(struct ndr_pull *ndr, int ndr_flags, struct mapi2k7_response *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_RPC_HEADER_EXT(ndr, NDR_SCALARS, &r->header));
		{
			uint32_t _flags_save_mapi_response = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REMAINING);
			{
				struct ndr_pull *_ndr_buffer;

				if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
					NDR_PULL_ALLOC(ndr, r->mapi_response);
				}

				NDR_CHECK((ndr_pull_subcontext_start(ndr, &_ndr_buffer, 0, r->header.Size)));
				{
					if (r->header.Flags & RHEF_Compressed) {
						struct ndr_pull *_ndr_data_compressed = NULL;

						NDR_CHECK(ndr_pull_lzxpress_decompress(_ndr_buffer, &_ndr_data_compressed, r->header.SizeActual));
						NDR_CHECK(ndr_pull_mapi_response(_ndr_data_compressed, NDR_SCALARS|NDR_BUFFERS, r->mapi_response));
					} else if (r->header.Flags & RHEF_XorMagic) {
						obfuscate_data(_ndr_buffer->data, _ndr_buffer->data_size, 0xA5);
						NDR_CHECK(ndr_pull_mapi_response(_ndr_buffer, NDR_SCALARS|NDR_BUFFERS, r->mapi_response));
					} else {
						NDR_CHECK(ndr_pull_mapi_response(_ndr_buffer, NDR_SCALARS|NDR_BUFFERS, r->mapi_response));
					}
				}
				NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_buffer, 0, r->header.Size));
			}
			ndr->flags = _flags_save_mapi_response;
		}
	}

	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_push_mapi2k7_response(struct ndr_push *ndr, int ndr_flags, const struct mapi2k7_response *r)
{
	NDR_PUSH_CHECK_FLAGS(ndr, ndr_flags);
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 5));

		if ((r->header.Flags & RHEF_Last) == 0) {
			return ndr_push_error(ndr, NDR_ERR_VALIDATE, "RPC_HEADER_EXT.Flags indicates this isn't the last header block.");
		}

		/* Make a local copy so the flags can be reset, compression and obfuscation are not currently supported. */
		struct RPC_HEADER_EXT tempRPC_HEADER_EXT;
		tempRPC_HEADER_EXT = r->header;
		tempRPC_HEADER_EXT.Size = tempRPC_HEADER_EXT.SizeActual;  /* Original size is not valid, use actual size */
		tempRPC_HEADER_EXT.Flags = RHEF_Last;

		NDR_CHECK(ndr_push_RPC_HEADER_EXT(ndr, NDR_SCALARS, &tempRPC_HEADER_EXT));

		if (r->mapi_response) {
			struct ndr_push *_ndr_mapi_response;
			NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_mapi_response, 0, -1));
			NDR_CHECK(ndr_push_mapi_response(_ndr_mapi_response, NDR_SCALARS|NDR_BUFFERS, r->mapi_response));
			NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_mapi_response, 0, -1));
		}
	}
	return NDR_ERR_SUCCESS;
}


_PUBLIC_ void ndr_print_AUX_HEADER(struct ndr_print *ndr, const char *name, const struct AUX_HEADER *r)
{
	ndr_print_struct(ndr, name, "AUX_HEADER");
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		ndr->depth++;
		ndr_print_uint16(ndr, "Size", r->Size);
		ndr_print_AUX_VERSION(ndr, "Version", r->Version);

		switch (r->Version) {
		case AUX_VERSION_1:
			ndr_print_AUX_HEADER_TYPE_1(ndr, "Type", (enum AUX_HEADER_TYPE_1) r->Type);
			ndr_print_set_switch_value(ndr, &r->Payload_1, r->Type);
			ndr_print_AUX_HEADER_TYPE_UNION_1(ndr, "Payload", &r->Payload_1);
			break;
		case AUX_VERSION_2:
			ndr_print_AUX_HEADER_TYPE_2(ndr, "Type", (enum AUX_HEADER_TYPE_2) r->Type);
			ndr_print_set_switch_value(ndr, &r->Payload_2, r->Type);
			ndr_print_AUX_HEADER_TYPE_UNION_2(ndr, "Payload", &r->Payload_2);
		}
		ndr->depth--;
		ndr->flags = _flags_save_STRUCT;
	}
}


_PUBLIC_ enum ndr_err_code ndr_pull_AUX_HEADER(struct ndr_pull *ndr, int ndr_flags, struct AUX_HEADER *r)
{
	struct ndr_pull	*_ndr_buffer;
	uint32_t	_flags_save_STRUCT = ndr->flags;

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->Size));

		NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_buffer, 0, r->Size - 2));
		{
			NDR_CHECK(ndr_pull_AUX_VERSION(_ndr_buffer, NDR_SCALARS, &r->Version));
			NDR_CHECK(ndr_pull_uint8(_ndr_buffer, NDR_SCALARS, &r->Type));
			switch (r->Version) {
			case AUX_VERSION_1:
				NDR_CHECK(ndr_pull_set_switch_value(_ndr_buffer, &r->Payload_1, r->Type));
				NDR_CHECK(ndr_pull_AUX_HEADER_TYPE_UNION_1(_ndr_buffer, NDR_SCALARS, &r->Payload_1));
				break;
			case AUX_VERSION_2:
				NDR_CHECK(ndr_pull_set_switch_value(_ndr_buffer, &r->Payload_2, r->Type));
				NDR_CHECK(ndr_pull_AUX_HEADER_TYPE_UNION_2(_ndr_buffer, NDR_SCALARS, &r->Payload_2));
				break;
			}
		}
		NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_buffer, 0, -1));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	ndr->flags = _flags_save_STRUCT;

	return NDR_ERR_SUCCESS;
}


_PUBLIC_ enum ndr_err_code ndr_push_AUX_HEADER(struct ndr_push *ndr, int ndr_flags, const struct AUX_HEADER *r)
{
	uint32_t _flags_save_STRUCT = ndr->flags;
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	NDR_PUSH_CHECK_FLAGS(ndr, ndr_flags);
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->Size));
		NDR_CHECK(ndr_push_AUX_VERSION(ndr, NDR_SCALARS, r->Version));
		NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, r->Type));
		switch (r->Version) {
		case AUX_VERSION_1:
			ndr_push_set_switch_value(ndr, &r->Payload_1, r->Type);
			NDR_CHECK(ndr_push_AUX_HEADER_TYPE_UNION_1(ndr, NDR_SCALARS, &r->Payload_1));
			break;
		case AUX_VERSION_2:
			ndr_push_set_switch_value(ndr, &r->Payload_2, r->Type);
			NDR_CHECK(ndr_push_AUX_HEADER_TYPE_UNION_2(ndr, NDR_SCALARS, &r->Payload_2));
			break;
		}
		NDR_CHECK(ndr_push_trailer_align(ndr, 4));
	}
	ndr->flags = _flags_save_STRUCT;

	return NDR_ERR_SUCCESS;
}


_PUBLIC_ void ndr_print_mapi2k7_AuxInfo(struct ndr_print *ndr, const char *name, const struct mapi2k7_AuxInfo *r)
{
	uint32_t	i;

	if (r && r->AUX_HEADER) {
		ndr_print_struct(ndr, name, "mapi2k7_AuxInfo");
		ndr->depth++;
		ndr_print_RPC_HEADER_EXT(ndr, "RPC_HEADER_EXT", &r->RPC_HEADER_EXT);
		for (i = 0; r->AUX_HEADER[i].Size; i++) {
			ndr_print_AUX_HEADER(ndr, "AUX_HEADER", &r->AUX_HEADER[i]);
		}
		ndr->depth--;
	} else {
		ndr_print_pointer(ndr, "mapi2k7_AuxInfo", NULL);
	}
}


_PUBLIC_ enum ndr_err_code ndr_pull_mapi2k7_AuxInfo(struct ndr_pull *ndr, int ndr_flags, struct mapi2k7_AuxInfo *r)
{
	if (ndr_flags & NDR_SCALARS) {

		/* Sanity Checks */
		if (!ndr->data_size) {
			r->AUX_HEADER = NULL;
			return NDR_ERR_SUCCESS;
		}

		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_RPC_HEADER_EXT(ndr, NDR_SCALARS, &r->RPC_HEADER_EXT));
		{
			uint32_t _flags_save_DATA_BLOB = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REMAINING);
			if (r->RPC_HEADER_EXT.Size) {
				struct ndr_pull *_ndr_buffer;
				TALLOC_CTX	*_mem_save_AUX_HEADER_0;
				uint32_t	cntr_AUX_HEADER_0 = 0;

				_mem_save_AUX_HEADER_0 = NDR_PULL_GET_MEM_CTX(ndr);

				NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_buffer, 0, r->RPC_HEADER_EXT.Size));
				{
					r->AUX_HEADER = talloc_array(_mem_save_AUX_HEADER_0, struct AUX_HEADER, 2);

					/* lzxpress case */
					if (r->RPC_HEADER_EXT.Flags & RHEF_Compressed) {
						struct ndr_pull *_ndr_data_compressed = NULL;

						ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
						NDR_CHECK(ndr_pull_lzxpress_decompress(_ndr_buffer, &_ndr_data_compressed, r->RPC_HEADER_EXT.SizeActual));

						for (cntr_AUX_HEADER_0 = 0; _ndr_data_compressed->offset < _ndr_data_compressed->data_size; cntr_AUX_HEADER_0++) {
							NDR_CHECK(ndr_pull_AUX_HEADER(_ndr_data_compressed, NDR_SCALARS, &r->AUX_HEADER[cntr_AUX_HEADER_0]));
							r->AUX_HEADER = talloc_realloc(_mem_save_AUX_HEADER_0, r->AUX_HEADER, struct AUX_HEADER, cntr_AUX_HEADER_0 + 2);
						}
						r->AUX_HEADER = talloc_realloc(_mem_save_AUX_HEADER_0, r->AUX_HEADER, struct AUX_HEADER, cntr_AUX_HEADER_0 + 2);
						r->AUX_HEADER[cntr_AUX_HEADER_0].Size = 0;

					/* obfuscation case */
					} else if (r->RPC_HEADER_EXT.Flags & RHEF_XorMagic) {
						obfuscate_data(_ndr_buffer->data, _ndr_buffer->data_size, 0xA5);

						for (cntr_AUX_HEADER_0 = 0; _ndr_buffer->offset < _ndr_buffer->data_size; cntr_AUX_HEADER_0++) {
							NDR_CHECK(ndr_pull_AUX_HEADER(_ndr_buffer, NDR_SCALARS, &r->AUX_HEADER[cntr_AUX_HEADER_0]));
							r->AUX_HEADER = talloc_realloc(_mem_save_AUX_HEADER_0, r->AUX_HEADER, struct AUX_HEADER, cntr_AUX_HEADER_0 + 2);
						}
						r->AUX_HEADER = talloc_realloc(_mem_save_AUX_HEADER_0, r->AUX_HEADER, struct AUX_HEADER, cntr_AUX_HEADER_0 + 2);
						r->AUX_HEADER[cntr_AUX_HEADER_0].Size = 0;
					/* plain case */
					} else {
						for (cntr_AUX_HEADER_0 = 0; _ndr_buffer->offset < _ndr_buffer->data_size; cntr_AUX_HEADER_0++) {
							NDR_CHECK(ndr_pull_AUX_HEADER(_ndr_buffer, NDR_SCALARS, &r->AUX_HEADER[cntr_AUX_HEADER_0]));
							r->AUX_HEADER = talloc_realloc(_mem_save_AUX_HEADER_0, r->AUX_HEADER, struct AUX_HEADER, cntr_AUX_HEADER_0 + 2);
						}
						r->AUX_HEADER = talloc_realloc(_mem_save_AUX_HEADER_0, r->AUX_HEADER, struct AUX_HEADER, cntr_AUX_HEADER_0 + 2);
						r->AUX_HEADER[cntr_AUX_HEADER_0].Size = 0;
					}
				}
				NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_buffer, 0, -1));
			} else {
				r->AUX_HEADER = NULL;
			}
			ndr->flags = _flags_save_DATA_BLOB;
		}
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}


_PUBLIC_ enum ndr_err_code ndr_push_mapi2k7_AuxInfo(struct ndr_push *ndr, int ndr_flags, const struct mapi2k7_AuxInfo *r)
{
	uint32_t	i;

	NDR_PUSH_CHECK_FLAGS(ndr, ndr_flags);
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 5));

		if ((r->RPC_HEADER_EXT.Flags & RHEF_Last) == 0) {
			return ndr_push_error(ndr, NDR_ERR_VALIDATE, "RPC_HEADER_EXT.Flags indicates this isn't the last header block.");
		}

		/* Make a local copy so the flags can be reset, compression and obfuscation are not currently supported. */
		struct RPC_HEADER_EXT tempRPC_HEADER_EXT;
		tempRPC_HEADER_EXT = r->RPC_HEADER_EXT;
		tempRPC_HEADER_EXT.Size = tempRPC_HEADER_EXT.SizeActual;  /* Original size is not valid, use actual size */
		tempRPC_HEADER_EXT.Flags = RHEF_Last;

		NDR_CHECK(ndr_push_RPC_HEADER_EXT(ndr, NDR_SCALARS, &tempRPC_HEADER_EXT));

		if (r->AUX_HEADER) {
			struct ndr_push *_ndr_AUX_HEADER;
			NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_AUX_HEADER, 0, tempRPC_HEADER_EXT.Size));
			for (i = 0; r->AUX_HEADER[i].Size; i++) {
				NDR_CHECK(ndr_push_AUX_HEADER(_ndr_AUX_HEADER, NDR_SCALARS,  &r->AUX_HEADER[i]));
			}

			NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_AUX_HEADER, 0, tempRPC_HEADER_EXT.Size));
		}
	}
	return NDR_ERR_SUCCESS;
}

/*
  print mapi_request / mapi_response structures
 */

void ndr_print_mapi_request(struct ndr_print *ndr, const char *name, const struct mapi_request *r)
{
	uint32_t	rlength;

	rlength = r->mapi_len - r->length;

	ndr_print_uint32(ndr, "mapi_len", r->mapi_len);
	if (r->length && r->length > sizeof(uint16_t)) {
		uint32_t cntr_mapi_req_0;

		ndr_print_uint16(ndr, "length", r->length);
		ndr->depth++;
		for (cntr_mapi_req_0=0; r->mapi_req[cntr_mapi_req_0].opnum; cntr_mapi_req_0++) {
			char	*idx_0 = NULL;
			int	ret;

			ret = asprintf(&idx_0, "[%u]", cntr_mapi_req_0);
			if (ret != -1 && idx_0) {
				ndr_print_EcDoRpc_MAPI_REQ(ndr, "mapi_request", &r->mapi_req[cntr_mapi_req_0]);
				free(idx_0);
			}
		}
		ndr->depth--;
	}

	if (rlength) {
		uint32_t i;

		ndr->depth++;
		ndr->print(ndr, "%-25s: (handles) number=%u", name, rlength / 4);
		ndr->depth++;
		for (i = 0; i < (rlength / 4); i++) {
			ndr_print_uint32(ndr, "handle", r->handles[i]);
		}
		ndr->depth--;
	}
}

void ndr_print_mapi_response(struct ndr_print *ndr, const char *name, const struct mapi_response *r)
{
	uint32_t	rlength;

	rlength = r->mapi_len - r->length;

	ndr->print(ndr, "%-25s: length=%u", name, r->length);
	if (r->length && r->length > sizeof(uint16_t)) {
		uint32_t cntr_mapi_repl_0;

		ndr->print(ndr, "%s: ARRAY(%d)", name, r->length - 2);
		ndr->depth++;
		for (cntr_mapi_repl_0=0; r->mapi_repl[cntr_mapi_repl_0].opnum; cntr_mapi_repl_0++) {
			ndr_print_EcDoRpc_MAPI_REPL(ndr, "mapi_repl", &r->mapi_repl[cntr_mapi_repl_0]);
		}
		ndr->depth--;
	}

	ndr->print(ndr, "%-25s: (handles) number=%u", name, rlength / 4);

	if (rlength) {
		uint32_t i;

		ndr->depth++;

		for (i = 0; i < (rlength / 4); i++) {
			ndr_print_uint32(ndr, "handle id", r->handles[i]);
		}
		ndr->depth--;
	}
}


/*
  push mapi_request / mapi_response onto the wire.

  MAPI length field includes length bytes.
  But these bytes do not belong to the mapi content in the user
  context. We have to add them when pushing mapi content length
  (uint16_t) and next subtract when pushing the content blob
*/

enum ndr_err_code ndr_push_mapi_request(struct ndr_push *ndr, int ndr_flags, const struct mapi_request *r)
{
	uint32_t		cntr_mapi_req_0;
	uint32_t		count;

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->length));

	for (count = 0; ndr->offset < r->length - 2; count++) {
		NDR_CHECK(ndr_push_EcDoRpc_MAPI_REQ(ndr, NDR_SCALARS, &r->mapi_req[count]));
	}

	count = (r->mapi_len - r->length) / sizeof(uint32_t);
	for (cntr_mapi_req_0=0; cntr_mapi_req_0 < count; cntr_mapi_req_0++) {
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->handles[cntr_mapi_req_0]));
	}

	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_push_mapi_response(struct ndr_push *ndr, int ndr_flags, const struct mapi_response *r)
{
	uint32_t	cntr_mapi_repl_0;
	uint32_t	count;

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->length));

	if (r->length > sizeof (uint16_t)) {
		for (count = 0; ndr->offset < r->length - 2; count++) {
			NDR_CHECK(ndr_push_EcDoRpc_MAPI_REPL(ndr, NDR_SCALARS, &r->mapi_repl[count]));
		}
	}

	count = (r->mapi_len - r->length) / sizeof (uint32_t);
	for (cntr_mapi_repl_0 = 0; cntr_mapi_repl_0 <count; cntr_mapi_repl_0++) {
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->handles[cntr_mapi_repl_0]));
	}

	return NDR_ERR_SUCCESS;
}

/*
  pull mapi_request / mapi_response from the wire
*/

enum ndr_err_code ndr_pull_mapi_request(struct ndr_pull *ndr, int ndr_flags, struct mapi_request *r)
{
	uint32_t length,count;
	uint32_t cntr_mapi_req_0;
	TALLOC_CTX *_mem_save_mapi_req_0;
	TALLOC_CTX *_mem_save_handles_0;
	struct ndr_pull *_ndr_mapi_req;

	if (ndr->flags & LIBNDR_FLAG_REMAINING) {
		length = ndr->data_size - ndr->offset;
	} else {
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &length));
	}
	r->mapi_len = length;

	NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->length));

	/* If length equals length field then skipping subcontext */
	if (r->length > sizeof (uint16_t)) {
		NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_mapi_req, 0, r->length - 2));
		_mem_save_mapi_req_0 = NDR_PULL_GET_MEM_CTX(_ndr_mapi_req);
		r->mapi_req = talloc_zero(_mem_save_mapi_req_0, struct EcDoRpc_MAPI_REQ);
		for (cntr_mapi_req_0 = 0; _ndr_mapi_req->offset < _ndr_mapi_req->data_size - 2; cntr_mapi_req_0++) {
			NDR_CHECK(ndr_pull_EcDoRpc_MAPI_REQ(_ndr_mapi_req, NDR_SCALARS, &r->mapi_req[cntr_mapi_req_0]));
			r->mapi_req = talloc_realloc(_mem_save_mapi_req_0, r->mapi_req, struct EcDoRpc_MAPI_REQ, cntr_mapi_req_0 + 2);
		}
		r->mapi_req = talloc_realloc(_mem_save_mapi_req_0, r->mapi_req, struct EcDoRpc_MAPI_REQ, cntr_mapi_req_0 + 2);
		r->mapi_req[cntr_mapi_req_0].opnum = 0;

		if (_ndr_mapi_req->offset != r->length - 2) {
			return NDR_ERR_BUFSIZE;
		}
		NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_mapi_req, 4, -1));

		_mem_save_handles_0 = NDR_PULL_GET_MEM_CTX(ndr);
		count = (r->mapi_len - r->length) / sizeof(uint32_t);
		r->handles = talloc_array(_mem_save_handles_0, uint32_t, count + 1);
		for (cntr_mapi_req_0=0; cntr_mapi_req_0 < count; cntr_mapi_req_0++) {
			NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->handles[cntr_mapi_req_0]));
		}
	} else {
		r->handles = NULL;
	}

	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_mapi_response(struct ndr_pull *ndr, int ndr_flags, struct mapi_response *r)
{
	uint32_t length,count;
	uint32_t cntr_mapi_repl_0;
	TALLOC_CTX *_mem_save_mapi_repl_0;
	TALLOC_CTX *_mem_save_handles_0;
	struct ndr_pull *_ndr_mapi_repl;

	if (ndr->flags & LIBNDR_FLAG_REMAINING) {
		length = ndr->data_size - ndr->offset;
	} else {
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &length));
	}
	r->mapi_len = length;

	NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->length));

	/* If length equals length field then skipping subcontext */
	if (r->length > sizeof (uint16_t)) {
		_mem_save_mapi_repl_0 = NDR_PULL_GET_MEM_CTX(ndr);
		r->mapi_repl = talloc_zero_array(_mem_save_mapi_repl_0, struct EcDoRpc_MAPI_REPL, 2);
		NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_mapi_repl, 0, r->length - 2));
		for (cntr_mapi_repl_0 = 0; _ndr_mapi_repl->offset < _ndr_mapi_repl->data_size - 2; cntr_mapi_repl_0++) {
			NDR_CHECK(ndr_pull_EcDoRpc_MAPI_REPL(_ndr_mapi_repl, NDR_SCALARS, &r->mapi_repl[cntr_mapi_repl_0]));
			r->mapi_repl = talloc_realloc(_ndr_mapi_repl, r->mapi_repl, struct EcDoRpc_MAPI_REPL, cntr_mapi_repl_0 + 2);
		}
		r->mapi_repl[cntr_mapi_repl_0].opnum = 0;
		NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_mapi_repl, 4, -1));
		talloc_free(_ndr_mapi_repl);
	} else {
		r->mapi_repl = NULL;
	}

	_mem_save_handles_0 = NDR_PULL_GET_MEM_CTX(ndr);
	count = (r->mapi_len - r->length) / sizeof(uint32_t);
	NDR_PULL_ALLOC_N(ndr, r->handles, count + 1);

	for (cntr_mapi_repl_0=0; cntr_mapi_repl_0 < count; cntr_mapi_repl_0++) {
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->handles[cntr_mapi_repl_0]));
	}
	NDR_PULL_SET_MEM_CTX(ndr, _mem_save_handles_0, LIBNDR_FLAG_REF_ALLOC);

	return NDR_ERR_SUCCESS;
}

/*
  We stop processing the IDL if MAPISTATUS is different from MAPI_E_SUCCESS
 */

_PUBLIC_ enum ndr_err_code ndr_push_EcDoRpc_MAPI_REPL(struct ndr_push *ndr, int ndr_flags, const struct EcDoRpc_MAPI_REPL *r)
{
	if (r->opnum != op_MAPI_Release)
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_push_align(ndr, 8));
			NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, r->opnum));
			if ((r->opnum == op_MAPI_Notify) || (r->opnum == op_MAPI_Pending)) {
				NDR_CHECK(ndr_push_set_switch_value(ndr, &r->u, r->opnum));
				NDR_CHECK(ndr_push_EcDoRpc_MAPI_REPL_UNION(ndr, NDR_SCALARS, &r->u));
			} else {
				NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, r->handle_idx));
				NDR_CHECK(ndr_push_MAPISTATUS(ndr, NDR_SCALARS, r->error_code));
				if (r->error_code == MAPI_E_SUCCESS) {
					NDR_CHECK(ndr_push_set_switch_value(ndr, &r->u, r->opnum));
					NDR_CHECK(ndr_push_EcDoRpc_MAPI_REPL_UNION(ndr, NDR_SCALARS, &r->u));
				} else {
					switch (r->opnum) {
					case op_MAPI_Logon: {
						if (r->error_code == ecWrongServer) {
							NDR_CHECK(ndr_push_Logon_redirect(ndr, NDR_SCALARS, &(r->us.mapi_Logon)));
						}
						break; }
					case op_MAPI_GetIDsFromNames: {
						/* MAPI_W_ERRORS_RETURNED still enables the final array to be passed */
						if (r->error_code == MAPI_W_ERRORS_RETURNED) {
							NDR_CHECK(ndr_push_set_switch_value(ndr, &r->u, r->opnum));
							NDR_CHECK(ndr_push_EcDoRpc_MAPI_REPL_UNION(ndr, NDR_SCALARS, &r->u));
						}
						break; }
					case op_MAPI_MoveFolder:
					case op_MAPI_CopyFolder: {
						/* ecDstNullObject requires the return of an additional uint32_t for DestHandleIndex */
						if (r->error_code == ecDstNullObject) {
							NDR_CHECK(ndr_push_set_switch_value(ndr, &r->u, r->opnum));
							NDR_CHECK(ndr_push_EcDoRpc_MAPI_REPL_UNION(ndr, NDR_SCALARS, &r->u));
						}
						break; }
					default:
						break;
					}
				}
			}
		}
		if (ndr_flags & NDR_BUFFERS) {
			NDR_CHECK(ndr_push_EcDoRpc_MAPI_REPL_UNION(ndr, NDR_BUFFERS, &r->u));
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_EcDoRpc_MAPI_REPL(struct ndr_pull *ndr, int ndr_flags, struct EcDoRpc_MAPI_REPL *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_pull_align(ndr, 8));
			NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &r->opnum));
			if ((r->opnum == op_MAPI_Notify) || (r->opnum == op_MAPI_Pending)) {
				NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->u, r->opnum));
				NDR_CHECK(ndr_pull_EcDoRpc_MAPI_REPL_UNION(ndr, NDR_SCALARS, &r->u));
			} else {
				NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &r->handle_idx));
				NDR_CHECK(ndr_pull_MAPISTATUS(ndr, NDR_SCALARS, &r->error_code));
				if ( r->error_code == MAPI_E_SUCCESS) {
					NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->u, r->opnum));
					if (r->opnum == op_MAPI_MoveFolder) {
						r->u.mapi_MoveFolder.HasDestHandleIndex = false;
					}
					else if (r->opnum == op_MAPI_CopyFolder) {
						r->u.mapi_CopyFolder.HasDestHandleIndex = false;
					}
					NDR_CHECK(ndr_pull_EcDoRpc_MAPI_REPL_UNION(ndr, NDR_SCALARS, &r->u));
				} else {
					switch (r->opnum) {
					case op_MAPI_Logon: {
						if (r->error_code == ecWrongServer) {
							NDR_CHECK(ndr_pull_Logon_redirect(ndr, NDR_SCALARS, &(r->us.mapi_Logon)));
						}
						break;}
					case op_MAPI_GetIDsFromNames: {
						/* MAPI_W_ERRORS_RETURNED still enables the final array to be passed */
						if (r->error_code == MAPI_W_ERRORS_RETURNED) {
							NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->u, r->opnum));
							NDR_CHECK(ndr_pull_EcDoRpc_MAPI_REPL_UNION(ndr, NDR_SCALARS, &r->u));
						}
						break;}
					case op_MAPI_MoveFolder: {
						/* ecDstNullObject requires the return of an additional uint32_t for DestHandleIndex */
						if (r->error_code == ecDstNullObject) {
							r->u.mapi_MoveFolder.HasDestHandleIndex = true;
							NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->u, r->opnum));
							NDR_CHECK(ndr_pull_EcDoRpc_MAPI_REPL_UNION(ndr, NDR_SCALARS, &r->u));
						}
						else {
							r->u.mapi_MoveFolder.HasDestHandleIndex = false;
						}
						break; }
					case op_MAPI_CopyFolder: {
						/* ecDstNullObject requires the return of an additional uint32_t for DestHandleIndex */
						if (r->error_code == ecDstNullObject) {
							r->u.mapi_CopyFolder.HasDestHandleIndex = true;
							NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->u, r->opnum));
							NDR_CHECK(ndr_pull_EcDoRpc_MAPI_REPL_UNION(ndr, NDR_SCALARS, &r->u));
						}
						else {
							r->u.mapi_CopyFolder.HasDestHandleIndex = false;
						}
						break; }
					default:
						break;
					}
				}
			}
		}
		if (ndr_flags & NDR_BUFFERS) {
			ndr->flags = _flags_save_STRUCT;
		}
	}
	return NDR_ERR_SUCCESS;
}

void ndr_print_EcDoRpc_MAPI_REPL(struct ndr_print *ndr, const char *name, const struct EcDoRpc_MAPI_REPL *r)
{
	ndr_print_struct(ndr, name, "EcDoRpc_MAPI_REPL");
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		ndr->depth++;
		ndr_print_uint8(ndr, "opnum", r->opnum);
		if ((r->opnum != op_MAPI_Notify) && (r->opnum != op_MAPI_Pending)) {
			ndr_print_uint8(ndr, "handle_idx", r->handle_idx);
			ndr_print_MAPISTATUS(ndr, "error_code", r->error_code);
			if (r->error_code == MAPI_E_SUCCESS) {
				ndr_print_set_switch_value(ndr, &r->u, r->opnum);
				ndr_print_EcDoRpc_MAPI_REPL_UNION(ndr, "u", &r->u);
			} else {
				switch (r->opnum) {
				case op_MAPI_Logon: {
					if (r->error_code == ecWrongServer) {
						ndr_print_set_switch_value(ndr, &r->us, r->opnum);
						ndr_print_EcDoRpc_MAPI_REPL_UNION_SPECIAL(ndr, "us", &r->us);
					}
					break;}
				case op_MAPI_GetIDsFromNames: {
					/* MAPI_W_ERRORS_RETURNED still enables the final array to be passed */
					if (r->error_code == MAPI_W_ERRORS_RETURNED) {
						ndr_print_set_switch_value(ndr, &r->u, r->opnum);
						ndr_print_EcDoRpc_MAPI_REPL_UNION(ndr, "u", &r->u);
					}
					break; }
				case op_MAPI_MoveFolder:
				case op_MAPI_CopyFolder: {
					/* ecDstNullObject requires the return of an additional uint32_t for DestHandleIndex */
					if (r->error_code == ecDstNullObject) {
						ndr_print_set_switch_value(ndr, &r->u, r->opnum);
						ndr_print_EcDoRpc_MAPI_REPL_UNION(ndr, "u", &r->u);
					}
					break; }
				default:
					break;
				}
			}
		} else {
			ndr_print_set_switch_value(ndr, &r->u, r->opnum);
			ndr_print_EcDoRpc_MAPI_REPL_UNION(ndr, "u", &r->u);
		}
		ndr->depth--;
		ndr->flags = _flags_save_STRUCT;
	}
}


_PUBLIC_ enum ndr_err_code ndr_pull_EcDoRpc(struct ndr_pull *ndr, int flags, struct EcDoRpc *r)
{
	TALLOC_CTX *_mem_save_handle_0;
	TALLOC_CTX *_mem_save_mapi_request_0;
	TALLOC_CTX *_mem_save_mapi_response_0;
	TALLOC_CTX *_mem_save_length_0;

	if (flags & NDR_IN) {
		ZERO_STRUCT(r->out);

		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->in.handle);
		}
		_mem_save_handle_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->in.handle, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_policy_handle(ndr, NDR_SCALARS|NDR_BUFFERS, r->in.handle));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_handle_0, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->in.size));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->in.offset));
		{
			uint32_t _flags_save_mapi_request = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_REMAINING|LIBNDR_FLAG_NOALIGN);
			if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
				NDR_PULL_ALLOC(ndr, r->in.mapi_request);
			}
			_mem_save_mapi_request_0 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, r->in.mapi_request, LIBNDR_FLAG_REF_ALLOC);
			{
				struct ndr_pull *_ndr_mapi_request;
				NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_mapi_request, 4, -1));
				obfuscate_data(_ndr_mapi_request->data, _ndr_mapi_request->data_size, 0xA5);
				NDR_CHECK(ndr_pull_mapi_request(_ndr_mapi_request, NDR_SCALARS|NDR_BUFFERS, r->in.mapi_request));
				NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_mapi_request, 4, -1));
			}
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_mapi_request_0, LIBNDR_FLAG_REF_ALLOC);
			ndr->flags = _flags_save_mapi_request;
		}
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->in.length);
		}
		_mem_save_length_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->in.length, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, r->in.length));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_length_0, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->in.max_data));
		NDR_PULL_ALLOC(ndr, r->out.handle);
		*r->out.handle = *r->in.handle;
		NDR_PULL_ALLOC(ndr, r->out.mapi_response);
		ZERO_STRUCTP(r->out.mapi_response);
		NDR_PULL_ALLOC(ndr, r->out.length);
		*r->out.length = *r->in.length;
	}
	if (flags & NDR_OUT) {
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.handle);
		}
		_mem_save_handle_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.handle, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_policy_handle(ndr, NDR_SCALARS|NDR_BUFFERS, r->out.handle));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_handle_0, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->out.size));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->out.offset));
		{
			uint32_t _flags_save_mapi_response = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_REMAINING|LIBNDR_FLAG_NOALIGN);
			if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
				NDR_PULL_ALLOC(ndr, r->out.mapi_response);
			}
			_mem_save_mapi_response_0 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, r->out.mapi_response, LIBNDR_FLAG_REF_ALLOC);
			{
				struct ndr_pull *_ndr_mapi_response;
				NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_mapi_response, 4, -1));
				obfuscate_data(_ndr_mapi_response->data, _ndr_mapi_response->data_size, 0xA5);
				NDR_CHECK(ndr_pull_mapi_response(_ndr_mapi_response, NDR_SCALARS|NDR_BUFFERS, r->out.mapi_response));
				NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_mapi_response, 4, -1));
			}
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_mapi_response_0, LIBNDR_FLAG_REF_ALLOC);
			ndr->flags = _flags_save_mapi_response;
		}
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.length);
		}
		_mem_save_length_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.length, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, r->out.length));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_length_0, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_MAPISTATUS(ndr, NDR_SCALARS, &r->out.result));
	}
	return NDR_ERR_SUCCESS;
}


_PUBLIC_ enum ndr_err_code ndr_push_EcDoRpc(struct ndr_push *ndr, int flags, const struct EcDoRpc *r)
{
	if (flags & NDR_IN) {
		if (r->in.handle == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_policy_handle(ndr, NDR_SCALARS|NDR_BUFFERS, r->in.handle));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->in.size));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->in.offset));
		{
			uint32_t _flags_save_mapi_request = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_REMAINING|LIBNDR_FLAG_NOALIGN);
			if (r->in.mapi_request == NULL) {
				return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
			}
			{
				struct ndr_push *_ndr_mapi_request;
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_mapi_request, 4, -1));
				NDR_CHECK(ndr_push_mapi_request(_ndr_mapi_request, NDR_SCALARS|NDR_BUFFERS, r->in.mapi_request));
				obfuscate_data(_ndr_mapi_request->data, _ndr_mapi_request->offset, 0xA5);
				NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_mapi_request, 4, -1));
			}
			ndr->flags = _flags_save_mapi_request;
		}
		if (r->in.length == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, *r->in.length));
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->in.max_data));
	}
	if (flags & NDR_OUT) {
		if (r->out.handle == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_policy_handle(ndr, NDR_SCALARS|NDR_BUFFERS, r->out.handle));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->out.size));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->out.offset));
		{
			uint32_t _flags_save_mapi_response = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_REMAINING|LIBNDR_FLAG_NOALIGN);
			if (r->out.mapi_response == NULL) {
				return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
			}
			{
				struct ndr_push *_ndr_mapi_response;
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_mapi_response, 4, -1));
				NDR_CHECK(ndr_push_mapi_response(_ndr_mapi_response, NDR_SCALARS|NDR_BUFFERS, r->out.mapi_response));
				obfuscate_data(_ndr_mapi_response->data, _ndr_mapi_response->alloc_size, 0xA5);
				NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_mapi_response, 4, -1));
			}
			ndr->flags = _flags_save_mapi_response;
		}
		if (r->out.length == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, *r->out.length));
		NDR_CHECK(ndr_push_MAPISTATUS(ndr, NDR_SCALARS, r->out.result));
	}
	return NDR_ERR_SUCCESS;
}


_PUBLIC_ enum ndr_err_code ndr_push_EcDoConnectEx(struct ndr_push *ndr, int flags, const struct EcDoConnectEx *r)
{
	uint32_t	cntr_rgwClientVersion_0;
	uint32_t	cntr_rgwServerVersion_0;
	uint32_t	cntr_rgwBestVersion_0;

	if (flags & NDR_IN) {
		NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, ndr_charset_length(r->in.szUserDN, CH_DOS)));
		NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, 0));
		NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, ndr_charset_length(r->in.szUserDN, CH_DOS)));
		NDR_CHECK(ndr_push_charset(ndr, NDR_SCALARS, r->in.szUserDN, ndr_charset_length(r->in.szUserDN, CH_DOS), sizeof (uint8_t), CH_DOS));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->in.ulFlags));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->in.ulConMod));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->in.cbLimit));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->in.ulCpid));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->in.ulLcidString));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->in.ulLcidSort));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->in.ulIcxrLink));
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->in.usFCanConvertCodePages));
		for (cntr_rgwClientVersion_0 = 0; cntr_rgwClientVersion_0 < 3; cntr_rgwClientVersion_0++) {
			NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->in.rgwClientVersion[cntr_rgwClientVersion_0]));
		}
		if (r->in.pulTimeStamp == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, *r->in.pulTimeStamp));
		{
			/* Encode contents to temporary buffer to determine size */
			uint32_t 	size_rgbAuxIn_0 = 0;
			uint32_t	_flags_save_mapi2k7_AuxInfo = ndr->flags;
			struct ndr_push	*_ndr_rgbAuxIn;

			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REMAINING);
			NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_rgbAuxIn, 4, -1));

			if (r->in.cbAuxIn) {
				if (r->in.rgbAuxIn == NULL) {
					return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
				}
				NDR_CHECK(ndr_push_mapi2k7_AuxInfo(_ndr_rgbAuxIn, NDR_SCALARS|NDR_BUFFERS, r->in.rgbAuxIn));
			}

			/* Extract encoded size */
			size_rgbAuxIn_0 = _ndr_rgbAuxIn->offset;

			/* Push conformant array of encoded size bytes */
			NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_rgbAuxIn, 4, -1));
			ndr->flags = _flags_save_mapi2k7_AuxInfo;

			/* Value in cbAuxIn is not used, size was calculated when rgbAuxIn was pushed above */ 
			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, size_rgbAuxIn_0));
		}
		if (r->in.pcbAuxOut == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, *r->in.pcbAuxOut));
	}

	if (flags & NDR_OUT) {
		if (r->out.handle == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_policy_handle(ndr, NDR_SCALARS|NDR_BUFFERS, r->out.handle));
		if (r->out.pcmsPollsMax == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, *r->out.pcmsPollsMax));
		if (r->out.pcRetry == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, *r->out.pcRetry));
		if (r->out.pcmsRetryDelay == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, *r->out.pcmsRetryDelay));
		if (r->out.picxr == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, *r->out.picxr));
		if (r->out.szDNPrefix == NULL || *r->out.szDNPrefix == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_unique_ptr(ndr, *r->out.szDNPrefix));
		if (r->out.szDNPrefix) {
			NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, ndr_charset_length(*r->out.szDNPrefix, CH_DOS)));
			NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, 0));
			NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, ndr_charset_length(*r->out.szDNPrefix, CH_DOS)));
			NDR_CHECK(ndr_push_charset(ndr, NDR_SCALARS, *r->out.szDNPrefix, ndr_charset_length(*r->out.szDNPrefix, CH_DOS), sizeof(uint8_t), CH_DOS));
		}
		if (r->out.szDisplayName == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer: szDisplayName");
		}
		NDR_CHECK(ndr_push_unique_ptr(ndr, *r->out.szDisplayName));
		if (*r->out.szDisplayName) {
			NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, ndr_charset_length(*r->out.szDisplayName, CH_DOS)));
			NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, 0));
			NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, ndr_charset_length(*r->out.szDisplayName, CH_DOS)));
			NDR_CHECK(ndr_push_charset(ndr, NDR_SCALARS, *r->out.szDisplayName, ndr_charset_length(*r->out.szDisplayName, CH_DOS), sizeof(uint8_t), CH_DOS));
		}
		for (cntr_rgwServerVersion_0 = 0; cntr_rgwServerVersion_0 < 3; cntr_rgwServerVersion_0++) {
			NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->out.rgwServerVersion[cntr_rgwServerVersion_0]));
		}
		for (cntr_rgwBestVersion_0 = 0; cntr_rgwBestVersion_0 < 3; cntr_rgwBestVersion_0++) {
			NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->out.rgwBestVersion[cntr_rgwBestVersion_0]));
		}
		if (r->out.pulTimeStamp == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer: pulTimeStamp");
		}
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, *r->out.pulTimeStamp));

		{
			uint32_t 	size_rgbAuxOut_0 = 0;

			/* Does rgbAuxOut exist (it is optional) */
			if (r->out.rgbAuxOut == NULL) {
				/* No, push empty conformant-varying array */
				NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, 0));
				NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, 0));
				NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, 0));
			}
			else {
				/* Yes, encode contents to temporary buffer to determine size */
				uint32_t	_flags_save_mapi2k7_AuxInfo = ndr->flags;
				struct ndr_push	*_ndr_rgbAuxOut;

				ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REMAINING);
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_rgbAuxOut, 4, -1));

				NDR_CHECK(ndr_push_mapi2k7_AuxInfo(_ndr_rgbAuxOut, NDR_SCALARS|NDR_BUFFERS, r->out.rgbAuxOut));

				/* Extract encoded size */
				size_rgbAuxOut_0 = _ndr_rgbAuxOut->offset;

				/* Push conformant-varying array of encoded size bytes */
				NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, size_rgbAuxOut_0));
				NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, 0));
				NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_rgbAuxOut, 4, -1));

				ndr->flags = _flags_save_mapi2k7_AuxInfo;
			}

			/* Value in pcbAuxOut is not used, size was calculated when rgbAuxOut was pushed above */
			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, size_rgbAuxOut_0));
		}

		NDR_CHECK(ndr_push_MAPISTATUS(ndr, NDR_SCALARS, r->out.result));
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_EcDoConnectEx(struct ndr_pull *ndr, int flags, struct EcDoConnectEx *r)
{
	uint32_t	_ptr_szDNPrefix;
	uint32_t	_ptr_szDisplayName;
	uint32_t	cntr_rgwClientVersion_0;
	uint32_t	cntr_rgwServerVersion_0;
	uint32_t	cntr_rgwBestVersion_0;
	TALLOC_CTX	*_mem_save_handle_0;
	TALLOC_CTX	*_mem_save_pcmsPollsMax_0;
	TALLOC_CTX	*_mem_save_pcRetry_0;
	TALLOC_CTX	*_mem_save_pcmsRetryDelay_0;
	TALLOC_CTX	*_mem_save_picxr_0;
	TALLOC_CTX	*_mem_save_szDNPrefix_0;
	TALLOC_CTX	*_mem_save_szDNPrefix_1;
	TALLOC_CTX	*_mem_save_szDisplayName_0;
	TALLOC_CTX	*_mem_save_szDisplayName_1;
	TALLOC_CTX	*_mem_save_pulTimeStamp_0;
	TALLOC_CTX	*_mem_save_rgbAuxIn_0;
	TALLOC_CTX	*_mem_save_pcbAuxOut_0;
	TALLOC_CTX	*_mem_save_rgbAuxOut_1;

	if (flags & NDR_IN) {
		ZERO_STRUCT(r->out);

		NDR_CHECK(ndr_pull_array_size(ndr, &r->in.szUserDN));
		NDR_CHECK(ndr_pull_array_length(ndr, &r->in.szUserDN));
		if (ndr_get_array_length(ndr, &r->in.szUserDN) > ndr_get_array_size(ndr, &r->in.szUserDN)) {
			return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, "Bad array size %u should exceed array length %u", ndr_get_array_size(ndr, &r->in.szUserDN), ndr_get_array_length(ndr, &r->in.szUserDN));
		}
		NDR_CHECK(ndr_check_string_terminator(ndr, ndr_get_array_length(ndr, &r->in.szUserDN), sizeof(uint8_t)));
		NDR_CHECK(ndr_pull_charset(ndr, NDR_SCALARS, &r->in.szUserDN, ndr_get_array_length(ndr, &r->in.szUserDN), sizeof(uint8_t), CH_DOS));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->in.ulFlags));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->in.ulConMod));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->in.cbLimit));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->in.ulCpid));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->in.ulLcidString));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->in.ulLcidSort));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->in.ulIcxrLink));
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->in.usFCanConvertCodePages));
		for (cntr_rgwClientVersion_0 = 0; cntr_rgwClientVersion_0 < 3; cntr_rgwClientVersion_0++) {
			NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->in.rgwClientVersion[cntr_rgwClientVersion_0]));
		}
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->in.pulTimeStamp);
		}
		_mem_save_pulTimeStamp_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->in.pulTimeStamp, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->in.pulTimeStamp));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_pulTimeStamp_0, LIBNDR_FLAG_REF_ALLOC);
		{
			uint32_t _flags_save_mapi2k7_AuxInfo = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REMAINING);
			if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
				NDR_PULL_ALLOC(ndr, r->in.rgbAuxIn);
			}
			_mem_save_rgbAuxIn_0 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, r->in.rgbAuxIn, LIBNDR_FLAG_REF_ALLOC);
			{
				struct ndr_pull *_ndr_rgbAuxIn;
				NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_rgbAuxIn, 4, -1));
				NDR_CHECK(ndr_pull_mapi2k7_AuxInfo(_ndr_rgbAuxIn, NDR_SCALARS|NDR_BUFFERS, r->in.rgbAuxIn));
				NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_rgbAuxIn, 4, -1));
			}
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_rgbAuxIn_0, LIBNDR_FLAG_REF_ALLOC);
			ndr->flags = _flags_save_mapi2k7_AuxInfo;
		}
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->in.cbAuxIn));
		if (r->in.cbAuxIn > 0x1008) {
			return ndr_pull_error(ndr, NDR_ERR_RANGE, "[in] cbAuxIn value out of range: 0x%x\n", r->in.cbAuxIn);
		}
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->in.pcbAuxOut);
		}
		_mem_save_pcbAuxOut_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->in.pcbAuxOut, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->in.pcbAuxOut));
		if (r->in.pcbAuxOut && *r->in.pcbAuxOut > 0x1008) {
			return ndr_pull_error(ndr, NDR_ERR_RANGE, "[in] pcbAuxOut value out of range: 0x%x\n", *r->in.pcbAuxOut);
		}
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_pcbAuxOut_0, LIBNDR_FLAG_REF_ALLOC);
		NDR_PULL_ALLOC(ndr, r->out.handle);
		ZERO_STRUCTP(r->out.handle);
		NDR_PULL_ALLOC(ndr, r->out.pcmsPollsMax);
		ZERO_STRUCTP(r->out.pcmsPollsMax);
		NDR_PULL_ALLOC(ndr, r->out.pcRetry);
		ZERO_STRUCTP(r->out.pcRetry);
		NDR_PULL_ALLOC(ndr, r->out.pcmsRetryDelay);
		ZERO_STRUCTP(r->out.pcmsRetryDelay);
		NDR_PULL_ALLOC(ndr, r->out.picxr);
		ZERO_STRUCTP(r->out.picxr);
		NDR_PULL_ALLOC(ndr, r->out.szDNPrefix);
		ZERO_STRUCTP(r->out.szDNPrefix);
		NDR_PULL_ALLOC(ndr, r->out.szDisplayName);
		ZERO_STRUCTP(r->out.szDisplayName);
		NDR_PULL_ALLOC(ndr, r->out.pulTimeStamp);
		*r->out.pulTimeStamp = *r->in.pulTimeStamp;
		NDR_PULL_ALLOC(ndr, r->out.pcbAuxOut);
		*r->out.pcbAuxOut = *r->in.pcbAuxOut;
	}

	if (flags & NDR_OUT) {
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.handle);
		}
		_mem_save_handle_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.handle, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_policy_handle(ndr, NDR_SCALARS|NDR_BUFFERS, r->out.handle));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_handle_0, LIBNDR_FLAG_REF_ALLOC);
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.pcmsPollsMax);
		}
		_mem_save_pcmsPollsMax_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.pcmsPollsMax, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->out.pcmsPollsMax));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_pcmsPollsMax_0, LIBNDR_FLAG_REF_ALLOC);
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.pcRetry);
		}
		_mem_save_pcRetry_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.pcRetry, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->out.pcRetry));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_pcRetry_0, LIBNDR_FLAG_REF_ALLOC);
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.pcmsRetryDelay);
		}
		_mem_save_pcmsRetryDelay_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.pcmsRetryDelay, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->out.pcmsRetryDelay));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_pcmsRetryDelay_0, LIBNDR_FLAG_REF_ALLOC);
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.picxr);
		}
		_mem_save_picxr_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.picxr, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->out.picxr));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_picxr_0, LIBNDR_FLAG_REF_ALLOC);

		_mem_save_szDNPrefix_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.szDNPrefix, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_szDNPrefix));
		if (_ptr_szDNPrefix) {
			NDR_PULL_ALLOC(ndr, *r->out.szDNPrefix);
		} else {
			*r->out.szDNPrefix = NULL;
		}
		if (*r->out.szDNPrefix) {
			_mem_save_szDNPrefix_1 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, *r->out.szDNPrefix, 0);
			NDR_CHECK(ndr_pull_array_size(ndr, r->out.szDNPrefix));
			NDR_CHECK(ndr_pull_array_length(ndr, r->out.szDNPrefix));
			if (ndr_get_array_length(ndr, r->out.szDNPrefix) > ndr_get_array_size(ndr, r->out.szDNPrefix)) {
				return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, "Bad array size %u should exceed array length %u", ndr_get_array_size(ndr, r->out.szDNPrefix), ndr_get_array_length(ndr, r->out.szDNPrefix));
			}
			NDR_CHECK(ndr_check_string_terminator(ndr, ndr_get_array_length(ndr, r->out.szDNPrefix), sizeof(uint8_t)));
			NDR_CHECK(ndr_pull_charset(ndr, NDR_SCALARS, r->out.szDNPrefix, ndr_get_array_length(ndr, r->out.szDNPrefix), sizeof(uint8_t), CH_DOS));
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_szDNPrefix_1, 0);
		}
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_szDNPrefix_0, LIBNDR_FLAG_REF_ALLOC);

		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.szDisplayName);
		}
		_mem_save_szDisplayName_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.szDisplayName, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_szDisplayName));
		if (_ptr_szDisplayName) {
			NDR_PULL_ALLOC(ndr, *r->out.szDisplayName);
		} else {
			*r->out.szDisplayName = NULL;
		}
		if (*r->out.szDisplayName) {
			_mem_save_szDisplayName_1 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, *r->out.szDisplayName, 0);
			NDR_CHECK(ndr_pull_array_size(ndr, r->out.szDisplayName));
			NDR_CHECK(ndr_pull_array_length(ndr, r->out.szDisplayName));
			if (ndr_get_array_length(ndr, r->out.szDisplayName) > ndr_get_array_size(ndr, r->out.szDisplayName)) {
				return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, "Bad array size %u should exceed array length %u", ndr_get_array_size(ndr, r->out.szDisplayName), ndr_get_array_length(ndr, r->out.szDisplayName));
			}
			NDR_CHECK(ndr_check_string_terminator(ndr, ndr_get_array_length(ndr, r->out.szDisplayName), sizeof(uint8_t)));
			NDR_CHECK(ndr_pull_charset(ndr, NDR_SCALARS, r->out.szDisplayName, ndr_get_array_length(ndr, r->out.szDisplayName), sizeof(uint8_t), CH_DOS));
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_szDisplayName_1, 0);
		}
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_szDisplayName_0, LIBNDR_FLAG_REF_ALLOC);

		for (cntr_rgwServerVersion_0 = 0; cntr_rgwServerVersion_0 < 3; cntr_rgwServerVersion_0++) {
			NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->out.rgwServerVersion[cntr_rgwServerVersion_0]));
		}
		for (cntr_rgwBestVersion_0 = 0; cntr_rgwBestVersion_0 < 3; cntr_rgwBestVersion_0++) {
			NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->out.rgwBestVersion[cntr_rgwBestVersion_0]));
		}
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.pulTimeStamp);
		}
		_mem_save_pulTimeStamp_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.pulTimeStamp, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->out.pulTimeStamp));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_pulTimeStamp_0, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_array_size(ndr, &r->out.rgbAuxOut));
		NDR_CHECK(ndr_pull_array_length(ndr, &r->out.rgbAuxOut));
		if (ndr_get_array_length(ndr, &r->out.rgbAuxOut) > ndr_get_array_size(ndr, &r->out.rgbAuxOut)) {
			return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, "Bad array size %u should exceed array length %u", ndr_get_array_size(ndr, &r->out.rgbAuxOut), ndr_get_array_length(ndr, &r->out.rgbAuxOut));
		}
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC_N(ndr, r->out.rgbAuxOut, ndr_get_array_size(ndr, &r->out.rgbAuxOut));
		}
		/* Only try to pull rgbAuxOut if the fake array size is > 0 */
		if (ndr_get_array_size(ndr, &r->out.rgbAuxOut)) {
			_mem_save_rgbAuxOut_1 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, r->out.rgbAuxOut, 0);
			NDR_CHECK(ndr_pull_mapi2k7_AuxInfo(ndr, NDR_SCALARS, r->out.rgbAuxOut));
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_rgbAuxOut_1, 0);
		}
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.pcbAuxOut);
		}
		_mem_save_pcbAuxOut_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.pcbAuxOut, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->out.pcbAuxOut));
		if (r->out.pcbAuxOut && *r->out.pcbAuxOut > 0x1008) {
			return ndr_pull_error(ndr, NDR_ERR_RANGE, "value out of range !!");
		}
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_pcbAuxOut_0, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_MAPISTATUS(ndr, NDR_SCALARS, &r->out.result));
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ void ndr_print_EcDoConnectEx(struct ndr_print *ndr, const char *name, int flags, const struct EcDoConnectEx *r)
{
	uint32_t	cntr_rgwClientVersion_0;
	uint32_t	cntr_rgwServerVersion_0;
	uint32_t	cntr_rgwBestVersion_0;

	ndr_print_struct(ndr, name, "EcDoConnectEx");
	ndr->depth++;
	if (flags & NDR_SET_VALUES) {
		ndr->flags |= LIBNDR_PRINT_SET_VALUES;
	}
	if (flags & NDR_IN) {
		ndr_print_struct(ndr, "in", "EcDoConnectEx");
		ndr->depth++;
		ndr_print_string(ndr, "szUserDN", r->in.szUserDN);
		ndr_print_uint32(ndr, "ulFlags", r->in.ulFlags);
		ndr_print_uint32(ndr, "ulConMod", r->in.ulConMod);
		ndr_print_uint32(ndr, "cbLimit", r->in.cbLimit);
		ndr_print_uint32(ndr, "ulCpid", r->in.ulCpid);
		ndr_print_uint32(ndr, "ulLcidString", r->in.ulLcidString);
		ndr_print_uint32(ndr, "ulLcidSort", r->in.ulLcidSort);
		ndr_print_uint32(ndr, "ulIcxrLink", r->in.ulIcxrLink);
		ndr_print_uint16(ndr, "usFCanConvertCodePages", r->in.usFCanConvertCodePages);
		ndr->print(ndr, "%s: ARRAY(%d)", "rgwClientVersion", (int)3);
		ndr->depth++;
		for (cntr_rgwClientVersion_0=0;cntr_rgwClientVersion_0<3;cntr_rgwClientVersion_0++) {
			char *idx_0=NULL;
			if (asprintf(&idx_0, "[%d]", cntr_rgwClientVersion_0) != -1) {
				ndr_print_uint16(ndr, "rgwClientVersion", r->in.rgwClientVersion[cntr_rgwClientVersion_0]);
				free(idx_0);
			}
		}
		ndr->depth--;
		ndr_print_ptr(ndr, "pulTimeStamp", r->in.pulTimeStamp);
		ndr->depth++;
		ndr_print_uint32(ndr, "pulTimeStamp", *r->in.pulTimeStamp);
		ndr->depth--;
		ndr_print_ptr(ndr, "rgbAuxIn", r->in.rgbAuxIn);
		if (r->in.rgbAuxIn) {
			ndr->depth++;
			ndr_print_mapi2k7_AuxInfo(ndr, "rgbAuxIn", r->in.rgbAuxIn);
			ndr->depth--;
		}
		ndr_print_uint32(ndr, "cbAuxIn", r->in.cbAuxIn);
		ndr_print_ptr(ndr, "pcbAuxOut", r->in.pcbAuxOut);
		ndr->depth++;
		ndr_print_uint32(ndr, "pcbAuxOut", *r->in.pcbAuxOut);
		ndr->depth--;
		ndr->depth--;
	}
	if (flags & NDR_OUT) {
		ndr_print_struct(ndr, "out", "EcDoConnectEx");
		ndr->depth++;
		ndr_print_ptr(ndr, "handle", r->out.handle);
		ndr->depth++;
		ndr_print_policy_handle(ndr, "handle", r->out.handle);
		ndr->depth--;
		ndr_print_ptr(ndr, "pcmsPollsMax", r->out.pcmsPollsMax);
		ndr->depth++;
		ndr_print_uint32(ndr, "pcmsPollsMax", *r->out.pcmsPollsMax);
		ndr->depth--;
		ndr_print_ptr(ndr, "pcRetry", r->out.pcRetry);
		ndr->depth++;
		ndr_print_uint32(ndr, "pcRetry", *r->out.pcRetry);
		ndr->depth--;
		ndr_print_ptr(ndr, "pcmsRetryDelay", r->out.pcmsRetryDelay);
		ndr->depth++;
		ndr_print_uint32(ndr, "pcmsRetryDelay", *r->out.pcmsRetryDelay);
		ndr->depth--;
		ndr_print_ptr(ndr, "picxr", r->out.picxr);
		ndr->depth++;
		ndr_print_uint32(ndr, "picxr", *r->out.picxr);
		ndr->depth--;
		ndr_print_ptr(ndr, "szDNPrefix", r->out.szDisplayName);
		ndr->depth++;
		if (r->out.szDNPrefix && *r->out.szDNPrefix) {
			ndr_print_ptr(ndr, "szDNPrefix", *r->out.szDNPrefix);
			ndr->depth++;
			ndr_print_string(ndr, "szDNPrefix", *r->out.szDNPrefix);
			ndr->depth--;
		}
		ndr->depth--;
		ndr_print_ptr(ndr, "szDisplayName", r->out.szDisplayName);
		ndr->depth++;
		if (r->out.szDisplayName && *r->out.szDisplayName) {
			ndr_print_ptr(ndr, "szDisplayName", *r->out.szDisplayName);
			ndr->depth++;
			ndr_print_string(ndr, "szDisplayName", *r->out.szDisplayName);
			ndr->depth--;
		}
		ndr->depth--;
		ndr->print(ndr, "%s: ARRAY(%d)", "rgwServerVersion", (int)3);
		ndr->depth++;
		for (cntr_rgwServerVersion_0=0;cntr_rgwServerVersion_0<3;cntr_rgwServerVersion_0++) {
			char *idx_0=NULL;
			if (asprintf(&idx_0, "[%d]", cntr_rgwServerVersion_0) != -1) {
				ndr_print_uint16(ndr, "rgwServerVersion", r->out.rgwServerVersion[cntr_rgwServerVersion_0]);
				free(idx_0);
			}
		}
		ndr->depth--;
		ndr->print(ndr, "%s: ARRAY(%d)", "rgwBestVersion", (int)3);
		ndr->depth++;
		for (cntr_rgwBestVersion_0=0;cntr_rgwBestVersion_0<3;cntr_rgwBestVersion_0++) {
			char *idx_0=NULL;
			if (asprintf(&idx_0, "[%d]", cntr_rgwBestVersion_0) != -1) {
				ndr_print_uint16(ndr, "rgwBestVersion", r->out.rgwBestVersion[cntr_rgwBestVersion_0]);
				free(idx_0);
			}
		}
		ndr->depth--;
		ndr_print_ptr(ndr, "pulTimeStamp", r->out.pulTimeStamp);
                if (r->out.pulTimeStamp) {
                        ndr->depth++;
                        ndr_print_uint32(ndr, "pulTimeStamp", *r->out.pulTimeStamp);
                        ndr->depth--;
                }
		ndr_print_ptr(ndr, "rgbAuxOut", r->out.rgbAuxOut);
		if (r->out.rgbAuxOut && r->out.pcbAuxOut) {
			ndr->depth++;
			ndr_print_mapi2k7_AuxInfo(ndr, "rgbAuxOut", r->out.rgbAuxOut);
			ndr->depth--;
		}
		ndr_print_ptr(ndr, "pcbAuxOut", r->out.pcbAuxOut);
                if (r->out.pcbAuxOut) {
                        ndr->depth++;
                        ndr_print_uint32(ndr, "pcbAuxOut", *r->out.pcbAuxOut);
                        ndr->depth--;
                }
		ndr_print_MAPISTATUS(ndr, "result", r->out.result);
		ndr->depth--;
	}
	ndr->depth--;
}

_PUBLIC_ void ndr_print_EcDoRpcExt(struct ndr_print *ndr, const char *name, int flags, const struct EcDoRpcExt *r)
{
	DATA_BLOB		rgbIn;
	DATA_BLOB		rgbOut;
	struct ndr_pull		*ndr_pull;
	struct mapi2k7_request	*mapi_request;
	struct mapi2k7_response	*mapi_response;
	TALLOC_CTX		*mem_ctx;

	mem_ctx = talloc_named(NULL, 0, "ndr_print_EcDoRpcExt");

	ndr_print_struct(ndr, name, "EcDoRpcExt");
	if (r == NULL) { ndr_print_null(ndr); return; }
	ndr->depth++;
	if (flags & NDR_SET_VALUES) {
		ndr->flags |= LIBNDR_PRINT_SET_VALUES;
	}
	if (flags & NDR_IN) {
		ndr_print_struct(ndr, "in", "EcDoRpcExt");
		ndr->depth++;
		ndr_print_ptr(ndr, "handle", r->in.handle);
		ndr->depth++;
		ndr_print_policy_handle(ndr, "handle", r->in.handle);
		ndr->depth--;
		ndr_print_ptr(ndr, "pulFlags", r->in.pulFlags);
		ndr->depth++;
		ndr_print_uint32(ndr, "pulFlags", *r->in.pulFlags);
		ndr->depth--;

		/* Put MAPI request blob into a ndr_pull structure */
		if (r->in.cbIn) {
			rgbIn.data = talloc_memdup(mem_ctx, r->in.rgbIn, r->in.cbIn);
			rgbIn.length = r->in.cbIn;
			ndr_pull = ndr_pull_init_blob(&rgbIn, mem_ctx);
			ndr_set_flags(&ndr_pull->flags, LIBNDR_FLAG_NOALIGN);
			while (ndr_pull->offset < ndr_pull->data_size) {
				mapi_request = talloc_zero(mem_ctx, struct mapi2k7_request);
				mapi_request->mapi_request = talloc_zero(mapi_request, struct mapi_request);
				if (ndr_pull_mapi2k7_request(ndr_pull, NDR_SCALARS|NDR_BUFFERS, mapi_request) == NDR_ERR_SUCCESS) {
					ndr_print_mapi2k7_request(ndr, "mapi_request", (const struct mapi2k7_request *)mapi_request);
				} else {
					dump_data(0, ndr_pull->data + ndr_pull->offset, ndr_pull->data_size - ndr_pull->offset);
					talloc_free(mapi_request);
					break;
				}
				talloc_free(mapi_request);
			}
			talloc_free(ndr_pull);
			talloc_free(rgbIn.data);
		}

		ndr_print_uint32(ndr, "cbIn", r->in.cbIn);
		ndr_print_ptr(ndr, "pcbOut", r->in.pcbOut);
		ndr->depth++;
		ndr_print_uint32(ndr, "pcbOut", *r->in.pcbOut);
		ndr->depth--;
		ndr_print_array_uint8(ndr, "Reserved0", r->in.Reserved0, *r->in.Reserved1);
		ndr_print_ptr(ndr, "Reserved1", r->in.Reserved1);
		ndr->depth++;
		ndr_print_uint32(ndr, "Reserved1", *r->in.Reserved1);
		ndr->depth--;
		ndr->depth--;
	}
	if (flags & NDR_OUT) {
		ndr_print_struct(ndr, "out", "EcDoRpcExt");
		ndr->depth++;
		ndr_print_ptr(ndr, "handle", r->out.handle);
		ndr->depth++;
		ndr_print_policy_handle(ndr, "handle", r->out.handle);
		ndr->depth--;
		ndr_print_ptr(ndr, "pulFlags", r->out.pulFlags);
		ndr->depth++;
		ndr_print_uint32(ndr, "pulFlags", *r->out.pulFlags);
		ndr->depth--;

		/* Put MAPI response blob into a ndr_pull structure */
		if (*r->out.pcbOut) {
			rgbOut.data = talloc_memdup(mem_ctx, r->out.rgbOut, *r->out.pcbOut);
			rgbOut.length = *r->out.pcbOut;
			ndr_pull = ndr_pull_init_blob(&rgbOut, mem_ctx);
			ndr_set_flags(&ndr_pull->flags, LIBNDR_FLAG_NOALIGN);
			while (ndr_pull->offset < ndr_pull->data_size) {
				mapi_response = talloc_zero(NULL, struct mapi2k7_response);
				mapi_response->mapi_response = talloc_zero(mapi_response, struct mapi_response);
				if (ndr_pull_mapi2k7_response(ndr_pull, NDR_SCALARS|NDR_BUFFERS, mapi_response) == NDR_ERR_SUCCESS) {
					ndr_print_mapi2k7_response(ndr, "mapi_response", (const struct mapi2k7_response *)mapi_response);
				} else {
					dump_data(0, ndr_pull->data + ndr_pull->offset, ndr_pull->data_size - ndr_pull->offset);
					talloc_free(mapi_response);
					break;
				}
				talloc_free(mapi_response);
			}
			talloc_free(ndr_pull);
			talloc_free(rgbOut.data);
		}

		ndr_print_ptr(ndr, "pcbOut", r->out.pcbOut);
		ndr->depth++;
		ndr_print_uint32(ndr, "pcbOut", *r->out.pcbOut);
		ndr->depth--;
		ndr_print_array_uint8(ndr, "Reserved0", r->out.Reserved0, *r->out.Reserved1);
		ndr_print_ptr(ndr, "Reserved1", r->out.Reserved1);
		ndr->depth++;
		ndr_print_uint32(ndr, "Reserved1", *r->out.Reserved1);
		ndr->depth--;
		ndr_print_ptr(ndr, "pulTransTime", r->out.pulTransTime);
		ndr->depth++;
		ndr_print_uint32(ndr, "pulTransTime", *r->out.pulTransTime);
		ndr->depth--;
		ndr_print_MAPISTATUS(ndr, "result", r->out.result);
		ndr->depth--;
	}
	ndr->depth--;

	talloc_free(mem_ctx);
}

_PUBLIC_ enum ndr_err_code ndr_push_EcDoRpcExt2(struct ndr_push *ndr, int flags, const struct EcDoRpcExt2 *r)
{
	NDR_PUSH_CHECK_FN_FLAGS(ndr, flags);
	if (flags & NDR_IN) {
		if (r->in.handle == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_policy_handle(ndr, NDR_SCALARS|NDR_BUFFERS, r->in.handle));
		if (r->in.pulFlags == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, *r->in.pulFlags));
		{
			/* Encode contents to temporary buffer to determine size */
			uint32_t 	size_rgbIn_0 = 0;			
			uint32_t	_flags_save_mapi2k7_request = ndr->flags;
			struct ndr_push	*_ndr_rgbIn;

			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REMAINING);
			NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_rgbIn, 4, -1));

			if (r->in.cbIn) {
				if (r->in.rgbIn == NULL) {
					return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
				}
				NDR_CHECK(ndr_push_mapi2k7_request(_ndr_rgbIn, NDR_SCALARS|NDR_BUFFERS, r->in.rgbIn));
			}

			/* Extract encoded size */
			size_rgbIn_0 = _ndr_rgbIn->offset;

			/* Push conformant array of encoded size bytes */
			NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_rgbIn, 4, -1));
			ndr->flags = _flags_save_mapi2k7_request;

			/* Value in cbIn is not used, size was calculated when rgbIn was pushed above */ 
			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, size_rgbIn_0));
		}
		if (r->in.pcbOut == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, *r->in.pcbOut));
		NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, r->in.cbAuxIn));
		NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, r->in.rgbAuxIn, r->in.cbAuxIn));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->in.cbAuxIn));
		if (r->in.pcbAuxOut == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, *r->in.pcbAuxOut));
	}
	if (flags & NDR_OUT) {
		if (r->out.handle == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_policy_handle(ndr, NDR_SCALARS|NDR_BUFFERS, r->out.handle));
		if (r->out.pulFlags == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, *r->out.pulFlags));
		{
			uint32_t 	size_rgbOut_0 = 0;			

			/* Does rgbOut exist (it is optional) */
			if (r->out.rgbOut == NULL) {
				/* No, push empty conformant-varying array */
				NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, 0));
				NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, 0));
				NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, 0));		
			}
			else {
				/* Yes, encode contents to temporary buffer to determine size */
				uint32_t	_flags_save_mapi2k7_response = ndr->flags;
				struct ndr_push	*_ndr_rgbOut;

				ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REMAINING);
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_rgbOut, 4, -1));

				NDR_CHECK(ndr_push_mapi2k7_response(_ndr_rgbOut, NDR_SCALARS|NDR_BUFFERS, r->out.rgbOut));

				/* Extract encoded size */
				size_rgbOut_0 = _ndr_rgbOut->offset;

				/* Push conformant-varying array of encoded size bytes */
				NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, size_rgbOut_0));
				NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, 0));
				NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_rgbOut, 4, -1));

				ndr->flags = _flags_save_mapi2k7_response;
			}

			/* Value in pcbOut is not used, size was calculated when rgbOut was pushed above */ 
			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, size_rgbOut_0));
		}
		NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, *r->out.pcbAuxOut));
		NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, 0));
		NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, *r->out.pcbAuxOut));
		NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, r->out.rgbAuxOut, *r->out.pcbAuxOut));
		if (r->out.pcbAuxOut == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, *r->out.pcbAuxOut));
		if (r->out.pulTransTime == NULL) {
			return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER, "NULL [ref] pointer");
		}
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, *r->out.pulTransTime));
		NDR_CHECK(ndr_push_MAPISTATUS(ndr, NDR_SCALARS, r->out.result));
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_EcDoRpcExt2(struct ndr_pull *ndr, int flags, struct EcDoRpcExt2 *r)
{
	uint32_t size_rgbOut_0 = 0;
	uint32_t length_rgbOut_0 = 0;
	uint32_t size_rgbAuxIn_0 = 0;
	uint32_t size_rgbAuxOut_0 = 0;
	uint32_t length_rgbAuxOut_0 = 0;
	TALLOC_CTX *_mem_save_handle_0;
	TALLOC_CTX *_mem_save_pulFlags_0;
	TALLOC_CTX *_mem_save_rgbIn_0;
	TALLOC_CTX *_mem_save_pcbOut_0;
	TALLOC_CTX *_mem_save_pcbAuxOut_0;
	TALLOC_CTX *_mem_save_pulTransTime_0;
	TALLOC_CTX *_mem_save_rgbOut_0;
	
	NDR_PULL_CHECK_FN_FLAGS(ndr, flags);
	if (flags & NDR_IN) {
		ZERO_STRUCT(r->out);

		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->in.handle);
		}
		_mem_save_handle_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->in.handle, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_policy_handle(ndr, NDR_SCALARS|NDR_BUFFERS, r->in.handle));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_handle_0, LIBNDR_FLAG_REF_ALLOC);
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->in.pulFlags);
		}
		_mem_save_pulFlags_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->in.pulFlags, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->in.pulFlags));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_pulFlags_0, LIBNDR_FLAG_REF_ALLOC);
		{
			uint32_t _flags_save_mapi2k7_request = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REMAINING);
			if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
				NDR_PULL_ALLOC(ndr, r->in.rgbIn);
			}
			_mem_save_rgbIn_0 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, r->in.rgbIn, LIBNDR_FLAG_REF_ALLOC);
			{
				struct ndr_pull *_ndr_rgbIn;
				NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_rgbIn, 4, -1));
				NDR_CHECK(ndr_pull_mapi2k7_request(_ndr_rgbIn, NDR_SCALARS|NDR_BUFFERS, r->in.rgbIn));
				NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_rgbIn, 4, -1));
			}
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_rgbIn_0, LIBNDR_FLAG_REF_ALLOC);
			ndr->flags = _flags_save_mapi2k7_request;
		}
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->in.cbIn));
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->in.pcbOut);
		}
		_mem_save_pcbOut_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->in.pcbOut, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->in.pcbOut));
		if (*r->in.pcbOut > 0x40000) {
			return ndr_pull_error(ndr, NDR_ERR_RANGE, "value out of range");
		}
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_pcbOut_0, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_array_size(ndr, &r->in.rgbAuxIn));
		size_rgbAuxIn_0 = ndr_get_array_size(ndr, &r->in.rgbAuxIn);
		NDR_PULL_ALLOC_N(ndr, r->in.rgbAuxIn, size_rgbAuxIn_0);
		NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, r->in.rgbAuxIn, size_rgbAuxIn_0));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->in.cbAuxIn));
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->in.pcbAuxOut);
		}
		_mem_save_pcbAuxOut_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->in.pcbAuxOut, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->in.pcbAuxOut));
		if (*r->in.pcbAuxOut > 0x1008) {
			return ndr_pull_error(ndr, NDR_ERR_RANGE, "value out of range");
		}
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_pcbAuxOut_0, LIBNDR_FLAG_REF_ALLOC);
		NDR_PULL_ALLOC(ndr, r->out.handle);
		*r->out.handle = *r->in.handle;
		NDR_PULL_ALLOC(ndr, r->out.pulFlags);
		*r->out.pulFlags = *r->in.pulFlags;
		NDR_PULL_ALLOC(ndr, r->out.pcbOut);
		*r->out.pcbOut = *r->in.pcbOut;
		NDR_PULL_ALLOC(ndr, r->out.pcbAuxOut);
		*r->out.pcbAuxOut = *r->in.pcbAuxOut;
		NDR_PULL_ALLOC(ndr, r->out.pulTransTime);
		ZERO_STRUCTP(r->out.pulTransTime);
		if (r->in.rgbAuxIn) {
			NDR_CHECK(ndr_check_array_size(ndr, (void*)&r->in.rgbAuxIn, r->in.cbAuxIn));
		}
	}
	if (flags & NDR_OUT) {
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.handle);
		}
		_mem_save_handle_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.handle, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_policy_handle(ndr, NDR_SCALARS|NDR_BUFFERS, r->out.handle));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_handle_0, LIBNDR_FLAG_REF_ALLOC);
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.pulFlags);
		}
		_mem_save_pulFlags_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.pulFlags, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->out.pulFlags));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_pulFlags_0, LIBNDR_FLAG_REF_ALLOC);
		{
			NDR_CHECK(ndr_pull_array_size(ndr, &r->out.rgbOut));
			NDR_CHECK(ndr_pull_array_length(ndr, &r->out.rgbOut));
			size_rgbOut_0 = ndr_get_array_size(ndr, &r->out.rgbOut);
			length_rgbOut_0 = ndr_get_array_length(ndr, &r->out.rgbOut);
			if (length_rgbOut_0 > size_rgbOut_0) {
				return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, "Bad array size %u should exceed array length %u", size_rgbOut_0, length_rgbOut_0);
			}
			if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
				NDR_PULL_ALLOC(ndr, r->out.rgbOut);
			}
			/* Only try to pull rgbOut if the fake array size is > 0 */
			if (ndr_get_array_size(ndr, &r->out.rgbOut)) {
				_mem_save_rgbOut_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->out.rgbOut, 0);
				NDR_CHECK(ndr_pull_mapi2k7_response(ndr, NDR_SCALARS, r->out.rgbOut));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_rgbOut_0, 0);
			}
		}		
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.pcbOut);
		}
		_mem_save_pcbOut_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.pcbOut, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->out.pcbOut));
		if (*r->out.pcbOut > 0x40000) {
			return ndr_pull_error(ndr, NDR_ERR_RANGE, "value out of range");
		}
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_pcbOut_0, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_array_size(ndr, &r->out.rgbAuxOut));
		NDR_CHECK(ndr_pull_array_length(ndr, &r->out.rgbAuxOut));
		size_rgbAuxOut_0 = ndr_get_array_size(ndr, &r->out.rgbAuxOut);
		length_rgbAuxOut_0 = ndr_get_array_length(ndr, &r->out.rgbAuxOut);
		if (length_rgbAuxOut_0 > size_rgbAuxOut_0) {
			return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, "Bad array size %u should exceed array length %u", size_rgbAuxOut_0, length_rgbAuxOut_0);
		}
		NDR_PULL_ALLOC_N(ndr, r->out.rgbAuxOut, size_rgbAuxOut_0);
		NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, r->out.rgbAuxOut, length_rgbAuxOut_0));
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.pcbAuxOut);
		}
		_mem_save_pcbAuxOut_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.pcbAuxOut, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->out.pcbAuxOut));
		if (*r->out.pcbAuxOut > 0x1008) {
			return ndr_pull_error(ndr, NDR_ERR_RANGE, "value out of range");
		}
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_pcbAuxOut_0, LIBNDR_FLAG_REF_ALLOC);
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.pulTransTime);
		}
		_mem_save_pulTransTime_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->out.pulTransTime, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, r->out.pulTransTime));
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_pulTransTime_0, LIBNDR_FLAG_REF_ALLOC);
		NDR_CHECK(ndr_pull_MAPISTATUS(ndr, NDR_SCALARS, &r->out.result));
		if (r->out.rgbAuxOut) {
			NDR_CHECK(ndr_check_array_size(ndr, (void*)&r->out.rgbAuxOut, *r->out.pcbAuxOut));
		}
		if (r->out.rgbAuxOut) {
			NDR_CHECK(ndr_check_array_length(ndr, (void*)&r->out.rgbAuxOut, *r->out.pcbAuxOut));
		}
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ void ndr_print_EcDoRpcExt2(struct ndr_print *ndr, const char *name, int flags, const struct EcDoRpcExt2 *r)
{
	uint32_t		cntr_rgbAuxOut_0;
	DATA_BLOB		rgbAuxIn;
	TALLOC_CTX		*mem_ctx;

	mem_ctx = talloc_named(NULL, 0, "ndr_print_EcDoRpcExt2");

	ndr_print_struct(ndr, name, "EcDoRpcExt2");
	ndr->depth++;
	if (flags & NDR_SET_VALUES) {
		ndr->flags |= LIBNDR_PRINT_SET_VALUES;
	}
	if (flags & NDR_IN) {
		ndr_print_struct(ndr, "in", "EcDoRpcExt2");
		ndr->depth++;
		ndr_print_ptr(ndr, "handle", r->in.handle);
		ndr->depth++;
		ndr_print_policy_handle(ndr, "handle", r->in.handle);
		ndr->depth--;
		ndr_print_ptr(ndr, "pulFlags", r->in.pulFlags);
		ndr->depth++;
		ndr_print_uint32(ndr, "pulFlags", *r->in.pulFlags);
		ndr->depth--;
		ndr_print_ptr(ndr, "rgbIn", r->in.rgbIn);
		if (r->in.rgbAuxIn) {
			ndr->depth++;
			ndr_print_mapi2k7_request(ndr, "rgbIn", r->in.rgbIn);
			ndr->depth--;
			ndr->depth--;
		}
		ndr_print_uint32(ndr, "cbIn", r->in.cbIn);
		ndr_print_ptr(ndr, "pcbOut", r->in.pcbOut);
		ndr->depth++;
		ndr_print_uint32(ndr, "pcbOut", *r->in.pcbOut);
		ndr->depth--;

		rgbAuxIn.data = r->in.rgbAuxIn;
		rgbAuxIn.length = r->in.cbAuxIn;
		ndr_print_DATA_BLOB(ndr, "rgbAuxIn", rgbAuxIn);
		/* ndr_print_array_uint8(ndr, "rgbAuxIn", r->in.rgbAuxIn, r->in.cbAuxIn); */
		ndr_print_uint32(ndr, "cbAuxIn", r->in.cbAuxIn);
		ndr_print_ptr(ndr, "pcbAuxOut", r->in.pcbAuxOut);
		ndr->depth++;
		ndr_print_uint32(ndr, "pcbAuxOut", *r->in.pcbAuxOut);
		ndr->depth--;
		ndr->depth--;
	}
	if (flags & NDR_OUT) {
		ndr_print_struct(ndr, "out", "EcDoRpcExt2");
		ndr->depth++;
		ndr_print_ptr(ndr, "handle", r->out.handle);
		ndr->depth++;
		ndr_print_policy_handle(ndr, "handle", r->out.handle);
		ndr->depth--;
		ndr_print_ptr(ndr, "pulFlags", r->out.pulFlags);
		ndr->depth++;
		ndr_print_uint32(ndr, "pulFlags", *r->out.pulFlags);
		ndr->depth--;
		ndr_print_ptr(ndr, "rgbOut", r->out.rgbOut);
		if (r->in.rgbAuxIn) {
			ndr->depth++;
			ndr_print_mapi2k7_response(ndr, "rgbOut", r->out.rgbOut);
			ndr->depth--;
			ndr->depth--;
		}
		ndr_print_ptr(ndr, "pcbOut", r->out.pcbOut);
		if (r->out.pcbOut) {
			ndr->depth++;
			ndr_print_uint32(ndr, "pcbOut", *r->out.pcbOut);
			ndr->depth--;
		}
		if (r->out.rgbAuxOut && r->out.pcbAuxOut) {
			ndr->print(ndr, "%s: ARRAY(%d)", "rgbAuxOut", (int)*r->out.pcbAuxOut);
			ndr->depth++;
			for (cntr_rgbAuxOut_0=0;cntr_rgbAuxOut_0<*r->out.pcbAuxOut;cntr_rgbAuxOut_0++) {
				char *idx_0=NULL;
				if (asprintf(&idx_0, "[%d]", cntr_rgbAuxOut_0) != -1) {
					ndr_print_uint32(ndr, "rgbAuxOut", r->out.rgbAuxOut[cntr_rgbAuxOut_0]);
					free(idx_0);
				}
			}
		} else {
			ndr->print(ndr, "%s: NULL", "rgbAuxOut");
		}
		ndr->depth--;
		ndr_print_ptr(ndr, "pcbAuxOut", r->out.pcbAuxOut);
		if (r->out.pcbAuxOut) {
			ndr->depth++;
			ndr_print_uint32(ndr, "pcbAuxOut", *r->out.pcbAuxOut);
			ndr->depth--;
		}
		ndr_print_ptr(ndr, "pulTransTime", r->out.pulTransTime);
		if (r->out.pulTransTime) {
			ndr->depth++;
			ndr_print_uint32(ndr, "pulTransTime", *r->out.pulTransTime);
			ndr->depth--;
		}
		ndr_print_MAPISTATUS(ndr, "result", r->out.result);
		ndr->depth--;
	}
	ndr->depth--;

	talloc_free(mem_ctx);
}

/*
  We need to pull QueryRows replies on our own:
  If we have no results, do not push/pull the DATA_BLOB
*/

enum ndr_err_code ndr_push_QueryRows_repl(struct ndr_push *ndr, int ndr_flags, const struct QueryRows_repl *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_push_align(ndr, 4));
			NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, r->Origin));
			NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->RowCount));

			if (r->RowCount) {
				uint32_t _flags_save_DATA_BLOB = ndr->flags;
				ndr_set_flags(&ndr->flags, LIBNDR_FLAG_REMAINING);
				NDR_CHECK(ndr_push_DATA_BLOB(ndr, NDR_SCALARS, r->RowData));
				ndr->flags = _flags_save_DATA_BLOB;
			}
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_QueryRows_repl(struct ndr_pull *ndr, int ndr_flags, struct QueryRows_repl *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_pull_align(ndr, 4));
			NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &r->Origin));
			NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->RowCount));

			if (r->RowCount)
			{
				uint32_t _flags_save_DATA_BLOB = ndr->flags;

				ndr_set_flags(&ndr->flags, LIBNDR_FLAG_REMAINING);
				NDR_CHECK(ndr_pull_DATA_BLOB(ndr, NDR_SCALARS, &r->RowData));
				ndr->flags = _flags_save_DATA_BLOB;
			} else {
				r->RowData.length = 0;
				r->RowData.data = NULL;
			}
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}


enum ndr_err_code ndr_push_Logon_req(struct ndr_push *ndr, int ndr_flags, const struct Logon_req *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_push_align(ndr, 4));
			NDR_CHECK(ndr_push_LogonFlags(ndr, NDR_SCALARS, r->LogonFlags));
			NDR_CHECK(ndr_push_OpenFlags(ndr, NDR_SCALARS, r->OpenFlags));
			NDR_CHECK(ndr_push_StoreState(ndr, NDR_SCALARS, r->StoreState));
			if (r->EssDN && r->EssDN[0] != '\0') {
				uint32_t _flags_save_string = ndr->flags;
				ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_SIZE2);
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->EssDN));
				ndr->flags = _flags_save_string;
			} else {
				NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, 0));
			}
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

/* MoveFolder */
enum ndr_err_code ndr_push_MoveFolder_repl(struct ndr_push *ndr, int ndr_flags, const struct MoveFolder_repl *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_push_align(ndr, 4));
			if (r->HasDestHandleIndex) {
				NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->DestHandleIndex));
			}
			NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, r->PartialCompletion));
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_MoveFolder_repl(struct ndr_pull *ndr, int ndr_flags, struct MoveFolder_repl *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_pull_align(ndr, 4));
			if (r->HasDestHandleIndex) {
				NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->DestHandleIndex));
			}
			NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &r->PartialCompletion));
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ void ndr_print_MoveFolder_repl(struct ndr_print *ndr, const char *name, const struct MoveFolder_repl *r)
{
	ndr_print_struct(ndr, name, "MoveFolder_repl");
	if (r == NULL) { ndr_print_null(ndr); return; }
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		ndr->depth++;
		if (r->HasDestHandleIndex) {
			ndr_print_uint32(ndr, "DestHandleIndex", r->DestHandleIndex);
		}
		ndr_print_uint8(ndr, "PartialCompletion", r->PartialCompletion);
		ndr->depth--;
		ndr->flags = _flags_save_STRUCT;
	}
}
/* /MoveFolder */

/* CopyFolder */
enum ndr_err_code ndr_push_CopyFolder_repl(struct ndr_push *ndr, int ndr_flags, const struct CopyFolder_repl *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_push_align(ndr, 4));
			if (r->HasDestHandleIndex) {
				NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->DestHandleIndex));
			}
			NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, r->PartialCompletion));
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_CopyFolder_repl(struct ndr_pull *ndr, int ndr_flags, struct CopyFolder_repl *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_pull_align(ndr, 4));
			if (r->HasDestHandleIndex) {
				NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->DestHandleIndex));
			}
			NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &r->PartialCompletion));
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ void ndr_print_CopyFolder_repl(struct ndr_print *ndr, const char *name, const struct CopyFolder_repl *r)
{
	ndr_print_struct(ndr, name, "CopyFolder_repl");
	if (r == NULL) { ndr_print_null(ndr); return; }
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		ndr->depth++;
		if (r->HasDestHandleIndex) {
			ndr_print_uint32(ndr, "DestHandleIndex", r->DestHandleIndex);
		}
		ndr_print_uint8(ndr, "PartialCompletion", r->PartialCompletion);
		ndr->depth--;
		ndr->flags = _flags_save_STRUCT;
	}
}
/* /CopyFolder */

_PUBLIC_ void ndr_print_SBinary_short(struct ndr_print *ndr, const char *name, const struct SBinary_short *r)
{
	ndr->print(ndr, "%-25s: SBinary_short cb=%u", name, (unsigned)r->cb);
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		ndr->depth++;
		ndr_dump_data(ndr, r->lpb, r->cb);
		ndr->depth--;
		ndr->flags = _flags_save_STRUCT;
	}
}

_PUBLIC_ void ndr_print_Binary_r(struct ndr_print *ndr, const char *name, const struct Binary_r *r)
{
	ndr->print(ndr, "%-25s: Binary_r cb=%u", name, (unsigned)r->cb);
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		ndr->depth++;
		dump_data(0, r->lpb, r->cb);
		ndr->depth--;
		ndr->flags = _flags_save_STRUCT;
	}
}

_PUBLIC_ void ndr_print_XID(struct ndr_print *ndr, const char *name, const struct XID *r)
{
	uint32_t	_flags_save_STRUCT = ndr->flags;
	char		*guid_str;
	char		*line;
	int		i;

	if (r == NULL) {
		ndr->print(ndr, "%s: NULL", name);
		return;
	}

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	guid_str = GUID_string(NULL, &r->NameSpaceGuid);
	line = talloc_asprintf(NULL, " ");
	for (i = 0; i < r->LocalId.length; i++) {
		line = talloc_asprintf_append(line, "%02X ", r->LocalId.data[i]);
	}
	if (name) {
		ndr->print(ndr, "%s: {%s}:%s", name, guid_str, line);
	} else {
		ndr->print(ndr, "{%s}:%s", guid_str, line);
	}
	talloc_free(guid_str);
	talloc_free(line);
	ndr->flags = _flags_save_STRUCT;
}

_PUBLIC_ void ndr_dump_data(struct ndr_print *ndr, const uint8_t *buf, int len)
{
	TALLOC_CTX *mem_ctx;
	char *line = NULL;
	int i=0, j=0, idx=0;

	if (len<=0) return;

	mem_ctx = talloc_named(NULL, 0, "ndr_dump_data");
	if (!mem_ctx) return;

	for (i=0;i<len;) {
		idx = i;
		if (i%16 == 0) {
			if (i<len)  {
				line = talloc_asprintf(mem_ctx, "[%04X] ", i);
			}
		}

		line = talloc_asprintf_append(line, "%02X ", (int)buf[i]);
		i++;
		if (i%8 == 0) {
			line = talloc_asprintf_append(line, "  ");
		}
		if (i%16 == 0) {
			idx = i - 16;
			for (j=0; j < 8; j++, idx++) {
				line = talloc_asprintf_append(line, "%c", isprint(buf[idx]) ? buf[idx] : '.');
			}

			line = talloc_asprintf_append(line, " ");
			for (j=8; j < 16; j++, idx++) {
				line = talloc_asprintf_append(line, "%c", isprint(buf[idx]) ? buf[idx] : '.');
			}
			ndr->print(ndr, "%s", line);
			talloc_free(line);
		}
	}

	if (i%16) {
		int n;

		n = 16 - (i%16);
		idx = i - (i%16);
		if (n>8) {
			line = talloc_asprintf_append(line, "  ");
		}
		while (n--) {
			line = talloc_asprintf_append(line, "   ");
		}
		line = talloc_asprintf_append(line, "  ");
		n = MIN(8,i%16);
		for (j = 0; j < n; j++, idx++) {
			line = talloc_asprintf_append(line, "%c", isprint(buf[idx]) ? buf[idx] : '.');
		}
		line = talloc_asprintf_append(line, " ");
		n = (i%16) - n;
		if (n>0) {
			for (j = i - n; j < i + n; j++, idx++) {
				line = talloc_asprintf_append(line, "%c", isprint(buf[idx]) ? buf[idx] : '.');
			}
		}
		ndr->print(ndr, "%s", line);
		talloc_free(line);
	}
}

_PUBLIC_ void ndr_print_SBinary(struct ndr_print *ndr, const char *name, const struct SBinary *r)
{
	ndr->print(ndr, "%-25s: SBinary cb=%u", name, (unsigned)r->cb);
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		ndr->depth++;
		ndr_dump_data(ndr, r->lpb, r->cb);
		ndr->depth--;
		ndr->flags = _flags_save_STRUCT;
	}
}

_PUBLIC_ void ndr_print_fuzzyLevel(struct ndr_print *ndr, const char *name, uint32_t r)
{
	ndr_print_uint32(ndr, name, r);
	ndr->depth++;
	switch ((r & 0x0000FFFF)) {
	case FL_FULLSTRING:
		ndr->print(ndr, "%-25s: FL_FULLSTRING", "lower  16 bits");
		break;
	case FL_SUBSTRING:
		ndr->print(ndr, "%-25s: FL_SUBSTRING", "lower  16 bits");
		break;
	case FL_PREFIX:
		ndr->print(ndr, "%-25s: FL_PREFIX", "lower  16 bits");
		break;
	}
	ndr->print(ndr, "%-25s", "higher 16 bits");
	ndr_print_bitmap_flag(ndr, sizeof(uint32_t), "FL_IGNORECASE", FL_IGNORECASE, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint32_t), "FL_IGNORENONSPACE", FL_IGNORENONSPACE, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint32_t), "FL_LOOSE", FL_LOOSE, r);
	ndr->depth--;
}

/*
 * Fake wrapper over mapi_SRestriction. Workaround the no-pointer deep
 * recursion problem in pidl
 */
enum ndr_err_code ndr_push_mapi_SRestriction_wrap(struct ndr_push *ndr, int ndr_flags, const struct mapi_SRestriction_wrap *r)
{
	return ndr_push_mapi_SRestriction(ndr, ndr_flags, (struct mapi_SRestriction *)r);
}

enum ndr_err_code ndr_pull_mapi_SRestriction_wrap(struct ndr_pull *ndr, int ndr_flags, struct mapi_SRestriction_wrap *r)
{
	return ndr_pull_mapi_SRestriction(ndr, ndr_flags, (struct mapi_SRestriction *)r);
}

void ndr_print_mapi_SRestriction_wrap(struct ndr_print *ndr, const char *name, const struct mapi_SRestriction_wrap *r)
{
	ndr_print_mapi_SRestriction(ndr, name, (const struct mapi_SRestriction *)r);
}

/*
 * Fake wrapper over mapi_SPropValue. Workaround the no-pointer deep
 * recursion problem in pidl
 */
enum ndr_err_code ndr_push_mapi_SPropValue_wrap(struct ndr_push *ndr, int ndr_flags, const struct mapi_SPropValue_wrap *r)
{
	NDR_CHECK(ndr_push_align(ndr, 8));
	return ndr_push_mapi_SPropValue(ndr, NDR_SCALARS, (const struct mapi_SPropValue *)r);
}

enum ndr_err_code ndr_pull_mapi_SPropValue_wrap(struct ndr_pull *ndr, int ndr_flags, struct mapi_SPropValue_wrap *r)
{
	return ndr_pull_mapi_SPropValue(ndr, NDR_SCALARS, (struct mapi_SPropValue *)r);
}

void ndr_print_mapi_SPropValue_wrap(struct ndr_print *ndr, const char *name, const struct mapi_SPropValue_wrap *r)
{
	ndr_print_mapi_SPropValue(ndr, name, (const struct mapi_SPropValue *)r);
}


/*
 * Fake wrapper over mapi_SPropValue_array. Workaround the no-pointer deep
 * recursion problem in pidl
 */
enum ndr_err_code ndr_push_mapi_SPropValue_array_wrap(struct ndr_push *ndr, int ndr_flags, const struct mapi_SPropValue_array_wrap *r)
{
	NDR_CHECK(ndr_push_align(ndr, 8));
	return ndr_push_mapi_SPropValue_array(ndr, NDR_SCALARS, (const struct mapi_SPropValue_array *)r);
}

enum ndr_err_code ndr_pull_mapi_SPropValue_array_wrap(struct ndr_pull *ndr, int ndr_flags, struct mapi_SPropValue_array_wrap *r)
{
	return ndr_pull_mapi_SPropValue_array(ndr, NDR_SCALARS, (struct mapi_SPropValue_array *)r);
}

void ndr_print_mapi_SPropValue_array_wrap(struct ndr_print *ndr, const char *name, const struct mapi_SPropValue_array_wrap *r)
{
	ndr_print_mapi_SPropValue_array(ndr, name, (const struct mapi_SPropValue_array *)r);
}

enum ndr_err_code ndr_push_RestrictionVariable(struct ndr_push *ndr, int ndr_flags, const union RestrictionVariable *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			int level = ndr_push_get_switch_value(ndr, r);
			switch (level) {
				case 0x0: {
					break; }

				case 0x1: {
					NDR_CHECK(ndr_push_mapi_SRestriction_comment(ndr, NDR_SCALARS, &r->res[0]));
				break; }

				default:
					return ndr_push_error(ndr, NDR_ERR_BAD_SWITCH, "Bad switch value %u", level);
			}
		}
		if (ndr_flags & NDR_BUFFERS) {
			int level = ndr_push_get_switch_value(ndr, r);
			switch (level) {
				case 0x0:
				break;

				case 0x1:
					if (r->res) {
						NDR_CHECK(ndr_push_mapi_SRestriction_comment(ndr, NDR_BUFFERS, &r->res[0]));
					}
				break;

				default:
					return ndr_push_error(ndr, NDR_ERR_BAD_SWITCH, "Bad switch value %u", level);
			}
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code  ndr_pull_RestrictionVariable(struct ndr_pull *ndr, int ndr_flags, union RestrictionVariable *r)
{
	int level;
	TALLOC_CTX *_mem_save_res_0;
	level = ndr_pull_get_switch_value(ndr, r);
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);

		if (ndr_flags & NDR_SCALARS) {
			switch (level) {
				case 0x0: {
				break; }

				case 0x1: {
					NDR_CHECK(ndr_pull_align(ndr, 4));
					NDR_PULL_ALLOC_N(ndr, r->res, 1);
					_mem_save_res_0 = NDR_PULL_GET_MEM_CTX(ndr);
					NDR_PULL_SET_MEM_CTX(ndr, r->res, 0);
					NDR_CHECK(ndr_pull_mapi_SRestriction_comment(ndr, NDR_SCALARS, &r->res[0]));
					NDR_PULL_SET_MEM_CTX(ndr, _mem_save_res_0, 0);
				break; }

				default:
					return ndr_pull_error(ndr, NDR_ERR_BAD_SWITCH, "Bad switch value %u", level);
			}
		}
		if (ndr_flags & NDR_BUFFERS) {
			switch (level) {
				case 0x0:
				break;

				case 0x1:
					if (r->res) {
						_mem_save_res_0 = NDR_PULL_GET_MEM_CTX(ndr);
						NDR_PULL_SET_MEM_CTX(ndr, r->res, 0);
						NDR_CHECK(ndr_pull_mapi_SRestriction_comment(ndr, NDR_BUFFERS, &r->res[0]));
						NDR_PULL_SET_MEM_CTX(ndr, _mem_save_res_0, 0);
				break; }

				default:
					return ndr_pull_error(ndr, NDR_ERR_BAD_SWITCH, "Bad switch value %u", level);
			}
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}


_PUBLIC_ void ndr_print_RestrictionVariable(struct ndr_print *ndr, const char *name, const union RestrictionVariable *r)
{
	int level;
	level = ndr_print_get_switch_value(ndr, r);
	ndr_print_union(ndr, name, level, "RestrictionVariable");
	switch (level) {
		case 0x0:
		break;

		case 0x1:
			ndr_print_ptr(ndr, "res", r->res);
			ndr->depth++;
			if (r->res) {
				ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
				ndr_print_mapi_SRestriction_comment(ndr, "res", &r->res[0]);
			}
			ndr->depth--;
		break;
	}
}

enum ndr_err_code ndr_push_Release_req(struct ndr_push *ndr, int ndr_flags, const struct Release_req *r)
{
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_Release_req(struct ndr_pull *ndr, int ndr_flags, struct Release_req *r)
{
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_push_Release_repl(struct ndr_push *ndr, int ndr_flags, const struct Release_repl *r)
{
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_Release_repl(struct ndr_pull *ndr, int ndr_flags, struct Release_repl *r)
{
	return NDR_ERR_SUCCESS;
}


enum ndr_err_code ndr_push_GetSearchCriteria_repl(struct ndr_push *ndr, int ndr_flags, const struct GetSearchCriteria_repl *r)
{
	uint32_t cntr_FolderIds_0;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_push_align(ndr, 8));
			NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->RestrictionDataSize));
			if (r->RestrictionDataSize) {
				struct ndr_push *_ndr_RestrictionData;
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_RestrictionData, 0, r->RestrictionDataSize));
				NDR_CHECK(ndr_push_mapi_SRestriction(_ndr_RestrictionData, NDR_SCALARS|NDR_BUFFERS, &r->RestrictionData));
				NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_RestrictionData, 0, r->RestrictionDataSize));
			}
			NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, r->LogonId));
			NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->FolderIdCount));
			for (cntr_FolderIds_0 = 0; cntr_FolderIds_0 < r->FolderIdCount; cntr_FolderIds_0++) {
				NDR_CHECK(ndr_push_hyper(ndr, NDR_SCALARS, r->FolderIds[cntr_FolderIds_0]));
			}
			NDR_CHECK(ndr_push_SearchFlags(ndr, NDR_SCALARS, r->SearchFlags));
			NDR_CHECK(ndr_push_trailer_align(ndr, 8));
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}


enum ndr_err_code ndr_pull_GetSearchCriteria_repl(struct ndr_pull *ndr, int ndr_flags, struct GetSearchCriteria_repl *r)
{
	uint32_t cntr_FolderIds_0;
	TALLOC_CTX *_mem_save_FolderIds_0;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_pull_align(ndr, 8));
			NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->RestrictionDataSize));
			if (r->RestrictionDataSize) {
				struct ndr_pull *_ndr_RestrictionData;
				NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_RestrictionData, 0, r->RestrictionDataSize));
				NDR_CHECK(ndr_pull_mapi_SRestriction(_ndr_RestrictionData, NDR_SCALARS|NDR_BUFFERS, &r->RestrictionData));
				NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_RestrictionData, 0, r->RestrictionDataSize));
			}
			NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &r->LogonId));
			NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->FolderIdCount));
			NDR_PULL_ALLOC_N(ndr, r->FolderIds, r->FolderIdCount);
			_mem_save_FolderIds_0 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, r->FolderIds, 0);
			for (cntr_FolderIds_0 = 0; cntr_FolderIds_0 < r->FolderIdCount; cntr_FolderIds_0++) {
				NDR_CHECK(ndr_pull_hyper(ndr, NDR_SCALARS, &r->FolderIds[cntr_FolderIds_0]));
			}
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_FolderIds_0, 0);
			NDR_CHECK(ndr_pull_SearchFlags(ndr, NDR_SCALARS, &r->SearchFlags));
			NDR_CHECK(ndr_pull_trailer_align(ndr, 8));
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}


void ndr_print_GetSearchCriteria_repl(struct ndr_print *ndr, const char *name, const struct GetSearchCriteria_repl *r)
{
	uint32_t cntr_FolderIds_0;
	ndr_print_struct(ndr, name, "GetSearchCriteria_repl");
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		ndr->depth++;
		ndr_print_uint16(ndr, "RestrictionDataSize", r->RestrictionDataSize);
		if (r->RestrictionDataSize) {
			ndr_print_mapi_SRestriction(ndr, "RestrictionData", &r->RestrictionData);
		} else {
			ndr_print_uint8(ndr, "RestrictionData", 0);
		}
		ndr_print_uint8(ndr, "LogonId", r->LogonId);
		ndr_print_uint16(ndr, "FolderIdCount", r->FolderIdCount);
		ndr->print(ndr, "%s: ARRAY(%d)", "FolderIds", (int)r->FolderIdCount);
		ndr->depth++;
		for (cntr_FolderIds_0=0;cntr_FolderIds_0<r->FolderIdCount;cntr_FolderIds_0++) {
			char *idx_0=NULL;
			if (asprintf(&idx_0, "[%d]", cntr_FolderIds_0) != -1) {
				ndr_print_hyper(ndr, "FolderIds", r->FolderIds[cntr_FolderIds_0]);
				free(idx_0);
			}
		}
		ndr->depth--;
		ndr_print_SearchFlags(ndr, "SearchFlags", r->SearchFlags);
		ndr->depth--;
		ndr->flags = _flags_save_STRUCT;
	}
}

enum ndr_err_code ndr_push_Backoff_req(struct ndr_push *ndr, int ndr_flags, const struct Backoff_req *r)
{
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_Backoff_req(struct ndr_pull *ndr, int ndr_flags, struct Backoff_req *r)
{
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_push_Backoff_repl(struct ndr_push *ndr, int ndr_flags, const struct Backoff_repl *r)
{
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_Backoff_repl(struct ndr_pull *ndr, int ndr_flags, struct Backoff_repl *r)
{
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_push_BufferTooSmall_req(struct ndr_push *ndr, int ndr_flags, const struct BufferTooSmall_req *r)
{
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_BufferTooSmall_req(struct ndr_pull *ndr, int ndr_flags, struct BufferTooSmall_req *r)
{
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_push_BufferTooSmall_repl(struct ndr_push *ndr, int ndr_flags, const struct BufferTooSmall_repl *r)
{
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_BufferTooSmall_repl(struct ndr_pull *ndr, int ndr_flags, struct BufferTooSmall_repl *r)
{
	return NDR_ERR_SUCCESS;
}

/* property.idl */

_PUBLIC_ enum ndr_err_code ndr_push_ExtendedException(struct ndr_push *ndr, int ndr_flags, uint16_t WriterVersion2, const struct ExceptionInfo *ExceptionInfo, const struct ExtendedException *r)
{
	bool subjectIsSet, locationIsSet;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;


		subjectIsSet = (ExceptionInfo->OverrideFlags & ARO_SUBJECT) == ARO_SUBJECT;
		locationIsSet = (ExceptionInfo->OverrideFlags & ARO_LOCATION) == ARO_LOCATION;

		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		NDR_PUSH_CHECK_FLAGS(ndr, ndr_flags);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_push_align(ndr, 4));
			if (WriterVersion2 > 0x3008) {
				NDR_CHECK(ndr_push_ChangeHighlight(ndr, NDR_SCALARS, &r->ChangeHighlight));
			}
			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->ReservedBlockEE1Size));
			NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, r->ReservedBlockEE1, r->ReservedBlockEE1Size));
			if (subjectIsSet || locationIsSet) {
				NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->StartDateTime));
				NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->EndDateTime));
				NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->OriginalStartDate));
			}
			if (subjectIsSet) {
				uint32_t _flags_save_string = ndr->flags;
				ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_SIZE2|LIBNDR_FLAG_STR_NOTERM);
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->Subject));
				ndr->flags = _flags_save_string;
			}
			if (locationIsSet) {
				uint32_t _flags_save_string = ndr->flags;
				ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_SIZE2|LIBNDR_FLAG_STR_NOTERM);
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->Location));
				ndr->flags = _flags_save_string;
			}
			if (subjectIsSet || locationIsSet) {
				NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->ReservedBlockEE2Size));
				NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, r->ReservedBlockEE2, r->ReservedBlockEE2Size));
			}
			NDR_CHECK(ndr_push_trailer_align(ndr, 4));
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_ExtendedException(struct ndr_pull *ndr, int ndr_flags, uint16_t WriterVersion2, const struct ExceptionInfo *ExceptionInfo, struct ExtendedException *r)
{
	bool subjectIsSet, locationIsSet;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;

		subjectIsSet = (ExceptionInfo->OverrideFlags & ARO_SUBJECT) == ARO_SUBJECT;
		locationIsSet = (ExceptionInfo->OverrideFlags & ARO_LOCATION) == ARO_LOCATION;

		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		NDR_PULL_CHECK_FLAGS(ndr, ndr_flags);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_pull_align(ndr, 4));
			if (WriterVersion2 > 0x3008) {
				NDR_CHECK(ndr_pull_ChangeHighlight(ndr, NDR_SCALARS, &r->ChangeHighlight));
			}
			else {
				memset(&r->ChangeHighlight, 0, sizeof(struct ChangeHighlight));
			}
			NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->ReservedBlockEE1Size));
			NDR_PULL_ALLOC_N(ndr, r->ReservedBlockEE1, r->ReservedBlockEE1Size);
			NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, r->ReservedBlockEE1, r->ReservedBlockEE1Size));
			if (subjectIsSet || locationIsSet) {
				NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->StartDateTime));
				NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->EndDateTime));
				NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->OriginalStartDate));
			}
			if (subjectIsSet) {
				uint32_t _flags_save_string = ndr->flags;
				ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_SIZE2|LIBNDR_FLAG_STR_NOTERM);
				NDR_CHECK(ndr_pull_string(ndr, NDR_SCALARS, &r->Subject));
				ndr->flags = _flags_save_string;
			}
			if (locationIsSet) {
				uint32_t _flags_save_string = ndr->flags;
				ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_SIZE2|LIBNDR_FLAG_STR_NOTERM);
				NDR_CHECK(ndr_pull_string(ndr, NDR_SCALARS, &r->Location));
				ndr->flags = _flags_save_string;
			}
			if (subjectIsSet || locationIsSet) {
				NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->ReservedBlockEE2Size));
				NDR_PULL_ALLOC_N(ndr, r->ReservedBlockEE2, r->ReservedBlockEE2Size);
				NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, r->ReservedBlockEE2, r->ReservedBlockEE2Size));
			}
			NDR_CHECK(ndr_pull_trailer_align(ndr, 4));
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_push_AppointmentRecurrencePattern(struct ndr_push *ndr, int ndr_flags, const struct AppointmentRecurrencePattern *r)
{
	uint32_t cntr_ExceptionInfo_0;
	uint32_t cntr_ReservedBlock1_0;
	uint32_t cntr_ExtendedException_0;
	uint32_t cntr_ReservedBlock2_0;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		NDR_PUSH_CHECK_FLAGS(ndr, ndr_flags);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_push_align(ndr, 4));
			NDR_CHECK(ndr_push_RecurrencePattern(ndr, NDR_SCALARS, &r->RecurrencePattern));
			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->ReaderVersion2));
			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->WriterVersion2));
			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->StartTimeOffset));
			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->EndTimeOffset));
			NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->ExceptionCount));
			for (cntr_ExceptionInfo_0 = 0; cntr_ExceptionInfo_0 < r->ExceptionCount; cntr_ExceptionInfo_0++) {
				NDR_CHECK(ndr_push_ExceptionInfo(ndr, NDR_SCALARS, &r->ExceptionInfo[cntr_ExceptionInfo_0]));
			}
			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->ReservedBlock1Size));
			for (cntr_ReservedBlock1_0 = 0; cntr_ReservedBlock1_0 < r->ReservedBlock1Size; cntr_ReservedBlock1_0++) {
				NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->ReservedBlock1[cntr_ReservedBlock1_0]));
			}
			for (cntr_ExtendedException_0 = 0; cntr_ExtendedException_0 < r->ExceptionCount; cntr_ExtendedException_0++) {
				NDR_CHECK(ndr_push_ExtendedException(ndr, NDR_SCALARS, r->WriterVersion2, r->ExceptionInfo + cntr_ExtendedException_0, &r->ExtendedException[cntr_ExtendedException_0]));
			}
			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->ReservedBlock2Size));
			for (cntr_ReservedBlock2_0 = 0; cntr_ReservedBlock2_0 < r->ReservedBlock2Size; cntr_ReservedBlock2_0++) {
				NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->ReservedBlock2[cntr_ReservedBlock2_0]));
			}
			NDR_CHECK(ndr_push_trailer_align(ndr, 4));
		}
		if (ndr_flags & NDR_BUFFERS) {
			NDR_CHECK(ndr_push_RecurrencePattern(ndr, NDR_BUFFERS, &r->RecurrencePattern));
			for (cntr_ExceptionInfo_0 = 0; cntr_ExceptionInfo_0 < r->ExceptionCount; cntr_ExceptionInfo_0++) {
				NDR_CHECK(ndr_push_ExceptionInfo(ndr, NDR_BUFFERS, &r->ExceptionInfo[cntr_ExceptionInfo_0]));
			}
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_AppointmentRecurrencePattern(struct ndr_pull *ndr, int ndr_flags, struct AppointmentRecurrencePattern *r)
{
	uint32_t cntr_ExceptionInfo_0;
	TALLOC_CTX *_mem_save_ExceptionInfo_0;
	uint32_t cntr_ReservedBlock1_0;
	TALLOC_CTX *_mem_save_ReservedBlock1_0;
	uint32_t cntr_ExtendedException_0;
	TALLOC_CTX *_mem_save_ExtendedException_0;
	uint32_t cntr_ReservedBlock2_0;
	TALLOC_CTX *_mem_save_ReservedBlock2_0;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		NDR_PULL_CHECK_FLAGS(ndr, ndr_flags);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_pull_align(ndr, 4));
			NDR_CHECK(ndr_pull_RecurrencePattern(ndr, NDR_SCALARS, &r->RecurrencePattern));
			NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->ReaderVersion2));
			NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->WriterVersion2));
			NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->StartTimeOffset));
			NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->EndTimeOffset));
			NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->ExceptionCount));
			NDR_PULL_ALLOC_N(ndr, r->ExceptionInfo, r->ExceptionCount);
			_mem_save_ExceptionInfo_0 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, r->ExceptionInfo, 0);
			for (cntr_ExceptionInfo_0 = 0; cntr_ExceptionInfo_0 < r->ExceptionCount; cntr_ExceptionInfo_0++) {
				NDR_CHECK(ndr_pull_ExceptionInfo(ndr, NDR_SCALARS, &r->ExceptionInfo[cntr_ExceptionInfo_0]));
			}
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_ExceptionInfo_0, 0);

			/* It seems that some clients don't send the reserved fields */
			if (ndr->offset < ndr->data_size) {
				NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->ReservedBlock1Size));
				NDR_PULL_ALLOC_N(ndr, r->ReservedBlock1, r->ReservedBlock1Size);
				_mem_save_ReservedBlock1_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->ReservedBlock1, 0);
				for (cntr_ReservedBlock1_0 = 0; cntr_ReservedBlock1_0 < r->ReservedBlock1Size; cntr_ReservedBlock1_0++) {
					NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->ReservedBlock1[cntr_ReservedBlock1_0]));
				}
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_ReservedBlock1_0, 0);
				NDR_PULL_ALLOC_N(ndr, r->ExtendedException, r->ExceptionCount);
				_mem_save_ExtendedException_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->ExtendedException, 0);
				for (cntr_ExtendedException_0 = 0; cntr_ExtendedException_0 < r->ExceptionCount; cntr_ExtendedException_0++) {
					NDR_CHECK(ndr_pull_ExtendedException(ndr, NDR_SCALARS, r->WriterVersion2, r->ExceptionInfo + cntr_ExtendedException_0, &r->ExtendedException[cntr_ExtendedException_0]));
				}
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_ExtendedException_0, 0);
				NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->ReservedBlock2Size));
				NDR_PULL_ALLOC_N(ndr, r->ReservedBlock2, r->ReservedBlock2Size);
				_mem_save_ReservedBlock2_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->ReservedBlock2, 0);
				for (cntr_ReservedBlock2_0 = 0; cntr_ReservedBlock2_0 < r->ReservedBlock2Size; cntr_ReservedBlock2_0++) {
					NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->ReservedBlock2[cntr_ReservedBlock2_0]));
				}
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_ReservedBlock2_0, 0);
			}

			NDR_CHECK(ndr_pull_trailer_align(ndr, 4));
		} else {
			/* If there are extended exceptions we have missing data */
			if (r->ExceptionCount > 0) {
				/* FIXME: I think that NDR_ERR_INCOMPLETE_BUFFER is more appropiate but is not in our samba target version */
				return NDR_ERR_BUFSIZE;
			}
		}

		if (ndr_flags & NDR_BUFFERS) {
			NDR_CHECK(ndr_pull_RecurrencePattern(ndr, NDR_BUFFERS, &r->RecurrencePattern));
			_mem_save_ExceptionInfo_0 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, r->ExceptionInfo, 0);
			for (cntr_ExceptionInfo_0 = 0; cntr_ExceptionInfo_0 < r->ExceptionCount; cntr_ExceptionInfo_0++) {
				NDR_CHECK(ndr_pull_ExceptionInfo(ndr, NDR_BUFFERS, &r->ExceptionInfo[cntr_ExceptionInfo_0]));
			}
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_ExceptionInfo_0, 0);
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_push_PersistDataArray(struct ndr_push *ndr, int ndr_flags, const struct PersistDataArray *r)
{
	uint32_t cntr_lpPersistData_1;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		NDR_PUSH_CHECK_FLAGS(ndr, ndr_flags);
		if (ndr_flags & NDR_BUFFERS) {
			for (cntr_lpPersistData_1 = 0; cntr_lpPersistData_1 < r->cValues; cntr_lpPersistData_1++) {
				NDR_CHECK(ndr_push_PersistData(ndr, NDR_SCALARS|NDR_BUFFERS,
							       &r->lpPersistData[cntr_lpPersistData_1]));
			}
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_PersistDataArray(struct ndr_pull *ndr, int ndr_flags, struct PersistDataArray *r)
{
	uint32_t cntr_lpPersistData_0;
	TALLOC_CTX *_mem_save_lpPersistData_0;
	bool sentinel_reached = false;
	uint32_t current_size;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		NDR_PULL_CHECK_FLAGS(ndr, ndr_flags);
		if (ndr_flags & NDR_BUFFERS) {
			NDR_PULL_ALLOC(ndr, r->lpPersistData);
			if (r->lpPersistData) {
				_mem_save_lpPersistData_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->lpPersistData, 0);
				NDR_CHECK(ndr_token_store(ndr, &ndr->array_size_list,
							  &r->lpPersistData, ndr->data_size));
				NDR_PULL_ALLOC_N(ndr, r->lpPersistData, 1);
				cntr_lpPersistData_0 = 0;
				current_size = 0;
				while (!sentinel_reached && current_size < ndr->data_size) {
					NDR_CHECK(ndr_pull_PersistData(ndr, NDR_SCALARS|NDR_BUFFERS, &r->lpPersistData[cntr_lpPersistData_0]));
					sentinel_reached = (r->lpPersistData[cntr_lpPersistData_0].PersistID == PERSIST_SENTINEL);
					current_size += r->lpPersistData[cntr_lpPersistData_0].DataElementsSize + 2 + 2;
					cntr_lpPersistData_0++;
					r->lpPersistData = talloc_realloc(ndr->current_mem_ctx,
									  r->lpPersistData,
									  struct PersistData,
									  cntr_lpPersistData_0 + 1);
					if (!r->lpPersistData) {
						return ndr_pull_error(ndr, NDR_ERR_ALLOC,
								      "Alloc failed: %s\n",
								      __location__);
					}
				}
				if (current_size > 0 && cntr_lpPersistData_0 == 0 && !sentinel_reached) {
					/* TODO: Use NDR_ERR_INCOMPLETE_BUFFER */
					return NDR_ERR_BUFSIZE;
				}
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_lpPersistData_0, 0);
				r->cValues = cntr_lpPersistData_0;
			}
			if (r->lpPersistData) {
				NDR_CHECK(ndr_check_array_size(ndr, (void*)&r->lpPersistData, ndr->data_size));
			}
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_push_PersistElementArray(struct ndr_push *ndr, int ndr_flags, const struct PersistElementArray *r)
{
	uint32_t cntr_lpPersistElement_1;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		NDR_PUSH_CHECK_FLAGS(ndr, ndr_flags);
		if (ndr_flags & NDR_BUFFERS) {
			for (cntr_lpPersistElement_1 = 0; cntr_lpPersistElement_1 < r->cValues; cntr_lpPersistElement_1++) {
				NDR_CHECK(ndr_push_PersistElement(ndr, NDR_SCALARS|NDR_BUFFERS,
								  &r->lpPersistElement[cntr_lpPersistElement_1]));
			}
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_PersistElementArray(struct ndr_pull *ndr, int ndr_flags, struct PersistElementArray *r)
{
	uint32_t cntr_lpPersistElement_0;
	TALLOC_CTX *_mem_save_lpPersistElement_0;
	bool sentinel_reached = false;
	uint32_t current_size;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		NDR_PULL_CHECK_FLAGS(ndr, ndr_flags);
		if (ndr_flags & NDR_BUFFERS) {
			NDR_PULL_ALLOC(ndr, r->lpPersistElement);
			if (r->lpPersistElement) {
				_mem_save_lpPersistElement_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->lpPersistElement, 0);
				NDR_CHECK(ndr_token_store(ndr, &ndr->array_size_list,
							  &r->lpPersistElement, ndr->data_size));
				NDR_PULL_ALLOC_N(ndr, r->lpPersistElement, 1);
				cntr_lpPersistElement_0 = 0;
				current_size = 0;
				while (!sentinel_reached && current_size < ndr->data_size) {
					NDR_CHECK(ndr_pull_PersistElement(ndr, NDR_SCALARS, &r->lpPersistElement[cntr_lpPersistElement_0]));
					sentinel_reached = (r->lpPersistElement[cntr_lpPersistElement_0].ElementID == ELEMENT_SENTINEL);
					current_size += r->lpPersistElement[cntr_lpPersistElement_0].ElementDataSize + 2 + 2;
					cntr_lpPersistElement_0++;
					r->lpPersistElement = talloc_realloc(ndr->current_mem_ctx,
									     r->lpPersistElement,
									     struct PersistElement,
									     cntr_lpPersistElement_0 + 1);
					if (!r->lpPersistElement) {
						return ndr_pull_error(ndr, NDR_ERR_ALLOC,
								      "Alloc failed: %s\n",
								      __location__);
					}
				}
				if (current_size > 0 && cntr_lpPersistElement_0 == 0 && !sentinel_reached) {
					/* TODO: Use NDR_ERR_INCOMPLETE_BUFFER */
					return NDR_ERR_BUFSIZE;
				}
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_lpPersistElement_0, 0);
				r->cValues = cntr_lpPersistElement_0;
			}
			if (r->lpPersistElement) {
				NDR_CHECK(ndr_check_array_size(ndr, (void*)&r->lpPersistElement, ndr->data_size));
			}
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ void ndr_print_FILETIME(struct ndr_print *ndr, const char *name, const struct FILETIME *r)
{
	TALLOC_CTX		*mem_ctx;
	NTTIME			time;
	const char		*date;

	mem_ctx = talloc_named(NULL, 0, "FILETIME");
	if (!mem_ctx) { ndr_print_string(ndr, name, "ERROR"); return; }
	if (r == NULL) { ndr_print_string(ndr, name, "NULL"); return; }

	time = r->dwHighDateTime;
	time = time << 32;
	time |= r->dwLowDateTime;
	date = nt_time_string(mem_ctx, time);
	ndr_print_string(ndr, name, date);

	talloc_free(mem_ctx);
}
