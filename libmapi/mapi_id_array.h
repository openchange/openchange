/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2008.

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

#ifndef	__MAPI_ID_ARRAY_H
#define	__MAPI_ID_ARRAY_H

typedef struct mapi_container_list {
	struct mapi_container_list	*prev;
	struct mapi_container_list	*next;
	mapi_id_t			id;
} mapi_container_list_t;

typedef struct mapi_id_array {
	uint16_t       		count;
	mapi_container_list_t	*lpContainerList;
} mapi_id_array_t;

#endif /* __MAPI_ID_ARRAY_H */
