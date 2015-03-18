/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2011.

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
#include "libmapi/mapi_nameid.h"
#include "libmapi/mapi_nameid_private.h"
#include "libmapi/libmapi_private.h"


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

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
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
	OPENCHANGE_RETVAL_IF(!mapi_nameid, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!OOM, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

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

			mapi_nameid->nameid[count].ulKind = (enum ulKind)mapi_nameid_tags[i].ulKind;
			GUID_from_string(mapi_nameid_tags[i].OLEGUID,
					 &(mapi_nameid->nameid[count].lpguid));
			switch (mapi_nameid_tags[i].ulKind) {
			case MNID_ID:
				mapi_nameid->nameid[count].kind.lid = mapi_nameid_tags[i].lid;
				break;
			case MNID_STRING:
				mapi_nameid->nameid[count].kind.lpwstr.Name = mapi_nameid_tags[i].Name;
				mapi_nameid->nameid[count].kind.lpwstr.NameSize = get_utf8_utf16_conv_length(mapi_nameid_tags[i].Name);
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

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
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
	OPENCHANGE_RETVAL_IF(!mapi_nameid, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!lid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

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

			mapi_nameid->nameid[count].ulKind = (enum ulKind) mapi_nameid_tags[i].ulKind;
			GUID_from_string(mapi_nameid_tags[i].OLEGUID,
					 &(mapi_nameid->nameid[count].lpguid));
			switch (mapi_nameid_tags[i].ulKind) {
			case MNID_ID:
				mapi_nameid->nameid[count].kind.lid = mapi_nameid_tags[i].lid;
				break;
			case MNID_STRING:
				mapi_nameid->nameid[count].kind.lpwstr.Name = mapi_nameid_tags[i].Name;
				mapi_nameid->nameid[count].kind.lpwstr.NameSize = get_utf8_utf16_conv_length(mapi_nameid_tags[i].Name);
				break;
			}
			mapi_nameid->count++;
			return MAPI_E_SUCCESS;
		}
	}

	return MAPI_E_NOT_FOUND;
}


