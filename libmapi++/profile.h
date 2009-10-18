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

		/* Create an new profile database
		 *
		 * \param profiledb the absolute path to the profile database intended to be created
		 * \param ldif_path the absolute path to the LDIF information to use for initial setup
		 *
		 */
		bool static create_profile_store(const char* profiledb, const char* ldif_path = NULL)
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
		bool static create_profile_store(const std::string& profiledb, const std::string& ldif_path)
		{
			return create_profile_store(profiledb.c_str(), ldif_path.c_str());
		}

		/**
		 * Make the specified profile the default profile
		 *
		 * \param profname the name of the profile to make default
		 */
		bool static set_default(const char* profname)
		{
			return (SetDefaultProfile(profname) == MAPI_E_SUCCESS);
		}

		/**
		 * Make the specified profile the default profile
		 *
		 * \param profname the name of the profile to make default
		 */
		bool static set_default(const std::string& profname)
		{
			return set_default(profname.c_str());
		}

		/**
		 * Get the default profile name
		 *
		 * \return the name of the default profile
		 */
		std::string static get_default_profile() throw (mapi_exception)
		{
			char* profname = NULL;
			if (GetDefaultProfile(&profname) != MAPI_E_SUCCESS)
				throw mapi_exception(GetLastError(), "profile::get_default_profile : GetDefaultProfile()");

			return std::string(profname);
		}

		~profile()
		{
			if (m_profile)
				::ShutDown(m_profile);
		}


	private:
		mapi_profile	*m_profile;
};

} // namespace libmapipp

#endif //!LIBMAPIPP__PROFILE_H__
