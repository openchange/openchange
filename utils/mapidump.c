/*
   MAPI EcDoRpc functions decoding tool

   OpenChange Project

   Copyright (C) Julien Kerihuel 2007

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
#include "ndr_exchange.h"
#include "libmapi/include/mapidefs.h"

struct mapi_calls
{
	uint8_t		opnum;
	uint8_t		opnum_request;	/* in which request/response function is the property array located */
	const char	*name;
	uint8_t		inout;		/* where the property tags array should be found (request or response)*/
};

struct mapi_calls MAPI_CALLS[] = {
	{ op_MAPI_OpenMessage,	op_MAPI_OpenMessage,	"OpenMessage",	NDR_OUT	},
	{ op_MAPI_GetProps,	op_MAPI_GetProps,	"GetProps",	NDR_IN	},
	{ op_MAPI_QueryRows,	op_MAPI_SetColumns,	"QueryRows",	NDR_IN	},
	{ 0xFF, 0, NULL, 0 }
};

static void show_functions(void)
{
	uint8_t	i;

	printf("\nYou must specify a function\n");
	printf("known MAPI functions with blob of data are:\n");
	for (i = 0; MAPI_CALLS[i].name; i++) {
		printf("\t(0x%.2x) %-15s\n", MAPI_CALLS[i].opnum, MAPI_CALLS[i].name);
	}
}

static struct EcDoRpc *pull_EcDoRpc(TALLOC_CTX *mem_ctx, const char *filename, uint8_t inout)
{
	void		*st;
	uint8_t		*data;
	size_t		size;
	DATA_BLOB	blob;
	struct ndr_pull	*ndr_pull;
	NTSTATUS	status;

	st  = talloc_zero_size(mem_ctx, sizeof (struct EcDoRpc));
	if (!st) {
		printf("Unable to allocate %d bytes\n", sizeof (struct EcDoRpc));
		exit(1);
	}

	data  = (uint8_t *)file_load(filename, &size, mem_ctx);

	if (!data) {
		perror(filename);
		exit (1);
	}

	blob.data = data;
	blob.length = size;

	ndr_pull = ndr_pull_init_blob(&blob, mem_ctx);
	ndr_pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	status = ndr_pull_EcDoRpc(ndr_pull, inout, st);

	printf("%-15s: pull returned %s\n", ((inout & NDR_IN)?"request":"response"), nt_errstr(status));

	if (ndr_pull->offset != ndr_pull->data_size) {
		printf("WARNING! %d unread bytes\n", ndr_pull->data_size - ndr_pull->offset);
		dump_data(0, ndr_pull->data+ndr_pull->offset, ndr_pull->data_size - ndr_pull->offset);
	}

	return (st);
}

/*
  fetch the property tag array from the request or response and
  returns the number of elements
 */

static uint32_t get_mapidump_properties(void *_r, uint8_t opnum, uint32_t **properties, uint8_t inout)
{
	int i;

	if (inout & NDR_IN) {
		struct mapi_request *r = (struct mapi_request *) _r;
		
		for (i = 0; r->mapi_req[i].opnum; i++) {
			if (r->mapi_req[i].opnum == opnum) {
				switch (opnum) {
				case op_MAPI_SetColumns:
					*properties = (uint32_t *)r->mapi_req[i].u.mapi_SetColumns.properties;
					return r->mapi_req[i].u.mapi_SetColumns.prop_count;
				case op_MAPI_GetProps:
					*properties = (uint32_t *)r->mapi_req[i].u.mapi_GetProps.properties;
					return r->mapi_req[i].u.mapi_GetProps.prop_count;
				default:
					return -1;
				}
			}
		}
	}

	if (inout & NDR_OUT) {
		struct mapi_response *r = (struct mapi_response *) _r;

		for (i = 0; r->mapi_repl[i].opnum; i++) {
			if (r->mapi_repl[i].opnum == opnum) {
				switch (opnum) {
				case op_MAPI_OpenMessage:
					*properties = (uint32_t *)r->mapi_repl[i].u.mapi_OpenMessage.properties;
					return r->mapi_repl[i].u.mapi_OpenMessage.prop_count;
				}
			}
		}
	}
	
	return (-1);
}

/*
  retrieve the data blob to decode from the response depending on the response opnum
*/

uint32_t get_mapidump_blob(struct mapi_response *r, uint8_t opnum, DATA_BLOB *blob)
{
	int i;

	for (i = 0; r->mapi_repl[i].opnum; i++) {
		if (r->mapi_repl[i].opnum == opnum) {
			switch (opnum) {
			case op_MAPI_QueryRows:
				*blob = r->mapi_repl[i].u.mapi_QueryRows.inbox;
				return ((uint32_t)r->mapi_repl[i].u.mapi_QueryRows.results_count);
			case op_MAPI_GetProps:
				*blob = r->mapi_repl[i].u.mapi_GetProps.prop_data;
				return 1;
			case op_MAPI_OpenMessage:
				*blob = r->mapi_repl[i].u.mapi_OpenMessage.message;
				return 1;
			default:
				return -1;
			}
		}
	}
	return -1;
}

/*
  pull the property value from DATA_BLOB according to the property type (PT_TYPE)
 */

