/*
   Stand-alone MAPI application

   OpenChange Project

   Copyright (C) Julien Kerihuel 2007

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

#include <libmapi/libmapi.h>
#include <samba/popt.h>
#include <param.h>
#include <param/proto.h>

#include "openchangeclient.h"
#include "openchange-tools.h"

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>

/**
 * init sendmail struct
 */

static void init_oclient(struct oclient *oclient)
{
	oclient->subject = "";
	oclient->pr_body = NULL;
	oclient->pr_html_inline = NULL;
	oclient->attach = NULL;
	oclient->attach_num = 0;
	oclient->store_folder = NULL;
	
	/* appointment related parameters */
	oclient->location = NULL;
	oclient->dtstart = NULL;
	oclient->dtend = NULL;
	oclient->busystatus = 0;
	oclient->label = 0;
	oclient->private = false;

	/* contact related parameters */
	oclient->email = "";
	oclient->full_name = "";
	oclient->card_name = "";

	/* task related parameters */
	oclient->importance = 1;
	oclient->priority = 0;
	oclient->taskstatus = 0;

	/* note related parameters */
	oclient->color = olYellow;
	oclient->width = 166;
	oclient->height = 200;

	/* pf related parameters */
	oclient->pf = false;

	/* folder related parameters */
	oclient->folder = NULL;
	oclient->folder_name = NULL;
	oclient->folder_comment = NULL;
}

static char *utf8tolinux(TALLOC_CTX *mem_ctx, const char *wstring)
{
	char		*newstr;

	newstr = windows_to_utf8(mem_ctx, wstring);
	return newstr;
}

static enum MAPISTATUS openchangeclient_getdir(TALLOC_CTX *mem_ctx, 
					       mapi_object_t *obj_container,
					       mapi_object_t *obj_child,
					       const char *folder)
{
	enum MAPISTATUS		retval;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		rowset;
	mapi_object_t		obj_htable;
	char			*newname;
	const char		*name;
	const uint64_t		*fid;
	uint32_t		index;

	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(obj_container, &obj_htable);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2,
					  PR_DISPLAY_NAME,
					  PR_FID);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;
	
	while ((retval = QueryRows(&obj_htable, 0x32, TBL_ADVANCE, &rowset) != MAPI_E_NOT_FOUND) && rowset.cRows) {
		for (index = 0; index < rowset.cRows; index++) {
			fid = (const uint64_t *)find_SPropValue_data(&rowset.aRow[index], PR_FID);
			name = (const char *)find_SPropValue_data(&rowset.aRow[index], PR_DISPLAY_NAME);

			newname = utf8tolinux(mem_ctx, name);
			if (newname && fid && !strcmp(newname, folder)) {
				retval = OpenFolder(obj_container, *fid, obj_child);
				MAPI_RETVAL_IF(retval, GetLastError(), NULL);
				return MAPI_E_SUCCESS;
			}
			MAPIFreeBuffer(newname);
		}
	}

	return MAPI_E_NOT_FOUND;
}

static enum MAPISTATUS openchangeclient_getpfdir(TALLOC_CTX *mem_ctx, 
						 mapi_object_t *obj_store,
						 mapi_object_t *obj_child,
						 const char *name)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_pf;
	mapi_id_t		id_pf;

	retval = GetDefaultPublicFolder(obj_store, &id_pf, olFolderPublicIPMSubtree);
	if (retval != MAPI_E_SUCCESS) return retval;
	
	mapi_object_init(&obj_pf);
	retval = OpenFolder(obj_store, id_pf, &obj_pf);
	if (retval != MAPI_E_SUCCESS) return retval;
	
	retval = openchangeclient_getdir(mem_ctx, &obj_pf, obj_child, name);
	if (retval != MAPI_E_SUCCESS) return retval;

	return MAPI_E_SUCCESS;
}

/**
 * read a file and store it in the appropriate structure element
 */

static bool oclient_read_file(TALLOC_CTX *mem_ctx, const char *filename, 
			      struct oclient *oclient, uint32_t mapitag)
{
	struct stat	sb;
	int		fd;

	/* stat the file */
	if (stat(filename, &sb) != 0) return false;
	if ((fd = open(filename, O_RDONLY)) == -1) {
		printf("Error while opening %s\n", filename);
		return false;
	}

	switch (mapitag) {
	case PR_HTML:
		oclient->pr_html.lpb = talloc_size(mem_ctx, sb.st_size);
		oclient->pr_html.cb = read(fd, oclient->pr_html.lpb, sb.st_size);
		close(fd);
		break;
	case PR_ATTACH_DATA_BIN:
		oclient->attach[oclient->attach_num].filename = talloc_strdup(mem_ctx, filename);
		oclient->attach[oclient->attach_num].bin.lpb = talloc_size(mem_ctx, sb.st_size);
		oclient->attach[oclient->attach_num].bin.cb = sb.st_size;
		if ((oclient->attach[oclient->attach_num].bin.lpb = mmap(NULL, sb.st_size, PROT_READ, MAP_FILE|MAP_SHARED, fd, 0)) == (void *) -1) {
			perror("mmap");
			close(fd);
			return false;
		}
		oclient->attach[oclient->attach_num].fd = fd;
		printf("filename = %s (size = %d / %d)\n", filename, oclient->attach[oclient->attach_num].bin.cb, (uint32_t)sb.st_size);
		close(fd);
		break;
	default:
		printf("unsupported MAPITAG: %s\n", get_proptag_name(mapitag));
		close(fd);
		return false;
		break;
	}

	return true;
}

/**
 * Parse attachments and load their content
 */
static bool oclient_parse_attachments(TALLOC_CTX *mem_ctx, const char *filename,
				      struct oclient *oclient)
{
	char		**filenames;
	char		*tmp = NULL;
	uint32_t	j;

	if ((tmp = strtok((char *)filename, ";")) == NULL) {
		printf("Invalid string format [;]\n");
		return false;
	}

	filenames = talloc_array(mem_ctx, char *, 2);
	filenames[0] = strdup(tmp);

	for (j = 1; (tmp = strtok(NULL, ";")) != NULL; j++) {
		filenames = talloc_realloc(mem_ctx, filenames, char *, j+2);
		filenames[j] = strdup(tmp);
	}
	filenames[j] = 0;
	oclient->attach = talloc_array(mem_ctx, struct attach, j);

	for (j = 0; filenames[j]; j++) {
		oclient->attach_num = j;
		if (oclient_read_file(mem_ctx, filenames[j], oclient, PR_ATTACH_DATA_BIN) == false) {
			return false;
		}
	}

	return true;
}


static const char *get_filename(const char *filename)
{
	const char *substr;

	if (!filename) return NULL;

	substr = rindex(filename, '/');
	if (substr) return substr;

	return filename;
}

/**
 * fetch the user INBOX
 */

#define	MAX_READ_SIZE	0x1000

static bool store_attachment(mapi_object_t obj_attach, const char *filename, uint32_t size, struct oclient *oclient)
{
	TALLOC_CTX	*mem_ctx;
	enum MAPISTATUS	retval;
	char		*path;
	mapi_object_t	obj_stream;
	uint32_t	read_size;
	int		fd;
	unsigned char  	buf[MAX_READ_SIZE];

	if (!filename || !size) return false;

	mem_ctx = talloc_init("store_attachment");
	mapi_object_init(&obj_stream);

	if ((fd = open(oclient->store_folder, O_DIRECTORY)) == -1) {
		if (mkdir(oclient->store_folder, 0700) == -1) return false;
	} else {
		close (fd);
	}

	path = talloc_asprintf(mem_ctx, "%s/%s", oclient->store_folder, filename);
	if ((fd = open(path, O_CREAT|O_WRONLY, S_IWUSR|S_IRUSR)) == -1) return false;
	talloc_free(path);

	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 0, &obj_stream);
	if (retval != MAPI_E_SUCCESS) return false;

	read_size = 0;
	do {
		retval = ReadStream(&obj_stream, buf, MAX_READ_SIZE, &read_size);
		if (retval != MAPI_E_SUCCESS) goto error;
		write(fd, buf, read_size);
	} while (read_size);
	
	close(fd);

	mapi_object_release(&obj_stream);
	close(fd);
	talloc_free(mem_ctx);
	return true;

error:
	mapi_object_release(&obj_stream);
	close(fd);
	talloc_free(mem_ctx);
	return false;
}

static enum MAPISTATUS openchangeclient_fetchmail(mapi_object_t *obj_store, 
						  struct oclient *oclient)
{
	enum MAPISTATUS			retval;
	bool				status;
	TALLOC_CTX			*mem_ctx;
	mapi_object_t			obj_inbox;
	mapi_object_t			obj_message;
	mapi_object_t			obj_table;
	mapi_object_t			obj_tb_attach;
	mapi_object_t			obj_attach;
	uint64_t			id_inbox;
	struct SPropTagArray		*SPropTagArray;
	struct SRowSet			rowset;
	struct SRowSet			rowset_attach;
	uint32_t			i, j;
	uint32_t			count;
	const uint8_t			*has_attach;
	const uint32_t			*attach_num;
	const char			*attach_filename;
	const uint32_t			*attach_size;
	
	mem_ctx = talloc_init("openchangeclient_fetchmail");

	mapi_object_init(&obj_inbox);
	mapi_object_init(&obj_table);

