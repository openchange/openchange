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
  \details Register and add a custom MNID_ID named property given its
  lid, proptype and OLEGUID.
 
  \param mapi_nameid the structure where results are stored 
  \param lid the light ID of the name property (used by MNID_ID named
  props only)
  \param propType the named property type
  \param OLEGUID the property set this entry belongs to
 
  \return MAPI_E_SUCCESS on success, otherwise -1.
 
  \note Developers should call GetLastError() to retrieve the last
  MAPI error code. Possible MAPI error codes are:
  - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
  - MAPI_E_INVALID_PARAMETER: one of the parameter was no set properly

  \sa mapi_nameid_new, mapi_nameid_lid_add
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_custom_lid_add(struct mapi_nameid *mapi_nameid, 
						    uint16_t lid, uint16_t propType, 
						    const char *OLEGUID)
{
	uint16_t	count;

	/* Sanity check */
	MAPI_RETVAL_IF(!mapi_nameid, MAPI_E_NOT_INITIALIZED,  NULL);
	MAPI_RETVAL_IF(!lid, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!propType, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_nameid->nameid = talloc_realloc(mapi_nameid,
					     mapi_nameid->nameid, struct MAPINAMEID,
					     mapi_nameid->count + 1);
	mapi_nameid->entries = talloc_realloc(mapi_nameid, 
					      mapi_nameid->entries, struct mapi_nameid_tags,
					      mapi_nameid->count + 1);

	count = mapi_nameid->count;
	mapi_nameid->entries[count].lid = lid;
	mapi_nameid->entries[count].propType = propType;
	mapi_nameid->entries[count].ulKind = MNID_ID;
	mapi_nameid->entries[count].OLEGUID = OLEGUID;

	mapi_nameid->nameid[count].ulKind = MNID_ID;
	GUID_from_string(OLEGUID, &(mapi_nameid->nameid[count].lpguid));
	mapi_nameid->nameid[count].kind.lid = lid;

	mapi_nameid->count++;
	return MAPI_E_SUCCESS;
}


/**
   \details Register and add a custom MNID_STRING named property given
   its string, proptype and OLEGUID.
 
   \param mapi_nameid the structure where results are stored
   \param lpwstrName the property name (used by MNID_STRING named props only)
   \param propType the named property type
   \param OLEGUID the property set this entry belongs to

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the parameter was not set properly.

   \sa mapi_nameid_new, mapi_nameid_string_add
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_custom_string_add(struct mapi_nameid *mapi_nameid,
						       const char *lpwstrName, uint16_t propType,
						       const char *OLEGUID)
{
	uint16_t	count;

	/* Sanity check */
	MAPI_RETVAL_IF(!mapi_nameid, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!lpwstrName, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!propType, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_nameid->nameid = talloc_realloc(mapi_nameid,
					     mapi_nameid->nameid, struct MAPINAMEID,
					     mapi_nameid->count + 1);
	mapi_nameid->entries = talloc_realloc(mapi_nameid,
					      mapi_nameid->entries, struct mapi_nameid_tags,
					      mapi_nameid->count + 1);
	count = mapi_nameid->count;
	mapi_nameid->entries[count].lpwstrName = lpwstrName;
	mapi_nameid->entries[count].propType = propType;
	mapi_nameid->entries[count].ulKind = MNID_STRING;
	mapi_nameid->entries[count].OLEGUID = OLEGUID;

	mapi_nameid->nameid[count].ulKind = MNID_STRING;
	GUID_from_string(OLEGUID, &(mapi_nameid->nameid[count].lpguid));
	mapi_nameid->nameid[count].kind.lpwstr.lpwstrName = lpwstrName;
	mapi_nameid->nameid[count].kind.lpwstr.length = strlen(lpwstrName) * 2 + 2;

	mapi_nameid->count++;
	return MAPI_E_SUCCESS;
}


/**
   \details Search for a given OOM,OLEGUID couple and return the
   associated propType.

   \param OOM The Outlook Object Model
   \param OLEGUID the named property GUID for this entry
   \param propType pointer on returned named property type

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameter was not set properly.
   - MAPI_E_NOT_FOUND: no named property found
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_OOM_lookup(const char *OOM, const char *OLEGUID,
						uint16_t *propType)
{
	uint32_t	i;

	/* Sanity checks */
	MAPI_RETVAL_IF(!OOM, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; mapi_nameid_tags[i].OLEGUID; i++) {
		if (mapi_nameid_tags[i].OOM && 
		    !strcmp(mapi_nameid_tags[i].OOM, OOM) &&
		    !strcmp(mapi_nameid_tags[i].OLEGUID, OLEGUID)) {
			*propType = mapi_nameid_tags[i].propType;
			return MAPI_E_SUCCESS;
		}
	}

	errno = MAPI_E_NOT_FOUND;
	return -1;
}


/**
   \details Search for a given OOM,OLEGUID couple and return the
   associated propType.

   \param lid the named property light ID
   \param OLEGUID the named property GUID for this entry
   \param propType pointer on returned named property type

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameter was not set properly.
   - MAPI_E_NOT_FOUND: no named property found
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_lid_lookup(uint16_t lid, const char *OLEGUID,
						uint16_t *propType)
{
	uint32_t	i;

	/* Sanity checks */
	MAPI_RETVAL_IF(!lid, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; mapi_nameid_tags[i].OLEGUID; i++) {
		if (mapi_nameid_tags[i].lid == lid &&
		    !strcmp(mapi_nameid_tags[i].OLEGUID, OLEGUID)) {
			*propType = mapi_nameid_tags[i].propType;
			return MAPI_E_SUCCESS;
		}
	}

	errno = MAPI_E_NOT_FOUND;
	return -1;
}


/**
   \details Search for a given OOM,OLEGUID couple and return the
   associated propType.

   \param lpwstrName the named property name
   \param OLEGUID the named property GUID for this entry
   \param propType pointer on returned named property type

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameter was not set properly.
   - MAPI_E_NOT_FOUND: no named property found
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_string_lookup(const char *lpwstrName, 
						   const char *OLEGUID,
						   uint16_t *propType)
{
	uint32_t	i;

	/* Sanity checks */
	MAPI_RETVAL_IF(!lpwstrName, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; mapi_nameid_tags[i].OLEGUID; i++) {
		if (mapi_nameid_tags[i].lpwstrName &&
		    !strcmp(mapi_nameid_tags[i].lpwstrName, lpwstrName) &&
		    !strcmp(mapi_nameid_tags[i].OLEGUID, OLEGUID)) {
			*propType = mapi_nameid_tags[i].propType;
			return MAPI_E_SUCCESS;
		}
	}
	
	errno = MAPI_E_NOT_FOUND;
	return -1;
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
