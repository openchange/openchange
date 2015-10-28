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

#define MAXCAT  100000


/*Taken from Samba4 code*/
/*
  this base64 decoder was taken from jitterbug (written by tridge).
  we might need to replace it with a new version
*/
static int ldb_base64_decode(char *s)
{
	const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int bit_offset=0, byte_offset, idx, i, n;
	uint8_t *d = (uint8_t *)s;
	char *p=NULL;

	n=i=0;

	while (*s && (p=strchr(b64,*s))) {
		idx = (int)(p - b64);
		byte_offset = (i*6)/8;
		bit_offset = (i*6)%8;
		d[byte_offset] &= ~((1<<(8-bit_offset))-1);
		if (bit_offset < 3) {
			d[byte_offset] |= (idx << (2-bit_offset));
			n = byte_offset+1;
		} else {
			d[byte_offset] |= (idx >> (bit_offset-2));
			d[byte_offset+1] = 0;
			d[byte_offset+1] |= (idx << (8-(bit_offset-2))) & 0xFF;
			n = byte_offset+2;
		}
		s++; i++;
	}
	if (bit_offset >= 3) {
		n--;
	}

	if (*s && !p) {
		/* the only termination allowed */
		if (*s != '=') {
			return -1;
		}
	}

	/* null terminate */
	d[n] = 0;
	return n;
}


void ical2exchange_property_ATTACH(struct ical2exchange *ical2exchange)
{
	int 		data;
	char 		*extension = NULL;
	char 		*filename = NULL;
	const char 	*fmttype = NULL;
	icalattach	*icalattach = NULL;
	icalparameter	*fmttypePar = NULL;
	icalparameter	*xfilePar = NULL;
	const char 	*xname = NULL;
	icalproperty	*attachProp = NULL;
	
	/*sanity check*/
	if(!ical2exchange->attachEvent) return;
	
	attachProp=icalcomponent_get_first_property(ical2exchange->attachEvent, ICAL_ATTACH_PROPERTY);
	while(attachProp){
	
		icalattach = icalproperty_get_attach(attachProp);
		data = ldb_base64_decode((char *) icalattach_get_data (icalattach));
		
		/*FMTTYPE*/
		fmttypePar = icalproperty_get_first_parameter(attachProp, ICAL_FMTTYPE_PARAMETER);
		if(fmttypePar){
			fmttype = icalparameter_get_fmttype(fmttypePar);
		}
		
		/*X-FIlename*/
		xfilePar = icalproperty_get_first_parameter(attachProp, ICAL_X_PARAMETER);
		if(xfilePar){
			xname = icalparameter_get_xname(xfilePar);
			if(!strcmp(xname,"X-FILENAME")){
				filename = (char *) icalparameter_get_x(xfilePar);
		
			}
		}
		
		/*Extension*/
		if(filename){
			char buff[256]; 
			char *temp;
			strncpy(buff,filename, 255);
			buff[255] = '\0';
			extension = strtok(buff, ".");
			while((temp = strtok(NULL, "."))) extension = temp;
		}
		
		printf("Create a new attachment object with\n");
		printf("	set PidTagAttachDataBinary to %d\n", data);
		printf("	set PidTagAttachExtension to %s\n", extension);
		printf("	set PidTagAttachFilename to %s\n", filename);
		printf("	set PidTagAttachLongFilename to %s\n", filename);
		printf("	set PidTagAttachMimeTag to %s\n", fmttype);
		printf("	set PidTagAttachFlags to 0x00000000\n");
		printf("	set PidTagAttachMethod to 0x00000001\n");
		printf("	set PidTagAttachmentContactPhoto to FALSE\n");
		printf("	set PidTagAttachmentFlags to 0x00000000\n");
		printf("	set PidTagAttachEncoding to empty SBinary");
		printf("	set PidTagAttachmentHidden to FALSE\n");
		printf("	set PidTagAttachmentLinkId to 0x00000000\n");
		printf("	set PidTagDisplayName to  %s\n", filename);
		printf("	set PidTagExceptionEndTime to 0x0CB34557A3DD4000\n");
		printf("	set PidTagExceptionStartTime to 0x0CB34557A3DD4000\n");
		printf("	set PidTagRenderingPosition to 0xFFFFFFFF\n");
		
		attachProp=icalcomponent_get_next_property(ical2exchange->attachEvent, ICAL_ATTACH_PROPERTY);
	}
}


