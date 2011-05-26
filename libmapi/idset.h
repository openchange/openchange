/*
   OpenChange MAPI implementation.

   Copyright (C) Wolfgang Sourdeau 2011

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

#ifndef __LIBMAPI_IDSET_H_
#define __LIBMAPI_IDSET_H_

#include <gen_ndr/misc.h>

struct idset {
	struct GUID		guid;
	uint32_t		range_count;
	struct globset_range	*ranges;
};

struct globset_range {
	uint64_t		low;
	uint64_t		high;
	struct globset_range	*prev;
	struct globset_range	*next;
};

#endif /* __LIBMAPI_IDSET_H_ */
