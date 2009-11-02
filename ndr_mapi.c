/* 
   OpenChange implementation.

   libndr mapi support

   Copyright (C) Julien Kerihuel 2005-2009
   
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
#include <ndr.h>
#include <gen_ndr/ndr_exchange.h>


static void obfuscate_data(uint8_t *data, uint32_t size, uint8_t salt)
{
	uint32_t i;

	for (i=0; i<size; i++) {
		data[i] ^= salt;
	}
}

ssize_t lzxpress_decompress(const uint8_t *input,
			    uint32_t input_size,
			    uint8_t *output,
			    uint32_t max_output_size);

static enum ndr_err_code ndr_pull_lxpress_chunk(struct ndr_pull *ndrpull,
						struct ndr_push *ndrpush,
						ssize_t decompressed_len)
{
	DATA_BLOB	comp_chunk;
	DATA_BLOB	plain_chunk;
	int		ret;

	/* Step 1. Retrieve the compressed buf */
	comp_chunk.length = ndrpull->data_size;
	comp_chunk.data = ndrpull->data;

	plain_chunk.length = decompressed_len;
	plain_chunk.data = ndrpush->data;

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

	return NDR_ERR_SUCCESS;
}

static enum ndr_err_code ndr_pull_lzxpress_decompress(struct ndr_pull *subndr,
						      struct ndr_pull **_comndr,
						      ssize_t decompressed_len)
{
	struct ndr_push *ndrpush;
	struct ndr_pull *comndr;
	DATA_BLOB uncompressed;

	ndrpush = ndr_push_init_ctx(subndr, subndr->iconv_convenience);
	NDR_ERR_HAVE_NO_MEMORY(ndrpush);

	NDR_CHECK(ndr_pull_lxpress_chunk(subndr, ndrpush, decompressed_len));

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

	comndr->iconv_convenience = talloc_reference(comndr, subndr->iconv_convenience);
	*_comndr = comndr;
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
					if (r->header.Flags & RHEF_Compressed) {
						struct ndr_pull *_ndr_data_compressed = NULL;

						NDR_CHECK(ndr_pull_lzxpress_decompress(_ndr_buffer, &_ndr_data_compressed, r->header.SizeActual));
						NDR_CHECK(ndr_pull_mapi_request(_ndr_data_compressed, NDR_SCALARS|NDR_BUFFERS, r->mapi_request));
					} else if (r->header.Flags & RHEF_XorMagic) {
						obfuscate_data(_ndr_buffer->data, _ndr_buffer->data_size, 0xA5);
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
				
				NDR_CHECK((ndr_pull_subcontext_start(ndr, &_ndr_buffer, 0, -1)));
				{
					if (r->header.Flags & RHEF_Compressed) {
						struct ndr_pull *_ndr_data_compressed = NULL;
						
						NDR_CHECK(ndr_pull_lzxpress_decompress(_ndr_buffer, &_ndr_data_compressed, r->header.SizeActual));
						NDR_CHECK(ndr_pull_mapi_response(_ndr_data_compressed, NDR_SCALARS|NDR_BUFFERS, r->mapi_response));
					} else if (r->header.Flags & RHEF_XorMagic) {
						obfuscate_data(_ndr_buffer->data, _ndr_buffer->data_size, 0xA5);
						NDR_CHECK(ndr_pull_mapi_response(_ndr_buffer, NDR_SCALARS|NDR_BUFFERS, r->mapi_response));
					}
				}
				NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_buffer, 0, -1));
			}
			ndr->flags = _flags_save_mapi_response;
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
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_pull_align(ndr, 4));
			NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->Size));
			NDR_CHECK(ndr_pull_AUX_VERSION(ndr, NDR_SCALARS, &r->Version));
			NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &r->Type));
			switch (r->Version) {
			case AUX_VERSION_1:
				NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->Payload_1, r->Type));
				NDR_CHECK(ndr_pull_AUX_HEADER_TYPE_UNION_1(ndr, NDR_SCALARS, &r->Payload_1));
				break;
			case AUX_VERSION_2:
				NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->Payload_2, r->Type));
				NDR_CHECK(ndr_pull_AUX_HEADER_TYPE_UNION_2(ndr, NDR_SCALARS, &r->Payload_2));
				break;
			}
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
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
						
					} else if (r->RPC_HEADER_EXT.Flags & RHEF_XorMagic) {
						obfuscate_data(_ndr_buffer->data, _ndr_buffer->data_size, 0xA5);

						for (cntr_AUX_HEADER_0 = 0; _ndr_buffer->offset < _ndr_buffer->data_size; cntr_AUX_HEADER_0++) {
							NDR_CHECK(ndr_pull_AUX_HEADER(_ndr_buffer, NDR_SCALARS, &r->AUX_HEADER[cntr_AUX_HEADER_0]));
							r->AUX_HEADER = talloc_realloc(_mem_save_AUX_HEADER_0, r->AUX_HEADER, struct AUX_HEADER, cntr_AUX_HEADER_0 + 2);
						}
						r->AUX_HEADER = talloc_realloc(_mem_save_AUX_HEADER_0, r->AUX_HEADER, struct AUX_HEADER, cntr_AUX_HEADER_0 + 2);
						r->AUX_HEADER[cntr_AUX_HEADER_0].Size = 0;
					}
				}
				NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_buffer, 0, -1));
			} 
			ndr->flags = _flags_save_DATA_BLOB;
		}
	}
	if (ndr_flags & NDR_BUFFERS) {
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
  (uint16_t) and next substract when pushing the content blob
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

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_REMAINING);
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
		r->mapi_repl = talloc_array(_mem_save_mapi_repl_0, struct EcDoRpc_MAPI_REPL, 2);
		NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_mapi_repl, 0, r->length - 2));
		for (cntr_mapi_repl_0 = 0; _ndr_mapi_repl->offset < _ndr_mapi_repl->data_size - 2; cntr_mapi_repl_0++) {
			NDR_CHECK(ndr_pull_EcDoRpc_MAPI_REPL(_ndr_mapi_repl, NDR_SCALARS, &r->mapi_repl[cntr_mapi_repl_0]));
			r->mapi_repl = talloc_realloc(_ndr_mapi_repl, r->mapi_repl, struct EcDoRpc_MAPI_REPL, cntr_mapi_repl_0 + 2);
		}
		r->mapi_repl[cntr_mapi_repl_0].opnum = 0;
		NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_mapi_repl, 4, -1));
		talloc_free(_ndr_mapi_repl);
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
					NDR_CHECK(ndr_pull_EcDoRpc_MAPI_REPL_UNION(ndr, NDR_SCALARS, &r->u));
				} else {
					switch (r->opnum) {
					case op_MAPI_Logon: {
						if (r->error_code == ecWrongServer) {
							NDR_CHECK(ndr_pull_Logon_redirect(ndr, NDR_SCALARS, &(r->us.mapi_Logon)));
						}
						break;}
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


_PUBLIC_ void ndr_print_SBinary_short(struct ndr_print *ndr, const char *name, const struct SBinary_short *r)
{
	ndr->print(ndr, "%-25s: SBinary_short cb=%u", name, (unsigned)r->cb);
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		ndr->depth++;
		dump_data(0, r->lpb, r->cb);
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
	return ndr_push_mapi_SRestriction(ndr, NDR_SCALARS, (const struct mapi_SRestriction *)r);
}


enum ndr_err_code ndr_pull_mapi_SRestriction_wrap(struct ndr_pull *ndr, int ndr_flags, struct mapi_SRestriction_wrap *r)
{
	return ndr_pull_mapi_SRestriction(ndr, NDR_SCALARS|NDR_BUFFERS, (struct mapi_SRestriction *)r);
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
