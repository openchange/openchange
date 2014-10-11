/*
   List the system and special folders for the user mailbox

   OpenChange Project

   Copyright (C) Wolfgang Sourdeau <wsourdeau@inverse.ca> 2011

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

/* gcc list-folders.c -o list-folders -I /usr/local/samba/include -L /usr/local/samba/lib -lmapi -ltalloc -lpopt */

#include <inttypes.h>
#include <stdbool.h>
#include "gen_ndr/exchange.h"

#include "libmapi/libmapi.h"

#include <popt.h>
#include <ldb.h>
#include <talloc.h>

static void popt_openchange_version_callback(poptContext con,
                                             enum poptCallbackReason reason,
                                             const struct poptOption *opt,
                                             const char *arg,
                                             const void *data)
{
        switch (opt->val) {
        case 'V':
                printf("Version %s\n", OPENCHANGE_VERSION_STRING);
                exit (0);
        }
}

struct poptOption popt_openchange_version[] = {
        { NULL, '\0', POPT_ARG_CALLBACK, (void *)popt_openchange_version_callback, '\0', NULL, NULL },
        { "version", 'V', POPT_ARG_NONE, NULL, 'V', "Print version ", NULL },
        POPT_TABLEEND
};

#define DEFAULT_PROFDB  "%s/.openchange/profiles.ldb"

static int64_t GetGlobalCounter(uint8_t *ptr)
{
	int64_t id, base;
	uint8_t i;

	id = 0;
	base = 1;
	for (i = 0; i < 6; i++) {
		id += *(ptr + i) * base;
		base <<= 8;
	}

	return id;
}

static void ShowBinaryEntryId(struct Binary_r *bin)
{
	int64_t id;

	id = GetGlobalCounter(bin->lpb + 38);
	printf("0x%.12"PRIx64"\n", id);
}

static void ShowSimpleEntryId(const char *label, struct SPropValue *value)
{
	printf("%25s: ", label);
	if ((value->ulPropTag & 0xffff) == PT_BINARY) {
		ShowBinaryEntryId(&value->value.bin);
	}
	else {
		printf("(not available)\n");
	}
}

static void ShowAdditionalRenEntryIds(struct SPropValue *value)
{
	struct BinaryArray_r *mbin;
	struct Binary_r	*bin;
	uint8_t i;
	const char *names[] = { "Conflicts", "Sync Issues", "Local Failures", "Server Failures", "Junk E-mail" };

	if ((value->ulPropTag & 0xffff) == PT_MV_BINARY) {
		mbin = &value->value.MVbin;
		for (i = 0; i < mbin->cValues; i++) {
			if (i < 5) {
				printf("%25s: ", names[i]);
				bin = mbin->lpbin + i;
				if (bin->cb == 46) {
					ShowBinaryEntryId(mbin->lpbin + i);
				}
				else {
					printf("skipped\n");
				}
			}
		}
	}
	else {
		printf("'AdditionalRenEntryIds' not available\n");
	}
}


struct PersistDataArray {
	struct PersistData *data;
	uint16_t PersistDataCount;
};

struct PersistData {
	uint16_t PersistID;
	uint16_t DataElementsSize;
	struct PersistElement *DataElements;
	uint16_t DataElementsCount;
	struct PersistData *next;
	struct PersistData *prev;
};

struct PersistElement {
	uint16_t ElementID;
	uint16_t ElementDataSize;
	uint8_t *ElementData;
	struct PersistElement *next;
	struct PersistElement *prev;
};

static inline uint16_t ReadWord(const uint8_t *ptr)
{
	uint16_t w;

	w = *ptr;
	w += *(ptr + 1) << 8;

	return w;
}

static uint8_t *ReadPersistElement(TALLOC_CTX *mem_ctx, struct PersistElement **elementp, uint8_t *ptr, uint16_t max_size)
{
	uint8_t *newptr;
	uint16_t size;
	struct PersistElement *element = NULL;

	if (max_size > 4) {
		size = max_size;
		newptr = ptr;
		element = talloc_zero(mem_ctx, struct PersistElement);
		element->ElementID = ReadWord(newptr);
		newptr += 2;
		element->ElementDataSize = ReadWord(newptr);
		newptr += 2;
		size = element->ElementDataSize;
		if (size > 0) {
			element->ElementData = talloc_memdup(element, newptr, size);
		}
	}
	else {
		newptr = NULL;
	}

	*elementp = element;

	return newptr;
}

