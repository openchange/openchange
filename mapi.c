/*
   OpenChange MAPI PHP bindings

   Copyright (C) Zentyal SL, <jamor@zentyal.com> 2013.

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"
#include "mapi_profile_db.h"
#include "mapi_profile.h"
#include "mapi_session.h"

static zend_function_entry mapi_functions[] = {
	{NULL, NULL, NULL}
};

zend_module_entry mapi_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	PHP_MAPI_EXTNAME,
	mapi_functions,
	PHP_MINIT(mapi),
	PHP_MSHUTDOWN(mapi),
	NULL,
	NULL,
	NULL,
#if ZEND_MODULE_API_NO >= 20010901
	PHP_MAPI_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_MAPI
ZEND_GET_MODULE(mapi)
#endif

PHP_MINIT_FUNCTION(mapi)
{
	// register classes
	MAPIProfileDBRegisterClass(TSRMLS_C);
	MAPIProfileRegisterClass(TSRMLS_C);
	MAPISessionRegisterClass(TSRMLS_C);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(mapi)
{
}
