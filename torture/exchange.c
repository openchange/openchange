/* 
   Outlook to Exchange connection

   test suite for emsmdb rpc operations

   Copyright (C) Julien Kerihuel 2005
   
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

/*
   We simulate a complete connection to Exchange:
   1- MapiLogonEx(): only NspiBind - not mandatory here
                     use RPC-NSPI test instead
   2- OpenMessageStore:
      2.1 EcDoConnect: establish the connection on the emsmdb endpoint 
                       and send some values to Exchange such as the 
                       locale_id.
      2.2 EcDoRpc
      2.3 EcRRegisterPushNotification: Register client to Exchange
 */

#include "openchange.h"
#include <torture/torture.h>
#include "ndr_exchange.h"
#include "libmapi/include/emsmdb.h"
#include "libmapi/include/mapi_proto.h"

/* FIXME: Should be part of Samba's data: */
NTSTATUS torture_rpc_connection(TALLOC_CTX *parent_ctx, 
				struct dcerpc_pipe **p, 
				const struct dcerpc_interface_table *table);



#define ECDOCONNECT_FORMAT       "/o=%s Organization/ou=First Administrative Group/cn=Recipients/cn=%s"

struct GUID	uuid;
const char	*user;
const char	*org;
char	       	*session;


/*
  Open Message Store:
  EcDoRpc_1_data is sent when Opening the default message store (Mailbox)
 */

#define EcDoRpc_1_mapilen 108
#define EcDoRpc_1_length 102
#define	EcDoRpc_1_opnum 0x00FE
uint8_t EcDoRpc_1_content[] = "\x00\x01\x0C\x04\x00\x00\x00\x00\x00\x00\x58\x00\x2F\x6F\x3D\x4F\x70\x65\x6E\x43\x68\x61\x6E\x67\x65\x20\x4F\x72\x67\x61\x6E\x69\x7A\x61\x74\x69\x6F\x6E\x2F\x6F\x75\x3D\x46\x69\x72\x73\x74\x20\x41\x64\x6D\x69\x6E\x69\x73\x74\x72\x61\x74\x69\x76\x65\x20\x47\x72\x6F\x75\x70\x2F\x63\x6E\x3D\x52\x65\x63\x69\x70\x69\x65\x6E\x74\x73\x2F\x63\x6E\x3D\x41\x64\x6D\x69\x6E\x69\x73\x74\x72\x61\x74\x6F\x72\x00";
uint8_t EcDoRpc_1_remaining[] = "\xFF\xFF\xFF\xFF";

uint8_t EcRRegisterPushNotification_1_data[] = "\x28\x29\x63\x01\x00\x00\x00\x00";
#define EcRRegisterPushNotification_1_length 8;

/* 02 00 is an argument: changing its value cause a E_INVALIDARG (0x80070057) error */

uint8_t EcRRegisterPushNotification_2_data[] = "\x02\x00\x04\xDC\xC0\xA8\x01\x02\x00\x00\x00\x00\x00\x00\x00\x00";
#define EcRRegisterPushNotification_2_length 16;

/*
  Open Root Folder:
 */

uint8_t EcDoRpc_2_content[] = "\x00\x01\x02\x00\x01\x29\x00\x00\x02\x06\x00\x01\x01\x00\x01\x29\x00\x00\x03\x0E\x00\x01\x01\x00\x02\x29\x00\x00\x04\x1E\x00\x01\x01\x00\x03\x29\x00\x00\x05\x3E\x00\x01\x01\x00\x04\x29\x00\x00\x06\x7E\x00\x01\x01\x00\x05\x29\x00\x00\x07\x7F\x00\x01\x01\x00\x06\x02\x00\x00\x08\x01\x00\x00\x00\x00\x00\x28\x19\x00";

uint8_t EcDoRpc_3_content[] = "\x00\x01\x01\x00\x00\x01\x00\x00\x00\x00\x00\x28\x19\x00\x00\x00\x00\x00\x00\x00\x00\x29\x00\x00\x02\x05\x00\x00\x01\x00\x00\x00\x00\x00\x28\x19\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x01\x29\x00\x00\x03\x0D\x00\x00\x01\x00\x00\x00\x00\x00\x28\x19\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x02\x29\x00\x00\x04\x1D\x00\x00\x01\x00\x00\x00\x00\x00\x28\x19\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x03\x29\x00\x00\x05\x3D\x00\x00\x01\x00\x00\x00\x00\x00\x28\x19\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x04\x29\x00\x00\x06\x7D\x00\x00\x01\x00\x00\x00\x00\x00\x28\x19\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x05\x07\x00\x07\x00\x00\x00\x00\x01\x00\x03\x00\x01\x36";

uint8_t EcDoRpc_4_content[] = "\x00\x01\x00";

uint8_t EcDoRpc_5_content[] = "\x00\x00\x02\x00\x14\x00\x48\x67\x1E\x00\x01\x30\x15\x00\x00\x00\x01\x32\x00";

