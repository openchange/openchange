/*
   Stand-alone MAPI testsuite

   OpenChange Project

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
#include <samba/popt.h>
#include <param.h>
#include <param/proto.h>

#include <utils/openchange-tools.h>
#include <utils/mapitest/mapitest.h>

/**
 * This convenient function searches for an obj_message given the
 * specified subject. Note: this function doesn't use the Restrict
 * call so we can respect the ring policy we defined.
 */
bool mapitest_find_message_subject(struct mapitest *mt,
				   mapi_object_t *obj_folder,
				   mapi_object_t *obj_message,
				   uint8_t mode,
				   const char *subject,
				   const char *call,
				   uint32_t *index)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_ctable;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	uint32_t		count;
	uint32_t		count2 = 0;
	uint32_t		attempt = 0;
	const char		*msubject = NULL;
	uint32_t		i;

	/* Sanity checks */
	if (index == NULL) return false;
	if (call == NULL) return false;
	if (subject == NULL) return false;

	/* Retrieve the contents table */
	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(obj_folder, &obj_ctable);
	MT_ERRNO_IF_CALL_BOOL(mt, retval, call, "GetContentsTable");

	/* Customize the content table view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_FID,
					  PR_MID,
					  PR_SUBJECT);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MT_ERRNO_IF_CALL_BOOL(mt, retval, call, "SetColumns");

	/* Count the total number of rows */
	retval = GetRowCount(&obj_ctable, &count);
	MT_ERRNO_IF_CALL_BOOL(mt, retval, call, "GetRowCount");

	/* Position the table at the latest know position */
	retval = SeekRow(&obj_ctable, BOOKMARK_BEGINNING, *index, &count2);
	MT_ERRNO_IF_CALL_BOOL(mt, retval, call, "SeekRow");

	/* Substract current position from the total */
	count -= count2;

	while (((retval = QueryRows(&obj_ctable, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND) &&
	       SRowSet.cRows) {
		for (i = 0; i < SRowSet.cRows; i++) {
		retry:
			errno = 0;
			mapi_object_init(obj_message);
			retval = OpenMessage(obj_folder,
					     SRowSet.aRow[i].lpProps[0].value.d,
					     SRowSet.aRow[i].lpProps[1].value.d,
					     obj_message, mode);
			if (GetLastError() != MAPI_E_SUCCESS && attempt < 3) {
				sleep(1);
				attempt++;
				goto retry;
			}
			attempt = 0;

			if (retval == MAPI_E_SUCCESS) {
				msubject = SRowSet.aRow[i].lpProps[2].value.lpszA;
				if (msubject && !strncmp(subject, msubject, strlen(msubject))) {
					*index += i + 1;
					mapi_object_release(&obj_ctable);
					return true;
				}
			}
			mapi_object_release(obj_message);
		}
		*index += SRowSet.cRows;
	}
	mapi_object_release(&obj_ctable);
	return false;
}
