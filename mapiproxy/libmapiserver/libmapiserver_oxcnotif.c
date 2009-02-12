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

   \brief OXCNOTIF Rops
 */

#include "libmapiserver.h"

/**
   \details Calculate RegisterNotification Rop size

   \return Size of RegisterNotification response
 */
_PUBLIC_ uint16_t libmapiserver_RopRegisterNotification_size(void)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}
