/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009

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
   \file mapistore_errors.h

   This header provides a set of result codes for MAPISTORE function calls.

 */

#ifndef	__MAPISTORE_ERRORS_H
#define	__MAPISTORE_ERRORS_H

void mapistore_set_errno(int);

#define	MAPISTORE_RETVAL_IF(x,e,c)	\
do {					\
	if (x) {			\
		mapistore_set_errno(e);	\
		if (c) {		\
			talloc_free(c);	\
		}			\
		return (e);		\
	}				\
} while (0);

#define	MAPISTORE_RETVAL_ERR(e,c)	\
do {					\
	mapistore_set_errno(e);		\
	if (c) {			\
		talloc_free(c);		\
	}				\
	return (e);			\
} while (0);

#define	MAPISTORE_SANITY_CHECKS(x,c)						\
MAPISTORE_RETVAL_IF(!x, MAPISTORE_ERR_NOT_INITIALIZED, c);			\
MAPISTORE_RETVAL_IF(!x->processing_ctx, MAPISTORE_ERR_NOT_INITIALIZED, c);	\
MAPISTORE_RETVAL_IF(!x->context_list, MAPISTORE_ERR_NOT_INITIALIZED, c);

enum mapistore_error {
/**
   The function call succeeded.
*/
	MAPISTORE_SUCCESS = 0,

/**
   The function call failed for some non-specific reason.
*/
	MAPISTORE_ERROR = 1,

/**
   The function call failed because it was unable to allocate the
   memory required by underlying operations.
*/
	MAPISTORE_ERR_NO_MEMORY = 2,

/**
   The function call failed because underlying context has already
   been initialized
*/
	MAPISTORE_ERR_ALREADY_INITIALIZED = 3,

/**
   The function call failed because context has not been initialized.
*/
	MAPISTORE_ERR_NOT_INITIALIZED = 4,

/**
   The function call failed because an internal mapistore storage
   component has corrupted data.
*/
	MAPISTORE_ERR_CORRUPTED = 5,

/**
   The function call failed because one of the function parameters is
   invalid
*/
	MAPISTORE_ERR_INVALID_PARAMETER = 6,

/**
   The function call failed because the directory doesn't exist
*/
	MAPISTORE_ERR_NO_DIRECTORY = 7,

/**
   The function call failed because the underlying function couldn't
   open a database.
*/
	MAPISTORE_ERR_DATABASE_INIT = 8,

/**
   The function call failed because the underlying function didn't run
   a database operation successfully.
*/
	MAPISTORE_ERR_DATABASE_OPS = 9,

/**
   The function failed to register a storage backend
*/
	MAPISTORE_ERR_BACKEND_REGISTER = 10,

/**
   One of more storage backend initialization functions failed to
   complete successfully.
*/
	MAPISTORE_ERR_BACKEND_INIT = 11,

/**
   The function failed because mapistore failed to create a context
*/
	MAPISTORE_ERR_CONTEXT_FAILED = 12,

/**
   The function failed because the provided namespace is invalid
*/
	MAPISTORE_ERR_INVALID_NAMESPACE = 13,

/**
   The specified URI is invalid
*/
	MAPISTORE_ERR_INVALID_URI = 14,

/**
   The function failed to find requested record/data
*/
	MAPISTORE_ERR_NOT_FOUND = 15,

/**
   The function still has a reference count
*/
	MAPISTORE_ERR_REF_COUNT = 16,

/**
   The function already have record/data for the searched element
*/
	MAPISTORE_ERR_EXIST = 17,

/**
   The function failed to generate requested data/payload
*/
	MAPISTORE_ERR_INVALID_DATA = 18,

/**
   The function failed to send message
*/
	MAPISTORE_ERR_MSG_SEND = 19,

/**
   The function failed to receive message
*/
	MAPISTORE_ERR_MSG_RCV = 20,

/** 
    The operation required privileges that the user does not have
*/
	MAPISTORE_ERR_DENIED = 21,

/**
   The function failed to initiate remote connection
*/
	MAPISTORE_ERR_CONN_REFUSED = 22,

/**
   The function is not implemented
*/
	MAPISTORE_ERR_NOT_IMPLEMENTED
};

#endif /* ! __MAPISTORE_ERRORS_H */