	if (oclient->pf == true) {
		retval = openchangeclient_getpfdir(mem_ctx, obj_store, &obj_inbox, oclient->folder);
		MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx);
	} else {
		retval = GetReceiveFolder(obj_store, &id_inbox);
		MAPI_RETVAL_IF(retval, retval, mem_ctx);

		retval = OpenFolder(obj_store, id_inbox, &obj_inbox);
		MAPI_RETVAL_IF(retval, retval, mem_ctx);
	}

	retval = GetContentsTable(&obj_inbox, &obj_table);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	retval = GetRowCount(&obj_table, &count);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	printf("MAILBOX (%d messages)\n", count);

	while ((retval = QueryRows(&obj_table, count, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		count -= rowset.cRows;
		for (i = 0; i < rowset.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(obj_store,
					     rowset.aRow[i].lpProps[0].value.d,
					     rowset.aRow[i].lpProps[1].value.d,
					     &obj_message, 0);
			if (GetLastError() == MAPI_E_SUCCESS) {
				struct SPropValue	*lpProps;
				struct SRow		aRow;

				SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_HASATTACH);
				lpProps = talloc_zero(mem_ctx, struct SPropValue);
				retval = GetProps(&obj_message, SPropTagArray, &lpProps, &count);
				MAPIFreeBuffer(SPropTagArray);
				if (retval != MAPI_E_SUCCESS) return false;

				aRow.ulAdrEntryPad = 0;
				aRow.cValues = count;
				aRow.lpProps = lpProps;

				retval = octool_message(mem_ctx, &obj_message);

				has_attach = (const uint8_t *) get_SPropValue_SRow_data(&aRow, PR_HASATTACH);

 				/* If we have attachments, retrieve them */
				if (has_attach && *has_attach) {
					mapi_object_init(&obj_tb_attach);
					retval = GetAttachmentTable(&obj_message, &obj_tb_attach);
					if (retval == MAPI_E_SUCCESS) {
						SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_ATTACH_NUM);
						retval = SetColumns(&obj_tb_attach, SPropTagArray);
						if (retval != MAPI_E_SUCCESS) return retval;
						MAPIFreeBuffer(SPropTagArray);
						
						retval = QueryRows(&obj_tb_attach, 0xa, TBL_ADVANCE, &rowset_attach);
						if (retval != MAPI_E_SUCCESS) return false;

						for (j = 0; j < rowset_attach.cRows; j++) {
							attach_num = (const uint32_t *)find_SPropValue_data(&(rowset_attach.aRow[j]), PR_ATTACH_NUM);
							retval = OpenAttach(&obj_message, *attach_num, &obj_attach);
							if (retval == MAPI_E_SUCCESS) {
								struct SPropValue	*lpProps2;
								uint32_t		count2;

								SPropTagArray = set_SPropTagArray(mem_ctx, 0x3, 
												  PR_ATTACH_FILENAME,
												  PR_ATTACH_LONG_FILENAME,
												  PR_ATTACH_SIZE);
								lpProps2 = talloc_zero(mem_ctx, struct SPropValue);
								retval = GetProps(&obj_attach, SPropTagArray, &lpProps2, &count2);
								MAPIFreeBuffer(SPropTagArray);
								if (retval != MAPI_E_SUCCESS) return false;
								
								aRow.ulAdrEntryPad = 0;
								aRow.cValues = count2;
								aRow.lpProps = lpProps2;

								attach_filename = get_filename(octool_get_propval(&aRow, PR_ATTACH_LONG_FILENAME));
								if (!attach_filename || (attach_filename && !strcmp(attach_filename, ""))) {
									attach_filename = get_filename(octool_get_propval(&aRow, PR_ATTACH_FILENAME));
								}
								attach_size = (const uint32_t *) octool_get_propval(&aRow, PR_ATTACH_SIZE);
								printf("[%d] %s (%d Bytes)\n", j, attach_filename, attach_size ? *attach_size : 0);
								fflush(0);
								if (oclient->store_folder) {
									status = store_attachment(obj_attach, attach_filename, *attach_size, oclient);
									if (status == false) {
										printf("A Problem was encountered while storing attachments on the filesystem\n");
										MAPI_RETVAL_IF(status == false, MAPI_E_UNABLE_TO_COMPLETE, mem_ctx);

									}
								}
								MAPIFreeBuffer(lpProps2);
							}
						}
						errno = 0;
					}
					MAPIFreeBuffer(lpProps);
				}
			}
			mapi_object_release(&obj_message);
		}
	}

	mapi_object_release(&obj_table);
	mapi_object_release(&obj_inbox);

	talloc_free(mem_ctx);

	errno = 0;
	return MAPI_E_SUCCESS;
}

/**
 * send a mail to a user whom belongs to the Exchange organization
 */

static char **get_cmdline_recipients(TALLOC_CTX *mem_ctx, const char *recipients)
{
	char		**usernames;
	char		*tmp = NULL;
	uint32_t	j = 0;

	/* no recipients */
	if (!recipients) {
		return NULL;
	}

	if ((tmp = strtok((char *)recipients, ",")) == NULL) {
		DEBUG(2, ("Invalid recipient string format\n"));
		return NULL;
	}
	
	usernames = talloc_array(mem_ctx, char *, 2);
	usernames[0] = strdup(tmp);
	
	for (j = 1; (tmp = strtok(NULL, ",")) != NULL; j++) {
		usernames = talloc_realloc(mem_ctx, usernames, char *, j+2);
		usernames[j] = strdup(tmp);
	}
	usernames[j] = 0;

	return (usernames);
}

/**
 * We set external recipients at the end of aRow
 */
static bool set_external_recipients(TALLOC_CTX *mem_ctx, struct SRowSet *SRowSet, const char *username, enum ulRecipClass RecipClass)
{
	uint32_t		last;
	struct SPropValue	SPropValue;

	SRowSet->aRow = talloc_realloc(mem_ctx, SRowSet->aRow, struct SRow, SRowSet->cRows + 2);
	last = SRowSet->cRows;
	SRowSet->aRow[last].cValues = 0;
	SRowSet->aRow[last].lpProps = talloc_zero(mem_ctx, struct SPropValue);
	
	/* PR_OBJECT_TYPE */
	SPropValue.ulPropTag = PR_OBJECT_TYPE;
	SPropValue.value.l = MAPI_MAILUSER;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_DISPLAY_TYPE */
	SPropValue.ulPropTag = PR_DISPLAY_TYPE;
	SPropValue.value.l = 0;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_GIVEN_NAME */
	SPropValue.ulPropTag = PR_GIVEN_NAME;
	SPropValue.value.lpszA = username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_DISPLAY_NAME */
	SPropValue.ulPropTag = PR_DISPLAY_NAME;
	SPropValue.value.lpszA = username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_7BIT_DISPLAY_NAME */
	SPropValue.ulPropTag = PR_7BIT_DISPLAY_NAME;
	SPropValue.value.lpszA = username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_SMTP_ADDRESS */
	SPropValue.ulPropTag = PR_SMTP_ADDRESS;
	SPropValue.value.lpszA = username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_ADDRTYPE */
	SPropValue.ulPropTag = PR_ADDRTYPE;
	SPropValue.value.lpszA = "SMTP";
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	SetRecipientType(&(SRowSet->aRow[last]), RecipClass);

	SRowSet->cRows += 1;
	return true;
}

static bool set_usernames_RecipientType(TALLOC_CTX *mem_ctx, uint32_t *index, struct SRowSet *rowset, 
					char **usernames, struct FlagList *flaglist,
					enum ulRecipClass RecipClass)
{
	uint32_t	i;
	uint32_t	count = *index;
	static uint32_t	counter = 0;

	if (count == 0) counter = 0;
	if (!usernames) return false;

	for (i = 0; usernames[i]; i++) {
		if (flaglist->ulFlags[count] == MAPI_UNRESOLVED) {
			set_external_recipients(mem_ctx, rowset, usernames[i], RecipClass);
		}
		if (flaglist->ulFlags[count] == MAPI_RESOLVED) {
			SetRecipientType(&(rowset->aRow[counter]), RecipClass);
			counter++;
		}
		count++;
	}
	
	*index = count;
	
	return true;
}

static char **collapse_recipients(TALLOC_CTX *mem_ctx, struct oclient *oclient)
{
	uint32_t	count;
	uint32_t       	i;
	char		**usernames;

	if (!oclient->mapi_to && !oclient->mapi_cc && !oclient->mapi_bcc) return NULL;

	count = 0;
	for (i = 0; oclient->mapi_to && oclient->mapi_to[i]; i++,  count++);
	for (i = 0; oclient->mapi_cc && oclient->mapi_cc[i]; i++,  count++);
	for (i = 0; oclient->mapi_bcc && oclient->mapi_bcc[i]; i++, count++);

	usernames = talloc_array(mem_ctx, char *, count + 1);
	count = 0;

	for (i = 0; oclient->mapi_to && oclient->mapi_to[i]; i++, count++) {
		usernames[count] = talloc_strdup(mem_ctx, oclient->mapi_to[i]);
	}

	for (i = 0; oclient->mapi_cc && oclient->mapi_cc[i]; i++, count++) {
		usernames[count] = talloc_strdup(mem_ctx, oclient->mapi_cc[i]);
	}

	for (i = 0; oclient->mapi_bcc && oclient->mapi_bcc[i]; i++, count++) {
		usernames[count] = talloc_strdup(mem_ctx, oclient->mapi_bcc[i]);
	}

	usernames[count++] = 0;

	return usernames;
}

/**
 * Write a stream with MAX_READ_SIZE chunks
 */

#define	MAX_READ_SIZE	0x1000

static bool openchangeclient_stream(TALLOC_CTX *mem_ctx, mapi_object_t obj_parent, 
				    mapi_object_t obj_stream, uint32_t mapitag, 
				    uint32_t access_flags, struct SBinary bin)
{
	enum MAPISTATUS	retval;
	DATA_BLOB	stream;
	uint32_t	size;
	uint32_t	offset;
	uint16_t	read_size;

	/* Open a stream on the parent for the given property */
	retval = OpenStream(&obj_parent, mapitag, access_flags, &obj_stream);
	if (retval != MAPI_E_SUCCESS) return false;

	/* WriteStream operation */
	printf("We are about to write %d bytes in the stream\n", bin.cb);
	size = MAX_READ_SIZE;
	offset = 0;
	while (offset <= bin.cb) {
		stream.length = size;
		stream.data = talloc_size(mem_ctx, size);
		memcpy(stream.data, bin.lpb + offset, size);
		
		retval = WriteStream(&obj_stream, &stream, &read_size);
		talloc_free(stream.data);
		if (retval != MAPI_E_SUCCESS) return false;
		printf(".");
		fflush(0);

		/* Exit when there is nothing left to write */
		if (!read_size) return true;
		
		offset += read_size;

		if ((offset + size) > bin.cb) {
			size = bin.cb - offset;
		}
	}

	return true;
}


#define SETPROPS_COUNT	4

/**
 * Send a mail
 */

