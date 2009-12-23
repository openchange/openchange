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

#include <libexchange2ical/libexchange2ical.h>

static void getRange(const char *range, struct tm *start, struct tm *end)
{
	char *startString;
	char *endString;

	startString = strtok((char *) range, "-");
	endString = strtok(NULL, "-");

	end->tm_mon	= atoi(strtok(endString, "/"))-1;
	end->tm_mday	= atoi(strtok(NULL, "/"));
	end->tm_year	= atoi(strtok(NULL, "/"))-1900;
	end->tm_min	= 0;
	end->tm_hour	= 0;
	end->tm_sec	= 0;

	
	start->tm_mon	= atoi(strtok(startString, "/"))-1;
	start->tm_mday	= atoi(strtok(NULL, "/"));
	start->tm_year	= atoi(strtok(NULL, "/"))-1900;
	start->tm_min	= 0;
	start->tm_hour	= 0;
	start->tm_sec	= 0;
	
	return;
}

static char* read_stream(char *s, size_t size, void *d) 
{ 
  char *c = fgets(s, size, (FILE*)d);

  return c;
}

int main(int argc, const char *argv[])
{
	enum MAPISTATUS			retval;
	poptContext			pc;
	int				opt;
	mapi_object_t			obj_store;
	mapi_object_t			obj_folder;
	const char			*opt_profdb = NULL;
	const char			*opt_profname = NULL;
	const char			*opt_password = NULL;
	const char			*opt_debug = NULL;
	const char			*opt_filename = NULL;
	const char			*opt_icalsync = NULL;
	const char			*opt_range = NULL;
	bool				opt_dumpdata = false;
	FILE 	 			*fp = NULL;
	mapi_id_t			fid;
	struct mapi_session		*session = NULL;
	icalcomponent *vcal;
	struct tm start;
	struct tm end;
	icalparser *parser;
	icalcomponent *ical;
	icalcomponent *vevent;
	TALLOC_CTX			*mem_ctx;

	

	enum { OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_PASSWORD, OPT_DEBUG, OPT_DUMPDATA, OPT_FILENAME, OPT_RANGE, OPT_ICALSYNC };

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "database",	'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB,	"set the profile database path",		NULL },
		{ "profile",	'p', POPT_ARG_STRING, NULL, OPT_PROFILE,	"set the profile name",				NULL },
		{ "password",	'P', POPT_ARG_STRING, NULL, OPT_PASSWORD,	"set the profile password",			NULL },
		{ "icalsync", 	'i', POPT_ARG_STRING, NULL, OPT_ICALSYNC,	"set the icalendar to convert to exchange",	NULL },
		{ "filename",	'o', POPT_ARG_STRING, NULL, OPT_FILENAME,	"set the output iCalendar filename",		NULL },
		{ "range",	'R', POPT_ARG_STRING, NULL, OPT_RANGE,		"set the range of accepted start dates", 	NULL },
		{ "debuglevel",	'd', POPT_ARG_STRING, NULL, OPT_DEBUG,		"set the debug level",				NULL },
		{ "dump-data",	  0, POPT_ARG_NONE,   NULL, OPT_DUMPDATA,	"dump the hex data",				NULL },
		POPT_OPENCHANGE_VERSION
		{ NULL,		  0, 0,		      NULL, 0,			NULL,					NULL }
	};

	
	pc = poptGetContext("exchange2ical", argc, argv, long_options, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_PROFILE_DB:
			opt_profdb = poptGetOptArg(pc);
			break;
		case OPT_FILENAME:
			opt_filename = poptGetOptArg(pc);
			break;
		case OPT_ICALSYNC:
			opt_icalsync = poptGetOptArg(pc);
			break;
		case OPT_RANGE:
			opt_range = poptGetOptArg(pc);
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
	
	mem_ctx = talloc_named(NULL, 0, "exchange2ical_tool");

	/* Sanity Checks */
	if (!opt_profdb) {
		opt_profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
	}

	/* Initialize MAPI subsystem */
	retval = MAPIInitialize(opt_profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		return 1;
	}
	
	/* debug options */
	if (opt_debug) {
		SetMAPIDebugLevel(atoi(opt_debug));
	}
	SetMAPIDumpData(opt_dumpdata);
	
	session = octool_init_mapi(opt_profname, opt_password, 0);
	if(!session){
		mapi_errstr("Session", GetLastError());
		return 1;
	}


		
	/* Open Mailbox */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(session, &obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore", GetLastError());
		return 1;
	}

	/* Get default calendar folder */
	retval = GetDefaultFolder(&obj_store, &fid, olFolderCalendar);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("GetDefaultFolder", GetLastError());
		return 1;
	}
	
	/* Open default calendar folder */
	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, fid, &obj_folder);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenFolder", GetLastError());
		return 1;
	}
	

	/*Ical2exchange*/
	if(opt_icalsync){
		if ((fp = fopen(opt_icalsync, "r")) == NULL) {
			perror("Can not open Icalendar file");
		} else {
			parser = icalparser_new();
			icalparser_set_gen_data(parser,fp);
			ical = icalparser_parse(parser, read_stream);
			printf("\n\nICAL file:\n%s\n", icalcomponent_as_ical_string(ical));

			icalcomponent_strip_errors(ical);
		
			vevent = icalcomponent_get_first_component(ical, ICAL_VEVENT_COMPONENT);
			while(vevent){
				_IcalEvent2Exchange(&obj_folder, vevent);
				vevent = icalcomponent_get_next_component(ical, ICAL_VEVENT_COMPONENT);
			}	
			icalcomponent_free(ical);
			icalparser_free(parser);
			fclose(fp);
			fp = NULL;
		}
	}
	
	if(opt_range){
		getRange(opt_range, &start, &end);
		vcal = Exchange2IcalRange(&obj_folder, &start, &end);
	} else {
		vcal = Exchange2Ical(&obj_folder);
	}


	if(vcal){				
		/* Icalendar save or print to console */
		char *cal = icalcomponent_as_ical_string(vcal);
		if (!opt_filename) {
			printf("\n\nICAL file:\n%s\n", cal);
		} else {
			size_t bytesWritten;
			
			if ((fp = fopen(opt_filename, "w")) == NULL) {
				perror("fopen");
				exit (1);
			}
			bytesWritten = fwrite(cal, strlen(cal), 1, fp);
			if (bytesWritten < 1) {
				printf("BOGUS write length: %zi", bytesWritten);
			}
			fclose(fp);
		}
		free(cal);
		icalcomponent_free(vcal);
	}
	poptFreeContext(pc);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);
	MAPIUninitialize();
	talloc_free(mem_ctx);	

	return 0;
}