static uint8_t *ReadPersistData(TALLOC_CTX *mem_ctx, struct PersistData **datap, uint8_t *ptr, uint16_t max_size)
{
	uint8_t *newptr, *newnewptr;
	uint16_t size;
	struct PersistData *data = NULL;
	struct PersistElement *element;

	if (max_size > 4) {
		size = max_size;
		newptr = ptr;
		data = talloc_zero(mem_ctx, struct PersistData);
		data->PersistID = 0;
		data->PersistID = ReadWord(newptr);
		newptr += 2;
		data->DataElementsSize = ReadWord(newptr);
		newptr += 2;
		size = data->DataElementsSize;
		while (size > 0) {
			newnewptr = ReadPersistElement(data, &element, newptr, size);
			if (newnewptr) {
				size -= (newnewptr - newptr);
				newptr = newnewptr;
				DLIST_ADD_END(data->DataElements, element, void);
				data->DataElementsCount++;
			}
			else {
				size = 0;
			}
		}
	}
	else {
		newptr = NULL;
	}

	*datap = data;

	return newptr;
}

static struct PersistDataArray *BinToPersistData(TALLOC_CTX *mem_ctx, struct Binary_r *bin)
{
	struct PersistDataArray *array;
	struct PersistData *data;
	uint8_t *ptr, *newptr;
	uint16_t size;

	size = bin->cb;
	ptr = bin->lpb;
	if (size > 0) {
		array = talloc_zero(mem_ctx, struct PersistDataArray);
		while (size > 0) {
			newptr = ReadPersistData(array, &data, ptr, size);
			if (newptr) {
				size -= (newptr - ptr);
				ptr = newptr;
				DLIST_ADD_END(array->data, data, void);
				array->PersistDataCount++;
			}
			else {
				size = 0;
			}
		}
	}
	else {
		array = NULL;
	}

	return array;
}

static void ShowAdditionalRenEntryIdsEx(struct SPropValue *value)
{
	struct Binary_r	*bin;
	struct PersistDataArray *array;

	bin = &value->value.bin;
	if (value->ulPropTag == PidTagAdditionalRenEntryIdsEx && bin->cb > 0) {
		bin = &value->value.bin;
		array = BinToPersistData(NULL, bin);
		talloc_free(array);
	}
	else {
		printf("'AdditionalRenEntryIdsEx' not available\n");
	}
}