//TODO:
void ical2exchange_property_CATEGORIES(struct ical2exchange *ical2exchange)
{
	struct StringArray_r *sArray;
	char **stringArray = NULL;
	char string[256];
	char *value;
	char *tok;
	icalproperty *categoriesProp;
	uint32_t i = 0;

	/*sanity check*/
 	if(!ical2exchange->categoriesEvent) return;
	
	sArray = talloc(ical2exchange->mem_ctx, struct StringArray_r);
	
	categoriesProp = icalcomponent_get_first_property(ical2exchange->categoriesEvent, ICAL_CATEGORIES_PROPERTY);
	sArray->cValues = 0;
	while(categoriesProp){
	
		value = strdup(icalproperty_get_categories(categoriesProp));
		tok = strtok(value, ",");
		while(tok){
			if(!stringArray){
				stringArray = talloc_array(ical2exchange->mem_ctx, char *, 1);
			} else {
				stringArray = talloc_realloc(ical2exchange->mem_ctx, stringArray, char *, sArray->cValues+2);
			}
			strcpy(string, "");
			while(tok[i]){
				if (strlen(string) == 255) break;
				//remove beginning and ending white spaces
				if((tok[i]!= ' ' || (tok[i+1] && tok[i+1] != ' ')) && (strlen(string) || tok[i]!=' ')){
					strncat(string, &tok[i], 1);
				}
				i++;
			}
			stringArray[sArray->cValues] = talloc_strdup(ical2exchange->mem_ctx, string);
			sArray->cValues++;
			i=0;
			tok= strtok(NULL, ",");
		}
		categoriesProp = icalcomponent_get_next_property(ical2exchange->categoriesEvent, ICAL_CATEGORIES_PROPERTY);

	}
	sArray->lppszA= (uint8_t **) stringArray;
	
 	/* SetProps */
 	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidNameKeywords, 
 					(const void *) sArray);
}

void ical2exchange_property_CLASS(struct ical2exchange *ical2exchange)
{
	enum icalproperty_class class;
	uint32_t	temp = 0;
	uint32_t	*tag;
	
	/*sanity check*/
	if(!ical2exchange->classProp) return;
	
	class = icalproperty_get_class(ical2exchange->classProp); 
	
	switch(class){
		case ICAL_CLASS_PUBLIC:
			temp = 0x00000000;
			break;
		case ICAL_CLASS_X:
			temp = 0x00000001;
			break;
		case ICAL_CLASS_PRIVATE:
			temp = 0x00000002;
			break;
		case ICAL_CLASS_CONFIDENTIAL:
			temp = 0x00000003;
			break;
		case ICAL_CLASS_NONE:
			return;
	}
	
	tag = talloc(ical2exchange->mem_ctx, uint32_t);
	*tag = temp;
	
	/* SetProps */
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PR_SENSITIVITY, 
					(const void *) tag);
}

void ical2exchange_property_COMMENT(struct ical2exchange *ical2exchange)
{
	const char *comment;

	/*sanity check*/
	if(!ical2exchange->commentProp) return;
	if(ical2exchange->method != ICAL_METHOD_COUNTER && ical2exchange->method != ICAL_METHOD_REPLY) return;
	
	comment  = icalproperty_get_comment(ical2exchange->commentProp);
	comment = talloc_strdup(ical2exchange->mem_ctx, comment);
	
	/* SetProps */
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PR_BODY,
			(const void *) comment);
}

void ical2exchange_property_CONTACT(struct ical2exchange *ical2exchange)
{
	struct StringArray_r *sArray;
 	char *value;
	char **stringArray = NULL;
	icalproperty *contactProp;
	
	/*sanity check*/
	if(!ical2exchange->contactEvent) return;
	

	contactProp=icalcomponent_get_first_property(ical2exchange->contactEvent, ICAL_CONTACT_PROPERTY);
	sArray = talloc(ical2exchange->mem_ctx, struct StringArray_r);	
	sArray->cValues =0;
	while(contactProp){
		if(!stringArray){
			stringArray = talloc_array(ical2exchange->mem_ctx, char *, 1);
		} else {
			stringArray = talloc_realloc(ical2exchange->mem_ctx, stringArray, char *, sArray->cValues+2);
		}
		value = strdup(icalproperty_get_contact(contactProp));
 		if(strlen(value)<500){
 			stringArray[sArray->cValues] = talloc_strdup(ical2exchange->mem_ctx, value);
 		} else {
			value[499] = '\0';
 			stringArray[sArray->cValues] = talloc_strndup(ical2exchange->mem_ctx, value, 500);
		}
 		sArray->cValues++;
		contactProp=icalcomponent_get_next_property(ical2exchange->contactEvent, ICAL_CONTACT_PROPERTY);
	}
		
	/*set up struct*/
	sArray->lppszA=(uint8_t **) stringArray;

	/* SetProps */
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidContacts, 
					(const void *) sArray);

}

void ical2exchange_property_DESCRIPTION(struct ical2exchange *ical2exchange)
{
	const char *description;
	
	/*sanity check*/
	if(!ical2exchange->descriptionProp) return;
	
	if(ical2exchange->method == ICAL_METHOD_COUNTER || ical2exchange->method == ICAL_METHOD_REPLY) return;
	
	description  = icalproperty_get_description(ical2exchange->descriptionProp);
	description = talloc_strdup(ical2exchange->mem_ctx, description);
	
	/* SetProps */
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PR_BODY,
			(const void *) description);	
}

void ical2exchange_property_DTSTAMP(struct ical2exchange *ical2exchange)
{
	struct FILETIME *ft;
	icaltimetype dtstamp;
	
	/*sanity check*/
	if(!ical2exchange->dtstampProp) return;
		
	dtstamp  = icalproperty_get_dtstamp(ical2exchange->dtstampProp);
	ft = talloc(ical2exchange->mem_ctx, struct FILETIME);
	*ft = get_FILETIME_from_icaltimetype(&dtstamp);
	
	if(ical2exchange->method == ICAL_METHOD_COUNTER || ical2exchange->method == ICAL_METHOD_REPLY){
		ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAttendeeCriticalChange,
			       (const void *) ft);
	} else {
		ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidOwnerCriticalChange,
			       (const void *) ft);
	}
}

