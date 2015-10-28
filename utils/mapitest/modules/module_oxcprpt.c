/*
   Stand-alone MAPI testsuite

   OpenChange Project - PROPERTY AND STREAM OBJECT PROTOCOL operations

   Copyright (C) Julien Kerihuel 2008
   Copyright (C) Brad Hards 2008-2009

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
   \file module_oxcprpt.c

   \brief Property and Stream Object Protocol test suite
*/

/**
   \details Test the GetProps (0x7) operation

   This function:
   -# Log on the user private mailbox
   -# Retrieve the properties list using GetPropList
   -# Retrieve their associated values using the GetProps operation

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
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}	
	
	/* Step 2. Retrieve the properties list using GetPropList */
	SPropTagArray = talloc_zero(mt->mem_ctx, struct SPropTagArray);
	retval = GetPropList(&obj_store, SPropTagArray);
	mapitest_print_retval(mt, "GetPropList");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(SPropTagArray);
		return false;
	}

	/* Step 3. Call the GetProps operation */
	retval = GetProps(&obj_store, 0, SPropTagArray, &lpProps, &cValues);
	mapitest_print_retval(mt, "GetProps");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(SPropTagArray);
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
   -# Log on the user private mailbox
   -# Retrieve the whole set of properties and values associated to the store object

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
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. GetPropsAll operation */
	retval = GetPropsAll(&obj_store, 0, &properties_array);
	mapitest_print_retval(mt, "GetPropsAll");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}
	MAPIFreeBuffer(properties_array.lpProps);

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the GetPropList (0x9) operation

   This function:
   -# Log on the user private mailbox
   -# Retrieve the list of properties associated to the store object object

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
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. GetPropList operation */
	SPropTagArray = talloc_zero(mt->mem_ctx, struct SPropTagArray);
	retval = GetPropList(&obj_store, SPropTagArray);
	mapitest_print_retval(mt, "GetPropList");
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the SetProps (0xa) operation

   This function:
   -# Logon Private mailbox
   -# Use GetProps to retrieve the mailbox name
   -# Change it using SetProps
   -# Reset the mailbox name to its original value

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
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
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval_step_fmt(mt, "1.", "OpenMsgStore", "(%s)", "Logon Private Mailbox");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2: GetProps, retrieve mailbox name */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = GetProps(&obj_store, 0, SPropTagArray, &lpProps, &cValues);
	mapitest_print_retval_step_fmt(mt, "2.", "GetProps", "(%s)", "Retrieve the mailbox name");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}
	MAPIFreeBuffer(SPropTagArray);

	if (cValues && lpProps[0].value.lpszA) {
		mailbox = (const char *) lpProps[0].value.lpszA;
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
	retval = SetProps(&obj_store, 0, lpProp, cValues);
	mapitest_print_retval_step_fmt(mt, "2.1.", "SetProps", "(%s)", "NEW mailbox name");

	/* Step 2.2: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = GetProps(&obj_store, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(new_mailbox, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 2.2 - Check: NEW mailbox name - [SUCCESS] (%s)\n", lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 2.2 - Check: NEW mailbox name [FAILURE]\n");
		}
	}
	MAPIFreeBuffer((void *)new_mailbox);

	/* Step 3.1: Reset mailbox to its original value */
	cValues = 1;
	set_SPropValue_proptag(&lpProp[0], PR_DISPLAY_NAME, (const void *)mailbox);
	retval = SetProps(&obj_store, 0, lpProp, cValues);
	mapitest_print_retval_step_fmt(mt, "3.1.", "SetProps", "(%s)", "OLD mailbox name");
	
	/* Step 3.2: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = GetProps(&obj_store, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(mailbox, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 3.2 - Check: OLD mailbox name [SUCCESS]\n");
		} else {
			mapitest_print(mt, "* Step 3.2 - Check: OLD mailbox name, [FAILURE]\n");
		}
	}

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the DeleteProps (0xb) operation)

   This function:
   -# Opens the mailbox
   -# Create a test folder
   -# Creates a reference email, and sets some properties on it
   -# Delete properties from this message
   -# Checks that properties got deleted
   -# Deletes both email and the test folder

   \todo It would be useful to test the problem return values

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_DeleteProps(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_top_folder;
	mapi_id_t		id_top_folder;
	mapi_object_t		obj_ref_folder;
	mapi_object_t		obj_ref_message;
	const char		*name = NULL;
	const char		*subject = NULL;
	struct SPropValue	lpProp[3];
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		cValues;
	bool			result;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval_step_fmt(mt, "1.", "OpenMsgStore", "(%s)", "Logon Private Mailbox");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}
	mapi_object_init(&obj_top_folder);
	retval = GetDefaultFolder(&obj_store, &id_top_folder, olFolderTopInformationStore);
	retval = OpenFolder(&obj_store, id_top_folder, &obj_top_folder);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2: Create reference folder */
	mapi_object_init(&obj_ref_folder);
	retval = CreateFolder(&obj_top_folder, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
                              OPEN_IF_EXISTS, &obj_ref_folder);
	mapitest_print_retval_step_fmt(mt, "2.", "CreateFolder", "(%s)", "Create the test folder");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3: Create reference message */
	mapi_object_init(&obj_ref_message);
	result = mapitest_common_message_create(mt, &obj_ref_folder, &obj_ref_message, MT_MAIL_SUBJECT);
	mapitest_print_retval_step_fmt(mt, "3.1.", "mapitest_common_message_create", "(%s)", "Create a reference email");
	if (result != true) {
		return false;
	}
	retval = SaveChangesMessage(&obj_ref_folder, &obj_ref_message, KeepOpenReadWrite);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

        name = talloc_asprintf(mt->mem_ctx, "Reference: %s", "display name");
	subject = talloc_asprintf(mt->mem_ctx, "Reference: %s", "subject");
	set_SPropValue_proptag(&lpProp[0], PR_DISPLAY_NAME, (const void *)name);
	set_SPropValue_proptag(&lpProp[1], PR_CONVERSATION_TOPIC, (const void *)subject);
	retval = SetProps(&obj_ref_message, 0, lpProp, 2);
	mapitest_print_retval_step_fmt(mt, "3.2.", "SetProps", "(%s)", "Set email properties");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_CONVERSATION_TOPIC);
	retval = GetProps(&obj_ref_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 4.1. - Check: Reference props set - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 4.1. - Check: Reference props set [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
		}
	}
	if (lpProps[1].value.lpszA) {
		if (!strncmp(subject, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 4.2. - Check: Reference props set - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 4.2. - Check: Reference props set [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
		}
	}
	MAPIFreeBuffer(lpProps);

	/* Step 5. Delete Properties */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_CONVERSATION_TOPIC);
	retval = DeleteProps(&obj_ref_message, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	mapitest_print_retval_step_fmt(mt, "5.", "DeleteProps", "PR_CONVERSATION_TOPIC");

	/* Step 6. Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_CONVERSATION_TOPIC);
	retval = GetProps(&obj_ref_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (get_SPropValue(lpProps, PR_CONVERSATION_TOPIC) == NULL) {
		mapitest_print(mt, "* Step 5.1. - GetProps verifier [SUCCESS]\n");
	} else {
		mapitest_print(mt, "* Step 5.1. - GetProps verifier [FAILURE]:\n");
	}
	MAPIFreeBuffer(lpProps);

	/* Step 7: cleanup folders */
	retval = DeleteFolder(&obj_top_folder, mapi_object_get_id(&obj_ref_folder),
			      DEL_FOLDERS | DEL_MESSAGES | DELETE_HARD_DELETE, NULL);
	mapitest_print_retval_step(mt, "6.", "DeleteFolder", retval);

	/* Release */
	mapi_object_release(&obj_ref_message);
	mapi_object_release(&obj_ref_folder);
	mapi_object_release(&obj_top_folder);
	mapi_object_release(&obj_store);


	return true;
}


/**
   \details Test the CopyProps (0x67) operation

   This function:
   -# Opens the mailbox
   -# Creates a test folder
   -# Creates a reference email, and sets some properties on it
   -# Checks those properties are set correctly
   -# Creates a second email, and sets some (different) properties on it
   -# Checks those properties on the second folder are set correctly
   -# Copies properties from the reference email to the second email (no overwrite)
   -# Checks that properties on both emails are correct
   -# Copies properties again, but with overwrite
   -# Checks that properties on both emails are correct
   -# Moves properties from the original email to the second email (no overwrite)
   -# Checks that properties on both emails are correct
   -# Deletes both emails and the test folder

   \todo It would be useful to test the problem return values

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_CopyProps(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_top_folder;
	mapi_id_t		id_top_folder;
	mapi_object_t		obj_ref_folder;
	mapi_object_t		obj_ref_message;
	const char		*name = NULL;
	const char		*subject = NULL;
	struct SPropValue	lpProp[3];
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		cValues;
	mapi_object_t		obj_target_message;
	const char		*targ_name = NULL;
	const char		*targ_dept = NULL;
	uint16_t		problem_count = 999;
	struct PropertyProblem	*problems = NULL;
	bool			result;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval_step_fmt(mt, "1.", "OpenMsgStore", "(%s)", "Logon Private Mailbox");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}
	mapi_object_init(&obj_top_folder);
	retval = GetDefaultFolder(&obj_store, &id_top_folder, olFolderTopInformationStore);
	retval = OpenFolder(&obj_store, id_top_folder, &obj_top_folder);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2: Create reference folder */
	mapi_object_init(&obj_ref_folder);
        retval = CreateFolder(&obj_top_folder, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
                              OPEN_IF_EXISTS, &obj_ref_folder);
	mapitest_print_retval_step_fmt(mt, "2.", "CreateFolder", "(%s)", "Create the test folder");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3: Create reference message */
	mapi_object_init(&obj_ref_message);
	result = mapitest_common_message_create(mt, &obj_ref_folder, &obj_ref_message, MT_MAIL_SUBJECT);
	mapitest_print_retval_step_fmt(mt, "3.1.", "mapitest_common_message_create", "(%s)", "Create a reference email");
	if (result != true) {
		return false;
	}
	retval = SaveChangesMessage(&obj_ref_folder, &obj_ref_message, KeepOpenReadWrite);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

        name = talloc_asprintf(mt->mem_ctx, "Reference: %s", "display name");
	subject = talloc_asprintf(mt->mem_ctx, "Reference: %s", "subject");
	set_SPropValue_proptag(&lpProp[0], PR_DISPLAY_NAME, (const void *)name);
	set_SPropValue_proptag(&lpProp[1], PR_CONVERSATION_TOPIC, (const void *)subject);
	retval = SetProps(&obj_ref_message, 0, lpProp, 2);
	mapitest_print_retval_step_fmt(mt, "3.2.", "SetProps", "(%s)", "Set email properties");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_CONVERSATION_TOPIC);
	retval = GetProps(&obj_ref_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 4.1. - Check: Reference props set - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 4.1. - Check: Reference props set [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
		}
	}
	if (lpProps[1].value.lpszA) {
		if (!strncmp(subject, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 4.2. - Check: Reference props set - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 4.2. - Check: Reference props set [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
		}
	}

	/* Step 5: Create target message */
	mapi_object_init(&obj_target_message);
	result = mapitest_common_message_create(mt, &obj_ref_folder, &obj_target_message, MT_MAIL_SUBJECT);
	mapitest_print_retval_step_fmt(mt, "5.1.", "mapitest_common_message_create", "(%s)", "Create target email");
	if (result != true) {
		return false;
	}
	retval = SaveChangesMessage(&obj_ref_folder, &obj_target_message, KeepOpenReadWrite);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

        targ_name = talloc_asprintf(mt->mem_ctx, "Target: %s", "display name");
	targ_dept = talloc_asprintf(mt->mem_ctx, "Target: %s", "department");
	set_SPropValue_proptag(&lpProp[0], PR_DISPLAY_NAME, (const void *)targ_name);
	set_SPropValue_proptag(&lpProp[1], PR_DEPARTMENT_NAME, (const void *)targ_dept);
	retval = SetProps(&obj_target_message, 0, lpProp, 2);
	mapitest_print_retval_step_fmt(mt, "5.2.", "SetProps", "(%s)", "set properties on target email");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 6: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_DEPARTMENT_NAME);
	retval = GetProps(&obj_target_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(targ_name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 6A - Check: Reference props set - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 6A - Check: Reference props set [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
		}
	}
	if (lpProps[1].value.lpszA) {
		if (!strncmp(targ_dept, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 6B - Check: Reference props set - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 6B - Check: Reference props set [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
		}
	}



	/* Step 7: Copy properties, no overwrite */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_CONVERSATION_TOPIC);
	retval = CopyProps(&obj_ref_message, &obj_target_message, SPropTagArray, CopyFlagsNoOverwrite,
			   &problem_count, &problems);
	mapitest_print_retval_step_fmt(mt, "7.", "CopyProps", "(%s)", "no overwrite");
	MAPIFreeBuffer(problems);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}
	MAPIFreeBuffer(SPropTagArray);

	/* Step 8: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_CONVERSATION_TOPIC);
	retval = GetProps(&obj_ref_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 8A - Check: Reference props still good - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 8A - Check: Reference props still good [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
		}
	}
	if (lpProps[1].value.lpszA) {
		if (!strncmp(subject, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 8B - Check: Reference props still good - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 8B - Check: Reference props still good [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
		}
	}
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3, PR_DISPLAY_NAME, PR_CONVERSATION_TOPIC, PR_DEPARTMENT_NAME);
	retval = GetProps(&obj_target_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	/* this one shouldn't be overwritten */
	if (lpProps[0].value.lpszA) {
		if (!strncmp(targ_name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 8C - Check: Reference props copy - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 8C - Check: Reference props copy [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
		}
	}
	/* this one should be copied */
	if (lpProps[1].value.lpszA) {
		if (!strncmp(subject, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 8D - Check: Reference props copy - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 8D - Check: Reference props copy [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
		}
	}
	/* this one should be unchanged */
	if (lpProps[2].value.lpszA) {
		if (!strncmp(targ_dept, (const char *) lpProps[2].value.lpszA, strlen((const char *) lpProps[2].value.lpszA))) {
			mapitest_print(mt, "* Step 8E - Check: Reference props copy - [SUCCESS] (%s)\n",
				       lpProps[2].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 8E - Check: Reference props copy [FAILURE] (%s)\n",
				       lpProps[2].value.lpszA);
		}
	}

	/* Step 9: Copy properties, with overwrite */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_CONVERSATION_TOPIC);
	retval = CopyProps(&obj_ref_message, &obj_target_message, SPropTagArray, 0x0,
			   &problem_count, &problems);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(problems);
	mapitest_print_retval_step_fmt(mt, "9.", "CopyProps", "(%s)", "with overwrite");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 10: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_CONVERSATION_TOPIC);
	retval = GetProps(&obj_ref_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 10A - Check: Reference props still good - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 10A - Check: Reference props still good [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
		}
	}
	if (lpProps[1].value.lpszA) {
		if (!strncmp(subject, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 10B - Check: Reference props still good - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 10B - Check: Reference props still good [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
		}
	}
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3, PR_DISPLAY_NAME, PR_CONVERSATION_TOPIC, PR_DEPARTMENT_NAME);
	retval = GetProps(&obj_target_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	/* this one should now be overwritten */
	if (lpProps[0].value.lpszA) {
		if (!strncmp(name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 10C - Check: Reference props copy - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 10C - Check: Reference props copy [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
		}
	}
	/* this one should be copied */
	if (lpProps[1].value.lpszA) {
		if (!strncmp(subject, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 10D - Check: Reference props copy - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 10D - Check: Reference props copy [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
		}
	}
	/* this one should be unchanged */
	if (lpProps[2].value.lpszA) {
		if (!strncmp(targ_dept, (const char *) lpProps[2].value.lpszA, strlen((const char *) lpProps[2].value.lpszA))) {
			mapitest_print(mt, "* Step 10E - Check: Reference props copy - [SUCCESS] (%s)\n",
				       lpProps[2].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 10E - Check: Reference props copy [FAILURE] (%s)\n",
				       lpProps[2].value.lpszA);
		}
	}

	if ( mt->info.rgwServerVersion[0] >= Exchange2010SP0Version ) {
		/* the combination of CopyFlagsNoOverwrite|CopyFlagsMove isn't support in Exchange2010 */
		goto cleanup;
	}
	    
	/* Step 11: Move properties, no overwrite */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_CONVERSATION_TOPIC);
	retval = CopyProps(&obj_ref_message, &obj_target_message, SPropTagArray, CopyFlagsNoOverwrite|CopyFlagsMove,
			   &problem_count, &problems);
	MAPIFreeBuffer(SPropTagArray);
	if (problem_count) {
		MAPIFreeBuffer(problems);
	}
	mapitest_print_retval_step_fmt(mt, "11.", "CopyProps", "(%s)", "move");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 12: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_CONVERSATION_TOPIC);
	retval = GetProps(&obj_ref_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (cValues == 2) {
		mapitest_print(mt, "* Step 12A - Properties removed [SUCCESS]\n");
	} else {
		mapitest_print(mt, "* Step 12A - Properties removed [FAILURE]\n");
	}
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3, PR_DISPLAY_NAME, PR_CONVERSATION_TOPIC, PR_DEPARTMENT_NAME);
	retval = GetProps(&obj_target_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 12B - Check: Reference props move - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 12B - Check: Reference props move [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
		}
	}
	if (lpProps[1].value.lpszA) {
		if (!strncmp(subject, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 12C - Check: Reference props move - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 12C - Check: Reference props move [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
		}
	}
	if (lpProps[2].value.lpszA) {
		if (!strncmp(targ_dept, (const char *) lpProps[2].value.lpszA, strlen((const char *) lpProps[2].value.lpszA))) {
			mapitest_print(mt, "* Step 12D - Check: Reference props move - [SUCCESS] (%s)\n",
				       lpProps[2].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 12D - Check: Reference props move [FAILURE] (%s)\n",
				       lpProps[2].value.lpszA);
		}
	}

	cleanup:

	/* Cleanup reference strings */
	MAPIFreeBuffer((void *)subject);
	MAPIFreeBuffer((void *)name);
	MAPIFreeBuffer((void *)targ_name);
	MAPIFreeBuffer((void *)targ_dept);

	/* Step 13: cleanup folders */
	retval = DeleteFolder(&obj_top_folder, mapi_object_get_id(&obj_ref_folder),
			      DEL_FOLDERS | DEL_MESSAGES | DELETE_HARD_DELETE, NULL);
	mapitest_print_retval_step(mt, "13.1.", "DeleteFolder", retval);

	/* Release */
	mapi_object_release(&obj_ref_message);
	mapi_object_release(&obj_ref_folder);
	mapi_object_release(&obj_top_folder);
	mapi_object_release(&obj_store);


	return true;
}



/**
   \details Test Stream operations. This test uses related stream
   operations: OpenStream (0x2b), SetStreamSize (0x2f), WriteStream
   (0x2d), CommitStream (0x5d), ReadStream (0x2c), SeekStream (0x2e),
   LockRegionStream (0x5b), UnlockRegionStream (0x5c), CloneStream (0x3b)
   
   This function:
   -# Logon 
   -# Open Outbox folder
   -# Create message
   -# Create attachment and set properties
   -# Open the stream
   -# Set the stream size
   -# Write into the stream
   -# Commit the stream
   -# Save the message
   -# Get stream size and compare values
   -# Open the stream again with different permissions
   -# Read the stream and compare buffers
   -# SeekStream at 0x1000 from the end of the stream
   -# Read the 0x1000 last bytes and check if it matches
   -# Lock a range of the stream
   -# TODO: test if the locking works
   -# Unlock a range of the stream
   -# Clone the stream
   -# Delete the message;

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise -1
 */
_PUBLIC_ bool mapitest_oxcprpt_Stream(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_object_t		obj_attach;
	mapi_object_t		obj_stream;
	mapi_object_t		obj_stream_clone;
	mapi_id_t		id_folder;
	DATA_BLOB		data;
	struct SPropValue	attach[3];
	char			*stream = NULL;
	char			*out_stream = NULL;
	uint32_t		stream_len = 0x32146;
	unsigned char		buf[MT_STREAM_MAX_SIZE];
	uint32_t		StreamSize = 0;
	uint16_t		read_size = 0;
	uint16_t		write_len = 0;
	uint32_t		len = 0;
	uint32_t		offset = 0;
	mapi_id_t		id_msgs[1];
	uint32_t		i;
	uint64_t		NewPosition;

	stream = mapitest_common_genblob(mt->mem_ctx, stream_len);
	if (stream == NULL) {
		return false;
	}

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Inbox folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderInbox);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create the message */
	mapi_object_init(&obj_message);
	ret = mapitest_common_message_create(mt, &obj_folder, &obj_message, MT_MAIL_SUBJECT);
	mapitest_print_retval(mt, "Message Creation");
	if (ret != true) {
		return false;
	}

	/* Step 4. Create the attachment */
	mapi_object_init(&obj_attach);
	retval = CreateAttach(&obj_message, &obj_attach);
	mapitest_print_retval(mt, "CreateAttach");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	attach[0].ulPropTag = PR_ATTACH_METHOD;
	attach[0].value.l = ATTACH_BY_VALUE;
	attach[1].ulPropTag = PR_RENDERING_POSITION;
	attach[1].value.l = 0;
	attach[2].ulPropTag = PR_ATTACH_FILENAME;
	attach[2].value.lpszA = (uint8_t *) MT_MAIL_ATTACH;

	retval = SetProps(&obj_attach, 0, attach, 3);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 5. Open the stream */
	mapi_object_init(&obj_stream);
	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 2, &obj_stream);
	mapitest_print_retval(mt, "OpenStream");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 6. Set the stream size */
	retval = SetStreamSize(&obj_stream, (uint64_t) stream_len);
	mapitest_print_retval(mt, "SetStreamSize");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 7. Write the stream */
	write_len = 0;

	StreamSize = stream_len;

	for (offset = 0, len = MT_STREAM_MAX_SIZE, i = 0; StreamSize; i++) {
		data.length = len;
		data.data = (uint8_t *)stream + offset;
		retval = WriteStream(&obj_stream, &data, &write_len);
		mapitest_print_retval_fmt(mt, "WriteStream", "[%d] (0x%x bytes written)", i, write_len);
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
			break;
		}

		StreamSize -= write_len;
		if (StreamSize > MT_STREAM_MAX_SIZE) {
			offset += MT_STREAM_MAX_SIZE;
		} else {
			offset += write_len;
			len = StreamSize;
		}
	}

 	/* Step 8. Commit the stream */
	retval = CommitStream(&obj_stream);
	mapitest_print_retval(mt, "CommitStream");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 9. Save the attachment */
	retval = SaveChangesAttachment(&obj_message, &obj_attach, KeepOpenReadOnly);
	mapitest_print_retval(mt, "SaveChangesAttachment");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
	mapitest_print_retval(mt, "SaveChangesMessage");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 10. Get stream size */
	retval = GetStreamSize(&obj_stream, &StreamSize);
	mapitest_print_retval(mt, "GetStreamSize");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}
	mapitest_print(mt, "* %-35s: %s\n", "StreamSize comparison", 
		       (StreamSize == stream_len) ? "[PASSED]" : "[FAILURE]");

	/* Step 11. Read the stream */
	mapi_object_release(&obj_stream);
	mapi_object_init(&obj_stream);

	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 0, &obj_stream);
	mapitest_print_retval(mt, "OpenStream");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	offset = 0;
	out_stream = talloc_size(mt->mem_ctx, StreamSize + 1);
	do {
		retval = ReadStream(&obj_stream, buf, MT_STREAM_MAX_SIZE, &read_size);
		mapitest_print_retval_fmt(mt, "ReadStream", "(0x%x bytes read)", read_size);
		memcpy(out_stream + offset, buf, read_size);
		offset += read_size;
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
			break;
		}
	} while (read_size && (offset != StreamSize));
	out_stream[offset] = '\0';

	if (offset) {
		if (!strcmp(stream, out_stream)) {
			mapitest_print(mt, "* %-35s: [IN,OUT] stream [PASSED]\n", "Comparison");
		} else {
			mapitest_print(mt, "* %-35s: [IN,OUT] stream [FAILURE]\n", "Comparison");

		}
	}

	/* Step 12. SeekStream from the end of the stream */
	retval = SeekStream(&obj_stream, 0x2, (uint64_t) -0x1000, &NewPosition);
	mapitest_print_retval(mt, "SeekStream");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}


	talloc_free(out_stream);
	out_stream = talloc_size(mt->mem_ctx, 0x1001);
	retval = ReadStream(&obj_stream, (uint8_t *)out_stream, 0x1000, &read_size);
	out_stream[read_size] = '\0';
	mapitest_print_retval_fmt(mt, "ReadStream", "(0x%x bytes read)", read_size);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	if (read_size && !strcmp(out_stream, stream + StreamSize - read_size)) {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Comparison");
	} else {
		mapitest_print(mt, "* %-35s: [FAILURE]\n", "Comparison");
	}

	mapi_object_init(&obj_stream_clone);
	if (mt->info.rgwServerVersion[0] >= Exchange2010SP0Version) {
		mapitest_print(mt, "* SKIPPING test for LockRegionStream, UnlockRegionStream and CloneStream\n");
	} else {
		/* Step 13. Lock a region */
		retval = LockRegionStream(&obj_stream, 0x2000, 0x1000, 0x0);
		mapitest_print_retval(mt, "LockRegionStream");
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
		}

		/* TODO: Step 14. Test the locking */


		/* Step 15. Unlock the region */
		retval = UnlockRegionStream(&obj_stream, 0x2000, 0x1000, 0x0);
		mapitest_print_retval(mt, "UnlockRegionStream");
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
		}

		/* Step 16. Clone the stream */
		retval = CloneStream(&obj_stream, &obj_stream_clone);
		mapitest_print_retval(mt, "CloneStream");
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
		}

		/* Step 17. Test the clone */
		retval = SeekStream(&obj_stream_clone, 0x0, 0, &NewPosition);
		mapitest_print_retval(mt, "SeekStream");
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
		}
		retval = ReadStream(&obj_stream_clone, buf, MT_STREAM_MAX_SIZE, &read_size);
		mapitest_print_retval_fmt(mt, "ReadStream", "(0x%x bytes read)", read_size);
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
		}
	}

	/* Delete the message */
	errno = 0;
	id_msgs[0] = mapi_object_get_id(&obj_message);
	retval = DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print_retval(mt, "DeleteMessage");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Release */
	mapi_object_release(&obj_stream_clone);
	mapi_object_release(&obj_stream);
	mapi_object_release(&obj_attach);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	talloc_free(stream);
	talloc_free(out_stream);

	return ret;
}


