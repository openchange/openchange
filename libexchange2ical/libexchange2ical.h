/*
   Convert Exchange appointments to ICAL

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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

#include <libexchange2ical/exchange2ical.h>
#ifndef	__LIBEXCHANGE2ICAL_H_
#define	__LIBEXCHANGE2ICAL_H_

/**
   \details Retrieve entire exchange calendar as an Icalendar

   This function returns an Icalendar of the entire exchange calendar inside the obj_folder

   \param obj_folder the folder to operate in
  
   \return Icalendar on success, otherwise Null

   \note Developers should call ical_component_free() on the returned icalendar after use.

 */
icalcomponent * Exchange2Ical(mapi_object_t *obj_folder);

/**
   \details Retrieve a range of exchange appointments as an Icalendar

   This function returns an Icalendar of exchange appointments that begin within the specified range.

   \param obj_folder the folder to operate in
   \param begin	a tm that specifies the start date of the range
   \param end a tm that specifies the end date of the range

   \return Icalendar on success, otherwise Null

   \note Developers should call ical_component_free() on the returned icalendar after use.
   If no events are within the specified range, an icalendar will be returned without any vevents.

 */
icalcomponent * Exchange2IcalRange(mapi_object_t *obj_folder, struct tm *begin, struct tm *end);


/**
   \details Retrieve a specific exchange appointment as an Icalendar

   This function returns an Icalendar with an appointments that match
   the specified GlobalObjectId and sequence number.

   \param obj_folder the folder to operate in
   \param GlobalObjectId the unique GlobalObjectId of the appointment
   \param Sequence the sequence number of the appointment

   \return Icalendar on success, otherwise Null

   \note Developers should call ical_component_free() on the returned icalendar after use.
   If no event's GlobalObjectId match the specifid GlobalObjectId, an icalendar will be returned without any vevents.

 */
icalcomponent *Exchange2IcalEvent(mapi_object_t *obj_folder, struct GlobalObjectId *GlobalObjectId, uint32_t Sequence);


/**
   \details Retrieve an exchange appointment and its occurrences as an Icalendar

   This function returns an Icalendar with any appointments that match
   the specified GlobalObjectId and sequence number.
   
   \param obj_folder the folder to operate in
   \param GlobalObjectId the unique GlobalObjectId of the appointment

   \return Icalendar on success, otherwise Null

   \note Developers should call ical_component_free() on the returned icalendar after use.
   If no event's GlobalObjectId match the specifid GlobalObjectId, an icalendar will be returned without any vevents.

 */
icalcomponent *Exchange2IcalEvents(mapi_object_t *obj_folder, struct GlobalObjectId *GlobalObjectId);

#endif /* __LIBEXCHANGE2ICAL_H_ */
