/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2008.

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

#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"


/**
   \file IABContainer.c

   \brief Provides access to address book containers -- Used to
   perform name resolution
*/


/**
   \details Resolve user names against the Windows Address Book Provider

   \param session pointer to the MAPI session context
   \param usernames list of user names to resolve
   \param rowset resulting list of user details
   \param props resulting list of resolved names
   \param flaglist resulting list of resolution status (see below)
   \param flags if set to MAPI_UNICODE then UNICODE MAPITAGS can be
   used, otherwise only UTF8 encoded fields may be returned.

   Possible flaglist values are:
   -# MAPI_UNRESOLVED: could not be resolved
   -# MAPI_AMBIGUOUS: resolution match more than one entry
   -# MAPI_RESOLVED: resolution matched a single entry
 
   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
   
   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   -# MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   -# MAPI_E_SESSION_LIMIT: No session has been opened on the provider
   -# MAPI_E_NOT_ENOUGH_RESOURCES: MAPI subsystem failed to allocate
     the necessary resources to operate properly
   -# MAPI_E_NOT_FOUND: No suitable profile database was found in the
     path pointed by profiledb
   -# MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

     It is the developer responsibility to call MAPIFreeBuffer on
     rowset and flaglist once they have finished to use them.
   
   \sa MAPILogonProvider, GetLastError
 */
_PUBLIC_ enum MAPISTATUS ResolveNames(struct mapi_session *session,
				      const char **usernames, 
				      struct SPropTagArray *props, 
				      struct PropertyRowSet_r **rowset, 
				      struct PropertyTagArray_r **flaglist, 
				      uint32_t flags)
{
	TALLOC_CTX		*mem_ctx;
	struct nspi_context	*nspi;
	enum MAPISTATUS		retval;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_SESSION_LIMIT, NULL);
	OPENCHANGE_RETVAL_IF(!session->nspi, MAPI_E_SESSION_LIMIT, NULL);
	OPENCHANGE_RETVAL_IF(!session->nspi->ctx, MAPI_E_SESSION_LIMIT, NULL);
	OPENCHANGE_RETVAL_IF(!rowset, MAPI_E_INVALID_PARAMETER, NULL);

	nspi = (struct nspi_context *)session->nspi->ctx;
	mem_ctx = talloc_named(session, 0, "ResolveNames");

	switch (flags) {
	case MAPI_UNICODE:
		retval = nspi_ResolveNamesW(nspi, mem_ctx, usernames, props, &rowset, &flaglist);
		break;
	default:
		retval = nspi_ResolveNames(nspi, mem_ctx, usernames, props, &rowset, &flaglist);
		break;
	}

	*rowset = talloc_steal(nspi->mem_ctx, *rowset);
	*flaglist = talloc_steal(nspi->mem_ctx, *flaglist);

	talloc_free(mem_ctx);

	if (retval != MAPI_E_SUCCESS) return retval;

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the global address list
   
   The Global Address List is the full list of email addresses (and other
   account-type things, such as "rooms" and distribution lists) accessible
   on the server. A user will usually have access to both a personal
   address book, and to the Global Address List. Public Address Book is
   another name for Global Address List.
   
   You access the Global Address List by setting the list of things that
   you want to retrieve from the Global Address List as property names
   in the SPropTagArray argument, and then calling this function. The 
   results are returned in SRowSet.
   
   You can get a convenient output of the results using mapidump_PAB_entry()
   for each row returned.
   
   \param session pointer to the MAPI session context
   \param SPropTagArray pointer to an array of MAPI properties we want
   to fetch
   \param rowsetp pointer to the rows of the table returned
   \param count the number of rows we want to fetch
   \param ulFlags specify the table cursor location

   Possible value for ulFlags:
   -# TABLE_START: Fetch rows from the beginning of the table
   -# TABLE_CUR: Fetch rows from current table location
   
   The Global Address List may be quite large (tens of thousands of
   entries in a large deployment), so you usually call this function
   with ulFlags set to TABLE_START the first time, and then subsequent
   calls will be made with TABLE_CUR to progress through the table.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   -# MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   -# MAPI_E_SESSION_LIMIT: No session has been opened on the provider
   -# MAPI_E_INVALID_PARAMETER: if a function parameter is invalid
   -# MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa MapiLogonEx, MapiLogonProvider, mapidump_PAB_entry
 */
