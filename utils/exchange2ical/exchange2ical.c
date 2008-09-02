/*
   Convert Exchange appointments and meetings to ICAL files

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

#include <utils/exchange2ical/exchange2ical.h>
#include <samba/popt.h>
#include <param.h>
#include <utils/openchange-tools.h>

static void exchange2ical_init(TALLOC_CTX *mem_ctx, struct exchange2ical *exchange2ical)
{
	exchange2ical->mem_ctx = mem_ctx;

	exchange2ical->method = NULL;
	exchange2ical->partstat = NULL;
	exchange2ical->Recurring = NULL;
	exchange2ical->RecurrencePattern = NULL;
	exchange2ical->TimeZoneStruct = NULL;
	exchange2ical->TimeZoneDesc = NULL;
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
}

static void exchange2ical_reset(struct exchange2ical *exchange2ical)
{
	if (exchange2ical->RecurrencePattern) {
		talloc_free(exchange2ical->RecurrencePattern);
	}

	if (exchange2ical->TimeZoneStruct) {
		talloc_free(exchange2ical->TimeZoneStruct);
	}

	exchange2ical_init(exchange2ical->mem_ctx, exchange2ical);
}

static struct SPropTagArray *exchange2ical_set_properties(TALLOC_CTX *mem_ctx, mapi_object_t *obj_message)
{
	enum MAPISTATUS		retval;
	struct mapi_nameid	*nameid;
	struct SPropTagArray	*SPropTagArray = NULL;

	/* Build the list of named properties we want to fetch */
	nameid = mapi_nameid_new(mem_ctx);

	mapi_nameid_string_add(nameid, "Keywords", PS_PUBLIC_STRINGS);

	mapi_nameid_OOM_add(nameid, "Recurring", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "ApptRecur", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "ApptStateFlags", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "TimeZoneDesc", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "TimeZoneStruct", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "Contacts", PSETID_Common);
	mapi_nameid_OOM_add(nameid, "ApptStartWhole", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "ApptEndWhole", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "ApptSubType", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "LID_OWNER_CRITICAL_CHANGE", PSETID_Meeting);
	mapi_nameid_OOM_add(nameid, "Location", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "ExceptionReplaceTime", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "NonSendableBCC", PSETID_Common);
	mapi_nameid_OOM_add(nameid, "ApptSequence", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "BusyStatus", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "IntendedBusyStatus", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "CleanGlobalObjId", PSETID_Meeting);
	mapi_nameid_OOM_add(nameid, "LID_ATTENDEE_CRITICAL_CHANGE", PSETID_Meeting);
	mapi_nameid_OOM_add(nameid, "ApptReplyTime", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "ApptNotAllowPropose", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "AllowExternCheck", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "ApptLastSequence", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "ApptSeqTime", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "AutoFillLocation", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "AutoStartCheck", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "CollaborateDoc", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "ConfCheck", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "ConfType", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "Directory", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "MWSURL", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "NetShowURL", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "OnlinePassord", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "OrgAlias", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "ReminderSet", PSETID_Common);
	mapi_nameid_OOM_add(nameid, "ReminderDelta", PSETID_Common);

	/* GetIDsFromNames and map property types */
	SPropTagArray = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = GetIDsFromNames(obj_message, nameid->count, nameid->nameid, 0, &SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("GetIDsFromNames", GetLastError());
		return NULL;
	}
	mapi_nameid_SPropTagArray(nameid, SPropTagArray);
	MAPIFreeBuffer(nameid);

	SPropTagArray_add(mem_ctx, SPropTagArray, PR_MESSAGE_CLASS_UNICODE);
	SPropTagArray_add(mem_ctx, SPropTagArray, PR_SENSITIVITY);
	SPropTagArray_add(mem_ctx, SPropTagArray, PR_BODY_UNICODE);
	SPropTagArray_add(mem_ctx, SPropTagArray, PR_CREATION_TIME);
	SPropTagArray_add(mem_ctx, SPropTagArray, PR_LAST_MODIFICATION_TIME);
	SPropTagArray_add(mem_ctx, SPropTagArray, PR_IMPORTANCE);
	SPropTagArray_add(mem_ctx, SPropTagArray, PR_RESPONSE_REQUESTED);
	SPropTagArray_add(mem_ctx, SPropTagArray, PR_SUBJECT_UNICODE);
	SPropTagArray_add(mem_ctx, SPropTagArray, PR_OWNER_APPT_ID);
	SPropTagArray_add(mem_ctx, SPropTagArray, PR_SENDER_NAME);
	SPropTagArray_add(mem_ctx, SPropTagArray, PR_SENDER_EMAIL_ADDRESS);

	return SPropTagArray;
}

