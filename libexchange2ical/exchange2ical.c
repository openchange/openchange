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

#include "libexchange2ical/libexchange2ical.h"


static void exchange2ical_init(TALLOC_CTX *mem_ctx, struct exchange2ical *exchange2ical)
{
	exchange2ical->TimeZoneStruct = NULL;
	exchange2ical->TimeZoneDesc = NULL;
	exchange2ical->method = ICAL_METHOD_NONE;
	exchange2ical->vtimezone = NULL;
	exchange2ical->vcalendar = NULL;
	exchange2ical->mem_ctx = mem_ctx;
	exchange2ical->partstat = ICAL_PARTSTAT_NONE;
	exchange2ical->ResponseStatus = NULL;
	exchange2ical->Recurring = NULL;
	exchange2ical->RecurrencePattern = NULL;
	exchange2ical->AppointmentRecurrencePattern = NULL;
	exchange2ical->Keywords = NULL;
	exchange2ical->Contacts = NULL;
	exchange2ical->apptStateFlags = NULL;
	exchange2ical->sensitivity = NULL;
	exchange2ical->apptStartWhole = NULL;
	exchange2ical->apptEndWhole = NULL;
	exchange2ical->apptSubType = NULL;
	exchange2ical->OwnerCriticalChange = NULL;
	exchange2ical->body = NULL;
	exchange2ical->LastModified = NULL;
	exchange2ical->Location = NULL;
	exchange2ical->Importance = NULL;
	exchange2ical->ExceptionReplaceTime = NULL;
	exchange2ical->ResponseRequested = NULL;
	exchange2ical->NonSendableBcc = NULL;
	exchange2ical->Sequence = NULL;
	exchange2ical->Subject = NULL;
	exchange2ical->MessageLocaleId = NULL;
	exchange2ical->BusyStatus = NULL;
	exchange2ical->IntendedBusyStatus = NULL;
	exchange2ical->GlobalObjectId = NULL;
	exchange2ical->AttendeeCriticalChange = NULL;
	exchange2ical->OwnerApptId = NULL;
	exchange2ical->apptReplyTime = NULL;
	exchange2ical->NotAllowPropose = NULL;
	exchange2ical->AllowExternCheck = NULL;
	exchange2ical->apptLastSequence = NULL;
	exchange2ical->apptSeqTime = NULL;
	exchange2ical->AutoFillLocation = NULL;
	exchange2ical->AutoStartCheck = NULL;
	exchange2ical->CollaborateDoc = NULL;
	exchange2ical->ConfCheck = NULL;
	exchange2ical->ConfType = NULL;
	exchange2ical->Directory = NULL;
	exchange2ical->MWSURL = NULL;
	exchange2ical->NetShowURL = NULL;
	exchange2ical->OnlinePassword = NULL;
	exchange2ical->OrgAlias = NULL;
	exchange2ical->SenderName = NULL;
	exchange2ical->SenderEmailAddress = NULL;
	exchange2ical->ReminderSet = NULL;
	exchange2ical->ReminderDelta = NULL;
	exchange2ical->vevent = NULL;
	exchange2ical->valarm = NULL;
	exchange2ical->bodyHTML = NULL;
	exchange2ical->idx=0;
}

static void exchange2ical_clear(struct exchange2ical *exchange2ical)
{
	if (exchange2ical->AppointmentRecurrencePattern){
		talloc_free(exchange2ical->AppointmentRecurrencePattern);
	}
	
	if (exchange2ical->TimeZoneStruct) {
		talloc_free(exchange2ical->TimeZoneStruct);
	}

	exchange2ical_init(exchange2ical->mem_ctx, exchange2ical);
}


