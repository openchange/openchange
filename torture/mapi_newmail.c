/*
   OpenChange MAPI implementation testsuite

   New mail notifications

   Copyright (C) Julien Kerihuel 2007
   Copyright (C) Fabien Le Mentec 2007
   
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


#include <libmapi/libmapi.h>
#include <gen_ndr/ndr_exchange.h>
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture/torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>


static int callback(uint32_t ulEventType, void *notif_data, void *private_data)
{
	struct NEWMAIL_NOTIFICATION	*newmail;

	switch(ulEventType) {
	case fnevNewMail:
		printf("[+]New mail Received!!!!\n");
		newmail = (struct NEWMAIL_NOTIFICATION *)notif_data;
		mapidump_newmail(newmail, "\t");
		break;
	case fnevObjectCreated:
		printf("[+]Object Created!!!\n");
		break;
	}

	return 0;
}

bool torture_rpc_mapi_newmail(struct torture_context *torture)
{
	NTSTATUS		nt_status;
	enum MAPISTATUS		retval;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	bool			ret = True;
	struct mapi_session	*session;
	mapi_object_t		obj_store;
	mapi_object_t		obj_inbox;
	uint64_t		id_inbox;
	uint32_t		ulEventMask;
	uint32_t		ulConnection;

	/* init torture */
	mem_ctx = talloc_init("torture_rpc_mapi_newmail");
	nt_status = torture_rpc_connection(mem_ctx, &p, &ndr_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return False;
	}

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx)) == NULL) return False;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_inbox);

	/* session::OpenMsgStore() */
	retval = OpenMsgStore(&obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* Register notification */
	retval = RegisterNotification(fnevTableModified);
	mapi_errstr("RegisterNotification", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* Open Inbox */
	retval = GetDefaultFolder(&obj_store, &id_inbox, olFolderInbox);
	mapi_errstr("GetDefaultFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* newmail and created|modified object notifications in inbox */
	ulEventMask = fnevObjectCreated;
	retval = Subscribe(&obj_inbox, &ulConnection, ulEventMask, (mapi_notify_callback_t)callback);
	mapi_errstr("Subscribe", GetLastError());

	ulEventMask = fnevNewMail;
	retval = Subscribe(&obj_inbox, &ulConnection, ulEventMask, (mapi_notify_callback_t)callback);
	mapi_errstr("Subscribe", GetLastError());


	if (retval != MAPI_E_SUCCESS) return False;

 	/* wait for notifications */
	MonitorNotification((void *)&obj_store);

	/* uninitialize mapi */
	MAPIUninitialize();
	talloc_free(mem_ctx);
	
	return (ret);
}
