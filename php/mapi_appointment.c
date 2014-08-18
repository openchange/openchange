/*
   OpenChange MAPI PHP bindings

   Copyright (C) 2013 Javier Amor Garcia.

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

#include <php_mapi.h>
#define SEC_TO_UNIX_EPOCH 11644473600LL
#define MIN_TO_UNIX_EPOCH 194074560LL
static zend_function_entry mapi_appointment_class_functions[] = {
	PHP_ME(MAPIAppointment,	__construct,	NULL,	ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(MAPIAppointment,	__destruct,	NULL,	ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_ME(MAPIAppointment, setRecurrence,	NULL,	ZEND_ACC_PUBLIC)
	PHP_ME(MAPIAppointment, getRecurrence,	NULL,	ZEND_ACC_PUBLIC)

	{ NULL, NULL, NULL }
};

static zend_class_entry		*mapi_appointment_ce;
static zend_object_handlers	mapi_appointment_object_handlers;

void MAPIAppointmentRegisterClass(TSRMLS_D)
{
	zend_class_entry	ce;

	INIT_CLASS_ENTRY(ce, "MAPIAppointment", mapi_appointment_class_functions);
	mapi_appointment_ce = zend_register_internal_class_ex(&ce, NULL, "mapimessage" TSRMLS_CC);

	mapi_appointment_ce->create_object = mapi_message_create_handler;

	memcpy(&mapi_appointment_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	mapi_appointment_object_handlers.clone_obj = NULL;
}

zval *create_appointment_object(zval *folder, mapi_object_t *message, char open_mode TSRMLS_DC)
{
	zval 			*appointment;
	mapi_message_object_t 	*obj;
	enum MAPISTATUS		retval;
	struct SPropTagArray	*SPropTagArray;

	appointment =  create_message_object("mapiappointment", folder, message, open_mode TSRMLS_CC);
	mapi_message_request_all_properties(appointment TSRMLS_CC);

	return appointment;
}

int hash_find_long(HashTable *ht, char *key, long *foundValue)
{
	int found;
	long **value;
	found = zend_hash_find(ht, key, strlen(key)+1,  (void**) &value);
	if (found == SUCCESS) {
		long val = **value;
		*foundValue = val;
	}

	return found;
}

long mandatory_long_from_hash(HashTable *ht, char *key)
{
	long lvalue;
	if (hash_find_long(ht, key, &lvalue) == SUCCESS) {
		return lvalue;
	} else {
		php_error(E_ERROR, "Missing mandatory key %s\n", key);
	}
}




PHP_METHOD(MAPIAppointment, __construct)
{
	php_error(E_ERROR, "The appointment object should not created directly.");
}


PHP_METHOD(MAPIAppointment, __destruct)
{
	/* zval			*php_this_obj; */
	/* mapi_appointment_object_t	*this_obj; */
	/* php_this_obj = getThis(); */
	/* this_obj = (mapi_appointment_object_t *) zend_object_store_get_object(php_this_obj TSRMLS_CC); */

}




