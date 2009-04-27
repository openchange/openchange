/*
   Stand-alone MAPI testsuite

   OpenChange Project - Core Notifications Protocol operation tests

   Copyright (C) Brad Hards 2009

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
   \file module_oxcnotif.c

   \brief Core Notifications Protocol test suite
*/

/* Internal callback functions */
int cb(uint16_t, void*, void*);

int cb(uint16_t type, void* data, void* priv)
{
	return 0;
}

/**
   \details Test the RegisterNotification (0x29) operation

   This function:
   -# 

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcnotif_RegisterNotification(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	uint32_t tcon;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}
	
	/* Step 2. Open Inbox folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_folder, olFolderInbox);
	if (!ret) {
		return ret;
	}

	/* Step 3. Register notification */
	retval = RegisterNotification(fnevObjectCopied);
	mapitest_print_retval(mt, "RegisterNotification");
	if ( retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Subscribe for notifications */
	retval = Subscribe(&obj_store, &tcon, fnevObjectCopied, true, cb, NULL);
	mapitest_print_retval(mt, "Subscribe");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 5. Unsubscribe for notifications */
	retval = Unsubscribe(mt->session, tcon);
	mapitest_print_retval(mt, "Unsubscribe");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 6. Cleanup */
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}