_PUBLIC_ enum MAPISTATUS GetGALTable(struct mapi_session *session,
				     struct SPropTagArray *SPropTagArray, 
				     struct PropertyRowSet_r **rowsetp,
				     uint32_t count,
				     uint8_t ulFlags)
{
	TALLOC_CTX		*mem_ctx;
	struct nspi_context	*nspi;
	struct PropertyRowSet_r	*rowset;
	enum MAPISTATUS		retval;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_SESSION_LIMIT, NULL);
	OPENCHANGE_RETVAL_IF(!session->nspi, MAPI_E_SESSION_LIMIT, NULL);
	OPENCHANGE_RETVAL_IF(!session->nspi->ctx, MAPI_E_SESSION_LIMIT, NULL);
	OPENCHANGE_RETVAL_IF(!rowsetp, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(session, 0, "GetGALTable");
	nspi = (struct nspi_context *)session->nspi->ctx;

	if (ulFlags == TABLE_START) {
		nspi->pStat->CurrentRec = 0;
		nspi->pStat->Delta = 0;
		nspi->pStat->NumPos = 0;
		nspi->pStat->TotalRecs = 0xffffffff;
	}

	rowset = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	retval = nspi_QueryRows(nspi, mem_ctx, SPropTagArray, NULL, count, &rowset);
	rowset = talloc_steal(session, rowset);
	*rowsetp = rowset;

	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Retrieve the total number of records in the global address
   list

   The Global Address List is the full list of email addresses (and other
   account-type things, such as "rooms" and distribution lists) accessible
   on the server. A user will usually have access to both a personal
   address book, and to the Global Address List. Public Address Book is
   another name for Global Address List.

   \param session pointer to the MAPI session context
   \param totalRecs pointers to the total number of records in the
   global address list returned

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   -# MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   -# MAPI_E_SESSION_LIMIT: No session has been opened on the provider
   -# MAPI_E_INVALID_PARAMETER: if a function parameter is invalid
   -# MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
 */
_PUBLIC_ enum MAPISTATUS GetGALTableCount(struct mapi_session *session,
					  uint32_t *totalRecs)
{
	TALLOC_CTX		*mem_ctx;
	struct nspi_context	*nspi;
	enum MAPISTATUS		retval;
	struct PropertyRowSet_r	*rowset;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_SESSION_LIMIT, NULL);
	OPENCHANGE_RETVAL_IF(!session->nspi, MAPI_E_SESSION_LIMIT, NULL);
	OPENCHANGE_RETVAL_IF(!session->nspi->ctx, MAPI_E_SESSION_LIMIT, NULL);

	mem_ctx = talloc_named(session, 0, "GetGALTableCount");
	nspi = (struct nspi_context *) session->nspi->ctx;

	nspi->pStat->CurrentRec = 0;
	nspi->pStat->Delta = 0;
	nspi->pStat->NumPos = 0;
	nspi->pStat->TotalRecs = 0xffffffff;

	rowset = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	retval = nspi_QueryRows(nspi, mem_ctx, NULL, NULL, 0, &rowset);

	*totalRecs = nspi->pStat->TotalRecs;
	
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}

/**
   \details Retrieve Address Book information for a given recipient

   \param session pointer to the MAPI session context
   \param username pointer to the username to retrieve information from
   \param pPropTags pointer to the property tags array to lookup
   \param ppRowSet pointer on pointer to the results

   Note that if pPropTags is NULL, then GetABNameInfo will fetch
   the following default property tags:
   -# PR_ADDRTYPE_UNICODE
   -# PR_EMAIL_ADDRESS_UNICODE
   -# PR_DISPLAY_NAME_UNICODE
   -# PR_OBJECT_TYPE

   \return MAPI_E_SUCCESS on success, otherwise MAPI error. 

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   -# MAPI_E_NOT_INITIALIZED if MAPI subsystem is not initialized
   -# MAPI_E_SESSION_LIMIT if the NSPI session is unavailable
   -# MAPI_E_INVALID_PARAMETER if a function parameter is invalid
   -# MAPI_E_NOT_FOUND if the username to lookup doesn't match any
      records

   \sa nspi_DNToMId, nspi_GetProps
 */
