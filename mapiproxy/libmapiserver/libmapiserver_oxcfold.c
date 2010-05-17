/*
   libmapiserver - MAPI library for Server side

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
   \file libmapiserver_oxcfold.c

   \brief OXCFOLD Rops
 */

#include "libmapiserver.h"

/**
   \details Calculate OpenFolder Rop size

   \param response pointer to the OpenFolder EcDoRpc_MAPI_REPL
   structure

   \return Size of OpenFolder response
 */
_PUBLIC_ uint16_t libmapiserver_RopOpenFolder_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPOPENFOLDER;
	
	/* No ghosted folder for the moment */
	return size;
}


/**
   \details Calculate GetHierarchyTable Rop size

   \param response pointer to the GetHierarchyTable EcDoRpc_MAPI_REPL
   structure

   \return Size of GetHierarchyTable response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetHierarchyTable_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETHIERARCHYTABLE;

	return size;
}


/**
   \details Calculate GetContentsTable Rop size

   \param response pointer to the GetContentsTable EcDoRpc_MAPI_REPL
   structure

   \return Size of GetContentsTable response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetContentsTable_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETCONTENTSTABLE;
	
	return size;
}


/**
   \details Calculate CreateFolder Rop size

   \param response pointer to the CreateFolder EcDoRpc_MAPI_REPL
   structure

   \retun Size of CreateFolder response
 */
_PUBLIC_ uint16_t libmapiserver_RopCreateFolder_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPCREATEFOLDER;

	return size;
}


/**
   \details Calculate DeleteFolder Rop size

   \param response pointer to the DeleteFolder EcDoRpc_MAPI_REPL
   structure

   \return Size of DeleteFolder response
 */
_PUBLIC_ uint16_t libmapiserver_RopDeleteFolder_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPDELETEFOLDER;

	return size;
}

/**
   \details Calculate DeleteMessage (0x1e) Rop size

   \param response pointer to the DeleteMessage EcDoRpc_MAPI_REPL
   structure

   \return Size of DeleteMessage response
 */
_PUBLIC_ uint16_t libmapiserver_RopDeleteMessage_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPDELETEMESSAGE;

	return size;
}