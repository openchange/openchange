/*
   Common conversion routines for exchange2ical

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

static void ical2exchange_get_properties(struct ical2exchange *ical2exchange, icalcomponent *vevent){
	icalproperty *icalProp = NULL;
	icalproperty *prop = NULL;
	const char *xname;
	enum icalproperty_kind propType;

	icalProp=icalcomponent_get_first_property(vevent, ICAL_ANY_PROPERTY);
	
	while(icalProp!=0){
		propType=icalproperty_isa(icalProp);
		switch (propType){
			case ICAL_ATTACH_PROPERTY:
				if(!ical2exchange->attachEvent){
					ical2exchange->attachEvent=icalcomponent_new_vevent();
				}
				prop = icalproperty_new_clone(icalProp);
				icalcomponent_add_property(ical2exchange->attachEvent, prop);
				break;
			case ICAL_ATTENDEE_PROPERTY:
				if(!ical2exchange->attendeeEvent){
					ical2exchange->attendeeEvent=icalcomponent_new_vevent();
				}
				prop = icalproperty_new_clone(icalProp);
				icalcomponent_add_property(ical2exchange->attendeeEvent, prop);
				break;
			case ICAL_CATEGORIES_PROPERTY:
				if(!ical2exchange->categoriesEvent){
					ical2exchange->categoriesEvent=icalcomponent_new_vevent();
				}
				prop = icalproperty_new_clone(icalProp);
				icalcomponent_add_property(ical2exchange->categoriesEvent, prop);
				break;
			case ICAL_CLASS_PROPERTY:
				ical2exchange->classProp=icalProp;
				break;
			case ICAL_COMMENT_PROPERTY:
				ical2exchange->commentProp=icalProp;
				break;
			case ICAL_CONTACT_PROPERTY:
				if(!ical2exchange->contactEvent){
					ical2exchange->contactEvent=icalcomponent_new_vevent();
				}
				prop = icalproperty_new_clone(icalProp);
				icalcomponent_add_property(ical2exchange->contactEvent, prop);
				break;
			case ICAL_CREATED_PROPERTY:
				break;
			case ICAL_DESCRIPTION_PROPERTY:
				ical2exchange->descriptionProp=icalProp;
				break;
			case ICAL_DTEND_PROPERTY:
				ical2exchange->dtendProp=icalProp;
				break;
			case ICAL_DTSTAMP_PROPERTY:
				ical2exchange->dtstampProp=icalProp;
				break;
			case ICAL_DTSTART_PROPERTY:
				ical2exchange->dtstartProp=icalProp;
				break;
			case ICAL_DURATION_PROPERTY:
				ical2exchange->durationProp=icalProp;
				break;
			case ICAL_EXDATE_PROPERTY:
				if(!ical2exchange->exdateEvent){
					ical2exchange->exdateEvent=icalcomponent_new_vevent();
				}
				prop = icalproperty_new_clone(icalProp);
				ical2exchange->exdateCount ++;
				icalcomponent_add_property(ical2exchange->exdateEvent, prop);
				break;
			case ICAL_LASTMODIFIED_PROPERTY:
				break;
			case ICAL_LOCATION_PROPERTY:
				ical2exchange->locationProp=icalProp;
				break;
			case ICAL_ORGANIZER_PROPERTY:
				//TODO: figure out how to MS-OXOABK
				ical2exchange->organizerProp=icalProp;
				break;
			case ICAL_PRIORITY_PROPERTY:
				ical2exchange->priorityProp=icalProp;
				break;
			case ICAL_RDATE_PROPERTY:
				if(!ical2exchange->rdateEvent){
					ical2exchange->rdateEvent=icalcomponent_new_vevent();
				}
				icalproperty *prop = icalproperty_new_clone(icalProp);
				ical2exchange->rdateCount ++;
				icalcomponent_add_property(ical2exchange->rdateEvent, prop);
				break;
			case ICAL_RECURRENCEID_PROPERTY:
				ical2exchange->recurrenceidProp=icalProp;
				break;
			case ICAL_RESOURCES_PROPERTY:
				if(!ical2exchange->resourcesEvent){
					ical2exchange->resourcesEvent=icalcomponent_new_vevent();
				}
				prop = icalproperty_new_clone(icalProp);
				icalcomponent_add_property(ical2exchange->resourcesEvent, prop);
				break;
			case ICAL_RRULE_PROPERTY:
				ical2exchange->rruleProp=icalProp;
				break;
			case ICAL_SEQUENCE_PROPERTY:
				ical2exchange->sequenceProp=icalProp;
				break;
			case ICAL_STATUS_PROPERTY:
				ical2exchange->statusProp=icalProp;
				break;
			case ICAL_SUMMARY_PROPERTY:
				ical2exchange->summaryProp=icalProp;
				break;
			case ICAL_TRANSP_PROPERTY:
				ical2exchange->transpProp=icalProp;
				break;
			case ICAL_UID_PROPERTY:
				//TODO
				ical2exchange->uidProp=icalProp;
				break;
		 	case ICAL_X_PROPERTY: 
				xname=icalproperty_get_x_name(icalProp);
				if(!strcmp(xname,"X-MICROSOFT-CDO-BUSYSTATUS")){
					ical2exchange->x_busystatusProp=icalProp;
				} else if(!strcmp(xname,"X-MICROSOFT-MSNCALENDAR-BUSYSTATUS")){
					ical2exchange->x_busystatusProp=icalProp;
				} else if(!strcmp(xname,"X-MICROSOFT-CDO-APPT-SEQUENCE")){
					ical2exchange->x_sequenceProp=icalProp;
				} else if(!strcmp(xname,"X-MICROSOFT-CDO-IMPORTANCE")){
					ical2exchange->x_importanceProp=icalProp;
				} else if(!strcmp(xname,"X-MICROSOFT-MSNCALENDAR-IMPORTANCE")){
					ical2exchange->x_importanceProp=icalProp;
				} else if(!strcmp(xname,"X-MICROSOFT-CDO-INTENDEDSTATUS")){
					ical2exchange->x_intendedProp=icalProp;
				} else if(!strcmp(xname,"X-MICROSOFT-MSNCALENDAR-INTENDEDSTATUS")){
					ical2exchange->x_intendedProp=icalProp;
				} else if(!strcmp(xname,"X-MICROSOFT-CDO-OWNERAPPTID")){
					ical2exchange->x_ownerapptidProp=icalProp;
				} else if(!strcmp(xname,"X-MICROSOFT-CDO-ATTENDEE-CRITICAL-CHANGE")){
					ical2exchange->x_attendeecriticalchangeProp=icalProp;
				} else if(!strcmp(xname,"X-MICROSOFT-CDO-OWNER-CRITICAL-CHANGE")){
					ical2exchange->x_ownercriticalchangeProp=icalProp;
				} else if(!strcmp(xname,"X-MICROSOFT-CDO-REPLYTIME")){
					ical2exchange->x_replytimeProp=icalProp;
				} else if(!strcmp(xname,"X-MICROSOFT-DISALLOW-COUNTER")){
					ical2exchange->x_disallowcounterProp=icalProp;
				} else if(!strcmp(xname,"X-MICROSOFT-ISDRAFT")){
					ical2exchange->x_isdraftProp=icalProp;
				} else if(!strcmp(xname,"X-MS-OLK-ALLOWEXTERNCHECK")){
					ical2exchange->x_allowexterncheckProp=icalProp;
				} else if(!strcmp(xname,"X-MS-OLK-APPTLASTSEQUENCE")){
					ical2exchange->x_apptlastsequenceProp=icalProp;
				} else if(!strcmp(xname,"X-MS-OLK-APPTSEQTIME")){
					ical2exchange->x_apptseqtimeProp=icalProp;
				} else if(!strcmp(xname,"X-MS-OLK-AUTOFILLLOCATION")){
					ical2exchange->x_autofilllocationProp=icalProp;
				} else if(!strcmp(xname,"X-MS-OLK-AUTOSTARTCHECK")){
					ical2exchange->x_autostartcheckProp=icalProp;
				} else if(!strcmp(xname,"X-MS-OLK-CONFCHECK")){
					ical2exchange->x_confcheckProp=icalProp;
				} else if(!strcmp(xname,"X-MS-OLK-COLLABORATEDDOC")){
					ical2exchange->x_collaborateddocProp=icalProp;
				} else if(!strcmp(xname,"X-MS-OLK-CONFTYPE")){
					ical2exchange->x_conftypeProp=icalProp;
				} else if(!strcmp(xname,"X-MS-OLK-MWSURL")){
					ical2exchange->x_mwsurlProp=icalProp;
				} else if(!strcmp(xname,"X-MS-OLK-NETSHOWURL")){
					ical2exchange->x_netshowurlProp=icalProp;
				} else if(!strcmp(xname,"X-MS-OLK-ONLINEPASSWORD")){
					ical2exchange->x_onlinepasswordProp=icalProp;
				} else if(!strcmp(xname,"X-MS-OLK-ORGALIAS")){
					ical2exchange->x_orgaliasProp=icalProp;
				} else if(!strcmp(xname,"X-MS-OLK-ORIGINALEND")){
					ical2exchange->x_originalendProp=icalProp;
				} else if(!strcmp(xname,"X-MS-OLK-ORIGINALSTART")){
					ical2exchange->x_originalstartProp=icalProp;
				}
				
				break;
			default:
				break;
		}
		icalProp=icalcomponent_get_next_property(vevent, ICAL_ANY_PROPERTY);
	}
	
	ical2exchange->valarmEvent = icalcomponent_get_first_component(vevent,ICAL_VALARM_COMPONENT);
	
}

static void ical2exchange_convert_event(struct ical2exchange *ical2exchange)
{
	ical2exchange_property_ATTACH(ical2exchange);
// 	//TODO ATTENDEE, ORGANIZER, x_ms_ok_sender
	ical2exchange_property_CATEGORIES(ical2exchange);
	ical2exchange_property_CLASS(ical2exchange);
	ical2exchange_property_COMMENT(ical2exchange);
	ical2exchange_property_CONTACT(ical2exchange);
	ical2exchange_property_DESCRIPTION(ical2exchange);
	ical2exchange_property_DTSTAMP(ical2exchange);
	ical2exchange_property_DTSTART_DTEND(ical2exchange);
	ical2exchange_property_LOCATION(ical2exchange);
	ical2exchange_property_PRIORITY(ical2exchange);
	ical2exchange_property_RRULE_EXDATE_RDATE(ical2exchange);
	ical2exchange_property_SEQUENCE(ical2exchange);
	ical2exchange_property_STATUS(ical2exchange);
	ical2exchange_property_SUMMARY(ical2exchange);
	ical2exchange_property_VALARM(ical2exchange);
	ical2exchange_property_X_ALLOWEXTERNCHECK(ical2exchange);
	ical2exchange_property_X_APPTSEQTIME(ical2exchange);
	ical2exchange_property_X_APPTLASTSEQUENCE(ical2exchange);
	ical2exchange_property_X_ATTENDEE_CRITICAL_CHANGE(ical2exchange);
	ical2exchange_property_X_AUTOFILLLOCATION(ical2exchange);
	ical2exchange_property_X_AUTOSTARTCHECK(ical2exchange);
	ical2exchange_property_X_COLLABORATEDDOC(ical2exchange);
	ical2exchange_property_X_CONFCHECK(ical2exchange);
	ical2exchange_property_X_CONFTYPE(ical2exchange);
	ical2exchange_property_X_DISALLOW_COUNTER(ical2exchange);
	ical2exchange_property_X_INTENDEDSTATUS(ical2exchange);
	ical2exchange_property_X_ISDRAFT(ical2exchange);
	ical2exchange_property_X_MWSURL(ical2exchange);
	ical2exchange_property_X_NETSHOWURL(ical2exchange);
	ical2exchange_property_X_ONLINEPASSWORD(ical2exchange);
	ical2exchange_property_X_ORGALIAS(ical2exchange);
	ical2exchange_property_X_ORIGINALEND_ORIGINALSTART(ical2exchange);
	ical2exchange_property_X_OWNER_CRITICAL_CHANGE(ical2exchange);
	ical2exchange_property_X_OWNERAPPTID(ical2exchange);
	ical2exchange_property_X_REPLYTIME(ical2exchange);
}

static void ical2exchange_init(struct ical2exchange *ical2exchange, TALLOC_CTX *mem_ctx)
{
	ical2exchange->mem_ctx = mem_ctx;
	ical2exchange->classProp = NULL;
	ical2exchange->commentProp = NULL;
	ical2exchange->descriptionProp = NULL;
	ical2exchange->dtendProp = NULL;
	ical2exchange->dtstampProp = NULL;
	ical2exchange->dtstartProp = NULL;
	ical2exchange->durationProp = NULL;
	ical2exchange->locationProp = NULL;
	ical2exchange->organizerProp = NULL;
	ical2exchange->priorityProp = NULL;
	ical2exchange->recurrenceidProp = NULL;
	ical2exchange->rruleProp = NULL;
	ical2exchange->sequenceProp = NULL;
	ical2exchange->statusProp = NULL;
	ical2exchange->summaryProp = NULL;
	ical2exchange->transpProp = NULL;
	ical2exchange->uidProp = NULL;
	ical2exchange->attachEvent = NULL;
	ical2exchange->attendeeEvent = NULL;
	ical2exchange->categoriesEvent = NULL;
	ical2exchange->contactEvent = NULL;
	ical2exchange->exdateEvent = NULL;
	ical2exchange->rdateEvent = NULL;
	ical2exchange->resourcesEvent = NULL;
	ical2exchange->valarmEvent = NULL;
	ical2exchange->x_busystatusProp = NULL;
	ical2exchange->x_sequenceProp = NULL;
	ical2exchange->x_importanceProp = NULL;
	ical2exchange->x_intendedProp= NULL;
	ical2exchange->x_ownerapptidProp = NULL;
	ical2exchange->x_attendeecriticalchangeProp = NULL;
	ical2exchange->x_ownercriticalchangeProp = NULL;
	ical2exchange->x_replytimeProp = NULL;
	ical2exchange->x_disallowcounterProp = NULL;
	ical2exchange->x_isdraftProp = NULL;
	ical2exchange->x_allowexterncheckProp = NULL;
	ical2exchange->x_apptlastsequenceProp = NULL;
	ical2exchange->x_apptseqtimeProp = NULL;
	ical2exchange->x_autofilllocationProp = NULL;
	ical2exchange->x_autostartcheckProp = NULL;
	ical2exchange->x_confcheckProp = NULL;
	ical2exchange->x_collaborateddocProp = NULL;
	ical2exchange->x_conftypeProp = NULL;
	ical2exchange->x_mwsurlProp = NULL;
	ical2exchange->x_netshowurlProp = NULL;
	ical2exchange->x_onlinepasswordProp = NULL;
	ical2exchange->x_originalstartProp = NULL;
	ical2exchange->x_originalendProp = NULL;
	ical2exchange->x_orgaliasProp = NULL;
	
	ical2exchange->lpProps = NULL;
	ical2exchange->rdateCount = 0 ;
	ical2exchange->exdateCount = 0;
	ical2exchange->cValues = 0;
	
}

static void ical2exchange_reset(struct ical2exchange *ical2exchange)
{
	if(ical2exchange->attachEvent){
		icalcomponent_free(ical2exchange->attachEvent);
	}
	if(ical2exchange->attendeeEvent){
		icalcomponent_free(ical2exchange->attendeeEvent);
	}

	if(ical2exchange->contactEvent){
		icalcomponent_free(ical2exchange->contactEvent);
	}
	if(ical2exchange->exdateEvent){
		icalcomponent_free(ical2exchange->exdateEvent);
	}
	if(ical2exchange->rdateEvent){
		icalcomponent_free(ical2exchange->rdateEvent);
	}
	if(ical2exchange->resourcesEvent){
		icalcomponent_free(ical2exchange->resourcesEvent);
	}
	if(ical2exchange->categoriesEvent){
		icalcomponent_free(ical2exchange->categoriesEvent);
	}
	ical2exchange_init(ical2exchange, NULL);
}

void _IcalEvent2Exchange(mapi_object_t *obj_folder, icalcomponent *vevent)
{
	struct ical2exchange ical2exchange;
	TALLOC_CTX	*mem_ctx;
	enum MAPISTATUS			retval;
	mapi_object_t		obj_message;
	ical2exchange.obj_message = &obj_message;

	ical2exchange.method=ICAL_METHOD_PUBLISH;
	mapi_object_init(&obj_message);
	
	/*sanity check*/
	if(icalcomponent_isa(vevent) != ICAL_VEVENT_COMPONENT) return;
	
	mem_ctx = talloc_named(NULL, 0, "ical2exchange");
	ical2exchange.lpProps = talloc_array(mem_ctx, struct SPropValue, 2);

	ical2exchange_init(&ical2exchange, mem_ctx);
	ical2exchange_get_properties(&ical2exchange, vevent);
	ical2exchange_convert_event(&ical2exchange);
	
	retval = CreateMessage(obj_folder, &obj_message);
	
	if (retval != MAPI_E_SUCCESS){
		mapi_errstr("CreateMessage", GetLastError());
	} else {
		retval = SetProps(&obj_message, ical2exchange.lpProps, ical2exchange.cValues);
		if (retval != MAPI_E_SUCCESS){
			mapi_errstr("SetProps", GetLastError());
		} else {
			retval = SaveChangesMessage(obj_folder, &obj_message, KeepOpenReadOnly);
			if (retval != MAPI_E_SUCCESS){
				mapi_errstr("SaveChangesMessage", GetLastError());
			}
		}
	}
	

	MAPIFreeBuffer(ical2exchange.lpProps);
	ical2exchange_reset(&ical2exchange);
	talloc_free(mem_ctx);

}
