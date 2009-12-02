/*
   libmapi C++ Wrapper
   Property Tag Container Class

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

#ifndef LIBMAPIPP__PROPERTY_CONTAINER_H__
#define LIBMAPIPP__PROPERTY_CONTAINER_H__

#include <stdint.h>
#include <iostream> // for debugging only.

#include <libmapi++/clibmapi.h>
#include <libmapi++/mapi_exception.h>

// This is not declared in any of libmapi's headers, but it is defined in libmapi/property.c
extern "C" {
extern const void *get_mapi_SPropValue_data(struct mapi_SPropValue *lpProp);
}

namespace libmapipp
{
/// Iterator to use with Property Container
class property_container_iterator {
	public:
		/// Default Constructor. Creates an invalid iterator.
		property_container_iterator() : m_property_values(NULL), m_mapi_property_values(NULL)
		{}

		property_container_iterator(SPropValue* property_values) : m_property_values(property_values),
									   m_mapi_property_values(NULL)
		{}

		property_container_iterator(mapi_SPropValue* mapi_property_values) : m_property_values(NULL),
										     m_mapi_property_values(mapi_property_values)
		{}

		/** \brief Get Property Tag Type.
		 *  \return Type of property this iterator points to or MAPI_PROP_RESERVED if this is an invalid iterator.
		 *  Valid Types are: 
		 *  - PT_BOOLEAN 
		 *  - PT_I2 
		 *  - PT_LONG 
		 *  - PT_DOUBLE 
		 *  - PT_I8 
		 *  - PT_SYSTIME 
		 *  - PT_ERROR 
		 *  - PT_STRING8 
		 *  - PT_UNICODE 
		 *  - PT_BINARY
		 *  - PT_CLSID
		 *  - PT_ERROR
		 *  - PT_MV_SHORT
		 *  - PT_MV_LONG
		 *  - PT_MV_STRING8
		 *  - PT_MV_BINARY
		 *  - PT_MV_CLSID
		 *  - PT_MV_UNICODE
		 *  - PT_MV_SYSTIME
		 *  - PT_NULL
		 *  - PT_OBJECT
		 */
		int get_type()
		{
			if (m_property_values) {
				return m_property_values->ulPropTag & 0xffff;
			} else if (m_mapi_property_values) {
				return m_mapi_property_values->ulPropTag & 0xffff;
			} else {
				return MAPI_PROP_RESERVED;
			}
		}

		/**
		 * \brief Get Property Tag of the Property Value this operator points to.
		 *
		 * \return Property Tag of Property this iterator points to or MAPI_PROP_RESERVED if this is an invalid iterator.
		 */
		int get_tag()
		{
			if (m_property_values) {
				return m_property_values->ulPropTag;
			} else if (m_mapi_property_values) {
				return m_mapi_property_values->ulPropTag;
			} else {
				return MAPI_PROP_RESERVED;
			}
		}

		/// operator++
		property_container_iterator& operator++() // prefix
		{
			if (m_property_values)
				++m_property_values;
			else if (m_mapi_property_values)
				++m_mapi_property_values;

			return *this;
		}

		/// operator++ postfix
		property_container_iterator operator++(int postfix) // postfix
		{
			property_container_iterator retval = *this;
			if (m_property_values)
				 ++m_property_values;
			else if (m_mapi_property_values)
				++m_mapi_property_values;

			return retval;
		}
		
		/// operator==
		bool operator==(const property_container_iterator& rhs) const
		{
			return ( (m_property_values == rhs.m_property_values) && (m_mapi_property_values == rhs.m_mapi_property_values) );
		}

		/// operator!=
		bool operator!=(const property_container_iterator& rhs) const
		{
			return !(*this == rhs);
		}

		/**
		 * \brief operator*
		 *
		 * \return The property value as a void pointer.
		 */
		const void* operator*()
		{
			if (m_property_values)
				return get_SPropValue_data(m_property_values);
			else
				return get_mapi_SPropValue_data(m_mapi_property_values);
		}		

	private:
		SPropValue*		m_property_values;
		mapi_SPropValue*	m_mapi_property_values;
};


/// A container of properties to be used with classes derived from object.
class property_container {

	public:
		typedef property_container_iterator	iterator;
		typedef const void*			value_type;

