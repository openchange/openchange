/*
   Asynchronous EMSMDB server

   OpenChange Project

   Copyright (C) Julien Kerihuel 2015

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

#ifndef	DCESRV_ASYNCEMSMDB_H
#define	DCESRV_ASYNCEMSMDB_H

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "gen_ndr/asyncemsmdb.h"
#include "gen_ndr/ndr_asyncemsmdb.h"

#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"

#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>

struct asyncemsmdb_private_data {
	struct dcesrv_call_state		*dce_call;
	struct EcDoAsyncWaitEx			*r;
	struct mapistore_context		*mstore_ctx;
	char					*emsmdb_session_str;
	struct tevent_fd			*fd_event;
	int					sock;
	int					fd;
	int					lock;
};

struct exchange_asyncemsmdb_session {
	struct asyncemsmdb_private_data		*data;
	struct GUID				uuid;
	struct exchange_asyncemsmdb_session	*prev;
	struct exchange_asyncemsmdb_session	*next;
};

#define	ASYNCEMSMDB_FALLBACK_ADDR	"127.0.0.1"

#endif /* !DCESRV_ASYNCEMSMDB_H */