void ical2exchange_property_DTSTART_DTEND(struct ical2exchange *ical2exchange)
{
	icaltimetype dtstart;
	icaltimetype dtend;
	struct FILETIME *startft; 
	struct FILETIME *endft; 
	double difference;
	uint32_t *duration; 
	
	/*sanity check*/
	if(!ical2exchange->dtstartProp) return;
	if(!ical2exchange->dtendProp) return;

	/*dtstart property*/
	dtstart  = icalproperty_get_dtstart(ical2exchange->dtstartProp);
	startft = talloc(ical2exchange->mem_ctx, struct FILETIME);
	*startft = get_FILETIME_from_icaltimetype(&dtstart);
	
	/*dtend property*/
	dtend  = icalproperty_get_dtend(ical2exchange->dtendProp);
	endft = talloc(ical2exchange->mem_ctx, struct FILETIME);
	*endft = get_FILETIME_from_icaltimetype(&dtend);
	
	if(ical2exchange->method == ICAL_METHOD_COUNTER){
		ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentProposedStartWhole,
			       (const void *) startft);
		ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentProposedEndWhole,
			       (const void *) endft);
	} else {
		ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentStartWhole,
			       (const void *) startft);
		ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentEndWhole,
			       (const void *) endft);
			       
		/*duration property*/
		duration = talloc(ical2exchange->mem_ctx, uint32_t);
		difference =  difftime(nt_time_to_unix(FILETIME_to_NTTIME(*endft)), 
				       nt_time_to_unix(FILETIME_to_NTTIME(*startft)));  
		*duration = difference/60;
		ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentDuration,
				(const void *) duration);
		/*check if all day appointment*/
		if(*duration==1440 && dtstart.hour==0 && dtstart.minute==0){
			uint32_t *allday = talloc(ical2exchange->mem_ctx, uint32_t);
			*allday = 0x00000001;
			ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentSubType,
					(const void *) allday);
		}
	}
}

void ical2exchange_property_LOCATION(struct ical2exchange *ical2exchange)
{
	uint32_t *langtag;
	char location[256] = "";
	icalparameter *param = NULL;
	const char *value;
	char *string;
	
	/*sanity check*/
	if(!ical2exchange->locationProp) return;
		
	value  = icalproperty_get_location(ical2exchange->locationProp);
	int i;
	
	for(i=0; i<255; i++){
		if(!value[i]) break;
		char c = value[i];
		if(c !='\xD' && c!='\xA' )
			strncat(location, &c, 1);
	}
	string = talloc_strdup(ical2exchange->mem_ctx, location);

	/* SetProps */
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidLocation, 
			(const void *) string);
			
	if((param=icalproperty_get_first_parameter(ical2exchange->locationProp, ICAL_LANGUAGE_PARAMETER))){
		const char* langName;
		langtag = talloc(ical2exchange->mem_ctx, uint32_t);
		langName = icalparameter_get_language(param);
		*langtag = mapi_get_lcid_from_language(langName);
		ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PR_MESSAGE_LOCALE_ID, 
					(const void *) langtag);
	} 

}

void ical2exchange_property_PRIORITY(struct ical2exchange *ical2exchange)
{
	uint32_t temp = 0;
	uint32_t *tag;
	
	if(ical2exchange->x_importanceProp){
		const char *value = icalproperty_get_x(ical2exchange->x_importanceProp);
		switch(atoi(value)){
			case 0:
				temp = 0x00000000;
				break;
			case 1:
				temp = 0x00000001;
				break;
			case 2:
				temp = 0x00000002;
				break;
		}
	} else if(ical2exchange->priorityProp){
		switch(icalproperty_get_priority(ical2exchange->priorityProp)){
			case 0:
				return;
			case 1:
			case 2:
			case 3:
			case 4:
				temp = 0x00000002;
				break;
			case 5:
				temp = 0x00000001;
				break;
			case 6:
			case 7:
			case 8:
			case 9:
				temp = 0x00000000;
				break;
			default:
				return;
		}
	} else return;
	
	tag = talloc(ical2exchange->mem_ctx, uint32_t);
	*tag = temp;
	
	/* SetProps */
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentSequence, 
					(const void *) tag);	
}

