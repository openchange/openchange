/*
   libmapi C++ Wrapper
   Message Store Class

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


#ifndef LIBMAPIPP__MESSAGE_STORE_H__
#define LIBMAPIPP__MESSAGE_STORE_H__

#include <stdint.h>

#include <libmapi++/clibmapi.h>
#include <libmapi++/mapi_exception.h>
#include <libmapi++/object.h>

namespace libmapipp
{
class session;

/**
 * \brief This class represents the Message Store in Exchange.
 *
 * The message_store is the grouping of message folders (which could be the 
 * users private store including mail, calendar, todo list, journal, contacts
 * and so on) or could be the public store (e.g. shared public folders).
 *
 * It is not possible for you, the user, to create a message_store object.
 * Instead, you should retrieve the message_store associated with a session
 * using session::get_message_store()
 */
class message_store : public object {
	public:
		/**
		 * \brief Retrieves the folder id for the specified default folder in the Message Store.
		 *
		 * \param id The type of folder to search for.
		 *
		 * The following types of folders are supported:
		 * - olFolderTopInformationStore
		 * - olFolderDeletedItems
		 * - olFolderOutbox
		 * - olFolderSentMail
		 * - olFolderInbox
		 * - olFolderCalendar
		 * - olFolderContacts
		 * - olFolderJournal
		 * - olFolderNotes
		 * - olFolderTasks
		 * - olFolderDrafts
		 *
		 * If you are trying to enumerate all folders, you should open the 
		 * olFolderTopInformationStore, and then get the hierarchy container for
		 * that top level folder.
		 *
		 * \return The resulting folder id.
		 */
		mapi_id_t get_default_folder(const uint32_t id) const throw(mapi_exception)
		{
			mapi_id_t folder;

			if (GetDefaultFolder(const_cast<mapi_object_t*>(&m_object), &folder, id) != MAPI_E_SUCCESS)
				throw mapi_exception(GetLastError(), "message_store::get_default_folder() : GetDefaultFolder");

			return folder;
		}


	private:
		friend class session;
		message_store(session& mapi_session) throw() : object(mapi_session, "message_store")
		{}

		void open(mapi_session *mapi_session) throw(mapi_exception)
		{
			if (OpenMsgStore(mapi_session, &m_object) != MAPI_E_SUCCESS)
				throw mapi_exception(GetLastError(), "message_store::open() : OpenMsgStore");
		}

		~message_store() throw()
		{
		}
};

} // namespace libmapipp

#endif //!LIBMAPIPP__MESSAGE_STORE_H__
