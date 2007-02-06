/*
 *  OpenChange NSPI implementation.
 *
 *  Copyright (C) Julien Kerihuel 2005-2007
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
#include "ndr_exchange_c.h"
#include "libmapi/include/nspi.h"
#include "libmapi/include/mapidefs.h"
#include "libmapi/mapicode.h"
#include "libmapi/include/proto.h"
#include "libmapi/include/mapi_proto.h"

/*
 * nspi_set_MAPI_SETTINGS
 * set the client language/keyboard preferences (fetched from smb.conf
 * or inline)
 */

static struct MAPI_SETTINGS *nspi_set_MAPI_SETTINGS(TALLOC_CTX *mem_ctx)
{
	struct MAPI_SETTINGS	*settings;
	uint32_t		codepage = lp_parm_int(-1, "locale", "codepage", 0);
	uint32_t		language = lp_parm_int(-1, "locale", "language", 0);
	uint32_t		method = lp_parm_int(-1, "locale", "method", 0);

	if (!codepage || !language || !method) {
		DEBUG(3, ("nspi_set_dflt_settings: Invalid parameter\n"));
		return NULL;
	}

	settings = talloc(mem_ctx, struct MAPI_SETTINGS);
	settings->handle = 0;
	settings->flag = 0;

	memset(settings, 0, 16);

	settings->codepage = codepage;
	settings->input_locale.language = language;
	settings->input_locale.method = method;

	return settings;
}

/*
 * nspi_bind
 * Bind on the nspi endpoint and returns an opaque nspi_context
 * structure
 */

struct nspi_context *nspi_bind(TALLOC_CTX *mem_ctx, struct dcerpc_pipe *p, struct cli_credentials *cred)
{
	struct NspiBind		r;
	NTSTATUS		status;
	struct nspi_context	*ret;
	struct GUID		guid;

	ret = talloc(mem_ctx, struct nspi_context);
	ret->rpc_connection = p;
	ret->mem_ctx = mem_ctx;
	ret->cred = cred;
	if (!(ret->settings = nspi_set_MAPI_SETTINGS(mem_ctx))) {
		DEBUG(0, ("nspi_set_MAPI_SETTINGS: Invalid parameter\n"));
		return NULL;
	}
	ret->profile = talloc(mem_ctx, struct mapi_profile);
	ret->profile->instance_key = talloc(mem_ctx, uint32_t);

	r.in.unknown = 0;

	r.in.settings = ret->settings;
	r.in.settings->flag = 0x0;

	memset(r.in.settings->service_provider.ab, 0, 16);
	r.in.settings->service_provider.ab[12] = 0xd6;
	r.in.settings->service_provider.ab[13] = 0x11;
	r.in.settings->service_provider.ab[14] = 0x43;
	r.in.settings->service_provider.ab[15] = 0x80;

	r.in.mapiuid = talloc(mem_ctx, struct GUID);
	memset(r.in.mapiuid, 0, sizeof(struct GUID));
	
	r.out.mapiuid = &guid;

	r.in.mapiuid = talloc(mem_ctx, struct GUID);
	memset(r.in.mapiuid, 0, sizeof(struct GUID));

	r.out.handle = &ret->handle;


	status = dcerpc_NspiBind(p, mem_ctx, &r);

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status)) || r.out.result != MAPI_E_SUCCESS) {
		mapi_errstr("NspiBind", r.out.result);
		return NULL;
	}
	
	mapi_errstr("NspiBind", r.out.result);

	return ret;
}


/*
 * nspi_unbind
 * Unbind from the endpoint
 */

BOOL nspi_unbind(struct nspi_context *nspi)
{
	struct NspiUnbind	r;
	NTSTATUS		status;

	r.in.handle = r.out.handle = &nspi->handle;
	r.in.status = 0;

	status = dcerpc_NspiUnbind(nspi->rpc_connection, nspi->mem_ctx, &r);

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		mapi_errstr("NspiUnbind", r.out.result);
		return False;
	}

	mapi_errstr("NspiUnbind", r.out.result);

	return True;
}

