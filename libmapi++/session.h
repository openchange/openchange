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

using std::cout;
using std::endl;

namespace libmapipp
{
class message_store;
/// This class represents a MAPI session with the Exchange Server
class session {
	public:
		/**
		 * \brief Constructor
		 * \param profiledb An absolute path specifying where the profile database is.\n
		 *        If none or ""  is specified the default location will be used (~/.openchange.profiles.ldb).
		 *
		 * \param debug Whether to output debug information to stdout
		 */
		session(const std::string& profiledb = "", const bool debug = false) throw(std::runtime_error, mapi_exception);

		/**
		 * \brief Log-in to the Exchange Server
		 * \param profile_name The name of the profile, if left blank the default profile will be used.
		 * \param password The password to use to login with.\n
		 *        It is possible to omit this if the password is stored in the profile database.
		 */
		void login(const std::string& profile_name = "", const std::string& password = "") throw(mapi_exception); 

		static std::string get_default_profile_path();

		std::string get_profile_name() const { return m_profile_name; }

		/** \brief Returns a reference to the message_store associated with this session
		 *  \return The message_store associated with this session.
		 */
		message_store& get_message_store() throw() { return *m_message_store; }

		/// \brief Get Session TALLOC Memory Context pointer.
		TALLOC_CTX* get_memory_ctx() throw() { return m_memory_ctx; }

		/// Uninitializes the MAPI session.
		~session()
		{
			uninitialize();
		}

	private:
		mapi_session		*m_session;
		TALLOC_CTX		*m_memory_ctx;
		message_store		*m_message_store;
		std::string		m_profile_name;

		void uninitialize() throw();
};

} // namespace libmapipp

#include <libmapi++/impl/session.ipp>

#endif //!LIBMAPIPP__SESSION_H__