/**
   \details Test the CopyToStream (0x3a) operation

   This function:
   -# Logon the mailbox
   -# Open the inbox folder
   -# Create a sample messages with an attachment
   -# Create 2 streams
   -# Fill the first stream with random data
   -# Seek stream positions to the beginning
   -# CopyToStream data from first stream to the second stream
   -# Read dst stream and compare with src stream
   -# Delete the message
   
   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_CopyToStream(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_object_t		obj_attach;
	mapi_object_t		obj_attach2;
	mapi_object_t		obj_stream;
	mapi_object_t		obj_stream2;
	mapi_id_t		id_folder;
	mapi_id_t		id_msgs[1];
	struct SPropValue	attach[3];
	DATA_BLOB		data;
	char			*stream = NULL;
	char			*dst_stream = NULL;
	uint32_t		stream_len = 0x32146;
	unsigned char		buf[MT_STREAM_MAX_SIZE];
	uint32_t		StreamSize = 0;
	uint16_t		write_len = 0;
	uint16_t		read_size = 0;
	uint32_t		len = 0;
	uint32_t		offset = 0;
	uint32_t		i;
	uint64_t		ReadByteCount = 0;
	uint64_t		WrittenByteCount = 0;
	uint64_t		NewPosition = 0;

	stream = mapitest_common_genblob(mt->mem_ctx, stream_len);
	if (stream == NULL) {
		return false;
	}

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Inbox folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderInbox);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create the message */
	mapi_object_init(&obj_message);
	ret = mapitest_common_message_create(mt, &obj_folder, &obj_message, MT_MAIL_SUBJECT);
	mapitest_print_retval(mt, "Message Creation");
	if (ret != true) {
		return false;
	}

	/* Step 4. Create the first attachment */
	mapi_object_init(&obj_attach);
	retval = CreateAttach(&obj_message, &obj_attach);
	mapitest_print_retval(mt, "CreateAttach");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	attach[0].ulPropTag = PR_ATTACH_METHOD;
	attach[0].value.l = ATTACH_BY_VALUE;
	attach[1].ulPropTag = PR_RENDERING_POSITION;
	attach[1].value.l = 0;
	attach[2].ulPropTag = PR_ATTACH_FILENAME;
	attach[2].value.lpszA = (uint8_t *) MT_MAIL_ATTACH;

	retval = SetProps(&obj_attach, 0, attach, 3);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 5. Open the stream */
	mapi_object_init(&obj_stream);
	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 2, &obj_stream);
	mapitest_print_retval(mt, "OpenStream");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 6. Set the stream size */
	retval = SetStreamSize(&obj_stream, (uint64_t) stream_len);
	mapitest_print_retval(mt, "SetStreamSize");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 7. Write the stream */
	write_len = 0;

	StreamSize = stream_len;

	for (offset = 0, len = MT_STREAM_MAX_SIZE, i = 0; StreamSize; i++) {
		data.length = len;
		data.data = (uint8_t *)stream + offset;
		retval = WriteStream(&obj_stream, &data, &write_len);
		mapitest_print_retval_fmt(mt, "WriteStream", "[%d] (0x%x bytes written)", i, write_len);

		StreamSize -= write_len;
		if (StreamSize > MT_STREAM_MAX_SIZE) {
			offset += MT_STREAM_MAX_SIZE;
		} else {
			offset += write_len;
			len = StreamSize;
		}
	}

 	/* Step 8. Commit the stream */
	retval = CommitStream(&obj_stream);
	mapitest_print_retval(mt, "CommitStream");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 9. Save the attachment */
	retval = SaveChangesAttachment(&obj_message, &obj_attach, KeepOpenReadOnly);
	mapitest_print_retval(mt, "SaveChangesAttachment");

	/* Step 10. Create the second attachment */
	mapi_object_init(&obj_attach2);
	retval = CreateAttach(&obj_message, &obj_attach2);
	mapitest_print_retval(mt, "CreateAttach");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	attach[0].ulPropTag = PR_ATTACH_METHOD;
	attach[0].value.l = ATTACH_BY_VALUE;
	attach[1].ulPropTag = PR_RENDERING_POSITION;
	attach[1].value.l = 0;
	attach[2].ulPropTag = PR_ATTACH_FILENAME;
	attach[2].value.lpszA = (uint8_t *) MT_MAIL_ATTACH2;

	retval = SetProps(&obj_attach2, 0, attach, 3);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 11. Open the dst stream */
	mapi_object_init(&obj_stream2);
	retval = OpenStream(&obj_attach2, PR_ATTACH_DATA_BIN, 2, &obj_stream2);
	mapitest_print_retval(mt, "OpenStream");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 12. Get src stream size */
	retval = GetStreamSize(&obj_stream, &StreamSize);
	mapitest_print_retval_fmt(mt, "GetStreamSize", "(%s: 0x%x)", "Src", StreamSize);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 13. Reset streams positions to the beginning */
	retval = SeekStream(&obj_stream, 0, 0, &NewPosition);
	mapitest_print_retval_fmt(mt, "SeekStream", "(%s)", "Src");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	retval = SeekStream(&obj_stream2, 0, 0, &NewPosition);
	mapitest_print_retval_fmt(mt, "SeekStream", "(%s)", "Dst");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 14. Copy src to dst stream */
	retval = CopyToStream(&obj_stream, &obj_stream2, StreamSize, &ReadByteCount, &WrittenByteCount);
	mapitest_print_retval(mt, "CopyToStream");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 14. Save the attachment */
	retval = SaveChangesAttachment(&obj_message, &obj_attach2, KeepOpenReadOnly);
	mapitest_print_retval(mt, "SaveChangesAttachment");
	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
	mapitest_print_retval(mt, "SaveChangesMessage");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 15. Compare values */
	mapitest_print(mt, "* %-35s: 0x%llx - 0x%llx %s\n", "Read/Write bytes comparison",
		       ReadByteCount, WrittenByteCount, 
		       (ReadByteCount == WrittenByteCount) ? "[SUCCESS]" : "[FAILURE]");


	/* Step 16. Get dst stream size */
	retval = GetStreamSize(&obj_stream2, &StreamSize);
	mapitest_print_retval_fmt(mt, "GetStreamSize", "(%s: 0x%x)", "Dst", StreamSize);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	retval = SeekStream(&obj_stream2, 0, 0, &NewPosition);
	mapitest_print_retval_fmt(mt, "SeekStream", "(%s)", "Dst");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 17. Read the dst stream */
	offset = 0;
	dst_stream = talloc_size(mt->mem_ctx, StreamSize + 1);
	do {
		retval = ReadStream(&obj_stream2, buf, MT_STREAM_MAX_SIZE, &read_size);
		mapitest_print_retval_fmt(mt, "ReadStream", "(0x%x bytes read)", read_size);
		memcpy(dst_stream + offset, buf, read_size);
		offset += read_size;
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
			break;
		}
	} while (read_size || offset != StreamSize);
	dst_stream[offset] = '\0';

	/* Step 18. Compare streams */
	if (!strcmp(stream, dst_stream)) {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Comparison");
	} else {
		mapitest_print(mt, "* %-35s: [FAILURE]\n", "Comparison");
	}
	

	/* Step 19. Delete Message */
	errno = 0;
	id_msgs[0] = mapi_object_get_id(&obj_message);
	retval = DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print_retval(mt, "DeleteMessage");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Release */
	mapi_object_release(&obj_stream2);
	mapi_object_release(&obj_stream);
	mapi_object_release(&obj_attach2);
	mapi_object_release(&obj_attach);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	talloc_free(stream);
	talloc_free(dst_stream);

	return ret;
}


