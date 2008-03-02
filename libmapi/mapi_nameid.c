/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007.

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
#include <libmapi/mapi_nameid.h>
#include <libmapi/mapi_nameid_private.h>


/**
   \file mapi_nameid.c

   \brief mapi_nameid convenience API
*/


/**
   \details Create a new mapi_nameid structure

   \param mem_ctx memory context to use for allocation

   \returns a pointer to an allocated mapi_nameid structure on
   success, otherwise NULL

   \sa GetIDsFromNames
*/
_PUBLIC_ struct mapi_nameid *mapi_nameid_new(TALLOC_CTX *mem_ctx)
{
	struct mapi_nameid	*mapi_nameid = NULL;

	/* Sanity check */
	if (!mem_ctx) return NULL;

	mapi_nameid = talloc_zero(mem_ctx, struct mapi_nameid);
	if (!mapi_nameid) return NULL;

	mapi_nameid->nameid = NULL;
	mapi_nameid->entries = NULL;
	mapi_nameid->count = 0;

	return mapi_nameid;
}


/**
   \details Add a mapi_nameid entry given its OOM and OLEGUID
   (MNID_ID|MNID_STRING)

   \param mapi_nameid the structure where results are stored
   \param OOM the Outlook Object Model matching string
   \param OLEGUID the property set this entry belongs to

   \return MAPI_E_SUCCESS on success, otherwise -1.
   
   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the parameters was not set
   properly
   - MAPI_E_NOT_FOUND: the entry intended to be added was not found

   \sa mapi_nameid_new
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_OOM_add(struct mapi_nameid *mapi_nameid,
					     const char *OOM, 
					     const char *OLEGUID)
{
	uint32_t		i;
	uint16_t		count;

	/* Sanity check */
	MAPI_RETVAL_IF(!mapi_nameid, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!OOM, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; mapi_nameid_tags[i].OLEGUID; i++) {
		if (mapi_nameid_tags[i].OOM &&
		    !strcmp(OOM, mapi_nameid_tags[i].OOM) && 
		    !strcmp(OLEGUID, mapi_nameid_tags[i].OLEGUID)) {
			mapi_nameid->nameid = talloc_realloc(mapi_nameid, 
							     mapi_nameid->nameid, struct MAPINAMEID,
							     mapi_nameid->count + 1);
			mapi_nameid->entries = talloc_realloc(mapi_nameid,
							    mapi_nameid->entries, struct mapi_nameid_tags,
							    mapi_nameid->count + 1);
			count = mapi_nameid->count;

			mapi_nameid->entries[count] = mapi_nameid_tags[i];

			mapi_nameid->nameid[count].ulKind = mapi_nameid_tags[i].ulKind;
			GUID_from_string(mapi_nameid_tags[i].OLEGUID,
					 &(mapi_nameid->nameid[count].lpguid));
			switch (mapi_nameid_tags[i].ulKind) {
			case MNID_ID:
				mapi_nameid->nameid[count].kind.lid = mapi_nameid_tags[i].lid;
				break;
			case MNID_STRING:
				mapi_nameid->nameid[count].kind.lpwstr.lpwstrName = mapi_nameid_tags[i].lpwstrName;
				mapi_nameid->nameid[count].kind.lpwstr.length = strlen(mapi_nameid_tags[i].lpwstrName) * 2 + 2;
				break;
			}
			mapi_nameid->count++;
			return MAPI_E_SUCCESS;
		}
	}

	return MAPI_E_NOT_FOUND;
}


/**
   \details Add a mapi_nameid entry given its lid and OLEGUID
   (MNID_ID)

   \param mapi_nameid the structure where results are stored
   \param lid the light ID of the name property (used by MNID_ID named
   props only)
   \param OLEGUID the property set this entry belongs to

   \return MAPI_E_SUCCESS on success, otherwise -1.
   
   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the parameters was not set
   properly
   - MAPI_E_NOT_FOUND: the entry intended to be added was not found

   \sa mapi_nameid_new
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_lid_add(struct mapi_nameid *mapi_nameid,
					     uint16_t lid, const char *OLEGUID)
{
	uint32_t		i;
	uint16_t		count;

	/* Sanity check */
	MAPI_RETVAL_IF(!mapi_nameid, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!lid, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; mapi_nameid_tags[i].OLEGUID; i++) {
		if ((lid == mapi_nameid_tags[i].lid) &&
		    !strcmp(OLEGUID, mapi_nameid_tags[i].OLEGUID)) {
			mapi_nameid->nameid = talloc_realloc(mapi_nameid, 
							     mapi_nameid->nameid, struct MAPINAMEID,
							     mapi_nameid->count + 1);
			mapi_nameid->entries = talloc_realloc(mapi_nameid,
							    mapi_nameid->entries, struct mapi_nameid_tags,
							    mapi_nameid->count + 1);
			count = mapi_nameid->count;

			mapi_nameid->entries[count] = mapi_nameid_tags[i];

			mapi_nameid->nameid[count].ulKind = mapi_nameid_tags[i].ulKind;
			GUID_from_string(mapi_nameid_tags[i].OLEGUID,
					 &(mapi_nameid->nameid[count].lpguid));
			switch (mapi_nameid_tags[i].ulKind) {
			case MNID_ID:
				mapi_nameid->nameid[count].kind.lid = mapi_nameid_tags[i].lid;
				break;
			case MNID_STRING:
				mapi_nameid->nameid[count].kind.lpwstr.lpwstrName = mapi_nameid_tags[i].lpwstrName;
				mapi_nameid->nameid[count].kind.lpwstr.length = strlen(mapi_nameid_tags[i].lpwstrName) * 2 + 2;
				break;
			}
			mapi_nameid->count++;
			return MAPI_E_SUCCESS;
		}
	}

	return MAPI_E_NOT_FOUND;	
}


