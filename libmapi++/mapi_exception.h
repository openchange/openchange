/*
   libmapi C++ Wrapper
   MAPI Exception Class

   Copyright (C) Alan Alvarez 2007.

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

#ifndef LIBMAPIPP__MAPI_EXCEPTION_H__
#define LIBMAPIPP__MAPI_EXCEPTION_H__

#include <exception>
#include <map>
#include <iostream>
#include <string>

#include <cstring>

#include <libmapi++/clibmapi.h>

#define STATUS_TABLE_INSERT(status) sm_status_map.insert(status_map::value_type(status, #status));

namespace libmapipp {

class mapi_exception : public std::exception
{
	public:
		explicit mapi_exception(enum MAPISTATUS status, const std::string& origin = "") : std::exception(), m_status(status), m_origin(origin), m_what_string(origin)
		{
			status_map::iterator iter = sm_status_map.find(m_status);

			m_what_string += ": ";
			m_what_string += (iter != sm_status_map.end()) ? iter->second : "Unknown MAPISTATUS value";
		}

		virtual const char* what() const throw() { return m_what_string.c_str(); }

		enum MAPISTATUS get_status() const { return m_status; }

		virtual ~mapi_exception() throw() {}

	private:
		enum MAPISTATUS	m_status;
		std::string	m_origin;
		std::string	m_what_string;
		friend class session;

		typedef std::map<enum MAPISTATUS, const char*> status_map;
		static status_map	sm_status_map;

		static void fill_status_map()
		{
			static bool filled = false;

			if (!filled) {
				STATUS_TABLE_INSERT(MAPI_E_SUCCESS);
				STATUS_TABLE_INSERT(MAPI_E_CALL_FAILED);
				STATUS_TABLE_INSERT(MAPI_E_NO_SUPPORT);
				STATUS_TABLE_INSERT(MAPI_E_BAD_CHARWIDTH);
				STATUS_TABLE_INSERT(MAPI_E_STRING_TOO_LONG);
				STATUS_TABLE_INSERT(MAPI_E_UNKNOWN_FLAGS);
				STATUS_TABLE_INSERT(MAPI_E_INVALID_ENTRYID);
				STATUS_TABLE_INSERT(MAPI_E_INVALID_OBJECT);
				STATUS_TABLE_INSERT(MAPI_E_OBJECT_CHANGED);
				STATUS_TABLE_INSERT(MAPI_E_OBJECT_DELETED);
				STATUS_TABLE_INSERT(MAPI_E_BUSY);
				STATUS_TABLE_INSERT(MAPI_E_NOT_ENOUGH_DISK);
				STATUS_TABLE_INSERT(MAPI_E_NOT_ENOUGH_RESOURCES);
				STATUS_TABLE_INSERT(MAPI_E_NOT_FOUND);
				STATUS_TABLE_INSERT(MAPI_E_VERSION);
				STATUS_TABLE_INSERT(MAPI_E_LOGON_FAILED);
				STATUS_TABLE_INSERT(MAPI_E_SESSION_LIMIT);
				STATUS_TABLE_INSERT(MAPI_E_USER_CANCEL);
				STATUS_TABLE_INSERT(MAPI_E_UNABLE_TO_ABORT);
				STATUS_TABLE_INSERT(ecRpcFailed);
				STATUS_TABLE_INSERT(MAPI_E_DISK_ERROR);
				STATUS_TABLE_INSERT(MAPI_E_TOO_COMPLEX);
				STATUS_TABLE_INSERT(MAPI_E_BAD_COLUMN);
				STATUS_TABLE_INSERT(MAPI_E_EXTENDED_ERROR);
				STATUS_TABLE_INSERT(MAPI_E_COMPUTED);
				STATUS_TABLE_INSERT(MAPI_E_CORRUPT_DATA);
				STATUS_TABLE_INSERT(MAPI_E_UNCONFIGURED);
				STATUS_TABLE_INSERT(MAPI_E_FAILONEPROVIDER);
				STATUS_TABLE_INSERT(MAPI_E_UNKNOWN_CPID);
				STATUS_TABLE_INSERT(MAPI_E_UNKNOWN_LCID);
				STATUS_TABLE_INSERT(MAPI_E_PASSWORD_CHANGE_REQUIRED);
				STATUS_TABLE_INSERT(MAPI_E_PASSWORD_EXPIRED);
				STATUS_TABLE_INSERT(MAPI_E_INVALID_WORKSTATION_ACCOUNT);
				STATUS_TABLE_INSERT(MAPI_E_INVALID_ACCESS_TIME);
				STATUS_TABLE_INSERT(MAPI_E_ACCOUNT_DISABLED);
				STATUS_TABLE_INSERT(MAPI_E_END_OF_SESSION);
				STATUS_TABLE_INSERT(MAPI_E_UNKNOWN_ENTRYID);
				STATUS_TABLE_INSERT(MAPI_E_MISSING_REQUIRED_COLUMN);
				STATUS_TABLE_INSERT(MAPI_E_BAD_VALUE);
				STATUS_TABLE_INSERT(MAPI_E_INVALID_TYPE);
				STATUS_TABLE_INSERT(MAPI_E_TYPE_NO_SUPPORT);
				STATUS_TABLE_INSERT(MAPI_E_UNEXPECTED_TYPE);
				STATUS_TABLE_INSERT(MAPI_E_TOO_BIG);
				STATUS_TABLE_INSERT(MAPI_E_DECLINE_COPY);
				STATUS_TABLE_INSERT(MAPI_E_UNEXPECTED_ID);
				STATUS_TABLE_INSERT(MAPI_E_UNABLE_TO_COMPLETE);
				STATUS_TABLE_INSERT(MAPI_E_TIMEOUT);
				STATUS_TABLE_INSERT(MAPI_E_TABLE_EMPTY);
				STATUS_TABLE_INSERT(MAPI_E_TABLE_TOO_BIG);
				STATUS_TABLE_INSERT(MAPI_E_INVALID_BOOKMARK);
				STATUS_TABLE_INSERT(MAPI_E_WAIT);
				STATUS_TABLE_INSERT(MAPI_E_CANCEL);
				STATUS_TABLE_INSERT(MAPI_E_NOT_ME);
				STATUS_TABLE_INSERT(MAPI_E_CORRUPT_STORE);
				STATUS_TABLE_INSERT(MAPI_E_NOT_IN_QUEUE);
				STATUS_TABLE_INSERT(MAPI_E_NO_SUPPRESS);
				STATUS_TABLE_INSERT(MAPI_E_COLLISION);
				STATUS_TABLE_INSERT(MAPI_E_NOT_INITIALIZED);
				STATUS_TABLE_INSERT(MAPI_E_NON_STANDARD);
				STATUS_TABLE_INSERT(MAPI_E_NO_RECIPIENTS);
				STATUS_TABLE_INSERT(MAPI_E_SUBMITTED);
				STATUS_TABLE_INSERT(MAPI_E_HAS_FOLDERS);
				STATUS_TABLE_INSERT(MAPI_E_HAS_MESAGES);
				STATUS_TABLE_INSERT(MAPI_E_FOLDER_CYCLE);
				STATUS_TABLE_INSERT(MAPI_E_AMBIGUOUS_RECIP);
				STATUS_TABLE_INSERT(MAPI_E_NO_ACCESS);
				STATUS_TABLE_INSERT(MAPI_E_INVALID_PARAMETER);
				STATUS_TABLE_INSERT(MAPI_E_RESERVED);

				filled = true;
			}
		}	
};

} // namespace libmapipp

#endif //!LIBMAPIPP__EXCEPTION_H__
