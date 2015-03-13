/*
   Logging API

   OpenChange Project

   Copyright (C) Jelmer Vernooij 2015

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

#include "libmapi/libmapi.h"
#include <util/debug.h>

void oc_log(enum oc_log_level level, const char *fmt_string, ...)
{
	va_list ap;
	va_start(ap, fmt_string);
	oc_logv(level, fmt_string, ap);
	va_end(ap);
}

void oc_logv(enum oc_log_level level, const char *fmt_string, va_list ap)
{
	char line[OC_LOG_MAX_LINE];
	int samba_level;
	int nwritten;

	nwritten = vsnprintf(line, sizeof(line), fmt_string, ap);

	if (level >= 0) {
		samba_level = level;
	} else {
		/* Log OC_LOG_FATAL, OC_LOG_ERROR, OC_LOG_WARNING and OC_LOG_INFO
		 * all at samba debug level 0 */
		samba_level = 0;
	}

	DEBUG(samba_level, ("%s\n", line));
}

void oc_log_init_stdout()
{
	setup_logging("", DEBUG_DEFAULT_STDOUT);
}

void oc_log_init_stderr()
{
	setup_logging("", DEBUG_DEFAULT_STDERR);
}

/* Initialize logging subsystem to write to users' local log file, e.g. ~/.openchange.log */
void oc_log_init_user(const char *progname, struct loadparm_context *lp_ctx)
{
	/* For the moment, just log to the system logs if possible. */
	setup_logging(progname, DEBUG_FILE);
}

/* Initialize logging subsystem to write to config file specified in smb.conf,
   defaulting to /var/log/openchange.log */
void oc_log_init_server(const char *progname, struct loadparm_context *lp_ctx)
{
	setup_logging(progname, DEBUG_FILE);
}
