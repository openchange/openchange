/*
   OpenChange MAPI PHP bindings

   Copyright (C) 2013 Javier Amor Garcia.

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

#include <php_mapi.h>

static zend_class_entry *mapi_exception_ce;

void MAPIExceptionRegisterClass(TSRMLS_D) {
	zend_class_entry ex_entry;
	INIT_CLASS_ENTRY(ex_entry, "MAPIException", NULL);
	mapi_exception_ce = zend_register_internal_class_ex(&ex_entry, NULL, "exception" TSRMLS_CC);
}

zend_class_entry *mapi_exception_get_default(){
	return mapi_exception_ce;
}
