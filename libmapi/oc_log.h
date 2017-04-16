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

#ifndef _OC_LOG_H_
#define _OC_LOG_H_

#include <stdarg.h>
#include <param.h>

#define OC_LOG_MAX_LINE 1024

/* There are two ways of logging messages in OpenChange.
 *
 * oc_log() should be used for all messages that are intended to be seen by users.
 * It does not include file/lineno information. The first arugment to oc_log()
 * indicates the kind of message: FATAL, ERROR, WARNING, INFO or DEBUG.
 *
 * OC_DEBUG() should be used for anything that is relevant to developers, i.e.
 * helpful messages during debugging. OC_DEBUG() takes a priority argument,
 * and by default these messages will *not* be shown to users unless they
 * explicitly pass a flag to get more debugging information.
 * OC_DEBUG() is a macro and will include file/lineno information. OC_DEBUG()
 * statements might be compiled out on some platforms for performance reasons.
 */

enum oc_log_level {
	OC_LOG_FATAL=-3,
	OC_LOG_ERROR=-2,
	OC_LOG_WARNING=-1,
	OC_LOG_INFO=0,
	OC_LOG_DEBUG=1,
	/* Anything with a log level of zero or lower is considered a debug message,
	   in other words: not meant to be understood by end users.
	*/
};

/* Logs source file and line, at log level -priority and with the specified message.
 * This macro is a simple wrapper around oc_log() that adds the
 * source file name and line number to the message. */
#define OC_DEBUG(priority, format, ...) \
	oc_log (OC_LOG_INFO+(priority), __location__ "(%s): " format, __PRETTY_FUNCTION__, ## __VA_ARGS__)

/* Write a log message.
 * Like in syslog, a trailing newline is *not* required. The library will add it
 * if needed. */
void oc_log(enum oc_log_level level, const char *fmt_string, ...);
void oc_logv(enum oc_log_level level, const char *fmt_string, va_list ap);

/* Setup functions:: */

/* Initialize logging subsystem to write to stdout or stderr. */
void oc_log_init_stdout(void);
void oc_log_init_stderr(void);

/* Initialize logging subsystem to write to users' local log file, e.g. ~/.openchange.log */
void oc_log_init_user(const char *progname, struct loadparm_context *lp_ctx);

/* Initialize logging subsystem to write to config file specified in smb.conf,
   defaulting to /var/log/openchange.log */
void oc_log_init_server(const char *progname, struct loadparm_context *lp_ctx);

#endif /* _OC_LOG_H_ */