/*
 * nspi_UpdateStat
 *
 */

BOOL nspi_UpdateStat(struct nspi_context *nspi)
{
	struct NspiUpdateStat		r;
	NTSTATUS			status;

	r.in.handle = &nspi->handle;

	r.in.settings = *nspi->settings;
	memset(r.in.settings.service_provider.ab, 0, 16);
	memset(r.in.settings.service_provider.ab + 12, 0xff, 4);

	status = dcerpc_NspiUpdateStat(nspi->rpc_connection, nspi->mem_ctx, &r);


	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		mapi_errstr("NspiUpdateStat", r.out.result);
		return False;
	}
	
	mapi_errstr("NspiUpdateStat", r.out.result);

	return True;
}

/*
 * nspi_QueryRows
 * retrieve mapitags/x500 attr values from the object requested in
 * NspiGetMatches
 */

BOOL nspi_QueryRows(struct nspi_context *nspi, struct SPropTagArray *SPropTagArray)
{
	struct NspiQueryRows		r;
	NTSTATUS			status;
	struct MAPI_SETTINGS		*settings;

	r.in.handle = &nspi->handle;
	r.in.flag = 0x0;

	r.in.settings = nspi->settings;
	memset(r.in.settings->service_provider.ab, 0, 16);
	r.in.settings->service_provider.ab[12] = 0x1;

	r.in.lRows = 0x1;

	r.in.instance_key = nspi->profile->instance_key;
	DEBUG(5, ("r.in.instance_key = 0x%x\n", *r.in.instance_key));

	r.in.unknown = 0x1;
	
 	r.in.REQ_properties = SPropTagArray;

	settings = talloc(nspi->mem_ctx, struct MAPI_SETTINGS);
	r.out.settings = settings;

	nspi->rowSet = talloc(nspi->mem_ctx, struct SRowSet);
	r.out.RowSet = &(nspi->rowSet);

	status = dcerpc_NspiQueryRows(nspi->rpc_connection, nspi->mem_ctx, &r);

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		mapi_errstr("NspiQueryRows", r.out.result);
		return False;
	}
	
	mapi_errstr("NspiQueryRows", r.out.result);

	return True;

}

/*
 * nspi_GetMatches
 * Request an exchange server for a view (mapitable) on an Address
 * Book object and retrieve an instance key
 *
 */