static enum MAPISTATUS openchangeclient_sendmail(TALLOC_CTX *mem_ctx, 
						 mapi_object_t *obj_store, 
						 struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	SPropValue;
	struct SRowSet		*SRowSet = NULL;
	struct FlagList		*flaglist = NULL;
	mapi_id_t		id_outbox;
	mapi_object_t		obj_outbox;
	mapi_object_t		obj_message;
	mapi_object_t		obj_stream;
	uint32_t		index = 0;
	uint32_t		msgflag;
	struct SPropValue	props[SETPROPS_COUNT];
	uint32_t		prop_count = 0;
	uint32_t		editor = 0;

	mapi_object_init(&obj_outbox);
	mapi_object_init(&obj_message);

	if (oclient->pf == true) {
		retval = openchangeclient_getpfdir(mem_ctx, obj_store, &obj_outbox, oclient->folder);
		if (retval != MAPI_E_SUCCESS) return retval;
	} else {
		/* Get Sent Items folder but should be using olFolderOutbox */
		retval = GetDefaultFolder(obj_store, &id_outbox, olFolderSentMail);
		if (retval != MAPI_E_SUCCESS) return retval;

		/* Open outbox folder */
		retval = OpenFolder(obj_store, id_outbox, &obj_outbox);
		if (retval != MAPI_E_SUCCESS) return retval;
	}

	/* Create the message */
	retval = CreateMessage(&obj_outbox, &obj_message);
	if (retval != MAPI_E_SUCCESS) return retval;

	/* Recipients operations */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x6,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_7BIT_DISPLAY_NAME,
					  PR_DISPLAY_NAME,
					  PR_SMTP_ADDRESS,
					  PR_GIVEN_NAME);

	oclient->usernames = collapse_recipients(mem_ctx, oclient);

	/* ResolveNames */
	retval = ResolveNames((const char **)oclient->usernames, SPropTagArray, &SRowSet, &flaglist, 0);
	if (retval != MAPI_E_SUCCESS) return false;

	if (!SRowSet) {
		SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	}

	set_usernames_RecipientType(mem_ctx, &index, SRowSet, oclient->mapi_to, flaglist, MAPI_TO);
	set_usernames_RecipientType(mem_ctx, &index, SRowSet, oclient->mapi_cc, flaglist, MAPI_CC);
	set_usernames_RecipientType(mem_ctx, &index, SRowSet, oclient->mapi_bcc, flaglist, MAPI_BCC);

	SPropValue.ulPropTag = PR_SEND_INTERNET_ENCODING;
	SPropValue.value.l = 0;
	SRowSet_propcpy(mem_ctx, SRowSet, SPropValue);

	/* ModifyRecipients operation */
	retval = ModifyRecipients(&obj_message, SRowSet);
	MAPIFreeBuffer(SRowSet);
	MAPIFreeBuffer(flaglist);
	if (retval != MAPI_E_SUCCESS) return retval;

	/* set message properties */
	msgflag = MSGFLAG_UNSENT;
	set_SPropValue_proptag(&props[0], PR_SUBJECT, 
						   (const void *)oclient->subject);
	set_SPropValue_proptag(&props[1], PR_MESSAGE_FLAGS, 
						   (const void *)&msgflag);
	prop_count = 2;

	/* Set PR_BODY or PR_HTML or inline PR_HTML */
	if (oclient->pr_body) {
		editor = EDITOR_FORMAT_PLAINTEXT;
		set_SPropValue_proptag(&props[3], PR_MSG_EDITOR_FORMAT, (const void *)&editor);

		if (strlen(oclient->pr_body) > MAX_READ_SIZE) {
			struct SBinary	bin;

			bin.lpb = (uint8_t *)oclient->pr_body;
			bin.cb = strlen(oclient->pr_body);
			openchangeclient_stream(mem_ctx, obj_message, obj_stream, PR_BODY, 2, bin);
		} else {
			set_SPropValue_proptag(&props[2], PR_BODY, 
								   (const void *)oclient->pr_body);
			prop_count++;
		}
	} else if (oclient->pr_html_inline) {
		editor = EDITOR_FORMAT_HTML;
		set_SPropValue_proptag(&props[3], PR_MSG_EDITOR_FORMAT, (const void *)&editor);

		if (strlen(oclient->pr_html_inline) > MAX_READ_SIZE) {
			struct SBinary	bin;
			
			bin.lpb = (uint8_t *)oclient->pr_html_inline;
			bin.cb = strlen(oclient->pr_html_inline);
			openchangeclient_stream(mem_ctx, obj_message, obj_stream, PR_HTML, 2, bin);
		} else {
			struct SBinary_short bin;
			
			bin.cb = strlen(oclient->pr_html_inline);
			bin.lpb = (uint8_t *)oclient->pr_html_inline;
			set_SPropValue_proptag(&props[2], PR_HTML, (void *)&bin);
			prop_count++;
		}
		
	} else if (&oclient->pr_html) {
		editor = EDITOR_FORMAT_HTML;
		set_SPropValue_proptag(&props[3], PR_MSG_EDITOR_FORMAT, (const void *)&editor);

		if (oclient->pr_html.cb <= MAX_READ_SIZE) {
			struct SBinary_short bin;

			bin.cb = oclient->pr_html.cb;
			bin.lpb = oclient->pr_html.lpb;

			set_SPropValue_proptag(&props[2], PR_HTML, (void *)&bin);
			prop_count++;
		} else {
			mapi_object_init(&obj_stream);
			openchangeclient_stream(mem_ctx, obj_message, obj_stream, PR_HTML, 2, oclient->pr_html);
			mapi_object_release(&obj_stream);
		}
	}

	retval = SetProps(&obj_message, props, prop_count);
	if (retval != MAPI_E_SUCCESS) return retval;

	/* attachment related code */
	if (oclient->attach) {
		uint32_t	i;

		for (i = 0; oclient->attach[i].filename; i++) {
			mapi_object_t		obj_attach;
			mapi_object_t		obj_stream;
			struct SPropValue	props_attach[3];
			uint32_t		count_props_attach;

			mapi_object_init(&obj_attach);

			retval = CreateAttach(&obj_message, &obj_attach);
			if (retval != MAPI_E_SUCCESS) return retval;
		
			props_attach[0].ulPropTag = PR_ATTACH_METHOD;
			props_attach[0].value.l = ATTACH_BY_VALUE;
			props_attach[1].ulPropTag = PR_RENDERING_POSITION;
			props_attach[1].value.l = 0;
			props_attach[2].ulPropTag = PR_ATTACH_FILENAME;
			printf("Sending %s:\n", oclient->attach[i].filename);
			props_attach[2].value.lpszA = get_filename(oclient->attach[i].filename);
			count_props_attach = 3;

			/* SetProps */
			retval = SetProps(&obj_attach, props_attach, count_props_attach);
			if (retval != MAPI_E_SUCCESS) return retval;

			/* Stream operations */
			openchangeclient_stream(mem_ctx, obj_attach, obj_stream, PR_ATTACH_DATA_BIN, 2, oclient->attach[i].bin);

			/* Save changes */
			retval = SaveChanges(&obj_message, &obj_attach, KEEP_OPEN_READWRITE);
			if (retval != MAPI_E_SUCCESS) return retval;

			mapi_object_release(&obj_attach);
		}
	}

	if (oclient->pf) {
		retval = SaveChangesMessage(&obj_outbox, &obj_message);
		if (retval != MAPI_E_SUCCESS);

	} else {
		/* Submit the message */
		retval = SubmitMessage(&obj_message);
		if (retval != MAPI_E_SUCCESS) return retval;
	}

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_outbox);

	return MAPI_E_SUCCESS;
}

/**
 * delete a mail from user INBOX
 */
static bool openchangeclient_deletemail(TALLOC_CTX *mem_ctx,
					mapi_object_t *obj_store, 
					struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	mapi_id_t		id_inbox;
	mapi_id_t		*id_messages;
	uint32_t		count_messages;
	uint32_t		count_rows;
	mapi_object_t		obj_inbox;
	mapi_object_t		obj_table;
	uint32_t		len;
	uint32_t		i;

	if (!oclient->subject) {
		printf("You need to specify at least a message subject pattern\n");
		MAPI_RETVAL_IF(!oclient->subject, MAPI_E_INVALID_PARAMETER, NULL);
	}

	mapi_object_init(&obj_inbox);
	mapi_object_init(&obj_table);

	if (oclient->pf == true) {
		retval = openchangeclient_getpfdir(mem_ctx, obj_store, &obj_inbox, oclient->folder);
		if (retval != MAPI_E_SUCCESS) return retval;
	} else {
		retval = GetReceiveFolder(obj_store, &id_inbox);
		if (retval != MAPI_E_SUCCESS) return false;

		retval = OpenFolder(obj_store, id_inbox, &obj_inbox);
		if (retval != MAPI_E_SUCCESS) return false;
	}

	retval = GetContentsTable(&obj_inbox, &obj_table);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT);
	retval = SetColumns(&obj_table, SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;
	
	while ((retval = QueryRows(&obj_table, 0x10, TBL_ADVANCE, &SRowSet)) == MAPI_E_SUCCESS) {
		count_rows = SRowSet.cRows;
		if (!count_rows) break;
		id_messages = talloc_array(mem_ctx, uint64_t, count_rows);
		count_messages = 0;
		
		len = strlen(oclient->subject);

		for (i = 0; i < count_rows; i++) {
			if (!strncmp(SRowSet.aRow[i].lpProps[4].value.lpszA, oclient->subject, len)) {
				id_messages[count_messages] = SRowSet.aRow[i].lpProps[1].value.d;
				count_messages++;
			}
		}
		if (count_messages) {
			retval = DeleteMessage(&obj_inbox, id_messages, count_messages);
			if (retval != MAPI_E_SUCCESS) return false;
		}
	}

	mapi_object_release(&obj_table);
	mapi_object_release(&obj_inbox);

	return true;
}