static int exchange2ical_get_properties(TALLOC_CTX *mem_ctx, struct SRow *aRow, struct exchange2ical *exchange2ical)
{
	struct SBinary	*apptrecur;
	struct SBinary	*TimeZoneStruct;

	/* We don't have convenient way to retrieve MNID_STRING tags */
	if ((aRow->lpProps[0].ulPropTag & 0xFFFF) == PT_ERROR) {
		exchange2ical->Keywords = NULL;
	} else {
		exchange2ical->Keywords = (const struct SLPSTRArray *)get_SPropValue_data(&(aRow->lpProps[0]));
	}

	exchange2ical->method = get_ical_method((const char *) octool_get_propval(aRow, PR_MESSAGE_CLASS_UNICODE));

	if (!exchange2ical->method) return -1;

	exchange2ical->Recurring = (uint8_t *) octool_get_propval(aRow, 0x8223000B);
	apptrecur = (struct SBinary *) octool_get_propval(aRow, 0x82160102);
	exchange2ical->RecurrencePattern = get_RecurrencePattern(mem_ctx, apptrecur);

	exchange2ical->TimeZoneDesc = (const char *) octool_get_propval(aRow, 0x8234001F);
	TimeZoneStruct = (struct SBinary *) octool_get_propval(aRow, 0x82330102);
	exchange2ical->TimeZoneStruct = get_TimeZoneStruct(mem_ctx, TimeZoneStruct);

	exchange2ical->GlobalObjectId = (struct SBinary *) octool_get_propval(aRow, 0x00230102);
	exchange2ical->apptStateFlags = (uint32_t *) octool_get_propval(aRow, 0x82170003);
	exchange2ical->Contacts = (const struct SLPSTRArray *)octool_get_propval(aRow, 0x853A101E);
	exchange2ical->apptStartWhole = (const struct FILETIME *)octool_get_propval(aRow, 0x820D0040);
	exchange2ical->apptEndWhole = (const struct FILETIME *)octool_get_propval(aRow, 0x820E0040);
	exchange2ical->apptSubType = (uint8_t *) octool_get_propval(aRow, 0x8215000B);
	exchange2ical->OwnerCriticalChange = (const struct FILETIME *)octool_get_propval(aRow, 0x001A0040);
	exchange2ical->Location = (const char *) octool_get_propval(aRow, 0x8208001F);
	exchange2ical->ExceptionReplaceTime = (const struct FILETIME *)octool_get_propval(aRow, 0x82280040);
	exchange2ical->NonSendableBcc = (const char *) octool_get_propval(aRow, 0x8538001F);
	exchange2ical->Sequence = (uint32_t *) octool_get_propval(aRow, 0x82010003);
	exchange2ical->BusyStatus = (uint32_t *) octool_get_propval(aRow, 0x82050003);
	exchange2ical->IntendedBusyStatus = (uint32_t *) octool_get_propval(aRow, 0x82240003);
	exchange2ical->AttendeeCriticalChange = (const struct FILETIME *) octool_get_propval(aRow, 0x00010040);
	exchange2ical->apptReplyTime = (const struct FILETIME *)octool_get_propval(aRow, 0x82200040);
	exchange2ical->NotAllowPropose = (uint8_t *) octool_get_propval(aRow, 0x825A000B);
	exchange2ical->AllowExternCheck = (uint8_t *) octool_get_propval(aRow, 0x8246000B);
	exchange2ical->apptLastSequence = (uint32_t *) octool_get_propval(aRow, 0x82030003);
	exchange2ical->apptSeqTime = (const struct FILETIME *)octool_get_propval(aRow, 0x82020040);
	exchange2ical->AutoFillLocation = (uint8_t *) octool_get_propval(aRow, 0x823A000B);
	exchange2ical->AutoStartCheck = (uint8_t *) octool_get_propval(aRow, 0x8244000B);
	exchange2ical->CollaborateDoc = (const char *) octool_get_propval(aRow, 0x8247001F);
	exchange2ical->ConfCheck = (uint8_t *) octool_get_propval(aRow, 0x8240000B);
	exchange2ical->ConfType = (uint32_t *) octool_get_propval(aRow, 0x82410003);
	exchange2ical->Directory = (const char *) octool_get_propval(aRow, 0x8242001F);
	exchange2ical->MWSURL = (const char *) octool_get_propval(aRow, 0x8209001F);
	exchange2ical->NetShowURL = (const char *) octool_get_propval(aRow, 0x8248001F);
	exchange2ical->OnlinePassword = (const char *) octool_get_propval(aRow, 0x8249001F);
	exchange2ical->OrgAlias = (const char *) octool_get_propval(aRow, 0x8243001F);
	exchange2ical->ReminderSet = (uint8_t *) octool_get_propval(aRow, 0x8503000B);
	exchange2ical->ReminderDelta = (uint32_t *) octool_get_propval(aRow, 0x85010003);

	exchange2ical->sensitivity = (uint32_t *) octool_get_propval(aRow, PR_SENSITIVITY);
	exchange2ical->partstat = get_ical_partstat((const char *) octool_get_propval(aRow, PR_MESSAGE_CLASS_UNICODE));
	exchange2ical->created = (const struct FILETIME *)octool_get_propval(aRow, PR_CREATION_TIME);
	exchange2ical->body = (const char *)octool_get_propval(aRow, PR_BODY_UNICODE);
	exchange2ical->LastModified = (const struct FILETIME *)octool_get_propval(aRow, PR_LAST_MODIFICATION_TIME);
	exchange2ical->Importance = (uint32_t *) octool_get_propval(aRow, PR_IMPORTANCE);
	exchange2ical->ResponseRequested = (uint8_t *) octool_get_propval(aRow, PR_RESPONSE_REQUESTED);
	exchange2ical->Subject = (const char *) octool_get_propval(aRow, PR_SUBJECT_UNICODE);
	exchange2ical->OwnerApptId = (uint32_t *) octool_get_propval(aRow, PR_OWNER_APPT_ID);
	exchange2ical->SenderName = (const char *) octool_get_propval(aRow, PR_SENDER_NAME);
	exchange2ical->SenderEmailAddress = (const char *) octool_get_propval(aRow, PR_SENDER_EMAIL_ADDRESS);

	return 0;
}

