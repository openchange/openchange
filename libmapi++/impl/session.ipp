/*
   libmapi C++ Wrapper
   Session Class implementation.

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

#include <libmapi++/message_store.h>
#include <libmapi++/profile.h>

namespace libmapipp {

inline std::string session::get_default_profile_path()
{
	const char* profile_path = getenv("HOME");
	std::string retval = "";
	if (profile_path) {
		retval = profile_path;
		retval += "/.openchange/profiles.ldb";
	}

	return retval;
}

session::session(const std::string& profiledb, bool debug) throw(std::runtime_error, mapi_exception) 
: m_session(NULL), m_memory_ctx(talloc_named(NULL, 0, "libmapi++")), m_message_store(new message_store(*this))
{
	mapi_exception::fill_status_map();

	std::string profile_path;

	// If profile is not provided, attempt to get it from default location
	// (~/.openchange/profiles.ldb)
	if (profiledb == "") {
		profile_path = get_default_profile_path();
		if (profile_path == "") {
			talloc_free(m_memory_ctx);
			delete m_message_store;
			throw std::runtime_error("libmapipp::session(): Failed to get $HOME env variable");
		}
	} else {
		profile_path = profiledb;
	}

	if (MAPIInitialize(profile_path.c_str()) != MAPI_E_SUCCESS) {
		talloc_free(m_memory_ctx);
		delete m_message_store;
		throw mapi_exception(GetLastError(), "session::session : MAPIInitialize");
	}

	if (debug) global_mapi_ctx->dumpdata = true;
}

void session::login(const std::string& profile_name, const std::string& password) throw (mapi_exception)
{
	m_profile_name = profile_name;
	if (m_profile_name == "") { // if profile is not set, try to get default profile
		try {
			m_profile_name = profile::get_default_profile();
		} catch(mapi_exception e) {
			uninitialize();
			throw;
		}
	}

	if (MapiLogonEx(&m_session, m_profile_name.c_str(), (password != "") ? password.c_str() : 0 ) != MAPI_E_SUCCESS) {
		uninitialize();
		throw mapi_exception(GetLastError(), "session::session : MapiLogonEx");
	}

	try {
		m_message_store->open(m_session);
	} catch (mapi_exception e) {
		throw;
	}
}

inline void session::uninitialize() throw()
{
	talloc_free(m_memory_ctx);
	MAPIUninitialize();
	delete m_message_store;
}

} // namespace libmapipp
