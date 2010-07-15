/*
   Stand-alone MAPI testsuite

   OpenChange Project - MS-LCID tests

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

#include "utils/mapitest/mapitest.h"
#include "utils/mapitest/proto.h"

/**
   \file module_lcid.c

   \brief Language Code checks (MS-LCID)

   \note This is low level (internal) checking, and application programmers
   do not normally need to deal with the functions checked in this test suite.
*/

/**
   \details Verify libmapi/util/lcid.c functions

   This function:
   -# exercises lcid_langcode2langtag()

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
*/ 
_PUBLIC_ bool mapitest_lcid_langcode2langtag(struct mapitest *mt)
{
	const char	*locale;

	locale = mapi_get_locale_from_lcid(0x0409);
	if (strcmp(locale, "en_US") != 0) {
		mapitest_print(mt, "* %-35s: [FAILURE] - %s\n", "Step 1 - mismatch", locale);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 1");
	}

	locale = mapi_get_locale_from_lcid(0x0439);
	if (strcmp(locale, "hi_IN") != 0) {
		mapitest_print(mt, "* %-35s: [FAILURE] - %s\n", "Step 2 - mismatch", locale);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 2");
	}

	locale = mapi_get_locale_from_lcid(0x1401);
	if (strcmp(locale, "ar_DZ") != 0) {
		mapitest_print(mt, "* %-35s: [FAILURE] - %s\n", "Step 3 - mismatch", locale);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 3");
	}

	locale = mapi_get_locale_from_lcid(0x0);
	if (locale != NULL) {
		mapitest_print(mt, "* %-35s: [FAILURE] - %s\n", "Step 4 - expected NULL", locale);
		return false;
	} else {
		mapitest_print(mt, "* %-35s: [SUCCESS]\n", "Step 4");
	}
	return true;
}

