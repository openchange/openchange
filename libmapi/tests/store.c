/*
 *  OpenChange MAPI implementation.
 *  Access the OpenChange Store database
 *
 *  Copyright (C) Julien Kerihuel 2005.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "openchange.h"
#include "ndr_exchange.h"
#include "libmapi/IMAPISession.h"
#include "libmapi/include/mapi_proto.h"


#define	OP_HELP			0x0
#define	OP_GETHIERARCHYINFO	0x1

/* command line options */
static struct {
	uint32_t	operation;
} options;

static void op_help(void)
{
	printf("%-15s %s\n", "0x0", "This Help message");
	printf("%-15s %s\n", "0x1", "GetHierarchyInfo");
}

static void op_getHierarchyInfo(void)
{
	TALLOC_CTX		*mem_ctx;
	struct SRowSet		*RowSet;
	struct ndr_print	*ndr;

	mem_ctx = talloc_init("GetHierarchyTableDump");
	lp_set_cmdline("exchange:store", "/usr/local/samba/private/store.ldb");
	lp_set_cmdline("exchange:dbtype", "tdb");

	GetHierarchyTable(0, &RowSet);

	ndr = talloc_zero(mem_ctx, struct ndr_print);
	ndr->depth = 1;
	ndr->print = ndr_print_debug_helper;
	ndr_print_SRowSet(ndr, "GetHierarchyInfo", RowSet);
	talloc_free(mem_ctx);
}

static void process_one(const char *name)
{
	if (options.operation) {
		switch(options.operation) {
		case OP_HELP:
			(void)op_help();
			break;
		case OP_GETHIERARCHYINFO:
			op_getHierarchyInfo();
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	poptContext pc;
	int opt;
	enum{OPT_OPERATION=1};
	struct poptOption popt_options[] = {
		POPT_AUTOHELP
		{"operation",	'o', POPT_ARG_INT, &options.operation, OPT_OPERATION, "Execute a single operation on the store database", "OPERATION"},
		POPT_TABLEEND
	};
	
	pc = poptGetContext(argv[0], argc, (const char **)argv, popt_options,
			    POPT_CONTEXT_KEEP_FIRST);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_OPERATION:
			process_one(poptGetOptArg(pc));
			break;
		}
	}
	
	return (0);
}
