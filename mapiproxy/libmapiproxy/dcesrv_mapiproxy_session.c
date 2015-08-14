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
#include "utils/dlinklist.h"

/**
   \file dcesrv_mapiproxy_session.c

   \brief session API for mapiproxy modules
 */

static struct mpm_session	*mpm_sessions = NULL;

/**
   \details Create and return an allocated pointer to a mpm session

   \param mem_ctx pointer to the memory context
   \param serverid reference to the session context server identifier
   structure
   \param context_id reference to the context identifier

   \return Pointer to an allocated mpm_session structure on success,
   otherwise NULL
 */
static struct mpm_session *mpm_session_new(struct server_id serverid,
				    uint32_t context_id,
				    const char *username,
				    struct GUID	*uuid)
{
	struct mpm_session	*session = NULL;

	session = talloc_zero(NULL, struct mpm_session);
	if (!session) return NULL;

	session->server_id = serverid;
	session->context_id = context_id;
	if (uuid) session->uuid = *uuid;
	session->destructor = NULL;
	session->private_data = NULL;

	/* Released RPC connection, session may be kept to avoid premature release
	of private_data, that will be cleared when all user sessions are released */
	session->released = false;
	session->username = talloc_strdup(session, username);
	if (!session->username) return NULL;

	DLIST_ADD_END(mpm_sessions, session, struct mpm_session *);

	return session;
}


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
	if (!dce_call) return NULL;
	if (!dce_call->conn) return NULL;
	if (!dce_call->context) return NULL;

	return mpm_session_new(dce_call->conn->server_id,
			       dce_call->context->context_id,
			       dcesrv_call_account_name(dce_call),
			       uuid);
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
   \details Release a mapiproxy session, private data (if any) will be released
   when all user sessions are gone

   \param session pointer to the mpm session context

   \return true on success, otherwise false
 */
bool mpm_session_release(struct mpm_session *session)
{
	bool		ret;

	if (!session) return false;

	// TODO proper checking before release
	return false;

	if (session->destructor) {
		ret = session->destructor(session->private_data);
		if (ret == false) return ret;
	}

	talloc_free(session);

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
