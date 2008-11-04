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
#include <vector>
#include <boost/shared_ptr.hpp>

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
		typedef boost::shared_ptr<message>		message_shared_ptr;

		typedef std::vector<message_shared_ptr >	message_container_type;

		/**
		 * Pointer to a %folder
		*/
		typedef boost::shared_ptr<folder>		folder_shared_ptr;

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
		message_container_type fetch_messages() throw(mapi_exception)
		{
			uint32_t 	contents_table_row_count = 0;
			mapi_object_t 	contents_table;

			mapi_object_init(&contents_table);
			if (GetContentsTable(&m_object, &contents_table, 0, &contents_table_row_count) != MAPI_E_SUCCESS) {
				mapi_object_release(&contents_table);
				throw mapi_exception(GetLastError(), "folder::fetch_messages : GetContentsTable");
			}

			SPropTagArray* property_tag_array = set_SPropTagArray(m_session.get_memory_ctx(), 0x2, PR_FID,
													       PR_MID);

			if (SetColumns(&contents_table, property_tag_array) != MAPI_E_SUCCESS) {
				MAPIFreeBuffer(property_tag_array);
				mapi_object_release(&contents_table);
				throw mapi_exception(GetLastError(), "folder::fetch_messages : SetColumns");
			}

			MAPIFreeBuffer(property_tag_array);

			uint32_t rows_to_read = contents_table_row_count;
			SRowSet  row_set;

			message_container_type message_container;
			message_container.reserve(contents_table_row_count);

			while( (QueryRows(&contents_table, rows_to_read, TBL_ADVANCE, &row_set) == MAPI_E_SUCCESS) && row_set.cRows) {
				rows_to_read -= row_set.cRows;
				for (unsigned int i = 0; i < row_set.cRows; ++i) {
					try {
						message_container.push_back(message_shared_ptr(new message(m_session,
													   m_id,
												           row_set.aRow[i].lpProps[1].value.d)));
					}
					catch(mapi_exception e) {
						mapi_object_release(&contents_table);
						throw;
					}
				}
			}

			mapi_object_release(&contents_table);

			return message_container;
		}

		/**
		 * \brief Fetch all subfolders within this %folder
		 *
		 * \return A container of %folder shared pointers.
		 */
		hierarchy_container_type fetch_hierarchy() throw(mapi_exception)
		{
			mapi_object_t 	hierarchy_table;
			uint32_t	hierarchy_table_row_count = 0;

			mapi_object_init(&hierarchy_table);
			if (GetHierarchyTable(&m_object, &hierarchy_table, 0, &hierarchy_table_row_count) != MAPI_E_SUCCESS) {
				mapi_object_release(&hierarchy_table);
				throw mapi_exception(GetLastError(), "folder::fetch_hierarchy : GetHierarchyTable");
			}

			SPropTagArray* property_tag_array = set_SPropTagArray(m_session.get_memory_ctx(), 0x1, PR_FID);

			if (SetColumns(&hierarchy_table, property_tag_array)) {
				MAPIFreeBuffer(property_tag_array);
				mapi_object_release(&hierarchy_table);
				throw mapi_exception(GetLastError(), "folder::fetch_hierarchy : SetColumns");
			}

			MAPIFreeBuffer(property_tag_array);

			uint32_t rows_to_read = hierarchy_table_row_count;
			SRowSet  row_set;
			
			hierarchy_container_type hierarchy_container;
			hierarchy_container.reserve(hierarchy_table_row_count);

			while( (QueryRows(&hierarchy_table, rows_to_read, TBL_ADVANCE, &row_set) == MAPI_E_SUCCESS) && row_set.cRows) {
				rows_to_read -= row_set.cRows;
				for (unsigned int i = 0; i < row_set.cRows; ++i) {
					try {
						hierarchy_container.push_back(folder_shared_ptr(new folder(*this, 
												           row_set.aRow[i].lpProps[0].value.d)));
					}
					catch(mapi_exception e) {
						mapi_object_release(&hierarchy_table);
						throw;
					}
				}
			}

                        mapi_object_release(&hierarchy_table);

                        return hierarchy_container;
		}

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
