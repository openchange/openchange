/* OpenChange MAPI implementation testsuite

   fetch mail from an Exchange server

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

#include "openchange.h"
#include <torture/torture.h>
#include "ndr_exchange.h"
#include "libmapi/mapicode.h"
#include "libmapi/include/mapidefs.h"
#include "libmapi/include/emsmdb.h"
#include "libmapi/include/mapi_proto.h"

#define CN_MSG_PROPS 3

/* FIXME: Should be part of Samba's data: */
NTSTATUS torture_rpc_connection(TALLOC_CTX*, struct dcerpc_pipe**, const struct dcerpc_interface_table *);

static char **get_cmdline_recipients(TALLOC_CTX *mem_ctx)
{
	char		**usernames;
	const char	*recipients = lp_parm_string(-1, "mapi", "to");
	char		*tmp = NULL;
	uint32_t	j = 0;

	/* no recipients */
	if (recipients == 0)
	  return 0;

	if ((tmp = strtok((char *)recipients, ",")) == NULL){
		DEBUG(2, ("Invalid mapi:to string format\n"));
		return NULL;
	}
	
	usernames = talloc_array(mem_ctx, char *, 2);
	usernames[0] = strdup(tmp);
	
	for (j = 1; (tmp = strtok(NULL, ",")) != NULL; j++) {
		usernames = talloc_realloc(mem_ctx, usernames, char *, j+2);
		usernames[j] = strdup(tmp);
	}
	usernames[j] = 0;

	return (usernames);
}

