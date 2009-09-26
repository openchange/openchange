/*
   libmapi C++ Wrapper

   Sample folder message application

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

		// We start off by fetching the inbox
		mapi_id_t inbox_id = msg_store.get_default_folder(olFolderInbox);
		libmapipp::folder inbox_folder(msg_store, inbox_id);
		// Now get the messages in this folder These are returned as
		// a std::vector of pointers to messages
		libmapipp::folder::message_container_type messages = inbox_folder.fetch_messages();
		std::cout << "Inbox contains " << messages.size() << " messages" << std::endl;

		// Work through each message
        	for (unsigned int i = 0; i < messages.size(); ++i) {
			// Get the properties we are interested in
			libmapipp::property_container msg_props = messages[i]->get_property_container();
			// We get the "to" addressee, and the subject
			// You can get a lot of other properties here (e.g. sender, body, etc).
			msg_props << PR_DISPLAY_TO << PR_CONVERSATION_TOPIC;
			msg_props.fetch();
			// Display those properties
			if (msg_props[PR_DISPLAY_TO] != 0) {
				std::cout << "|-----> " << (const char*)msg_props[PR_DISPLAY_TO];
				if(msg_props[PR_CONVERSATION_TOPIC] != 0) {
					std::cout << "\t\t| " << (const char*)msg_props[PR_CONVERSATION_TOPIC];
				}
				std::cout << std::endl;
			}
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
