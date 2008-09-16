/*
   libmapi C++ Wrapper

   Sample folder tree list application

   Copyright (C) Brad Hards <bradh@frogmouth.net> 2008.

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
#include <string>

#include <libmapi++/libmapi++.h>

int main ()
{
        try {
                // Initialize MAPI Session
                libmapipp::session mapi_session;
		// You could log in with a non-default profile here
                mapi_session.login();

		// Get the private (user) folders message store
		libmapipp::message_store &msg_store = mapi_session.get_message_store();

		// Get a property of the top level message store
		libmapipp::property_container msg_store_props = msg_store.get_property_container();
		msg_store_props << PR_DISPLAY_NAME; // you could use other properties here
		msg_store_props.fetch();

		// Display the property. You can also use a property_container_iterator 
		// to work through the properties, but in this case there is only one.
		std::cout << "Message store display name: "
			  << (const char*)msg_store_props[PR_DISPLAY_NAME]
			  << std::endl;

		// Fetch the folder list.
		// We start off by fetching the top level folder
		mapi_id_t top_folder_id = msg_store.get_default_folder(olFolderTopInformationStore);
		libmapipp::folder top_folder(msg_store, top_folder_id);
		// Now get the child folders of the top level folder. These are returned as
		// a std::vector of pointers to folders
		libmapipp::folder::hierarchy_container_type child_folders = top_folder.fetch_hierarchy();
		// Display the name, total item count and unread item count for each folder
        	for (unsigned int i = 0; i < child_folders.size(); ++i) {
			libmapipp::property_container child_props = child_folders[i]->get_property_container();
			child_props << PR_DISPLAY_NAME << PR_CONTENT_COUNT << PR_CONTENT_UNREAD;
			child_props.fetch();
			std::cout << "|-----> " << (const char*)child_props[PR_DISPLAY_NAME]
				  << " (" << (*(int*)child_props[PR_CONTENT_COUNT]) << " items, "
				  << (*(int*)child_props[PR_CONTENT_UNREAD]) << " unread)"
				  << std::endl;
        	}

        }
        catch (libmapipp::mapi_exception e) // Catch any MAPI exceptions
        {
                std::cout << "MAPI Exception in main: " <<  e.what()
			  << std::endl;
        }
        catch (std::runtime_error e) // Catch any other runtime exceptions
        {
                std::cout << "std::runtime_error exception in main: "
			  << e.what() << std::endl;
        }

        return 0;
}
