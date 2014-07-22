/*
   Stand-alone MAPI testsuite

   OpenChange Project - Zentyal functional testing tests

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

#include "utils/mapitest/mapitest.h"
#include "utils/mapitest/proto.h"

/**
   \file module_zentyal.c

   \brief Zentyal tests
 */

/**
   \details Test #1876 NspiQueryRows

   \param mt pointer to the top level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_zentyal_1876(struct mapitest *mt)
{
	TALLOC_CTX			*mem_ctx;
	struct nspi_context		*nspi_ctx;
	struct PropertyRowSet_r		*RowSet;
	struct PropertyTagArray_r	*MIds;
	enum MAPISTATUS	retval;

	/* Sanity checks */
	mem_ctx = talloc_named(NULL, 0, "mapitest_zentyal_1876");
	if (!mem_ctx) return false;

	nspi_ctx = (struct nspi_context *) mt->session->nspi->ctx;
	if (!nspi_ctx) return false;

	MIds = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	if (!MIds) return false;

	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	if (!RowSet) return false;

	/* Update pStat with incorrect data */
	nspi_ctx->pStat->NumPos = 99;

	retval = nspi_QueryRows(nspi_ctx, mem_ctx, NULL, MIds, 1, &RowSet);
	MAPIFreeBuffer(RowSet);
	mapitest_print_retval_clean(mt, "1876", retval);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(MIds);
		talloc_free(mem_ctx);
		return false;
	}

	talloc_free(mem_ctx);
	return true;
}