static bool openchangeclient_sendappointment(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	struct SPropValue	props[CAL_CNPROPS];
	mapi_object_t		obj_calendar;
	mapi_object_t		obj_message;
	mapi_id_t		id_calendar;
	struct mapi_nameid	*nameid;
	struct SPropTagArray	*SPropTagArray;
	struct FILETIME		*start_date;
	struct FILETIME		*end_date;
	NTTIME			nt;
	struct tm		tm;
	uint32_t		flag;
	uint8_t			flag2;

	mapi_object_init(&obj_calendar);

	if (oclient->pf == true) {
		retval = openchangeclient_getpfdir(mem_ctx, obj_store, &obj_calendar, oclient->folder);
		if (retval != MAPI_E_SUCCESS) return false;
	} else {
		/* Open Calendar default folder */
		retval = GetDefaultFolder(obj_store, &id_calendar, olFolderCalendar);
		if (retval != MAPI_E_SUCCESS) return false;
		
		retval = OpenFolder(obj_store, id_calendar, &obj_calendar);
		if (retval != MAPI_E_SUCCESS) return false;
	}

	/* Create calendar mesage */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_calendar, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Build the list of named properties we want to set */
	nameid = mapi_nameid_new(mem_ctx);
	mapi_nameid_OOM_add(nameid, "Location", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "BusyStatus", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "MeetingStatus", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "CommonStart", PSETID_Common);
	mapi_nameid_OOM_add(nameid, "CommonEnd", PSETID_Common);
	mapi_nameid_OOM_add(nameid, "Label", PSETID_Appointment);
	mapi_nameid_OOM_add(nameid, "ReminderMinutesBeforeStart", PSETID_Common);
	mapi_nameid_OOM_add(nameid, "Private", PSETID_Common);

	/* GetIDsFromNames and map property types */
	SPropTagArray = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = GetIDsFromNames(&obj_calendar, nameid->count,
				 nameid->nameid, 0, &SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;
	mapi_nameid_SPropTagArray(nameid, SPropTagArray);
	MAPIFreeBuffer(nameid);

	if (!strptime(oclient->dtstart, DATE_FORMAT, &tm)) {
		printf("Invalid date format: yyyy-mm-dd hh:mm:ss (e.g.: 2007-09-17 10:00:00)\n");
		return false;
	}
	unix_to_nt_time(&nt, mktime(&tm));
	start_date = talloc(mem_ctx, struct FILETIME);
	start_date->dwLowDateTime = (nt << 32) >> 32;
	start_date->dwHighDateTime = (nt >> 32);

	if (!strptime(oclient->dtend, DATE_FORMAT, &tm)) {
		printf("Invalid date format: yyyy-mm-dd hh:mm:ss (e.g.:2007-09-17 18:30:00)\n");
		return false;
	}
	unix_to_nt_time(&nt, mktime(&tm));
	end_date = talloc(mem_ctx, struct FILETIME);
	end_date->dwLowDateTime = (nt << 32) >> 32;
	end_date->dwHighDateTime = (nt >> 32);

	set_SPropValue_proptag(&props[0], PR_CONVERSATION_TOPIC, 
						   (const void *) oclient->subject);
	set_SPropValue_proptag(&props[1], PR_NORMALIZED_SUBJECT, 
						   (const void *) oclient->subject);
	set_SPropValue_proptag(&props[2], PR_START_DATE, (const void *) start_date);
	set_SPropValue_proptag(&props[3], PR_END_DATE, (const void *) end_date);
	set_SPropValue_proptag(&props[4], PR_MESSAGE_CLASS, (const void *)"IPM.Appointment");
	flag = 1;
	set_SPropValue_proptag(&props[5], PR_MESSAGE_FLAGS, (const void *) &flag);
	set_SPropValue_proptag(&props[6], SPropTagArray->aulPropTag[0], (const void *)(oclient->location?oclient->location:""));
	set_SPropValue_proptag(&props[7], SPropTagArray->aulPropTag[1], (const void *) &oclient->busystatus);
	flag= MEETING_STATUS_NONMEETING;
	set_SPropValue_proptag(&props[8], SPropTagArray->aulPropTag[2], (const void *) &flag);
	set_SPropValue_proptag(&props[9], SPropTagArray->aulPropTag[3], (const void *) start_date);
	set_SPropValue_proptag(&props[10], SPropTagArray->aulPropTag[4], (const void *) end_date);
	set_SPropValue_proptag(&props[11], SPropTagArray->aulPropTag[5], (const void *)&oclient->label);
	flag = 30;
	set_SPropValue_proptag(&props[12], SPropTagArray->aulPropTag[6], (const void *)&flag);
	set_SPropValue_proptag(&props[13], PR_BODY, (const void *)(oclient->pr_body?oclient->pr_body:""));
	flag2 = oclient->private;
	set_SPropValue_proptag(&props[14], SPropTagArray->aulPropTag[7], (const void *)&flag2);
	flag = (oclient->private == true) ? 2 : 0;
	set_SPropValue_proptag(&props[15], PR_SENSITIVITY, (const void *)&flag);
	
	retval = SetProps(&obj_message, props, CAL_CNPROPS);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = SaveChangesMessage(&obj_calendar, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_calendar);
	
	return true;
}

static bool openchangeclient_sendcontact(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	struct SPropValue	props[CONTACT_CNPROPS];
	struct mapi_nameid	*nameid;
	struct SPropTagArray	*SPropTagArray;
	mapi_object_t		obj_contact;
	mapi_object_t		obj_message;
	mapi_id_t		id_contact;	

	mapi_object_init(&obj_contact);

	if (oclient->pf == true) {
		retval = openchangeclient_getpfdir(mem_ctx, obj_store, &obj_contact, oclient->folder);
		if (retval != MAPI_E_SUCCESS) return retval;
	} else {
		/* Open Contact default folder */
		retval = GetDefaultFolder(obj_store, &id_contact, olFolderContacts);
		if (retval != MAPI_E_SUCCESS) return false;

		retval = OpenFolder(obj_store, id_contact, &obj_contact);
		if (retval != MAPI_E_SUCCESS) return false;
	}

	/* Create contact mesage */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_contact, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Build the list of named properties we want to set */
	nameid = mapi_nameid_new(mem_ctx);
	mapi_nameid_OOM_add(nameid, "FileAs", PSETID_Address);
	mapi_nameid_lid_add(nameid, 0x8084, PSETID_Address);
	mapi_nameid_OOM_add(nameid, "Email1Address", PSETID_Address);
	mapi_nameid_string_add(nameid, "urn:schemas:contacts:fileas", PS_PUBLIC_STRINGS);

	/* GetIDsFromNames and map property types */
	SPropTagArray = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = GetIDsFromNames(&obj_contact, nameid->count,
				 nameid->nameid, 0, &SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;
	mapi_nameid_SPropTagArray(nameid, SPropTagArray);
	MAPIFreeBuffer(nameid);

	set_SPropValue_proptag(&props[0], SPropTagArray->aulPropTag[0], (const void *)oclient->card_name);
	set_SPropValue_proptag(&props[1], PR_DISPLAY_NAME, (const void *)oclient->full_name);
	set_SPropValue_proptag(&props[2], PR_MESSAGE_CLASS, (const void *)"IPM.Contact");
	set_SPropValue_proptag(&props[3], PR_NORMALIZED_SUBJECT, (const void *)oclient->card_name);
	set_SPropValue_proptag(&props[4], SPropTagArray->aulPropTag[1], (const void *)oclient->email);
	set_SPropValue_proptag(&props[5], SPropTagArray->aulPropTag[2], (const void *)oclient->email);
	set_SPropValue_proptag(&props[6], SPropTagArray->aulPropTag[3], (const void *)oclient->card_name);
	retval = SetProps(&obj_message, props, CONTACT_CNPROPS);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = SaveChangesMessage(&obj_contact, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_contact);

	return true;
}

static bool openchangeclient_sendtask(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	struct SPropValue	props[TASK_CNPROPS];
	mapi_object_t		obj_task;
	mapi_object_t		obj_message;
	mapi_id_t		id_task;	
	struct mapi_nameid	*nameid;
	struct SPropTagArray	*SPropTagArray;
	struct FILETIME		*start_date;
	struct FILETIME		*end_date;
	NTTIME			nt;
	struct tm		tm;

	mapi_object_init(&obj_task);

	if (oclient->pf == true) {
		retval = openchangeclient_getpfdir(mem_ctx, obj_store, &obj_task, oclient->folder);
		if (retval != MAPI_E_SUCCESS) return retval;
	} else {
		/* Open Contact default folder */
		retval = GetDefaultFolder(obj_store, &id_task, olFolderTasks);
		if (retval != MAPI_E_SUCCESS) return false;

		retval = OpenFolder(obj_store, id_task, &obj_task);
		if (retval != MAPI_E_SUCCESS) return false;
	}

	/* Create contact mesage */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_task, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Build the list of named properties we want to set */
	nameid = mapi_nameid_new(mem_ctx);
	mapi_nameid_OOM_add(nameid, "Status", PSETID_Task);
	mapi_nameid_OOM_add(nameid, "StartDate", PSETID_Task);
	mapi_nameid_OOM_add(nameid, "DueDate", PSETID_Task);

	/* GetIDsFromNames and map property types */
	SPropTagArray = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = GetIDsFromNames(&obj_task, nameid->count,
				 nameid->nameid, 0, &SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;
	mapi_nameid_SPropTagArray(nameid, SPropTagArray);
	MAPIFreeBuffer(nameid);

	if (!strptime(oclient->dtstart, DATE_FORMAT, &tm)) {
		printf("Invalid date format (e.g.: 2007-06-01 22:30:00)\n");
		return false;
	}
	unix_to_nt_time(&nt, mktime(&tm));
	start_date = talloc(mem_ctx, struct FILETIME);
	start_date->dwLowDateTime = (nt << 32) >> 32;
	start_date->dwHighDateTime = (nt >> 32);

	if (!strptime(oclient->dtend, DATE_FORMAT, &tm)) {
		printf("Invalid date format (e.g.: 2007-06-01 22:30:00)\n");
		return false;
	}
	unix_to_nt_time(&nt, mktime(&tm));
	end_date = talloc(mem_ctx, struct FILETIME);
	end_date->dwLowDateTime = (nt << 32) >> 32;
	end_date->dwHighDateTime = (nt >> 32);


	set_SPropValue_proptag(&props[0], PR_CONVERSATION_TOPIC, (const void *)oclient->card_name);
	set_SPropValue_proptag(&props[1], PR_NORMALIZED_SUBJECT, (const void *)oclient->card_name);
	set_SPropValue_proptag(&props[2], PR_MESSAGE_CLASS, (const void *)"IPM.Task");
	set_SPropValue_proptag(&props[3], PR_IMPORTANCE, (const void *)&oclient->importance);
	set_SPropValue_proptag(&props[4], PR_BODY, (const void *)oclient->pr_body);
	set_SPropValue_proptag(&props[5], SPropTagArray->aulPropTag[0], (const void *)&oclient->taskstatus);
	set_SPropValue_proptag(&props[6], SPropTagArray->aulPropTag[1], (const void *)start_date);
	set_SPropValue_proptag(&props[7], SPropTagArray->aulPropTag[2], (const void *)end_date);
	retval = SetProps(&obj_message, props, TASK_CNPROPS);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = SaveChangesMessage(&obj_task, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_task);

	return true;
}


static bool openchangeclient_sendnote(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	struct SPropValue	props[NOTE_CNPROPS];
	mapi_object_t		obj_note;
	mapi_object_t		obj_message;
	mapi_id_t		id_note;	
	struct mapi_nameid	*nameid;
	struct SPropTagArray	*SPropTagArray;
	uint32_t		value;

	mapi_object_init(&obj_note);

	if (oclient->pf == true) {
		retval = openchangeclient_getpfdir(mem_ctx, obj_store, &obj_note, oclient->folder);
		if (retval != MAPI_E_SUCCESS) return retval;
	} else {
		/* Open Contact default folder */
		retval = GetDefaultFolder(obj_store, &id_note, olFolderNotes);
		if (retval != MAPI_E_SUCCESS) return false;

		retval = OpenFolder(obj_store, id_note, &obj_note);
		if (retval != MAPI_E_SUCCESS) return false;
	}

	/* Create contact mesage */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_note, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Build the list of named properties we want to set */
	nameid = mapi_nameid_new(mem_ctx);
	mapi_nameid_OOM_add(nameid, "Color", PSETID_Note);
	mapi_nameid_OOM_add(nameid, "Width", PSETID_Note);
	mapi_nameid_OOM_add(nameid, "Height", PSETID_Note);
	mapi_nameid_lid_add(nameid, 0x8510, PSETID_Common);
	/* GetIDsFromNames and map property types */
	SPropTagArray = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = GetIDsFromNames(&obj_note, nameid->count,
				 nameid->nameid, 0, &SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;
	mapi_nameid_SPropTagArray(nameid, SPropTagArray);
	MAPIFreeBuffer(nameid);

	set_SPropValue_proptag(&props[0], PR_CONVERSATION_TOPIC, (const void *)oclient->card_name);
	set_SPropValue_proptag(&props[1], PR_SUBJECT, (const void *)oclient->card_name);
	set_SPropValue_proptag(&props[2], PR_NORMALIZED_SUBJECT, (const void *)oclient->card_name);
	set_SPropValue_proptag(&props[3], PR_MESSAGE_CLASS, (const void *)"IPM.StickyNote");
	value = 1;
	set_SPropValue_proptag(&props[4], PR_MESSAGE_FLAGS, (const void *)&value);
	value = 768;
	set_SPropValue_proptag(&props[5], PR_ICON_INDEX, (const void *)&value);
	set_SPropValue_proptag(&props[6], SPropTagArray->aulPropTag[0], (const void *)&oclient->color);
	set_SPropValue_proptag(&props[7], SPropTagArray->aulPropTag[1], (const void *)&oclient->width);
	set_SPropValue_proptag(&props[8], SPropTagArray->aulPropTag[2], (const void *)&oclient->height);
	value = 272;
	set_SPropValue_proptag(&props[9], SPropTagArray->aulPropTag[3], (const void *)&value);
	
	retval = SetProps(&obj_message, props, NOTE_CNPROPS);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = SaveChangesMessage(&obj_note, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_note);

	return true;
}


static const char *get_container_class(TALLOC_CTX *mem_ctx, mapi_object_t *parent, mapi_id_t folder_id)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_folder;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		count;

	mapi_object_init(&obj_folder);
	retval = OpenFolder(parent, folder_id, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_CONTAINER_CLASS);
	retval = GetProps(&obj_folder, SPropTagArray, &lpProps, &count);
	MAPIFreeBuffer(SPropTagArray);
	if ((lpProps[0].ulPropTag != PR_CONTAINER_CLASS) || (retval != MAPI_E_SUCCESS)) {
		errno = 0;
		return IPF_NOTE;
	}
	return lpProps[0].value.lpszA;
}

static bool get_child_folders(TALLOC_CTX *mem_ctx, mapi_object_t *parent, mapi_id_t folder_id, int count)
{
	enum MAPISTATUS		retval;
	bool			ret;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		rowset;
	const char	       	*name;
	char			*newname;
	const char		*comment;
	const uint32_t		*total;
	const uint32_t		*unread;
	const uint32_t		*child;
	uint32_t		index;
	const uint64_t		*fid;
	int			i;

	mapi_object_init(&obj_folder);
	retval = OpenFolder(parent, folder_id, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x6,
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_COMMENT,
					  PR_CONTENT_UNREAD,
					  PR_CONTENT_COUNT,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;
	
	while ((retval = QueryRows(&obj_htable, 0x32, TBL_ADVANCE, &rowset) != MAPI_E_NOT_FOUND) && rowset.cRows) {
		for (index = 0; index < rowset.cRows; index++) {
			fid = (const uint64_t *)find_SPropValue_data(&rowset.aRow[index], PR_FID);
			name = (const char *)find_SPropValue_data(&rowset.aRow[index], PR_DISPLAY_NAME);
			comment = (const char *)find_SPropValue_data(&rowset.aRow[index], PR_COMMENT);
			total = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_CONTENT_COUNT);
			unread = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_CONTENT_UNREAD);
			child = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_FOLDER_CHILD_COUNT);

			for (i = 0; i < count; i++) {
				printf("|   ");
			}
			newname = utf8tolinux(mem_ctx, name);
			printf("|---+ %-15s : %-20s (Total: %d / Unread: %d - Container class: %s)\n", newname, comment, *total, *unread,
			       get_container_class(mem_ctx, parent, *fid));
			MAPIFreeBuffer(newname);
			if (*child) {
				ret = get_child_folders(mem_ctx, &obj_folder, *fid, count + 1);
				if (ret == false) return ret;
			}
			
		}
	}
	return true;
}

static bool get_child_folders_pf(TALLOC_CTX *mem_ctx, mapi_object_t *parent, mapi_id_t folder_id, int count)
{
	enum MAPISTATUS		retval;
	bool			ret;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		rowset;
	const char	       	*name;
	char			*newname;
	const uint32_t		*child;
	uint32_t		index;
	const uint64_t		*fid;
	int			i;

	mapi_object_init(&obj_folder);
	retval = OpenFolder(parent, folder_id, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x3,
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;
	
	while ((retval = QueryRows(&obj_htable, 0x32, TBL_ADVANCE, &rowset) != MAPI_E_NOT_FOUND) && rowset.cRows) {
		for (index = 0; index < rowset.cRows; index++) {
			fid = (const uint64_t *)find_SPropValue_data(&rowset.aRow[index], PR_FID);
			name = (const char *)find_SPropValue_data(&rowset.aRow[index], PR_DISPLAY_NAME);
			child = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_FOLDER_CHILD_COUNT);

			for (i = 0; i < count; i++) {
				printf("|   ");
			}
			newname = utf8tolinux(mem_ctx, name);
			printf("|---+ %-15s\n", newname);
			MAPIFreeBuffer(newname);
			if (*child) {
				ret = get_child_folders_pf(mem_ctx, &obj_folder, *fid, count + 1);
				if (ret == false) return ret;
			}
			
		}
	}
	return true;
}


static bool openchangeclient_pf(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store)
{
	enum MAPISTATUS			retval;
	mapi_id_t			id_pubroot;

	retval = GetDefaultPublicFolder(obj_store, &id_pubroot, olFolderPublicRoot);
	if (retval != MAPI_E_SUCCESS) return false;

	return get_child_folders_pf(mem_ctx, obj_store, id_pubroot, 0);
}


static bool openchangeclient_mailbox(TALLOC_CTX *mem_ctx, 
				     mapi_object_t *obj_store)
{
	enum MAPISTATUS			retval;
	mapi_id_t			id_mailbox;
	struct SPropTagArray		*SPropTagArray;
	struct SPropValue		*lpProps;
	uint32_t			cValues;
	const char			*mailbox_name;
	char				*utf8_mailbox_name;

	/* Retrieve the mailbox folder name */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = GetProps(obj_store, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;

	if (lpProps[0].value.lpszA) {
		mailbox_name = lpProps[0].value.lpszA;
	} else {
		return false;
	}

	/* Prepare the directory listing */
	retval = GetDefaultFolder(obj_store, &id_mailbox, olFolderTopInformationStore);
	if (retval != MAPI_E_SUCCESS) return false;

	utf8_mailbox_name = utf8tolinux(mem_ctx, mailbox_name);
	printf("+ %s\n", utf8_mailbox_name);
	MAPIFreeBuffer(utf8_mailbox_name);
	return get_child_folders(mem_ctx, obj_store, id_mailbox, 0);
}

static bool openchangeclient_fetchitems(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, const char *item,
					struct oclient *oclient)
{
	enum MAPISTATUS			retval;
	mapi_object_t			obj_folder;
	mapi_object_t			obj_table;
	mapi_object_t			obj_message;
	mapi_id_t			fid;
	uint32_t			olFolder = 0;
	struct SRowSet			SRowSet;
	struct SPropTagArray		*SPropTagArray;
	struct mapi_SPropValue_array	properties_array;
	uint32_t			count;
	int				i;
	
	if (!item) return false;

	mapi_object_init(&obj_folder);

	for (i = 0; defaultFolders[i].olFolder; i++) {
		if (!strncasecmp(defaultFolders[i].container_class, item, strlen(defaultFolders[i].container_class))) {
			olFolder = defaultFolders[i].olFolder;
		}
	}
	
	if (!olFolder) return false;

	if (oclient->pf == true) {
		retval = openchangeclient_getpfdir(mem_ctx, obj_store, &obj_folder, oclient->folder);
		if (retval != MAPI_E_SUCCESS) return retval;
	} else {
		retval = GetDefaultFolder(obj_store, &fid, olFolder);
		if (retval != MAPI_E_SUCCESS) return false;

		/* We now open the folder */
		retval = OpenFolder(obj_store, fid, &obj_folder);
		if (retval != MAPI_E_SUCCESS) return false;
	}

	/* Operations on the  folder */
	mapi_object_init(&obj_table);
	retval = GetContentsTable(&obj_folder, &obj_table);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x8,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT,
					  PR_MESSAGE_CLASS,
					  PR_RULE_MSG_PROVIDER,
					  PR_RULE_MSG_NAME);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = GetRowCount(&obj_table, &count);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	printf("MAILBOX (%d messages)\n", count);

	while ((retval = QueryRows(&obj_table, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND && SRowSet.cRows) {
		count -= SRowSet.cRows;
		for (i = 0; i < SRowSet.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(&obj_folder, 
					     SRowSet.aRow[i].lpProps[0].value.d,
					     SRowSet.aRow[i].lpProps[1].value.d,
					     &obj_message, 0);
			if (retval != MAPI_E_NOT_FOUND) {
				retval = GetPropsAll(&obj_message, &properties_array);
				if (retval == MAPI_E_SUCCESS) {
					mapi_SPropValue_array_named(&obj_message, 
								    &properties_array);
					switch (olFolder) {
					case olFolderInbox:
						mapidump_message(&properties_array);
						break;
					case olFolderCalendar:
						mapidump_appointment(&properties_array);
						break;
					case olFolderContacts:
						mapidump_contact(&properties_array);
						break;
					case olFolderTasks:
						mapidump_task(&properties_array);
						break;
					case olFolderNotes:
						mapidump_note(&properties_array);
						break;
					}
					mapi_object_release(&obj_message);
				}
			}
		}
	}
	
	mapi_object_release(&obj_table);
	mapi_object_release(&obj_folder);

	return true;
}

/**
 * Dump email
 * 
 * Assume msg is received in Inbox folder since we only register
 * notifications for this folder.
 *
 */

static enum MAPISTATUS openchangeclient_findmail(mapi_object_t *obj_store, 
						   mapi_id_t parentID,
						   mapi_id_t msgid)
{
	enum MAPISTATUS			retval;
	TALLOC_CTX			*mem_ctx;
	struct SRowSet			rowset;
	struct SPropValue		*lpProp;
	struct mapi_SPropValue_array	properties_array;
	mapi_id_t			fid;
	const mapi_id_t			*mid;
	mapi_object_t			obj_inbox;
	mapi_object_t			obj_table;
	mapi_object_t			obj_message;
	struct SPropTagArray		*SPropTagArray = NULL;
	uint32_t			count;
	uint32_t			i;

	mem_ctx = talloc_init("openchangeclient_findmail");

	/* Get Inbox folder */
	retval = GetDefaultFolder(obj_store, &fid, olFolderInbox);
	MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx);

	/* Open Inbox */
	mapi_object_init(&obj_inbox);
	retval = OpenFolder(obj_store, fid, &obj_inbox);
	MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx);

	/* Retrieve contents table */
	mapi_object_init(&obj_table);
	retval = GetContentsTable(&obj_inbox, &obj_table);
	MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx);

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2,
					  PR_FID,
					  PR_MID);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx);

	retval = GetRowCount(&obj_table, &count);
	MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx);

	while ((retval = QueryRows(&obj_table, 0xa, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		for (i = 0; i < rowset.cRows; i++) {
			lpProp = get_SPropValue_SRowSet(&rowset, PR_MID);
			if (lpProp != NULL) {
				mid = (const uint64_t *)get_SPropValue(lpProp, PR_MID);
				if (*mid == msgid) {
					mapi_object_init(&obj_message);
					retval = OpenMessage(obj_store,
							     rowset.aRow[i].lpProps[0].value.d,
							     rowset.aRow[i].lpProps[1].value.d,
							     &obj_message, 0);
					if (GetLastError() == MAPI_E_SUCCESS) {
						retval = GetPropsAll(&obj_message, &properties_array);
						if (retval != MAPI_E_SUCCESS) return false;
						mapidump_message(&properties_array);
						mapi_object_release(&obj_message);

						goto end;
					}
					mapi_object_release(&obj_message);
				}
			}
		}
	}
end:
	mapi_object_release(&obj_table);
	mapi_object_release(&obj_inbox);

	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}


static int callback(uint32_t ulEventType, void *notif_data, void *private_data)
{
	struct NEWMAIL_NOTIFICATION	*newmail;
	enum MAPISTATUS			retval;

	switch(ulEventType) {
	case fnevNewMail:
		printf("[+]New mail Received!!!!\n");
		newmail = (struct NEWMAIL_NOTIFICATION *)notif_data;
		mapidump_newmail(newmail, "\t");
		retval = openchangeclient_findmail((mapi_object_t *)private_data,
						   newmail->lpParentID,
						   newmail->lpEntryID);
		mapi_errstr("openchangeclient_findmail", GetLastError());
		break;
	case fnevObjectCreated:
		printf("[+]Object Created!!!\n");
		break;
	default:
		printf("[+] Unsupported notification (%d)\n", ulEventType);
		break;
	}

	return 0;
}


static bool openchangeclient_notifications(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS	retval;
	mapi_object_t	obj_inbox;
	mapi_id_t	fid;
	uint32_t	ulConnection;
	uint32_t	ulEventMask;

	/* Register notification */
	retval = RegisterNotification(0);
	if (retval != MAPI_E_SUCCESS) return false;

	if (oclient->pf == true) {
		retval = openchangeclient_getpfdir(mem_ctx, obj_store, &obj_inbox, oclient->folder);
		if (retval != MAPI_E_SUCCESS) return retval;
	} else {
		/* Retrieve Inbox folder ID */
		retval = GetDefaultFolder(obj_store, &fid, olFolderInbox);
		if (retval != MAPI_E_SUCCESS) return false;

		/* Open Inbox folder */
		mapi_object_init(&obj_inbox);
		retval = OpenFolder(obj_store, fid, &obj_inbox);
		if (retval != MAPI_E_SUCCESS) return false;
	}

	/* subscribe Inbox to receive newmail notifications */
	ulEventMask = fnevNewMail;
	retval = Subscribe(&obj_inbox, &ulConnection, ulEventMask, (mapi_notify_callback_t)callback);
	if (retval != MAPI_E_SUCCESS) return false;

	/* wait for notifications: infinite loop */
	retval = MonitorNotification((void *)obj_store);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = Unsubscribe(ulConnection);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_release(&obj_inbox);

	return true;
}


static bool openchangeclient_mkdir(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_child;
	mapi_object_t		obj_tis;
	mapi_id_t		id_inbox;

	mapi_object_init(&obj_tis);
	mapi_object_init(&obj_folder);
	mapi_object_init(&obj_child);

	if (oclient->folder) {
		retval = GetDefaultFolder(obj_store, &id_inbox, olFolderTopInformationStore);
		if (retval != MAPI_E_SUCCESS) return false;

		retval = OpenFolder(obj_store, id_inbox, &obj_tis);
		if (retval != MAPI_E_SUCCESS) return false;

		retval = openchangeclient_getdir(mem_ctx, &obj_tis, &obj_folder, oclient->folder);
		if (retval != MAPI_E_SUCCESS) return false;
	} else {
		retval = GetDefaultFolder(obj_store, &id_inbox, olFolderInbox);
		if (retval != MAPI_E_SUCCESS) return false;

		retval = OpenFolder(obj_store, id_inbox, &obj_folder);
		if (retval != MAPI_E_SUCCESS) return false;
	}

	retval = CreateFolder(&obj_folder, FOLDER_GENERIC, oclient->folder_name, 
			      oclient->folder_comment, OPEN_IF_EXISTS, &obj_child);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_release(&obj_child);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_tis);

	return true;
}