BOOL nspi_GetMatches(struct nspi_context *nspi, struct SPropTagArray *SPropTagArray)
{
	struct NspiGetMatches		r;
	NTSTATUS			status;
	struct SPropertyRestriction	*prop_restrictions;
	struct SPropValue		*lpProp;
	struct instance_key		*instance_key;
	struct MAPI_SETTINGS		*settings;
	const char			*username = lp_parm_string(-1, "exchange", "username");
	struct SBinary			bin;

	if (!username) {
		username = cli_credentials_get_username(nspi->cred);
		if (!username) {
			DEBUG(0, ("nspi_GetMatches: not loggued on the domain\n"));
		}
	}
		
	r.in.handle = &nspi->handle;
	r.in.unknown1 = 0;
	
	r.in.settings = nspi->settings;
	r.in.settings->flag = 0x0;

	memset(r.in.settings->service_provider.ab, 0, 16);

	r.in.PropTagArray = NULL;
	r.in.unknown2 = 0;

	r.in.restrictions = talloc(nspi->mem_ctx, struct SRestriction);
	r.in.restrictions->rt = RES_PROPERTY;

	prop_restrictions = talloc(nspi->mem_ctx, struct SPropertyRestriction);
	prop_restrictions->relop = RES_PROPERTY;
	prop_restrictions->ulPropTag = PR_ANR;

	lpProp = talloc(nspi->mem_ctx, struct SPropValue);
	lpProp->ulPropTag = PR_ANR;
	lpProp->dwAlignPad = 0;
	lpProp->value.lpszA = username;
	prop_restrictions->lpProp = lpProp;

	r.in.restrictions->res.resProperty = *prop_restrictions;

	r.in.unknown3 = 0;

	r.in.REQ_properties = SPropTagArray;
	
	settings = talloc(nspi->mem_ctx, struct MAPI_SETTINGS);
	r.out.settings = settings;

	instance_key = talloc(nspi->mem_ctx, struct instance_key);
	r.out.instance_key = instance_key;

	nspi->rowSet = talloc(nspi->mem_ctx, struct SRowSet);
	r.out.RowSet = &(nspi->rowSet);

	status = dcerpc_NspiGetMatches(nspi->rpc_connection, nspi->mem_ctx, &r);

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		mapi_errstr("NspiGetMatches", r.out.result);
		return False;
	}
	
	mapi_errstr("NspiGetMatches", r.out.result);
	
	lpProp = get_SPropValue_SRowSet(*r.out.RowSet, PR_INSTANCE_KEY);
	if (lpProp) {
	  
	  bin = lpProp->value.bin;
          
	  nspi->profile->instance_key = (uint32_t *)bin.lpb;
	} else {
	  nspi->profile->instance_key = 0;
	}	

	lpProp = get_SPropValue_SRowSet(*r.out.RowSet, PR_EMAIL_ADDRESS);
	if (lpProp) {
		DEBUG(3, ("PR_EMAIL_ADDRESS: %s\n", lpProp->value.lpszA));
		nspi->org = x500_get_dn_element(nspi->mem_ctx, lpProp->value.lpszA, ORG);
		nspi->org_unit = x500_get_dn_element(nspi->mem_ctx, lpProp->value.lpszA, ORG_UNIT);
		if (!nspi->org_unit || !nspi->org) return False;
	}

	return True;
}

/*
 * nspi_DNToEph
 * Request for an object giving its DN and retrieve its instance key
 * This function is used during MAPI profiles setup to request the
 * Exchange object in the Active Directory
 */

BOOL nspi_DNToEph(struct nspi_context *nspi)
{
	struct NspiDNToEph	r;
	NTSTATUS		status;
	struct NAME_STRING	*server_dn;
	const char		*org = lp_parm_string(-1, "exchange", "org");
	const char		*org_unit = lp_parm_string(-1, "exchange", "org_unit");
	const char		*servername = lp_parm_string(-1, "exchange", "server");
	struct instance_key	*instance_key;

	if (!servername || (!nspi->org && !org) || (!nspi->org_unit && !org_unit)) {
		DEBUG(0, ("server, organization or organization unit name required\n"));
		return False;
	}


	r.in.handle = &nspi->handle;
	r.in.flag = 0;
	r.in.size = 1;

	server_dn = talloc(nspi->mem_ctx, struct NAME_STRING);
	server_dn->str = talloc_asprintf(nspi->mem_ctx, SERVER_DN,
					 nspi->org ? nspi->org : org,
					 nspi->org_unit ? nspi->org_unit : org_unit,
					 servername);

	r.in.server_dn = server_dn;

	instance_key = talloc(nspi->mem_ctx, struct instance_key);
	r.out.instance_key = instance_key;

	status = dcerpc_NspiDNToEph(nspi->rpc_connection, nspi->mem_ctx, &r);

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		mapi_errstr("NspiDNToEph", r.out.result);
		return False;
	}

	nspi->profile->instance_key =  instance_key->value;

	mapi_errstr("NspiDNToEph", r.out.result);

	return True;
}


/*
 * nspi_GetProps
 * request for properties associated with the given mapiuid (mapiuid
 * is computed with the instance key)
 */

