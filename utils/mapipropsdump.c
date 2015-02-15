/*
   Dump MAPI properties from MAPI Rops

   OpenChange Project

   Copyright (C) Julien Kerihuel 2014

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
#include "gen_ndr/ndr_exchange.h"
#include <popt.h>
#include <dlfcn.h>

#define	PLUGIN		"libmapi.so"
#define	FUNCTION	0xB
#define	FUNCTION_STR	"EcDoRpcExt2"

/*
  This code is a 3 hours hack.

  It allows developers to dump the packed content of the following
  MAPI ROPs response buffer: GetProps, QueryRows, FindRow and turn it
  into readable content.

  For all of these functions, the information needed to unpack the
  response buffer is available in the preceeding request (or even
  earlier in some cases). This tool takes a request and a response
  packet and try to map packed data (unpack it) using the following
  logic:

  GetProps:
  =========
	1. If the request has a GetProps call, we save the array of
	property tags for this call.

	2. We know that the response ROPs will be at the same index
	than the request one, therefore jump directly to this offset

	3. We finally map the property tags to the packed content into
	a SPropValue structure and dump its content on stdout

  QueryRows:
  ==========
	1. If there is a SetColumns call in the request, we save the
	array of property tags for this call.

	2. We iterate over response opnums and search for QueryRows

	3. We save the RowCount (number of rows to dump), we
	initialize the offset to 0 (position in the packed buffer) and
	we iterate over the number of rows.

	4. For each row, we map the property tags saved from the
	SetColumns request call to the buffer at current offset.

  FindRow: Same behavior than QueryRows, except that we only fetch 1 row.
  =======

  Note: ModifyRecipients and ExpandRow are not implemented

  Usage: ./mapipropsdump --request=<file_in> --response=<file_out>
 */


static const const struct ndr_interface_table *load_exchange_emsmdb_dso(const char *plugin)
{
	const struct ndr_interface_table	*p;
	void					*handle;

	handle = dlopen(plugin, RTLD_NOW);
	if (handle == NULL) {
		printf("%s: Unable to open: %s\n", plugin, dlerror());
		return NULL;
	}

	p = (const struct ndr_interface_table *) dlsym(handle, "ndr_table_exchange_emsmdb");
	if (!p) {
		OC_DEBUG(0, "%s: Unable to find DCE/RPC interface table for 'ndr_table_exchange_emsmdb': %s", plugin, dlerror());
		dlclose(handle);
		return NULL;
	}

	return p;
}

static struct SPropTagArray *process_request(TALLOC_CTX *mem_ctx,
					     const struct ndr_interface_call *f,
					     const char *filename,
					     uint8_t *opnum,
					     int *index)
{
	void			*st;
	uint8_t			*data;
	size_t			size = 0;
	DATA_BLOB		blob;
	enum ndr_err_code	ndr_err;
	struct ndr_pull		*ndr_pull;
	struct ndr_pull		*sndr_pull;
	struct EcDoRpcExt2	*r;
	struct mapi2k7_request	mapi2k7_request;
	struct mapi_request	*mapi_request;
	struct SPropTagArray	*SPropTagArray = NULL;
	DATA_BLOB		rgbIn;
	uint32_t		i, j;

	st = talloc_zero_size(mem_ctx, f->struct_size);
	if (!st) {
		OC_DEBUG(0, "Unable to allocate %d bytes", (int)f->struct_size);
		return NULL;
	}

	data = (uint8_t *) file_load(filename, &size, 0, mem_ctx);
	if (!data) {
		perror(filename);
		return NULL;
	}

	blob.data = data;
	blob.length = size;

	ndr_pull = ndr_pull_init_blob(&blob, mem_ctx);
	if (ndr_pull == NULL) {
		perror("ndr_pull_init_blob");
		return NULL;
	}
	ndr_pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = f->ndr_pull(ndr_pull, NDR_IN, st);
	if (ndr_err) {
		OC_DEBUG(0, "Unable to pull request, ndr_err = 0x%x", ndr_err);
		talloc_free(ndr_pull);
		return NULL;
	}

	r = (struct EcDoRpcExt2 *) st;
	rgbIn.data = r->in.rgbIn;
	rgbIn.length = r->in.cbIn;

	sndr_pull = ndr_pull_init_blob(&rgbIn, mem_ctx);
	if (sndr_pull == NULL) {
		perror("ndr_pull_init_blob");
		talloc_free(ndr_pull);
		return NULL;
	}
	ndr_set_flags(&sndr_pull->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REF_ALLOC);
	ndr_err = ndr_pull_mapi2k7_request(sndr_pull, NDR_SCALARS|NDR_BUFFERS, &mapi2k7_request);
	talloc_free(ndr_pull);
	if (ndr_err) {
		OC_DEBUG(0, "Unable to pull mapi2k7_request, ndr_err = 0x%x", ndr_err);
		talloc_free(sndr_pull);
		return NULL;
	}

	mapi_request = mapi2k7_request.mapi_request;
	for (i = 0; mapi_request->mapi_req[i].opnum; i++) {
		switch (mapi_request->mapi_req[i].opnum) {
		case op_MAPI_GetProps:
			*opnum = op_MAPI_GetProps;
			*index = i;
			SPropTagArray = talloc(mem_ctx, struct SPropTagArray);
			SPropTagArray->aulPropTag = talloc_zero(SPropTagArray, enum MAPITAGS);
			SPropTagArray->cValues = 0;
			for (j = 0; j < mapi_request->mapi_req[i].u.mapi_GetProps.prop_count; j++) {
				SPropTagArray_add(SPropTagArray, SPropTagArray,
						  mapi_request->mapi_req[i].u.mapi_GetProps.properties[j]);
			}
			break;
		case op_MAPI_ModifyRecipients:
			break;
		case op_MAPI_SetColumns:
			*opnum = op_MAPI_SetColumns;
			*index = i;
			SPropTagArray = talloc(mem_ctx, struct SPropTagArray);
			SPropTagArray->aulPropTag = talloc_zero(SPropTagArray, enum MAPITAGS);
			SPropTagArray->cValues = 0;
			for (j = 0; j < mapi_request->mapi_req[i].u.mapi_SetColumns.prop_count; j++) {
				SPropTagArray_add(SPropTagArray, SPropTagArray,
						  mapi_request->mapi_req[i].u.mapi_SetColumns.properties[j]);
			}
			break;
		}
	}
	talloc_free(sndr_pull);
	return SPropTagArray;
}

