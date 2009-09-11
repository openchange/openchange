/*
   libmapi C++ Wrapper

   Sample test application

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

#include <exception>
#include <vector>
#include <string>

#include <libmapi++/libmapi++.h>

using namespace libmapipp;
using std::string;

// The best way to get a folder message count is to get property PR_FOLDER_CHILD_COUNT.
// This is only used to test opening all messages.
static unsigned int get_message_count(folder& the_folder, session& mapi_session)
{
	folder::message_container_type messages = the_folder.fetch_messages();
	return messages.size();
}

static void print_folder_tree(folder& up_folder, session& mapi_session, unsigned int deep = 0)
{
	property_container up_folder_property_container = up_folder.get_property_container();
	up_folder_property_container << PR_DISPLAY_NAME << PR_CONTAINER_CLASS;
	up_folder_property_container.fetch();

	string display_name = static_cast<const char*>(up_folder_property_container[PR_DISPLAY_NAME]);
	string container_class;
	if (up_folder_property_container[PR_CONTAINER_CLASS])
		container_class = static_cast<const char*>(up_folder_property_container[PR_CONTAINER_CLASS]);

	for (unsigned int i = 0; i < deep; ++i) {
		cout << "|----> ";
	}

	cout << display_name << " (id: " << up_folder.get_id()  << ") (messages: " << get_message_count(up_folder, mapi_session) << ")" 
	     << " container class: " << container_class << endl;

	// Fetch Top Information Folder Hierarchy (child folders)
	folder::hierarchy_container_type hierarchy = up_folder.fetch_hierarchy();
	for (unsigned int i = 0; i < hierarchy.size(); ++i) {
		print_folder_tree(*hierarchy[i], mapi_session, deep+1);
	}
}

int main ()
{
	try {
		// Initialize MAPI Session
		session mapi_session;

		mapi_session.login();

		property_container store_properties = mapi_session.get_message_store().get_property_container();
		store_properties << PR_DISPLAY_NAME;
		store_properties.fetch();
		cout << "Mailbox Name: " << static_cast<const char*>(*store_properties.begin()) << endl;

		// Get Default Inbox folder ID.
		mapi_id_t inbox_id = mapi_session.get_message_store().get_default_folder(olFolderInbox);
		cout << "inbox_id: " << inbox_id << endl;

		// Open Inbox Folder
		folder inbox_folder(mapi_session.get_message_store(), inbox_id);

		// Fetch messages in Inbox Folder
		folder::message_container_type messages = inbox_folder.fetch_messages();
		cout << "Inbox size: " << messages.size() << endl;

		// Get and Print some information about the first message in the Inbox Folder
		if (messages.size() > 0) {
			// Get a property container associated to the first message
			property_container message_property_container = messages[0]->get_property_container();

			// Add Property Tags to be fetched and then fetch the properties.
			message_property_container << PR_DISPLAY_TO << PR_CONVERSATION_TOPIC;
			message_property_container.fetch_all();

			string to;
			string subject;
			// Get some property values using property_container::operator[]
			// to		= static_cast<const char*>(message_property_container[PR_DISPLAY_TO]);
			// subject	= static_cast<const char*>(message_property_container[PR_CONVERSATION_TOPIC]);
			// Print property values
			// cout << "To: " << to << endl;
			// cout << "Subject: " << subject << endl;

			for (property_container::iterator Iter = message_property_container.begin(); Iter != message_property_container.end(); Iter++)
			{
				if (Iter.get_tag() == PR_DISPLAY_TO) 
					to = (const char*) *Iter;
				else if (Iter.get_tag() == PR_CONVERSATION_TOPIC) 
					subject = (const char*) *Iter;
			}

			// Print property values
			cout << "To: " << to << endl;
			cout << "Subject: " << subject << endl;
		}

		// Get Default Top Information Store folder ID
		mapi_id_t top_folder_id = mapi_session.get_message_store().get_default_folder(olFolderTopInformationStore);

		// Open Top Information Folder
		folder top_folder(mapi_session.get_message_store(), top_folder_id);

		print_folder_tree(top_folder, mapi_session);

		cout << "finished session" << endl;
	}
	catch (mapi_exception e) // Catch any mapi exceptions
	{
		cout << "MAPI Exception @ main: " <<  e.what() << endl;
	}
	catch (std::runtime_error e) // Catch runtime exceptions
	{
		cout << "std::runtime_error exception @ main: " << e.what() << endl;

	}

	return 0;
}
