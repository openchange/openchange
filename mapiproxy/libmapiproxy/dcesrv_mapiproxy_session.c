/*
   MAPI Proxy

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008
   Copyright (C) Carlos Pérez-Aradros Herce 2015

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

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "libmapiproxy.h"
#include "libmapi/libmapi.h"
#include "utils/dlinklist.h"

/**
   \file dcesrv_mapiproxy_session.c

   \brief session API for mapiproxy modules
 */

static struct mpm_session	*mpm_sessions = NULL;


/**
   \details Create and return an allocated pointer to a mpm session

   \param dce_call pointer to the session context
   \param uuid session

   \return Pointer to an allocated mpm_session structure on success,
   otherwise NULL
 */
struct mpm_session *mpm_session_init(struct dcesrv_call_state *dce_call,
				     struct GUID *uuid)
{
	struct mpm_session	*session = NULL;

	if (!dce_call) return NULL;
	if (!dce_call->conn) return NULL;
	if (!dce_call->context) return NULL;

	session = talloc_zero(NULL, struct mpm_session);
	if (!session) return NULL;

	session->server_id = dce_call->conn->server_id;
	session->context_id = dce_call->context->context_id;
	if (uuid) {
		session->uuid = *uuid;
	}
	session->destructor = NULL;
	session->private_data = NULL;

	/* Released RPC connection, session may be kept to avoid premature release
	of private_data, that will be cleared when all user sessions are released */
	session->released = false;
	session->username = talloc_strdup(session, dcesrv_call_account_name(dce_call));
	if (!session->username) return NULL;
	DLIST_ADD(mpm_sessions, session);

	return session;
}


/**
   \details Free session private data

   \param session to free

   \return true if data was released
 */
bool mpm_session_release(struct mpm_session *session) {
	if (!session) return false;

	if (session->destructor) {
		session->destructor(session->private_data);
		session->destructor = NULL;
		return true;
	}

	return false;
}


/**
   \details Set the mpm session destructor

   \param session pointer to the mpm session context
   \param destructor pointer to the destructor function

   \return true on success, otherwise false
 */
bool mpm_session_set_destructor(struct mpm_session *session,
				bool (*destructor)(void *))
{
	if (!session) return false;
	if (!destructor) return false;

	session->destructor = destructor;

	return true;
}


/**
   \details Set the mpm session pointer on private data

   \param session pointer to the mpm session context
   \param private_data generic pointer on private data

   \return true on success, otherwise false
 */
bool mpm_session_set_private_data(struct mpm_session *session,
				  void *private_data)
{
	if (!session) return false;

	session->private_data = private_data;

	return true;
}


/**
   \details Unbind RPC session

   This code will release mapiproxy session data (and remove it from
   mpm_sessions array) when all user sessions are marked as deleted


   \return true when session was released, false if only marked for release
 */
bool mpm_session_unbind(struct server_id *server_id, uint32_t context_id)
{
	struct mpm_session	*current, *next;
	char			*username = NULL;

	if (!server_id) return false;

	/* Mark all server_id/context_id sessions as released */
	for (current = mpm_sessions; current; current = current->next) {
		if (server_id_equal(server_id, &current->server_id) &&
		    context_id == current->context_id) {
			current->released = true;
			username = current->username;
		}
	}

	if (username) {
		/* At least one session marked, are all of them gone? */
		for (current = mpm_sessions; current; current = current->next) {
			if (strcmp(username, current->username) == 0 && !current->released) {
				OC_DEBUG(6, "Keeping %s sessions as some context is not unbinded yet", username);
				return false;
			}
		}

		/* If we reach here, all user contexts are marked for release, clean up */
		OC_DEBUG(5, "Cleaning sessions for user %s", username);
		username = talloc_strdup(NULL, username);
		current = mpm_sessions;
		while (current) {
			if (strcmp(username, current->username) == 0) {
				/* Destroy private data */
				mpm_session_release(current);

				/* Remove and free session entry */
				next = current->next;
				DLIST_REMOVE(mpm_sessions, current);
				talloc_free(current);
				current = next;
			} else {
				current = current->next;
			}
		}
		talloc_free(username);
	}

	return true;
}


/**
   \details Find and return a session by UUID (will return NULL if not found)

   \param Session UUID
 */
struct mpm_session *mpm_session_find_by_uuid(struct GUID *uuid)
{
	struct mpm_session	*session;

	if (!uuid) return NULL;

	for (session = mpm_sessions; session; session = session->next) {
		if (GUID_equal(uuid, &session->uuid)) {
			return session;
		}
	}

	return NULL;
}


/**
   \details Compare the mpm session with the session context one

   \param session pointer to the mapiproxy module session
   \param sid reference to a server_id structure to compare
   \param context_id the connection context id to compare
 */
bool mpm_session_cmp_sub(struct mpm_session *session,
			 struct server_id sid,
			 uint32_t context_id)
{
	if (!session) return false;

	if (memcmp(&session->server_id, &sid, sizeof(struct server_id) == 0) &&
	    (session->context_id == context_id)) {
		return true;
	}


	return false;
}


/**
   \details Compare the mpm session with the session context one

   This function is a wrapper on mpm_session_cmp_sub

   \param session pointer to the mapiproxy module session
   \param dce_call pointer to the session context

   \return true on success, otherwise false

   \sa mpm_session_cmp_sub
 */
bool mpm_session_cmp(struct mpm_session *session,
		     struct dcesrv_call_state *dce_call)
{
	if (!session || !dce_call) return false;

	return mpm_session_cmp_sub(session, dce_call->conn->server_id,
				   dce_call->context->context_id);
}