int main(int argc, const char *argv[])
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	int				ret;
	struct SRowSet			SRowSet;
	struct SRow			aRow;
	struct SPropValue		*lpProps;
	struct SPropTagArray		*SPropTagArray = NULL;
	struct mapi_SPropValue_array	properties_array;
	struct exchange2ical		exchange2ical;
	mapi_object_t			obj_store;
	mapi_object_t			obj_folder;
	mapi_object_t			obj_table;
	mapi_object_t			obj_message;
	mapi_id_t			fid;
	uint32_t			count;
	poptContext			pc;
	int				opt;
	uint32_t			i;
	const char			*opt_profdb = NULL;
	const char			*opt_profname = NULL;
	const char			*opt_password = NULL;
	const char			*opt_debug = NULL;
	bool				opt_dumpdata = false;

	enum { OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_PASSWORD, OPT_DEBUG, OPT_DUMPDATA };

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "database",	'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB,	"set the profile database path" },
		{ "profile",	'p', POPT_ARG_STRING, NULL, OPT_PROFILE,	"set the profile name" },
		{ "password",	'P', POPT_ARG_STRING, NULL, OPT_PASSWORD,	"set the profile password" },
		{ "debuglevel",	'd', POPT_ARG_STRING, NULL, OPT_DEBUG,		"set the debug level" },
		{ "dump-data",	  0, POPT_ARG_NONE,   NULL, OPT_DUMPDATA,	"dump the hex data" },
		{ NULL }
	};

	mem_ctx = talloc_init("exchange2ical");
	exchange2ical_init(mem_ctx, &exchange2ical);

	pc = poptGetContext("exchange2ical", argc, argv, long_options, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_PROFILE_DB:
			opt_profdb = poptGetOptArg(pc);
			break;
		case OPT_PROFILE:
			opt_profname = poptGetOptArg(pc);
			break;
		case OPT_PASSWORD:
			opt_password = poptGetOptArg(pc);
			break;
		case OPT_DEBUG:
			opt_debug = poptGetOptArg(pc);
			break;
		case OPT_DUMPDATA:
			opt_dumpdata = true;
			break;
		}
	}

	/* Sanity Checks */
	if (!opt_profdb) {
		opt_profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
	}

	/* Initialize MAPI subsystem */
	retval = MAPIInitialize(opt_profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}
	
	/* debug options */
	if (opt_debug) {
		lp_set_cmdline(global_mapi_ctx->lp_ctx, "log level", opt_debug);
	}

	if (opt_dumpdata == true) {
		global_mapi_ctx->dumpdata = true;
	}

	retval = octool_init_mapi(mem_ctx, opt_profname, opt_password, 0);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* Open Mailbox */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore", GetLastError());
		exit (1);
	}

	/* Open default calendar folder */
