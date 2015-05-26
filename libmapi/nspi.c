/*
   OpenChange NSPI implementation.

   Copyright (C) Julien Kerihuel 2005 - 2011.

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
#include "gen_ndr/ndr_exchange_c.h"
#include "gen_ndr/ndr_exchange.h"
#include <param.h>


/**
   \file nspi.c

   \brief Name Service Provider (NSPI) stack functions
 */

/**
   \details Dump the STAT structure
   
   \param name the name of STAT structure member
   \param pStat pointer on the STAT structure to dump
 */
_PUBLIC_ void nspi_dump_STAT(const char *name, struct STAT *pStat)
{
	struct ndr_print	ndr_print;
	
	ndr_print.depth = 1;
	ndr_print.print = ndr_print_debug_helper;
	ndr_print.no_newline = false;
	ndr_print_STAT(&ndr_print, name, pStat);
}

/**
   \details Initialize the STAT structure and set common STAT parameters

   \param mem_ctx pointer to the memory context
   \param CodePage the CodePage value to set in the STAT structure
   \param TemplateLocale the Locale for the STAT TemplateLocale parameter
   \param SortLocale the Locale for the STAT SortLocal parameter
 */
static struct STAT *nspi_set_STAT(TALLOC_CTX *mem_ctx, 
				  uint32_t CodePage,
				  uint32_t TemplateLocale, 
				  uint32_t SortLocale)
{
	struct STAT		*pStat;

	/* Sanity Checks */
	if (!CodePage || !TemplateLocale || !SortLocale) {
		return NULL;
	}

	pStat = talloc_zero(mem_ctx, struct STAT);
	pStat->SortType = SortTypeDisplayName;
	pStat->CodePage = CodePage;
	pStat->TemplateLocale = TemplateLocale;
	pStat->SortLocale = SortLocale;

	return pStat;
}


/**
   \details Initiates a session between a client and the NSPI server.

   \param parent_ctx pointer to the memory context
   \param p pointer to the DCERPC pipe
   \param cred pointer to the user credentials
   \param codepage the code to set in the STAT structure
   \param language the language to set in the STAT structure
   \param method the method to set in the STAT structure

   \return Allocated pointer to a nspi_context structure on success,
   otherwise NULL
 */