static void exchange2ical_reset(struct exchange2ical *exchange2ical)
{
	if (exchange2ical->AppointmentRecurrencePattern){
		talloc_free(exchange2ical->AppointmentRecurrencePattern);
	}

	if (exchange2ical->TimeZoneStruct) {
		talloc_free(exchange2ical->TimeZoneStruct);
	}
	
	exchange2ical->partstat = ICAL_PARTSTAT_NONE;
	exchange2ical->ResponseStatus = NULL;
	exchange2ical->Recurring = NULL;
	exchange2ical->RecurrencePattern = NULL;
	exchange2ical->AppointmentRecurrencePattern = NULL;
	exchange2ical->Keywords = NULL;
	exchange2ical->Contacts = NULL;
	exchange2ical->apptStateFlags = NULL;
	exchange2ical->sensitivity = NULL;
	exchange2ical->apptStartWhole = NULL;
	exchange2ical->apptEndWhole = NULL;
	exchange2ical->apptSubType = NULL;
	exchange2ical->OwnerCriticalChange = NULL;
	exchange2ical->body = NULL;
	exchange2ical->LastModified = NULL;
	exchange2ical->Location = NULL;
	exchange2ical->Importance = NULL;
	exchange2ical->ExceptionReplaceTime = NULL;
	exchange2ical->ResponseRequested = NULL;
	exchange2ical->NonSendableBcc = NULL;
	exchange2ical->Sequence = NULL;
	exchange2ical->Subject = NULL;
	exchange2ical->MessageLocaleId = NULL;
	exchange2ical->BusyStatus = NULL;
	exchange2ical->IntendedBusyStatus = NULL;
	exchange2ical->GlobalObjectId = NULL;
	exchange2ical->AttendeeCriticalChange = NULL;
	exchange2ical->OwnerApptId = NULL;
	exchange2ical->apptReplyTime = NULL;
	exchange2ical->NotAllowPropose = NULL;
	exchange2ical->AllowExternCheck = NULL;
	exchange2ical->apptLastSequence = NULL;
	exchange2ical->apptSeqTime = NULL;
	exchange2ical->AutoFillLocation = NULL;
	exchange2ical->AutoStartCheck = NULL;
	exchange2ical->CollaborateDoc = NULL;
	exchange2ical->ConfCheck = NULL;
	exchange2ical->ConfType = NULL;
	exchange2ical->Directory = NULL;
	exchange2ical->MWSURL = NULL;
	exchange2ical->NetShowURL = NULL;
	exchange2ical->OnlinePassword = NULL;
	exchange2ical->OrgAlias = NULL;
	exchange2ical->SenderName = NULL;
	exchange2ical->SenderEmailAddress = NULL;
	exchange2ical->ReminderSet = NULL;
	exchange2ical->ReminderDelta = NULL;
	exchange2ical->vevent = NULL;
	exchange2ical->valarm = NULL;
	exchange2ical->bodyHTML = NULL;
	exchange2ical->TimeZoneDesc = NULL;
	exchange2ical->TimeZoneStruct = NULL;
}


