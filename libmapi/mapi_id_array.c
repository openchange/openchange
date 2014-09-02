/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2008-2010.

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
   \file mapi_id_array.c

   \brief mapi_id_array support functions
*/


/**
   \details Initialize a mapi_id_array structure

   \param mem_ctx pointer to the talloc context
   \param id pointer to a mapi_id_array structure

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: The mapi_id_array_t is uninitialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa mapi_id_array_release
 */
_PUBLIC_ enum MAPISTATUS mapi_id_array_init(TALLOC_CTX *mem_ctx,
					    mapi_id_array_t *id)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!id, MAPI_E_INVALID_PARAMETER, NULL);

	id->count = 0;
	id->lpContainerList = talloc_zero(mem_ctx, mapi_container_list_t);

	return MAPI_E_SUCCESS;
}


/**
   \details Uninitialize a mapi_id_array structure

   \param id pointer to a mapi_id_array structure

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: The mapi_id_array_t is uninitialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa mapi_id_array_init
 */
_PUBLIC_ enum MAPISTATUS mapi_id_array_release(mapi_id_array_t *id)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!id, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!id->lpContainerList, MAPI_E_INVALID_PARAMETER, NULL);

	id->count = 0;
	talloc_free(id->lpContainerList);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the ContainerList and store it within a int64_t
   array.

   \param mem_ctx allocated talloc pointer
   \param id pointer to a mapi_id_array structure
   \param ContainerList pointer on a pointer of int64_t values

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: The mapi_id_array_t is uninitialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa GetSearchCriteria
 */
_PUBLIC_ enum MAPISTATUS mapi_id_array_get(TALLOC_CTX *mem_ctx,
					   mapi_id_array_t *id, 
					   mapi_id_t **ContainerList)
{
	mapi_container_list_t	*element;
	uint32_t		i = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!id, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!id->lpContainerList, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ContainerList, MAPI_E_INVALID_PARAMETER, NULL);

	*ContainerList = talloc_array(mem_ctx, int64_t, id->count + 1);

	element = id->lpContainerList;
	while (element) {
		ContainerList[0][i] = element->id;
		i++;
		element = element->next;
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Add a container ID to the list given its mapi_object_t

   \param id pointer to a mapi_id_array structure
   \param obj pointer on the mapi object we retrieve the container ID
   from

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: The mapi_id_array_t is uninitialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa mapi_id_array_add_id
 */
_PUBLIC_ enum MAPISTATUS mapi_id_array_add_obj(mapi_id_array_t *id, 
					       mapi_object_t *obj)
{
	mapi_container_list_t	*element;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!id, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!id->lpContainerList, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	element = talloc_zero((TALLOC_CTX *)id->lpContainerList, mapi_container_list_t);
	element->id = mapi_object_get_id(obj);
	DLIST_ADD(id->lpContainerList, element);
	
	id->count++;

	return MAPI_E_SUCCESS;
}


/**
   \details Add a container ID to the list given its container ID

   \param id pointer to a mapi_id_array structure
   \param fid the container ID

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: The mapi_id_array_t is uninitialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa mapi_id_array_add_obj
 */
_PUBLIC_ enum MAPISTATUS mapi_id_array_add_id(mapi_id_array_t *id, mapi_id_t fid)
{
	mapi_container_list_t	*element;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!id, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!id->lpContainerList, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!fid, MAPI_E_INVALID_PARAMETER, NULL);

	element = talloc_zero((TALLOC_CTX *)id->lpContainerList, mapi_container_list_t);
	element->id = fid;
	DLIST_ADD(id->lpContainerList, element);
	
	id->count++;

	return MAPI_E_SUCCESS;
}


/**
   \details Delete a container ID from the list given its container ID

   \param id pointer to a mapi_id_array structure
   \param fid the container ID

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: The mapi_id_array_t is uninitialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa mapi_id_array_add_id
 */
_PUBLIC_ enum MAPISTATUS mapi_id_array_del_id(mapi_id_array_t *id, mapi_id_t fid)
{
	mapi_container_list_t	*element;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!id, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!id->count, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!id->lpContainerList, MAPI_E_NOT_INITIALIZED, NULL);

	element = id->lpContainerList;

	while (element) {
		if (element->id == fid) {
			DLIST_REMOVE(id->lpContainerList, element);
			return MAPI_E_SUCCESS;
		}
		element = element->next;
	}
	return MAPI_E_NOT_FOUND;
}


/**
   \details Delete a container ID from the list given its mapi_object_t

   \param id pointer to a mapi_id_array structure
   \param obj pointer on the mapi object we retrieve the container ID
   from

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: The mapi_id_array_t is uninitialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa mapi_id_array_add_id
 */
_PUBLIC_ enum MAPISTATUS mapi_id_array_del_obj(mapi_id_array_t *id, mapi_object_t *obj)
{
	mapi_container_list_t	*element;
	mapi_id_t		fid;
	
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!id, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!id->count, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!id->lpContainerList, MAPI_E_NOT_INITIALIZED, NULL);

	fid = mapi_object_get_id(obj);
	OPENCHANGE_RETVAL_IF(!fid, MAPI_E_NOT_INITIALIZED, NULL);

	element = id->lpContainerList;

	while (element) {
		if (element->id == fid) {
			DLIST_REMOVE(id->lpContainerList, element);
			return MAPI_E_SUCCESS;
		}
		element = element->next;
	}
	return MAPI_E_NOT_FOUND;
}
