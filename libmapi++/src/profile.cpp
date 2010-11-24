/*
   libmapi C++ Wrapper
   Profile and Profile database Classes

   Copyright (C) Alan Alvarez 2008.
   Copyright (C) Brad Hards <bradh@openchange.org> 2010

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

#include <libmapi++/profile.h>

namespace libmapipp {


profile_database::profile_database(const std::string& profiledb_path)  throw(std::runtime_error, mapi_exception)
  : m_mapi_context(0), m_memory_ctx(talloc_named(NULL, 0, "libmapipp-profile"))
{
	std::string profile_path;

	// If profile is not provided, attempt to get it from default location
	// (~/.openchange/profiles.ldb)
	if (profiledb_path == "") {
		profile_path = get_default_profile_path();
		if (profile_path == "") {
			talloc_free(m_memory_ctx);
			throw std::runtime_error("libmapipp::session(): Failed to get $HOME env variable");
		}
	} else {
		profile_path = profiledb_path;
	}
	enum MAPISTATUS	retval = MAPIInitialize(&m_mapi_context, profile_path.c_str());
	if (retval != MAPI_E_SUCCESS) {
		talloc_free(m_memory_ctx);
		throw mapi_exception(retval, "session::session : MAPIInitialize");
	}
}


profile_database::~profile_database()
{
	if (m_mapi_context) {
		MAPIUninitialize(m_mapi_context);
	}
	talloc_free(m_memory_ctx);
}



std::string profile_database::get_default_profile_name() throw (mapi_exception)
{
	char* profname = NULL;
	enum MAPISTATUS	retval = GetDefaultProfile(m_mapi_context, &profname);
	if (retval != MAPI_E_SUCCESS)
		throw mapi_exception(retval, "profile::get_default_profile : GetDefaultProfile()");
	return std::string(profname);
}



bool profile_database::create_profile_store(const char* profiledb, const char* ldif_path)
{
	if (ldif_path == NULL)
	    ldif_path = ::mapi_profile_get_ldif_path();
	return (CreateProfileStore(profiledb, ldif_path) == MAPI_E_SUCCESS);
}



bool profile_database::create_profile_store(const std::string& profiledb, const std::string& ldif_path)
{
	if (ldif_path == "") {
		return (create_profile_store(profiledb.c_str()));
	} else {
		return create_profile_store(profiledb.c_str(), ldif_path.c_str());
	}
}



std::string profile_database::get_default_profile_path()
{
	const char* profile_path = getenv("HOME");
	std::string retval = "";
	if (profile_path) {
		retval = profile_path;
		retval += "/.openchange/profiles.ldb";
	}

	return retval;
}


} // namespace libmapipp