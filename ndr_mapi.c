/* 
   OpenChange implementation.

   libndr mapi support

   Copyright (C) Julien Kerihuel 2005-2007
   
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
			char *idx_0=NULL;
			asprintf(&idx_0, "[%d]", cntr_mapi_req_0);
			if (idx_0) {
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
			char *idx_0=NULL;
			asprintf(&idx_0, "[%d]", cntr_mapi_repl_0);
			if (idx_0) {
				ndr_print_EcDoRpc_MAPI_REPL(ndr, "mapi_repl", &r->mapi_repl[cntr_mapi_repl_0]);
				free(idx_0);
			} 
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

	obfuscate_data(ndr->data, ndr->offset, 0xA5);

	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_push_mapi_response(struct ndr_push *ndr, int ndr_flags, const struct mapi_response *_blob)
{
	uint32_t	remaining_length;
	struct mapi_response blob = *_blob;
	uint32_t i,count;

	if (!(ndr->flags & LIBNDR_FLAG_REMAINING)) {
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, blob.mapi_len));
	}
	blob.length += 2;
	NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, blob.length));

	blob.mapi_len -= 2;
	blob.length -= 2;

	remaining_length = blob.mapi_len - blob.length;

	count = remaining_length / sizeof(uint32_t);
	for (i=0; i < count; i++) {
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, blob.handles[i]));
	}
	obfuscate_data(ndr->data, ndr->alloc_size, 0xA5);

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

	obfuscate_data(ndr->data, ndr->data_size, 0xA5);

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
	}
	_mem_save_handles_0 = NDR_PULL_GET_MEM_CTX(ndr);
	count = (r->mapi_len - r->length) / sizeof(uint32_t);
	r->handles = talloc_array(_mem_save_handles_0, uint32_t, count + 1);
	for (cntr_mapi_req_0=0; cntr_mapi_req_0 < count; cntr_mapi_req_0++) {
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->handles[cntr_mapi_req_0]));
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

	obfuscate_data(ndr->data, ndr->data_size, 0xA5);

	if (ndr->flags & LIBNDR_FLAG_REMAINING) {
		length = ndr->data_size - ndr->offset;
	} else {
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &length));
	}
	r->mapi_len = length;

	NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->length));
	
	/* If length equals length field then skipping subcontext */
	if (r->length > sizeof (uint16_t)) {
	  NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_mapi_repl, 0, r->length - 2));
	  _mem_save_mapi_repl_0 = NDR_PULL_GET_MEM_CTX(_ndr_mapi_repl);
	  r->mapi_repl = talloc_zero(_mem_save_mapi_repl_0, struct EcDoRpc_MAPI_REPL);
	  for (cntr_mapi_repl_0 = 0; _ndr_mapi_repl->offset < _ndr_mapi_repl->data_size - 2; cntr_mapi_repl_0++) {
	    NDR_CHECK(ndr_pull_EcDoRpc_MAPI_REPL(_ndr_mapi_repl, NDR_SCALARS, &r->mapi_repl[cntr_mapi_repl_0]));
	    r->mapi_repl = talloc_realloc(_mem_save_mapi_repl_0, r->mapi_repl, struct EcDoRpc_MAPI_REPL, cntr_mapi_repl_0 + 2);
	  }
	  r->mapi_repl = talloc_realloc(_mem_save_mapi_repl_0, r->mapi_repl, struct EcDoRpc_MAPI_REPL, cntr_mapi_repl_0 + 2);
	  r->mapi_repl[cntr_mapi_repl_0].opnum = 0;
	  NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_mapi_repl, 4, -1));
	}
	_mem_save_handles_0 = NDR_PULL_GET_MEM_CTX(ndr);
	count = (r->mapi_len - r->length) / sizeof(uint32_t);
	r->handles = talloc_array(_mem_save_handles_0, uint32_t, count + 1);
	for (cntr_mapi_repl_0=0; cntr_mapi_repl_0 < count; cntr_mapi_repl_0++) {
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->handles[cntr_mapi_repl_0]));
	}

	return NDR_ERR_SUCCESS;
}

/*
  We stop processing the IDL if MAPISTATUS is different from MAPI_E_SUCCESS
 */