//TODO: Finish this
void ical2exchange_property_RRULE_EXDATE_RDATE(struct ical2exchange *ical2exchange)
{
	struct RecurrencePattern	rp;
	struct icalrecurrencetype 	irt;
	struct icaltimetype 		next;
	struct icaltimetype 		last;
	icalrecur_iterator 		*ritr;
	icaltimetype 			dtstart;
	icaltimetype 			rdate;
	icaltimetype 			exdate;
	icalproperty 			*exdateProp;
	icalproperty 			*rdateProp;
	enum CalendarType 		calendarType;
	enum EndType 			endType;
	enum icalrecurrencetype_weekday	weekday; 
	uint32_t 			startTime;
	uint32_t 			endTime;
	uint32_t 			occurrenceCount;
	uint32_t			i;
	uint32_t 			modifiedInstanceDates[ical2exchange->rdateCount];
	uint32_t 			deletedInstanceDates[ical2exchange->exdateCount+ical2exchange->rdateCount];
	
	if(!ical2exchange->rruleProp) return;
	if(!ical2exchange->dtstartProp) return;
	
	irt =  icalproperty_get_rrule(ical2exchange->rruleProp);

	/*StartTime*/
	enum icalrecurrencetype_weekday icalrecurrencetype_day_day_of_week(short day);
	dtstart  = icalproperty_get_dtstart(ical2exchange->dtstartProp);
	dtstart.hour = 0;
	dtstart.minute = 0;
	dtstart.second = 0;
	startTime = get_minutes_from_icaltimetype(dtstart);
	
	/*WeekDay*/
	if(irt.week_start == ICAL_NO_WEEKDAY){
		weekday = icalrecurrencetype_day_day_of_week(icaltime_day_of_week(dtstart));
	} else {
		weekday = irt.week_start;
	}
	
	/*calendarType*/
	//if(ical2exchange->calscaleProp){
		//calendarType = get_exchange_calendartype(icalproperty_get_x(ical2exchange->calscaleProp));
		
	//} else {
		calendarType = CAL_DEFAULT;
	//}
	
	/*endType occurrenceCount endTime*/
	if(irt.count || !icaltime_is_null_time(irt.until)){
		endType	= END_AFTER_N_OCCURRENCES;
		occurrenceCount = irt.count;
		
		ritr = icalrecur_iterator_new(irt,dtstart);
		next=icalrecur_iterator_next(ritr);
		
		while (!icaltime_is_null_time(next)){
			last = next;
			next=icalrecur_iterator_next(ritr);
			
			if(!irt.count) occurrenceCount++;
		}
		
		endTime	= get_minutes_from_icaltimetype(last); 
		icalrecur_iterator_free(ritr);
		
	} else {
		endType	= END_NEVER_END;
		occurrenceCount = 0x0000000A;
		endTime		= 0x5AE980DF;
	}
	
	/*Common values for all rrule*/
	rp.ReaderVersion 	= 0x3004;
	rp.WriterVersion 	= 0x3004;
	rp.CalendarType		= calendarType;
	rp.FirstDateTime	= startTime;
	rp.SlidingFlag		= 0x00000000;
	rp.EndType		= endType;
	rp.OccurrenceCount	= occurrenceCount;
	rp.FirstDOW		= get_exchange_day_from_ical(weekday);	
	rp.StartDate		= startTime;
	rp.EndDate		= endTime;
	
	/*Specific values*/
	switch(irt.freq){
		case ICAL_DAILY_RECURRENCE:
			rp.RecurFrequency 	= RecurFrequency_Daily;
			rp.PatternType		= PatternType_Day;
			rp.Period		= 1440 * irt.interval;
			break;
		case ICAL_WEEKLY_RECURRENCE:
			rp.RecurFrequency 	= RecurFrequency_Weekly;
			rp.PatternType		= PatternType_Day;
			rp.Period		= irt.interval;
			break;
		case ICAL_MONTHLY_RECURRENCE:
			rp.RecurFrequency 	= RecurFrequency_Monthly;
			rp.Period		= irt.interval;
			
			if(irt.by_day[0] == ICAL_RECURRENCE_ARRAY_MAX){
				rp.PatternType	= PatternType_Month;
				
				if(irt.by_month_day[0] == -1){
					rp.PatternTypeSpecific.Day = 0x0000001F;
				} else {
					rp.PatternTypeSpecific.Day = irt.by_month_day[0];
				}
				
			} else {
				rp.PatternType	= PatternType_MonthNth;
				i = 0;
				rp.PatternTypeSpecific.MonthRecurrencePattern.WeekRecurrencePattern = 0;
				while( irt.by_day[i] != ICAL_RECURRENCE_ARRAY_MAX){
					rp.PatternTypeSpecific.MonthRecurrencePattern.WeekRecurrencePattern  |= get_exchange_rdfDays_from_ical(irt.by_day[i]);
					i++;
				}
				if(irt.by_set_pos[0] == -1) rp.PatternTypeSpecific.MonthRecurrencePattern.N = RecurrenceN_Last;
				else rp.PatternTypeSpecific.MonthRecurrencePattern.N = irt.by_set_pos[0];
			}
			break;
		case ICAL_YEARLY_RECURRENCE:
			rp.RecurFrequency 	= RecurFrequency_Yearly;
			rp.Period		= 12 * irt.interval;
			
			/*Nth*/
			if(irt.by_day[0] == ICAL_RECURRENCE_ARRAY_MAX){
				rp.PatternType	= PatternType_Month;
				if(irt.by_month_day[0] == -1){
					rp.PatternTypeSpecific.Day = 0x0000001F;
				} else {
					rp.PatternTypeSpecific.Day = irt.by_month_day[0];
				}
			} else {
				rp.PatternType	= PatternType_MonthNth;
				i = 0;
				rp.PatternTypeSpecific.MonthRecurrencePattern.WeekRecurrencePattern = 0;
				while( irt.by_day[i] != ICAL_RECURRENCE_ARRAY_MAX){
					rp.PatternTypeSpecific.MonthRecurrencePattern.WeekRecurrencePattern  |= get_exchange_rdfDays_from_ical(irt.by_day[i]);
					i++;
				}
				if(irt.by_set_pos[0] == -1) rp.PatternTypeSpecific.MonthRecurrencePattern.N = RecurrenceN_Last;
				else rp.PatternTypeSpecific.MonthRecurrencePattern.N = irt.by_set_pos[0];
			}
			break;
		default:
			printf("not handled yet\n");
			
	}
	

	/*deletedInstanceDates & modifiedInstanceDates*/
	if(ical2exchange->exdateEvent){
		exdateProp=icalcomponent_get_first_property(ical2exchange->exdateEvent, ICAL_EXDATE_PROPERTY);

		for(i=0; i<ical2exchange->exdateCount; i++){
			exdate = icalproperty_get_exdate(exdateProp);
			deletedInstanceDates[i] = get_minutes_from_icaltimetype(exdate);
			exdateProp=icalcomponent_get_first_property(ical2exchange->exdateEvent, ICAL_EXDATE_PROPERTY);
		}
	}
	
	if(ical2exchange->rdateEvent){
		rdateProp = icalcomponent_get_first_property(ical2exchange->rdateEvent, ICAL_EXDATE_PROPERTY);

		for(i=0; i<ical2exchange->rdateCount; i++){
			rdate = icalproperty_get_exdate(rdateProp);
			deletedInstanceDates[i + ical2exchange->exdateCount] = get_minutes_from_icaltimetype(rdate);
			modifiedInstanceDates[i] = get_minutes_from_icaltimetype(rdate);
			rdateProp = icalcomponent_get_first_property(ical2exchange->rdateEvent, ICAL_EXDATE_PROPERTY);
		}
	}
	/*Sort array*/
	qsort(deletedInstanceDates, ical2exchange->rdateCount + ical2exchange->exdateCount, sizeof(uint32_t), compare_minutes);
	qsort(modifiedInstanceDates, ical2exchange->rdateCount, sizeof(uint32_t), compare_minutes);
	
	rp.DeletedInstanceCount = ical2exchange->rdateCount + ical2exchange->exdateCount;
	rp.ModifiedInstanceCount = ical2exchange->rdateCount;
	
	rp.DeletedInstanceDates = deletedInstanceDates;
	rp.ModifiedInstanceDates = modifiedInstanceDates;
	
	
}

