/*
   libmapi C++ Wrapper

   Test application for profile database and profile classes

   Copyright (C) Brad Hards <bradh@openchange.org> 2010.

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

#include <exception>
#include <vector>
#include <string>

#include <libmapi++/libmapi++.h>
#define PROFILEDB_NAME_TEMPLATE "/tmp/mapidbXXXXXX"
int main ()
{
	try {
		libmapipp::profile_database db;

		std::cout << "default profile name: " << db.get_default_profile_name() << std::endl;

		{
			char *tmpname = (char*) calloc(sizeof(PROFILEDB_NAME_TEMPLATE) + 1, sizeof(char));
			strncpy(tmpname, PROFILEDB_NAME_TEMPLATE, sizeof(PROFILEDB_NAME_TEMPLATE));
			int ret = mkstemp(tmpname);
			if (ret < 0) {
				std::cout << "failed to create temporary file: " << strerror(errno) << std::endl;
			}
			if (libmapipp::profile_database::create_profile_store(tmpname)) {
				std::cout << "success creating a temporary profile store" << std::endl;
			} else {
				std::cout << "failed to create a temporary profile store" << std::endl;
			}
			unlink(tmpname);
		}
		
		{
			char *tmpname2 = (char*) calloc(sizeof(PROFILEDB_NAME_TEMPLATE) + 1, sizeof(char));
			strncpy(tmpname2, PROFILEDB_NAME_TEMPLATE, sizeof(PROFILEDB_NAME_TEMPLATE));
			int ret = mkstemp(tmpname2);
			if (ret < 0) {
				std::cout << "failed to create temporary file: " << strerror(errno) << std::endl;
			}
			if (libmapipp::profile_database::create_profile_store(std::string(tmpname2))) {
				std::cout << "success creating a temporary profile store with std::string" << std::endl;
			} else {
				std::cout << "failed to create a temporary profile store with std::string" << std::endl;
			}
			unlink(tmpname2);
		}

		std::cout << "finished profile and profile database tests" << std::endl;
	}
	catch (libmapipp::mapi_exception e) // Catch any mapi exceptions
	{
		std::cout << "MAPI Exception @ main: " <<  e.what() << std::endl;
	}
	catch (std::runtime_error e) // Catch runtime exceptions
	{
		std::cout << "std::runtime_error exception @ main: " << e.what() << std::endl;

	}

	return 0;
}