/* 	retval = GetDefaultFolder(&obj_store, &fid, olFolderInbox); */
	retval = GetDefaultFolder(&obj_store, &fid, olFolderCalendar);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("GetDefaultFolder", GetLastError());
		exit (1);
	}

	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, fid, &obj_folder);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenFolder", GetLastError());
		exit (1);
	}

	/* Open the contents table */
	mapi_object_init(&obj_table);
	retval = GetContentsTable(&obj_folder, &obj_table, 0, &count);
	if (retval != MAPI_E_SUCCESS) return false;

	DEBUG(0, ("MAILBOX (%d appointments)\n", count));

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2,
					  PR_FID,
					  PR_MID);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("SetColumns", GetLastError());
	}

	while ((retval = QueryRows(&obj_table, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND && SRowSet.cRows) {
		count -= SRowSet.cRows;
		for (i = 0; i < SRowSet.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(&obj_folder,
					     SRowSet.aRow[i].lpProps[0].value.d,
					     SRowSet.aRow[i].lpProps[1].value.d,
					     &obj_message, 0);
			if (retval != MAPI_E_NOT_FOUND) {

				retval = GetRecipientTable(&obj_message, 
							   &exchange2ical.Recipients.SRowSet,
							   &exchange2ical.Recipients.SPropTagArray);

				SPropTagArray = exchange2ical_set_properties(mem_ctx, &obj_message);
				retval = GetProps(&obj_message, SPropTagArray, &lpProps, &count);
				MAPIFreeBuffer(SPropTagArray);

				if (retval == MAPI_E_SUCCESS) {
					uint32_t j;
					properties_array.cValues = count;
					properties_array.lpProps = talloc_array(mem_ctx, struct mapi_SPropValue, count + 1);

					for (j = 0; j < count; j++) {
						cast_mapi_SPropValue(&properties_array.lpProps[j], &lpProps[j]);
					}
					mapi_SPropValue_array_named(&obj_message, &properties_array);
					talloc_free(lpProps);
					
					lpProps = talloc_array(mem_ctx, struct SPropValue, count + 1);
					for (j = 0; j < count; j++) {
						cast_SPropValue(&properties_array.lpProps[j], &lpProps[j]);
					}

					aRow.ulAdrEntryPad = 0;
					aRow.cValues = count;
					aRow.lpProps = lpProps;

					ret = exchange2ical_get_properties(mem_ctx, &aRow, &exchange2ical);
					if (!ret) {
						ical_component_VCALENDAR(&exchange2ical);
						DEBUG(0, ("\n\n"));
					}
					exchange2ical_reset(&exchange2ical);
				}
				MAPIFreeBuffer(lpProps);

				mapi_object_release(&obj_message);
			}
		}
	}

	/* Uninitialize MAPI subsystem */
	mapi_object_release(&obj_table);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	MAPIUninitialize();
	talloc_free(mem_ctx);

	return 0;
}
