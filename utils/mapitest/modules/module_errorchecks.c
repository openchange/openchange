/*
   Stand-alone MAPI testsuite

   OpenChange Project - Error / sanity-check path tests

   Copyright (C) Brad Hards 2008

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

#include "utils/mapitest/mapitest.h"
#include "utils/mapitest/proto.h"

/**
   \file module_errorchecks.c

   \brief Error / sanity-check path tests

   \note These tests do not show how to use libmapi properly, and should
  not be used as a programming reference.
*/


/**
   \details Verify simplemapi.c functions

   This function:
   -# Tests the sanity checks in GetDefaultPublicFolder
   -# Tests the sanity checks in GetDefaultFolder

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
*/ 
_PUBLIC_ bool mapitest_errorchecks_simplemapi_c(struct mapitest *mt)
{
	enum MAPISTATUS status;
	mapi_object_t 	*obj_store = 0;
	mapi_object_t	*obj_folder = 0;
	mapi_object_t	*obj_message = 0;
	mapi_object_t	tmp;
	int64_t		*folder = 0;
	uint32_t	id = 0x99;
	uint32_t	arg; 		// an all purpose argument...

	status = GetDefaultPublicFolder(obj_store, folder, id);
	if ( ( status != MAPI_E_INVALID_PARAMETER ) || (GetLastError() != MAPI_E_INVALID_PARAMETER) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 1 - MAPI_E_INVALID_PARAMETER", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 1 - MAPI_E_INVALID_PARAMETER");
	}
	obj_store = &tmp;
	status = GetDefaultPublicFolder(obj_store, folder, id);
	if ( ( status != MAPI_E_NOT_FOUND ) || (GetLastError() != MAPI_E_NOT_FOUND) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 2 - MAPI_E_NOT_FOUND", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 2 - MAPI_E_NOT_FOUND");
	}

	obj_store = 0;

	status = GetDefaultFolder(obj_store, folder, id);
	if ( ( status != MAPI_E_INVALID_PARAMETER ) || (GetLastError() != MAPI_E_INVALID_PARAMETER) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 3 - MAPI_E_INVALID_PARAMETER", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 3 - MAPI_E_INVALID_PARAMETER");
	}

	obj_store = &tmp;
	obj_store->private_data = 0;
	status = GetDefaultFolder(obj_store, folder, id);
	if ( (status != MAPI_E_INVALID_PARAMETER) || (GetLastError() != MAPI_E_INVALID_PARAMETER) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 4 - MAPI_E_INVALID_PARAMETER", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 4 - MAPI_E_INVALID_PARAMETER");
	}

	obj_store = 0;

	status = GetFolderItemsCount(obj_folder, 0, 0);
	if ( ( status != MAPI_E_INVALID_PARAMETER ) || (GetLastError() != MAPI_E_INVALID_PARAMETER) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 5 - MAPI_E_INVALID_PARAMETER", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 5 - MAPI_E_INVALID_PARAMETER");
	}

	obj_folder = &tmp;
	status = GetFolderItemsCount(obj_folder, 0, 0);
	if ( ( status != MAPI_E_INVALID_PARAMETER ) || (GetLastError() != MAPI_E_INVALID_PARAMETER) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 6 - MAPI_E_INVALID_PARAMETER", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 6 - MAPI_E_INVALID_PARAMETER");
	}

	status = GetFolderItemsCount(obj_folder, &arg, 0);
	if ( ( status != MAPI_E_INVALID_PARAMETER ) || (GetLastError() != MAPI_E_INVALID_PARAMETER) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 7 - MAPI_E_INVALID_PARAMETER", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 7 - MAPI_E_INVALID_PARAMETER");
	}

	obj_folder = 0;

	/**************************************************************************************
	   Testing AddUserPermission(mapi_object_t *obj_folder, const char *username, enum ACLRIGHTS role)
	*/
	status = AddUserPermission(obj_folder, 0, RightsNone);
	if ( ( status != MAPI_E_INVALID_PARAMETER ) || (GetLastError() != MAPI_E_INVALID_PARAMETER) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 8 - MAPI_E_INVALID_PARAMETER", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 8 - MAPI_E_INVALID_PARAMETER");
	}

	obj_folder = &tmp;
	status = AddUserPermission(obj_folder, 0, RightsNone);
	if ( ( status != MAPI_E_INVALID_PARAMETER ) || (GetLastError() != MAPI_E_INVALID_PARAMETER) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 9 - MAPI_E_INVALID_PARAMETER", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 9 - MAPI_E_INVALID_PARAMETER");
	}

	obj_folder = 0;

	/**************************************************************************************
	  Testing ModifyUserPermission(mapi_object_t *obj_folder, const char *username, enum ACLRIGHTS role)
	*/
	status = ModifyUserPermission(obj_folder, 0, RightsNone);
	if ( ( status != MAPI_E_INVALID_PARAMETER ) || (GetLastError() != MAPI_E_INVALID_PARAMETER) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 10 - MAPI_E_INVALID_PARAMETER", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 10 - MAPI_E_INVALID_PARAMETER");
	}

	obj_folder = &tmp;
	status = ModifyUserPermission(obj_folder, 0, RightsNone);
	if ( ( status != MAPI_E_INVALID_PARAMETER ) || (GetLastError() != MAPI_E_INVALID_PARAMETER) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 11 - MAPI_E_INVALID_PARAMETER", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 11 - MAPI_E_INVALID_PARAMETER");
	}

	obj_folder = 0;

	/**************************************************************************************
	  Testing RemoveUserPermission(mapi_object_t *obj_folder, const char *username)
	*/
	status = RemoveUserPermission(obj_folder, 0);
	if ( ( status != MAPI_E_INVALID_PARAMETER ) || (GetLastError() != MAPI_E_INVALID_PARAMETER) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 12 - MAPI_E_INVALID_PARAMETER", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 12 - MAPI_E_INVALID_PARAMETER");
	}

	obj_folder = &tmp;
	status = RemoveUserPermission(obj_folder, 0);
	if ( ( status != MAPI_E_INVALID_PARAMETER ) || (GetLastError() != MAPI_E_INVALID_PARAMETER) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 13 - MAPI_E_INVALID_PARAMETER", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 13 - MAPI_E_INVALID_PARAMETER");
	}

	obj_folder = 0;

	/**************************************************************************************
	  Testing GetBestBody(mapi_object_t *obj_message, uint8_t *format)
	*/
	status = GetBestBody(obj_message, 0);
	if ( ( status != MAPI_E_INVALID_PARAMETER ) || (GetLastError() != MAPI_E_INVALID_PARAMETER) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 14 - MAPI_E_INVALID_PARAMETER", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 14 - MAPI_E_INVALID_PARAMETER");
	}

	obj_message = &tmp;
	status = GetBestBody(obj_message, 0);
	if ( ( status != MAPI_E_INVALID_PARAMETER ) || (GetLastError() != MAPI_E_INVALID_PARAMETER) ) {
		mapitest_print(mt, "* %-35s: [FAILURE] - 0x%x\n", "Step 15 - MAPI_E_INVALID_PARAMETER", status);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 15 - MAPI_E_INVALID_PARAMETER");
	}

	obj_message = 0;


	return true;
}
