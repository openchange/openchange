/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2011
   Copyright (C) Brad Hards <bradh@openchange.org> 2010-2011

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
   \file mapistore_common.h

   \brief MAPISTORE shared (common) API
   
   This header contains the shared declarations for functions that are 
   used across the MAPISTORE API.
 */

#ifndef	__MAPISTORE_COMMON_H
#define	__MAPISTORE_COMMON_H

#include <ldb.h>

#define	MSTORE_LEVEL_CRITICAL		0
#define	MSTORE_LEVEL_HIGH		1
#define	MSTORE_LEVEL_MEDIUM		2
#define	MSTORE_LEVEL_NORMAL		3
#define	MSTORE_LEVEL_LOW		4
#define	MSTORE_LEVEL_INFO		5
#define	MSTORE_LEVEL_DEBUG		6
#define	MSTORE_LEVEL_PEDANTIC		7
#define	MSTORE_LEVEL_EXTREME		8

#define	MSTORE_DEBUG_FMT_SUCCESS	"* [%s:%d][%s]: "
#define	MSTORE_DEBUG_FMT_INFO		"I [%s:%d][%s]: "
#define	MSTORE_DEBUG_FMT_ERROR		"! [%s:%d][%s]: "
#define	MSTORE_SINGLE_MSG		"%s\n"
#define	MSTORE_DEBUG_FMT_PARAM		__FILE__, __LINE__, __FUNCTION__

#define	__MSTORE_DEBUG(l, mstore_msg, fmt, ...)						\
	if (fmt) {									\
		DEBUG(l, (mstore_msg fmt, MSTORE_DEBUG_FMT_PARAM, __VA_ARGS__));	\
	} else {									\
		DEBUG(l, (mstore_msg, MSTORE_DEBUG_FMT_PARAM));				\
	}										

#define	MSTORE_DEBUG_INFO(l,fmt,...)		__MSTORE_DEBUG(l, MSTORE_DEBUG_FMT_INFO, fmt, __VA_ARGS__)
#define	MSTORE_DEBUG_SUCCESS(l,fmt,...)		__MSTORE_DEBUG(l, MSTORE_DEBUG_FMT_SUCCESS, fmt, __VA_ARGS__)
#define	MSTORE_DEBUG_ERROR(l,fmt,...)		__MSTORE_DEBUG(l, MSTORE_DEBUG_FMT_ERROR, fmt, __VA_ARGS__)

struct mapistore_backend_context;

__BEGIN_DECLS

const char		*mapistore_get_mapping_path(void);
const char		*mapistore_get_firstorgdn(void);
const char		*mapistore_get_database_path(void);
const char		*mapistore_get_named_properties_database_path(void);

/* definition from mapistore_backend_public.c */
struct ldb_context	*mapistore_public_ldb_connect(struct mapistore_backend_context *, const char *);
enum MAPISTORE_ERROR	mapistore_exist(struct mapistore_backend_context *, const char *, const char *);
enum MAPISTORE_ERROR mapistore_register_folder(struct mapistore_backend_context *, const char *, const char *, const char *, uint64_t);

__END_DECLS

#endif /* __MAPISTORE_COMMON_H */
