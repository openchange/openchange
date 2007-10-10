/*
   OpenChange MAPI implementation.
   MAPI handles data structures

   Copyright (C) Julien Kerihuel 2007.
   Copyright (C) Fabien Le Mentec 2007.

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

#ifndef __MAPI_HANDLES_H
#define __MAPI_HANDLES_H

#include <libmapi/dlinklist.h>

struct mapi_handles {
	uint32_t		handle;

	struct mapi_handles	*prev;
	struct mapi_handles	*next;
};

#endif /* !__MAPI_HANDLES_H */