void ical2exchange_property_SEQUENCE(struct ical2exchange *ical2exchange)
{
	uint32_t temp;
	uint32_t *tag;
	
	if(ical2exchange->x_sequenceProp){
		temp = atoi(icalproperty_get_x(ical2exchange->x_sequenceProp));
	} else if(ical2exchange->sequenceProp){
		temp = icalproperty_get_sequence(ical2exchange->sequenceProp);
	} else return;
	
	tag = talloc(ical2exchange->mem_ctx, uint32_t);
	*tag = temp;
	
	/* SetProps */
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentSequence, 
					(const void *) tag);	
}

void ical2exchange_property_STATUS(struct ical2exchange *ical2exchange)
{
	
	enum icalproperty_status status;
	enum icalproperty_transp transp;
	uint32_t *tag;
	uint32_t temp = 0;
	const char *prop;
	
	if(ical2exchange->x_busystatusProp){
		prop=icalproperty_get_x(ical2exchange->x_busystatusProp);

		if(!strcmp(prop, "FREE")){
			temp = 0x00000000;
		} else if(!strcmp(prop, "TENTATIVE")){
			temp = 0x00000001;
		} else if(!strcmp(prop, "BUSY")){
			temp = 0x00000002;
		} else if(!strcmp(prop, "OOF")){
			temp = 0x00000003;
		}
		
	} else if(ical2exchange->transpProp){
		transp = icalproperty_get_transp(ical2exchange->transpProp); 
		switch(transp){
			case ICAL_TRANSP_TRANSPARENT:
				temp = 0x00000000;
				break;
			case ICAL_TRANSP_OPAQUE:
				temp = 0x00000002;
				break;
			default:
				return;
		}
		
	} else if(ical2exchange->statusProp){
		status = icalproperty_get_status(ical2exchange->classProp); 
		switch(status){
			case ICAL_STATUS_CANCELLED:
				temp = 0x00000000;
				break;
			case ICAL_STATUS_TENTATIVE:
				temp = 0x00000001;
				break;
			case ICAL_STATUS_CONFIRMED:
				temp = 0x00000002;
				break;
			default:
				return;
		}
	} else return;
	
	tag = talloc(ical2exchange->mem_ctx, uint32_t);
	*tag = temp;
	
	/* SetProps */
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidBusyStatus, 
					(const void *) tag);	
}