/**
   \details Add a mapi_nameid entry given its lpwstrName and OLEGUID
   (MNID_STRING)

   \param mapi_nameid the structure where results are stored
   \param lpwstrName the property name (used by MNID_STRING named
   props only)
   \param OLEGUID the property set this entry belongs to

   \return MAPI_E_SUCCESS on success, otherwise -1.
   
   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the parameters was not set
   properly
   - MAPI_E_NOT_FOUND: the entry intended to be added was not found

   \sa mapi_nameid_new
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_string_add(struct mapi_nameid *mapi_nameid,
						const char *lpwstrName,
						const char *OLEGUID)
{
	uint32_t		i;
	uint16_t		count;

	/* Sanity check */
	MAPI_RETVAL_IF(!mapi_nameid, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!lpwstrName, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; mapi_nameid_tags[i].OLEGUID; i++) {
		if (mapi_nameid_tags[i].lpwstrName && 
		    !strcmp(lpwstrName, mapi_nameid_tags[i].lpwstrName) &&
		    !strcmp(OLEGUID, mapi_nameid_tags[i].OLEGUID)) {
			mapi_nameid->nameid = talloc_realloc(mapi_nameid, 
							     mapi_nameid->nameid, struct MAPINAMEID,
							     mapi_nameid->count + 1);
			mapi_nameid->entries = talloc_realloc(mapi_nameid,
							    mapi_nameid->entries, struct mapi_nameid_tags,
							    mapi_nameid->count + 1);
			count = mapi_nameid->count;

			mapi_nameid->entries[count] = mapi_nameid_tags[i];

			mapi_nameid->nameid[count].ulKind = mapi_nameid_tags[i].ulKind;
			GUID_from_string(mapi_nameid_tags[i].OLEGUID,
					 &(mapi_nameid->nameid[count].lpguid));
			switch (mapi_nameid_tags[i].ulKind) {
			case MNID_ID:
				mapi_nameid->nameid[count].kind.lid = mapi_nameid_tags[i].lid;
				break;
			case MNID_STRING:
				mapi_nameid->nameid[count].kind.lpwstr.lpwstrName = mapi_nameid_tags[i].lpwstrName;
				mapi_nameid->nameid[count].kind.lpwstr.length = strlen(mapi_nameid_tags[i].lpwstrName) * 2 + 2;
				break;
			}
			mapi_nameid->count++;
			return MAPI_E_SUCCESS;
		}
	}

	return MAPI_E_NOT_FOUND;
}


/**
   \details set SPropTagArray ulPropTag property types from
   mapi_nameid returned by GetIDsFromNames()

   \param mapi_nameid the structure where results are stored
   \param SPropTagArray the array of property tags returned by
   previous call to GetIDsFromNames()

   \return MAPI_E_SUCCESS on success, otherwise -1.
   
   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameters was not set
   properly

   \sa GetIDsFromNames
*/
_PUBLIC_ enum MAPISTATUS mapi_nameid_SPropTagArray(struct mapi_nameid *mapi_nameid,
						   struct SPropTagArray *SPropTagArray)
{
	uint32_t	i;

	/* sanity check */
	MAPI_RETVAL_IF(!mapi_nameid, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; i < mapi_nameid->count; i++) {
		SPropTagArray->aulPropTag[i] = (SPropTagArray->aulPropTag[i] & 0xFFFF0000) | 
			mapi_nameid->entries[i].propType;
	}
	
	return MAPI_E_SUCCESS;
}


/**
   \details Lookup named properties (MNID_STRING) and return their
   mapped proptags

   This convenient function calls GetIDsFromNames() and returns
   property tags with their real property type.

   \return MAPI_E_SUCCESS on success, otherwise -1.
   
   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameters was not set
   properly

   \sa GetIDsFromNames, mapi_nameid_SPropTagArray
*/
_PUBLIC_ enum MAPISTATUS mapi_nameid_GetIDsFromNames(struct mapi_nameid *mapi_nameid,
						     mapi_object_t *obj, 
						     struct SPropTagArray *SPropTagArray)
{
	enum MAPISTATUS		retval;
	uint32_t		i;

	/* sanity check */
	MAPI_RETVAL_IF(!mapi_nameid, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);

	retval = GetIDsFromNames(obj, mapi_nameid->count, mapi_nameid->nameid, 0,
				 &SPropTagArray);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);

	for (i = 0; i < SPropTagArray->cValues; i++) {
		SPropTagArray->aulPropTag[i] = (SPropTagArray->aulPropTag[i] & 0xFFFF0000) | 
			mapi_nameid->entries[i].propType;
	}

	return MAPI_E_SUCCESS;
}
