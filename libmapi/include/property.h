/* 
   OpenChange Project

   property headers

   Copyright (C) Gregory Schiro 2006
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _PROPERTY_H_
#define _PROPERTY_H_

struct				SPropTagByFunc
{
	char			*FuncName;
	uint32_t		nb_tags;
	uint32_t		poped_yet;
	uint32_t		*aulPropTag;
};

struct				SPropTagGBuild
{
	TALLOC_CTX		*mem_ctx;
	int			cur_idx;
	int			tag_idx;
	struct SPropTagByFunc   *TagsT;
};

struct				mapitags_x500
{
        uint32_t                mapitag;
        const char              *x500;
};

# define GENERATED       (-1)

# define STRING_TYPE	(0)
# define INT_TYPE	(1)
# define DOUBLE_TYPE	(2)
# define FLOAT_TYPE	(3)

#endif /* !_PROPERTY_H_ */

