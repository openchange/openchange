/*
   OpenChange NSPI torture suite implementation.

   Create a MAPI profile

   Copyright (C) Julien Kerihuel 2005 - 2007.
   
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

bool set_profile_attribute(const char *profname, struct SRowSet rowset, 
			   uint32_t property, const char *attr)
{
	struct SPropValue	*lpProp;
	enum MAPISTATUS		ret;

	lpProp = get_SPropValue_SRowSet(&rowset, property);

	if (!lpProp) {
		DEBUG(0, ("MAPI Property %s not set\n", attr));
		return true;
	}

	ret = mapi_profile_add_string_attr(profname, attr, lpProp->value.lpszA);

	if (ret != MAPI_E_SUCCESS) {
		DEBUG(0, ("Problem adding attribute %s in profile %s\n", attr, profname));
		return false;
	}
	return true;
}

bool set_profile_mvstr_attribute(const char *profname, struct SRowSet rowset,
				 uint32_t property, const char *attr)
{
	struct SPropValue	*lpProp;
	enum MAPISTATUS		ret;
	int			i;

	lpProp = get_SPropValue_SRowSet(&rowset, property);

	if (!lpProp) {
		DEBUG(0, ("MAPI Property %s not set\n", attr));
		return true;
	}

	for (i = 0; i < lpProp->value.MVszA.cValues; i++) {
		ret = mapi_profile_add_string_attr(profname, attr, lpProp->value.MVszA.lppszA[i]);
		if (ret != MAPI_E_SUCCESS) {
			DEBUG(0, ("Problem adding attriute %s in profile %s\n", attr, profname));
			return false;
		}
	}
	return true;
}

bool torture_rpc_nspi_profile(struct torture_context *torture)
{
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	struct nspi_context	*nspi;
	struct SPropTagArray	*SPropTagArray;
	struct SPropTagArray	*MIds = NULL;
	struct SPropTagArray	MIds2;
	struct SPropTagArray	*MId_server = NULL;
	struct StringsArray_r	pNames;
	struct Restriction_r	Filter;
	struct SRowSet		*rowset;
	struct SPropValue	*lpProp;
	const char		*profname = lp_parm_string(torture->lp_ctx, NULL, "mapi", "profile");
	const char		*profdb = lp_parm_string(torture->lp_ctx, NULL, "mapi", "profile_store");
	uint32_t		codepage = lp_parm_int(torture->lp_ctx, NULL, "mapi", "codepage", 0);
	uint32_t		language = lp_parm_int(torture->lp_ctx, NULL, "mapi", "language", 0);
	uint32_t		method = lp_parm_int(torture->lp_ctx, NULL, "mapi", "method", 0);
	const char		*username = NULL;
	uint32_t		instance_key = 0;

	mem_ctx = talloc_named(NULL, 0, "torture_rpc_nspi_profile");
	
	status = torture_rpc_connection(torture, &p, &ndr_table_exchange_nsp);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);

		return false;
	}

	/* profiles */
	retval = MAPIInitialize(profdb);
	mapi_errstr("MAPIInitialize", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	nspi = nspi_bind(mem_ctx, p, cmdline_credentials, codepage, language, method);
	if (!nspi) return false;
	
	if (profname) {
		const char *username = cli_credentials_get_username(cmdline_credentials);
		const char *password = cli_credentials_get_password(cmdline_credentials);
		
		retval = CreateProfile(profname, username, password, 0);
		mapi_errstr("CreateProfile", GetLastError());
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(0, ("Unable to create %s profile\n", profname));
			return false;
		}
		{
			const char *workstation = cli_credentials_get_workstation(cmdline_credentials);
			const char *domain = cli_credentials_get_domain(cmdline_credentials);
			const char *binding = lp_parm_string(torture->lp_ctx, 
												 NULL, "torture", "binding");
			struct dcerpc_binding *dcerpc_binding;
			char *p_codepage = talloc_asprintf(mem_ctx, "0x%x", codepage);
			char *p_language = talloc_asprintf(mem_ctx, "0x%x", language);
			char *p_method = talloc_asprintf(mem_ctx, "0x%x", method);
			
			dcerpc_parse_binding(mem_ctx, binding, &dcerpc_binding);

			retval = mapi_profile_add_string_attr(profname, "workstation", workstation);
			retval = mapi_profile_add_string_attr(profname, "domain", domain);
			retval = mapi_profile_add_string_attr(profname, "binding", dcerpc_binding->host);
			retval = mapi_profile_add_string_attr(profname, "codepage", p_codepage);
			retval = mapi_profile_add_string_attr(profname, "language", p_language);
			retval = mapi_profile_add_string_attr(profname, "method", p_method);
		}
	}
	
	nspi->mem_ctx = mem_ctx;

	retval = nspi_GetSpecialTable(nspi, mem_ctx, 0, &rowset);
	mapi_errstr("NspiGetSpecialTable", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		talloc_free(mem_ctx);
		return false;
	}

	SPropTagArray = set_SPropTagArray(nspi->mem_ctx, 0xc,
					  PR_DISPLAY_NAME,
					  PR_OFFICE_TELEPHONE_NUMBER,
					  PR_OFFICE_LOCATION,
					  PR_TITLE,
					  PR_COMPANY_NAME,
					  PR_ACCOUNT,
					  PR_ADDRTYPE,
					  PR_ENTRYID,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_INSTANCE_KEY,
					  PR_EMAIL_ADDRESS
					  );

	/* Set the username to match */
	username = cli_credentials_get_username(nspi->cred);
	if (!username) return false;

	/* Build the restriction we want for NspiGetMatches */
	lpProp = talloc_zero(nspi->mem_ctx, struct SPropValue);
	lpProp->ulPropTag = PR_ANR_UNICODE;
	lpProp->dwAlignPad = 0;
	lpProp->value.lpszW = username;

	Filter.rt = RES_PROPERTY;
	Filter.res.resProperty.relop = RES_PROPERTY;
	Filter.res.resProperty.ulPropTag = PR_ANR_UNICODE;
	Filter.res.resProperty.lpProp = lpProp;

	rowset = talloc_zero(nspi->mem_ctx, struct SRowSet);
	MIds = talloc_zero(nspi->mem_ctx, struct SPropTagArray);
	retval = nspi_GetMatches(nspi, nspi->mem_ctx, SPropTagArray, &Filter, &rowset, &MIds);
	MAPIFreeBuffer(lpProp);
	mapi_errstr("NspiGetMatches", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;
	
	lpProp = get_SPropValue_SRowSet(rowset, PR_EMAIL_ADDRESS);
	if (lpProp) {
		DEBUG(3, ("PR_EMAIL_ADDRESS: %s\n", lpProp->value.lpszA));
		nspi->org = x500_get_dn_element(nspi->mem_ctx, lpProp->value.lpszA, ORG);
		nspi->org_unit = x500_get_dn_element(nspi->mem_ctx, lpProp->value.lpszA, ORG_UNIT);
		MAPI_RETVAL_IF(!nspi->org_unit, MAPI_E_INVALID_PARAMETER, mem_ctx);
		MAPI_RETVAL_IF(!nspi->org, MAPI_E_INVALID_PARAMETER, mem_ctx);
	}

	if (profname) {
		set_profile_attribute(profname, *rowset, PR_EMAIL_ADDRESS, "EmailAddress");
		set_profile_attribute(profname, *rowset, PR_DISPLAY_NAME, "DisplayName");
		set_profile_attribute(profname, *rowset, PR_ACCOUNT, "Account");
		set_profile_attribute(profname, *rowset, PR_ADDRTYPE, "AddrType");
		retval = mapi_profile_add_string_attr(profname, "Organization", nspi->org);
		retval = mapi_profile_add_string_attr(profname, "OrganizationUnit", nspi->org_unit);
	}

	SPropTagArray = set_SPropTagArray(nspi->mem_ctx, 0x7,
					  PR_DISPLAY_NAME,
					  PR_EMAIL_ADDRESS,
					  PR_DISPLAY_TYPE,
					  PR_EMS_AB_HOME_MDB,
					  PR_ATTACH_NUM,
					  PR_PROFILE_HOME_SERVER_ADDRS,
					  PR_EMS_AB_PROXY_ADDRESSES
					  );

	nspi->pStat->CurrentRec = 0x0;
	nspi->pStat->Delta = 0x0;
	nspi->pStat->NumPos = 0x0;
	nspi->pStat->TotalRecs = 0x1;

	instance_key = MIds->aulPropTag[0];
	MIds2.cValues = 0x1;
	MIds2.aulPropTag = (enum MAPITAGS *) &instance_key;

	retval = nspi_QueryRows(nspi, nspi->mem_ctx, SPropTagArray, &MIds2, 1, &rowset);
	mapi_errstr("NspiQueryRows", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	lpProp = get_SPropValue_SRowSet(rowset, PR_EMS_AB_HOME_MDB);
	if (lpProp) {
		nspi->servername = x500_get_servername(lpProp->value.lpszA);
		if (profname) {
			mapi_profile_add_string_attr(profname, "ServerName", nspi->servername);
			set_profile_attribute(profname, *rowset, PR_EMS_AB_HOME_MDB, "HomeMDB");
			set_profile_mvstr_attribute(profname, *rowset, PR_EMS_AB_PROXY_ADDRESSES, "ProxyAddress");
		}
	} else {
		printf("Unable to find the server name\n");
		return -1;
	}


	MId_server = talloc_zero(nspi->mem_ctx, struct SPropTagArray);
	pNames.Count = 0x1;
	pNames.Strings = (const char **) talloc_array(nspi->mem_ctx, char **, 1);
	pNames.Strings[0] = (const char *) talloc_asprintf(nspi->mem_ctx, SERVER_DN, 
							   nspi->org, nspi->org_unit, 
							   nspi->servername);
	retval = nspi_DNToMId(nspi, nspi->mem_ctx, &pNames, &MId_server);
	mapi_errstr("NspiDNToMId", GetLastError());
	MAPIFreeBuffer((char *)pNames.Strings[0]);
	MAPIFreeBuffer((char **)pNames.Strings);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(nspi->mem_ctx, 0x2,
					  PR_EMS_AB_NETWORK_ADDRESS);
	retval = nspi_GetProps(nspi, nspi->mem_ctx, SPropTagArray, MId_server, &rowset);
	mapi_errstr("NspiGetProps", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	if (profname) {
		set_profile_mvstr_attribute(profname, *rowset, PR_EMS_AB_NETWORK_ADDRESS, "NetworkAddress");
	}

	retval = nspi_unbind(nspi);
	mapi_errstr("NspiUnbind", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	MAPIUninitialize();

	talloc_free(mem_ctx);

	return true;
}
