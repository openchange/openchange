/*
   libmapi C++ Wrapper
   Message class implementation

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

namespace libmapipp {

message::attachment_container_type message::fetch_attachments()
{
	mapi_object_t   attachment_table;

	mapi_object_init(&attachment_table);
	if (GetAttachmentTable(&m_object, &attachment_table) != MAPI_E_SUCCESS) {
		mapi_object_release(&attachment_table);
		throw mapi_exception(GetLastError(), "message::fetch_attachments : GetAttachmentTable");
	}

	SPropTagArray* property_tag_array = set_SPropTagArray(m_session.get_memory_ctx(), 0x1, PR_ATTACH_NUM);

	if (SetColumns(&attachment_table, property_tag_array) != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(property_tag_array);
		mapi_object_release(&attachment_table);
		throw mapi_exception(GetLastError(), "message::fetch_attachments : SetColumns");
	}

	MAPIFreeBuffer(property_tag_array);

	SRowSet  row_set;
	attachment_container_type attachment_container;

	while( (QueryRows(&attachment_table, 0x32, TBL_ADVANCE, TBL_FORWARD_READ, &row_set) == MAPI_E_SUCCESS) && row_set.cRows) {
		for (unsigned int i = 0; i < row_set.cRows; ++i) {
			try {
				attachment_container.push_back(attachment_shared_ptr(new attachment(*this, row_set.aRow[i].lpProps[0].value.l)));
			}
			catch(mapi_exception e) {
				mapi_object_release(&attachment_table);
				throw;
			}
		}
	}
	mapi_object_release(&attachment_table);

	return attachment_container;
}

} // namespace libmapipp