static bool openchangeclient_rmdir(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_child;
	mapi_object_t		obj_tis;
	mapi_id_t		id_inbox;

	mapi_object_init(&obj_tis);
	mapi_object_init(&obj_folder);
	mapi_object_init(&obj_child);

	if (oclient->folder) {
		printf("Removing folder within %s\n", oclient->folder);
		retval = GetDefaultFolder(obj_store, &id_inbox, olFolderTopInformationStore);
		if (retval != MAPI_E_SUCCESS) return false;
		
		retval = OpenFolder(obj_store, id_inbox, &obj_tis);
		if (retval != MAPI_E_SUCCESS) return false;

		retval = openchangeclient_getdir(mem_ctx, &obj_tis, &obj_folder, oclient->folder);
		if (retval != MAPI_E_SUCCESS) return false;
	} else {
		retval = GetDefaultFolder(obj_store, &id_inbox, olFolderInbox);
		if (retval != MAPI_E_SUCCESS) return false;
		
		retval = OpenFolder(obj_store, id_inbox, &obj_folder);
		if (retval != MAPI_E_SUCCESS) return false;
	}
	
	retval = openchangeclient_getdir(mem_ctx, &obj_folder, &obj_child, oclient->folder_name);
	if (retval != MAPI_E_SUCCESS) return false;
	
	retval = EmptyFolder(&obj_child);
	if (retval != MAPI_E_SUCCESS) return false;
	
	printf("obj_child fid = 0x%llx\n", mapi_object_get_id(&obj_child));

	retval = DeleteFolder(&obj_folder, mapi_object_get_id(&obj_child));
	if (retval != MAPI_E_SUCCESS) return false;
	
	mapi_object_release(&obj_child);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_tis);
	
	return true;
}

