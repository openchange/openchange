/*
   Stand-alone MAPI application

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010

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

/*
  This sample code shows how to use multiple sessions in OpenChange
  and shows notification system works with multiple sessions.

  To use this sample code, run in the console:
  ./multiple_notification --profileA=XXX --profileB=YYY

  Send an email using any MAPI compliant client (openchangeclient for
  example) to recipients matching profileA and profileB.

  If everything works properly, you should see the output above:
  $ ./multiple_notification --profileA=XXX --profileB=YYY
  profileA: New mail Received
  profileB: New mail received

  For further example on how to retrieve and analyse 
 */

#include <libmapi/libmapi.h>
#include <pthread.h> 
#include <popt.h>

#define	DEFAULT_PROFDB	"%s/.openchange/profiles.ldb"

static int callback(uint16_t NotificationType, void *NotificationData, void *private_data)
{
	switch (NotificationType) {
	case fnevNewMail:
	case fnevNewMail|fnevMbit:
		OC_DEBUG(0, "%s: New mail Received", (const char *)private_data));
		break;
	default:
		break;
	}
	
	return (0);
}

void *monitor(void *val)
{
	enum MAPISTATUS	retval;
	mapi_object_t	*obj_store;

	obj_store = (mapi_object_t *) val;
	retval = MonitorNotification(mapi_object_get_session(obj_store), (void *)obj_store, NULL);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MonitorNotification", GetLastError());
		pthread_exit(NULL);
	}
}


int main(int ac, const char *av[])
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	struct mapi_context	*mapi_ctx;
	struct mapi_session	*sessionA = NULL;
	struct mapi_session	*sessionB = NULL;
	mapi_object_t		obj_storeA;
	mapi_object_t		obj_storeB;
	uint32_t		ulConnectionA;
	uint32_t		ulConnectionB;
	poptContext		pc;
	int			opt;
	const char		*opt_profdb = NULL;
	const char		*profileA = NULL;
	const char		*profileB = NULL;
	pthread_t		pthreadA;
	pthread_t		pthreadB;

	enum { OPT_PROFILE_DB=1000, OPT_PROFILEA, OPT_PROFILEB };

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 0, POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path", NULL},
		{"profileA", 0, POPT_ARG_STRING, NULL, OPT_PROFILEA, "profile A", NULL},
		{"profileB", 0, POPT_ARG_STRING, NULL, OPT_PROFILEB, "profile B", NULL},
		{NULL, 0, 0, NULL, 0, NULL, NULL}
	};

	/* Step 1. Retrieve and parse command line options */
	mem_ctx = talloc_named(NULL, 0, "multiple_notif");

	pc = poptGetContext("multiple_notif", ac, av, long_options, 0);
	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_PROFILE_DB:
			opt_profdb = poptGetOptArg(pc);
			break;
		case OPT_PROFILEA:
			profileA = poptGetOptArg(pc);
			break;
		case OPT_PROFILEB:
			profileB = poptGetOptArg(pc);
			break;
		}
	}

	if (!opt_profdb) {
		opt_profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
	}

	if (!profileA || !profileB) {
		printf("You need to specify 2 profiles (--profileA and --profileB options\n");
		exit (1);
	}

	/* Step 2. Initialize MAPI subsystem */
	retval = MAPIInitialize(&mapi_ctx, opt_profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	retval = MapiLogonEx(mapi_ctx, &sessionA, profileA, NULL);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx for profileA", GetLastError());
		exit (1);
	}

	retval = MapiLogonEx(mapi_ctx, &sessionB, profileB, NULL);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx for profileB", GetLastError());
		exit (1);
	}

	/* Step 3. Open the stores */
	mapi_object_init(&obj_storeA);
	retval = OpenMsgStore(sessionA, &obj_storeA);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore profileA", GetLastError());
		exit (1);
	}
	
	mapi_object_init(&obj_storeB);
	retval = OpenMsgStore(sessionB, &obj_storeB);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore profileB", GetLastError());
		exit (1);
	}

	/* Step 4. Register for notifications */
	retval = RegisterNotification(sessionA);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("RegisterNotification profileA", GetLastError());
		exit (1);
	}

	retval = RegisterNotification(sessionB);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("RegisterNotification profileB", GetLastError());
		exit (1);
	}

	/* Step 5. Subscribe for newmail notifications */
	retval = Subscribe(&obj_storeA, &ulConnectionA, fnevNewMail, true, 
			   (mapi_notify_callback_t)callback, (void *) "profileA");
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("Subscribe profileA", GetLastError());
		exit (1);
	}

	retval = Subscribe(&obj_storeB, &ulConnectionB, fnevNewMail, true, 
			   (mapi_notify_callback_t)callback, (void *) "profileB");
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("Subscribe profileB", GetLastError());
		exit (1);
	}

	/* Step 6. Create threads A and B and wait for notifications */
	if (pthread_create(&pthreadA, NULL, monitor, (void *)&obj_storeA)) { 
		mapi_errstr("Thread creation for profile A failed", GetLastError());
		exit (1);
	}

	if (pthread_create(&pthreadB, NULL, monitor, (void *)&obj_storeB)) {
		mapi_errstr("Thread creation for profile B failed", GetLastError());
		exit (1);
	}

	sleep(200);
	pthread_exit(NULL);
	MAPIUninitialize(mapi_ctx);

	return 0;
}