static NTSTATUS dump_property(uint32_t property, struct ndr_print *ndr_print, struct ndr_pull *ndr_pull)
{
	const char		*pt_string8;
	uint64_t		pt_i8;
	uint32_t		pt_long;
	struct SBinary_short	pt_binary;

	ndr_print_MAPITAGS(ndr_print, "properties", property);
	switch (property & 0xFFFF) {
	case PT_LONG:
		NDR_CHECK(ndr_pull_uint32(ndr_pull, NDR_SCALARS, &pt_long));
		ndr_print_uint32(ndr_print, "", pt_long);
		break;
	case PT_I8:
		NDR_CHECK(ndr_pull_hyper(ndr_pull, NDR_SCALARS, &pt_i8));
		ndr_print_hyper(ndr_print, "", pt_i8);
		break;
	case PT_STRING8:
		ndr_set_flags(&ndr_pull->flags, LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_NULLTERM);
		NDR_CHECK(ndr_pull_string(ndr_pull, NDR_SCALARS, &pt_string8));
		ndr_print_string(ndr_print, "", pt_string8);
		break;
	case PT_BINARY:
		NDR_CHECK(ndr_pull_SBinary_short(ndr_pull, NDR_SCALARS, &pt_binary));
		ndr_print_SBinary_short(ndr_print, "", &pt_binary);
	}

	return NT_STATUS_OK;
}

/*
  get mapi opnum from name
 */

static uint8_t get_opnum_from_name(const char *name)
{
	uint8_t i;

	if (!name) {
		return 0xFF;
	}

	for (i = 0; MAPI_CALLS[i].name; i++) {
		if (!strcmp(MAPI_CALLS[i].name, name)) return MAPI_CALLS[i].opnum;
	}
	
	return 0xFF;
}

/*
  Depending on the response opnum specified on command line, we set
  the request opnum where the property array should be found.
*/

static uint8_t get_request_opnum(uint8_t opnum)
{
	uint8_t i;

	for (i = 0; MAPI_CALLS[i].name; i++) {
		if (MAPI_CALLS[i].opnum == opnum) return MAPI_CALLS[i].opnum_request;
	}
	return 0xFF;
}

/*
  Depending on the operation data blob we try to decode, we set
  the location of the properties array (request or within response)
*/

static uint8_t get_inout(uint8_t opnum)
{
	uint8_t i;

	for (i = 0; MAPI_CALLS[i].name; i++) {
		if (MAPI_CALLS[i].opnum == opnum) return MAPI_CALLS[i].inout;
	}
	return -1;
}

int main(int argc, char *argv[])
{
	struct ndr_print	*ndr_print;
	struct ndr_pull		*ndr_pull;
	const char		*function_name;
	poptContext		pc;
	TALLOC_CTX		*mem_ctx;
	const char		*filename_in, *filename_out;
	struct EcDoRpc		*st_in, *st_out;
	int			opt;
	int			i,j;
	uint32_t		*properties = NULL;
	uint8_t			opnum;
	uint32_t		prop_count, elem_count;
	DATA_BLOB		blob;
	uint32_t		offset;

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ NULL }
	};

	pc = poptGetContext("mapidump", argc, (const char **)argv, long_options, 0);

	poptSetOtherOptionHelp(
		pc, "<function> [<in filename>] [<out filename>]");

	while ((opt = poptGetNextOpt(pc)) != -1) {
	}
	
	function_name = poptGetArg(pc);

	if (!function_name) {
		poptPrintUsage(pc, stderr, 0);
		show_functions();
		exit (1);
	}

	opnum = (uint8_t) strtol(function_name, NULL, 16);

	if (!opnum) {
		if ((opnum = get_opnum_from_name(function_name)) == 0xFF) {
			printf("mapidump: Undefined 0x%x MAPI call\n", opnum);
			exit (1);
		}
	}

	filename_in = poptGetArg(pc);
	filename_out = poptGetArg(pc);

	if (!filename_in || !filename_out) {
		poptPrintUsage(pc, stderr, 0);
		show_functions();
		exit(1);
	}

	/* check the function number - name */

	mem_ctx = talloc_init("mapidump");

	st_in  = pull_EcDoRpc(mem_ctx, filename_in, NDR_IN);	
	st_out = pull_EcDoRpc(mem_ctx, filename_out, NDR_OUT);

	switch (get_inout(opnum)) {
	case NDR_IN:
		prop_count = get_mapidump_properties((void *)&st_in->in.mapi_request, 
						     get_request_opnum(opnum), &properties, NDR_IN);
		break;
	case NDR_OUT:
		prop_count = get_mapidump_properties((void *)&st_out->out.mapi_response, 
						     get_request_opnum(opnum), &properties, NDR_OUT);
		break;
	default:
		printf("mapidump: Undefined 0x%x MAPI call\n", opnum);
		exit (1);
	}

	if (prop_count == -1) {
		printf("mapidump: No properties found\n");
		exit (1);
	}

	elem_count = get_mapidump_blob((void *)&(st_out->out.mapi_response), opnum, &blob);

	if (elem_count == -1) {
		printf("mapidump: No data blob found in response\n");
		exit (1);
	}

	ndr_pull = ndr_pull_init_blob(&blob, mem_ctx);
	ndr_set_flags(&ndr_pull->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REF_ALLOC);

	ndr_print = talloc_zero(mem_ctx, struct ndr_print);
	ndr_print->print = ndr_print_debug_helper;
	ndr_print->depth = 1;

	for (i = 0, offset = 0; i < elem_count; i++) {
		for (j = 0; j < prop_count; j++){
			dump_property(properties[j], ndr_print, ndr_pull);
		}
		ndr_pull_advance(ndr_pull, 1);
	}

	return 0;
}
