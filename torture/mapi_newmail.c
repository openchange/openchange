/*
   OpenChange MAPI implementation testsuite

   Fetch mail from an Exchange server

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
#include <torture/torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>


/* udp newmail related routines
 */

typedef struct {
	struct sockaddr_in addr;
	int fd;
	int (*cb_notif)(void*);
} newmail_ctx_t;

static int get_ifaddr(const char* if_name, unsigned long* buf)
{
	struct ifaddrs* addrs;
	struct ifaddrs* pos;
	struct ifaddrs* res;

	getifaddrs(&addrs);
	res = 0;

	for (pos = addrs; !res && pos; pos = pos->ifa_next) {
		if (pos->ifa_addr && (pos->ifa_addr->sa_family == AF_INET)) {
			if (!strcmp(pos->ifa_name, if_name)) res = pos;
		}
	}
	if (res) { 
		*buf = ((struct sockaddr_in*)res->ifa_addr)->sin_addr.s_addr;
	}

	freeifaddrs(addrs);

	return (res != 0) ? 0 : -1;
}

static void newmail_release(newmail_ctx_t* ctx)
{
	if (ctx->fd != -1) {
		shutdown(ctx->fd, SHUT_RDWR);
		close(ctx->fd);
		ctx->fd = -1;
	}
}

static int newmail_init(newmail_ctx_t* ctx,
			unsigned short port,
			int (*cb_notif)(void*))
{
	unsigned long buf;
	int err;

	err = 0;

	err = get_ifaddr("eth0", &buf);
	if (err == -1) {
		printf("[!] get_ifaddr(eth0) == failure\n");
		return -1;
	}
	memset(&ctx->addr, 0, sizeof(newmail_ctx_t));
	ctx->addr.sin_family = AF_INET;
	ctx->addr.sin_addr.s_addr = buf;
	ctx->addr.sin_port = htons(port);

	ctx->fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (ctx->fd == -1) {
		err = -1;
		goto newmail_error;
	}

	if (bind(ctx->fd, (const struct sockaddr*)&ctx->addr, sizeof(struct sockaddr_in)) == -1) {
		err = -1;
		goto newmail_error;
	}

	ctx->cb_notif = cb_notif;

	emsmdb_register_notification(&ctx->addr);

 newmail_error:
	if (err) newmail_release(ctx);

	return err;
}

static int newmail_loop(newmail_ctx_t* ctx, void* private)
{
	int is_done;
	int err;
	char buf[512];

	is_done = 0;
	while (!is_done) {
		err = read(ctx->fd, buf, sizeof(buf));
		if (err > 0) {
			if (ctx->cb_notif && (ctx->cb_notif(private) == -1))
				err = -1;
		}
		
		if (err <= 0) is_done = 1;
	}

	return 0;
}

static int newmail_handle_notif(void* unused)
{
	printf("[!] newmail\n");
	return 0;
}


BOOL torture_rpc_mapi_newmail(struct torture_context *torture)
{
	NTSTATUS		nt_status;
	enum MAPISTATUS		retval;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	BOOL			ret = True;
	const char		*profname;
	const char		*profdb;
	struct mapi_session	*session;
	mapi_object_t		obj_store;
	mapi_object_t		obj_inbox;
	mapi_object_t		obj_message;
	mapi_object_t		obj_table;
	uint64_t		id_inbox;
	newmail_ctx_t		newmail;

	/* init torture */
	mem_ctx = talloc_init("local");
	nt_status = torture_rpc_connection(mem_ctx, &p, &dcerpc_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return False;
	}

	/* init mapi */
	profdb = lp_parm_string(-1, "mapi", "profile_store");
	retval = MAPIInitialize(profdb);
	mapi_errstr("MAPIInitialize", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* profile name */
	profname = lp_parm_string(-1, "mapi", "profile");
	if (profname == 0) {
		DEBUG(0, ("Please specify a valid profile name\n"));
		return False;
	}

	retval = MapiLogonEx(&session, profname);
	mapi_errstr("MapiLogonEx", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_inbox);
	mapi_object_init(&obj_message);
	mapi_object_init(&obj_table);

	/* session::OpenMsgStore() */
	retval = OpenMsgStore(&obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* newmail */
	newmail_init(&newmail, 40000, newmail_handle_notif);

	/* id_inbox = store->GetReceiveFolder */
	retval = GetReceiveFolder(&obj_store, &id_inbox);
	mapi_errstr("GetReceiveFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* inbox = store->OpenFolder() */
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* table = inbox->GetContentsTable() */
	retval = GetContentsTable(&obj_inbox, &obj_table);
	mapi_errstr("GetContentsTable", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

 	/* wait for notifications */
	newmail_loop(&newmail, 0);

	/* release mapi objects */
	mapi_object_release(&obj_store);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_table);
	newmail_release(&newmail);

	/* uninitialize mapi */
	MAPIUninitialize();
	talloc_free(mem_ctx);
	
	return (ret);
}