void ical2exchange_property_SUMMARY(struct ical2exchange *ical2exchange)
{
	const char *value;
	uint32_t langtag;
	char summary[256] = "";
	icalparameter *param = NULL;
	char *string;
	
	/*sanity check*/
	if(!ical2exchange->summaryProp) return;
	
	value  = icalproperty_get_summary(ical2exchange->summaryProp);
	
	int i;
	
	for(i=0; i<255; i++){
		if(!value[i]) break;
		char c = value[i];
		if(c !='\xD' && c!='\xA' )
			strncat(summary, &c, 1);
	}
	string = talloc_strdup(ical2exchange->mem_ctx, summary);
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PR_SUBJECT, 
			(const void *) string);
	if((param=icalproperty_get_first_parameter(ical2exchange->summaryProp, ICAL_LANGUAGE_PARAMETER))){
		const char *langName;
		langName = icalparameter_get_language(param);
		langtag = mapi_get_lcid_from_language(langName);
		ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PR_MESSAGE_LOCALE_ID, 
						(const void *) &langtag);
	}
}

void ical2exchange_property_VALARM(struct ical2exchange *ical2exchange)
{
	icalproperty *triggerProp = NULL;
	uint32_t *duration;
	icaltimetype dtstart;
	icaltimetype current;
	icaltimetype triggerSet;
	icaltimetype next;
	
	const icaltimezone *timezone; 
	struct FILETIME *rtimeft;
	struct FILETIME *rsignalft;
	struct icalrecurrencetype 	irt;
	icalrecur_iterator 		*ritr;
	struct icaltriggertype trigger;
	bool *set;
	

	/*sanity check*/
	if(!ical2exchange->valarmEvent) return;
	if(!ical2exchange->dtstartProp) return;

	triggerProp = icalcomponent_get_first_property(ical2exchange->valarmEvent, ICAL_TRIGGER_PROPERTY);
	if(!triggerProp) return;
	
	trigger = icalproperty_get_trigger(triggerProp);
	dtstart  = icalproperty_get_dtstart(ical2exchange->dtstartProp);
	duration = talloc(ical2exchange->mem_ctx, uint32_t);
	
	if(trigger.duration.is_neg){
		*duration = -trigger.duration.minutes;
	} else {
		*duration = trigger.duration.minutes;
	}

	timezone = icaltime_get_timezone(dtstart);
	current = icaltime_current_time_with_zone(timezone);
	
	if(icaltime_compare(dtstart, current)) {
		rtimeft = talloc(ical2exchange->mem_ctx, struct FILETIME);
		rsignalft = talloc(ical2exchange->mem_ctx, struct FILETIME);

		*rtimeft = get_FILETIME_from_icaltimetype(&dtstart);
		ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidReminderTime, 
						(const void *) rtimeft);
			
		triggerSet = icaltime_add(dtstart, trigger.duration);
		*rsignalft = get_FILETIME_from_icaltimetype(&triggerSet);
		ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidReminderSignalTime, 
						(const void *) rsignalft);
	} else if(ical2exchange->rruleProp) {
		irt =  icalproperty_get_rrule(ical2exchange->rruleProp);
		ritr = icalrecur_iterator_new(irt,dtstart);
		next = icalrecur_iterator_next(ritr);
		
		while (!icaltime_is_null_time(next)){
			if(icaltime_compare(next, current)) {
				rtimeft = talloc(ical2exchange->mem_ctx, struct FILETIME);
				rsignalft = talloc(ical2exchange->mem_ctx, struct FILETIME);
				
				*rtimeft = get_FILETIME_from_icaltimetype(&next);
				ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidReminderTime, 
					(const void *) rtimeft);
					
				triggerSet = icaltime_add(next, trigger.duration);
				*rsignalft = get_FILETIME_from_icaltimetype(&triggerSet);
				ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidReminderSignalTime, 
					(const void *) &rsignalft);
				break;
			}
			next=icalrecur_iterator_next(ritr);
		}
	} else return;
	
	set = talloc(ical2exchange->mem_ctx, bool);
	*set = true;
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidReminderSet, 
					(const void *) set);
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidReminderDelta, 
					(const void *) duration);
}

void ical2exchange_property_X_ALLOWEXTERNCHECK(struct ical2exchange *ical2exchange)
{
	const char *prop;
	bool *allow;
	
	if(!ical2exchange->x_allowexterncheckProp) return;
	
	prop=icalproperty_get_x(ical2exchange->x_allowexterncheckProp);
	allow = talloc(ical2exchange->mem_ctx, bool);
	
	if(strcmp(prop, "TRUE")){
		*allow = true;
	} else {
		*allow = false;
	}
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAllowExternalCheck, 
					(const void *) allow);
}

void ical2exchange_property_X_APPTSEQTIME(struct ical2exchange *ical2exchange)
{
	const char *prop;
	struct FILETIME *ft;
	
	if(!ical2exchange->x_apptseqtimeProp) return;
	
	ft = talloc(ical2exchange->mem_ctx, struct FILETIME);
	
	prop=icalproperty_get_x(ical2exchange->x_apptseqtimeProp);
	*ft = get_FILETIME_from_string(prop);

	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentSequenceTime, 
					(const void *) ft);
}