		/// Constructor
		property_container(TALLOC_CTX* memory_ctx, mapi_object_t& mapi_object) : 
		m_memory_ctx(memory_ctx), m_mapi_object(mapi_object), m_fetched(false), m_property_tag_array(NULL), m_cn_vals(0), m_property_values(0)
		{
			m_property_value_array.cValues = 0;
			m_property_value_array.lpProps = NULL;
		}

		/**
		 * \brief Fetches properties with the tags supplied using operator<<
		 *
		 * \return The number of objects that were fetched.
		 */
		uint32_t fetch()
		{
			if (GetProps(&m_mapi_object, m_property_tag_array, &m_property_values, &m_cn_vals) != MAPI_E_SUCCESS)
				throw mapi_exception(GetLastError(), "property_container::fetch : GetProps");

			MAPIFreeBuffer(m_property_tag_array);
			m_property_tag_array = NULL;

			m_fetched = true;
			return m_cn_vals;
		}

		/// \brief Fetches \b ALL properties of the object associated with this container.
		void fetch_all()
		{
			if (GetPropsAll(&m_mapi_object, &m_property_value_array) != MAPI_E_SUCCESS)
				throw mapi_exception(GetLastError(), "property_container::fetch_all : GetPropsAll");

			// Free property_tag_array in case user used operator<< by mistake.
			if (m_property_tag_array) {
				MAPIFreeBuffer(m_property_tag_array);
				m_property_tag_array = NULL;
			}

			m_fetched = true;
		}

		/// \brief Adds a Property Tag to be fetched by fetch().
		property_container& operator<<(uint32_t property_tag)
		{
			if (!m_property_tag_array) {
				m_property_tag_array = set_SPropTagArray(m_memory_ctx, 1, property_tag);
			} else {
				if (SPropTagArray_add(m_memory_ctx, m_property_tag_array, property_tag) != MAPI_E_SUCCESS)
					throw mapi_exception(GetLastError(), "property_container::operator<< : SPropTagArray_add");
			}

			return *this;
		}

		/**
		 * \brief Finds the property value associated with a property tag
		 *
		 * \param property_tag The Property Tag to be searched for
		 *
		 * \return Property Value as a const void pointer
		 */
		const void* operator[](uint32_t property_tag)
		{
			if (!m_fetched) return NULL;

			if (m_property_values)
				return find_SPropValue_value(property_tag);
			else
				return find_mapi_SPropValue_data(&m_property_value_array, property_tag);
		}

		enum MAPITAGS get_tag_at(uint32_t pos)
		{
			if (m_property_values)
				return m_property_values[pos].ulPropTag;
			else
				return m_property_value_array.lpProps[pos].ulPropTag;
		}

		iterator begin() const
		{
			if (!m_fetched) return iterator();
		
			if (m_property_values)
				return iterator(m_property_values);
			else
				return iterator(m_property_value_array.lpProps);
		}

		iterator end() const
		{
			if (!m_fetched) return iterator();
			
			if (m_property_values)
				return iterator(m_property_values + m_cn_vals);
			else
				return iterator(m_property_value_array.lpProps + m_property_value_array.cValues);
		}

		/// \brief Get number of properties in container.
		size_t size() const
		{
			if (!m_fetched) return 0;

			if (m_property_values)
				return m_cn_vals;
			else
				return m_property_value_array.cValues;
		}

		/// Destructor
		~property_container()
		{
			if (m_property_tag_array) MAPIFreeBuffer(m_property_tag_array);
		}

	private:
		TALLOC_CTX*		m_memory_ctx;
		mapi_object_t&		m_mapi_object;

		bool			m_fetched;

		SPropTagArray*		m_property_tag_array;

		// Used when calling GetProps (fetch)
		uint32_t		m_cn_vals;
		SPropValue*		m_property_values;

		// Used when calling GetPropsAll (fetch_all)
		mapi_SPropValue_array	m_property_value_array;

		const void* find_SPropValue_value(uint32_t property_tag)
		{
			for (uint32_t i = 0; i < m_cn_vals; ++i)
			{
			  if ((uint32_t)m_property_values[i].ulPropTag == property_tag)
					return get_SPropValue_data(&m_property_values[i]);
			}

			return NULL;
		}
};

} // namespace libmapipp

#endif //!LIBMAPIPP__PROPERTY_CONTAINER_H__
