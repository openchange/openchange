/*
   libmapi C++ Wrapper
   Attachment Class

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


#ifndef LIBMAPIPP__ATTACHMENT_H__
#define LIBMAPIPP__ATTACHMENT_H__

#include <iostream> //for debugging
#include <string>

#include <libmapi++/clibmapi.h>
#include <libmapi++/message.h>

namespace libmapipp
{
class object;

/**
 * \brief This class represents a message %attachment
 *
 * A message can contain both text content, and also have attached
 * (embedded) files and messages. This class represents the attachments
 * for one messaage.
 * 
 * You may not need to create the attachments yourself, since you can 
 * create a container with all the attachments using message::fetch_attachments().
 */
class attachment : public object {
	public:
		/** \brief Constructor
		 *
		 *  \param mapi_message the message that this attachment belongs to.
		 *  \param attach_num Attachment Number.
		 */
		attachment(message& mapi_message, const uint32_t attach_num) throw(mapi_exception);

		/**
		 * \brief The %attachment number
		 */
		uint32_t get_num() const { return m_attach_num; }

		/**
		 * \brief the contents of the %attachment
		 *
		 * \note the length of the array is given by get_data_size()
		 */
		const uint8_t* get_data() const  { return m_bin_data; }

		/**
		 * \brief the size of the %attachment
		 *
		 * \return the size of the %attachment in bytes
		 */
		uint32_t get_data_size() const { return m_data_size; }

		/**
		 * \brief the filename of the %attachment
		 *
		 * \note not all attachments have file names
		 *
		 * \return string containing the file name of the %attachment, if any
		 */
		std::string get_filename() const { return m_filename; }

		/**
		 * Destructor
		 */
		virtual ~attachment() throw()
		{
			if (m_bin_data) delete[] m_bin_data;
		}

	private:
		uint32_t	m_attach_num;
		uint8_t*	m_bin_data; 	// (same as unsigned char* ?)
		uint32_t	m_data_size;
		std::string	m_filename;
};

} // namespace libmapipp

#endif //!LIBMAPIPP__ATTACHMENT_H__
