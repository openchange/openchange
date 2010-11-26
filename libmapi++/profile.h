/*
   libmapi C++ Wrapper
   Profile Class

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


#ifndef LIBMAPIPP__PROFILE_H__
#define LIBMAPIPP__PROFILE_H__

#include <libmapi++/clibmapi.h>

#include <string>
#include <stdexcept>
#include <libmapi++/mapi_exception.h>

namespace libmapipp {

/**
 * This class represents a user %profile database
 *
 * \todo possibly rename profile class to profile_database?
 * \todo we should be able to create a profile using libmapi++ classes
 * \todo we should be able to delete a profile using libmapi++ classes
 * \todo maybe move some of the session.h documentation on profiles to profile.h?
 */
class profile_database
{
	public:
		/**
		 * \brief Constructor
		 *
		 * \param profiledb_path An absolute path specifying the location of the
		 * %profile database. If not specified (or ""  is specified) the default
		 * location will be used (~/.openchange.profiles.ldb).
		 */
		explicit profile_database(const std::string& profiledb_path = "")  throw(std::runtime_error, mapi_exception);

		/* Create an new profile database
		 *
		 * \param profiledb the absolute path to the profile database intended to be created
		 * \param ldif_path the absolute path to the LDIF information to use for initial setup
		 *
		 */
		static bool create_profile_store(const char* profiledb, const char* ldif_path = NULL);
 
 		/**
		 * Create an new profile database
		 *
		 * \param profiledb the absolute path to the profile database intended to be created
		 * \param ldif_path the absolute path to the LDIF information to use for initial setup
		 *
		 */
		static bool create_profile_store(const std::string& profiledb, const std::string& ldif_path = "");

		/**
		 * Make the specified profile the default profile
		 *
		 * \param profname the name of the profile to make default
		 */
		bool set_default(const char* profname)
		{
			return (SetDefaultProfile(m_mapi_context, profname) == MAPI_E_SUCCESS);
		}

		/**
		 * Make the specified profile the default profile
		 *
		 * \param profname the name of the profile to make default
		 */
		bool set_default(const std::string& profname)
		{
			return set_default(profname.c_str());
		}

		/**
		 * Get the default profile name
		 *
		 * \return the name of the default profile
		 */
		std::string get_default_profile_name() throw (mapi_exception);

		/**
		 * \brief The path to the default %profile database
		 *
		 * This method is not normally required to be called by user applications
		 * but might be useful under some circumstances.
		 */
		static std::string get_default_profile_path();

		~profile_database();

	private:
		struct mapi_context	*m_mapi_context;
		TALLOC_CTX		*m_memory_ctx;
};

class profile
{
	public:
		~profile()
		{
			if (m_profile) {
				::ShutDown(m_profile);
			}
			if (m_mapi_context) {
				MAPIUninitialize(m_mapi_context);
			}
			talloc_free(m_memory_ctx);
		}
	private:
		mapi_profile		*m_profile;
		struct mapi_context	*m_mapi_context;
		TALLOC_CTX		*m_memory_ctx;
};

} // namespace libmapipp

#endif //!LIBMAPIPP__PROFILE_H__