/**
   \details Add a mapi_nameid entry given its Name and OLEGUID
   (MNID_STRING)

   \param mapi_nameid the structure where results are stored
   \param Name the property name (used by MNID_STRING named
   props only)
   \param OLEGUID the property set this entry belongs to

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the parameters was not set
   properly
   - MAPI_E_NOT_FOUND: the entry intended to be added was not found

   \sa mapi_nameid_new
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_string_add(struct mapi_nameid *mapi_nameid,
						const char *Name,
						const char *OLEGUID)
{
	uint32_t		i;
	uint16_t		count;

	/* Sanity check */
	OPENCHANGE_RETVAL_IF(!mapi_nameid, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!Name, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; mapi_nameid_tags[i].OLEGUID; i++) {
		if (mapi_nameid_tags[i].Name &&
		    !strcmp(Name, mapi_nameid_tags[i].Name) &&
		    !strcmp(OLEGUID, mapi_nameid_tags[i].OLEGUID)) {
			mapi_nameid->nameid = talloc_realloc(mapi_nameid,
							     mapi_nameid->nameid, struct MAPINAMEID,
							     mapi_nameid->count + 1);
			mapi_nameid->entries = talloc_realloc(mapi_nameid,
							    mapi_nameid->entries, struct mapi_nameid_tags,
							    mapi_nameid->count + 1);
			count = mapi_nameid->count;

			mapi_nameid->entries[count] = mapi_nameid_tags[i];

			mapi_nameid->nameid[count].ulKind = (enum ulKind) mapi_nameid_tags[i].ulKind;
			GUID_from_string(mapi_nameid_tags[i].OLEGUID,
					 &(mapi_nameid->nameid[count].lpguid));
			switch (mapi_nameid_tags[i].ulKind) {
			case MNID_ID:
				mapi_nameid->nameid[count].kind.lid = mapi_nameid_tags[i].lid;
				break;
			case MNID_STRING:
				mapi_nameid->nameid[count].kind.lpwstr.Name = mapi_nameid_tags[i].Name;
				mapi_nameid->nameid[count].kind.lpwstr.NameSize = get_utf8_utf16_conv_length(mapi_nameid_tags[i].Name);
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

  \return MAPI_E_SUCCESS on success, otherwise MAPI error.

  \note Developers may also call GetLastError() to retrieve the last
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
	OPENCHANGE_RETVAL_IF(!mapi_nameid, MAPI_E_NOT_INITIALIZED,  NULL);
	OPENCHANGE_RETVAL_IF(!lid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!propType, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

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
   \param Name the property name (used by MNID_STRING named props only)
   \param propType the named property type
   \param OLEGUID the property set this entry belongs to

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the parameter was not set properly.

   \sa mapi_nameid_new, mapi_nameid_string_add
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_custom_string_add(struct mapi_nameid *mapi_nameid,
						       const char *Name, uint16_t propType,
						       const char *OLEGUID)
{
	uint16_t	count;

	/* Sanity check */
	OPENCHANGE_RETVAL_IF(!mapi_nameid, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!Name, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!propType, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_nameid->nameid = talloc_realloc(mapi_nameid,
					     mapi_nameid->nameid, struct MAPINAMEID,
					     mapi_nameid->count + 1);
	mapi_nameid->entries = talloc_realloc(mapi_nameid,
					      mapi_nameid->entries, struct mapi_nameid_tags,
					      mapi_nameid->count + 1);
	count = mapi_nameid->count;
	mapi_nameid->entries[count].Name = Name;
	mapi_nameid->entries[count].propType = propType;
	mapi_nameid->entries[count].ulKind = MNID_STRING;
	mapi_nameid->entries[count].OLEGUID = OLEGUID;

	mapi_nameid->nameid[count].ulKind = MNID_STRING;
	GUID_from_string(OLEGUID, &(mapi_nameid->nameid[count].lpguid));
	mapi_nameid->nameid[count].kind.lpwstr.Name = Name;
	mapi_nameid->nameid[count].kind.lpwstr.NameSize = get_utf8_utf16_conv_length(Name);

	mapi_nameid->count++;
	return MAPI_E_SUCCESS;
}


/**
   \details Add a mapi_nameid entry given its canonical property tag

   \param mapi_nameid the structure where results are stored
   \param proptag the canonical property tag we are searching

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZE: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: one of the parameters was not set
   properly
   - MAPI_E_NOT_FOUND: the entry intended to be added was not found

   \sa mapi_nameid_new
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_canonical_add(struct mapi_nameid *mapi_nameid,
						   uint32_t proptag)
{
	uint32_t	i;
	uint16_t	count;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!mapi_nameid, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!proptag, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; mapi_nameid_tags[i].OLEGUID; i++) {
		if (mapi_nameid_tags[i].proptag == proptag) {
			mapi_nameid->nameid = talloc_realloc(mapi_nameid,
							     mapi_nameid->nameid, struct MAPINAMEID,
							     mapi_nameid->count + 1);
			mapi_nameid->entries = talloc_realloc(mapi_nameid,
							      mapi_nameid->entries, struct mapi_nameid_tags,
							      mapi_nameid->count + 1);
			count = mapi_nameid->count;

			mapi_nameid->entries[count] = mapi_nameid_tags[i];

			mapi_nameid->nameid[count].ulKind = (enum ulKind) mapi_nameid_tags[i].ulKind;
			GUID_from_string(mapi_nameid_tags[i].OLEGUID,
					 &(mapi_nameid->nameid[count].lpguid));
			switch (mapi_nameid_tags[i].ulKind) {
			case MNID_ID:
				mapi_nameid->nameid[count].kind.lid = mapi_nameid_tags[i].lid;
				break;
			case MNID_STRING:
				mapi_nameid->nameid[count].kind.lpwstr.Name = mapi_nameid_tags[i].Name;
				mapi_nameid->nameid[count].kind.lpwstr.NameSize = get_utf8_utf16_conv_length(mapi_nameid_tags[i].Name);
				break;
			}
			mapi_nameid->count++;
			return MAPI_E_SUCCESS;
		}
	}

	return MAPI_E_NOT_FOUND;
}


/**
   \details Search for a given referenced unmapped named property and
   return whether it was found or not.

   \param proptag the unmapped property tag to lookup

   \return MAPI_E_SUCCESS on success otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_property_lookup(uint32_t proptag)
{
	uint32_t	i;

	for (i = 0; mapi_nameid_tags[i].proptag; i++) {
		if (mapi_nameid_tags[i].proptag == proptag) {
			return MAPI_E_SUCCESS;
		}
	}

	return MAPI_E_NOT_FOUND;
}


/**
   \details Search for a given OOM,OLEGUID couple and return the
   associated propType.

   \param OOM The Outlook Object Model
   \param OLEGUID the named property GUID for this entry
   \param propType pointer on returned named property type

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameter was not set properly.
   - MAPI_E_NOT_FOUND: no named property found
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_OOM_lookup(const char *OOM, const char *OLEGUID,
						uint16_t *propType)
{
	uint32_t	i;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!OOM, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; mapi_nameid_tags[i].OLEGUID; i++) {
		if (mapi_nameid_tags[i].OOM &&
		    !strcmp(mapi_nameid_tags[i].OOM, OOM) &&
		    !strcmp(mapi_nameid_tags[i].OLEGUID, OLEGUID)) {
			*propType = mapi_nameid_tags[i].propType;
			return MAPI_E_SUCCESS;
		}
	}

	OPENCHANGE_RETVAL_ERR(MAPI_E_NOT_FOUND, NULL);
}


/**
   \details Search for a given lid,OLEGUID couple and return the
   associated propType.

   \param lid the named property light ID
   \param OLEGUID the named property GUID for this entry
   \param propType pointer on returned named property type

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameter was not set properly.
   - MAPI_E_NOT_FOUND: no named property found
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_lid_lookup(uint16_t lid, const char *OLEGUID,
						uint16_t *propType)
{
	uint32_t	i;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!lid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; mapi_nameid_tags[i].OLEGUID; i++) {
		if (mapi_nameid_tags[i].lid == lid &&
		    !strcmp(mapi_nameid_tags[i].OLEGUID, OLEGUID)) {
			*propType = mapi_nameid_tags[i].propType;
			return MAPI_E_SUCCESS;
		}
	}

	OPENCHANGE_RETVAL_ERR(MAPI_E_NOT_FOUND, NULL);
}


/**
   \details Search for a given lid,OLEGUID couple and return the
   associated canonical propTag.

   \param lid the named property light ID
   \param OLEGUID the named property GUID for this entry
   \param propTag pointer on returned named canonical property tag

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameter was not set properly.
   - MAPI_E_NOT_FOUND: no named property found
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_lid_lookup_canonical(uint16_t lid, const char *OLEGUID,
							  uint32_t *propTag)
{
	uint32_t	i;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!lid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!propTag, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; mapi_nameid_tags[i].OLEGUID; i++) {
		if (mapi_nameid_tags[i].lid == lid &&
		    !strcmp(mapi_nameid_tags[i].OLEGUID, OLEGUID)) {
			*propTag = mapi_nameid_tags[i].proptag;
			return MAPI_E_SUCCESS;
		}
	}

	OPENCHANGE_RETVAL_ERR(MAPI_E_NOT_FOUND, NULL);
}


/**
   \details Search for a given Name,OLEGUID couple and return the
   associated propType.

   \param Name the named property name
   \param OLEGUID the named property GUID for this entry
   \param propType pointer on returned named property type

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameter was not set properly.
   - MAPI_E_NOT_FOUND: no named property found
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_string_lookup(const char *Name,
						   const char *OLEGUID,
						   uint16_t *propType)
{
	uint32_t	i;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!Name, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; mapi_nameid_tags[i].OLEGUID; i++) {
		if (mapi_nameid_tags[i].Name &&
		    !strcmp(mapi_nameid_tags[i].Name, Name) &&
		    !strcmp(mapi_nameid_tags[i].OLEGUID, OLEGUID)) {
			*propType = mapi_nameid_tags[i].propType;
			return MAPI_E_SUCCESS;
		}
	}

	OPENCHANGE_RETVAL_ERR(MAPI_E_NOT_FOUND, NULL);
}


/**
   \details Search for a given Name,OLEGUID couple and return the
   associated canonical propTag.

   \param Name the named property name
   \param OLEGUID the named property GUID for this entry
   \param propTag pointer on returned named canonical property tag

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameter was not set properly.
   - MAPI_E_NOT_FOUND: no named property found
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_string_lookup_canonical(const char *Name,
							     const char *OLEGUID,
							     uint32_t *propTag)
{
	uint32_t	i;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!Name, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!OLEGUID, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!propTag, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; mapi_nameid_tags[i].OLEGUID; i++) {
		if (mapi_nameid_tags[i].Name &&
		    !strcmp(mapi_nameid_tags[i].Name, Name) &&
		    !strcmp(mapi_nameid_tags[i].OLEGUID, OLEGUID)) {
			*propTag = mapi_nameid_tags[i].proptag;
			return MAPI_E_SUCCESS;
		}
	}

	OPENCHANGE_RETVAL_ERR(MAPI_E_NOT_FOUND, NULL);
}


/**
   \details set SPropTagArray ulPropTag property types from
   mapi_nameid returned by GetIDsFromNames(). If a SPropTagArray property tag
   has the type PT_ERROR set, it's kept untouched to propagate the error.

   \param mapi_nameid the structure where results are stored
   \param SPropTagArray the array of property tags returned by
   previous call to GetIDsFromNames()

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
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
	OPENCHANGE_RETVAL_IF(!mapi_nameid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; i < mapi_nameid->count; i++) {
		if (((int)SPropTagArray->aulPropTag[i] & 0x0000FFFF) != PT_ERROR) {
			if (mapi_nameid->entries[i].propType) {
				SPropTagArray->aulPropTag[i] = (enum MAPITAGS)(
					((int)SPropTagArray->aulPropTag[i] & 0xFFFF0000) |
					mapi_nameid->entries[i].propType);
			}
		}
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Replace named property tags in SPropTagArray with the
   property ID Exchange expects and stored in SPropTagArray2.

   \param mapi_nameid the structure where results are stored
   \param SPropTagArray the array of property tags with original
   property tags
   \param SPropTagArray2 the array of named property tags resolved
   with GetIDsFromNames

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameters was not set
   properly

   \sa GetIDsFromNames
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_map_SPropTagArray(struct mapi_nameid *mapi_nameid,
							struct SPropTagArray *SPropTagArray,
							struct SPropTagArray *SPropTagArray2)
{
	uint32_t	i;
	uint32_t	j;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!mapi_nameid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray2, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; i < SPropTagArray->cValues; i++) {
		for (j = 0; j < mapi_nameid->count; j++) {
		  if ((enum MAPITAGS)mapi_nameid->entries[j].proptag == SPropTagArray->aulPropTag[i]) {
			  SPropTagArray->aulPropTag[i] = (enum MAPITAGS)(((int)SPropTagArray2->aulPropTag[j] & 0xFFFF0000) |
									  mapi_nameid->entries[j].propType);
				mapi_nameid->entries[j].position = i;
			}
		}
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Restore the original SPropTagArray array with the property
   tags saved in the mapi_nameid structure.

   \param mapi_nameid the structure where results are stored
   \param SPropTagArray the array of property tags with original
   property tags

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameters was not set
   properly

   \sa GetIDsFromNames
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_unmap_SPropTagArray(struct mapi_nameid *mapi_nameid,
							 struct SPropTagArray *SPropTagArray)
{
	uint32_t	i;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!mapi_nameid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; i < mapi_nameid->count; i++) {
		if (mapi_nameid->entries[i].position <= SPropTagArray->cValues) {
		  SPropTagArray->aulPropTag[mapi_nameid->entries[i].position] = (enum MAPITAGS) mapi_nameid->entries[i].proptag;
		}
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Replace named property tags in the SPropValue array with
   the property ID Exchange expects and stored in SPropTagArray.

   \param mapi_nameid the structure where results are stored
   \param lpProps pointer on a SPropValue structure with property tags
   and values
   \param PropCount count of lpProps elements
   \param SPropTagArray the array of named property tags resolved
   with GetIDsFromNames

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameters was not set
   properly

   \sa GetIDsFromNames
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_map_SPropValue(struct mapi_nameid *mapi_nameid,
						    struct SPropValue *lpProps,
						    uint32_t PropCount,
						    struct SPropTagArray *SPropTagArray)
{
	uint32_t	i;
	uint32_t	j;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!mapi_nameid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!lpProps, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!PropCount, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; i < PropCount; i++) {
		for (j = 0; j < mapi_nameid->count; j++) {
		  if ((enum MAPITAGS)mapi_nameid->entries[j].proptag == lpProps[i].ulPropTag) {
			  lpProps[i].ulPropTag = (enum MAPITAGS)(((int)SPropTagArray->aulPropTag[j] & 0xFFFF0000) |
								 mapi_nameid->entries[j].propType);
				mapi_nameid->entries[j].position = i;
			}
		}
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Restore the original SPropValue array with the property
   tags saved in the mapi_nameid structure.

   \param mapi_nameid the structure where results are stored
   \param lpProps the array of SPropValue structures with original
   property tags
   \param PropCount count of lpProps elements

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameters was not set
   properly

   \sa GetIDsFromNames
 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_unmap_SPropValue(struct mapi_nameid *mapi_nameid,
						      struct SPropValue *lpProps,
						      uint32_t PropCount)
{
	uint32_t	i;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!mapi_nameid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!lpProps, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!PropCount, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; i < mapi_nameid->count; i++) {
		if (mapi_nameid->entries[i].position <= PropCount) {
		  lpProps[mapi_nameid->entries[i].position].ulPropTag = (enum MAPITAGS) mapi_nameid->entries[i].proptag;
		}
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Loop over SPropTagArray and look for canonical named
   property tags we can add to the nameid structure.

   \param nameid the structure where results are stored
   \param SPropTagArray the array of property tags where to look for
   canonical named property tags.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameters was not set
   properly
   - MAPI_E_NOT_FOUND: no named property found

   \sa GetIDsFromNames

 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_lookup_SPropTagArray(struct mapi_nameid *nameid,
							  struct SPropTagArray *SPropTagArray)
{
	enum MAPISTATUS	retval;
	uint32_t	i;
	bool		status = false;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!nameid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; i < SPropTagArray->cValues; i++) {
		if (mapi_nameid_property_lookup(SPropTagArray->aulPropTag[i]) == MAPI_E_SUCCESS) {
			retval = mapi_nameid_canonical_add(nameid, SPropTagArray->aulPropTag[i]);
			if (retval == MAPI_E_SUCCESS) {
				status = true;
			}
		}
	}

	return (status == true) ? MAPI_E_SUCCESS : MAPI_E_NOT_FOUND;
}


/**
   \details Loop over lpProps and look for canonical named
   property tags we can add to the nameid structure.

   \param mapi_nameid the structure where results are stored
   \param lpProps pointer on a SPropValue structure with the property
   tags where to look for canonical named property tags
   \param PropCount count of lpProps elemense

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: one of the parameters was not set
   properly
   - MAPI_E_NOT_FOUND: no named property found

   \sa GetIDsFromNames

 */
_PUBLIC_ enum MAPISTATUS mapi_nameid_lookup_SPropValue(struct mapi_nameid *mapi_nameid,
						       struct SPropValue *lpProps,
						       unsigned long PropCount)
{
	enum MAPISTATUS	retval;
	uint32_t	i;
	uint16_t	proptype;
	bool		status = false;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!mapi_nameid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!lpProps, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; i < PropCount; i++) {
		proptype = (lpProps[i].ulPropTag & 0xFFFF0000) >> 16;
		if (((proptype >= 0x8000) && (proptype <= 0x8FFF)) ||
		    ((proptype >= 0xa000) && (proptype <= 0xaFFF))) {
			retval = mapi_nameid_canonical_add(mapi_nameid, lpProps[i].ulPropTag);
			if (retval == MAPI_E_SUCCESS) {
				status = true;
			}
		}
	}

	return (status == true) ? MAPI_E_SUCCESS : MAPI_E_NOT_FOUND;
}


/**
   \details Lookup named properties (MNID_STRING) and return their
   mapped proptags

   This convenient function calls GetIDsFromNames() and returns
   property tags with their real property type.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
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

	/* sanity check */
	OPENCHANGE_RETVAL_IF(!mapi_nameid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);

	retval = GetIDsFromNames(obj, mapi_nameid->count, mapi_nameid->nameid, 0,
				 &SPropTagArray);
	OPENCHANGE_RETVAL_IF(((retval != MAPI_E_SUCCESS) && (retval != MAPI_W_ERRORS_RETURNED)), retval, NULL);

	retval = mapi_nameid_SPropTagArray(mapi_nameid, SPropTagArray);
	OPENCHANGE_RETVAL_IF((retval != MAPI_E_SUCCESS), retval, NULL);


	return retval;
}

_PUBLIC_ const char *get_namedid_name(uint32_t proptag)
{
	uint32_t idx;

	for (idx = 0; mapi_nameid_names[idx].proptag; idx++) {
		if (mapi_nameid_names[idx].proptag == proptag) {
			return mapi_nameid_names[idx].propname;
		}
	}
	if (((proptag & 0xFFFF) == PT_STRING8) ||
	    ((proptag & 0xFFFF) == PT_MV_STRING8)) {
		proptag += 1; /* try as _UNICODE variant */
	}
	for (idx = 0; mapi_nameid_names[idx].proptag; idx++) {
		if (mapi_nameid_names[idx].proptag == proptag) {
			return mapi_nameid_names[idx].propname;
		}
	}
	return NULL;
}

_PUBLIC_ uint32_t get_namedid_value(const char *propname)
{
	uint32_t idx;

	for (idx = 0; mapi_nameid_names[idx].proptag; idx++) {
		if (!strcmp(mapi_nameid_names[idx].propname, propname)) {
			return mapi_nameid_names[idx].proptag;
		}
	}

	return 0;
}

_PUBLIC_ uint16_t get_namedid_type(uint16_t untypedtag)
{
	uint32_t	idx;
	uint16_t	current_type;

	for (idx = 0; mapi_nameid_names[idx].proptag; idx++) {
		if ((mapi_nameid_names[idx].proptag >> 16) == untypedtag) {
			current_type = mapi_nameid_names[idx].proptag & 0xFFFF;
			if (current_type != PT_ERROR && current_type != PT_STRING8) {
				return current_type;
			}
		}
	}

	OC_DEBUG(5, "type for property '%x' could not be deduced", untypedtag);
	return 0;
}
