/*
   OpenChange MAPI implementation.
   libmapi macros header file

   Copyright (C) Julien Kerihuel 2010.

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

#ifndef __MAPICODE_H__
#define __MAPICODE_H__

#define MAPI_RETVAL_IF(x,e,c)	\
do {				\
	if (x) {			\
		errno = (e);		\
		if (c) {		\
			talloc_free(c);	\
		}			\
		return -1;		\
	}				\
} while (0);

#define OPENCHANGE_RETVAL_IF(x,e,c)	\
do {				\
	if (x) {			\
		set_errno(e);		\
		if (c) {		\
			talloc_free(c);	\
		}			\
		return (e);		\
	}				\
} while (0);

#define OPENCHANGE_RETVAL_CALL_IF(x,e,r,c)	\
do {					\
	if (x) {				\
		set_errno(e);			\
		if (r) {			\
			talloc_free(r);		\
		}				\
		if (c) {			\
			talloc_free(c);		\
		}				\
		return (e);			\
	}					\
 } while (0);

#define OPENCHANGE_RETVAL_ERR(e,c)	\
do {				\
	set_errno(e);			\
	if (c) {			\
		talloc_free(c);		\
	}				\
	return (e);			\
} while (0);

#define OPENCHANGE_CHECK_NOTIFICATION(s,r)		\
do {						\
	if (s->notify_ctx)				\
		ProcessNotification(s->notify_ctx, r);	\
} while (0);

/* Status macros for MAPI */
typedef unsigned long	SCODE;


#define SEVERITY_ERROR	1
#define SEVERITY_WARN	0

#define FACILITY_ITF	4
#define	MAKE_MAPI_CODE(sev, fac, code) \
(((SCODE)(sev)<<31)|((SCODE)(fac)<<16)|((SCODE)(code)))

#define	MAKE_MAPI_E(code) (MAKE_MAPI_CODE(SEVERITY_ERROR, FACILITY_ITF, code))
#define	MAKE_MAPI_S(code) (MAKE_MAPI_CODE(SEVERITY_WARN, FACILITY_ITF, code))

#define	MAPI_STATUS_V(x) ((SCODE)x)

/*
 * MAPI_STATUS_IS_OK() is deprecated. Please use status == MAPI_E_SUCCESS instead
 */
/*#define	MAPI_STATUS_IS_OK(x) (MAPI_STATUS_V(x) == 0)*/
/*
 * MAPI_STATUS_IS_ERR() is deprecated. Please use status != MAPI_E_SUCCESS instead
 */
/*#define	MAPI_STATUS_IS_ERR(x) ((MAPI_STATUS_V(x) & 0xc0000000) == 0x80000000)*/
#define	MAPI_STATUS_EQUAL(x,y) (MAPI_STATUS_V(x) == MAPI_STATUS_V(y))

#define	MAPI_STATUS_IS_OK_RETURN(x) do { \
		if (MAPI_STATUS_IS_OK(x)) {\
			return x;\
		}\
} while (0)

#define	MAPI_STATUS_NOT_OK_RETURN(x) do { \
		if (!MAPI_STATUS_IS_OK(x)) {\
			return x;\
		}\
} while (0)

#define	MAPI_STATUS_IS_ERR_RETURN(x) do { \
		if (MAPI_STATUS_IS_ERR(x)) {\
			return x;\
		}\
} while (0)

#define	MAPI_STATUS_NOT_ERR_RETURN(x) do { \
		if (!MAPI_STATUS_IS_ERR(x)) {\
			return x;\
		}\
} while (0)


#endif /* !__MAPICODE_H__ */
