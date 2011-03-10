/*
   MAPI Proxy

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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

/**
   \file dcesrv_mapiproxy_session.c

   \brief session API for mapiproxy modules
 */


/**
   \details Create and return an allocated pointer to a mpm session

   \param mem_ctx pointer to the memory context
   \param serverid reference to the session context server identifier
   structure
   \param context_id reference to the context identifier

   \return Pointer to an allocated mpm_session structure on success,
   otherwise NULL
 */
struct mpm_session *mpm_session_new(TALLOC_CTX *mem_ctx, 
				     struct server_id serverid,
				     uint32_t context_id)
{
	struct mpm_session	*session = NULL;

	if (!mem_ctx) return NULL;

	session = talloc_zero(mem_ctx, struct mpm_session);
	if (!session) return NULL;

	session->server_id.id = serverid.id;
	session->server_id.id2 = serverid.id2;
	session->server_id.node = serverid.node;
	session->context_id = context_id;
	session->ref_count = 0;
	session->destructor = NULL;
	session->private_data = NULL;

	return session;
}


/**
   \details Increment the ref_count associated to a session

   \param session pointer to the session where to increment ref_count

   \return true on success, otherwise false
 */
bool mpm_session_increment_ref_count(struct mpm_session *session)
{
	if (!session) return false;

	session->ref_count += 1;

	return true;
}


/**
   \details Create and return an allocated pointer to a mpm session

   \param mem_ctx pointer to the memory context
   \param dce_call pointer to the session context

   \return Pointer to an allocated mpm_session structure on success,
   otherwise NULL
 */
struct mpm_session *mpm_session_init(TALLOC_CTX *mem_ctx,
				     struct dcesrv_call_state *dce_call)
{
	if (!mem_ctx) return NULL;
	if (!dce_call) return NULL;
	if (!dce_call->conn) return NULL;
	if (!dce_call->context) return NULL;

	return mpm_session_new(mem_ctx, dce_call->conn->server_id, 
			       dce_call->context->context_id);
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
   \details Release a mapiproxy session context

   \param session pointer to the mpm session context

   \return true on success, otherwise false
 */
bool mpm_session_release(struct mpm_session *session)
{
	bool		ret;

	if (!session) return false;

	if (session->ref_count) {
		session->ref_count -= 1;
		return false;
	}

	if (session->destructor) {
		ret = session->destructor(session->private_data);
		if (ret == false) return ret;
	}

	talloc_free(session);

	return true;
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

	if ((session->server_id.id   == sid.id) &&
	    (session->server_id.id2  == sid.id2) &&
	    (session->server_id.node == sid.node) &&
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

/**
   \details Helper to compare two session unique identifiers

   \param uuid1 a uuid
   \param uuid2 another uuid

   \return true on success, otherwise false
 */
bool mpm_uuid_cmp(const struct GUID *uuid1, const struct GUID *uuid2)
{
        bool rc;

        rc = (uuid1 == uuid2
              || ((uuid1 != NULL) && (uuid2 != NULL)
                  && (uuid1->time_low == uuid2->time_low)
                  && (uuid1->time_mid == uuid2->time_mid)
                  && (uuid1->time_hi_and_version == uuid2->time_hi_and_version)
                  && (memcmp(uuid1->clock_seq, uuid2->clock_seq, 2) == 0)
                  && (memcmp(uuid1->node, uuid2->node, 6) == 0)));

        return rc;
}
