/*
   OpenChange MAPI implementation.

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

#ifndef __MAPI_PROVIDER_H
#define __MAPI_PROVIDER_H


/* forward decls */
struct mapi_object;
struct mapi_profile;
struct mapi_notify_ctx;

enum PROVIDER_ID {
	PROVIDER_ID_EMSMDB = 0x1,
	PROVIDER_ID_NSPI = 0x2,
	PROVIDER_ID_UNKNOWN
};

struct mapi_provider {
	enum PROVIDER_ID	id;
	void			*ctx;
};

struct mapi_objects {
	struct mapi_object	*object;
	struct mapi_objects	*prev;
	struct mapi_objects	*next;
};

struct mapi_session {
	struct mapi_provider		*emsmdb;
	struct mapi_provider		*nspi;
	struct mapi_profile		*profile;
	struct mapi_notify_ctx		*notify_ctx;
	struct mapi_objects		*objects;
	struct mapi_context		*mapi_ctx;
	uint8_t				logon_ids[255];

	struct mapi_session		*next;
	struct mapi_session		*prev;
};

#endif /* !__MAPI_PROVIDER_H */
