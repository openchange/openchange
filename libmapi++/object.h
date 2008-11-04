/*
   libmapi C++ Wrapper
   Base Object Class

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

#ifndef LIBMAPIPP__OBJECT_H__
#define LIBMAPIPP__OBJECT_H__

#include <iostream> //for debugging

#include <stdexcept>

#include <libmapi++/clibmapi.h>
#include <libmapi++/session.h>

/**
 *  The libmapi++ classes and other definitions are all enclosed in
 *  the libmapipp namespace.
 */
namespace libmapipp
{
// #define INVALID_HANDLE_VALUE 0xffffffff
class property_container;

/**
 * Base Object class
 *
 * Most classes such as folder, message and message_store derive from this class.
 * It is important that objects be passed around as references and that no unnecessary
 * copies are made as this will call the class destructor which will call
 * mapi_object_release() and release the handle associated with this object.
 */
class object {
	public:
		/**
		 * \brief Object Constructor
		 *
		 * \param mapi_session Session this object is to be associated with.
		 * \param object_type The name of the type of object (to be set in a subclass)
		 */
		object(session& mapi_session, const std::string& object_type = "") throw() : m_session(mapi_session), m_object_type(object_type)
		{
			mapi_object_init(&m_object);
		}

		/**
		 * \brief Obtain a reference to the mapi_object_t associated with this object
		 * 
		 * \return A reference to the C struct mapi_object_t associated with this object
		 */
		virtual mapi_object_t& data() throw() { return m_object; }

		/**
		 * \brief Obtain a property_container to be used with this object.
		 *
		 * \return A property_container to be used with this object.
		 */
		virtual property_container get_property_container();

		/**
		 * \brief Obtain the session associated with this object.
		 *
		 * \return The session associated with this object
		 */
		virtual session& get_session() { return m_session; }

		/**
		 * \brief Destructor
		 *
		 * Calls mapi_object_release() which releases the handle associated with this object.
		 */ 
		virtual ~object() throw()
		{
			// TODO: Check for invalid handle in libmapi 0.7
			// if (m_object.handle != INVALID_HANDLE_VALUE)
			mapi_object_release(&m_object);

//			std::cout << "destroying object " << m_object_type << std::endl;
		}

	protected:
		mapi_object_t	m_object;
		session&	m_session;

	private:
		std::string m_object_type; // for debugging purposes
};

} // namespace libmapipp

#include <libmapi++/impl/object.ipp>

#endif //!LIBMAPIPP__OBJECT_H__