void ical2exchange_property_X_APPTLASTSEQUENCE(struct ical2exchange *ical2exchange)
{
	uint32_t *tag;
	const char *prop;
	
	if(!ical2exchange->x_apptlastsequenceProp) return;

	prop=icalproperty_get_x(ical2exchange->x_apptlastsequenceProp);
	tag = talloc(ical2exchange->mem_ctx, uint32_t);
	*tag=atoi(prop);
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentLastSequence, 
					(const void *) tag);
}

void ical2exchange_property_X_ATTENDEE_CRITICAL_CHANGE(struct ical2exchange *ical2exchange)
{
	const char *prop;
	struct FILETIME *ft;
	
	if(!ical2exchange->x_attendeecriticalchangeProp) return;
	
	prop=icalproperty_get_x(ical2exchange->x_attendeecriticalchangeProp);
	ft = talloc(ical2exchange->mem_ctx, struct FILETIME);
	*ft = get_FILETIME_from_string(prop);

	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAttendeeCriticalChange, 
					(const void *) ft);
}

void ical2exchange_property_X_AUTOFILLLOCATION(struct ical2exchange *ical2exchange)
{
	const char *prop;
	bool *autoFill;
	
	if(!ical2exchange->x_autofilllocationProp) return;
	
	prop=icalproperty_get_x(ical2exchange->x_autofilllocationProp);
	autoFill = talloc(ical2exchange->mem_ctx, bool);
	
	if(strcmp(prop, "TRUE")){
		*autoFill = true;
	} else {
		*autoFill = false;
	}
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAutoFillLocation, 
					(const void *) autoFill);
}

void ical2exchange_property_X_AUTOSTARTCHECK(struct ical2exchange *ical2exchange)
{
	const char *prop;
	bool *autoStart;
	
	if(!ical2exchange->x_autostartcheckProp) return;
	
	prop=icalproperty_get_x(ical2exchange->x_autostartcheckProp);
	autoStart = talloc(ical2exchange->mem_ctx, bool);	
	
	if(strcmp(prop, "TRUE")){
		*autoStart = true;
	} else {
		*autoStart = false;
	}
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAutoStartCheck, 
					(const void *) autoStart);
}

void ical2exchange_property_X_COLLABORATEDDOC(struct ical2exchange *ical2exchange)
{
	const char *prop;
	
	if(!ical2exchange->x_collaborateddocProp) return;
	
	prop=icalproperty_get_x(ical2exchange->x_collaborateddocProp);		
	
	prop = talloc_strdup(ical2exchange->mem_ctx, prop);
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidCollaborateDoc, 
					(const void *) prop);
}

void ical2exchange_property_X_CONFCHECK(struct ical2exchange *ical2exchange)
{
	const char *prop;
	bool *confCheck;
	
	if(!ical2exchange->x_confcheckProp) return;
	
	prop=icalproperty_get_x(ical2exchange->x_confcheckProp);
	confCheck = talloc(ical2exchange->mem_ctx, bool);
	
	if(strcmp(prop, "TRUE")){
		*confCheck = true;
	} else {
		*confCheck = false;
	}
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidConferencingCheck, 
					(const void *) confCheck);
}

void ical2exchange_property_X_CONFTYPE(struct ical2exchange *ical2exchange)
{
	uint32_t *tag;
	const char *prop;
	
	if(!ical2exchange->x_conftypeProp) return;
	
	prop=icalproperty_get_x(ical2exchange->x_conftypeProp);
	tag = talloc(ical2exchange->mem_ctx, uint32_t);
	*tag=atoi(prop);
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidConferencingType, 
					(const void *) tag);
}

void ical2exchange_property_X_DISALLOW_COUNTER(struct ical2exchange *ical2exchange)
{
	const char *prop;
	bool *disallow;
	
	if(!ical2exchange->x_disallowcounterProp) return;
	
	prop=icalproperty_get_x(ical2exchange->x_disallowcounterProp);
	disallow = talloc(ical2exchange->mem_ctx, bool);
	
	if(strcmp(prop, "TRUE")){
		*disallow = true;
	} else {
		*disallow = false;
	}
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentNotAllowPropose, 
					(const void *) disallow);
}

void ical2exchange_property_X_INTENDEDSTATUS(struct ical2exchange *ical2exchange)
{
	uint32_t *tag;
	const char *prop;
	
	if(!ical2exchange->x_intendedProp) return;
	
	prop=icalproperty_get_x(ical2exchange->x_intendedProp);
	tag = talloc(ical2exchange->mem_ctx,uint32_t);
	
	if(!strcmp(prop, "FREE")){
		*tag = 0x00000000;
	} else if(!strcmp(prop, "TENTATIVE")){
		*tag = 0x00000001;
	} else if(!strcmp(prop, "BUSY")){
		*tag = 0x00000002;
	} else if(!strcmp(prop, "OOF")){
		*tag = 0x00000003;
	}
	
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidIntendedBusyStatus, 
					(const void *) tag);
}