static int exchange2ical_get_properties(TALLOC_CTX *mem_ctx, struct SRow *aRow, struct exchange2ical *exchange2ical, enum exchange2ical_flags eFlags)
{
	struct Binary_r	*apptrecur;
	const char *messageClass;
	struct Binary_r	*TimeZoneStruct;

	if(eFlags & VcalFlag){
		messageClass = octool_get_propval(aRow, PR_MESSAGE_CLASS_UNICODE);
		exchange2ical->method = get_ical_method(messageClass);
		if (!exchange2ical->method) return -1;
	}
	
	if(((eFlags & RangeFlag) && !(eFlags & EntireFlag))||
		(!(eFlags & RangeFlag) && (eFlags & EntireFlag))){
		exchange2ical->apptStartWhole = (const struct FILETIME *)octool_get_propval(aRow, PidLidAppointmentStartWhole);

	}
	
	if(((eFlags & EventFlag) && !(eFlags & EntireFlag))||
		(!(eFlags & EventFlag) && (eFlags & EntireFlag))){
		exchange2ical->GlobalObjectId = (struct Binary_r *) octool_get_propval(aRow, PidLidGlobalObjectId);
		exchange2ical->Sequence = (uint32_t *) octool_get_propval(aRow, PidLidAppointmentSequence); 	

	}
	
	if(((eFlags & EventsFlag) && !(eFlags & EntireFlag))||
		(!(eFlags & EventsFlag) && (eFlags & EntireFlag))){
		exchange2ical->GlobalObjectId = (struct Binary_r *) octool_get_propval(aRow, PidLidGlobalObjectId);

	}
	
	if(eFlags & EntireFlag) {
	  
		apptrecur = (struct Binary_r *) octool_get_propval(aRow, PidLidAppointmentRecur);
		exchange2ical->AppointmentRecurrencePattern = get_AppointmentRecurrencePattern(mem_ctx,apptrecur);
		exchange2ical->RecurrencePattern = &exchange2ical->AppointmentRecurrencePattern->RecurrencePattern;
		
		TimeZoneStruct = (struct Binary_r *) octool_get_propval(aRow, PidLidTimeZoneStruct);
		exchange2ical->TimeZoneStruct = get_TimeZoneStruct(mem_ctx, TimeZoneStruct);

		exchange2ical->TimeZoneDesc = (const char *) octool_get_propval(aRow, PidLidTimeZoneDescription);
		exchange2ical->Keywords = (const struct StringArray_r *) octool_get_propval(aRow, PidNameKeywords);
		exchange2ical->Recurring = (uint8_t *) octool_get_propval(aRow, PidLidRecurring);
		exchange2ical->TimeZoneDesc = (const char *) octool_get_propval(aRow, PidLidTimeZoneDescription);
		exchange2ical->ExceptionReplaceTime = (const struct FILETIME *)octool_get_propval(aRow, PidLidExceptionReplaceTime);
		exchange2ical->ResponseStatus = (uint32_t *) octool_get_propval(aRow, PidLidResponseStatus);
		exchange2ical->apptStateFlags = (uint32_t *) octool_get_propval(aRow, PidLidAppointmentStateFlags);
		exchange2ical->Contacts = (const struct StringArray_r *)octool_get_propval(aRow, PidLidContacts);
		exchange2ical->apptEndWhole = (const struct FILETIME *)octool_get_propval(aRow, PidLidAppointmentEndWhole);	
		exchange2ical->apptSubType = (uint8_t *) octool_get_propval(aRow, PidLidAppointmentSubType);
		exchange2ical->OwnerCriticalChange = (const struct FILETIME *)octool_get_propval(aRow, PidLidOwnerCriticalChange);
		exchange2ical->Location = (const char *) octool_get_propval(aRow, PidLidLocation); 	
		exchange2ical->NonSendableBcc = (const char *) octool_get_propval(aRow, PidLidNonSendableBcc);
		exchange2ical->BusyStatus = (uint32_t *) octool_get_propval(aRow, PidLidBusyStatus); 	
		exchange2ical->IntendedBusyStatus = (uint32_t *) octool_get_propval(aRow, PidLidIntendedBusyStatus);	
		exchange2ical->AttendeeCriticalChange = (const struct FILETIME *) octool_get_propval(aRow, PidLidAttendeeCriticalChange);	
		exchange2ical->apptReplyTime = (const struct FILETIME *)octool_get_propval(aRow, PidLidAppointmentReplyTime);
		exchange2ical->NotAllowPropose = (uint8_t *) octool_get_propval(aRow, PidLidAppointmentNotAllowPropose);	
		exchange2ical->AllowExternCheck = (uint8_t *) octool_get_propval(aRow, PidLidAllowExternalCheck);
		exchange2ical->apptLastSequence = (uint32_t *) octool_get_propval(aRow, PidLidAppointmentLastSequence);
		exchange2ical->apptSeqTime = (const struct FILETIME *)octool_get_propval(aRow, PidLidAppointmentSequenceTime);	
		exchange2ical->AutoFillLocation = (uint8_t *) octool_get_propval(aRow, PidLidAutoFillLocation);
		exchange2ical->AutoStartCheck = (uint8_t *) octool_get_propval(aRow, PidLidAutoStartCheck);
		exchange2ical->CollaborateDoc = (const char *) octool_get_propval(aRow, PidLidCollaborateDoc);
		exchange2ical->ConfCheck = (uint8_t *) octool_get_propval(aRow, PidLidConferencingCheck);
		exchange2ical->ConfType = (uint32_t *) octool_get_propval(aRow, PidLidConferencingType);
		exchange2ical->Directory = (const char *) octool_get_propval(aRow, PidLidDirectory);
		exchange2ical->MWSURL = (const char *) octool_get_propval(aRow, PidLidMeetingWorkspaceUrl);
		exchange2ical->NetShowURL = (const char *) octool_get_propval(aRow, PidLidNetShowUrl);
		exchange2ical->OnlinePassword = (const char *) octool_get_propval(aRow, PidLidOnlinePassword);
		exchange2ical->OrgAlias = (const char *) octool_get_propval(aRow, PidLidOrganizerAlias);
		exchange2ical->ReminderSet = (uint8_t *) octool_get_propval(aRow, PidLidReminderSet);
		exchange2ical->ReminderDelta = (uint32_t *) octool_get_propval(aRow, PidLidReminderDelta);
		exchange2ical->sensitivity = (uint32_t *) octool_get_propval(aRow, PR_SENSITIVITY);
		exchange2ical->created = (const struct FILETIME *)octool_get_propval(aRow, PR_CREATION_TIME);
		exchange2ical->body = (const char *)octool_get_propval(aRow, PR_BODY_UNICODE);
		exchange2ical->LastModified = (const struct FILETIME *)octool_get_propval(aRow, PR_LAST_MODIFICATION_TIME);
		exchange2ical->Importance = (uint32_t *) octool_get_propval(aRow, PR_IMPORTANCE);
		exchange2ical->ResponseRequested = (uint8_t *) octool_get_propval(aRow, PR_RESPONSE_REQUESTED);
		exchange2ical->Subject = (const char *) octool_get_propval(aRow, PR_SUBJECT_UNICODE);
		exchange2ical->MessageLocaleId = (uint32_t *) octool_get_propval(aRow, PR_MESSAGE_LOCALE_ID);
		exchange2ical->OwnerApptId = (uint32_t *) octool_get_propval(aRow, PR_OWNER_APPT_ID);
		exchange2ical->SenderName = (const char *) octool_get_propval(aRow, PR_SENDER_NAME);
		exchange2ical->SenderEmailAddress = (const char *) octool_get_propval(aRow, PR_SENDER_EMAIL_ADDRESS);
	}
	
	return 0;
	
}