_PUBLIC_ struct nspi_context *nspi_bind(TALLOC_CTX *parent_ctx, 
					struct dcerpc_pipe *p,
					struct cli_credentials *cred, 
					uint32_t codepage,
					uint32_t language, 
					uint32_t method)
{
	TALLOC_CTX		*mem_ctx;
	struct NspiBind		r;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	struct nspi_context	*ret;
	struct GUID		guid;

	/* Sanity checks */
	if (!p) return NULL;
	if (!cred) return NULL;

	ret = talloc(parent_ctx, struct nspi_context);
	ret->rpc_connection = p;
	ret->mem_ctx = parent_ctx;
	ret->cred = cred;
	ret->version = 0;

	/* Sanity Checks */
	if (!(ret->pStat = nspi_set_STAT((TALLOC_CTX *) ret, codepage, language, method))) {
		talloc_free(ret);
		return NULL;
	}

	mem_ctx = talloc_named(parent_ctx, 0, __FUNCTION__);

	r.in.dwFlags = 0;

	r.in.pStat = ret->pStat;
	r.in.pStat->ContainerID = 0x0;

	r.in.mapiuid = talloc(mem_ctx, struct GUID);
	memset(r.in.mapiuid, 0, sizeof(struct GUID));
	
	r.out.mapiuid = &guid;

	r.in.mapiuid = talloc(mem_ctx, struct GUID);
	memset(r.in.mapiuid, 0, sizeof(struct GUID));

	r.out.handle = &ret->handle;

	status = dcerpc_NspiBind_r(p->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	if ((!NT_STATUS_IS_OK(status)) || (retval != MAPI_E_SUCCESS)) {
		talloc_free(ret);
		talloc_free(mem_ctx);
		return NULL;
	}
	
	talloc_free(mem_ctx);
	return ret;
}


/**
   \details Destructor for the NSPI context. Call the NspiUnbind
   function.
   
   \param data generic pointer to data with mapi_provider information

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
 */

int nspi_disconnect_dtor(void *data)
{
	enum MAPISTATUS		retval;
	struct mapi_provider	*provider = (struct mapi_provider *) data;

	retval = nspi_unbind((struct nspi_context *)provider->ctx);
	return retval;
}


/**
   \details Destroys the context handle

   \param nspi_ctx pointer to the NSPI connection context

   \return return 1 on success or 2 if the input context is NULL
 */
_PUBLIC_ enum MAPISTATUS nspi_unbind(struct nspi_context *nspi_ctx)
{
	struct NspiUnbind	r;
	NTSTATUS		status;
	enum MAPISTATUS		retval;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	r.in.handle = r.out.handle = &nspi_ctx->handle;
	r.in.Reserved = 0;

	status = dcerpc_NspiUnbind_r(nspi_ctx->rpc_connection->binding_handle, nspi_ctx->mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF((retval != 1) && !NT_STATUS_IS_OK(status), retval, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Updates the STAT block representing position in a table to
   reflect positioning changes requested by the client.

   \param nspi_ctx pointer to the NSPI connection context
   \param mem_ctx pointer to the memory context
   \param plDelta pointer to an unsigned long indicating movement
   within the address book container specified by the input parameter
   pStat.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS nspi_UpdateStat(struct nspi_context *nspi_ctx, 
					 TALLOC_CTX *mem_ctx,
					 int32_t *plDelta)
{
	struct NspiUpdateStat		r;
	NTSTATUS			status;
	enum MAPISTATUS			retval;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!plDelta, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;
	r.in.Reserved = 0x0;

	r.in.pStat = nspi_ctx->pStat;
	r.in.plDelta = plDelta;

	r.out.pStat = nspi_ctx->pStat;
	r.out.plDelta = r.in.plDelta;

	status = dcerpc_NspiUpdateStat_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Returns a number of Rows from a specified table.

   \param nspi_ctx pointer to the NSPI connection context
   \param mem_ctx pointer to the memory context
   \param pPropTags pointer to the list of proptags that the client
   requires to be returned for each row.
   \param MIds pointer to a list of values representing an Explicit
   table
   \param count the number of rows requested
   \param ppRows pointer on pointer to the the rows returned by the
   server

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS nspi_QueryRows(struct nspi_context *nspi_ctx, 
					TALLOC_CTX *mem_ctx,
					struct SPropTagArray *pPropTags,
					struct PropertyTagArray_r *MIds, 
					uint32_t count,
					struct PropertyRowSet_r **ppRows)
{
	struct NspiQueryRows		r;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	struct STAT			*pStat;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;
	r.in.dwFlags = 0x0;
	r.in.pStat = nspi_ctx->pStat;

	if (MIds && MIds->cValues) {
		r.in.dwETableCount = MIds->cValues;
		r.in.lpETable = (uint32_t *) MIds->aulPropTag;
		/* We set CurrentRec to the first entry */
		r.in.pStat->CurrentRec = MIds->aulPropTag[0];
	} else {
		r.in.dwETableCount = 0;
		r.in.lpETable = NULL;
	}

	r.in.Count = count;	
 	r.in.pPropTags = pPropTags;

	pStat = talloc(mem_ctx, struct STAT);
	r.out.pStat = pStat;

	r.out.ppRows = ppRows;

	status = dcerpc_NspiQueryRows_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	nspi_ctx->pStat->CurrentRec = r.out.pStat->CurrentRec;
	nspi_ctx->pStat->Delta = r.out.pStat->Delta;
	nspi_ctx->pStat->NumPos = r.out.pStat->NumPos;
	nspi_ctx->pStat->TotalRecs = r.out.pStat->TotalRecs;

	return MAPI_E_SUCCESS;

}


/**
   \details Searches for and sets the logical position in a specific
   table to the first entry greater than or equal to a specified
   value. Optionally, it might also return information about rows in
   the table.

   \param nspi_ctx pointer to the NSPI connection context
   \param mem_ctx pointer to the memory context
   \param SortType the table sort order to use
   \param pTarget PropertyValue_r struct holding the value being sought
   \param pPropTags pointer to an array of property tags of columns
   that the client wants to be returned for each row returned.
   \param pMIds pointer to a list of Mid that comprise a restricted
   address book container
   \param pRows pointer to pointer to a SRowSet structure holding the
   rows returned by the server

   SortType can take the following values:
   -# SortTypeDisplayName
   -# SortTypePhoneticDisplayName

   If pTarget property tag is not set accordingly to SortType, the
   function returns MAPI_E_INVALID_PARAMETER. Possible values are:
   -# SortType set to SortTypeDisplayName and pTarget property tag set
      to PR_DISPLAY_NAME or PR_DISPLAY_UNICODE
   -# SortType set to SortTypePhoneticDisplayName and pTarget property
      tag set to PR_EMS_AB_PHONETIC_DISPLAY_NAME or
      PR_EMS_AB_PHONETIC_DISPLAY_NAME_UNICODE

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS nspi_SeekEntries(struct nspi_context *nspi_ctx,
					  TALLOC_CTX *mem_ctx,
					  enum TableSortOrders SortType,
					  struct PropertyValue_r *pTarget,
					  struct SPropTagArray *pPropTags,
					  struct PropertyTagArray_r *pMIds,
					  struct PropertyRowSet_r **pRows)
{
	struct NspiSeekEntries		r;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	struct STAT			*pStat;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!pTarget, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!pRows, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(((SortType != SortTypeDisplayName) 
			&& (SortType != SortTypePhoneticDisplayName)), 
		       MAPI_E_INVALID_PARAMETER, NULL);

	/* Sanity Checks on SortType and pTarget combination */
	OPENCHANGE_RETVAL_IF(((SortType == SortTypeDisplayName) && 
			(pTarget->ulPropTag != PR_DISPLAY_NAME) && 
			(pTarget->ulPropTag != PR_DISPLAY_NAME_UNICODE)),
		       MAPI_E_INVALID_PARAMETER, NULL);

	OPENCHANGE_RETVAL_IF(((SortType == SortTypePhoneticDisplayName) && 
			(pTarget->ulPropTag != PR_EMS_AB_PHONETIC_DISPLAY_NAME) &&
			(pTarget->ulPropTag != PR_EMS_AB_PHONETIC_DISPLAY_NAME_UNICODE)),
		       MAPI_E_INVALID_PARAMETER, NULL);


	r.in.handle = &nspi_ctx->handle;
	r.in.Reserved = 0x0;
	r.in.pStat = nspi_ctx->pStat;
	r.in.pStat->SortType = SortType;
	r.in.pTarget = pTarget;

	if (pMIds && pMIds->cValues) {
		r.in.lpETable = pMIds;
	} else {
		r.in.lpETable = NULL;
	}

	r.in.pPropTags = pPropTags;

	r.out.pRows = pRows;

	pStat = talloc(mem_ctx, struct STAT);
	r.out.pStat = pStat;

	status = dcerpc_NspiSeekEntries_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, pStat);
	OPENCHANGE_RETVAL_IF(retval, retval, pStat);

	nspi_ctx->pStat->CurrentRec = r.out.pStat->CurrentRec;
	nspi_ctx->pStat->Delta = r.out.pStat->Delta;
	nspi_ctx->pStat->NumPos = r.out.pStat->NumPos;
	nspi_ctx->pStat->TotalRecs = r.out.pStat->TotalRecs;

	return MAPI_E_SUCCESS;
}


/**
   \details Returns an explicit table.

   \param nspi_ctx pointer to the NSPI connection context
   \param mem_ctx pointer to the memory context
   \param pPropTags pointer to an array of property tags of columns
   \param Filter pointer to the Restriction to apply to the table
   \param ulRequested The upper limit for returned rows
   \param ppRows pointer to pointer to a SRowSet structure holding the
   rows returned by the server
   \param ppOutMIds pointer to pointer to a list of MId that comprise
   a restricted address book container

   \return MAPI_E_SUCCESS on success, otherwise MAPI error. If the resulting
   table rows count will be greater than ulRequested, then an error
   MAPI_E_TABLE_TOO_BIG is returned. Note, this error can be also returned
   when server limits for table size are exceeded.
 */
_PUBLIC_ enum MAPISTATUS nspi_GetMatches(struct nspi_context *nspi_ctx, 
					 TALLOC_CTX *mem_ctx,
					 struct SPropTagArray *pPropTags,
					 struct Restriction_r *Filter,
					 uint32_t ulRequested,
					 struct PropertyRowSet_r **ppRows,
					 struct PropertyTagArray_r **ppOutMIds)
{
	struct NspiGetMatches		r;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	struct STAT			*pStat;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ppRows, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ppOutMIds, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;
	r.in.Reserved1 = 0;
	
	r.in.pStat = nspi_ctx->pStat;
	r.in.pStat->ContainerID = 0x0;
	r.in.pStat->CurrentRec = 0x0;
	r.in.pStat->Delta = 0x0;
	r.in.pStat->NumPos = 0x0;
	r.in.pStat->TotalRecs = 0x0;

	r.in.pReserved = NULL;
	r.in.Reserved2 = 0;
	r.in.Filter = Filter;
	r.in.lpPropName = NULL;
	r.in.ulRequested = ulRequested;
	r.in.pPropTags = pPropTags;

	pStat = talloc(mem_ctx, struct STAT);
	r.out.pStat = pStat;
	r.out.ppOutMIds = ppOutMIds;
	r.out.ppRows = ppRows;

	status = dcerpc_NspiGetMatches_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_NOT_FOUND, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Applies a sort order to the objects in a restricted
   address book container

   \param nspi_ctx pointer to the NSPI connection context
   \param mem_ctx pointer to the memory context
   \param SortType the table sort order to use
   \param pInMIds pointer on a list of MIds that comprise a
   restricted address book container
   \param ppMIds pointer on pointer to the returned list of MIds that
   comprise a restricted address book container.

   SortType can take the following values:
   -# SortTypeDisplayName
   -# SortTypePhoneticDisplayName

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS nspi_ResortRestriction(struct nspi_context *nspi_ctx,
						TALLOC_CTX *mem_ctx,
						enum TableSortOrders SortType,
						struct PropertyTagArray_r *pInMIds,
						struct PropertyTagArray_r **ppMIds)
{
	struct NspiResortRestriction	r;
	enum MAPISTATUS			retval;
	NTSTATUS			status;
	struct PropertyTagArray_r		*ppInMIds = NULL;
	struct STAT			*pStat = NULL;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!pInMIds, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ppMIds, MAPI_E_INVALID_PARAMETER, NULL);

	/* Sanity check on SortType */
	OPENCHANGE_RETVAL_IF(((SortType != SortTypeDisplayName) && (SortType != SortTypePhoneticDisplayName)),
		       MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;
	r.in.Reserved = 0;
	r.in.pStat = nspi_ctx->pStat;
	r.in.pStat->SortType = SortType;
	r.in.pInMIds = pInMIds;
	r.in.ppMIds = &ppInMIds;

	pStat = talloc_zero(mem_ctx, struct STAT);
	r.out.pStat = pStat;
	r.out.ppMIds = ppMIds;

	status = dcerpc_NspiResortRestriction_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Maps a set of DN to a set of MId

   \param nspi_ctx pointer to the NSPI connection context
   \param mem_ctx pointer to the memory context
   \param pNames pointer to a StringsArray_r structure with the DN to
   map
   \param ppMIds pointer on pointer to the returned list of MIds

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS nspi_DNToMId(struct nspi_context *nspi_ctx, 
				      TALLOC_CTX *mem_ctx,
				      struct StringsArray_r *pNames,
				      struct PropertyTagArray_r **ppMIds)
{
	struct NspiDNToMId	r;
	NTSTATUS		status;
	enum MAPISTATUS		retval;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!pNames, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!pNames->Count, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ppMIds, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;
	r.in.Reserved = 0;
	r.in.pNames = pNames;

	r.out.ppMIds = ppMIds;

	status = dcerpc_NspiDNToMId_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL)

	return MAPI_E_SUCCESS;
}


/**
   \details Returns a list of all the properties that have values on
   the specified object

   \param nspi_ctx pointer to the NSPI connection context
   \param mem_ctx pointer to the memory context
   \param WantObject boolean value defining whether we want the server
   to include properties with the type set to PT_OBJECT
   \param dwMId the MId of the specified object
   \param ppPropTags pointer on pointer to the list of property tags
   associated to the object.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
 */
_PUBLIC_ enum MAPISTATUS nspi_GetPropList(struct nspi_context *nspi_ctx,
					  TALLOC_CTX *mem_ctx,
					  bool WantObject,
					  uint32_t dwMId,
					  struct SPropTagArray **ppPropTags)
{
	struct NspiGetPropList	r;
	NTSTATUS		status;
	enum MAPISTATUS		retval;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ppPropTags, MAPI_E_INVALID_PARAMETER, NULL);
	
	r.in.handle = &nspi_ctx->handle;
	r.in.dwFlags = (WantObject == true) ? 0x0 : fSkipObjects;
	r.in.dwMId = dwMId;
	r.in.CodePage = nspi_ctx->pStat->CodePage;
	
	r.out.ppPropTags = ppPropTags;

	status = dcerpc_NspiGetPropList_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}



/**
   \details Returns an address book row containing a set of the
   properties and values that exists on an object

   \param nspi_ctx pointer to the NSPI connection context
   \param mem_ctx pointer to the memory context
   \param pPropTags pointer to the list of property tags that the
   client wants to be returned
   \param MId pointer to the MId of the record
   \param SRowSet pointer on pointer to the row returned by the server

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS nspi_GetProps(struct nspi_context *nspi_ctx, 
				       TALLOC_CTX *mem_ctx,
				       struct SPropTagArray *pPropTags, 
				       struct PropertyTagArray_r *MId,
				       struct PropertyRowSet_r **SRowSet)

{
	struct NspiGetProps	r;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	struct PropertyRow_r	*ppRows;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!MId, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!MId->cValues, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;
	r.in.dwFlags = 0;

	r.in.pStat = nspi_ctx->pStat;
	r.in.pStat->CurrentRec = MId->aulPropTag[0];
	r.in.pStat->Delta = 0x0;
	r.in.pStat->NumPos = 0x0;
	r.in.pStat->TotalRecs = 0x0;

 	r.in.pPropTags = pPropTags;

	ppRows = talloc(mem_ctx, struct PropertyRow_r);
	r.out.ppRows = &ppRows;

	status = dcerpc_NspiGetProps_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL)

	SRowSet[0]->cRows = 1;
	SRowSet[0]->aRow = talloc(mem_ctx, struct PropertyRow_r);
	SRowSet[0]->aRow->Reserved = ppRows->Reserved;
	SRowSet[0]->aRow->cValues = ppRows->cValues;
	SRowSet[0]->aRow->lpProps = ppRows->lpProps;
	
	return MAPI_E_SUCCESS;
}


/**
   \details Compares the position in an address book container of two
   objects identified by MId and returns the value of the comparison

   \param nspi_ctx pointer to the NSPI connection context
   \param mem_ctx pointer to the memory context
   \param MId1 the first MId to compare
   \param MId2 the second MId to compare
   \param plResult pointer to the value of the comparison

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
 */
_PUBLIC_ enum MAPISTATUS nspi_CompareMIds(struct nspi_context *nspi_ctx,
					  TALLOC_CTX *mem_ctx,
					  uint32_t MId1, uint32_t MId2,
					  uint32_t *plResult)
{
	struct NspiCompareMIds	r;
	NTSTATUS		status;
	enum MAPISTATUS		retval;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!plResult, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;
	r.in.Reserved = 0x0;
	r.in.pStat = nspi_ctx->pStat;
	r.in.MId1 = MId1;
	r.in.MId2 = MId2;

	r.out.plResult = plResult;

	status = dcerpc_NspiCompareMIds_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Modify the properties of an object in the address book
   
   \param nspi_ctx pointer to the NSPI connection context
   \param mem_ctx pointer to the memory context
   \param MId the MId of the address book object
   \param pPropTags pointer to the list of properties to be modified
   on the object
   \param pRow Contains an address book row

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS nspi_ModProps(struct nspi_context *nspi_ctx,
				       TALLOC_CTX *mem_ctx,
				       uint32_t MId,
				       struct SPropTagArray *pPropTags,
				       struct PropertyRow_r *pRow)
{
	struct NspiModProps	r;
	NTSTATUS		status;
	enum MAPISTATUS		retval;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!pPropTags, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!pRow, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;
	r.in.Reserved = 0x0;
	r.in.pStat = nspi_ctx->pStat;

	if (MId) {
		r.in.pStat->CurrentRec = MId;
	}

	r.in.pPropTags = pPropTags;
	r.in.pRow = pRow;

	status = dcerpc_NspiModProps_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}



/**
   \details Returns the rows of a special table to the client. The
   special table can be a Hierarchy Table or an Address Creation Table

   \param nspi_ctx pointer to the NSPI connection context
   \param mem_ctx pointer to the memory context
   \param Type bitmap of flags defining the type of the special table
   \param ppRows pointer on pointer to the rows returned by the server

   Possible values for Type:
   -# NspiAddressCreationTemplates to access an Address Creation Table
   -# NspiUnicodeStrings for strings to be returned in Unicode

   If NspiAddressCreationTemplates is not set, then
   NspiGetSpecialTable will automatically fetch the Hierarchy Table.

   If NspiAddressCreationTemplates is set, then NspiUnicodeStrings is
   ignored.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
 */
_PUBLIC_ enum MAPISTATUS nspi_GetSpecialTable(struct nspi_context *nspi_ctx, 
					      TALLOC_CTX *mem_ctx,
					      uint32_t Type,
					      struct PropertyRowSet_r **ppRows)
{
	struct NspiGetSpecialTable	r;
	NTSTATUS			status;
	enum MAPISTATUS			retval;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(((Type != 0x0) && (Type != 0x2) && (Type != 0x4)),
		       MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ppRows, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;
	r.in.dwFlags = Type;

	r.in.pStat = nspi_ctx->pStat;
	r.in.lpVersion = &nspi_ctx->version;

	r.out.lpVersion = &nspi_ctx->version;
	r.out.ppRows = ppRows;

	status = dcerpc_NspiGetSpecialTable_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Returns information about template objects in the address
   book.

   \param nspi_ctx pointer to the NSPI memory context
   \param mem_ctx pointer to the memory context
   \param dwFlags set of bit flags
   \param ulType specifies the display type of the template
   \param pDN the DN of the template requested
   \param ppData pointer on pointer to the data requested

   Possible values for dwFlags:
   -# TI_TEMPLATE to return the template
   -# TI_SCRIPT to return the script associated to the template
   -# TI_EMT to return the e-mail type associated to the template
   -# TI_HELPFILE_NAME to return the help file associated to the
      template
   -# TI_HELPFILE_CONTENTS to return the contents of the help file
      associated to the template
   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
 */
_PUBLIC_ enum MAPISTATUS nspi_GetTemplateInfo(struct nspi_context *nspi_ctx,
					      TALLOC_CTX *mem_ctx,
					      uint32_t dwFlags,
					      uint32_t ulType,
					      char *pDN,
					      struct PropertyRow_r **ppData)
{
	struct NspiGetTemplateInfo	r;
	NTSTATUS			status;
	enum MAPISTATUS			retval;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ppData, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;
	r.in.dwFlags = dwFlags;
	r.in.ulType = ulType;
	r.in.pDN = pDN;
	r.in.dwCodePage = nspi_ctx->pStat->CodePage;
	r.in.dwLocaleID = nspi_ctx->pStat->TemplateLocale;
	
	r.out.ppData = ppData;

	status = dcerpc_NspiGetTemplateInfo_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);
	

	return MAPI_E_SUCCESS;
}


/**
   \details Modifies the values of a specific property of a specific
   row in the address book. This function only applies only to rows
   that support the PT_OBJECT Property Type.

   \param nspi_ctx pointer to the NSPI connection context
   \param Delete boolean value defining whether the server must remove
   all values specified by the input parameter lpEntryIDs from the
   property specified by ulPropTag
   \param ulPropTag property tag of the property the client wishes to
   modify
   \param MId the MId of the address book object
   \param lpEntryIds array of BinaryArray_r structures intended to be
   modified or deleted

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS nspi_ModLinkAtt(struct nspi_context *nspi_ctx,
					 bool Delete,
					 uint32_t ulPropTag,
					 uint32_t MId,
					 struct BinaryArray_r *lpEntryIds)
{
	struct NspiModLinkAtt	r;
	NTSTATUS		status;
	enum MAPISTATUS		retval;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(((ulPropTag & 0xFFFF) != PT_OBJECT), MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!lpEntryIds, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!lpEntryIds->cValues, MAPI_E_INVALID_PARAMETER, NULL)

	r.in.handle = &nspi_ctx->handle;
	/* FIXME: need to find fDelete value first */
	r.in.dwFlags = (Delete == true) ? 0x1 : 0x0; 
	r.in.ulPropTag = ulPropTag;
	r.in.MId = MId;
	r.in.lpEntryIds = lpEntryIds;

	status = dcerpc_NspiModLinkAtt_r(nspi_ctx->rpc_connection->binding_handle, nspi_ctx->mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Returns a list of all the properties the NSPI server is
   aware off.

   \param nspi_ctx pointer to the NSPI connection context
   \param mem_ctx pointer to the memory context
   \param WantUnicode whether we want UNICODE properties or not
   \param ppColumns pointer on pointer to a property tag array

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
 */
_PUBLIC_ enum MAPISTATUS nspi_QueryColumns(struct nspi_context *nspi_ctx,
					   TALLOC_CTX *mem_ctx,
					   bool WantUnicode,
					   struct SPropTagArray **ppColumns)
{
	struct NspiQueryColumns	r;
	NTSTATUS		status;
	enum MAPISTATUS		retval;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ppColumns, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;
	r.in.Reserved = 0x0;
	r.in.dwFlags = (WantUnicode != false) ? NspiUnicodeProptypes : 0x0;
	
	r.out.ppColumns = ppColumns;

	status = dcerpc_NspiQueryColumns_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Returns a list of property names for a set of proptags

   \param nspi_ctx pointer on the NSPI connection text
   \param mem_ctx pointer to the memory context
   \param lpGuid the property set about which the client is requesting
   information
   \param pPropTags pointer to the proptags list
   \param ppReturnedPropTags pointer on pointer to the list of
   all the proptags in the property set specified in lpGuid
   \param ppNames pointer on pointer to the list of property names
   returned by the server

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS nspi_GetNamesFromIDs(struct nspi_context *nspi_ctx,
					      TALLOC_CTX *mem_ctx,
					      struct FlatUID_r *lpGuid,
					      struct SPropTagArray *pPropTags,
					      struct SPropTagArray **ppReturnedPropTags,
					      struct PropertyNameSet_r **ppNames)
{
	struct NspiGetNamesFromIDs	r;
	NTSTATUS			status;
	enum MAPISTATUS			retval;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ppReturnedPropTags, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ppNames, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;
	r.in.Reserved = 0x0;
	r.in.lpGuid = lpGuid;
	r.in.pPropTags = pPropTags;

	r.out.ppReturnedPropTags = ppReturnedPropTags;
	r.out.ppNames = ppNames;

	status = dcerpc_NspiGetNamesFromIDs_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the Property IDs associated with property names
   from the NSPI server.

   \param nspi_ctx pointer on the NSPI connection context
   \param mem_ctx pointer to the memoty context
   \param VerifyNames boolean value defining whether the NSPI server
   must verify that all client specified names are recognized by the
   server
   \param cNames count of PropertyName_r entries
   \param ppNames pointer to a PropertyName_r structure with the list of
   property tags supplied by the client
   \param ppPropTags pointer on pointer to the list of proptags
   returned by the server

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS nspi_GetIDsFromNames(struct nspi_context *nspi_ctx,
					      TALLOC_CTX *mem_ctx,
					      bool VerifyNames,
					      uint32_t cNames,
					      struct PropertyName_r *ppNames,
					      struct SPropTagArray **ppPropTags)
{
	struct NspiGetIDsFromNames	r;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			i;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ppNames, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ppPropTags, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;
	r.in.Reserved = 0;
	r.in.dwFlags = (VerifyNames == true) ? 0x2 : 0x0;
	r.in.cPropNames = cNames;

	r.in.ppNames = talloc_array(mem_ctx, struct PropertyName_r *, cNames);
	for (i = 0; i < cNames; i++) {
		r.in.ppNames[i] = &ppNames[i];
	}

	r.out.ppPropTags = ppPropTags;
	
	status = dcerpc_NspiGetIDsFromNames_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	errno = retval;
	return MAPI_E_SUCCESS;
}


/**
   \details Takes a set of string values in an 8-bit character set and
   performs ANR on those strings

   \param nspi_ctx pointer on the NSPI connection context
   \param mem_ctx pointer to the memory context
   \param usernames pointer on pointer to the list of values we want
   to perform ANR on
   \param pPropTags pointer on the property tags list we want for each
   row returned
   \param pppRows pointer on pointer on pointer to the rows returned
   by the server
   \param pppMIds pointer on pointer on pointer to the MIds matching
   the array of strings

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS nspi_ResolveNames(struct nspi_context *nspi_ctx, 
					   TALLOC_CTX *mem_ctx,
					   const char **usernames, 
					   struct SPropTagArray *pPropTags, 
					   struct PropertyRowSet_r ***pppRows,
					   struct PropertyTagArray_r ***pppMIds)
{
	struct NspiResolveNames r;
	struct StringsArray_r	*paStr;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		count;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!nspi_ctx->rpc_connection, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!nspi_ctx->rpc_connection->binding_handle, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!usernames, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!pPropTags, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!pppRows, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!pppMIds, MAPI_E_INVALID_PARAMETER, NULL);

	for (count = 0; usernames[count]; count++);
	OPENCHANGE_RETVAL_IF(!count, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;

	r.in.pStat = nspi_ctx->pStat;
	r.in.Reserved = 0;
	r.in.pPropTags = pPropTags;
	
	paStr = talloc(mem_ctx, struct StringsArray_r);
	paStr->Count = count;
	paStr->Strings = usernames;
	r.in.paStr = paStr;

	r.out.ppMIds = *pppMIds;
	r.out.ppRows = *pppRows;

	status = dcerpc_NspiResolveNames_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Takes a set of string values in the Unicode character set
   and performs ambiguous name resolution (ANR) on those strings

   \param nspi_ctx pointer on the NSPI connection context
   \param mem_ctx pointer to the memory context
   \param usernames pointer on pointer to the list of values we want
   to perform ANR on
   \param pPropTags pointer on the property tags list we want for each
   row returned
   \param pppRows pointer on pointer on pointer to the rows returned
   by the server
   \param pppMIds pointer on pointer on pointer to the MIds matching
   the array of strings

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS nspi_ResolveNamesW(struct nspi_context *nspi_ctx, 
					    TALLOC_CTX *mem_ctx,
					    const char **usernames, 
					    struct SPropTagArray *pPropTags, 
					    struct PropertyRowSet_r ***pppRows,
					    struct PropertyTagArray_r ***pppMIds)
{
	struct NspiResolveNamesW	r;
	struct StringsArrayW_r		*paWStr;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			count;

	OPENCHANGE_RETVAL_IF(!nspi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!usernames, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!pppRows, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!pppMIds, MAPI_E_INVALID_PARAMETER, NULL);

	for (count = 0; usernames[count]; count++);
	OPENCHANGE_RETVAL_IF(!count, MAPI_E_INVALID_PARAMETER, NULL);

	r.in.handle = &nspi_ctx->handle;

	r.in.pStat = nspi_ctx->pStat;
	r.in.Reserved = 0;
	r.in.pPropTags = pPropTags;

	paWStr = talloc(mem_ctx, struct StringsArrayW_r);
	paWStr->Count = count;
	paWStr->Strings = usernames;
	r.in.paWStr = paWStr;

	r.out.ppMIds = *pppMIds;
	r.out.ppRows = *pppRows;

	status = dcerpc_NspiResolveNamesW_r(nspi_ctx->rpc_connection->binding_handle, mem_ctx, &r);
	retval = r.out.result;
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), retval, NULL);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}