int main(int argc, const char *argv[])
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct mapi_context		*mapi_ctx;
	struct mapi_session		*session = NULL;
	mapi_object_t			obj_store, obj_folder;
	struct mapi_obj_store		*store;
	struct SPropTagArray		folder_props;
	enum MAPITAGS			folder_properties[] = { PidTagIpmAppointmentEntryId, PidTagIpmContactEntryId, PidTagIpmJournalEntryId, PidTagIpmNoteEntryId, PidTagIpmTaskEntryId, PidTagRemindersOnlineEntryId, PidTagRemindersOfflineEntryId, PidTagIpmDraftsEntryId, PidTagAdditionalRenEntryIds, PidTagAdditionalRenEntryIdsEx };
	struct SPropValue		*folder_values;
	uint32_t			folder_value_count;
	poptContext			pc;
	int				opt;
	const char			*opt_profdb = NULL;
	char				*opt_profname = NULL;
	const char			*opt_password = NULL;

	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_PASSWORD};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path", "PATH"},
		{"profile", 'p', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name", "PROFILE"},
		{"password", 'P', POPT_ARG_STRING, NULL, OPT_PASSWORD, "set the profile password", "PASSWORD"},
		{ NULL, 0, POPT_ARG_INCLUDE_TABLE, popt_openchange_version, 0, "Common openchange options:", NULL },
		{ NULL, 0, POPT_ARG_NONE, NULL, 0, NULL, NULL }
	};

	mem_ctx = talloc_named(NULL, 0, "list-folders");

	pc = poptGetContext("list-folders", argc, argv, long_options, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_PROFILE_DB:
			opt_profdb = poptGetOptArg(pc);
			break;
		case OPT_PROFILE:
			opt_profname = talloc_strdup(mem_ctx, (char *)poptGetOptArg(pc));
			break;
		case OPT_PASSWORD:
			opt_password = poptGetOptArg(pc);
			break;
		}
	}

	/**
	 * Sanity checks
	 */

	if (!opt_profdb) {
		opt_profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
	}

	/**
	 * Initialize MAPI subsystem
	 */

	retval = MAPIInitialize(&mapi_ctx, opt_profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", retval);
		exit (1);
	}

	/* if no profile is supplied use the default one */
	if (!opt_profname) {
		retval = GetDefaultProfile(mapi_ctx, &opt_profname);
		if (retval != MAPI_E_SUCCESS) {
			printf("No profile specified and no default profile found\n");
			exit (1);
		}
	}

	retval = MapiLogonEx(mapi_ctx, &session, opt_profname, opt_password);
	talloc_free(opt_profname);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", retval);
		exit (1);
	}

	/* Open the default message store */
	mapi_object_init(&obj_store);

	retval = OpenMsgStore(session, &obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore", retval);
		exit (1);
	}
	store = (struct mapi_obj_store *)obj_store.private_data;

	printf("System folders (RopLogon):\n");
	printf("%25s: 0x%.12"PRIx64"\n", "Inbox", store->fid_inbox >> 16);
	printf("%25s: 0x%.12"PRIx64"\n", "Root", store->fid_mailbox_root >> 16);
	printf("%25s: 0x%.12"PRIx64"\n", "Deferred Actions", store->fid_deferred_actions >> 16);
	printf("%25s: 0x%.12"PRIx64"\n", "Spooler Queue", store->fid_spooler_queue >> 16);
	printf("%25s: 0x%.12"PRIx64"\n", "Top Information Store", store->fid_top_information_store >> 16);
	printf("%25s: 0x%.12"PRIx64"\n", "Outbox", store->fid_outbox >> 16);
	printf("%25s: 0x%.12"PRIx64"\n", "Sent Items", store->fid_sent_items >> 16);
	printf("%25s: 0x%.12"PRIx64"\n", "Deleted Items", store->fid_deleted_items >> 16);
	printf("%25s: 0x%.12"PRIx64"\n", "Common Views", store->fid_common_views >> 16);
	printf("%25s: 0x%.12"PRIx64"\n", "Schedule", store->fid_schedule >> 16);
	printf("%25s: 0x%.12"PRIx64"\n", "Search", store->fid_search >> 16);
	printf("%25s: 0x%.12"PRIx64"\n", "Views", store->fid_views >> 16);
	printf("%25s: 0x%.12"PRIx64"\n", "Shortcuts", store->fid_shortcuts >> 16);

	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, store->fid_inbox, &obj_folder);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenFolder", retval);
		exit (1);
	}

	folder_props.cValues = sizeof(folder_properties) / sizeof(enum MAPITAGS);
	folder_props.aulPropTag = folder_properties;
	retval = GetProps(&obj_folder, 0, &folder_props, &folder_values, &folder_value_count);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("GetProps", retval);
		exit (1);
	}

	printf("\n");
	printf("Folder entries:\n");
	ShowSimpleEntryId("Calendar", folder_values + 0);
	ShowSimpleEntryId("Contacts", folder_values + 1);
	ShowSimpleEntryId("Journal", folder_values + 2);
	ShowSimpleEntryId("Note", folder_values + 3);
	ShowSimpleEntryId("Tasks", folder_values + 4);
	ShowSimpleEntryId("Reminders (online)", folder_values + 5);
	ShowSimpleEntryId("Reminders (offline)", folder_values + 6);
	ShowSimpleEntryId("Drafts", folder_values + 7);
	
	printf("\n");
	printf("Additional entries:\n");
	ShowAdditionalRenEntryIds(folder_values + 8);

	/* FIXME: buggy code should be reviewed */
	/* printf("\n"); */
	/* printf("Extended additional entries:\n"); */
	/* ShowAdditionalRenEntryIdsEx(folder_values + 9); */

	Release(&obj_folder);


	/* mapi_object_release(&obj_msg2); */
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);
	MAPIUninitialize(mapi_ctx);

	talloc_free(mem_ctx);

	return 0;
}
