/*
   OpenChange MAPI PHP bindings

   Copyright (C) Zentyal SL. 2013.

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

#include "php_mapi.h"

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

void register_constants(int module_number TSRMLS_DC);

static void register_classes(TSRMLS_D)
{
	MAPIProfileDBRegisterClass(TSRMLS_C);
	MAPIProfileRegisterClass(TSRMLS_C);
	MAPISessionRegisterClass(TSRMLS_C);
	MAPIMailboxRegisterClass(TSRMLS_C);
	MAPIFolderRegisterClass(TSRMLS_C);

	MAPIMessageRegisterClass(TSRMLS_C);
	MAPIContactRegisterClass(TSRMLS_C);
	MAPITaskRegisterClass(TSRMLS_C);
	MAPIAppointmentRegisterClass(TSRMLS_C);

	MAPITableRegisterClass(TSRMLS_C);
	MAPIMessageTableRegisterClass(TSRMLS_C);
	MAPIFolderTableRegisterClass(TSRMLS_C);
}

PHP_MINIT_FUNCTION(mapi)
{
	register_constants(module_number TSRMLS_CC);
	register_classes(TSRMLS_C);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(mapi)
{
}

char *mapi_id_to_str(mapi_id_t id)
{
	char *str = (char*) emalloc(MAPI_ID_STR_SIZE);
	int res = snprintf(str, MAPI_ID_STR_SIZE, "0x%" PRIX64, id);
	if (res < 0) {
		php_error(E_ERROR, "Error  convertind MAPI ID 0x" PRIX64 " to string", id);
	}
	return str;
}

mapi_id_t str_to_mapi_id(const char *str)
{
	char *end;
	mapi_id_t res = strtoull(str, &end, 16);
	if ((errno == ERANGE) && (res == ULLONG_MAX)) {
		php_error(E_ERROR, "ID string '%40s' has overflowed", str);
	}
	if (*end != '\0') {
		php_error(E_ERROR, "Error parsing ID string '%40s'", str);
	}

	return res;
}

void print_stacktrace()
{
  void *buffer[100];
  int nptrs = backtrace(buffer, 100);
  char **strings = (char**) backtrace_symbols(buffer, nptrs);
  int i;
  for (i = 0; i < nptrs; i++) {
        php_printf("%s\n", strings[i]);
  }

  free(strings);
}