BOOL torture_rpc_mapi_sendattach(struct torture_context *torture)
{
	NTSTATUS		status;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	BOOL			ret = True;
	struct emsmdb_context	*emsmdb;
	const char		*organization = lp_parm_string(-1, "exchange", "organization");
	const char		*group = lp_parm_string(-1, "exchange", "ou");
	const char		*subject = lp_parm_string(-1, "mapi", "subject");
	const char		*body = lp_parm_string(-1, "mapi", "body");
	const char		*username;
	char			**usernames;
	char			*mailbox_path;
	uint32_t		hdl_msgstore, hdl_outbox, hdl_message, hdl_attach;
	uint64_t		id_outbox;
	struct SRowSet		*SRowSet;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	SPropValue;
	struct SPropValue	props[CN_MSG_PROPS];
	uint32_t		msgflag;


	mem_ctx = talloc_init("torture_rpc_mapi_sendattach");

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

	if (!organization) {
	  organization = cli_credentials_get_domain(cmdline_credentials);
	}
	username = cli_credentials_get_username(cmdline_credentials);
	if (!organization || !username) {
		DEBUG(1,("mapi_fetchmail: username and domain required"));
		return NULL;
	}

	if (MAPIInitialize(emsmdb, cmdline_credentials) != MAPI_E_SUCCESS) {
		printf("MAPIInitialize failed\n");	
		exit (1);
	}

	/* check for null contents */
	if (organization == 0)
	  {
	    printf("organization == (null)\n");
	    exit(1);
	  }
	if (group == 0)
	  {
	    printf("group == (null)\n");
	    exit(1);
	  }

	/* default if null */
	if (subject == 0)
	  subject = "";
	if (body == 0)
	  body = "";


	/* OpenMsgStore */
	mailbox_path = talloc_asprintf(emsmdb->mem_ctx, MAILBOX_PATH, organization, group, username);
	OpenMsgStore(emsmdb, 0, &hdl_msgstore, &id_outbox, mailbox_path);

	/* OpenFolder: open outbox */
	OpenFolder(emsmdb, 0, hdl_msgstore, id_outbox, &hdl_outbox);

	/* CreateMessage */
	CreateMessage(emsmdb, 0, hdl_outbox, id_outbox, &hdl_message);

	/* ModifyRecipient */	
	SPropTagArray = set_SPropTagArray(emsmdb->mem_ctx, 0x6,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_7BIT_DISPLAY_NAME,
					  PR_DISPLAY_NAME,
					  PR_SMTP_ADDRESS,
					  PR_GIVEN_NAME);

	usernames = get_cmdline_recipients(emsmdb->mem_ctx);
	if (usernames == 0)
	  {
	    printf("recipient_names == (null)\n");
	    exit(1);
	  }

	SRowSet = ResolveNames(emsmdb, (const char **)usernames, SPropTagArray);

	SPropValue.ulPropTag = PR_SEND_INTERNET_ENCODING;
	SPropValue.value.l = 0;
	SRowSet_propcpy(emsmdb->mem_ctx, &SRowSet[0], SPropValue);

	ModifyRecipients(emsmdb, 0, SRowSet, hdl_message);

	/* SetProps */
	msgflag = MSGFLAG_UNSENT;
	set_SPropValue_proptag(&props[0], PR_SUBJECT, (void *)subject);
	set_SPropValue_proptag(&props[1], PR_BODY, (void *)body);
	set_SPropValue_proptag(&props[2], PR_MESSAGE_FLAGS, (void *)&msgflag);
	SetProps(emsmdb, 0, props, CN_MSG_PROPS, hdl_message);

	/* create attachment */
	{
#define CN_ATTACH_PROPS 5
	  struct SPropValue props_attach[CN_ATTACH_PROPS];
	  unsigned long cn_props_attach;

	  printf("%08x->CreateAttach() == ", hdl_message); fflush(stdout);
	  CreateAttach(emsmdb, 0, hdl_message, &hdl_attach);
	  printf("%08x\n", hdl_attach);

	  /* send by reference */
/* 	  { */
/* 	    props_attach[0].ulPropTag = PR_ATTACH_METHOD; */
/* 	    props_attach[0].value.l = ATTACH_BY_REFERENCE; */
/* 	    props_attach[1].ulPropTag = PR_RENDERING_POSITION; */
/* 	    props_attach[1].value.l = -1; */
/* 	    props_attach[2].ulPropTag = PR_ATTACH_PATHNAME; */
/* 	    props_attach[2].value.lpszA = "\\\\exchange.local\\tmp\\attachment.file"; */
/* 	    props_attach[3].ulPropTag = PR_DISPLAY_NAME; */
/* 	    props_attach[3].value.lpszA = "attachment_display_name"; */
/* 	    props_attach[4].ulPropTag = PR_ATTACH_FILENAME; */
/* 	    props_attach[4].value.lpszA = "attachment.file"; */
/* 	    cn_props_attach = 5; */
/* 	  } */
	  /* send by value */
	  {
	    props_attach[0].ulPropTag = PR_ATTACH_METHOD;
	    props_attach[0].value.l = ATTACH_BY_VALUE;
	    props_attach[1].ulPropTag = PR_RENDERING_POSITION;
	    props_attach[1].value.l = -1;
	    props_attach[2].ulPropTag = PR_ATTACH_DATA_BIN;
	    props_attach[2].value.bin.lpb = (uint8_t *)"super test2";
	    props_attach[2].value.bin.cb = strlen("super test2") + 1;
	    props_attach[3].ulPropTag = PR_DISPLAY_NAME;
	    props_attach[3].value.lpszA = "attach.txt";
	    cn_props_attach = 4;
	  }

	  printf("%08x->SetProps2()", hdl_attach); fflush(stdout);
	  SetProps2(emsmdb, 0, props_attach, cn_props_attach, hdl_message, hdl_attach);
	  printf("\n");

	  printf("%08x->SaveChanges()\n", hdl_attach); fflush(stdout);
	  SaveChanges(emsmdb, 0, hdl_message, hdl_attach);

	}

	/* SubmitMessage */
	SubmitMessage(emsmdb, 0, hdl_message);

	MAPIUninitialize(emsmdb);

	talloc_free(mem_ctx);
	
	return (ret);
}
