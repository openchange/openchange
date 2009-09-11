/*
   libmapi C++ Wrapper

   Sample attachment test application

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

#include <iostream>

#include <libmapi++/libmapi++.h>

using namespace std;
using namespace libmapipp;

static uint32_t get_attachment_count(message& mapi_message)
{
	message::attachment_container_type attachment_container = mapi_message.fetch_attachments();
	return attachment_container.size();
}

static void print_messages_with_attachments(folder& up_folder)
{
	folder::message_container_type messages = up_folder.fetch_messages();
	for (folder::message_container_type::iterator Iter = messages.begin(); Iter != messages.end(); ++Iter) {
		uint32_t attachment_count = get_attachment_count(*(*Iter));
//		cout << "Attachment Count: " << attachment_count << endl;
		if (attachment_count) {
			cout << "Message (" << (*Iter)->get_id() << ") has " << attachment_count << " attachments." << endl; 
		}
	}
}

static void print_folder_tree(folder& up_folder, session& mapi_session, unsigned int deep = 0)
{
	property_container up_folder_property_container = up_folder.get_property_container();
	up_folder_property_container << PR_DISPLAY_NAME << PR_CONTENT_COUNT;
	up_folder_property_container.fetch();

	string display_name = static_cast<const char*>(up_folder_property_container[PR_DISPLAY_NAME]);
	const uint32_t message_count = *(static_cast<const uint32_t*>(up_folder_property_container[PR_CONTENT_COUNT]));

	for (unsigned int i = 0; i < deep; ++i) {
		cout << "|----> ";
	}

	cout << display_name << " (id: " << up_folder.get_id()  << ") (messages: " << message_count << ")" << endl;
	
	print_messages_with_attachments(up_folder);

	// Fetch Top Information Folder Hierarchy (child folders)
	folder::hierarchy_container_type hierarchy = up_folder.fetch_hierarchy();
	for (unsigned int i = 0; i < hierarchy.size(); ++i) {
		print_folder_tree(*hierarchy[i], mapi_session, deep+1);
	}
}


int main()
{
	try {
		session mapi_session;

		mapi_session.login();

		// Get Default Top Information Store folder ID
		mapi_id_t top_folder_id = mapi_session.get_message_store().get_default_folder(olFolderTopInformationStore);

		// Open Top Information Folder
		folder top_folder(mapi_session.get_message_store(), top_folder_id);

		print_folder_tree(top_folder, mapi_session);
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
