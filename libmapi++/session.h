/*
   libmapi C++ Wrapper
   Session Class

   Copyright (C) Alan Alvarez 2008.

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

#ifndef LIBMAPIPP__SESSION_H__
#define LIBMAPIPP__SESSION_H__

#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>

#include <iostream> // for debugging

#include <libmapi++/clibmapi.h>
#include <libmapi++/mapi_exception.h>
#include <libmapi++/message_store.h>

using std::cout;
using std::endl;

namespace libmapipp
{
/**
 * This class represents a MAPI %session with the Exchange Server
 *
 * The %session is best thought of as the connection by a single user to a
 * single server. It is possible to have multiple sessions open at the same
 * time.
 *
 * A key concept in the %session is that of a \em profile, and the %profile
 * database. The %profile is a pre-established data set that specifies the
 * server information (e.g. server name / IP address, and Exchange domain)
 * and client information (e.g. the user name, preferred language, and
 * optionally the users password). The profiles are stored in a local store
 * (Samba LDB database) known as the %profile store or %profile database. One
 * of the profiles in the %profile database can be designated as the default
 * %profile (which is particularly useful given that most users may only have
 * one). See the profile class documentation for more information.
 *
 * The general concept is that you create a %session %object, log in to the
 * %session using a selected %profile name, and then work with the message_store
 * associated with that %session. The message_store is only valid so long
 * as the session is valid.
 */
class session {
	public:
		/**
		 * \brief Constructor
		 *
		 * \param profiledb An absolute path specifying the location of the
		 * %profile database. If not specified (or ""  is specified) the default
		 * location will be used (~/.openchange.profiles.ldb).
		 *
		 * \param debug Whether to output debug information to stdout
		 */
		explicit session(const std::string& profiledb = "", const bool debug = false) throw(std::runtime_error, mapi_exception);

		/**
		 * \brief Log-in to the Exchange Server
		 *
		 * \param profile_name The name of the profile to use to login with.
		 * If not specified (or "" is specified), then the default %profile will
		 * be used.
		 *
		 * \param password The password to use to login with. It is possible to
		 * omit this if the password is stored in the %profile database.
		 */
		void login(const std::string& profile_name = "", const std::string& password = "") throw(mapi_exception); 

		/**
		 * \brief The name of the profile that is in use
		 *
		 * Calling this method only makes sense if login() has been called with
		 * this %session object.
		*/
		std::string get_profile_name() const { return m_profile_name; }

		/**
		 * \brief Obtain a reference to the message_store associated with this %session
		 *
		 * \return The message_store associated with this %session.
		 */
		message_store& get_message_store() throw() { return *m_message_store; }

		/**
		 * \brief Obtain a talloc memory context for this %session
		 *
		 * This pointer can be used for subsequent memory allocations, and
		 * these will be cleaned up when the %session is terminated.
		 */
		TALLOC_CTX* get_memory_ctx() throw() { return m_memory_ctx; }

		/**
		 * \brief Uninitialize and clean up the MAPI %session.
		 */
		~session()
		{
			uninitialize();
		}

		/**
		 * \brief The underlying mapi_session
		 * 
		 * Exposing this is temporary. Maybe be removed when we sort it out
		 */
		mapi_session* get_mapi_session() throw() { return m_session; }

	private:
		mapi_session		*m_session;
		struct mapi_context	*m_mapi_context;
		TALLOC_CTX		*m_memory_ctx;
		message_store		*m_message_store;
		std::string		m_profile_name;

		void uninitialize() throw()
		{
			if (m_message_store) {
				delete m_message_store;
				m_message_store = 0;
			}
			if (m_mapi_context) {
				MAPIUninitialize(m_mapi_context);
				m_mapi_context = 0;
			}
			talloc_free(m_memory_ctx);
			m_memory_ctx = 0;
		}
};

} // namespace libmapipp

#endif //!LIBMAPIPP__SESSION_H__