uint8_t EcDoRpc_6_content[] = "\x00\x01\x00";

uint8_t EcDoRpc_7_content[] = "\x00\x00\x08\x00\x14\x00\x48\x67\x14\x00\x4A\x67\x14\x00\x4D\x67\x03\x00\x4E\x67\x1E\x00\x37\x00\x1E\x00\x1A\x00\x1E\x00\xEB\x65\x1E\x00\xEC\x65\x05\x00\x01\x02\x02";

uint8_t EcDoRpc_8_content[] = "\x00\x00\x08\x00\x14\x00\x48\x67\x14\x00\x4A\x67\x14\x00\x4D\x67\x03\x00\x4E\x67\x1E\x00\x37\x00\x1E\x00\x1A\x00\x1E\x00\xEB\x65\x1E\x00\xEC\x65\x08\x00\x01\x00\x00\x00\x00";

struct MAPI_DATA RootFolder[] = {
	{ 118,	80,	0x29, 0x0, EcDoRpc_2_content, 0 },
	{ 200,	166,	0x29, 0x0, EcDoRpc_3_content, 0 },
	{ 15,	5,	0x04, 0x0, EcDoRpc_4_content, 0 },
	{ 27,	21,	0x12, 0x0, EcDoRpc_5_content, 0 },
	{ 15,	5,	0x05, 0x0, EcDoRpc_6_content, 0 },
	{ 57,	43,	0x12, 0x0, EcDoRpc_7_content, 0 },
	{ 55,	45,	0x12, 0x0, EcDoRpc_8_content, 0 },
	{0,	0,	0,    0,   0,		      0 }
};



/*
  OpenIPMSubTree()

 */

uint8_t EcDoRpc_9_content[] = "\x00\x01\x02\x00\x01\x29\x00\x00\x02\x06\x00\x01\x01\x00\x01\x29\x00\x00\x03\x0E\x00\x01\x01\x00\x02\x29\x00\x00\x04\x1E\x00\x01\x01\x00\x03\x29\x00\x00\x05\x3E\x00\x01\x01\x00\x04\x29\x00\x00\x06\x7E\x00\x01\x01\x00\x05\x29\x00\x00\x07\x7F\x00\x01\x01\x00\x06\x02\x00\x00\x08\x01\x00\x00\x00\x00\x00\x28\x1A\x00";

uint8_t EcDoRpc_10_content[] = "\x00\x01\x01\x00\x00\x01\x00\x00\x00\x00\x00\x28\x1A\x00\x00\x00\x00\x00\x00\x00\x00\x29\x00\x00\x02\x05\x00\x00\x01\x00\x00\x00\x00\x00\x28\x1A\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x01\x29\x00\x00\x03\x0D\x00\x00\x01\x00\x00\x00\x00\x00\x28\x1A\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x02\x29\x00\x00\x04\x1D\x00\x00\x01\x00\x00\x00\x00\x00\x28\x1A\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x03\x29\x00\x00\x05\x3D\x00\x00\x01\x00\x00\x00\x00\x00\x28\x1A\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x04\x29\x00\x00\x06\x7D\x00\x00\x01\x00\x00\x00\x00\x00\x28\x1A\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x05\x07\x00\x07\x00\x00\x00\x00\x01\x00\x03\x00\x01\x36";

uint8_t EcDoRpc_11_content[] = "\x00\x01\x00";

uint8_t EcDoRpc_12_content[] = "\x00\x00\x02\x00\x14\x00\x48\x67\x1E\x00\x01\x30\x15\x00\x00\x00\x01\x32\x00";

uint8_t EcDoRpc_13_content[] = "\x00\x01\x00";

uint8_t EcDoRpc_14_content[] = "\x00\x00\x08\x00\x14\x00\x48\x67\x14\x00\x4A\x67\x14\x00\x4D\x67\x03\x00\x4E\x67\x1E\x00\x37\x00\x1E\x00\x1A\x00\x1E\x00\xEB\x65\x1E\x00\xEC\x65\x05\x00\x01\x02\x02";

uint8_t EcDoRpc_15_content[] = "\x00\x00\x08\x00\x14\x00\x48\x67\x14\x00\x4A\x67\x14\x00\x4D\x67\x03\x00\x4E\x67\x1E\x00\x37\x00\x1E\x00\x1A\x00\x1E\x00\xEB\x65\x1E\x00\xEC\x65\x08\x00\x01\x00\x00\x00\x00";

struct MAPI_DATA	IPMSubTree[] = {
	{ 118,	80,	0x29, 0x0, EcDoRpc_9_content,	0 },
	{ 200,	166,	0x29, 0x0, EcDoRpc_10_content,	0 },
	{ 15,	5,	0x04, 0x0, EcDoRpc_11_content,	0 },
	{ 27,	21,	0x12, 0x0, EcDoRpc_12_content,	0 },
	{ 15,	5,	0x05, 0x0, EcDoRpc_13_content,	0 },
	{ 57,	43,	0x12, 0x0, EcDoRpc_14_content,	0 },
	{ 55,	45,	0x12, 0x0, EcDoRpc_15_content,	0 },
	{ 0,	0,	0,    0x0, 0,		0 }

};