static uint8_t exchange2ical_exception_from_ExceptionInfo(struct exchange2ical *exchange2ical, struct exchange2ical_check *exchange2ical_check)
{
	uint32_t	i;
	icalproperty *prop;
	icalparameter *param;
	icaltimetype icaltime;
	/*sanity checks*/
	if(!exchange2ical->AppointmentRecurrencePattern) return 1;
	if(!exchange2ical->AppointmentRecurrencePattern->ExceptionInfo) return 1;
	if(!exchange2ical->AppointmentRecurrencePattern->ExceptionCount) return 1;

	for(i=0; i<exchange2ical->AppointmentRecurrencePattern->ExceptionCount; i++) {
		
		/*Check to see if event is acceptable*/
		struct tm apptStart=get_tm_from_minutes_UTC(exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->StartDateTime);
		if (!checkEvent(exchange2ical, exchange2ical_check, &apptStart)){
			return 0;
		}
		
		/*Create a new vevent*/
		exchange2ical->vevent = icalcomponent_new_vevent();
		if ( ! (exchange2ical->vevent) ) {
			return 1;
		}
		icalcomponent_add_component(exchange2ical->vcalendar, exchange2ical->vevent);

		/*dtstart from StartDateTime*/
		if (exchange2ical->apptSubType && (*exchange2ical->apptSubType == 0x1)) {
			struct tm	tm;
			tm = get_tm_from_minutes_UTC(exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->StartDateTime);
			icaltime= get_icaldate_from_tm(&tm);
			prop = icalproperty_new_dtstart(icaltime);
			icalcomponent_add_property(exchange2ical->vevent, prop);
		}else {
			if(exchange2ical->TimeZoneDesc){
				struct tm	tm;
				tm = get_tm_from_minutes(exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->StartDateTime);
				icaltime= get_icaldate_from_tm(&tm);
				prop = icalproperty_new_dtstart(icaltime);
				icalcomponent_add_property(exchange2ical->vevent, prop);
				param = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
				icalproperty_add_parameter(prop, param);
			} else {
				struct tm	tm;
				tm = get_tm_from_minutes_UTC(exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->StartDateTime);
				icaltime= get_icaldate_from_tm(&tm);
				prop = icalproperty_new_dtstart(icaltime);
				icalcomponent_add_property(exchange2ical->vevent, prop);
			}
		}
		/*dtend from EndDateTime*/
		if (exchange2ical->apptSubType && (*exchange2ical->apptSubType == 0x1)) {
			struct tm	tm;
			tm = get_tm_from_minutes_UTC(exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->EndDateTime);
			icaltime= get_icaldate_from_tm(&tm);
			prop = icalproperty_new_dtend(icaltime);
			icalcomponent_add_property(exchange2ical->vevent, prop);
		}else {
			if(exchange2ical->TimeZoneDesc){
				struct tm	tm;
				tm = get_tm_from_minutes(exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->EndDateTime);
				icaltime= get_icaldate_from_tm(&tm);
				prop = icalproperty_new_dtend(icaltime);
				icalcomponent_add_property(exchange2ical->vevent, prop);
				param = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
				icalproperty_add_parameter(prop, param);
			} else {
				struct tm	tm;
				tm = get_tm_from_minutes_UTC(exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->EndDateTime);
				icaltime= get_icaldate_from_tm(&tm);
				prop = icalproperty_new_dtend(icaltime);
				icalcomponent_add_property(exchange2ical->vevent, prop);
			}
		}
		/*recurrence-id from OriginalStartDate*/
		if (exchange2ical->apptSubType && (*exchange2ical->apptSubType == 0x1)) {
			struct tm	tm;
			tm = get_tm_from_minutes_UTC(exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->OriginalStartDate);
			icaltime= get_icaldate_from_tm(&tm);
			prop = icalproperty_new_recurrenceid(icaltime);
			icalcomponent_add_property(exchange2ical->vevent, prop);
		}else {
			if(exchange2ical->TimeZoneDesc){
				struct tm	tm;
				tm = get_tm_from_minutes(exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->OriginalStartDate);
				icaltime= get_icaldate_from_tm(&tm);
				prop = icalproperty_new_recurrenceid(icaltime);
				icalcomponent_add_property(exchange2ical->vevent, prop);
				param = icalparameter_new_tzid(exchange2ical->TimeZoneDesc);
				icalproperty_add_parameter(prop, param);
			} else {
				struct tm	tm;
				tm = get_tm_from_minutes_UTC(exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->OriginalStartDate);
				icaltime= get_icaldate_from_tm(&tm);
				prop = icalproperty_new_recurrenceid(icaltime);
				icalcomponent_add_property(exchange2ical->vevent, prop);
			}
		}
		
		/*summary from Subject if subject is set*/
		if (exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->OverrideFlags & 0x0001) {
			exchange2ical->Subject=(const char *) &exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->Subject.subjectMsg.msg;
			ical_property_SUMMARY(exchange2ical);
		}
		

		/*Valarm*/
		/*ReminderSet*/
		if(exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->OverrideFlags & 0x0008){
			exchange2ical->ReminderSet=(uint8_t *) &exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->ReminderDelta.rSet;
			/*Reminder Delta*/
			if(exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->OverrideFlags & 0x0004){
				exchange2ical->ReminderDelta=&exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->ReminderDelta.rDelta;
			} else {
				exchange2ical->ReminderDelta=NULL;
			}
			ical_component_VALARM(exchange2ical);
		}

		/*Location*/
		if(exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->OverrideFlags & 0x0010){
			exchange2ical->Location=(const char *) &exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->Location.locationMsg.msg;
			ical_property_LOCATION(exchange2ical);
		}
		
		/*Busy Status*/
		if(exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->OverrideFlags & 0x0020){
			exchange2ical->BusyStatus=&exchange2ical->AppointmentRecurrencePattern->ExceptionInfo->BusyStatus.bStatus;
			ical_property_X_MICROSOFT_CDO_BUSYSTATUS(exchange2ical);
		}
	}
	return 0;
}