static int process_response(TALLOC_CTX *mem_ctx,
			    const struct ndr_interface_call *f,
			    const char *filename,
			    struct SPropTagArray *SPropTagArray,
			    uint8_t opnum, int index)
{
	struct SPropValue	*lpProps = NULL;
	void			*st;
	uint8_t			*data;
	size_t			size = 0;
	DATA_BLOB		blob;
	enum ndr_err_code	ndr_err;
	struct ndr_pull		*ndr_pull;
	struct ndr_pull		*sndr_pull;
	struct EcDoRpcExt2	*r;
	struct mapi2k7_response	mapi2k7_response;
	struct mapi_response	*mapi_response;
	DATA_BLOB		rgbOut;
	int			index_out = -1;
	int			i, j;

	st = talloc_zero_size(mem_ctx, f->struct_size);
	if (!st) {
		OC_DEBUG(0, "Unable to allocate %d bytes", (int)f->struct_size);
		return -1;
	}

	data = (uint8_t *) file_load(filename, &size, 0, mem_ctx);
	if (!data) {
		perror(filename);
		return -1;
	}

	blob.data = data;
	blob.length = size;

	ndr_pull = ndr_pull_init_blob(&blob, mem_ctx);
	if (ndr_pull == NULL) {
		perror("ndr_pull_init_blob");
		return -1;
	}
	ndr_pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = f->ndr_pull(ndr_pull, NDR_OUT, st);
	if (ndr_err) {
		OC_DEBUG(0, "Unable to pull response, ndr_err = 0x%x", ndr_err);
		return -1;
	}

	r = (struct EcDoRpcExt2 *) st;
	rgbOut.data = r->out.rgbOut;
	rgbOut.length = *r->out.pcbOut;

	sndr_pull = ndr_pull_init_blob(&rgbOut, mem_ctx);
	if (sndr_pull == NULL) {
		perror("ndr_pull_init_blob");
		return -1;
	}
	ndr_set_flags(&sndr_pull->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REF_ALLOC);
	ndr_err = ndr_pull_mapi2k7_response(sndr_pull, NDR_SCALARS|NDR_BUFFERS, &mapi2k7_response);
	talloc_free(ndr_pull);
	if (ndr_err) {
		OC_DEBUG(0, "Unable to pull response, ndr_err = 0x%x", ndr_err);
		talloc_free(sndr_pull);
		return -1;
	}

	mapi_response = mapi2k7_response.mapi_response;

	switch (opnum) {
	case op_MAPI_GetProps:
	case op_MAPI_ModifyRecipients:
		if (mapi_response->mapi_repl[index].opnum != opnum) {
			OC_DEBUG(0, "Mismatching request vs reply");
			talloc_free(sndr_pull);
			return -1;
		}
		break;
	case op_MAPI_SetColumns:
		for (i = 0; mapi_response->mapi_repl[i].opnum; i++) {
			if ((mapi_response->mapi_repl[i].opnum == op_MAPI_QueryRows) ||
			    (mapi_response->mapi_repl[i].opnum == op_MAPI_FindRow) ||
			    (mapi_response->mapi_repl[i].opnum == op_MAPI_ExpandRow)) {
				opnum = mapi_response->mapi_repl[i].opnum;
				index_out = i;
				break;
			}
		}
		if (index_out == -1) {
			OC_DEBUG(0, "Unable to find ROP QueryRows, FindRow or ExpandRow in the reply");
			talloc_free(sndr_pull);
			return -1;
		}
		break;
	}

	switch (opnum) {
	case op_MAPI_GetProps:
	{
		struct GetProps_repl	*g;
		uint32_t		cn_propvals;

		g = &(mapi_response->mapi_repl[index].u.mapi_GetProps);
		emsmdb_get_SPropValue(mem_ctx, &g->prop_data, SPropTagArray,
				      &lpProps, &cn_propvals, g->layout);
		for (j = 0; j < SPropTagArray->cValues; j++) {
			mapidump_SPropValue(lpProps[j], "\t");
		}
	}
	break;
	case op_MAPI_ModifyRecipients:
	case op_MAPI_ExpandRow:
		OC_DEBUG(0, "Not implemented!");
		return -1;
		break;
	case op_MAPI_FindRow:
	{
		uint32_t		offset = 0;
		uint32_t		cn_propvals;
		struct FindRow_repl	*repl;

		repl = &(mapi_response->mapi_repl[index_out].u.mapi_FindRow);
		if (!emsmdb_get_SPropValue_offset(mem_ctx, &repl->row,
						  SPropTagArray, &lpProps,
						  &cn_propvals, &offset)) {
			for (j = 0; j < SPropTagArray->cValues; j++) {
				mapidump_SPropValue(lpProps[j], "\t");
			}
			talloc_free(lpProps);
		}
	}
	break;
	case op_MAPI_QueryRows:
	{
		uint32_t		offset = 0;
		uint32_t		cn_propvals;
		struct QueryRows_repl	*repl;

		repl = &(mapi_response->mapi_repl[index_out].u.mapi_QueryRows);
		for (i = 0; i < repl->RowCount; i++) {
			if (!emsmdb_get_SPropValue_offset(mem_ctx, &repl->RowData, SPropTagArray,
							  &lpProps, &cn_propvals, &offset)) {
				OC_DEBUG(0, "\nrow %d:", i);
				for (j = 0; j < SPropTagArray->cValues; j++) {
					mapidump_SPropValue(lpProps[j], "\t");
				}
				talloc_free(lpProps);
			}
		}
	}
	break;
	}

	talloc_free(sndr_pull);
	return 0;
}

