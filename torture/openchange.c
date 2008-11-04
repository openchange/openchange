/*
   Unix EMSMDB implementation

   Test suite for OpenChange

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
#include <samba/popt.h>
#include <torture/mapi_torture.h>
#include <torture.h>
#include <torture/torture_proto.h>
#include <smbtorture.h>

NTSTATUS ndr_table_init(void);

NTSTATUS samba_init_module(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "OPENCHANGE");

	ndr_table_init();

	DEBUG(0, ("Loading openchange torture test\n"));

	/* OpenChange torture tests */
	/* Address Book name resolution */
	torture_suite_add_simple_test(suite, "NSPI-PROFILE", torture_rpc_nspi_profile);
	torture_suite_add_simple_test(suite, "NSPI-RESOLVENAMES", torture_rpc_nspi_resolvenames);
	/* MAPI mail torture tests */
	torture_suite_add_simple_test(suite, "MAPI-FETCHMAIL", torture_rpc_mapi_fetchmail);
	torture_suite_add_simple_test(suite, "MAPI-FETCHATTACH", torture_rpc_mapi_fetchattach);
	torture_suite_add_simple_test(suite, "MAPI-SENDMAIL", torture_rpc_mapi_sendmail);
	torture_suite_add_simple_test(suite, "MAPI-SENDMAIL-HTML", torture_rpc_mapi_sendmail_html);
	torture_suite_add_simple_test(suite, "MAPI-SENDATTACH", torture_rpc_mapi_sendattach);
        torture_suite_add_simple_test(suite, "MAPI-DELETEMAIL", torture_rpc_mapi_deletemail);
        torture_suite_add_simple_test(suite, "MAPI-NEWMAIL", torture_rpc_mapi_newmail);
	torture_suite_add_simple_test(suite, "MAPI-COPYMAIL", torture_rpc_mapi_copymail);
	/* MAPI calendar torture tests */
	torture_suite_add_simple_test(suite, "MAPI-FETCHAPPOINTMENT", torture_rpc_mapi_fetchappointment);
	torture_suite_add_simple_test(suite, "MAPI-SENDAPPOINTMENT", torture_rpc_mapi_sendappointment);
	torture_suite_add_simple_test(suite, "MAPI-FETCHCONTACTS", torture_rpc_mapi_fetchcontacts);
	torture_suite_add_simple_test(suite, "MAPI-SENDCONTACTS", torture_rpc_mapi_sendcontacts);
	torture_suite_add_simple_test(suite, "MAPI-FETCHTASKS", torture_rpc_mapi_fetchtasks);
	torture_suite_add_simple_test(suite, "MAPI-SENDTASKS", torture_rpc_mapi_sendtasks);
	/* MAPI object torture tests */
	torture_suite_add_simple_test(suite, "MAPI-ACLS", torture_rpc_mapi_permissions);
	torture_suite_add_simple_test(suite, "MAPI-RESTRICTIONS", torture_rpc_mapi_restrictions);
	torture_suite_add_simple_test(suite, "MAPI-CRITERIA", torture_rpc_mapi_criteria);
	torture_suite_add_simple_test(suite, "MAPI-SORTTABLE", torture_rpc_mapi_sorttable);
	torture_suite_add_simple_test(suite, "MAPI-BOOKMARK", torture_rpc_mapi_bookmark);
	torture_suite_add_simple_test(suite, "MAPI-RECIPIENT", torture_rpc_mapi_recipient);
	torture_suite_add_simple_test(suite, "MAPI-NAMEDPROPS", torture_rpc_mapi_namedprops);
	/* Exchange Administration torture tests */
	torture_suite_add_simple_test(suite, "EXCHANGE-CREATEUSER", torture_mapi_createuser);

	suite->description = talloc_strdup(suite, "Exchange protocols tests (NSPI and EMSMDB)");

	torture_register_suite(suite);
	return NT_STATUS_OK;
}

/**
 * Obtain the DCE/RPC binding context associated with a torture context.
 *
 * @param tctx Torture context
 * @param binding Pointer to store DCE/RPC binding
 */
NTSTATUS torture_rpc_binding(struct torture_context *tctx,
			     struct dcerpc_binding **binding)
{
	NTSTATUS status;
	const char *binding_string = torture_setting_string(tctx, "binding",
							    NULL);

	if (binding_string == NULL) {
		torture_comment(tctx,
				"You must specify a DCE/RPC binding string\n");
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = dcerpc_parse_binding(tctx, binding_string, binding);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(0,("Failed to parse dcerpc binding '%s'\n",
			 binding_string));
		return status;
	}

	return NT_STATUS_OK;
}

/**
 * open a rpc connection to the chosen binding string
 */
_PUBLIC_ NTSTATUS torture_rpc_connection(struct torture_context *tctx,
				struct dcerpc_pipe **p,
				const struct ndr_interface_table *table)
{
	NTSTATUS status;
	struct dcerpc_binding *binding;

	status = torture_rpc_binding(tctx, &binding);
	if (NT_STATUS_IS_ERR(status))
		return status;

	status = dcerpc_pipe_connect_b(tctx,
				     p, binding, table,
				     cmdline_credentials, NULL, tctx->lp_ctx);
 
	if (NT_STATUS_IS_ERR(status)) {
		printf("Failed to connect to remote server: %s %s\n",
			   dcerpc_binding_string(tctx, binding), nt_errstr(status));
	}

	return status;
}