/*
  OpenInbox()
 */

#define	OpenInbox_EcDoRpc_1_mapilen	22
#define	OpenInbox_EcDoRpc_1_length	18
#define	OpenInbox_EcDoRpc_1_opnum	0x0018
uint8_t OpenInbox_EcDoRpc_1_content[] = "\x00\x00\x00\x00\x00\x00\x00\x15\x00\x00\x00\x01\x01\x00";

static BOOL OpenMessageStore(struct emsmdb_context *emsmdb)
{
	struct MAPI_DATA		blob;
	DATA_BLOB		blob1;
	DATA_BLOB		blob2;
	NTSTATUS		status;
	struct MAPI_REQ		req;
	BOOL			ret = True;

	DEBUG(0, ("\nStep 2: OpenMessageStore\n"));

	req.opcode = op_MAPI_RPC_LOGON;
	req.u.mapi_rpc_logon.col = 0x0;
	req.u.mapi_rpc_logon.row = 0x1;
	req.u.mapi_rpc_logon.locale_id = 0x40c;
	req.u.mapi_rpc_logon.padding = 0x0;
	req.u.mapi_rpc_logon.profile_info = "/o=OpenChange Organization/ou=First Administrative Group/cn=Recipients/cn=Administrator";
	
	blob.mapi_len = EcDoRpc_1_mapilen;

	status = emsmdb_transaction(emsmdb, blob, &req);
	if (NT_STATUS_IS_ERR(status)) {
		return False;
	}

	blob1.data = EcRRegisterPushNotification_1_data;
	blob1.length = EcRRegisterPushNotification_1_length;

	blob2.data = EcRRegisterPushNotification_2_data;
	blob2.length = EcRRegisterPushNotification_2_length;

	ret &= emsmdb_registernotify(emsmdb, blob1, blob2);

	return True;
}

static BOOL OpenRootFolder(struct emsmdb_context *emsmdb)
{
	BOOL		ret = True;
	struct MAPI_DATA	blob;
	int		c;

	DEBUG(0, ("\nStep 3 - OpenRootFolder\n"));

	for (c = 0; RootFolder[c].mapi_len; c++) {
		blob.mapi_len = RootFolder[c].mapi_len;
		blob.length = RootFolder[c].length;
		blob.opnum = RootFolder[c].opnum;
		blob.content = RootFolder[c].content;
		
		ret &= NT_STATUS_IS_OK(emsmdb_transaction_unknown(emsmdb, blob));
	};

	return ret;
}

static BOOL OpenIPMSubTree(struct emsmdb_context *emsmdb)
{
        BOOL            ret = True;
        struct MAPI_DATA       blob;
	int		c;

	DEBUG(0, ("\nStep 4 - OpenIPMSubtree\n"));

	for (c = 0; IPMSubTree[c].mapi_len; c++) {
		blob.mapi_len = IPMSubTree[c].mapi_len;
		blob.length = IPMSubTree[c].length;
		blob.opnum = IPMSubTree[c].opnum;
		blob.content = IPMSubTree[c].content;

		ret &= NT_STATUS_IS_OK(emsmdb_transaction_unknown(emsmdb, blob));
	};

	return ret;
}

static BOOL OpenInbox(struct emsmdb_context *emsmdb)
{
        BOOL            ret = True;
        struct MAPI_DATA       blob;

	DEBUG(0, ("\nStep 5 - OpenInbox\n"));

	blob.mapi_len = OpenInbox_EcDoRpc_1_mapilen;
	blob.length = OpenInbox_EcDoRpc_1_length;
	blob.opnum = OpenInbox_EcDoRpc_1_opnum;
        blob.content = OpenInbox_EcDoRpc_1_content;

	ret &= NT_STATUS_IS_OK(emsmdb_transaction_unknown(emsmdb, blob));

	return ret;
}


BOOL torture_rpc_exchange(struct torture_context *torture)
{
	NTSTATUS		status;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	BOOL			ret = True;
	struct emsmdb_context	*emsmdb;

	mem_ctx = talloc_init("torture_rpc_emsmdb");
	status = torture_rpc_connection(mem_ctx, &p, &dcerpc_table_exchange_emsmdb);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return False;
	}

	emsmdb = emsmdb_connect(mem_ctx, p, cmdline_credentials);
	if (!emsmdb) {
		return False;
	}

	emsmdb->mem_ctx = mem_ctx;
	ret &= OpenMessageStore(emsmdb);
	ret &= OpenRootFolder(emsmdb);
	ret &= OpenIPMSubTree(emsmdb);
	ret &= OpenInbox(emsmdb);


	talloc_free(mem_ctx);

	return ret;
}
