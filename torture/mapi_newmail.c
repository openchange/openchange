/*
   OpenChange MAPI torture suite implementation.

   New mail notifications

   Copyright (C) Julien Kerihuel 2007.
   Copyright (C) Fabien Le Mentec 2007.

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
#include <gen_ndr/ndr_exchange.h>
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>


static int callback(uint32_t NotificationType, void *NotificationData, void *private_data)
{
	struct NewMailNotification	*newmail;

	switch(NotificationType) {
	case fnevNewMail:
		printf("[+]New mail Received!!!!\n");
		newmail = (struct NewMailNotification *) NotificationData;
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
	bool			ret = true;
	struct mapi_session	*session;
	mapi_object_t		obj_store;
	mapi_object_t		obj_inbox;
	uint64_t		id_inbox;
	uint32_t		ulEventMask;
	uint32_t		ulConnection;

	/* init torture */
	mem_ctx = talloc_named(NULL, 0, "torture_rpc_mapi_newmail");
	nt_status = torture_rpc_connection(torture, &p, &ndr_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return false;
	}

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx, torture->lp_ctx)) == NULL) return false;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_inbox);

	/* session::OpenMsgStore() */
	retval = OpenMsgStore(session, &obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Register notification */
	retval = RegisterNotification(fnevTableModified);
	mapi_errstr("RegisterNotification", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Open Inbox */
	retval = GetDefaultFolder(&obj_store, &id_inbox, olFolderInbox);
	mapi_errstr("GetDefaultFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* newmail and created|modified object notifications in inbox */
	ulEventMask = fnevObjectCreated;
	retval = Subscribe(&obj_inbox, &ulConnection, ulEventMask, false, (mapi_notify_callback_t)callback, (void*) &obj_store);
	mapi_errstr("Subscribe", GetLastError());

	ulEventMask = fnevNewMail;
	retval = Subscribe(&obj_inbox, &ulConnection, ulEventMask, false, (mapi_notify_callback_t)callback, (void*) &obj_store);
	mapi_errstr("Subscribe", GetLastError());


	if (retval != MAPI_E_SUCCESS) return false;

 	/* wait for notifications */
	MonitorNotification(mapi_object_get_session(&obj_inbox),(void *)&obj_store, NULL);

	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);

	/* uninitialize mapi */
	MAPIUninitialize();
	talloc_free(mem_ctx);
	
	return (ret);
}