void ical2exchange_property_X_ISDRAFT(struct ical2exchange *ical2exchange)
{
	const char *prop;
	bool *isDraft;
	
	isDraft = talloc(ical2exchange->mem_ctx, bool);
	
	if(ical2exchange->method == ICAL_METHOD_COUNTER || ical2exchange->method == ICAL_METHOD_REPLY 
		|| ical2exchange->method == ICAL_METHOD_REQUEST || ical2exchange->method == ICAL_METHOD_CANCEL){
		
		*isDraft = true;
	} else {
		if(ical2exchange->x_isdraftProp){
			prop=icalproperty_get_x(ical2exchange->x_isdraftProp);		
			if(strcmp(prop, "TRUE")){
				*isDraft = true;
			} else {
				*isDraft = false;
			}
		} else {
			*isDraft = false;
		}
	}
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidFInvited, 
					(const void *) isDraft);
}

void ical2exchange_property_X_MWSURL(struct ical2exchange *ical2exchange)
{
	char *mwsurl;
	const char *prop;
	
	if(!ical2exchange->x_mwsurlProp) return;
	
	prop = icalproperty_get_x(ical2exchange->x_mwsurlProp);		
	mwsurl = talloc_strdup(ical2exchange->mem_ctx, prop);
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidMeetingWorkspaceUrl, 
					(const void *) mwsurl);
}

void ical2exchange_property_X_NETSHOWURL(struct ical2exchange *ical2exchange)
{
	char *netshow;
	const char *prop;
	
	if(!ical2exchange->x_netshowurlProp) return;
	
	prop = icalproperty_get_x(ical2exchange->x_netshowurlProp);		
	netshow = talloc_strdup(ical2exchange->mem_ctx, prop);

	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidNetShowUrl, 
					(const void *) netshow);
}

void ical2exchange_property_X_ONLINEPASSWORD(struct ical2exchange *ical2exchange)
{
	char *onlinepass;
	const char *prop;
	
	if(!ical2exchange->x_onlinepasswordProp) return;
	
	prop = icalproperty_get_x(ical2exchange->x_onlinepasswordProp);		
	onlinepass = talloc_strdup(ical2exchange->mem_ctx, prop);

	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidOnlinePassword, 
					(const void *) onlinepass);
}

void ical2exchange_property_X_ORGALIAS(struct ical2exchange *ical2exchange)
{
	char *orgalias;
	const char *prop;
	
	if(!ical2exchange->x_orgaliasProp) return;
	
	prop = icalproperty_get_x(ical2exchange->x_orgaliasProp);		
	orgalias = talloc_strdup(ical2exchange->mem_ctx, prop);
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidOrganizerAlias, 
					(const void *) orgalias);
}

void ical2exchange_property_X_ORIGINALEND_ORIGINALSTART(struct ical2exchange *ical2exchange)
{
	const char *start;
	const char *end;
	double difference;
	uint32_t duration;
	struct FILETIME startft;
	struct FILETIME endft;
	
	/*sanity check*/
	if(!ical2exchange->x_originalendProp) return;
	if(!ical2exchange->x_originalstartProp) return;
	if(ical2exchange->method != ICAL_METHOD_COUNTER) return;
	
	start = icalproperty_get_x(ical2exchange->x_originalstartProp);
	end = icalproperty_get_x(ical2exchange->x_originalendProp);

	startft = get_FILETIME_from_string(start);
	endft = get_FILETIME_from_string(end);
	
	/*duration property*/
	difference =  difftime(nt_time_to_unix(FILETIME_to_NTTIME(endft)), 
				     nt_time_to_unix(FILETIME_to_NTTIME(startft)));
	duration = difference/60;
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentStartWhole, 
			(const void *) &startft);
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentEndWhole, 
			(const void *) &endft);
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentDuration, 
			(const void *) &duration);
			
}

void ical2exchange_property_X_OWNER_CRITICAL_CHANGE(struct ical2exchange *ical2exchange)
{
	const char *prop;
	struct FILETIME ft;
	
	if(!ical2exchange->x_ownercriticalchangeProp) return;
	
	prop=icalproperty_get_x(ical2exchange->x_ownercriticalchangeProp);
	ft = get_FILETIME_from_string(prop);

	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidOwnerCriticalChange, 
					(const void *) &ft);
}

void ical2exchange_property_X_OWNERAPPTID(struct ical2exchange *ical2exchange)
{
	uint32_t *tag;
	const char *prop;
	
	if(!ical2exchange->x_ownerapptidProp) return;
	
	prop=icalproperty_get_x(ical2exchange->x_ownerapptidProp);
	
	tag = talloc(ical2exchange->mem_ctx, uint32_t);
	*tag = atoi(prop);
	
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PR_OWNER_APPT_ID, 
					(const void *) tag);
}

void ical2exchange_property_X_REPLYTIME(struct ical2exchange *ical2exchange)
{
	const char *prop;
	struct FILETIME *ft;
	
	if(!ical2exchange->x_replytimeProp) return;
	
	prop=icalproperty_get_x(ical2exchange->x_replytimeProp);
	ft = talloc(ical2exchange->mem_ctx, struct FILETIME);
	*ft = get_FILETIME_from_string(prop);

	/* SetProps */
	ical2exchange->lpProps = add_SPropValue(ical2exchange->mem_ctx, ical2exchange->lpProps, &ical2exchange->cValues, PidLidAppointmentReplyTime, 
					(const void *) ft);
}
