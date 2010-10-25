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

inline std::string get_default_profile_path()
{
	const char* profile_path = getenv("HOME");
	std::string retval = "";
	if (profile_path) {
		retval = profile_path;
		retval += "/.openchange/profiles.ldb";
	}

	return retval;
}

namespace libmapipp {

/**
 * This class represents a user %profile database
 *
 * \todo possibly rename profile class to profile_database?
 * \todo we should be able to create a profile using libmapi++ classes
 * \todo we should be able to delete a profile using libmapi++ classes
 * \todo maybe move some of the session.h documentation on profiles to profile.h?
 */
class profile 
{
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
		explicit profile(const std::string& profiledb = "")  throw(std::runtime_error, mapi_exception)
		:  m_profile(0), m_mapi_context(0), m_memory_ctx(talloc_named(NULL, 0, "libmapipp-profile"))
		{
			std::string profile_path;

			// If profile is not provided, attempt to get it from default location
			// (~/.openchange/profiles.ldb)
			if (profiledb == "") {
				profile_path = get_default_profile_path();
				if (profile_path == "") {
					talloc_free(m_memory_ctx);
					throw std::runtime_error("libmapipp::session(): Failed to get $HOME env variable");
				}
			} else {
				profile_path = profiledb;
			}

			if (MAPIInitialize(&m_mapi_context, profile_path.c_str()) != MAPI_E_SUCCESS) {
				talloc_free(m_memory_ctx);
				throw mapi_exception(GetLastError(), "session::session : MAPIInitialize");
			}
		}

		/* Create an new profile database
		 *
		 * \param profiledb the absolute path to the profile database intended to be created
		 * \param ldif_path the absolute path to the LDIF information to use for initial setup
		 *
		 */
		bool create_profile_store(const char* profiledb, const char* ldif_path = NULL)
		{
                        if (ldif_path == NULL)
                            ldif_path = ::mapi_profile_get_ldif_path();
			return (CreateProfileStore(profiledb, ldif_path) == MAPI_E_SUCCESS);
		}
 
 		/**
		 * Create an new profile database
		 *
		 * \param profiledb the absolute path to the profile database intended to be created
		 * \param ldif_path the absolute path to the LDIF information to use for initial setup
		 *
		 */
		bool create_profile_store(const std::string& profiledb, const std::string& ldif_path)
		{
			return create_profile_store(profiledb.c_str(), ldif_path.c_str());
		}

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
		std::string get_default_profile() throw (mapi_exception)
		{
			char* profname = NULL;
			if (GetDefaultProfile(m_mapi_context, &profname) != MAPI_E_SUCCESS)
				throw mapi_exception(GetLastError(), "profile::get_default_profile : GetDefaultProfile()");
			return std::string(profname);
		}

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
