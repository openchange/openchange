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

#ifndef	__FAULT_UTIL_H__
#define	__FAULT_UTIL_H__

#define OC_ABORT( is_fatal, body ) \
	DEBUGSEP(0); \
	DEBUG(0,("OPENCHANGE INTERNAL ERROR: pid %d (%s)", (int)getpid(), OPENCHANGE_VERSION_STRING)); \
	DEBUG(0, body); \
	DEBUGSEP(0); \
	openchange_abort(is_fatal);

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS		extern "C" {
#define __END_DECLS		}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

__BEGIN_DECLS

/* definitions from fault_util.h */
void openchange_abort(bool is_fatal);
void debug_print_backtrace(int dbg_level);

__END_DECLS

#endif	/* !__FAULT_UTIL_H__ */
