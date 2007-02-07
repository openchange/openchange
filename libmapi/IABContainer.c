/*
 *  Copyright (C) Julien Kerihuel 2007.
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
#include "exchange.h"
#include "ndr_exchange.h"
#include "libmapi/include/mapidefs.h"
#include "libmapi/include/nspi.h"
#include "libmapi/include/emsmdb.h"
#include "libmapi/mapicode.h"
#include "libmapi/include/proto.h"
#include "libmapi/include/mapi_proto.h"


/**
 * Wrapper on nspi_ResolveNames
 * Fill an adrlist and flaglist structure
 *
 */

struct SRowSet *ResolveNames(struct emsmdb_context *emsmdb, const char **usernames, struct SPropTagArray *props)			
{
	struct SRowSet		SRowSet;
	struct SRowSet		*SRowSet2;

	nspi_ResolveNames(emsmdb->nspi, usernames, props, &SRowSet);

	SRowSet2 = talloc_zero(emsmdb->mem_ctx, struct SRowSet);
	*SRowSet2 = SRowSet;
	return SRowSet2;
}
