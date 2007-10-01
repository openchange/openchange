/* 
   OpenChange MAPI implementation testsuite

   Send appointments to an Exchange server

   Copyright (C) Julien Kerihuel 2007
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <libmapi/libmapi.h>
#include <gen_ndr/ndr_exchange.h>
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture/torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>

#include <time.h>

#define	DATE_FORMAT "%Y-%m-%d %H:%M:%S"

#define CN_PROPS 14

bool torture_rpc_mapi_sendappointment(struct torture_context *torture)
{
	NTSTATUS		nt_status;
	enum MAPISTATUS		retval;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	bool			ret = true;
	const char		*appointment = lp_parm_string(global_loadparm, NULL, "mapi", "appointment");
	const char		*body = lp_parm_string(global_loadparm, NULL, "mapi", "body");
	const char		*location = lp_parm_string(global_loadparm, NULL, "mapi", "location");
	const char		*start = lp_parm_string(global_loadparm, NULL, "mapi", "start");
	const char		*end = lp_parm_string(global_loadparm, NULL, "mapi", "end");
	uint32_t		busy_status = lp_parm_int(global_loadparm, NULL, "mapi", "busystatus", 0);
	uint32_t		label = lp_parm_int(global_loadparm, NULL, "mapi", "label", 0);
	struct mapi_session	*session;
	uint64_t		id_calendar;
	mapi_object_t		obj_store;
	mapi_object_t		obj_calendar;
	mapi_object_t		obj_message;
	struct SPropValue	props[CN_PROPS];
	NTTIME			nt;
	struct tm		tm;
	struct FILETIME		*start_date;
	struct FILETIME		*end_date;
	uint32_t		flag;
	uint8_t			flag2;

	if (!appointment) return false;
	if (busy_status > 3) return false;
	if (!start || !end) return false;

	/* init torture */
	mem_ctx = talloc_init("torture_rpc_mapi_sendappointment");
	nt_status = torture_rpc_connection(mem_ctx, &p, &ndr_table_exchange_emsmdb);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return false;
	}

	/* init mapi */
	if ((session = torture_init_mapi(mem_ctx)) == NULL) return false;

	/* init objects */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_calendar);

	/* session::OpenMsgStore */
	retval = OpenMsgStore(&obj_store);
	mapi_errstr("OpenMsgStore", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	retval = GetDefaultFolder(&obj_store, &id_calendar, olFolderCalendar);
	mapi_errstr("GetDefaultFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* We now open the calendar folder */
	retval = OpenFolder(&obj_store, id_calendar, &obj_calendar);
	mapi_errstr("OpenFolder", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* Operations on the calendar folder */
	retval = CreateMessage(&obj_calendar, &obj_message);
	mapi_errstr("CreateMessage", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	if (!strptime(start, DATE_FORMAT, &tm)) {
		printf("Invalid start date\n");
		return false;
	}
	unix_to_nt_time(&nt, mktime(&tm));
	start_date = talloc(mem_ctx, struct FILETIME);
	start_date->dwLowDateTime = (nt << 32) >> 32;
	start_date->dwHighDateTime = (nt >> 32);


	if (!strptime(end, DATE_FORMAT, &tm)) {
		printf("Invalid end date\n");
		return false;
	}
	unix_to_nt_time(&nt, mktime(&tm));
	end_date = talloc(mem_ctx, struct FILETIME);
	end_date->dwLowDateTime = (nt << 32) >> 32;
	end_date->dwHighDateTime = (nt >> 32);
	
	set_SPropValue_proptag(&props[0], PR_CONVERSATION_TOPIC, 
						   (const void *) appointment);
	set_SPropValue_proptag(&props[1], PR_NORMALIZED_SUBJECT, (const void *) appointment);
	set_SPropValue_proptag(&props[2], PR_START_DATE, (const void *) start_date);
	set_SPropValue_proptag(&props[3], PR_END_DATE, (const void *) end_date);
	set_SPropValue_proptag(&props[4], PR_MESSAGE_CLASS, (const void *)"IPM.Appointment");
	flag = 1;
	set_SPropValue_proptag(&props[5], PR_MESSAGE_FLAGS, (const void *) &flag);
	set_SPropValue_proptag(&props[6], PR_APPOINTMENT_LOCATION, (const void *)(location?location:""));
	set_SPropValue_proptag(&props[7], PR_BusyStatus, (const void *) &busy_status);
	flag= MEETING_STATUS_NONMEETING;
	set_SPropValue_proptag(&props[8], PR_APPOINTMENT_MEETING_STATUS, (const void *) &flag);
	flag2 = true;
	set_SPropValue_proptag(&props[9], PR_CommonStart, (const void *) start_date);
	set_SPropValue_proptag(&props[10], PR_CommonEnd, (const void *) end_date);
	set_SPropValue_proptag(&props[11], PR_LABEL, (const void *)&label);
	flag = 30;
	set_SPropValue_proptag(&props[12], PR_ReminderMinutesBeforeStart, (const void *)&flag);
	set_SPropValue_proptag(&props[13], PR_BODY, (const void *)(body?body:""));
	retval = SetProps(&obj_message, props, CN_PROPS);
	mapi_errstr("SetProps", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	retval = SaveChangesMessage(&obj_calendar, &obj_message);
	mapi_errstr("SaveChangesMessage", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;


	mapi_object_release(&obj_calendar);
	mapi_object_release(&obj_store);

	/* uninitialize mapi
	 */
	MAPIUninitialize();
	talloc_free(mem_ctx);
	
	return (ret);
}
