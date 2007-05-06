/*
   Stand-alone MAPI application

   OpenChange Project

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
#include <samba/popt.h>

#include "openchangeclient.h"

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

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
}

/**
 * read a file and store it in the appropriate structure element
 */

static BOOL oclient_read_file(TALLOC_CTX *mem_ctx, const char *filename, 
			      struct oclient *oclient, uint32_t mapitag)
{
	struct stat	sb;
	int		fd;

	/* stat the file */
	if (stat(filename, &sb) != 0) return False;
	if ((fd = open(filename, O_RDONLY)) == -1) {
		printf("Error while opening %s\n", filename);
		return False;
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
			return False;
		}
		oclient->attach[oclient->attach_num].fd = fd;
		printf("filename = %s (size = %d / %d)\n", filename, oclient->attach[oclient->attach_num].bin.cb, (uint32_t)sb.st_size);
		close(fd);
		break;
	default:
		printf("unsupported MAPITAG: %s\n", get_proptag_name(mapitag));
		close(fd);
		return False;
		break;
	}

	return True;
}

/**
 * Parse attachments and load their content
 */
static BOOL oclient_parse_attachments(TALLOC_CTX *mem_ctx, const char *filename,
				      struct oclient *oclient)
{
	char		**filenames;
	char		*tmp = NULL;
	uint32_t	j;

	if ((tmp = strtok((char *)filename, ";")) == NULL) {
		printf("Invalid string format [;]\n");
		return False;
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
		if (oclient_read_file(mem_ctx, filenames[j], oclient, PR_ATTACH_DATA_BIN) == False) {
			return False;
		}
	}

	return True;
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

#define	MAX_READ_SIZE	0x4000

static BOOL store_attachment(mapi_object_t obj_attach, const char *filename, uint32_t size, struct oclient *oclient)
{
	TALLOC_CTX	*mem_ctx;
	enum MAPISTATUS	retval;
	char		*path;
	mapi_object_t	obj_stream;
	uint32_t	stream_size;
	uint32_t	read_size;
	int		fd;
	unsigned char  	buf[MAX_READ_SIZE];
	uint32_t	max_read_size = MAX_READ_SIZE;

	if (!filename || !size) return False;

	mem_ctx = talloc_init("store_attachment");
	mapi_object_init(&obj_stream);

	if ((fd = open(oclient->store_folder, O_DIRECTORY)) == -1) {
		if (mkdir(oclient->store_folder, 0700) == -1) return False;
	} else {
		close (fd);
	}

	path = talloc_asprintf(mem_ctx, "%s/%s", oclient->store_folder, filename);
	if ((fd = open(path, O_CREAT|O_WRONLY)) == -1) return False;

	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 0, &obj_stream);
	if (retval != MAPI_E_SUCCESS) return False;

	if (size < MAX_READ_SIZE) {
		retval = ReadStream(&obj_stream, buf, size, &read_size);
		if (retval != MAPI_E_SUCCESS) goto error;
		write(fd, buf, read_size);
		close(fd);
	}

	for (stream_size = 0; stream_size < size; stream_size += 0x4000) {
		retval = ReadStream(&obj_stream, buf, max_read_size, &read_size);
		if (retval != MAPI_E_SUCCESS) goto error;
		write(fd, buf, read_size);
	}
	close(fd);

	mapi_object_release(&obj_stream);
	close(fd);
	talloc_free(mem_ctx);
	return True;

error:
	mapi_object_release(&obj_stream);
	close(fd);
	talloc_free(mem_ctx);
	return False;
}

