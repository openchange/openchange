/*
   libmapi C++ Wrapper
   Folder Class

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

#ifndef LIBMAPIPP__FOLDER_H__
#define LIBMAPIPP__FOLDER_H__

#include <iostream> //for debugging
#include <memory>
#include <vector>

#include <libmapi++/clibmapi.h>
#include <libmapi++/mapi_exception.h>
#include <libmapi++/object.h>
#include <libmapi++/message.h>

namespace libmapipp
{

/**
 *  This class represents a %folder or container within Exchange
 */
class folder : public object {
	public:
		/**
		 * Pointer to a message
		*/
		typedef std::shared_ptr<message>		message_shared_ptr;

		typedef std::vector<message_shared_ptr >	message_container_type;

		/**
		 * Pointer to a %folder
		*/
		typedef std::shared_ptr<folder>		folder_shared_ptr;

		/**
		 * Hierarchy folders
		 *
		 * This is a vector (list) of child folders for a given %folder
		*/
		typedef std::vector<folder_shared_ptr>		hierarchy_container_type;

		/** 
		 * \brief Constructor
		 *
		 * \param parent_folder The parent of this %folder.
		 * \param folder_id     This folder's id.
		*/
		folder(object& parent_folder, const mapi_id_t folder_id) throw(mapi_exception) 
		: object(parent_folder.get_session(), "folder"), m_id(folder_id)
		{
			if (OpenFolder(&parent_folder.data(), folder_id, &m_object) != MAPI_E_SUCCESS)
				throw mapi_exception(GetLastError(), "folder::folder : OpenFolder");
		}

		/**
		 * \brief Obtain %folder id
		 *
		 * \return This folder's id.
		 */
		mapi_id_t get_id() const { return m_id; }

		/** 
		 * \brief Delete a message that belongs to this %folder
		 *
		 * \param message_id The id of the message to delete.
		 */
		void delete_message(mapi_id_t message_id) throw (mapi_exception)
		{
			if (DeleteMessage(&m_object, &message_id, 1) != MAPI_E_SUCCESS)
				throw mapi_exception(GetLastError(), "folder::delete_message : DeleteMessage");
		}

		/**
		 * \brief Fetch all messages in this %folder
		 *
		 * \return A container of message shared pointers.
		 */
		message_container_type fetch_messages() throw(mapi_exception);

		/**
		 * \brief Fetch all subfolders within this %folder
		 *
		 * \return A container of %folder shared pointers.
		 */
		hierarchy_container_type fetch_hierarchy() throw(mapi_exception);

		/**
		 * Destructor
		 */
		virtual ~folder() throw()
		{
		}

	private:
		mapi_id_t	m_id;
};

} // namespace libmapipp

#endif //!LIBMAPIPP__FOLDER_H__