_PUBLIC_ enum MAPISTATUS GetABRecipientInfo(struct mapi_session *session,
				       const char *username,
				       struct SPropTagArray *pPropTags,
				       struct PropertyRowSet_r **ppRowSet)
{
	enum MAPISTATUS			retval;
	TALLOC_CTX			*mem_ctx;
	struct nspi_context		*nspi_ctx;
	struct PropertyRowSet_r		*RowSet;
	struct SPropTagArray		*SPropTagArray = NULL;
	struct PropertyTagArray_r	*pMId = NULL;
	struct PropertyTagArray_r	*flaglist = NULL;
	struct StringsArray_r		pNames;
	const char			*usernames[2];
	char				*email = NULL;
	bool				allocated = false;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_SESSION_LIMIT, NULL);
	OPENCHANGE_RETVAL_IF(!session->profile, MAPI_E_SESSION_LIMIT, NULL);
	OPENCHANGE_RETVAL_IF(!session->nspi, MAPI_E_SESSION_LIMIT, NULL);
	OPENCHANGE_RETVAL_IF(!session->nspi->ctx, MAPI_E_SESSION_LIMIT, NULL);
	OPENCHANGE_RETVAL_IF(!ppRowSet, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(session, 0, "GetABRecipientInfo");
	nspi_ctx = (struct nspi_context *)session->nspi->ctx;

	/* Step 1. Resolve the username */
	usernames[0] = username;
	usernames[1] = NULL;

	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	SPropTagArray = set_SPropTagArray(mem_ctx, 0xc,
					  PR_ENTRYID,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_ADDRTYPE_UNICODE,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_EMAIL_ADDRESS_UNICODE,
					  PR_SEND_INTERNET_ENCODING,
					  PR_SEND_RICH_INFO,
					  PR_SEARCH_KEY,
					  PR_TRANSMITTABLE_DISPLAY_NAME_UNICODE,
					  PR_7BIT_DISPLAY_NAME_UNICODE,
					  PR_SMTP_ADDRESS_UNICODE);
	retval = ResolveNames(session, usernames, SPropTagArray, &RowSet, &flaglist, MAPI_UNICODE);
	MAPIFreeBuffer(SPropTagArray);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	OPENCHANGE_RETVAL_IF((flaglist->aulPropTag[0] != MAPI_RESOLVED), MAPI_E_NOT_FOUND, mem_ctx);

	email = talloc_strdup(mem_ctx, (const char *) get_PropertyValue_PropertyRowSet_data(RowSet, PR_EMAIL_ADDRESS_UNICODE));
	MAPIFreeBuffer(RowSet);

	/* Step 2. Map recipient DN to MId */
	pNames.Count = 0x1;
	pNames.Strings = (const char **) talloc_array(mem_ctx, char *, 1);
	pNames.Strings[0] = email;
	pMId = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	retval = nspi_DNToMId(nspi_ctx, mem_ctx, &pNames, &pMId);
	MAPIFreeBuffer((char *)pNames.Strings[0]);
	MAPIFreeBuffer((char **)pNames.Strings);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Step 3. Get recipient's properties */
	if (!pPropTags) {
		allocated = true;
		SPropTagArray = set_SPropTagArray(mem_ctx, 0x4,
						  PR_ADDRTYPE_UNICODE,
						  PR_EMAIL_ADDRESS_UNICODE,
						  PR_DISPLAY_NAME_UNICODE,
						  PR_OBJECT_TYPE);
	} else {
		SPropTagArray = pPropTags;
	}

	RowSet = talloc_zero(mem_ctx, struct PropertyRowSet_r);
	retval = nspi_GetProps(nspi_ctx, RowSet, SPropTagArray, pMId, &RowSet);
	if (allocated == true) {
		MAPIFreeBuffer(SPropTagArray);
	}
	MAPIFreeBuffer(pMId);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	RowSet = talloc_steal((TALLOC_CTX *)session, RowSet);
	*ppRowSet = RowSet;

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
