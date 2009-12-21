/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007.

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

#ifndef	__MAPI_NOTIFICATION_H__
#define	__MAPI_NOTIFICATION_H__

/* notification which takes:
 * - ulEvenType = type of notification
 * - void * = notification data
 * - void * = private data pointer
*/
typedef int (*mapi_notify_callback_t)(uint16_t, void *, void *);

typedef int (*mapi_notify_continue_callback_t)(void *);

struct notifications {
	uint32_t		ulConnection;		/* connection number */
	uint32_t		NotificationFlags;	/* events mask associated */
	mapi_id_t		parentID;		/* parent EntryID == FID here */
	mapi_notify_callback_t	callback;		/* callback to run when */
	void			*private_data;		/* private data for the callback */
	struct mapi_object     	obj_notif;		/* notification object */
	struct notifications	*prev;
	struct notifications	*next;
};

struct mapi_notify_ctx {
	struct NOTIFKEY		key;		/* unique identifier */
	int			fd;		/* UDP socket file descriptor */
	struct sockaddr		*addr;
	struct notifications	*notifications;
};

struct mapi_notify_continue_callback_data {
        mapi_notify_continue_callback_t callback; /* Consulted for continuing processing events*/
        void *data;                               /* Data for callback */
        struct timeval tv;                        /* Timeout for Select call */
};

#define	DFLT_NOTIF_PORT	2500

#endif /*!__MAPI_NOTIFICATION_H__ */
