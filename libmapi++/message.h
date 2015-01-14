/*
   libmapi C++ Wrapper
   Message Class

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


#ifndef LIBMAPIPP__MESSAGE_H__
#define LIBMAPIPP__MESSAGE_H__

#include <iostream> //for debugging
#include <memory>
#include <vector>

#include <libmapi++/mapi_exception.h>
#include <libmapi++/session.h>
#include <libmapi++/clibmapi.h>
#include <libmapi++/object.h>
#include <libmapi++/message_store.h>

namespace libmapipp
{
class attachment;

/** 
 * \brief This class represents a %message in Exchange.
 *
 * It is important to note that a %message is not necessarily an email %message.
 * It could be a contact, journal or anything else that is not a folder.
 */
class message : public object {
	public:
		typedef std::shared_ptr<attachment>		attachment_shared_ptr;
		typedef std::vector<attachment_shared_ptr>	attachment_container_type;

		/**
		 * \brief Constructor
		 *
		 * \param mapi_session The session to use to retrieve this %message.
		 * \param folder_id The id of the folder this %message belongs to.
		 * \param message_id The %message id.
		 */
		message(session& mapi_session, const mapi_id_t folder_id, const mapi_id_t message_id) throw(mapi_exception) 
		: object(mapi_session, "message"), m_folder_id(folder_id), m_id(message_id)
		{
			if (OpenMessage(&mapi_session.get_message_store().data(), folder_id, message_id, &m_object, 0) != MAPI_E_SUCCESS)
				throw mapi_exception(GetLastError(), "message::message : OpenMessage");
		}

		/**
		 * \brief Fetches all attachments in this %message.
		 *
		 * \return A container of attachment shared pointers.
		 */
		attachment_container_type fetch_attachments();

		/**
		 * \brief Get this %message's ID.
		 */
		mapi_id_t get_id() const { return m_id; }

		/**
		 * \brief Get this message's parent folder ID.
		 */
		mapi_id_t get_folder_id() const { return m_folder_id; } 

		/**
		 * Destructor
		 */
		virtual ~message() throw()
		{
		}

	private:
		mapi_id_t	m_folder_id;
		mapi_id_t	m_id;

};

} // namespace libmapipp

#endif //!LIBMAPIPP__MESSAGE_H__