void set_recurrence_pattern(struct RecurrencePattern *rPattern, zval *to_set)
{
	HashTable			*ht;
	uint32_t 			startTime;
	uint32_t 			endTime;
	uint32_t 			modifiedInstanceDates[0];
	uint32_t 			deletedInstanceDates[0];
	uint32_t                        recurFrequency;
	uint32_t                        patternType;

	ht = to_set->value.ht;

	rPattern->ReaderVersion 	= 0x3004;
	rPattern->WriterVersion 	= 0x3004;

	rPattern->RecurFrequency  = mandatory_long_from_hash(ht, "RecurFrequency");
	switch  (rPattern->RecurFrequency) {
	case 0x200A:
	case 0x200B:
	case 0x200C:
	case 0x200D:
		// correct values, continue
		break;
	default:
		php_error(E_ERROR, "Invalid RecurFrequency: %i", rPattern->RecurFrequency);
	}

	rPattern->PatternType	  = mandatory_long_from_hash(ht, "PatternType");
	switch  (rPattern->PatternType) {
	case 0x0:
	case 0x1:
	case 0x2:
        case 0x4:
		// correct and suported values, continue
		break;
	case 0xA:
	case 0xB:
	case 0xC:
		php_error(E_ERROR, "PatternType: Hijr calendar not supported yet");
	default:
		php_error(E_ERROR, "Invalid PatternType: %i", rPattern->PatternType);
	}


	rPattern->CalendarType	  = 0x0000; // GREGORIAN

	rPattern->FirstDateTime	= mandatory_long_from_hash(ht, "FirstDateTime");
	if (rPattern->FirstDateTime < 0) {
		php_error(E_ERROR, "FistDateTime must be a positive number");
	}

	rPattern->Period	= mandatory_long_from_hash(ht, "Period"); //weeks THIS chnge!
	if (rPattern->Period < 0) {
		php_error(E_ERROR, "Period must be a positive number");
	}

	rPattern->SlidingFlag	= 0x00000000; // 0 for non-tasks

	switch  (rPattern->PatternType) {
	case 0x0:
		rPattern->PatternTypeSpecific.Day = 0;
		break;
	case 0x1:
		rPattern->PatternTypeSpecific.WeekRecurrencePattern =  mandatory_long_from_hash(ht, "WeekRecurrencePattern");
		break;
	case 0x2:
        case 0x4:
	case 0xA:
	case 0xC:
                //PatternTypeSpecific Month
		rPattern->PatternTypeSpecific.Day =  mandatory_long_from_hash(ht, "MonthRecurrencePattern");
		break;
	case 0x3:
	case 0xB:
                //PatternTypeSpecific MonthNth
		rPattern->PatternTypeSpecific.MonthRecurrencePattern.WeekRecurrencePattern= mandatory_long_from_hash(ht, "MonthRecurrencePattern.WeekRecurrencePattern");
		rPattern->PatternTypeSpecific.MonthRecurrencePattern.N = mandatory_long_from_hash(ht, "MonthRecurrencePattern.N");
		break;
	default:
		php_error(E_ERROR, "SHOULD NOT BE REACHED: Invalid PatternType: %i", rPattern->PatternType);
	}

	rPattern->EndType = mandatory_long_from_hash(ht, "EndType"); //; // NEVER END
	if (rPattern->EndType == 0x00002023) {
		rPattern->OccurrenceCount = 0X0000000A; // value recommended for non-end date
	} else {
		rPattern->OccurrenceCount =  mandatory_long_from_hash(ht, "OccurrenceCount");
		if (rPattern->OccurrenceCount < 0) {
			php_error(E_ERROR, "OccurrenceCount must be a positivwe number");
		}
	}

	rPattern->FirstDOW = 1; // week begins in monday this cannto change
	rPattern->DeletedInstanceCount  = 0;
	rPattern->ModifiedInstanceCount = 0;

	rPattern->StartDate		=  mandatory_long_from_hash(ht, "StartDate");
	if (rPattern->StartDate < 0) {
		php_error(E_ERROR, "StartDate must be a positive number");
	}

	if (rPattern->EndType ==  0x00002023) {
		rPattern->EndDate = 0x5AE980DF; // value for never ends
	} else {
		rPattern->EndDate = mandatory_long_from_hash(ht, "EndDate");
	}
	if (rPattern->EndDate < 0) {
		php_error(E_ERROR, "EndDate must be a positive number");
	}


	// WEEKLY RECURRENce see ical2exchange_property.c :541 for more examples
}

void set_appointment_recurrence_pattern(struct AppointmentRecurrencePattern *arp, zval *to_set)
{
	HashTable *ht = to_set->value.ht;
	zval **to_set_rp;

	if (zend_hash_find(ht, "RecurrencePattern", strlen("RecurrencePattern")+1, (void**) &to_set_rp) != SUCCESS) {
		php_error(E_ERROR, "Cannot find RecurrencePattern member");
	}

	set_recurrence_pattern(&(arp->RecurrencePattern), *to_set_rp);
	arp->ReaderVersion2 = 0x00003006;
	arp->WriterVersion2 = 0x00003009;

	long lval;
	if (hash_find_long(ht, "StartTimeOffset", &lval) == SUCCESS) {
		arp->StartTimeOffset = lval;
	} else {
		php_error(E_ERROR, "Missing StartTimeOffset field");
	}
	if (hash_find_long(ht, "EndTimeOffset", &lval) == SUCCESS) {
		arp->EndTimeOffset = lval;
	} else {
		php_error(E_ERROR, "Missing EndTimeOffset field");
	}

	// XXX Exceptions not managed by now
	arp->ExceptionCount = 0;
	arp->ExceptionInfo = NULL;

	arp->ReservedBlock1Size = 0;
	arp->ReservedBlock1 = NULL;
	arp->ExtendedException = NULL;
	arp->ReservedBlock2Size = 0;
	arp->ReservedBlock2 = NULL;
}

