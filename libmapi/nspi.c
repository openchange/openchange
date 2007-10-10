/*
   OpenChange NSPI implementation.

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
#include <libmapi/proto_private.h>
#include <gen_ndr/ndr_exchange_c.h>
#include <param.h>
#include <credentials.h>

/*
 * nspi_set_MAPI_SETTINGS
 * set the client language/keyboard preferences (fetched from smb.conf
 * or inline)
 */

static struct MAPI_SETTINGS *nspi_set_MAPI_SETTINGS(TALLOC_CTX *mem_ctx, uint32_t codepage,
						    uint32_t language, uint32_t method)
{
	struct MAPI_SETTINGS	*settings;

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

_PUBLIC_ struct nspi_context *nspi_bind(TALLOC_CTX *mem_ctx, struct dcerpc_pipe *p,
					struct cli_credentials *cred, uint32_t codepage,
					uint32_t language, uint32_t method)
{
	struct NspiBind		r;
	NTSTATUS		status;
	struct nspi_context	*ret;
	struct GUID		guid;

	ret = talloc(mem_ctx, struct nspi_context);
	ret->rpc_connection = p;
	ret->mem_ctx = mem_ctx;
	ret->cred = cred;
	if (!(ret->settings = nspi_set_MAPI_SETTINGS(mem_ctx, codepage, language, method))) {
		DEBUG(0, ("nspi_set_MAPI_SETTINGS: Invalid parameter\n"));
		return NULL;
	}

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
	
	return ret;
}


/**
 * Destructor
 */

int nspi_disconnect_dtor(void *data)
{
	bool res;
	struct mapi_provider	*provider = (struct mapi_provider *) data;

	res = nspi_unbind(provider->ctx);
	return 0;
}


/*
 * nspi_unbind
 * Unbind from the endpoint
 */

_PUBLIC_ enum MAPISTATUS nspi_unbind(struct nspi_context *nspi)
{
	struct NspiUnbind	r;
	NTSTATUS		status;
	enum MAPISTATUS		retval;

	MAPI_RETVAL_IF(!nspi, MAPI_E_NOT_INITIALIZED, NULL);

	r.in.handle = r.out.handle = &nspi->handle;
	r.in.status = 0;

	status = dcerpc_NspiUnbind(nspi->rpc_connection, nspi->mem_ctx, &r);
	retval = r.out.result;
	MAPI_RETVAL_IF((retval != 1) && !MAPI_STATUS_IS_OK(NT_STATUS_V(status)), retval, NULL);

	return MAPI_E_SUCCESS;
}

/*
 * nspi_UpdateStat
 *
 */

_PUBLIC_ enum MAPISTATUS nspi_UpdateStat(struct nspi_context *nspi)
{
	struct NspiUpdateStat		r;
	NTSTATUS			status;
	enum MAPISTATUS			retval;

	MAPI_RETVAL_IF(!nspi, MAPI_E_NOT_INITIALIZED, NULL);

	r.in.handle = &nspi->handle;

	r.in.settings = *nspi->settings;
	memset(r.in.settings.service_provider.ab, 0, 16);
	memset(r.in.settings.service_provider.ab + 12, 0xff, 4);

	status = dcerpc_NspiUpdateStat(nspi->rpc_connection, nspi->mem_ctx, &r);
	retval = r.out.result;
	MAPI_RETVAL_IF(!MAPI_STATUS_IS_OK(NT_STATUS_V(status)), retval, NULL);

	return MAPI_E_SUCCESS;
}

/*
 * nspi_QueryRows
 * retrieve mapitags/x500 attr values from the object requested in
 * NspiGetMatches
 */

_PUBLIC_ enum MAPISTATUS nspi_QueryRows(struct nspi_context *nspi, struct SPropTagArray *SPropTagArray,
					struct SRowSet *rowset)
{
	struct NspiQueryRows		r;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	struct MAPI_SETTINGS		*settings;

	MAPI_RETVAL_IF(!nspi, MAPI_E_NOT_INITIALIZED, NULL);

	r.in.handle = &nspi->handle;
	r.in.flag = 0x0;

	r.in.settings = nspi->settings;
	memset(r.in.settings->service_provider.ab, 0, 16);
	r.in.settings->service_provider.ab[12] = 0x1;

	r.in.lRows = 0x1;

	r.in.instance_key = &(nspi->profile_instance_key);
	DEBUG(5, ("r.in.instance_key = 0x%x\n", *r.in.instance_key));

	r.in.unknown = 0x1;
	
 	r.in.REQ_properties = SPropTagArray;

	settings = talloc(nspi->mem_ctx, struct MAPI_SETTINGS);
	r.out.settings = settings;

	r.out.RowSet = &rowset;

	status = dcerpc_NspiQueryRows(nspi->rpc_connection, nspi->mem_ctx, &r);
	retval = r.out.result;
	MAPI_RETVAL_IF(!MAPI_STATUS_IS_OK(NT_STATUS_V(status)), retval, NULL);

	return MAPI_E_SUCCESS;

}

/*
 * nspi_GetMatches
 * Request an exchange server for a view (mapitable) on an Address
 * Book object and retrieve an instance key
 *
 */

_PUBLIC_ enum MAPISTATUS nspi_GetMatches(struct nspi_context *nspi, struct SPropTagArray *SPropTagArray,
					 struct SRowSet *rowset, const char *username)
{
	struct NspiGetMatches		r;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	struct SPropertyRestriction	*prop_restrictions;
	struct SPropValue		*lpProp;
	struct instance_key		*instance_key;
	struct MAPI_SETTINGS		*settings;

	MAPI_RETVAL_IF(!nspi, MAPI_E_NOT_INITIALIZED, NULL);

	if (!username) {
		username = cli_credentials_get_username(nspi->cred);
		MAPI_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
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

	r.out.RowSet = &rowset;

	status = dcerpc_NspiGetMatches(nspi->rpc_connection, nspi->mem_ctx, &r);
	retval = r.out.result;
	MAPI_RETVAL_IF(!MAPI_STATUS_IS_OK(NT_STATUS_V(status)), MAPI_E_NOT_FOUND, NULL);
	
	return MAPI_E_SUCCESS;
}

/*
 * nspi_DNToEph
 * Request for an object giving its DN and retrieve its instance key
 * This function is used during MAPI profiles setup to request the
 * Exchange object in the Active Directory
 */

_PUBLIC_ enum MAPISTATUS nspi_DNToEph(struct nspi_context *nspi)
{
	struct NspiDNToEph	r;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	struct NAME_STRING	*server_dn;
	struct instance_key	*instance_key;

	MAPI_RETVAL_IF(!nspi, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!nspi->servername, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!nspi->org, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!nspi->org_unit, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi->handle;
	r.in.flag = 0;
	r.in.size = 1;

	server_dn = talloc(nspi->mem_ctx, struct NAME_STRING);
	server_dn->str = talloc_asprintf(nspi->mem_ctx, SERVER_DN, nspi->org,
					 nspi->org_unit, nspi->servername);

	r.in.server_dn = server_dn;

	instance_key = talloc(nspi->mem_ctx, struct instance_key);
	r.out.instance_key = instance_key;

	status = dcerpc_NspiDNToEph(nspi->rpc_connection, nspi->mem_ctx, &r);
	retval = r.out.result;
	MAPI_RETVAL_IF(!MAPI_STATUS_IS_OK(NT_STATUS_V(status)), retval, NULL);

	nspi->profile_instance_key = *(instance_key->value);

	return MAPI_E_SUCCESS;
}


/*
 * nspi_GetProps
 * request for properties associated with the given mapiuid (mapiuid
 * is computed with the instance key)
 */

_PUBLIC_ enum MAPISTATUS nspi_GetProps(struct nspi_context *nspi, 
				       struct SPropTagArray *SPropTagArray, 
				       struct SRowSet *rowset)
{
	struct NspiGetProps	r;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	struct SRow		*REPL_values;

	MAPI_RETVAL_IF(!nspi, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!nspi->profile_instance_key, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi->handle;
	r.in.flag = 0;

	r.in.settings = nspi->settings;
	memset(r.in.settings->service_provider.ab, 0, 16);
	r.in.settings->service_provider.ab[0] = nspi->profile_instance_key;
	r.in.settings->service_provider.ab[1] = (nspi->profile_instance_key & 0xFF00) >> 8;

 	r.in.REQ_properties = SPropTagArray;

	REPL_values = talloc(nspi->mem_ctx, struct SRow);
	r.out.REPL_values = &REPL_values;

	status = dcerpc_NspiGetProps(nspi->rpc_connection, nspi->mem_ctx, &r);
	retval = r.out.result;
	MAPI_RETVAL_IF(!MAPI_STATUS_IS_OK(NT_STATUS_V(status)), retval, NULL);

	rowset->cRows = 1;
	rowset->aRow = talloc(nspi->mem_ctx, struct SRow);
	rowset->aRow->ulAdrEntryPad = REPL_values->ulAdrEntryPad;
	rowset->aRow->cValues = REPL_values->cValues;
	rowset->aRow->lpProps = REPL_values->lpProps;
	
	return MAPI_E_SUCCESS;
}

/*
 * nspi_GetHierarchyInfo
 * Retrieve the Address Book container hierarchy
 */

_PUBLIC_ enum MAPISTATUS nspi_GetHierarchyInfo(struct nspi_context *nspi, 
					       struct SRowSet *rowset)
{
	struct NspiGetHierarchyInfo	r;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t       			unknown = 0;

	MAPI_RETVAL_IF(!nspi, MAPI_E_NOT_INITIALIZED, NULL);

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

	r.out.RowSet = &rowset;

	status = dcerpc_NspiGetHierarchyInfo(nspi->rpc_connection, nspi->mem_ctx, &r);
	retval = r.out.result;
	MAPI_RETVAL_IF(!MAPI_STATUS_IS_OK(NT_STATUS_V(status)), retval, NULL);

	return MAPI_E_SUCCESS;
}

/*
 * nspi_ResolveNames
 * query WAB (Windows Address Book) and try to resolve the provided
 * name and return a SRowSet with the requested property tags fetched
 * values.
 */

_PUBLIC_ enum MAPISTATUS nspi_ResolveNames(struct nspi_context *nspi, const char **usernames, 
					   struct SPropTagArray *props, struct SRowSet ***rowSet,
					   struct FlagList ***flaglist)
{
	TALLOC_CTX		*mem_ctx;
	struct NspiResolveNames r;
	struct names		*names;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		count;

	MAPI_RETVAL_IF(!nspi, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = nspi->mem_ctx;

	for (count = 0; usernames[count]; count++);

	r.in.handle = &nspi->handle;

	r.in.settings = nspi->settings;
	r.in.dwAlignPad = 0;
	r.in.SPropTagArray = props;
	r.in.SPropTagArray->cValues += 1;

	names = talloc(mem_ctx, struct names);
	names->cEntries = names->count = count;
	names->recipient = usernames;
	r.in.recipients = names;

	r.out.flags = *flaglist;

	status = dcerpc_NspiResolveNames(nspi->rpc_connection, mem_ctx, &r);
	retval = r.out.result;
	MAPI_RETVAL_IF(!MAPI_STATUS_IS_OK(NT_STATUS_V(status)), retval, names);

	rowSet[0][0] = r.out.RowSet;

	talloc_free(names);

	return MAPI_E_SUCCESS;
}

/*
 * nspi_ResolveNames
 * query WAB (Windows Address Book) and try to resolve the provided
 * name and return a SRowSet with the requested property tags fetched
 * values.
 */

_PUBLIC_ enum MAPISTATUS nspi_ResolveNamesW(struct nspi_context *nspi, const char **usernames, 
					    struct SPropTagArray *props, struct SRowSet ***rowSet,
					    struct FlagList ***flaglist)
{
	TALLOC_CTX		*mem_ctx;
	struct NspiResolveNamesW r;
	struct namesW		*names;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		count;

	MAPI_RETVAL_IF(!nspi, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = nspi->mem_ctx;

	for (count = 0; usernames[count]; count++);

	r.in.handle = &nspi->handle;

	r.in.settings = nspi->settings;
	r.in.dwAlignPad = 0;
	r.in.SPropTagArray = props;
	r.in.SPropTagArray->cValues += 1;

	names = talloc(mem_ctx, struct namesW);
	names->cEntries = names->count = count;
	names->recipient = usernames;
	r.in.recipients = names;

	r.out.flags = *flaglist;

	status = dcerpc_NspiResolveNamesW(nspi->rpc_connection, mem_ctx, &r);
	retval = r.out.result;
	MAPI_RETVAL_IF(!MAPI_STATUS_IS_OK(NT_STATUS_V(status)), retval, names);

	rowSet[0][0] = r.out.RowSet;

	talloc_free(names);

	return MAPI_E_SUCCESS;
}