static uint8_t exchange2ical_exception_from_EmbeddedObj(struct exchange2ical *exchange2ical, struct exchange2ical_check *exchange2ical_check)
{
	mapi_object_t			obj_tb_attach;
	mapi_object_t			obj_attach;
	struct SRowSet			rowset_attach;
	struct SPropTagArray		*SPropTagArray;
	const uint32_t			*attach_num;
	struct SPropValue		*lpProps;
	enum MAPISTATUS			retval;
	unsigned int			i;
	uint32_t			count;
	struct SRow			aRow2;
	struct SRow			aRowT;
	
	mapi_object_init(&obj_tb_attach);
	retval = GetAttachmentTable(&exchange2ical->obj_message, &obj_tb_attach);
	if (retval != MAPI_E_SUCCESS) {
		return 1;
	}else {
		SPropTagArray = set_SPropTagArray(exchange2ical->mem_ctx, 0x1, PR_ATTACH_NUM);
		retval = SetColumns(&obj_tb_attach, SPropTagArray);
		MAPIFreeBuffer(SPropTagArray);
		retval = QueryRows(&obj_tb_attach, 0xa, TBL_ADVANCE, TBL_FORWARD_READ, &rowset_attach);

		for (i = 0; i < rowset_attach.cRows; i++) {
			
			attach_num = (const uint32_t *)find_SPropValue_data(&(rowset_attach.aRow[i]), PR_ATTACH_NUM);
			retval = OpenAttach(&exchange2ical->obj_message, *attach_num, &obj_attach);

			if (retval != MAPI_E_SUCCESS) {
				return 1;
			}else {
				SPropTagArray = set_SPropTagArray(exchange2ical->mem_ctx, 0x3,
									  PR_ATTACH_METHOD,
									  PR_ATTACHMENT_FLAGS,
									  PR_ATTACHMENT_HIDDEN
									  );
									  
				lpProps = NULL;
				retval = GetProps(&obj_attach, 0, SPropTagArray, &lpProps, &count);
				MAPIFreeBuffer(SPropTagArray);
				if (retval != MAPI_E_SUCCESS) {
					return 1;
				}else {
					aRow2.ulAdrEntryPad = 0;
					aRow2.cValues = count;
					aRow2.lpProps = lpProps;
					
					uint32_t	*attachmentFlags;
					uint32_t	*attachMethod;
					uint8_t		*attachmentHidden;
					
					attachmentFlags	 = (uint32_t *) octool_get_propval(&aRow2, PR_ATTACHMENT_FLAGS);
					attachMethod	 = (uint32_t *) octool_get_propval(&aRow2, PR_ATTACH_METHOD);
					attachmentHidden = (uint8_t *) octool_get_propval(&aRow2, PR_ATTACHMENT_HIDDEN);

					if(attachmentFlags && (*attachmentFlags & 0x00000002) 
						&& (*attachMethod == 0x00000005) 
						&& (attachmentHidden && (*attachmentHidden))) {
					
						struct exchange2ical exception;
						exchange2ical_init(exchange2ical->mem_ctx,&exception);
					
						mapi_object_init(&exception.obj_message);
						
						retval = OpenEmbeddedMessage(&obj_attach, &exception.obj_message, MAPI_READONLY);
						if (retval != MAPI_E_SUCCESS) {
							return 1;
						}else {							
 							SPropTagArray = set_SPropTagArray(exchange2ical->mem_ctx, 0x2d,
												PidLidFExceptionalBody,
												PidLidRecurring,
												PidLidAppointmentRecur,
												PidLidAppointmentStateFlags,
												PidLidTimeZoneDescription,
												PidLidTimeZoneStruct,
												PidLidAppointmentStartWhole,
												PidLidAppointmentEndWhole,
												PidLidAppointmentSubType,
												PidLidOwnerCriticalChange,
												PidLidLocation,
												PidLidExceptionReplaceTime,
												PidLidNonSendableBcc,
												PidLidAppointmentSequence,
												PidLidBusyStatus,
												PidLidIntendedBusyStatus,
												PidLidCleanGlobalObjectId,
												PidLidAttendeeCriticalChange,
												PidLidAppointmentReplyTime,
												PidLidAppointmentNotAllowPropose,
												PidLidAllowExternalCheck,
												PidLidAppointmentLastSequence,
												PidLidAppointmentSequenceTime,
												PidLidAutoFillLocation,
												PidLidAutoStartCheck,
												PidLidCollaborateDoc,
												PidLidConferencingCheck,
												PidLidConferencingType,
												PidLidDirectory,
												PidLidNetShowUrl,
												PidLidOnlinePassword,
												PidLidOrganizerAlias,
												PidLidReminderSet,
												PidLidReminderDelta,
												PR_MESSAGE_CLASS_UNICODE,
												PR_BODY_UNICODE,
												PR_CREATION_TIME,
												PR_LAST_MODIFICATION_TIME,
												PR_IMPORTANCE,
												PR_RESPONSE_REQUESTED,
												PR_SUBJECT_UNICODE,
												PR_OWNER_APPT_ID,
												PR_SENDER_NAME,
												PR_SENDER_EMAIL_ADDRESS,
												PR_MESSAGE_LOCALE_ID
 								  );
								  
								  
		
							retval = GetProps(&exception.obj_message, MAPI_UNICODE, SPropTagArray, &lpProps, &count);
							
							if (retval == MAPI_E_SUCCESS) {	
								aRow2.ulAdrEntryPad = 0;
								aRow2.cValues = count;
								aRow2.lpProps = lpProps;
								
								
								/*Get required properties to check if right event*/
								exchange2ical_get_properties(exchange2ical->mem_ctx, &aRow2, &exception, exchange2ical_check->eFlags);
					
								/*Check to see if event is acceptable*/
								if (!checkEvent(&exception, exchange2ical_check, get_tm_from_FILETIME(exception.apptStartWhole))){
									break;
								}

								/*Grab Rest of Properties*/
								exchange2ical_get_properties(exchange2ical->mem_ctx, &aRow2, &exception, exchange2ical_check->eFlags | EntireFlag);
								uint8_t *dBody = (uint8_t *) octool_get_propval(&aRow2, PidLidFExceptionalBody);
								retval = GetRecipientTable(&exception.obj_message, 
									&exception.Recipients.SRowSet,
									&exception.Recipients.SPropTagArray);
									
								/*Check for set subject*/
								if (!exception.Subject){
									exception.Subject=exchange2ical->Subject;
								}
								/*Check for a set apptSubType*/
								if (!exception.apptSubType){
									exception.apptSubType=exchange2ical->apptSubType;
								}
								/*check for a set Location*/
								if (!exception.Location){
									exception.Location=exchange2ical->Location;
								}
								/*check for set valarm info*/
								if(!exception.ReminderSet){
									exception.ReminderSet=exchange2ical->ReminderSet;
								}
								if(!exception.ReminderDelta){
									exception.ReminderDelta=exchange2ical->ReminderDelta;
								}
								
								/*Set to same vcalendar as parent*/
								exception.vcalendar=exchange2ical->vcalendar;
								
								/*Set to same uid fallback in case GlobalObjId is missing*/
								exception.idx=exchange2ical->idx;
								
								
								/*has a modified summary*/
								if(dBody && *dBody){
									SPropTagArray = set_SPropTagArray(exchange2ical->mem_ctx, 0x1, PR_BODY_HTML_UNICODE);
									retval = GetProps(&exception.obj_message, MAPI_UNICODE, SPropTagArray, &lpProps, &count);
									MAPIFreeBuffer(SPropTagArray);
									if (retval == MAPI_E_SUCCESS) {
										aRowT.ulAdrEntryPad = 0;
										aRowT.cValues = count;
										aRowT.lpProps = lpProps;
										exception.bodyHTML = (const char *)octool_get_propval(&aRowT, PR_BODY_HTML_UNICODE);
									}
								/* has the same summary as parent*/
								} else{
									exception.body=exchange2ical->body;
									exception.bodyHTML=exchange2ical->bodyHTML;
								}
								
								ical_component_VEVENT(&exception);
								exception.vcalendar=NULL;
								exchange2ical_clear(&exception);
							}
						} 							
						mapi_object_release(&exception.obj_message);
					}
					MAPIFreeBuffer(lpProps);
				}
			}
			
		}

	}
	mapi_object_release(&obj_tb_attach);
	return 0;	
}