static enum MAPISTATUS openchangeclient_fetchmail(struct mapi_session *session, 
						  mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS			retval;
	BOOL				status;
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
	struct mapi_SPropValue_array	properties_array;
	uint32_t			i, j;
	uint32_t			count;
	uint8_t				*has_attach;
	uint32_t			*attach_num;
	const char			*attach_filename;
	uint32_t			*attach_size;
	
	mem_ctx = talloc_init("openchangeclient_fetchmail");

	mapi_object_init(&obj_inbox);
	mapi_object_init(&obj_table);

	retval = GetReceiveFolder(obj_store, &id_inbox);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	retval = OpenFolder(obj_store, id_inbox, &obj_inbox);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

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

	while ((retval = QueryRows(&obj_table, 0xa, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		for (i = 0; i < rowset.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(obj_store,
					     rowset.aRow[i].lpProps[0].value.d,
					     rowset.aRow[i].lpProps[1].value.d,
					     &obj_message);
			if (GetLastError() == MAPI_E_SUCCESS) {
				retval = GetPropsAll(&obj_message, &properties_array);
				if (retval != MAPI_E_SUCCESS) return False;
				has_attach = (uint8_t *) find_mapi_SPropValue_data(&properties_array, PR_HASATTACH);
				
				mapidump_message(&properties_array);

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
						if (retval != MAPI_E_SUCCESS) return False;

						for (j = 0; j < rowset_attach.cRows; j++) {
							attach_num = (uint32_t *)find_SPropValue_data(&(rowset_attach.aRow[j]), PR_ATTACH_NUM);
							retval = OpenAttach(&obj_message, *attach_num, &obj_attach);
							if (retval == MAPI_E_SUCCESS) {
								fflush(0);
								retval = GetPropsAll(&obj_attach, &properties_array);
								attach_filename = get_filename((char *)find_mapi_SPropValue_data(&properties_array, PR_ATTACH_FILENAME));
								attach_size = (uint32_t *)find_mapi_SPropValue_data(&properties_array, PR_ATTACH_SIZE);
								printf("[%d] %s (%d Bytes)\n", j, attach_filename, attach_size?*attach_size:0);
								fflush(0);
								if (oclient->store_folder) {
									status = store_attachment(obj_attach, attach_filename, *attach_size, oclient);
									if (status == False) {
										printf("A Problem was encountered while storing attachments on the filesystem\n");
										MAPI_RETVAL_IF(status == False, MAPI_E_UNABLE_TO_COMPLETE, mem_ctx);

									}
								}
							}
						}
						errno = 0;
					}
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

static BOOL set_usernames_RecipientType(uint32_t *index, struct SRowSet *rowset, char **usernames, struct FlagList *flaglist,
					enum ulRecipClass RecipClass)
{
	uint32_t	i;
	uint32_t	count = *index;

	if (!usernames) return False;

	for (i = 0; usernames[i]; i++) {
		if (flaglist->ulFlags[count] == MAPI_RESOLVED) {
			SetRecipientType(&(rowset->aRow[count]), RecipClass);
			count++;
		}
	}
	
	*index = count;
	
	return True;
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

#define	MAX_READ_SIZE	0x4000

static BOOL openchangeclient_stream(TALLOC_CTX *mem_ctx, mapi_object_t obj_parent, 
				    mapi_object_t obj_stream, uint32_t mapitag, 
				    uint32_t access_flags, struct SBinary bin)
{
	enum MAPISTATUS	retval;
	DATA_BLOB	stream;
	uint32_t	size;
	uint32_t	offset;

	/* Open a stream on the parent for the given property */
	retval = OpenStream(&obj_parent, mapitag, access_flags, &obj_stream);
	if (retval != MAPI_E_SUCCESS) return False;

	/* WriteStream operation */
	printf("We are about to write %d bytes in the stream\n", bin.cb);
	if (bin.cb <= MAX_READ_SIZE) {
		stream.length = bin.cb;
		stream.data = talloc_size(mem_ctx, bin.cb);
		memcpy(stream.data, bin.lpb, bin.cb);
		retval = WriteStream(&obj_stream, &stream);
		talloc_free(stream.data);
		if (retval != MAPI_E_SUCCESS) return False;

		return True;
	} else {
		for (size = 0, offset = - MAX_READ_SIZE; size <= bin.cb; size += MAX_READ_SIZE) {
			offset += MAX_READ_SIZE;
			stream.length = MAX_READ_SIZE;
			stream.data = talloc_size(mem_ctx, MAX_READ_SIZE);
			memcpy(stream.data, bin.lpb + offset, MAX_READ_SIZE);

			errno = 0;
			retval = WriteStream(&obj_stream, &stream);
			printf(".");
			fflush(0);
			if (retval != MAPI_E_SUCCESS) return False;
			talloc_free(stream.data);
		}
		if (size > bin.cb) {
			size = bin.cb - offset;		
			stream.length = size;
			stream.data = talloc_size(mem_ctx, size);
			memcpy(stream.data, bin.lpb + offset, size);
			
			errno = 0;
			retval = WriteStream(&obj_stream, &stream);
			printf(".\n");
			fflush(0);
			if (retval != MAPI_E_SUCCESS) return False;
			talloc_free(stream.data);
		}
	}

	return True;
}

#define SETPROPS_COUNT	3

/**
 * Send a mail
 */

static enum MAPISTATUS openchangeclient_sendmail(TALLOC_CTX *mem_ctx, 
						 struct mapi_session *session, 
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

	mapi_object_init(&obj_outbox);
	mapi_object_init(&obj_message);

	/* Get outbox folder */
	retval = GetOutboxFolder(obj_store, &id_outbox);
	if (retval != MAPI_E_SUCCESS) return retval;

	/* Open outbox folder */
	retval = OpenFolder(obj_store, id_outbox, &obj_outbox);
	if (retval != MAPI_E_SUCCESS) return retval;

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
	if (retval != MAPI_E_SUCCESS) return False;

	if (!SRowSet) {
		printf("None of the recipients were resolved\n");
		MAPI_RETVAL_IF(1, MAPI_E_NO_RECIPIENTS, NULL);
	}

	SPropValue.ulPropTag = PR_SEND_INTERNET_ENCODING;
	SPropValue.value.l = 0;
	SRowSet_propcpy(mem_ctx, SRowSet, SPropValue);

	set_usernames_RecipientType(&index, SRowSet, oclient->mapi_to, flaglist, MAPI_TO);
	set_usernames_RecipientType(&index, SRowSet, oclient->mapi_cc, flaglist, MAPI_CC);
	set_usernames_RecipientType(&index, SRowSet, oclient->mapi_bcc, flaglist, MAPI_BCC);

	/* ModifyRecipients operation */
	retval = ModifyRecipients(&obj_message, SRowSet);
	MAPIFreeBuffer(SRowSet);
	MAPIFreeBuffer(flaglist);
	if (retval != MAPI_E_SUCCESS) return retval;

	/* set message properties */
	msgflag = MSGFLAG_UNSENT;
	set_SPropValue_proptag(&props[0], PR_SUBJECT, (void *)oclient->subject);
	set_SPropValue_proptag(&props[1], PR_MESSAGE_FLAGS, (void *)&msgflag);
	prop_count = 2;

	/* Set PR_BODY or PR_HTML or inline PR_HTML */
	if (oclient->pr_body) {
		if (strlen(oclient->pr_body) > MAX_READ_SIZE) {
			struct SBinary	bin;

			bin.lpb = (uint8_t *)oclient->pr_body;
			bin.cb = strlen(oclient->pr_body);
			openchangeclient_stream(mem_ctx, obj_message, obj_stream, PR_BODY, 2, bin);
		} else {
			set_SPropValue_proptag(&props[2], PR_BODY, (void *)oclient->pr_body);
			prop_count++;
		}
	} else if (oclient->pr_html_inline) {
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
			retval = SaveChanges(&obj_message, &obj_attach);
			if (retval != MAPI_E_SUCCESS) return retval;

			mapi_object_release(&obj_attach);
		}
	}
	/* Submit the message */
	retval = SubmitMessage(&obj_message);
	if (retval != MAPI_E_SUCCESS) return retval;

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_outbox);

	return MAPI_E_SUCCESS;
}

/**
 * delete a mail from user INBOX
 */
static BOOL openchangeclient_deletemail(TALLOC_CTX *mem_ctx, struct mapi_session *session,
					mapi_object_t *obj_store, struct oclient *oclient)
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

	retval = GetReceiveFolder(obj_store, &id_inbox);
	if (retval != MAPI_E_SUCCESS) return False;

	retval = OpenFolder(obj_store, id_inbox, &obj_inbox);
	if (retval != MAPI_E_SUCCESS) return False;

	retval = GetContentsTable(&obj_inbox, &obj_table);
	if (retval != MAPI_E_SUCCESS) return False;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT);
	retval = SetColumns(&obj_table, SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return False;
	
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
			if (retval != MAPI_E_SUCCESS) return False;
		}
	}

	mapi_object_release(&obj_table);
	mapi_object_release(&obj_inbox);

	return True;
}

int main(int argc, const char *argv[])
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct mapi_session	*session = NULL;
	mapi_object_t		obj_store;
	struct oclient	oclient;
	poptContext		pc;
	int			opt;
	BOOL			opt_sendmail = false;
	BOOL			opt_fetchmail = false;
	BOOL			opt_deletemail = false;
	const char		*opt_profdb = NULL;
	const char		*opt_profname = NULL;
	const char		*opt_attachments = NULL;
	const char		*opt_html_file = NULL;
	const char		*opt_mapi_to = NULL;
	const char		*opt_mapi_cc = NULL;
	const char		*opt_mapi_bcc = NULL;

	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_SENDMAIL, OPT_FETCHMAIL, 
	      OPT_STOREMAIL,  OPT_DELETEMAIL, OPT_ATTACH, OPT_HTML_INLINE, OPT_HTML_FILE,
	      OPT_MAPI_TO, OPT_MAPI_CC, OPT_MAPI_BCC, OPT_MAPI_SUBJECT, OPT_MAPI_BODY};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path"},
		{"profile", 'p', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name"},
		{"sendmail", 'S', POPT_ARG_NONE, NULL, OPT_SENDMAIL, "send a mail"},
		{"fetchmail", 'F', POPT_ARG_NONE, NULL, OPT_FETCHMAIL, "fetch user INBOX mails"},
		{"storemail", 'G', POPT_ARG_STRING, NULL, OPT_STOREMAIL, "retrieve a mail on the filesystem"},
		{"deletemail", 'D', POPT_ARG_NONE, NULL, OPT_DELETEMAIL, "delete a mail from user INBOX"},
		{"attachments", 'A', POPT_ARG_STRING, NULL, OPT_ATTACH, "send a list of attachments"},
		{"html-inline", 'I', POPT_ARG_STRING, NULL, OPT_HTML_INLINE, "send PR_HTML content"},
		{"html-file", 'W', POPT_ARG_STRING, NULL, OPT_HTML_FILE, "use HTML file as content"},
		{"to", 't', POPT_ARG_STRING, NULL, OPT_MAPI_TO, "To recipients"},
		{"cc", 'c', POPT_ARG_STRING, NULL, OPT_MAPI_CC, "Cc recipients"},
		{"bcc", 'b', POPT_ARG_STRING, NULL, OPT_MAPI_BCC, "Bcc recipients"},
		{"subject", 's', POPT_ARG_STRING, NULL, OPT_MAPI_SUBJECT, "Mail subject"},
		{"body", 'B', POPT_ARG_STRING, NULL, OPT_MAPI_BODY, "Mail body"},
		{ NULL }
	};

	mem_ctx = talloc_init("openchangeclient");

	init_oclient(&oclient);

	pc = poptGetContext("openchangeclient", argc, argv, long_options, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_PROFILE_DB:
			opt_profdb = poptGetOptArg(pc);
			break;
		case OPT_PROFILE:
			opt_profname = poptGetOptArg(pc);
			break;
		case OPT_SENDMAIL:
			opt_sendmail = true;
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
		if (oclient_parse_attachments(mem_ctx, opt_attachments, &oclient) == False) {
			printf("Unable to parse one of the specified attachments\n");
			exit (1);
		}
	}

	/**
	 * Initialize MAPI subsystem
	 */

	retval = MAPIInitialize(opt_profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	/* If no profile is specified try to load the default one from
	 * the database 
	 */

	if (!opt_profname) {
		retval = GetDefaultProfile(&opt_profname, 0);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", GetLastError());
			exit (1);
		}
	}

	retval = MapiLogonEx(&session, opt_profname);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", GetLastError());
		exit (1);
	}

	/**
	 * Open Default Message Store
	 */

	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore", GetLastError());
		exit (1);
	}

	

	/* MAPI operations */
	if (opt_sendmail) {
		/* recipients management */
		oclient.mapi_to = get_cmdline_recipients(mem_ctx, opt_mapi_to);
		oclient.mapi_cc = get_cmdline_recipients(mem_ctx, opt_mapi_cc);
		oclient.mapi_bcc = get_cmdline_recipients(mem_ctx, opt_mapi_bcc);

		retval = openchangeclient_sendmail(mem_ctx, session, &obj_store, &oclient);
		mapi_errstr("sendmail", GetLastError());
		if (retval != True) {
			goto end;
		}
	}

	if (opt_fetchmail) {
		retval = openchangeclient_fetchmail(session, &obj_store, &oclient);
		mapi_errstr("fetchmail", GetLastError());
		if (retval != True) {
			goto end;
		}
	}

	if (opt_deletemail) {
		retval = openchangeclient_deletemail(mem_ctx, session, &obj_store, &oclient);
		mapi_errstr("deletemail", GetLastError());
		if (retval != True) {
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
