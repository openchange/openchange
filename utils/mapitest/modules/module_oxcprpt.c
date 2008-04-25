/*
   Stand-alone MAPI testsuite

   OpenChange Project - PROPERTY AND STREAM OBJECT PROTOCOL operations

   Copyright (C) Julien Kerihuel 2008

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
#include "utils/mapitest/mapitest.h"
#include "utils/mapitest/proto.h"

/**
   \file module_oxcprpt.c

   \brief Property and Stream Object Protocol test suite
*/

/**
   \details Test the GetProps (0x7) operation

   This function:
	1. Log on the user private mailbox
	2. Retrieve the properties list using GetPropList
	3. Retrieve their associated values using the GetProps
	operation

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_GetProps(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		cValues;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}	
	
	/* Step 2. Retrieve the properties list using GetPropList */
	SPropTagArray = talloc_zero(mt->mem_ctx, struct SPropTagArray);
	retval = GetPropList(&obj_store, SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Call the GetProps operation */
	retval = GetProps(&obj_store, SPropTagArray, &lpProps, &cValues);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetProps", GetLastError());	
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	MAPIFreeBuffer(SPropTagArray);

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the GetPropsAll (0x8) operation

   This function:
	1. Log on the user private mailbox
	2. Retrieve the whole set of properties and values associated
	to the store object

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_GetPropsAll(struct mapitest *mt)
{
	enum MAPISTATUS			retval;
	mapi_object_t			obj_store;
	struct mapi_SPropValue_array	properties_array;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. GetPropsAll operation */
	retval = GetPropsAll(&obj_store, &properties_array);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetPropsAll", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the GetPropList (0x9) operation

   This function:
	1. Log on the user private mailbox
	2. Retrieve the list of properties associated to the store object
	object

   \param mt pointer on the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_GetPropList(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	struct SPropTagArray	*SPropTagArray;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. GetPropList operation */
	SPropTagArray = talloc_zero(mt->mem_ctx, struct SPropTagArray);
	retval = GetPropList(&obj_store, SPropTagArray);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetPropList", GetLastError());
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}


_PUBLIC_ bool mapitest_oxcprpt_SetProps(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	struct SPropValue	*lpProps;
	struct SPropValue	lpProp[1];
	struct SPropTagArray	*SPropTagArray;
	const char		*mailbox = NULL;
	const char		*new_mailbox = NULL;
	uint32_t		cValues;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	mapitest_print(mt, "* Step 1. - Logon Private Mailbox\n", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2: GetProps, retrieve mailbox name */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = GetProps(&obj_store, SPropTagArray, &lpProps, &cValues);
	mapitest_print(mt, "* Step 2. - Retrieve the mailbox name\n", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	MAPIFreeBuffer(SPropTagArray);

	if (cValues && lpProps[0].value.lpszA) {
		mailbox = lpProps[0].value.lpszA;
		mapitest_print(mt, "* Step 2. Mailbox name = %s\n", mailbox);
	} else {
		mapitest_print(mt, MT_ERROR, "* Step 2 - GetProps: No mailbox name\n");
		return false;
	}

	/* Step 2.1: SetProps with new value */
	cValues = 1;
	new_mailbox = talloc_asprintf(mt->mem_ctx, "%s [MAPITEST]", mailbox);
	set_SPropValue_proptag(&lpProp[0], PR_DISPLAY_NAME, 
			       (const void *) new_mailbox);
	retval = SetProps(&obj_store, lpProp, cValues);
	mapitest_print(mt, "* Step 2.1 - Set:   NEW mailbox name: 0x%.8x\n", GetLastError());

	/* Step 2.2: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = GetProps(&obj_store, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(new_mailbox, lpProps[0].value.lpszA, strlen(lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 2.2 - Check: NEW mailbox name - [SUCCESS] (%s)\n", lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 2.2 - Check: NEW mailbox name [FAILURE]\n");
		}
	}
	MAPIFreeBuffer((void *)new_mailbox);

	/* Step 3.1: Reset mailbox to its original value */
	cValues = 1;
	set_SPropValue_proptag(&lpProp[0], PR_DISPLAY_NAME, (const void *)mailbox);
	retval = SetProps(&obj_store, lpProp, cValues);
	mapitest_print(mt, "* Step 3.1 - Set:   OLD mailbox name 0x%.8x\n", GetLastError());
	
	/* Step 3.2: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = GetProps(&obj_store, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(mailbox, lpProps[0].value.lpszA, strlen(lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 3.2 - Check: OLD mailbox name [SUCCESS]\n");
		} else {
			mapitest_print(mt, "* Step 3.2 - Check: OLD mailbox name, [FAILURE]\n");
		}
	}

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}
