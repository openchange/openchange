/*
   OpenChange Server implementation

   Fault handling helper utilities and handlers

   Copyright (C) Kamen Mazdrashki 2014

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

/**
   \file fault_util.c

   \brief Implements fault-handling utilities for OpenChange
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "libmapiproxy.h"
#include "fault_util.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include <util/debug.h>

#include <samba/version.h>

#include <execinfo.h>

/**
   \details print a backtrace using DEBUG() macro.

 */
_PUBLIC_ void debug_print_backtrace(int dbg_level)
{
	const int BACKTRACE_SIZE = 64;
	void *backtrace_stack[BACKTRACE_SIZE];
	size_t backtrace_size;
	char **backtrace_strings;

	backtrace_size = backtrace(backtrace_stack, BACKTRACE_SIZE);
	backtrace_strings = backtrace_symbols(backtrace_stack, backtrace_size);

	DEBUG(dbg_level, ("BACKTRACE: %lu stack frames:\n", (unsigned long)backtrace_size));

	if (backtrace_strings) {
		int i;

		for (i = 0; i < backtrace_size; i++) {
			DEBUGADD(dbg_level, (" #%.2u %s\n", i, backtrace_strings[i]));
		}

		free(backtrace_strings);
	}
}

/**
   \details Handle Openchange calls. Depending on configuration and severity
	    it may really abort() or just skip it

 */
_PUBLIC_ void openchange_abort(bool is_fatal)
{
	/* print Samba and OpenChange versions */
	const char *samba_version = SAMBA_VERSION_STRING;
	const char *openchange_version = OPENCHANGE_VERSION_STRING;

	OC_DEBUG(0, "Version Samba: %s", samba_version);
	OC_DEBUG(0, "Version OpenChange: %s", openchange_version);
	debug_print_backtrace(0);

	/* TODO: decide to abort or not */
	if (is_fatal) {
		abort();
	}
}