static bool openchangeclient_userlist(TALLOC_CTX *mem_ctx, struct oclient *oclient)
{
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		*SRowSet;
	enum MAPISTATUS		retval;
	uint32_t		i;
	uint32_t		count;
	uint8_t			ulFlags;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0xc,
					  PR_INSTANCE_KEY,
					  PR_ENTRYID,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_EMAIL_ADDRESS_UNICODE,
					  PR_DISPLAY_TYPE,
					  PR_OBJECT_TYPE,
					  PR_ADDRTYPE_UNICODE,
					  PR_OFFICE_TELEPHONE_NUMBER_UNICODE,
					  PR_OFFICE_LOCATION_UNICODE,
					  PR_TITLE_UNICODE,
					  PR_COMPANY_NAME_UNICODE,
					  PR_ACCOUNT_UNICODE);

	count = 0x7;
	ulFlags = TABLE_START;
	do {
		count += 0x2;
		retval = GetGALTable(SPropTagArray, &SRowSet, count, ulFlags);
		if (SRowSet->cRows) {
			for (i = 0; i < SRowSet->cRows; i++) {
				mapidump_PAB_entry(&SRowSet->aRow[i]);
			}
		}
		ulFlags = TABLE_CUR;
		MAPIFreeBuffer(SRowSet);
	} while (SRowSet->cRows == count);
	mapi_errstr("GetPABTable", GetLastError());

	MAPIFreeBuffer(SPropTagArray);

	return true;
}