zval* appointment_recurrence_pattern_to_zval(struct AppointmentRecurrencePattern *arp)
{
	struct RecurrencePattern   *rp;
	zval			   *zv;
	zval                       *zrp;
	MAKE_STD_ZVAL(zv);
	array_init(zv);

	rp = &(arp->RecurrencePattern);
	MAKE_STD_ZVAL(zrp);
	array_init(zrp);

	switch(rp->RecurFrequency) {
	case 0x200A:
		add_assoc_string(zrp, "RecurFrequency", "daily", 1);
		break;
	case 0x200B:
		add_assoc_string(zrp, "RecurFrequency", "weekly", 1);
		break;
	case 0x200C:
		add_assoc_string(zrp, "RecurFrequency", "monthly", 1);
		break;
	case 0x200D:
		add_assoc_string(zrp, "RecurFrequency", "yearly", 1);
		break;
	default:
		php_error(E_ERROR, "Unknown RecurFrequency: %i", rp->RecurFrequency);

	}


	switch(rp->PatternType) {
	case 0x0:
		add_assoc_string(zrp, "PatternType", "daily", 1);
		break;
	case 0x1:
		add_assoc_string(zrp, "PatternType", "week", 1);
		break;
	case 0x2:
		add_assoc_string(zrp, "PatternType", "month", 1);
		break;
	case 0x3:
		add_assoc_string(zrp, "PatternType", "monthNth", 1);
		break;
	case 0x4:
		add_assoc_string(zrp, "PatternType", "monthEnd", 1);
		break;
	case 0xA:
	case 0xB:
	case 0xC:
		php_error(E_ERROR, "Hijr calendar not supported yet");
	default:
		php_error(E_ERROR, "Invalid time pattern type: %i", rp->PatternType);
	}

	add_assoc_long(zrp, "FirstDateTime", rp->FirstDateTime);
	add_assoc_long(zrp, "Period", rp->Period);
	// PatternTypeSpecific
	switch (rp->EndType) {
	case 0x00002021:
		add_assoc_string(zrp, "EndType", "AFTER_DATE", 1);
		break;
	case 0x00002022:
		add_assoc_string(zrp, "EndType", "AFTER_N", 1);
		break;
	case 0x00002023:
	case 0xFFFFFFFF:
		add_assoc_string(zrp, "EndType", "NEVER", 1);
		break;
	default:
		php_error(E_ERROR, "Unknown EndType %i", rp->EndType);
	}
	if (rp->OccurrenceCount	!= 0X0000000A) {
		add_assoc_long(zrp, "OccurrenceCount", rp->OccurrenceCount);
	}

	add_assoc_long(zrp, "DeletedInstanceCount", rp->DeletedInstanceCount);
	add_assoc_long(zrp, "ModifiedInstanceCount", rp->ModifiedInstanceCount);
	// TODO list of Delted And Modified count

	add_assoc_long(zrp, "StartDate", rp->StartDate);
	if (rp->EndDate != 0x5AE980DF) {
		add_assoc_long(zrp, "EndDate", rp->EndDate);
	}

	add_assoc_zval(zv, "RecurrencePattern", zrp);
	add_assoc_long(zv, "StartTimeOffset", arp->StartTimeOffset);
	add_assoc_long(zv, "EndTimeOffset", arp->EndTimeOffset);


	return zv;
}


PHP_METHOD(MAPIAppointment, setRecurrence)
{
	zval			*values_to_set;
	mapi_message_object_t 	*message_obj;
	TALLOC_CTX	*mem_ctx;
	struct Binary_r	*bin_pattern;
	struct AppointmentRecurrencePattern recurrence;
	mapi_id_t id =  0x82160102; // PidLidAppointmentRecur

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &values_to_set) == FAILURE) {
		php_error(E_ERROR, "You need to pass an array with the values to set");
	}

	set_appointment_recurrence_pattern(&recurrence, values_to_set);

	message_obj = (mapi_message_object_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
	mem_ctx = message_obj->talloc_ctx;

	bin_pattern = set_AppointmentRecurrencePattern(mem_ctx, &recurrence);
	mapi_message_so_set_prop(mem_ctx, message_obj->message, id, (void*) bin_pattern TSRMLS_CC);
}

PHP_METHOD(MAPIAppointment, getRecurrence)
{
	mapi_message_object_t 	*message_obj;
	TALLOC_CTX	*mem_ctx;
	struct Binary_r	*bin_pattern;
	struct AppointmentRecurrencePattern *recurrence;
	mapi_id_t id =  0x82160102; // PidLidAppointmentRecur

	message_obj = (mapi_message_object_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
	mem_ctx = message_obj->talloc_ctx;

	bin_pattern = (struct Binary_r*) find_mapi_SPropValue_data(&(message_obj->properties), id);
	recurrence = get_AppointmentRecurrencePattern(mem_ctx, bin_pattern);

	zval *res = appointment_recurrence_pattern_to_zval(recurrence);
	RETURN_ZVAL(res, 0, 1);
}