int main(int argc, const char *argv[])
{
	TALLOC_CTX				*mem_ctx = NULL;
	const struct ndr_interface_table	*p = NULL;
	const struct ndr_interface_call		*f;
	struct SPropTagArray			*s;
	const char				*opt_reqfile = NULL;
	const char				*opt_replfile = NULL;
	poptContext				pc;
	int					opt;
	int					index = -1;
	int					ret = -1;
	uint8_t					opnum = 0;

	enum {OPT_REQ=1000, OPT_REPL};
	struct poptOption	long_options[] = {
		POPT_AUTOHELP
		{ "request",  'i', POPT_ARG_STRING, NULL, OPT_REQ,  "MAPI Request", NULL },
		{ "response", 'o', POPT_ARG_STRING, NULL, OPT_REPL, "MAPI Response", NULL },
		{ NULL }
	};

	pc = poptGetContext("mapipropsdump", argc, argv, long_options, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_REQ:
			opt_reqfile = poptGetOptArg(pc);
			break;
		case OPT_REPL:
			opt_replfile = poptGetOptArg(pc);
			break;
		}
	}

	/* Sanity checks on options */
	if (!opt_reqfile || !opt_replfile) {
		OC_DEBUG(0, "Request and Response files required");
		exit (1);
	}

	p = load_exchange_emsmdb_dso(PLUGIN);
	if (!p) {
		exit (1);
	}

	f = &p->calls[FUNCTION];
	mem_ctx = talloc_init("mapipropsdump");

	/* Load the request */
	s = process_request(mem_ctx, f, opt_reqfile, &opnum, &index);
	if (!s) {
		talloc_free(mem_ctx);
		exit (1);
	}

	/* Load the response */
	ret = process_response(mem_ctx, f, opt_replfile, s, opnum, index);
	if (ret == -1) {
		talloc_free(mem_ctx);
		exit (1);
	}

	talloc_free(mem_ctx);
	return (0);
}
