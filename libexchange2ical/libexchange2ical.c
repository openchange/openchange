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

#include <libexchange2ical/libexchange2ical.h>


icalcomponent * Exchange2Ical(mapi_object_t *obj_folder)
{
	struct exchange2ical_check exchange2ical_check;
	exchange2ical_check.eFlags=EntireFlag;
	
	return _Exchange2Ical(obj_folder, &exchange2ical_check);
}


icalcomponent * Exchange2IcalRange(mapi_object_t *obj_folder, struct tm *begin, struct tm *end)
{
	struct exchange2ical_check exchange2ical_check;
	exchange2ical_check.eFlags=RangeFlag;
	exchange2ical_check.begin = begin;
	exchange2ical_check.end = end;
	return _Exchange2Ical(obj_folder, &exchange2ical_check);
}


icalcomponent *Exchange2IcalEvent(mapi_object_t *obj_folder, struct GlobalObjectId *GlobalObjectId, uint32_t Sequence)
{
	struct exchange2ical_check exchange2ical_check;
	exchange2ical_check.eFlags=EventFlag;
	exchange2ical_check.GlobalObjectId=GlobalObjectId;
	exchange2ical_check.Sequence=Sequence;
	return _Exchange2Ical(obj_folder, &exchange2ical_check);
}


icalcomponent *Exchange2IcalEvents(mapi_object_t *obj_folder, struct GlobalObjectId *GlobalObjectId)
{
	struct exchange2ical_check exchange2ical_check;
	exchange2ical_check.eFlags=EventsFlag;
	exchange2ical_check.GlobalObjectId=GlobalObjectId;
	return _Exchange2Ical(obj_folder, &exchange2ical_check);
}
