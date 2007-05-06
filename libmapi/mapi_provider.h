/*
 *  Copyright (C) Julien Kerihuel  2007.
 *  Copyright (C) Fabien Le Mentec 2007.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __MAPI_PROVIDER_H
#define __MAPI_PROVIDER_H


/* forward decls */
struct mapi_profile;


enum PROVIDER_ID {
	PROVIDER_ID_EMSMDB = 0x1,
	PROVIDER_ID_NSPI = 0x2,
	PROVIDER_ID_UNKNOWN
};

struct mapi_provider {
	enum PROVIDER_ID	id;
	void			*ctx;
};

struct mapi_session {
	struct mapi_provider	*emsmdb;
	struct mapi_provider	*nspi;
	struct mapi_profile	*profile;

	struct mapi_session	*next;
	struct mapi_session	*prev;
};

#endif /* !__MAPI_PROVIDER_H */
