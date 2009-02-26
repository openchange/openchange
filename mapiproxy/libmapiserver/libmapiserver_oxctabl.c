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
   \file libmapiserver_oxctabl.c

   \brief OXCTABL Rops
 */

#include "libmapiserver.h"

/**
   \details Calculate SetColumns Rop size

   \param response pointer to the SetColumns EcDoRpc_MAPI_REPL
   structure

   \return Size of SetColumns response
 */
_PUBLIC_ uint16_t libmapiserver_RopSetColumns_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response) {
		return size;
	}

	size += SIZE_DFLT_ROPSETCOLUMNS;

	return size;
}


/**
   \details Calculate SortTable Rop size

   \param response pointer to the SortTable EcDoRpc_MAPI_REPL
   structure

   \return Size of SortTable response
 */
_PUBLIC_ uint16_t libmapiserver_RopSortTable_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response) {
		return size;
	}

	size += SIZE_DFLT_ROPSORTTABLE;

	return size;
}

/**
   \details Calculate QueryRows Rop size

   \param response pointer to the QueryRows EcDoRpc_MAPI_REPL
   structure

   \return Size of QueryRows response
 */
_PUBLIC_ uint16_t libmapiserver_RopQueryRows_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response) {
		return size;
	}

	size += SIZE_DFLT_ROPQUERYROWS;
	size += response->u.mapi_QueryRows.RowData.length;

	return size;
}


/**
   \details Calculate SeekRow Rop size

   \param response pointer to the SeekRow EcDoRpc_MAPI_REPL
   structure

   \return Size of SeekRow response
 */
_PUBLIC_ uint16_t libmapiserver_RopSeekRow_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response) {
		return size;
	}

	size += SIZE_DFLT_ROPSEEKROW;

	return size;
}