static void list_argument(const char *label, struct oc_element *oc_items)
{
	uint32_t	i;

	printf("Use one of the following %s values:\n", label);
	for (i = 0; oc_items[i].status; i++) {
		printf("%s\n", oc_items[i].status);
	}
}


static uint32_t oc_get_argument(const char *name, struct oc_element *oc_items, const char *label)
{
	uint32_t	i;

	for (i = 0; oc_items[i].status; i++) {
		if (!strncasecmp(oc_items[i].status, name, strlen(oc_items[i].status))) {
			return oc_items[i].index;
		}
	}
	list_argument(label, oc_items);
	exit (1);
}


int main(int argc, const char *argv[])
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct mapi_session	*session = NULL;
	mapi_object_t		obj_store;
	struct oclient		oclient;
	poptContext		pc;
	int			opt;
	bool			opt_sendmail = false;
	bool			opt_sendappointment = false;
	bool			opt_sendcontact = false;
	bool			opt_sendtask = false;
	bool			opt_sendnote = false;
	bool			opt_fetchmail = false;
	bool			opt_deletemail = false;
	bool			opt_mailbox = false;
	bool			opt_dumpdata = false;
	bool			opt_notifications = false;
	bool			opt_mkdir = false;
	bool			opt_rmdir = false;
	bool			opt_userlist = false;
	const char		*opt_profdb = NULL;
	const char		*opt_profname = NULL;
	const char		*opt_password = NULL;
	const char		*opt_attachments = NULL;
	const char		*opt_fetchitems = NULL;
	const char		*opt_html_file = NULL;
	const char		*opt_mapi_to = NULL;
	const char		*opt_mapi_cc = NULL;
	const char		*opt_mapi_bcc = NULL;
	const char		*opt_debug = NULL;

	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_SENDMAIL, OPT_PASSWORD, OPT_SENDAPPOINTMENT, 
	      OPT_SENDCONTACT, OPT_SENDTASK, OPT_FETCHMAIL, OPT_STOREMAIL,  OPT_DELETEMAIL, 
	      OPT_ATTACH, OPT_HTML_INLINE, OPT_HTML_FILE, OPT_MAPI_TO, OPT_MAPI_CC, 
	      OPT_MAPI_BCC, OPT_MAPI_SUBJECT, OPT_MAPI_BODY, OPT_MAILBOX, 
	      OPT_FETCHITEMS, OPT_MAPI_LOCATION, OPT_MAPI_STARTDATE, OPT_MAPI_ENDDATE, 
	      OPT_MAPI_BUSYSTATUS, OPT_NOTIFICATIONS, OPT_DEBUG, OPT_DUMPDATA, 
	      OPT_MAPI_EMAIL, OPT_MAPI_FULLNAME, OPT_MAPI_CARDNAME, OPT_MAPI_PRIORITY,
	      OPT_MAPI_TASKSTATUS, OPT_MAPI_IMPORTANCE, OPT_MAPI_LABEL, OPT_PF, 
	      OPT_FOLDER, OPT_MAPI_COLOR, OPT_SENDNOTE, OPT_MKDIR, OPT_RMDIR,
	      OPT_FOLDER_NAME, OPT_FOLDER_COMMENT, OPT_USERLIST, OPT_MAPI_PRIVATE};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path"},
		{"pf", 0, POPT_ARG_NONE, NULL, OPT_PF, "access public folders instead of mailbox"},
		{"profile", 'p', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name"},
		{"password", 'P', POPT_ARG_STRING, NULL, OPT_PASSWORD, "set the profile password"},
		{"sendmail", 'S', POPT_ARG_NONE, NULL, OPT_SENDMAIL, "send a mail"},
		{"sendappointment", 0, POPT_ARG_NONE, NULL, OPT_SENDAPPOINTMENT, "send an appointment"},
		{"sendcontact", 0, POPT_ARG_NONE, NULL, OPT_SENDCONTACT, "send a contact"},
		{"sendtask", 0, POPT_ARG_NONE, NULL, OPT_SENDTASK, "send a task"},
		{"sendnote", 0, POPT_ARG_NONE, NULL, OPT_SENDNOTE, "send a note"},
		{"fetchmail", 'F', POPT_ARG_NONE, NULL, OPT_FETCHMAIL, "fetch user INBOX mails"},
		{"storemail", 'G', POPT_ARG_STRING, NULL, OPT_STOREMAIL, "retrieve a mail on the filesystem"},
		{"fetch-items", 'i', POPT_ARG_STRING, NULL, OPT_FETCHITEMS, "fetch specified user INBOX items"},
		{"mailbox", 'm', POPT_ARG_NONE, NULL, OPT_MAILBOX, "list mailbox folder summary"},
		{"deletemail", 'D', POPT_ARG_NONE, NULL, OPT_DELETEMAIL, "delete a mail from user INBOX"},
		{"attachments", 'A', POPT_ARG_STRING, NULL, OPT_ATTACH, "send a list of attachments"},
		{"html-inline", 'I', POPT_ARG_STRING, NULL, OPT_HTML_INLINE, "send PR_HTML content"},
		{"html-file", 'W', POPT_ARG_STRING, NULL, OPT_HTML_FILE, "use HTML file as content"},
		{"to", 't', POPT_ARG_STRING, NULL, OPT_MAPI_TO, "To recipients"},
		{"cc", 'c', POPT_ARG_STRING, NULL, OPT_MAPI_CC, "Cc recipients"},
		{"bcc", 'b', POPT_ARG_STRING, NULL, OPT_MAPI_BCC, "Bcc recipients"},
		{"subject", 's', POPT_ARG_STRING, NULL, OPT_MAPI_SUBJECT, "Mail subject"},
		{"body", 'B', POPT_ARG_STRING, NULL, OPT_MAPI_BODY, "Mail body"},
		{"location", 0, POPT_ARG_STRING, NULL, OPT_MAPI_LOCATION, "Set the item location"},
		{"label", 0, POPT_ARG_STRING, NULL, OPT_MAPI_LABEL, "Set the event label"},
		{"dtstart", 0, POPT_ARG_STRING, NULL, OPT_MAPI_STARTDATE, "Set the event start date"},
		{"dtend", 0, POPT_ARG_STRING, NULL, OPT_MAPI_ENDDATE, "set the event end date"},
		{"busystatus", 0, POPT_ARG_STRING, NULL, OPT_MAPI_BUSYSTATUS, "set the item busy status"},
		{"taskstatus", 0, POPT_ARG_STRING, NULL, OPT_MAPI_TASKSTATUS, "set the task status"},
		{"priority", 0, POPT_ARG_STRING, NULL, OPT_MAPI_PRIORITY, "set the item priority"},
		{"importance", 0, POPT_ARG_STRING, NULL, OPT_MAPI_IMPORTANCE, "Set the item importance"},
		{"email", 0, POPT_ARG_STRING, NULL, OPT_MAPI_EMAIL, "set the email address"},
		{"fullname", 0, POPT_ARG_STRING, NULL, OPT_MAPI_FULLNAME, "set the full name"},
		{"cardname", 0, POPT_ARG_STRING, NULL, OPT_MAPI_CARDNAME, "set a contact card name"},
		{"color", 0, POPT_ARG_STRING, NULL, OPT_MAPI_COLOR, "set the note color"},
		{"notifications", 0, POPT_ARG_NONE, NULL, OPT_NOTIFICATIONS, "monitor INBOX newmail notifications"},
		{"folder", 0, POPT_ARG_STRING, NULL, OPT_FOLDER, "set the folder to use instead of inbox"},
		{"mkdir", 0, POPT_ARG_NONE, NULL, OPT_MKDIR, "create a folder"},
		{"rmdir", 0, POPT_ARG_NONE, NULL, OPT_RMDIR, "delete a folder"},
		{"userlist", 0, POPT_ARG_NONE, NULL, OPT_USERLIST, "list PAB entries"},
		{"folder-name", 0, POPT_ARG_STRING, NULL, OPT_FOLDER_NAME, "set the folder name"},
		{"folder-comment", 0, POPT_ARG_STRING, NULL, OPT_FOLDER_COMMENT, "set the folder comment"},
		{"debuglevel", 0, POPT_ARG_STRING, NULL, OPT_DEBUG, "Set Debug Level"},
		{"dump-data", 0, POPT_ARG_NONE, NULL, OPT_DUMPDATA, "dump the hex data"},
		{"private", 0, POPT_ARG_NONE, NULL, OPT_MAPI_PRIVATE, "Set the private flag on messages"},
		{ NULL }
	};

	mem_ctx = talloc_init("openchangeclient");

	init_oclient(&oclient);

	pc = poptGetContext("openchangeclient", argc, argv, long_options, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_PF:
			oclient.pf = true;
			break;
		case OPT_FOLDER:
			oclient.folder = poptGetOptArg(pc);
			break;
		case OPT_DEBUG:
			opt_debug = poptGetOptArg(pc);
			break;
		case OPT_DUMPDATA:
			opt_dumpdata = true;
			break;
		case OPT_USERLIST:
			opt_userlist = true;
			break;
		case OPT_MKDIR:
			opt_mkdir = true;
			break;
		case OPT_RMDIR:
			opt_rmdir = true;
			break;
		case OPT_FOLDER_NAME:
			oclient.folder_name = poptGetOptArg(pc);
			break;
		case OPT_FOLDER_COMMENT:
			oclient.folder_comment = poptGetOptArg(pc);
			break;
		case OPT_NOTIFICATIONS:
			opt_notifications = true;
			break;
		case OPT_PROFILE_DB:
			opt_profdb = poptGetOptArg(pc);
			break;
		case OPT_PROFILE:
			opt_profname = poptGetOptArg(pc);
			break;
		case OPT_PASSWORD:
			opt_password = poptGetOptArg(pc);
			break;
		case OPT_MAILBOX:
			opt_mailbox = true;
			break;
		case OPT_FETCHITEMS:
			opt_fetchitems = poptGetOptArg(pc);
			break;
		case OPT_SENDMAIL:
			opt_sendmail = true;
			break;
		case OPT_SENDAPPOINTMENT:
			opt_sendappointment = true;
			break;
		case OPT_SENDCONTACT:
			opt_sendcontact = true;
			break;
		case OPT_SENDTASK:
			opt_sendtask = true;
			break;
		case OPT_SENDNOTE:
			opt_sendnote = true;
			break;
		case OPT_FETCHMAIL:
			opt_fetchmail = true;
			break;
		case OPT_STOREMAIL:
			oclient.store_folder = poptGetOptArg(pc);
			break;
		case OPT_DELETEMAIL:
			opt_deletemail = true;
			break;
		case OPT_ATTACH:
			opt_attachments = poptGetOptArg(pc);
			break;
		case OPT_HTML_INLINE:
			oclient.pr_html_inline = poptGetOptArg(pc);
			break;
		case OPT_HTML_FILE:
			opt_html_file = poptGetOptArg(pc);
			break;
		case OPT_MAPI_TO:
			opt_mapi_to = poptGetOptArg(pc);
			break;
		case OPT_MAPI_CC:
			opt_mapi_cc = poptGetOptArg(pc);
			break;
		case OPT_MAPI_BCC:
			opt_mapi_bcc = poptGetOptArg(pc);
			break;
		case OPT_MAPI_SUBJECT:
			oclient.subject = poptGetOptArg(pc);
			break;
		case OPT_MAPI_BODY:
			oclient.pr_body = poptGetOptArg(pc);
			break;
		case OPT_MAPI_LOCATION:
			oclient.location = poptGetOptArg(pc);
			break;
		case OPT_MAPI_STARTDATE:
			oclient.dtstart = poptGetOptArg(pc);
			break;
		case OPT_MAPI_ENDDATE:
			oclient.dtend = poptGetOptArg(pc);
			break;
		case OPT_MAPI_BUSYSTATUS:
			oclient.busystatus = oc_get_argument(poptGetOptArg(pc),
							     oc_busystatus,
							     "busystatus");
			break;
		case OPT_MAPI_LABEL:
			oclient.label = oc_get_argument(poptGetOptArg(pc),
							oc_label,
							"label");
			break;
		case OPT_MAPI_PRIORITY:
			oclient.priority = oc_get_argument(poptGetOptArg(pc),
							   oc_priority,
							   "priority");
			break;
		case OPT_MAPI_IMPORTANCE:
			oclient.importance = oc_get_argument(poptGetOptArg(pc), 
							     oc_importance, 
							     "importance");
			break;
		case OPT_MAPI_TASKSTATUS:
			oclient.taskstatus = oc_get_argument(poptGetOptArg(pc),
							     oc_taskstatus,
							     "taskstatus");
			break;
		case OPT_MAPI_COLOR:
			oclient.color = oc_get_argument(poptGetOptArg(pc),
							oc_color,
							"color");
			break;
		case OPT_MAPI_EMAIL:
			oclient.email = poptGetOptArg(pc);
			break;
		case OPT_MAPI_FULLNAME:
			oclient.full_name = poptGetOptArg(pc);
			break;
		case OPT_MAPI_CARDNAME:
			oclient.card_name = poptGetOptArg(pc);
			break;
		case OPT_MAPI_PRIVATE:
			oclient.private = true;
			break;
		}
	}

	/* Sanity check on options */

	if (!opt_profdb) {
		opt_profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
	}

	if (opt_sendmail && !opt_mapi_to && !opt_mapi_cc && !opt_mapi_bcc) {
		printf("You need to specify at least one recipient\n");
		exit (1);
	}

	if (opt_sendmail && (!oclient.pr_body && !oclient.pr_html_inline && !opt_html_file)) {
		printf("No body specified (body, html-inline or html-file)\n");
		exit (1);
	}

	if ((opt_sendmail && oclient.pf == true && !oclient.folder) ||
	    (oclient.pf == true && !oclient.folder)) {
		printf("--folder option is mandatory\n");
		exit (1);
	}

	if (opt_html_file && oclient.pr_body) {
		printf("PR_BODY and PR_HTML can't be set at the same time\n");
		exit (1);
	}

	if (oclient.pr_body && oclient.pr_html_inline) {
		printf("Inline HTML and PR_BODY content can't be set simulteanously\n");
		exit (1);
	}

	if (opt_html_file && oclient.pr_html_inline) {
		printf("PR_HTML from file and stdin can't be specified at the same time\n");
		exit (1);
	}
	
	if (opt_html_file) {
		oclient_read_file(mem_ctx, opt_html_file, &oclient, PR_HTML);
	}

	if (opt_attachments) {
		if (oclient_parse_attachments(mem_ctx, opt_attachments, &oclient) == false) {
			printf("Unable to parse one of the specified attachments\n");
			exit (1);
		}
	}

	if (opt_mkdir && !oclient.folder_name) {
		printf("mkdir requires --folder-name to be defined\n");
		exit (1);
	}

	/**
	 * Initialize MAPI subsystem
	 */

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
	
	/* If no profile is specified try to load the default one from
	 * the database 
	 */

	if (!opt_profname) {
		retval = GetDefaultProfile(&opt_profname);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", GetLastError());
			exit (1);
		}
	}

	retval = MapiLogonEx(&session, opt_profname, opt_password);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", GetLastError());
		exit (1);
	}

	if (opt_userlist) {
		retval = openchangeclient_userlist(mem_ctx, &oclient);
		exit (0);
	}

	/**
	 * Open Default Message Store
	 */

	mapi_object_init(&obj_store);
	if (oclient.pf == true) {
		retval = OpenPublicFolder(&obj_store);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("OpenPublicFolder", GetLastError());
			exit (1);
		}
	} else {
		retval = OpenMsgStore(&obj_store);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("OpenMsgStore", GetLastError());
			exit (1);
		}
	}

	if (opt_fetchitems) {
		retval = openchangeclient_fetchitems(mem_ctx, &obj_store, opt_fetchitems, &oclient);
		mapi_errstr("fetchitems", GetLastError());
		if (retval != true) {
			goto end;
		}
	}

	if (opt_mailbox) {
		if (oclient.pf == true) {
			retval = openchangeclient_pf(mem_ctx, &obj_store);
			mapi_errstr("public folder", GetLastError());
			if (retval != true) {
				goto end;
			}
		} else {
			retval = openchangeclient_mailbox(mem_ctx, &obj_store);
			mapi_errstr("mailbox", GetLastError());
			if (retval != true) {
				goto end;
			}
		}
	}

	/* MAPI email operations */
	if (opt_sendmail) {
		/* recipients management */
		oclient.mapi_to = get_cmdline_recipients(mem_ctx, opt_mapi_to);
		oclient.mapi_cc = get_cmdline_recipients(mem_ctx, opt_mapi_cc);
		oclient.mapi_bcc = get_cmdline_recipients(mem_ctx, opt_mapi_bcc);

		retval = openchangeclient_sendmail(mem_ctx, &obj_store, &oclient);
		mapi_errstr("sendmail", GetLastError());
		if (retval != true) {
			goto end;
		}
	}

	if (opt_fetchmail) {
		retval = openchangeclient_fetchmail(&obj_store, &oclient);
		mapi_errstr("fetchmail", GetLastError());
		if (retval != true) {
			goto end;
		}
	}

	if (opt_deletemail) {
		retval = openchangeclient_deletemail(mem_ctx, &obj_store, &oclient);
		mapi_errstr("deletemail", GetLastError());
		if (retval != true) {
			goto end;
		}
	}

	/* MAPI calendar operations */
	if (opt_sendappointment) {
		if (!oclient.dtstart) {
			printf("You need to specify a start date (e.g: 2007-06-01 22:30:00)\n");
			goto end;
		}
		
		if (!oclient.dtend) {
			printf("Setting default end date\n");
			oclient.dtend = oclient.dtstart;
		}

		retval = openchangeclient_sendappointment(mem_ctx, &obj_store, &oclient);
		mapi_errstr("sendappointment", GetLastError());
		if (retval != true) {
			goto end;
		}
	}

	/* MAPI contact operations */
	if (opt_sendcontact) {
		retval = openchangeclient_sendcontact(mem_ctx, &obj_store, &oclient);
		mapi_errstr("sendcontact", GetLastError());
		if (retval != true) {
			goto end;
		}
	}

	/* MAPI task operations */
	if (opt_sendtask) {
		if (!oclient.dtstart) {
			printf("You need to specify a start date (e.g: 2007-06-01 22:30:00)\n");
			goto end;
		}
		
		if (!oclient.dtend) {
			printf("Setting default end date\n");
			oclient.dtend = oclient.dtstart;
		}

		retval = openchangeclient_sendtask(mem_ctx, &obj_store, &oclient);
		mapi_errstr("sentask", GetLastError());
		if (retval != true) {
			goto end;
		}
	}

	/* MAPI note operations */
	if (opt_sendnote) {
		retval = openchangeclient_sendnote(mem_ctx, &obj_store, &oclient);
		mapi_errstr("sendnote", GetLastError());
		if (retval != true) {
			goto end;
		}
		
	}
	
	/* Monitor newmail notifications */
	if (opt_notifications) {
		openchangeclient_notifications(mem_ctx, &obj_store, &oclient);
		mapi_errstr("notifications", GetLastError());
		if (retval != true) {
			goto end;
		}
	}

	/* Folder operations */
	if (opt_mkdir) {
		openchangeclient_mkdir(mem_ctx, &obj_store, &oclient);
		mapi_errstr("mkdir", GetLastError());
		if (retval != true) {
			goto end;
		}
	}

	if (opt_rmdir) {
		openchangeclient_rmdir(mem_ctx, &obj_store, &oclient);
		mapi_errstr("rmdir", GetLastError());
		if (retval != true) {
			goto end;
		}
	}

	/* Uninitialize MAPI subsystem */
end:
	mapi_object_release(&obj_store);

	MAPIUninitialize();

	talloc_free(mem_ctx);

	return 0;
}