/**
   \details Test the CopyTo (0x39) operation

   This function:
   -# Opens the mailbox
   -# Creates a test folder
   -# Creates a reference email, and sets some properties on it
   -# Checks those properties are set correctly
   -# Creates a second email, and sets some (different) properties on it
   -# Checks those properties on the second folder are set correctly
   -# Copies properties from the reference email to the second email (no overwrite)
   -# Checks that properties on both emails are correct
   -# Copies properties again, but with overwrite
   -# Checks that properties on both emails are correct
   -# Moves properties from the original email to the second email (no overwrite)
   -# Checks that properties on both emails are correct
   -# Creates an attachment (with properties) on the reference email
   -# Creates an attachment (with different properties) on the target email
   -# Copies the properties on the reference email to the target
   -# Checks the properties on both attachments are correct
   -# Creates another folder
   -# Copies properties from the test folder to the new folder
   -# Checks that the properties on both folders are correct
   -# Deletes both emails and the test folders

   \todo It would be useful to test the problem return values

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_CopyTo(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_top_folder;
	mapi_id_t		id_top_folder;
	mapi_object_t		obj_ref_folder;
	mapi_object_t		obj_targ_folder;
	mapi_object_t		obj_ref_message;
	mapi_object_t		obj_target_message;
	mapi_object_t		obj_ref_attach;
	mapi_object_t		obj_targ_attach;
	const char		*name = NULL;
	const char		*subject = NULL;
	const char		*dept = NULL;
	struct SPropValue	lpProp[3];
	struct SPropTagArray	*exclude;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		cValues;
	const char		*targ_name = NULL;
	const char		*targ_dept = NULL;
	uint16_t		problem_count = 999;
	struct PropertyProblem  *problems = NULL;
	bool			result;
	bool			ret = true;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_top_folder);
	mapi_object_init(&obj_ref_folder);
	mapi_object_init(&obj_targ_folder);
	mapi_object_init(&obj_ref_message);
	mapi_object_init(&obj_target_message);
	mapi_object_init(&obj_ref_attach);
	mapi_object_init(&obj_targ_attach);
	
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}
	retval = GetDefaultFolder(&obj_store, &id_top_folder, olFolderTopInformationStore);
	mapitest_print_retval(mt, "GetDefaultFolder");

	retval = OpenFolder(&obj_store, id_top_folder, &obj_top_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 2: Create reference folder */
        retval = CreateFolder(&obj_top_folder, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
                              OPEN_IF_EXISTS, &obj_ref_folder);
	mapitest_print_retval_fmt(mt, "CreateFolder", "(Create test folder)");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	lpProp[0].ulPropTag = PR_CONTAINER_CLASS;
	lpProp[0].value.lpszA = (uint8_t *) "IPF.Note";
	retval = SetProps(&obj_ref_folder, 0, lpProp, 1);
	mapitest_print_retval(mt, "SetProps");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3: Create reference message */
	result = mapitest_common_message_create(mt, &obj_ref_folder, &obj_ref_message, MT_MAIL_SUBJECT);
	mapitest_print_retval(mt, "mapitest_common_message_create");
	if (result != true) {
		ret = false;
		goto cleanup;
	}
	retval = SaveChangesMessage(&obj_ref_folder, &obj_ref_message, KeepOpenReadWrite);
	mapitest_print_retval(mt, "SaveChangesMessage");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

        name = talloc_asprintf(mt->mem_ctx, "Reference: %s", "display name");
	subject = talloc_asprintf(mt->mem_ctx, "Reference: %s", "subject");
	dept = talloc_asprintf(mt->mem_ctx, "Reference: %s", "dept");
	set_SPropValue_proptag(&lpProp[0], PR_DISPLAY_NAME, (const void *)name);
	set_SPropValue_proptag(&lpProp[1], PR_SUBJECT, (const void *)subject);
	set_SPropValue_proptag(&lpProp[2], PR_DEPARTMENT_NAME, (const void *)dept);
	retval = SetProps(&obj_ref_message, 0, lpProp, 3);
	mapitest_print_retval(mt, "SetProps");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 4: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3, PR_DISPLAY_NAME, PR_SUBJECT,
					  PR_DEPARTMENT_NAME);
	retval = GetProps(&obj_ref_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 4A - Check: Reference props set - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 4A - Check: Reference props set [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}
	if (lpProps[1].value.lpszA) {
		if (!strncmp(subject, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 4B - Check: Reference props set - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 4B - Check: Reference props set [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}
	if (lpProps[2].value.lpszA) {
		if (!strncmp(dept,  (const char *) lpProps[2].value.lpszA, strlen( (const char *) lpProps[2].value.lpszA))) {
			mapitest_print(mt, "* Step 4C - Check: Reference props set - [SUCCESS] (%s)\n",
				       lpProps[2].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 4C - Check: Reference props set [FAILURE] (%s)\n",
				       lpProps[2].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}

	/* Step 5: Create target message */
	result = mapitest_common_message_create(mt, &obj_ref_folder, &obj_target_message, MT_MAIL_SUBJECT);
	mapitest_print_retval(mt, "mapitest_common_message_create");
	if (result != true) {
		ret = false;
		goto cleanup;
	}

	retval = SaveChangesMessage(&obj_ref_folder, &obj_target_message, KeepOpenReadWrite);
	mapitest_print_retval_clean(mt, "5A. SaveChangesMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_SUBJECT);
	retval = DeleteProps(&obj_target_message, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	mapitest_print_retval_clean(mt, "5B. DeleteProps - PR_SUBJECT", retval);
	
        targ_name = talloc_asprintf(mt->mem_ctx, "Target: %s", "display name");
	targ_dept = talloc_asprintf(mt->mem_ctx, "Target: %s", "department");
	set_SPropValue_proptag(&lpProp[0], PR_DISPLAY_NAME, (const void *)targ_name);
	set_SPropValue_proptag(&lpProp[1], PR_DEPARTMENT_NAME, (const void *)targ_dept);
	retval = SetProps(&obj_target_message, 0, lpProp, 2);
	mapitest_print_retval_clean(mt, "SetProps", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 6: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_DEPARTMENT_NAME);
	retval = GetProps(&obj_target_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(targ_name,  (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 6A - Check: Reference props set - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 6A - Check: Reference props set [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
			ret = false;
			goto cleanup;

		}
	}
	if (lpProps[1].value.lpszA) {
		if (!strncmp(targ_dept, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 6B - Check: Reference props set - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 6B - Check: Reference props set [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
			ret = false;
			goto cleanup;

		}
	}


	/* Step 7: Copy properties, no overwrite */
	exclude = set_SPropTagArray(mt->mem_ctx, 0x0);
	retval = CopyTo(&obj_ref_message, &obj_target_message, exclude, CopyFlagsNoOverwrite,
			   &problem_count, &problems);
	MAPIFreeBuffer(exclude);
	MAPIFreeBuffer(problems);
	mapitest_print_retval_fmt(mt, "CopyTo", "(no overwrite)");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 8: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_SUBJECT);
	retval = GetProps(&obj_ref_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 8A - Check: Reference props still good - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 8A - Check: Reference props still good [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}
	if (lpProps[1].value.lpszA) {
		if (!strncmp(subject, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 8B - Check: Reference props still good - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 8B - Check: Reference props still good [FAILURE] (%s, %s)\n",
				       subject, lpProps[1].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3, PR_DISPLAY_NAME, PR_SUBJECT, PR_DEPARTMENT_NAME);
	retval = GetProps(&obj_target_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	/* this one shouldn't be overwritten */
	if (lpProps[0].value.lpszA) {
		if (!strncmp(targ_name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 8C - Check: Reference props copy - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 8C - Check: Reference props copy [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}
	/* this one should be copied */
	if (lpProps[1].value.lpszA) {
		if (!strncmp(subject, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 8D - Check: Reference props copy - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 8D - Check: Reference props copy [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}
	/* this one should be unchanged */
	if (lpProps[2].value.lpszA) {
		if (!strncmp(targ_dept, (const char *) lpProps[2].value.lpszA, strlen((const char *) lpProps[2].value.lpszA))) {
			mapitest_print(mt, "* Step 8E - Check: Reference props copy - [SUCCESS] (%s)\n",
				       lpProps[2].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 8E - Check: Reference props copy [FAILURE] (%s)\n",
				       lpProps[2].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}

	/* Step 9: Copy properties, with overwrite */
	exclude = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DEPARTMENT_NAME);
	retval = CopyTo(&obj_ref_message, &obj_target_message, exclude, 0x0,
			&problem_count, &problems);
	MAPIFreeBuffer(exclude);
	MAPIFreeBuffer(problems);
	mapitest_print_retval_fmt(mt, "CopyTo", "(with overwrite)");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 10: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_SUBJECT);
	retval = GetProps(&obj_ref_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 10A - Check: Reference props still good - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 10A - Check: Reference props still good [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}
	if (lpProps[1].value.lpszA) {
		if (!strncmp(subject, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 10B - Check: Reference props still good - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 10B - Check: Reference props still good [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3, PR_DISPLAY_NAME, PR_SUBJECT, PR_DEPARTMENT_NAME);
	retval = GetProps(&obj_target_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	/* this one should now be overwritten */
	if (lpProps[0].value.lpszA) {
		if (!strncmp(name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 10C - Check: Reference props copy - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 10C - Check: Reference props copy [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}
	/* this one should be copied */
	if (lpProps[1].value.lpszA) {
		if (!strncmp(subject, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 10D - Check: Reference props copy - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 10D - Check: Reference props copy [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}
	/* this one should be unchanged */
	if (lpProps[2].value.lpszA) {
		if (!strncmp(targ_dept, (const char *) lpProps[2].value.lpszA, strlen((const char *) lpProps[2].value.lpszA))) {
			mapitest_print(mt, "* Step 10E - Check: Reference props copy - [SUCCESS] (%s)\n",
				       lpProps[2].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 10E - Check: Reference props copy [FAILURE] (%s)\n",
				       lpProps[2].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}

	/* Step 11: Move properties, with overwrite */
	exclude = set_SPropTagArray(mt->mem_ctx, 0x0);
	retval = CopyTo(&obj_ref_message, &obj_target_message, exclude, CopyFlagsMove,
			   &problem_count, &problems);
	MAPIFreeBuffer(exclude);
	MAPIFreeBuffer(problems);
	mapitest_print_retval_clean(mt, "* Step 11 - CopyTo (move)", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 12: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_SUBJECT);
	retval = GetProps(&obj_ref_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (cValues == 2) {
		mapitest_print(mt, "* Step 12A - Properties removed [SUCCESS]\n");
	} else {
		mapitest_print(mt, "* Step 12A - Properties removed [FAILURE]\n");
		ret = false;
		goto cleanup;
	}
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3, PR_DISPLAY_NAME, PR_SUBJECT, PR_DEPARTMENT_NAME);
	retval = GetProps(&obj_target_message, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 12B - Check: Reference props move - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 12B - Check: Reference props move [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}
	if (lpProps[1].value.lpszA) {
		if (!strncmp(subject, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 12C - Check: Reference props move - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 12C - Check: Reference props move [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}
	if (lpProps[2].value.lpszA) {
		if (!strncmp(dept, (const char *) lpProps[2].value.lpszA, strlen((const char *) lpProps[2].value.lpszA))) {
			mapitest_print(mt, "* Step 12D - Check: Reference props move - [SUCCESS] (%s)\n",
				       lpProps[2].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 12D - Check: Reference props move [FAILURE] (%s)\n",
				       lpProps[2].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}

	/* Step 13: Create attachment on reference email, and set properties */
	retval = CreateAttach(&obj_ref_message, &obj_ref_attach);
	mapitest_print_retval(mt, "CreateAttach");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	lpProp[0].ulPropTag = PR_ATTACH_METHOD;
	lpProp[0].value.l = ATTACH_BY_VALUE;
	lpProp[1].ulPropTag = PR_RENDERING_POSITION;
	lpProp[1].value.l = 0;
	lpProp[2].ulPropTag = PR_ATTACH_FILENAME;
	lpProp[2].value.lpszA = (uint8_t *) MT_MAIL_ATTACH;
	retval = SetProps(&obj_ref_attach, 0, lpProp, 3);
	mapitest_print_retval(mt, "SetProps");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	SaveChangesAttachment(&obj_ref_message, &obj_ref_attach, KeepOpenReadWrite);
	mapitest_print_retval(mt, "SaveChangesAttachment");

	/* Step 14: Create attachment on target email */
	retval = CreateAttach(&obj_target_message, &obj_targ_attach);
	mapitest_print_retval(mt, "CreateAttach");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	lpProp[0].ulPropTag = PR_ATTACH_METHOD;
	lpProp[0].value.l = ATTACH_BY_VALUE;
	lpProp[1].ulPropTag = PR_RENDERING_POSITION;
	lpProp[1].value.l = 0;
	lpProp[2].ulPropTag = PR_ATTACH_FILENAME;
	lpProp[2].value.lpszA = (uint8_t *) MT_MAIL_ATTACH2;
	retval = SetProps(&obj_targ_attach, 0, lpProp, 3);
	mapitest_print_retval(mt, "SetProps");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	retval = SaveChangesAttachment(&obj_target_message, &obj_targ_attach, KeepOpenReadWrite);
	mapitest_print_retval_clean(mt, "SaveChangesAttachment", retval);

	/* Step 15: Copy props from reference email attachment to target email attachment */
	exclude = set_SPropTagArray(mt->mem_ctx, 0x0);
	retval = CopyTo(&obj_ref_attach, &obj_targ_attach, exclude, 0x0, &problem_count, &problems);
	MAPIFreeBuffer(exclude);
	MAPIFreeBuffer(problems);
	mapitest_print_retval_fmt(mt, "CopyTo", "(attachments)");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	retval = SaveChangesAttachment(&obj_target_message, &obj_targ_attach, KeepOpenReadWrite);
	mapitest_print_retval_clean(mt, "SaveChangesAttachment 2", retval);

	/* Step 16: Check properties on both attachments are correct */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_ATTACH_FILENAME);
	retval = GetProps(&obj_ref_attach, 0, SPropTagArray, &lpProps, &cValues);
	mapitest_print_retval(mt, "GetProps");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(MT_MAIL_ATTACH, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 16B - Check: Reference attachment props - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 16B - Check: Reference attachment props [FAILURE] (%s, %s)\n",
				       lpProps[0].value.lpszA, MT_MAIL_ATTACH);
			ret = false;
			goto cleanup;
		}
	}	
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_ATTACH_FILENAME);
	retval = GetProps(&obj_targ_attach, 0, SPropTagArray, &lpProps, &cValues);
	mapitest_print_retval(mt, "GetProps");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(MT_MAIL_ATTACH, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 16D - Check: Target attachment props - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 16D - Check: Target attachment props [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}	

	/* Create another folder */
        retval = CreateFolder(&obj_top_folder, FOLDER_GENERIC, "MT Target Folder", NULL,
                              OPEN_IF_EXISTS, &obj_targ_folder);
	mapitest_print_retval(mt, "CreateFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	lpProp[0].ulPropTag = PR_CONTAINER_CLASS;
	lpProp[0].value.lpszA = (uint8_t *) "IPF.Journal";
	retval = SetProps(&obj_targ_folder, 0, lpProp, 1);
	mapitest_print_retval(mt, "SetProps");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Copy properties from the test folder to the new folder */
	exclude = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = CopyTo(&obj_ref_folder, &obj_targ_folder, exclude, 0x0, &problem_count, &problems);
	MAPIFreeBuffer(exclude);
	MAPIFreeBuffer(problems);
	mapitest_print_retval_fmt(mt, "CopyTo", "(folder)");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}	

	/* Check that the properties on both folders are correct */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_CONTAINER_CLASS);
	retval = GetProps(&obj_ref_folder, 0, SPropTagArray, &lpProps, &cValues);
	mapitest_print_retval(mt, "GetProps");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(MT_DIRNAME_TOP, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 19B - Check: Reference folder props - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 19B - Check: Reference folder props [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}	
	if (lpProps[1].value.lpszA) {
		if (!strncmp("IPF.Note", (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 19C - Check: Reference folder props - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 19C - Check: Reference folder props [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_CONTAINER_CLASS);
	retval = GetProps(&obj_targ_folder, 0, SPropTagArray, &lpProps, &cValues);
	mapitest_print_retval(mt, "GetProps");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp("MT Target Folder", (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 19E - Check: Target folder props - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 19E - Check: Target folder props [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}	
	if (lpProps[1].value.lpszA) {
		if (!strncmp("IPF.Note", (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 19F - Check: Target folder props - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 19F - Check: Target folder props [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}	


 cleanup:
	/* Cleanup reference strings */
	MAPIFreeBuffer((void *)subject);
	MAPIFreeBuffer((void *)name);
	MAPIFreeBuffer((void *)targ_name);
	MAPIFreeBuffer((void *)targ_dept);

	/* Cleanup folders */
	retval = DeleteFolder(&obj_top_folder, mapi_object_get_id(&obj_targ_folder),
			      DEL_FOLDERS | DEL_MESSAGES | DELETE_HARD_DELETE, NULL);
	mapitest_print_retval(mt, "DeleteFolder");
	retval = DeleteFolder(&obj_top_folder, mapi_object_get_id(&obj_ref_folder),
			      DEL_FOLDERS | DEL_MESSAGES | DELETE_HARD_DELETE, NULL);
	mapitest_print_retval(mt, "DeleteFolder");

	/* Release */
	mapi_object_release(&obj_targ_attach);
	mapi_object_release(&obj_ref_attach);
	mapi_object_release(&obj_ref_message);
	mapi_object_release(&obj_ref_folder);
	mapi_object_release(&obj_top_folder);
	mapi_object_release(&obj_store);

	return ret;
}

#define NAMEDPROP_NAME "mapitest_namedprop"
#define NAMEDPROP_IDNUM 0xDB

/**
   \details Test the GetPropertyIdsFromNames (0x56),
   GetNamesFromPropertyIds (0x55) and QueryNamesFromIDs (0x5f) 
   operations

   This function:
   -# Logs into the server
   -# Create a test folder and test message
   -# Creates one MNID_ID property
   -# Creates one MNID_STRING property
   -# Builds a table of Name, ID pairs using QueryNamesFromIDs()
   -# Iterates over names, and calls GetIDsFromNames() on each name
   -# Iterates over IDs, and calls GetNamesFromIDs() on each ID
   -# Cleans up

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_NameId(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_top_folder;
	mapi_id_t		id_top_folder;
	mapi_object_t		obj_ref_folder;
	mapi_object_t		obj_ref_message;
	struct mapi_nameid	*nameid;
	struct mapi_nameid	*nameid2;
	struct MAPINAMEID	checknameid;
	struct SPropTagArray	*SPropTagArray;
	uint32_t		propID;
	uint16_t		*propIDs;
	bool 			ret = true;
	int			i;
	bool			result;

	/* Log into the server */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_top_folder);
	mapi_object_init(&obj_ref_folder);
	mapi_object_init(&obj_ref_message);

	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	retval = GetDefaultFolder(&obj_store, &id_top_folder, olFolderTopInformationStore);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = OpenFolder(&obj_store, id_top_folder, &obj_top_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 2: Create test folder */
        retval = CreateFolder(&obj_top_folder, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
		     OPEN_IF_EXISTS, &obj_ref_folder);
	mapitest_print_retval(mt, "CreateFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	result = mapitest_common_message_create(mt, &obj_ref_folder, &obj_ref_message, MT_MAIL_SUBJECT);
	if (result != true) {
		mapitest_print_retval(mt, "mapitest_common_message_create failed");
		ret = false;
		goto cleanup;
	}
	retval = SaveChangesMessage(&obj_ref_folder, &obj_ref_message, KeepOpenReadWrite);
	mapitest_print_retval(mt, "SaveChangesMessage");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3: Create and Retrieve one MNID_ID property */

	/* Build the list of named properties we want to create */
	nameid = mapi_nameid_new(mt->mem_ctx);
	mapi_nameid_custom_lid_add(nameid, NAMEDPROP_IDNUM, PT_STRING8, PS_PUBLIC_STRINGS);

	/* GetIDsFromNames and map property types */
	SPropTagArray = talloc_zero(mt->mem_ctx, struct SPropTagArray);
	retval = GetIDsFromNames(&obj_ref_message, nameid->count, 
				 nameid->nameid, MAPI_CREATE, &SPropTagArray);
	mapitest_print_retval(mt, "GetIDsFromNames");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		MAPIFreeBuffer(nameid);
		goto cleanup;
	}

	mapi_nameid_SPropTagArray(nameid, SPropTagArray);
	MAPIFreeBuffer(nameid);
	
	propID = SPropTagArray->aulPropTag[0];
	MAPIFreeBuffer(SPropTagArray);

	nameid = mapi_nameid_new(mt->mem_ctx);
	retval = GetNamesFromIDs(&obj_ref_message, propID, &nameid->count, &nameid->nameid);
	mapitest_print_retval(mt, "GetNamesFromIDs");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(nameid);
		ret = false;
		goto cleanup;
	}

	if ((nameid->nameid[0].ulKind != MNID_ID) || (nameid->nameid[0].kind.lid != NAMEDPROP_IDNUM)) {
		errno = MAPI_E_RESERVED;
		mapitest_print_retval_fmt(mt, "GetNamesFromIDs", 
					  "Unexpected result: ulKind: %x mapped to 0x%.4x", 
					  nameid->nameid[0].ulKind, nameid->nameid[0].kind.lid);
		ret = false;
		goto cleanup;
	}

	MAPIFreeBuffer(nameid);

	/* Step 4: Create one MNID_STRING property */
	nameid = mapi_nameid_new(mt->mem_ctx);
	SPropTagArray = talloc_zero(mt->mem_ctx, struct SPropTagArray);

	mapi_nameid_custom_string_add(nameid, NAMEDPROP_NAME, PT_STRING8, PS_PUBLIC_STRINGS);
	retval = GetIDsFromNames(&obj_ref_folder, nameid->count, nameid->nameid, MAPI_CREATE, &SPropTagArray);
	mapitest_print_retval(mt, "GetIDsFromNames");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(nameid);
		ret = false;
		goto cleanup;
	}

	mapi_nameid_SPropTagArray(nameid, SPropTagArray);
	MAPIFreeBuffer(nameid);

	propID = SPropTagArray->aulPropTag[0];
	MAPIFreeBuffer(SPropTagArray);

	/* Builds an array of Name,ID pairs using QueryNamesFromIDs() */
	nameid = mapi_nameid_new(mt->mem_ctx);
	retval = QueryNamedProperties(&obj_ref_message, 0x1, NULL, &nameid->count, &propIDs, &nameid->nameid);
	nameid->nameid = talloc_steal((TALLOC_CTX *)nameid, nameid->nameid);
	mapitest_print_retval(mt, "QueryNamedProperties");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(nameid);
		talloc_free(propIDs);
		ret = false;
		goto cleanup;
	}

	/* Iterate over names and call GetIDsFromNames() on each name */
	for (i = 0; i < nameid->count; i++) {
		checknameid.lpguid = nameid->nameid[i].lpguid;
		checknameid.ulKind = nameid->nameid[i].ulKind;

		switch (nameid->nameid[i].ulKind) {
		case MNID_ID:
			checknameid.kind.lid = nameid->nameid[i].kind.lid;
			break;
		case MNID_STRING:
			checknameid.kind.lpwstr.Name = nameid->nameid[i].kind.lpwstr.Name;
			checknameid.kind.lpwstr.NameSize = nameid->nameid[i].kind.lpwstr.NameSize;
			break;
		}

		SPropTagArray = talloc_zero(mt->mem_ctx, struct SPropTagArray);
		retval = GetIDsFromNames(&obj_ref_folder, 1, &checknameid, 0, &SPropTagArray);
		if (retval != MAPI_E_SUCCESS) {
			mapitest_print_retval(mt, "GetIDsFromNames");
			MAPIFreeBuffer(nameid);
			MAPIFreeBuffer(SPropTagArray);
			ret = false;
			goto cleanup;
		}

		/* check we got the right number of IDs */
		if (SPropTagArray->cValues != 1) {
			errno = MAPI_E_RESERVED;
			mapitest_print_retval_fmt(mt, "GetIDsFromNames", "Unexpected ID count (%i)", SPropTagArray->cValues);
			MAPIFreeBuffer(nameid);
			MAPIFreeBuffer(SPropTagArray);
			ret = false;
			goto cleanup;
		}

		/* check if the ID is the one we expected */
		if (SPropTagArray->aulPropTag[0] != (propIDs[i] << 16)) {
			errno = MAPI_E_RESERVED;
			mapitest_print_retval_fmt(mt, "GetIDsFromNames", "Unexpected ID (0x%x, expected 0x%x)",
						  SPropTagArray->aulPropTag[0], (propIDs[i] << 16));
			MAPIFreeBuffer(nameid);
			MAPIFreeBuffer(SPropTagArray);
			ret = false;
			goto cleanup;
		}
		MAPIFreeBuffer(SPropTagArray);
	}
	mapitest_print(mt, "* Step 6: All IDs matched [SUCCESS]\n");

	/* Iterates over IDs, and call GetNamesFromIDs() on each ID */
        for (i = 0; i < nameid->count; i++) {
		nameid2 = mapi_nameid_new(mt->mem_ctx);
		retval = GetNamesFromIDs(&obj_ref_folder, (propIDs[i] << 16), &nameid2->count, &nameid2->nameid);
		if (retval != MAPI_E_SUCCESS) {
			mapitest_print_retval(mt, "GetNamesFromIDs");
			MAPIFreeBuffer(nameid2);
			ret = false;
			goto cleanup;
		}

		/* Check we got the right number of names */
		if (nameid2->count != 1) {
			mapitest_print_retval_fmt(mt, "GetNamesFromIDs", "Unexpected name count (%i)", nameid2->count);
			MAPIFreeBuffer(nameid);
			MAPIFreeBuffer(nameid2);
			ret = false;
			goto cleanup;
		}

		/* Check we got the right kind of name */
		if (nameid2->nameid[0].ulKind != nameid->nameid[i].ulKind) {
			mapitest_print_retval_fmt(mt, "GetIDsFromNames", "Unexpected kind (0x%x, expected 0x%x)",
						  nameid2->nameid[0].ulKind, nameid->nameid[i].ulKind);
			MAPIFreeBuffer(nameid);
			MAPIFreeBuffer(nameid2);
			ret = false;
			goto cleanup;
		}

		switch (nameid->nameid[i].ulKind) {
		case MNID_ID:
			if (nameid2->nameid[0].kind.lid != nameid->nameid[i].kind.lid) {
				mapitest_print_retval_fmt(mt, "GetIDsFromNames", "Unexpected hex name (0x%x, expected 0x%x)",
							  nameid2->nameid[0].kind.lid, nameid->nameid[i].kind.lid);
				MAPIFreeBuffer(nameid);
				MAPIFreeBuffer(nameid2);
				ret = false;
				goto cleanup;
			}
			break;
		case MNID_STRING:
			if (nameid2->nameid[0].kind.lpwstr.NameSize != nameid->nameid[i].kind.lpwstr.NameSize) {
				mapitest_print_retval_fmt(mt, "GetIDsFromNames", "Unexpected name length (0x%x, expected 0x%x)",
							  nameid2->nameid[0].kind.lpwstr.NameSize, 
							  nameid->nameid[i].kind.lpwstr.NameSize);
				MAPIFreeBuffer(nameid);
				MAPIFreeBuffer(nameid2);
				ret = false;
				goto cleanup;
			}
			if (strncmp(nameid2->nameid[0].kind.lpwstr.Name, nameid->nameid[i].kind.lpwstr.Name, nameid->nameid[i].kind.lpwstr.NameSize) != 0) {
				mapitest_print_retval_fmt(mt, "GetIDsFromNames", "Unexpected name (%s, expected %s)",
							  nameid2->nameid[0].kind.lpwstr.Name, 
							  nameid->nameid[i].kind.lpwstr.Name);
				MAPIFreeBuffer(nameid);
				MAPIFreeBuffer(nameid2);
				ret = false;
				goto cleanup;				
			}
			break;
		}

		MAPIFreeBuffer(nameid2);
	}		
	
	MAPIFreeBuffer(nameid);
	MAPIFreeBuffer(propIDs);

 cleanup:
	errno = 0;
	/* Clean up */
	DeleteFolder(&obj_top_folder, mapi_object_get_id(&obj_ref_folder),
		     DEL_FOLDERS | DEL_MESSAGES | DELETE_HARD_DELETE, NULL);
	mapitest_print_retval(mt, "DeleteFolder");

	/* Release */
	mapi_object_release(&obj_ref_message);
	mapi_object_release(&obj_ref_folder);
	mapi_object_release(&obj_top_folder);
	mapi_object_release(&obj_store);

	return ret;
}

/**
   \details Test the GetPropertyIdsFromNames (0x56) and
   GetNamesFromPropertyIds (0x55) operations for the special
   case of the PS_MAPI namespace

   This function:
   -# Logs into the server
   -# Gets a property ID for a known property name
   -# Gets a property name for a known property ID
   -# Cleans up

   Refer to MS-OXPROPS for the list of properties

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_NameId_PSMAPI(struct mapitest *mt)
{	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	struct mapi_nameid	*nameid;
	struct SPropTagArray	*SPropTagArray;
	bool 			ret = true;

	/* Log into the server */
	mapi_object_init(&obj_store);

	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval_clean(mt, "OpenMsgStore", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Build the list of named properties we want to get */
	nameid = mapi_nameid_new(mt->mem_ctx);
	mapi_nameid_custom_lid_add(nameid, (PR_ACCESS>>16), PT_LONG, PS_MAPI); // 0x0FF4
	mapi_nameid_custom_lid_add(nameid, (PR_ATTACHMENT_HIDDEN>>16), PT_BOOLEAN, PS_MAPI);

	/* GetIDsFromNames and map property types */
	SPropTagArray = talloc_zero(mt->mem_ctx, struct SPropTagArray);
	retval = GetIDsFromNames(&obj_store, nameid->count, 
				 nameid->nameid, 0, &SPropTagArray);
	mapitest_print_retval_clean(mt, "GetIDsFromNames", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		MAPIFreeBuffer(nameid);
		goto cleanup;
	}

	mapi_nameid_SPropTagArray(nameid, SPropTagArray);
	MAPIFreeBuffer(nameid);
	
	if (SPropTagArray->aulPropTag[0] == PR_ACCESS) {
		mapitest_print(mt, "Comparison [0] matched\n");
	} else {
		mapitest_print(mt, "Comparison [0] failed : 0x%08x\n", SPropTagArray->aulPropTag[0]);
	}
	if (SPropTagArray->aulPropTag[1] == PR_ATTACHMENT_HIDDEN) {
		mapitest_print(mt, "Comparison [1] matched\n");
	} else {
		mapitest_print(mt, "Comparison [1] failed : 0x%08x\n", SPropTagArray->aulPropTag[1]);
	}
	MAPIFreeBuffer(SPropTagArray);

	nameid = mapi_nameid_new(mt->mem_ctx);
	retval = GetNamesFromIDs(&obj_store, PR_ATTACHMENT_HIDDEN, &nameid->count, &nameid->nameid);
	mapitest_print_retval_clean(mt, "GetNamesFromIDs", retval);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(nameid);
		ret = false;
		goto cleanup;
	}
	
	if (nameid->count != 1) {
		mapitest_print(mt, "Unexpected count from GetNamesFromIDs: %i", nameid->count);
		MAPIFreeBuffer(nameid);
		ret = false;
		goto cleanup;
	}
	if (nameid->nameid[0].ulKind != MNID_ID) {
		mapitest_print(mt, "Unexpected kind from GetNamesFromIDs: %i", nameid->nameid[0].ulKind);
		MAPIFreeBuffer(nameid);
		ret = false;
		goto cleanup;
	}
	if (nameid->nameid[0].kind.lid == (PR_ATTACHMENT_HIDDEN >> 16)) {
		mapitest_print(mt, "Comparision of values matches\n");
	} else {
		mapitest_print(mt, "Comparison of values mismatch (nameid->lid: 0x%04x)\n", nameid->nameid[0].kind.lid); 
	}

cleanup:
	mapi_object_release(&obj_store);
	return ret;
}
/**
   \details Test the SetPropertiesNoReplicate (0x79) and
    DeletePropertiesNoReplicate (0x7a) operations

   This function:
   -# Opens the mailbox
   -# Create a test folder
   -# Sets some properties on the test folder
   -# Delete properties from the test folder
   -# Deletes the test folder

   \todo It would be useful to test the problem return values

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcprpt_NoReplicate(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_top_folder;
	mapi_id_t		id_top_folder;
	mapi_object_t		obj_ref_folder;
	const char		*name = NULL;
	const char		*comment = NULL;
	struct SPropValue	lpProp[3];
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		cValues;
	bool			ret = true;

	/* Step 1. Logon Private Mailbox */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_top_folder);
	mapi_object_init(&obj_ref_folder);

	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval_step_fmt(mt, "1.", "OpenMsgStore", "(%s)", "Logon Private Mailbox");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = GetDefaultFolder(&obj_store, &id_top_folder, olFolderTopInformationStore);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	retval = OpenFolder(&obj_store, id_top_folder, &obj_top_folder);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 2: Create test folder */
        retval = CreateFolder(&obj_top_folder, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
                              OPEN_IF_EXISTS, &obj_ref_folder);
	mapitest_print_retval_step_fmt(mt, "2.", "CreateFolder", "(%s)", "Create the test folder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3: Set properties on the test folder */
	name = talloc_asprintf(mt->mem_ctx, "Reference: %s", "new name");
	comment = talloc_asprintf(mt->mem_ctx, "Reference: %s", "the folder comment");
	set_SPropValue_proptag(&lpProp[0], PR_DISPLAY_NAME, (const void *)name);
	set_SPropValue_proptag(&lpProp[1], PR_COMMENT, (const void *)comment);
	retval = SetPropertiesNoReplicate(&obj_ref_folder, 0, lpProp, 2);
	mapitest_print_retval_step_fmt(mt, "3.", "SetProps", "(%s)", "Set folder properties");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 4: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_DISPLAY_NAME, PR_COMMENT);
	retval = GetProps(&obj_ref_folder, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(name, (const char *) lpProps[0].value.lpszA, strlen((const char *) lpProps[0].value.lpszA))) {
			mapitest_print(mt, "* Step 4.1. - Check: Reference props set - [SUCCESS] (%s)\n",
				       lpProps[0].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 4.1. - Check: Reference props set [FAILURE] (%s)\n",
				       lpProps[0].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}
	if (lpProps[1].value.lpszA) {
		if (!strncmp(comment, (const char *) lpProps[1].value.lpszA, strlen((const char *) lpProps[1].value.lpszA))) {
			mapitest_print(mt, "* Step 4.2. - Check: Reference props set - [SUCCESS] (%s)\n",
				       lpProps[1].value.lpszA);
		} else {
			mapitest_print(mt, "* Step 4.2. - Check: Reference props set [FAILURE] (%s)\n",
				       lpProps[1].value.lpszA);
			ret = false;
			goto cleanup;
		}
	}

	/* Step 5. Delete Properties */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_COMMENT);
	retval = DeletePropertiesNoReplicate(&obj_ref_folder, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	mapitest_print_retval_step_fmt(mt, "5.", "DeletePropertiesNoReplicate", "PR_COMMENT");

	/* Step 6. Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_COMMENT);
	retval = GetProps(&obj_ref_folder, 0, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (get_SPropValue(lpProps, PR_COMMENT) == NULL) {
		mapitest_print(mt, "* Step 6.1. - GetProps verifier [SUCCESS]\n");
	} else {
		mapitest_print(mt, "* Step 6.1. - GetProps verifier [FAILURE]:\n");
	}

	/* Cleanup and release */
cleanup:
	retval = DeleteFolder(&obj_top_folder, mapi_object_get_id(&obj_ref_folder),
			      DEL_FOLDERS | DEL_MESSAGES | DELETE_HARD_DELETE, NULL);
	mapitest_print_retval_step(mt, "7.", "DeleteFolder", retval);
	mapi_object_release(&obj_ref_folder);
	mapi_object_release(&obj_top_folder);
	mapi_object_release(&obj_store);

	return ret;
}

/**
   \details Test WriteAndCommitStream (0x90) operation.

   This function:
   -# Logs in 
   -# Opens the Outbox folder
   -# Creates a test message
   -# Creates an attachment on the test messages and set properties on the attachment
   -# Opens a stream on the attachment
   -# Sets the stream size
   -# Write and commits into the stream
   -# Saves the message
   -# Gets stream size and compare values
   -# Opens the stream again with different permissions
   -# Reads the stream and compares buffers
   -# Deletes the test message

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise -1
 */
_PUBLIC_ bool mapitest_oxcprpt_WriteAndCommitStream(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_object_t		obj_attach;
	mapi_object_t		obj_stream;
	mapi_id_t		id_folder;
	DATA_BLOB		data;
	struct SPropValue	attach[3];
	char			*stream = NULL;
	char			*out_stream = NULL;
	const uint32_t		stream_len = 0x1000;
	unsigned char		buf[0x1000];
	uint32_t		StreamSize = 0;
	uint16_t		read_size = 0;
	uint16_t		write_len = 0;
	uint32_t		offset = 0;


	stream = mapitest_common_genblob(mt->mem_ctx, stream_len);
	if (stream == NULL) {
		return false;
	}

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Inbox folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderInbox);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create the message */
	mapi_object_init(&obj_message);
	ret = mapitest_common_message_create(mt, &obj_folder, &obj_message, MT_MAIL_SUBJECT);
	mapitest_print_retval(mt, "Message Creation");
	if (ret != true) {
		return false;
	}

	/* Step 4. Create the attachment */
	mapi_object_init(&obj_attach);
	retval = CreateAttach(&obj_message, &obj_attach);
	mapitest_print_retval(mt, "CreateAttach");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	attach[0].ulPropTag = PR_ATTACH_METHOD;
	attach[0].value.l = ATTACH_BY_VALUE;
	attach[1].ulPropTag = PR_RENDERING_POSITION;
	attach[1].value.l = 0;
	attach[2].ulPropTag = PR_ATTACH_FILENAME;
	attach[2].value.lpszA = (uint8_t *) MT_MAIL_ATTACH;

	retval = SetProps(&obj_attach, 0, attach, 3);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 5. Open the stream */
	mapi_object_init(&obj_stream);
	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 2, &obj_stream);
	mapitest_print_retval(mt, "OpenStream");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 6. Set the stream size */
	retval = SetStreamSize(&obj_stream, (uint64_t) stream_len);
	mapitest_print_retval(mt, "SetStreamSize");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 7. Write the stream */
	write_len = 0;

	data.length = stream_len;
	data.data = (uint8_t *) stream;
	retval = WriteAndCommitStream(&obj_stream, &data, &write_len);
	mapitest_print_retval_fmt_clean(mt, "WriteAndCommitStream", retval, "(0x%x bytes written)", write_len);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 8. Save the attachment */
	retval = SaveChangesAttachment(&obj_message, &obj_attach, KeepOpenReadOnly);
	mapitest_print_retval_clean(mt, "SaveChangesAttachment", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
	mapitest_print_retval_clean(mt, "SaveChangesMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 9. Get stream size */
	retval = GetStreamSize(&obj_stream, &StreamSize);
	mapitest_print_retval_clean(mt, "GetStreamSize", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}
	mapitest_print(mt, "* %-35s: %s\n", "StreamSize comparison", 
		       (StreamSize == stream_len) ? "[PASSED]" : "[FAILURE]");

	/* Step 10. Read the stream */
	mapi_object_release(&obj_stream);
	mapi_object_init(&obj_stream);

	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 0, &obj_stream);
	mapitest_print_retval_clean(mt, "OpenStream", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	offset = 0;
	out_stream = talloc_size(mt->mem_ctx, StreamSize + 1);
	do {
		retval = ReadStream(&obj_stream, buf, MT_STREAM_MAX_SIZE, &read_size);
		mapitest_print_retval_fmt_clean(mt, "ReadStream", retval, "(0x%x bytes read)", read_size);
		memcpy(out_stream + offset, buf, read_size);
		offset += read_size;
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
			break;
		}
	} while (read_size && (offset != StreamSize));
	out_stream[offset] = '\0';

	if (offset) {
		if (!strcmp(stream, out_stream)) {
			mapitest_print(mt, "* %-35s: [IN,OUT] stream [PASSED]\n", "Comparison");
		} else {
			mapitest_print(mt, "* %-35s: [IN,OUT] stream [FAILURE]\n", "Comparison");

		}
	}

	/* Release */
	mapi_object_release(&obj_stream);
	mapi_object_release(&obj_attach);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	talloc_free(stream);
	talloc_free(out_stream);

	return ret;
}