BOOL nspi_GetProps(struct nspi_context *nspi, struct SPropTagArray *SPropTagArray)
{
	struct NspiGetProps	r;
	NTSTATUS		status;
	struct SRow		*REPL_values;

	r.in.handle = &nspi->handle;
	r.in.flag = 0;

	if (!nspi->profile->instance_key) {
		return False;
	}

	r.in.settings = nspi->settings;
	memset(r.in.settings->service_provider.ab, 0, 16);
	r.in.settings->service_provider.ab[0] = *nspi->profile->instance_key;
	r.in.settings->service_provider.ab[1] = (*nspi->profile->instance_key & 0xFF00) >> 8;

 	r.in.REQ_properties = SPropTagArray;

	REPL_values = talloc(nspi->mem_ctx, struct SRow);
	r.out.REPL_values = &REPL_values;

	nspi->rowSet = talloc(nspi->mem_ctx, struct SRowSet);
	nspi->rowSet->cRows = 1;
	nspi->rowSet->aRow = talloc_size(nspi->rowSet, sizeof(struct SRow));

	status = dcerpc_NspiGetProps(nspi->rpc_connection, nspi->mem_ctx, &r);

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		mapi_errstr("NspiGetProps", r.out.result);
		return False;
	}

	nspi->rowSet->aRow[0].ulAdrEntryPad = REPL_values->ulAdrEntryPad;
	nspi->rowSet->aRow[0].cValues = REPL_values->cValues;
	nspi->rowSet->aRow[0].lpProps = REPL_values->lpProps;
	
	mapi_errstr("NspiGetProps", r.out.result);

	return True;
}

/*
 * nspi_GetHierarchyInfo
 * Retrieve the Address Book container hierarchy
 */

BOOL nspi_GetHierarchyInfo(struct nspi_context *nspi)
{
	struct NspiGetHierarchyInfo	r;
	NTSTATUS			status;
	uint32_t       			unknown = 0;
	struct SRowSet			*SRowSet;

	r.in.handle = &nspi->handle;
	r.in.unknown1 = 0;

	r.in.settings = nspi->settings;
	r.in.settings->flag = 0xffff;

	memset(r.in.settings->service_provider.ab, 0, 16);
	r.in.settings->service_provider.ab[12] = 0x98;
	r.in.settings->service_provider.ab[13] = 0x6a;
	r.in.settings->service_provider.ab[14] = 0xf8;
	r.in.settings->service_provider.ab[15] = 0x77;

	r.in.unknown2 = &unknown;

	r.out.unknown2 = &unknown;

	SRowSet = talloc(nspi->mem_ctx, struct SRowSet);
	r.out.RowSet = &SRowSet;

	status = dcerpc_NspiGetHierarchyInfo(nspi->rpc_connection, nspi->mem_ctx, &r);

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		mapi_errstr("NspiGetHierarchyInfo", r.out.result);
		return False;
	}
	
	mapi_errstr("NspiGetHierarchyInfo", r.out.result);

	return True;
}

/*
 * nspi_ResolveNames
 * query WAB (Windows Address Book) and try to resolve the provided
 * name and return a SRowSet with the requested property tags fetched
 * values.
 */

BOOL nspi_ResolveNames(struct nspi_context *nspi, const char **usernames, struct SPropTagArray *props, struct SRowSet *rowSet)
{
	struct NspiResolveNames r;
	struct FlagList		*FlagList;
	struct names		*names;
	NTSTATUS		status;
	uint32_t		count;

	for (count = 0; usernames[count]; count++);

	r.in.handle = &nspi->handle;

	r.in.settings = nspi->settings;
	r.in.dwAlignPad = 0;
	r.in.SPropTagArray = props;

	names = talloc(nspi->mem_ctx, struct names);
	names->cEntries = names->count = count;
	names->recipient = usernames;
	r.in.recipients = names;

	r.out.RowSet = &rowSet;

	FlagList = talloc(nspi->mem_ctx, struct FlagList);
	r.out.flags = &FlagList;

	status = dcerpc_NspiResolveNames(nspi->rpc_connection, nspi->mem_ctx, &r);

	if (!MAPI_STATUS_IS_OK(NT_STATUS_V(status))) {
		mapi_errstr("NspiResolveNames", r.out.result);
		return False;
	}
	
	mapi_errstr("NspiResolveNames", r.out.result);

	return True;
}