icalcomponent * _Exchange2Ical(mapi_object_t *obj_folder, struct exchange2ical_check *exchange2ical_check)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	int				ret;
	struct SRowSet			SRowSet;
	struct SRow			aRow;
	struct SRow			aRowT;
	struct SPropValue		*lpProps;
	struct SPropTagArray		*SPropTagArray = NULL;
	struct exchange2ical		exchange2ical;
	mapi_object_t			obj_table;
	uint32_t			count;
	int				i;

	mem_ctx = talloc_named(mapi_object_get_session(obj_folder), 0, "exchange2ical");
	exchange2ical_init(mem_ctx, &exchange2ical);
	
	/* Open the contents table */
	mapi_object_init(&obj_table);
	retval = GetContentsTable(obj_folder, &obj_table, 0, &count);
	if (retval != MAPI_E_SUCCESS){
		talloc_free(mem_ctx);
		return NULL;
	}
	
	DEBUG(0, ("MAILBOX (%d appointments)\n", count));
	if (count == 0) {
		talloc_free(mem_ctx);
		return NULL;
	}

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2,
					  PR_FID,
					  PR_MID);
					  
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("SetColumns", retval);
		talloc_free(mem_ctx);
		return NULL;
	}
	
	while ((retval = QueryRows(&obj_table, count, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet)) != MAPI_E_NOT_FOUND && SRowSet.cRows) {
		count -= SRowSet.cRows;
		for (i = (SRowSet.cRows-1); i >= 0; i--) {
			mapi_object_init(&exchange2ical.obj_message);
			retval = OpenMessage(obj_folder,
					     SRowSet.aRow[i].lpProps[0].value.d,
					     SRowSet.aRow[i].lpProps[1].value.d,
					     &exchange2ical.obj_message, 0);
			if (retval != MAPI_E_NOT_FOUND) {
				SPropTagArray = set_SPropTagArray(mem_ctx, 0x30,
								  PidLidGlobalObjectId,
								  PidNameKeywords,
								  PidLidRecurring,
								  PidLidAppointmentRecur,
								  PidLidAppointmentStateFlags,
								  PidLidTimeZoneDescription,
								  PidLidTimeZoneStruct,
								  PidLidContacts,
								  PidLidAppointmentStartWhole,
								  PidLidAppointmentEndWhole,
								  PidLidAppointmentSubType,
								  PidLidOwnerCriticalChange,
								  PidLidLocation,
								  PidLidNonSendableBcc,
								  PidLidAppointmentSequence,
								  PidLidBusyStatus,
								  PidLidIntendedBusyStatus,
								  PidLidAttendeeCriticalChange,
								  PidLidAppointmentReplyTime,
								  PidLidAppointmentNotAllowPropose,
								  PidLidAllowExternalCheck,
								  PidLidAppointmentLastSequence,
								  PidLidAppointmentSequenceTime,
								  PidLidAutoFillLocation,
								  PidLidAutoStartCheck,
								  PidLidCollaborateDoc,
								  PidLidConferencingCheck,
								  PidLidConferencingType,
								  PidLidDirectory,
								  PidLidMeetingWorkspaceUrl,
								  PidLidNetShowUrl,
								  PidLidOnlinePassword,
								  PidLidOrganizerAlias,
								  PidLidReminderSet,
								  PidLidReminderDelta,
								  PidLidResponseStatus,
								  PR_MESSAGE_CLASS_UNICODE,
								  PR_SENSITIVITY,
								  PR_BODY_UNICODE,
								  PR_CREATION_TIME,
								  PR_LAST_MODIFICATION_TIME,
								  PR_IMPORTANCE,
								  PR_RESPONSE_REQUESTED,
								  PR_SUBJECT_UNICODE,
								  PR_OWNER_APPT_ID,
								  PR_SENDER_NAME,
								  PR_SENDER_EMAIL_ADDRESS,
								  PR_MESSAGE_LOCALE_ID
								  );
								  
								  
				retval = GetProps(&exchange2ical.obj_message, MAPI_UNICODE, SPropTagArray, &lpProps, &count);

				MAPIFreeBuffer(SPropTagArray);
	
				if (retval == MAPI_E_SUCCESS) {
					aRow.ulAdrEntryPad = 0;
					aRow.cValues = count;
					aRow.lpProps = lpProps;
					
					/*Get Vcal info if first event*/
					if(i==(SRowSet.cRows-1)){
						ret = exchange2ical_get_properties(mem_ctx, &aRow, &exchange2ical, VcalFlag);
						/*TODO: exit nicely*/
						ical_component_VCALENDAR(&exchange2ical);
					}
					
					
					/*Get required properties to check if right event*/
					ret = exchange2ical_get_properties(mem_ctx, &aRow, &exchange2ical, exchange2ical_check->eFlags);
					
					/*Check to see if event is acceptable*/
					if (!checkEvent(&exchange2ical, exchange2ical_check, get_tm_from_FILETIME(exchange2ical.apptStartWhole))){
						continue;
					}
					
					/*Set RecipientTable*/
					retval = GetRecipientTable(&exchange2ical.obj_message, 
							   &exchange2ical.Recipients.SRowSet,
							   &exchange2ical.Recipients.SPropTagArray);
					
					/*Set PR_BODY_HTML for x_alt_desc property*/
					SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_BODY_HTML_UNICODE);
					retval = GetProps(&exchange2ical.obj_message, MAPI_UNICODE, SPropTagArray, &lpProps, &count);
					MAPIFreeBuffer(SPropTagArray);
					if (retval == MAPI_E_SUCCESS) {
						aRowT.ulAdrEntryPad = 0;
						aRowT.cValues = count;
						aRowT.lpProps = lpProps;
						exchange2ical.bodyHTML = (const char *)octool_get_propval(&aRowT, PR_BODY_HTML_UNICODE);
					}
					
					/*Get rest of properties*/
					ret = exchange2ical_get_properties(mem_ctx, &aRow, &exchange2ical, (exchange2ical_check->eFlags | EntireFlag));
					
					/*add new vevent*/
					ical_component_VEVENT(&exchange2ical);
					
					/*Exceptions to event*/
					if(exchange2ical_check->eFlags != EventFlag){
						ret = exchange2ical_exception_from_EmbeddedObj(&exchange2ical, exchange2ical_check);
						if (ret){
							ret=exchange2ical_exception_from_ExceptionInfo(&exchange2ical, exchange2ical_check);
						}
					}
					
					/*REMOVE once globalobjid is fixed*/
					exchange2ical.idx++;
					
					MAPIFreeBuffer(lpProps);
					exchange2ical_reset(&exchange2ical);
				}
				
			}
			mapi_object_release(&exchange2ical.obj_message);
		}
	}

	icalcomponent *icalendar = exchange2ical.vcalendar;
	exchange2ical_clear(&exchange2ical);
	
	/* Uninitialize MAPI subsystem */
	mapi_object_release(&obj_table);
	talloc_free(mem_ctx);	
	return icalendar;
}