enum ndr_err_code ndr_pull_EcDoRpc_MAPI_REPL(struct ndr_pull *ndr, int ndr_flags, struct EcDoRpc_MAPI_REPL *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_pull_align(ndr, 8));
			NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &r->opnum));
			if (r->opnum == op_MAPI_Notify) {
				NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->u, r->opnum));
				NDR_CHECK(ndr_pull_EcDoRpc_MAPI_REPL_UNION(ndr, NDR_SCALARS, &r->u));
			} else {
				NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &r->handle_idx));
				NDR_CHECK(ndr_pull_MAPISTATUS(ndr, NDR_SCALARS, &r->error_code));
				if ( r->error_code == MAPI_E_SUCCESS) {
					NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->u, r->opnum));
					NDR_CHECK(ndr_pull_EcDoRpc_MAPI_REPL_UNION(ndr, NDR_SCALARS, &r->u));
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
		if (r->opnum != op_MAPI_Notify) {
			ndr_print_uint8(ndr, "handle_idx", r->handle_idx);
			ndr_print_MAPISTATUS(ndr, "error_code", r->error_code);
			if (r->error_code == MAPI_E_SUCCESS) {
				ndr_print_set_switch_value(ndr, &r->u, r->opnum);
				ndr_print_EcDoRpc_MAPI_REPL_UNION(ndr, "u", &r->u);
			}
		} else {
			ndr_print_set_switch_value(ndr, &r->u, r->opnum);
			ndr_print_EcDoRpc_MAPI_REPL_UNION(ndr, "u", &r->u);
		}
		ndr->depth--;
		ndr->flags = _flags_save_STRUCT;
	}
}

/*
  We need to pull QueryRows replies on our own:
  If we have no results, do not pull/print the DATA_BLOB
*/

enum ndr_err_code ndr_pull_QueryRows_repl(struct ndr_pull *ndr, int ndr_flags, struct QueryRows_repl *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_pull_align(ndr, 4));
			NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &r->unknown));
			NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->results_count));

			if (r->results_count)
			{
				uint32_t _flags_save_DATA_BLOB = ndr->flags;

				NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &r->layout));
				ndr_set_flags(&ndr->flags, LIBNDR_FLAG_REMAINING);
				NDR_CHECK(ndr_pull_DATA_BLOB(ndr, NDR_SCALARS, &r->rows));
				ndr->flags = _flags_save_DATA_BLOB;
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
 * We need to have user defined push,pull,print function for
 * SNotRestriction so we can workaround the no-pointer deep recursion
 * pb in pidl.
 */

enum ndr_err_code ndr_push_mapi_SNotRestriction(struct ndr_push *ndr, int ndr_flags, const struct mapi_SNotRestriction *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr->flags & NDR_SCALARS) {
			NDR_CHECK(ndr_push_align(ndr, 4));
			NDR_CHECK(ndr_push_mapi_SRestriction_not(ndr, NDR_SCALARS, &r->res[0]));
		}
		if (ndr_flags & NDR_BUFFERS) {
			NDR_CHECK(ndr_push_mapi_SRestriction_not(ndr, NDR_BUFFERS, &r->res[0]));
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code  ndr_pull_mapi_SNotRestriction(struct ndr_pull *ndr, int ndr_flags, struct mapi_SNotRestriction *r)
{
	TALLOC_CTX *_mem_save_res_0;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_pull_align(ndr, 4));
			NDR_PULL_ALLOC_N(ndr, r->res, 1);
			_mem_save_res_0 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, r->res, 0);
			NDR_CHECK(ndr_pull_mapi_SRestriction_not(ndr, NDR_SCALARS, &r->res[0]));
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_res_0, 0);
		}
		if (ndr_flags & NDR_BUFFERS) {
			_mem_save_res_0 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, r->res, 0);
			NDR_CHECK(ndr_pull_mapi_SRestriction_not(ndr, NDR_BUFFERS, &r->res[0]));
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_res_0, 0);
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}


_PUBLIC_ void ndr_print_mapi_SNotRestriction(struct ndr_print *ndr, const char *name, const struct mapi_SNotRestriction *r)
{
	ndr_print_struct(ndr, name, "mapi_SNotRestriction");
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		ndr->depth++;
		ndr_print_mapi_SRestriction_not(ndr, "res", &r->res[0]);
		ndr->depth--;
		ndr->flags = _flags_save_STRUCT;
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
