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

   \brief Thie header providers a set of result codes for MAPISTORE
   function calls.
 */

#ifndef	__MAPISTORE_ERRORS_H
#define	__MAPISTORE_ERRORS_H

/**
   The function call succeeded.
 */
#define	MAPISTORE_SUCCESS			0

/**
   The function call failed for some non-specific reason.
 */
#define	MAPISTORE_ERROR				1

/**
   The function call failed because it was unable to allocate the
   memory required by underlying operations.
 */
#define	MAPISTORE_ERR_NO_MEMORY			2

/**
   The function call failed because underlying context has already
   been initialized
 */
#define	MAPISTORE_ERR_ALREADY_INITIALIZED	3

/**
   The function call failed because context has not been initialized.
 */
#define	MAPISTORE_ERR_NOT_INITIALIZED		4

/**
   The function call failed because an internal mapistore storage
   component has corrupted data.
 */
#define	MAPISTORE_ERR_CORRUPTED			5

/**
   The function call failed because one of the function parameters is
   invalid
 */
#define	MAPISTORE_ERR_INVALID_PARAMETER		6

/**
   The function call failed because the directory doesn't exist
 */
#define	MAPISTORE_ERR_NO_DIRECTORY		7

/**
   The function call failed because the underlying function couldn't
   open a database.
 */
#define	MAPISTORE_ERR_DATABASE_INIT		8

/**
   The function call failed because the underlying function didn't run
   a database operation successfully.
 */
#define	MAPISTORE_ERR_DATABASE_OPS		9

/**
   The function failed to register a storage backend
 */
#define	MAPISTORE_ERR_BACKEND_REGISTER		10

/**
   One of more storage backend initialization functions failed to
   complete successfully.
 */
#define	MAPISTORE_ERR_BACKEND_INIT		11

/**
   The function failed because mapistore failed to create a context
 */
#define	MAPISTORE_ERR_CONTEXT_FAILED		12

/**
   The function failed because the provided namespace is invalid
 */
#define	MAPISTORE_ERR_INVALID_NAMESPACE		13

#endif /* ! __MAPISTORE_ERRORS_H */
