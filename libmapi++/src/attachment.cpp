/*
   libmapi C++ Wrapper
   Attachment Class implementation.

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

#include <libmapi++/attachment.h>
#include <libmapi++/property_container.h>

namespace libmapipp {

attachment::attachment(message& mapi_message, const uint32_t attach_num) throw(mapi_exception)
: object(mapi_message.get_session(), "attachment"), m_attach_num(attach_num), m_bin_data(NULL), m_data_size(0), m_filename("")
{
	if (OpenAttach(&mapi_message.data(), attach_num, &m_object) != MAPI_E_SUCCESS)
		throw mapi_exception(GetLastError(), "attachment::attachment : OpenAttach");

	property_container properties = get_property_container();
	properties << PR_ATTACH_FILENAME << PR_ATTACH_LONG_FILENAME << PR_ATTACH_SIZE << PR_ATTACH_DATA_BIN << PR_ATTACH_METHOD;
	properties.fetch();

	const char* filename = static_cast<const char*>(properties[PR_ATTACH_LONG_FILENAME]);
	if (!filename) {
		filename = static_cast<const char*>(properties[PR_ATTACH_FILENAME]);
	}

	if (filename)
		m_filename = filename;

	m_data_size = *(static_cast<const uint32_t*>(properties[PR_ATTACH_SIZE]));

	const Binary_r* attachment_data = static_cast<const Binary_r*>(properties[PR_ATTACH_DATA_BIN]);

	// Don't load PR_ATTACH_DATA_BIN if it's embedded in message.
	// NOTE: Use RopOpenEmbeddedMessage when it is implemented.
	const uint32_t attach_method = *static_cast<const uint32_t*>(properties[PR_ATTACH_METHOD]);
	if (attach_method != ATTACH_BY_VALUE)
		return;

	// Get Binary Data.
	if (attachment_data) {
		m_data_size = attachment_data->cb;
		m_bin_data = new uint8_t[m_data_size];
		memcpy(m_bin_data, attachment_data->lpb, attachment_data->cb);
	} else {
		mapi_object_t obj_stream;
		mapi_object_init(&obj_stream);
		if (OpenStream(&m_object, (enum MAPITAGS)PidTagAttachDataBinary, OpenStream_ReadOnly, &obj_stream) != MAPI_E_SUCCESS)
			throw mapi_exception(GetLastError(), "attachment::attachment : OpenStream");

		if (GetStreamSize(&obj_stream, &m_data_size) != MAPI_E_SUCCESS)
			throw mapi_exception(GetLastError(), "attachment::attachment : GetStreamSize");

		m_bin_data = new uint8_t[m_data_size];

		uint32_t pos = 0;
		uint16_t bytes_read = 0;
		do {
			if (ReadStream(&obj_stream, m_bin_data+pos, 1024, &bytes_read) != MAPI_E_SUCCESS)
				throw mapi_exception(GetLastError(), "attachment::attachment : ReadStream");

			pos += bytes_read;

		} while (bytes_read && pos < m_data_size);

		mapi_object_release(&obj_stream);
	}
}


} // namespace libmapipp

