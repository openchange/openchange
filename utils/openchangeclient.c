/*
   Stand-alone MAPI application

   OpenChange Project

   Copyright (C) Julien Kerihuel 2007-2013

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

#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include "libmapi/mapi_nameid.h"
#include "libocpf/ocpf.h"
#include <popt.h>
#include <param.h>

#include "openchangeclient.h"
#include "openchange-tools.h"

#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>

/**
 * init sendmail struct
 */

static void init_oclient(struct oclient *oclient)
{
	oclient->mapi_ctx = NULL;

	/* update and delete parameter */
	oclient->update = NULL;
	oclient->delete = NULL;

	/* properties list */
	oclient->props = NULL;
	
	/* email related parameter */
	oclient->subject = NULL;
	oclient->pr_body = NULL;
	oclient->pr_html_inline = NULL;
	oclient->attach = NULL;
	oclient->attach_num = 0;
	oclient->store_folder = NULL;
	
	/* appointment related parameters */
	oclient->location = NULL;
	oclient->dtstart = NULL;
	oclient->dtend = NULL;
	oclient->busystatus = -1;
	oclient->label = -1;
	oclient->private = false;
	oclient->freebusy = NULL;
	oclient->force = false;
	oclient->summary = false;

	/* contact related parameters */
	oclient->email = NULL;
	oclient->full_name = NULL;
	oclient->card_name = NULL;

	/* task related parameters */
	oclient->importance = -1;
	oclient->taskstatus = -1;

	/* note related parameters */
	oclient->color = -1;
	oclient->width = -1;
	oclient->height = -1;

	/* pf related parameters */
	oclient->pf = false;

	/* folder related parameters */
	oclient->folder = NULL;
	oclient->folder_name = NULL;
	oclient->folder_comment = NULL;

	/* ocpf related parameters */
	oclient->ocpf_files = NULL;
	oclient->ocpf_dump = NULL;
}

static enum MAPISTATUS openchangeclient_getdir(TALLOC_CTX *mem_ctx,
					       mapi_object_t *obj_container,
					       mapi_object_t *obj_child,
					       const char *path)
{
	enum MAPISTATUS		retval;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct SRowSet		SRowSet;
	mapi_object_t		obj_htable;
	mapi_object_t		obj_folder;
	char     		**folder  = NULL;
	const char		*name;
	const uint64_t		*fid;
	bool			found = false;
	uint32_t		index;
	uint32_t		i;

	/* Step 1. Extract the folder list from full path */
	folder = str_list_make(mem_ctx, path, "/");
	mapi_object_copy(&obj_folder, obj_container);

	for (i = 0; folder[i]; i++) {
		found = false;

		mapi_object_init(&obj_htable);
		retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, NULL);
		MAPI_RETVAL_IF(retval, GetLastError(), folder);

		SPropTagArray = set_SPropTagArray(mem_ctx, 0x2,
						  PR_DISPLAY_NAME_UNICODE,
						  PR_FID);
		retval = SetColumns(&obj_htable, SPropTagArray);
		MAPIFreeBuffer(SPropTagArray);
		MAPI_RETVAL_IF(retval, retval, folder);

		while (((retval = QueryRows(&obj_htable, 0x32, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet)) != MAPI_E_NOT_FOUND) && SRowSet.cRows) {
			for (index = 0; (index < SRowSet.cRows) && (found == false); index++) {
				fid = (const uint64_t *)find_SPropValue_data(&SRowSet.aRow[index], PR_FID);
				name = (const char *)find_SPropValue_data(&SRowSet.aRow[index], PR_DISPLAY_NAME_UNICODE);
				if (name && fid && !strcmp(name, folder[i])) {
					retval = OpenFolder(&obj_folder, *fid, obj_child);
					MAPI_RETVAL_IF(retval, retval, folder);

					found = true;
					mapi_object_copy(&obj_folder, obj_child);
				}
			}
		}

		mapi_object_release(&obj_htable);
	}

	talloc_free(folder);
	MAPI_RETVAL_IF(found == false, MAPI_E_NOT_FOUND, NULL);

	return MAPI_E_SUCCESS;
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

	if ((fd = open(filename, O_RDONLY)) == -1) {
		printf("Error while opening %s\n", filename);
		return false;
	}
	/* stat the file */
	if (fstat(fd, &sb) != 0) {
		close(fd);
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
		if ((oclient->attach[oclient->attach_num].bin.lpb = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0)) == (void *) -1) {
			perror("mmap");
			close(fd);
			return false;
		}
		oclient->attach[oclient->attach_num].fd = fd;
		printf("filename = %s (size = %u / %u)\n", filename, oclient->attach[oclient->attach_num].bin.cb, (uint32_t)sb.st_size);
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
	if (substr) return substr + 1;

	return filename;
}


/**
 * build unique ID from folder and message
 */
static char *build_uniqueID(TALLOC_CTX *mem_ctx, mapi_object_t *obj_folder,
			    mapi_object_t *obj_message)
{
	char			*id;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		count;
	const uint64_t		*mid;
	const uint64_t		*fid;

	/* retrieve the folder ID */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_FID);
	GetProps(obj_folder, 0, SPropTagArray, &lpProps, &count);
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) return NULL;
	fid = (const uint64_t *)get_SPropValue_data(lpProps);

	/* retrieve the message ID */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_MID);
	GetProps(obj_message, 0, SPropTagArray, &lpProps, &count);
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) return NULL;
	mid = (const uint64_t *)get_SPropValue_data(lpProps);

	if (!fid || !mid) return NULL;

	id = talloc_asprintf(mem_ctx, "%"PRIX64"/%"PRIX64, *fid, *mid);
	return id;
}


/**
 * fetch the user INBOX
 */

#define	MAX_READ_SIZE	0x1000

static bool store_attachment(mapi_object_t obj_attach, const char *filename, uint32_t size, struct oclient *oclient)
{
	TALLOC_CTX	*mem_ctx;
	bool		ret = true;
	enum MAPISTATUS	retval;
	char		*path;
	mapi_object_t	obj_stream;
	uint16_t	read_size;
	int		fd;
	DIR		*dir;
	unsigned char  	buf[MAX_READ_SIZE];

	if (!filename || !size) return false;

	mapi_object_init(&obj_stream);

	mem_ctx = talloc_named(NULL, 0, "store_attachment");

	if (!(dir = opendir(oclient->store_folder))) {
		if (mkdir(oclient->store_folder, 0700) == -1) {
			ret = false;
			goto error_close;
		}
	} else {
		closedir(dir);
	}

	path = talloc_asprintf(mem_ctx, "%s/%s", oclient->store_folder, filename);
	if ((fd = open(path, O_CREAT|O_WRONLY, S_IWUSR|S_IRUSR)) == -1) {
		ret = false;
		talloc_free(path);
		goto error_close;
	}
	talloc_free(path);

	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 0, &obj_stream);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto error;
	}

	read_size = 0;
	do {
		retval = ReadStream(&obj_stream, buf, MAX_READ_SIZE, &read_size);
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
			goto error;
		}
		write(fd, buf, read_size);
	} while (read_size);

error:	
	close(fd);
error_close:
	mapi_object_release(&obj_stream);
	talloc_free(mem_ctx);
	return ret;
}

static enum MAPISTATUS openchangeclient_fetchmail(mapi_object_t *obj_store, 
						  struct oclient *oclient)
{
	enum MAPISTATUS			retval;
	bool				status;
	TALLOC_CTX			*mem_ctx;
	mapi_object_t			obj_tis;
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
	
	mem_ctx = talloc_named(NULL, 0, "openchangeclient_fetchmail");

	mapi_object_init(&obj_tis);
	mapi_object_init(&obj_inbox);
	mapi_object_init(&obj_table);

	if (oclient->pf == true) {
		retval = openchangeclient_getpfdir(mem_ctx, obj_store, &obj_inbox, oclient->folder);
		MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx);
	} else {
		if (oclient->folder) {
			retval = GetDefaultFolder(obj_store, &id_inbox, olFolderTopInformationStore);
			MAPI_RETVAL_IF(retval, retval, mem_ctx);

			retval = OpenFolder(obj_store, id_inbox, &obj_tis);
			MAPI_RETVAL_IF(retval, retval, mem_ctx);

			retval = openchangeclient_getdir(mem_ctx, &obj_tis, &obj_inbox, oclient->folder);
			MAPI_RETVAL_IF(retval, retval, mem_ctx);
		} else {
			retval = GetReceiveFolder(obj_store, &id_inbox, NULL);
			MAPI_RETVAL_IF(retval, retval, mem_ctx);

			retval = OpenFolder(obj_store, id_inbox, &obj_inbox);
			MAPI_RETVAL_IF(retval, retval, mem_ctx);
		}
	}

	retval = GetContentsTable(&obj_inbox, &obj_table, 0, &count);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	printf("MAILBOX (%u messages)\n", count);
	if (!count) goto end;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT_UNICODE);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	while ((retval = QueryRows(&obj_table, count, TBL_ADVANCE, TBL_FORWARD_READ, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		count -= rowset.cRows;
		for (i = 0; i < rowset.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(obj_store,
					     rowset.aRow[i].lpProps[0].value.d,
					     rowset.aRow[i].lpProps[1].value.d,
					     &obj_message, 0);
			if (GetLastError() == MAPI_E_SUCCESS) {
				if (oclient->summary) {
					mapidump_message_summary(&obj_message);
				} else {
					struct SPropValue	*lpProps;
					struct SRow		aRow;
					
					SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_HASATTACH);
					lpProps = NULL;
					retval = GetProps(&obj_message, 0, SPropTagArray, &lpProps, &count);
					MAPIFreeBuffer(SPropTagArray);
					if (retval != MAPI_E_SUCCESS) return retval;
					
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
							
							retval = QueryRows(&obj_tb_attach, 0xa, TBL_ADVANCE, TBL_FORWARD_READ, &rowset_attach);
							if (retval != MAPI_E_SUCCESS) return retval;
							
							for (j = 0; j < rowset_attach.cRows; j++) {
								attach_num = (const uint32_t *)find_SPropValue_data(&(rowset_attach.aRow[j]), PR_ATTACH_NUM);
								retval = OpenAttach(&obj_message, *attach_num, &obj_attach);
								if (retval == MAPI_E_SUCCESS) {
									struct SPropValue	*lpProps2;
									uint32_t		count2;
									
									SPropTagArray = set_SPropTagArray(mem_ctx, 0x4, 
													  PR_ATTACH_FILENAME,
													  PR_ATTACH_LONG_FILENAME,
													  PR_ATTACH_SIZE,
													  PR_ATTACH_CONTENT_ID);
									lpProps2 = NULL;
									retval = GetProps(&obj_attach, MAPI_UNICODE, SPropTagArray, &lpProps2, &count2);
									MAPIFreeBuffer(SPropTagArray);
									if (retval != MAPI_E_SUCCESS) return retval;
									
									aRow.ulAdrEntryPad = 0;
									aRow.cValues = count2;
									aRow.lpProps = lpProps2;
									
									attach_filename = get_filename(octool_get_propval(&aRow, PR_ATTACH_LONG_FILENAME));
									if (!attach_filename || (attach_filename && !strcmp(attach_filename, ""))) {
										attach_filename = get_filename(octool_get_propval(&aRow, PR_ATTACH_FILENAME));
									}
									attach_size = (const uint32_t *) octool_get_propval(&aRow, PR_ATTACH_SIZE);
									printf("[%u] %s (%u Bytes)\n", j, attach_filename, attach_size ? *attach_size : 0);
									fflush(0);
									if (oclient->store_folder) {
										status = store_attachment(obj_attach, attach_filename, attach_size ? *attach_size : 0, oclient);
										if (status == false) {
											printf("A Problem was encountered while storing attachments on the filesystem\n");
											talloc_free(mem_ctx);
											return MAPI_E_UNABLE_TO_COMPLETE;
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
			}
			mapi_object_release(&obj_message);
		}
	}
 end:
	mapi_object_release(&obj_table);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_tis);

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
		OC_DEBUG(2, "Invalid recipient string format");
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

	SRowSet->aRow = talloc_realloc(mem_ctx, SRowSet->aRow, struct SRow, SRowSet->cRows + 1);
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
	SPropValue.ulPropTag = PR_GIVEN_NAME_UNICODE;
	SPropValue.value.lpszA = (uint8_t *) username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_DISPLAY_NAME */
	SPropValue.ulPropTag = PR_DISPLAY_NAME_UNICODE;
	SPropValue.value.lpszA = (uint8_t *) username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_7BIT_DISPLAY_NAME */
	SPropValue.ulPropTag = PR_7BIT_DISPLAY_NAME_UNICODE;
	SPropValue.value.lpszA = (uint8_t *) username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_SMTP_ADDRESS */
	SPropValue.ulPropTag = PR_SMTP_ADDRESS_UNICODE;
	SPropValue.value.lpszA = (uint8_t *) username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_ADDRTYPE */
	SPropValue.ulPropTag = PR_ADDRTYPE_UNICODE;
	SPropValue.value.lpszA = (uint8_t *) "SMTP";
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	SetRecipientType(&(SRowSet->aRow[last]), RecipClass);

	SRowSet->cRows += 1;
	return true;
}

static bool set_usernames_RecipientType(TALLOC_CTX *mem_ctx, uint32_t *index, struct SRowSet *rowset, 
					char **usernames, struct PropertyTagArray_r *flaglist,
					enum ulRecipClass RecipClass)
{
	uint32_t	i;
	uint32_t	count = *index;
	static uint32_t	counter = 0;

	if (count == 0) counter = 0;
	if (!usernames) return false;

	for (i = 0; usernames[i]; i++) {
		if (flaglist->aulPropTag[count] == MAPI_UNRESOLVED) {
			set_external_recipients(mem_ctx, rowset, usernames[i], RecipClass);
		}
		if (flaglist->aulPropTag[count] == MAPI_RESOLVED) {
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
				    uint32_t access_flags, struct Binary_r bin)
{
	enum MAPISTATUS	retval;
	DATA_BLOB	stream;
	uint32_t	size;
	uint32_t	offset;
	uint16_t	read_size;

	/* Open a stream on the parent for the given property */
	retval = OpenStream(&obj_parent, mapitag, access_flags, &obj_stream);
	fprintf (stderr, "openstream: retval = %s (0x%.8x)\n",
		 mapi_get_errstr (retval), retval);

	if (retval != MAPI_E_SUCCESS) return false;

	/* WriteStream operation */
	printf("We are about to write %u bytes in the stream\n", bin.cb);
	size = MAX_READ_SIZE;
	if (size > bin.cb) {
		size = bin.cb;
	}
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
		if (!read_size) {
			mapi_object_release(&obj_stream);
			return true;
		}
		
		offset += read_size;

		if ((offset + size) > bin.cb) {
			size = bin.cb - offset;
		}
	}

	mapi_object_release(&obj_stream);

	return true;
}


#define SETPROPS_COUNT	5

/**
 * Send a mail
 */

static enum MAPISTATUS openchangeclient_sendmail(TALLOC_CTX *mem_ctx, 
						 mapi_object_t *obj_store, 
						 struct oclient *oclient)
{
	enum MAPISTATUS			retval;
	struct SPropTagArray		*SPropTagArray;
	struct SPropValue		SPropValue;
	struct SRowSet			*SRowSet = NULL;
	struct PropertyRowSet_r		*RowSet = NULL;
	struct PropertyTagArray_r	*flaglist = NULL;
	mapi_id_t			id_outbox;
	mapi_object_t			obj_outbox;
	mapi_object_t			obj_message;
	mapi_object_t			obj_stream;
	struct timeval                  now;
	uint32_t			index = 0;
	uint32_t			msgflag;
	struct SPropValue		props[SETPROPS_COUNT];
	uint32_t			prop_count = 0;
	uint32_t			editor = 0;

	mapi_object_init(&obj_outbox);
	mapi_object_init(&obj_message);

	if (oclient->pf == true) {
		retval = openchangeclient_getpfdir(mem_ctx, obj_store, &obj_outbox, oclient->folder);
		if (retval != MAPI_E_SUCCESS) return retval;
	} else {
		/* Get Sent Items folder but should be using olFolderOutbox */
		retval = GetDefaultFolder(obj_store, &id_outbox, olFolderOutbox);
		if (retval != MAPI_E_SUCCESS) return retval;

		/* Open outbox folder */
		retval = OpenFolder(obj_store, id_outbox, &obj_outbox);
		if (retval != MAPI_E_SUCCESS) return retval;
	}

	/* Create the message */
	retval = CreateMessage(&obj_outbox, &obj_message);
	if (retval != MAPI_E_SUCCESS) return retval;

	/* Recipients operations */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0xA,
					  PR_ENTRYID,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_TRANSMITTABLE_DISPLAY_NAME_UNICODE,
					  PR_EMAIL_ADDRESS_UNICODE,
					  PR_ADDRTYPE_UNICODE,
					  PR_SEND_RICH_INFO,
					  PR_7BIT_DISPLAY_NAME_UNICODE,
					  PR_SMTP_ADDRESS_UNICODE);

	oclient->usernames = collapse_recipients(mem_ctx, oclient);

	/* ResolveNames */
	retval = ResolveNames(mapi_object_get_session(&obj_message), (const char **)oclient->usernames, 
			      SPropTagArray, &RowSet, &flaglist, MAPI_UNICODE);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return retval;

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	if (RowSet) {
		cast_PropertyRowSet_to_SRowSet(mem_ctx, RowSet, SRowSet);
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
	oclient->subject = (!oclient->subject) ? "" : oclient->subject;
	set_SPropValue_proptag(&props[0], PR_SUBJECT_UNICODE, 
			       (const void *)oclient->subject);
	set_SPropValue_proptag(&props[1], PR_MESSAGE_FLAGS, 
			       (const void *)&msgflag);
	prop_count = 2;

	/* Set PR_BODY or PR_HTML or inline PR_HTML */
	if (oclient->pr_body) {
		editor = EDITOR_FORMAT_PLAINTEXT;
		set_SPropValue_proptag(&props[3], PR_MSG_EDITOR_FORMAT, (const void *)&editor);

		if (strlen(oclient->pr_body) > MAX_READ_SIZE) {
			struct Binary_r	bin;

			bin.lpb = (uint8_t *)oclient->pr_body;
			bin.cb = strlen(oclient->pr_body);
			openchangeclient_stream(mem_ctx, obj_message, obj_stream, PR_BODY, 2, bin);
		} else {
			set_SPropValue_proptag(&props[2], PR_BODY_UNICODE, 
								   (const void *)oclient->pr_body);
			prop_count++;
		}
	} else if (oclient->pr_html_inline) {
		editor = EDITOR_FORMAT_HTML;
		set_SPropValue_proptag(&props[3], PR_MSG_EDITOR_FORMAT, (const void *)&editor);

		if (strlen(oclient->pr_html_inline) > MAX_READ_SIZE) {
			struct Binary_r	bin;
			
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
		}
	}

	/* Send the submit time */
	if (gettimeofday(&now, NULL) == 0) {
		set_SPropValue_proptag_date_timeval(&props[prop_count - 1],
						    PR_CLIENT_SUBMIT_TIME, &now);
		prop_count++;
	}

	retval = SetProps(&obj_message, 0, props, prop_count);
	if (retval != MAPI_E_SUCCESS) return retval;

	/* attachment related code */
	if (oclient->attach) {
		uint32_t	i;

		for (i = 0; oclient->attach[i].filename; i++) {
			mapi_object_t		obj_attach;
			mapi_object_t		obj_stream;
			struct SPropValue	props_attach[4];
			uint32_t		count_props_attach;

			mapi_object_init(&obj_attach);

			retval = CreateAttach(&obj_message, &obj_attach);
			if (retval != MAPI_E_SUCCESS) return retval;
		
			props_attach[0].ulPropTag = PR_ATTACH_METHOD;
			props_attach[0].value.l = ATTACH_BY_VALUE;
			props_attach[1].ulPropTag = PR_RENDERING_POSITION;
			props_attach[1].value.l = 0;
			props_attach[2].ulPropTag = PR_ATTACH_FILENAME_UNICODE;
			printf("Sending %s:\n", oclient->attach[i].filename);
			props_attach[2].value.lpszW = get_filename(oclient->attach[i].filename);
			props_attach[3].ulPropTag = PR_ATTACH_CONTENT_ID;
			props_attach[3].value.lpszA = (uint8_t *) get_filename(oclient->attach[i].filename);
			count_props_attach = 4;

			/* SetProps */
			retval = SetProps(&obj_attach, 0, props_attach, count_props_attach);
			if (retval != MAPI_E_SUCCESS) return retval;

			/* Stream operations */
			mapi_object_init(&obj_stream);
			openchangeclient_stream(mem_ctx, obj_attach, obj_stream, PR_ATTACH_DATA_BIN, 2, oclient->attach[i].bin);

			/* Save changes on attachment */
			retval = SaveChangesAttachment(&obj_message, &obj_attach, KeepOpenReadWrite);
			if (retval != MAPI_E_SUCCESS) return retval;

			mapi_object_release(&obj_attach);
		}
	}

	if (oclient->pf) {
		retval = SaveChangesMessage(&obj_outbox, &obj_message, KeepOpenReadOnly);
		if (retval != MAPI_E_SUCCESS) return retval;

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
		retval = GetReceiveFolder(obj_store, &id_inbox, NULL);
		if (retval != MAPI_E_SUCCESS) return false;

		retval = OpenFolder(obj_store, id_inbox, &obj_inbox);
		if (retval != MAPI_E_SUCCESS) return false;
	}

	retval = GetContentsTable(&obj_inbox, &obj_table, 0, NULL);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT_UNICODE);
	retval = SetColumns(&obj_table, SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;
	
	while ((retval = QueryRows(&obj_table, 0x10, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet)) == MAPI_E_SUCCESS) {
		count_rows = SRowSet.cRows;
		if (!count_rows) break;
		id_messages = talloc_array(mem_ctx, uint64_t, count_rows);
		count_messages = 0;
		
		len = strlen(oclient->subject);

		for (i = 0; i < count_rows; i++) {
			if (!strncmp((const char *) SRowSet.aRow[i].lpProps[4].value.lpszA, oclient->subject, len)) {
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


static enum MAPISTATUS check_conflict_date(struct oclient *oclient,
					   mapi_object_t *obj,
					   struct FILETIME *ft)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	struct mapi_session	*session;
	struct mapi_session	*session2;
	bool			conflict;
			
	session = mapi_object_get_session(obj);
	retval = MapiLogonEx(oclient->mapi_ctx, &session2, session->profile->profname, session->profile->password);
	MAPI_RETVAL_IF(retval, retval, NULL);

	mapi_object_init(&obj_store);
	retval = OpenPublicFolder(session2, &obj_store);
	MAPI_RETVAL_IF(retval, retval, NULL);

	retval = IsFreeBusyConflict(&obj_store, ft, &conflict);
	Logoff(&obj_store);

	if (conflict == true) {
		printf("[WARNING]: This start date conflicts with another appointment\n");
		return MAPI_E_INVALID_PARAMETER;
	}	

	return MAPI_E_SUCCESS;
}

/**
 * Send appointment
 */

static enum MAPISTATUS appointment_SetProps(TALLOC_CTX *mem_ctx, 
					    mapi_object_t *obj_folder,
					    mapi_object_t *obj_message, 
					    struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	struct SPropValue	*lpProps;
	struct FILETIME		*start_date;
	struct FILETIME		*end_date;
	uint32_t		cValues = 0;
	NTTIME			nt;
	struct tm		tm;
	uint32_t		flag;
	uint8_t			flag2;

	cValues = 0;
	lpProps = talloc_array(mem_ctx, struct SPropValue, 2);

	if (oclient->subject) {
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_CONVERSATION_TOPIC_UNICODE, 
			       (const void *) oclient->subject);
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_NORMALIZED_SUBJECT_UNICODE,
			       (const void *) oclient->subject);
	}
	if (oclient->pr_body) {
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_BODY_UNICODE, (const void *)oclient->pr_body);
	}
	if (oclient->location) {
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidLocation, (const void *)oclient->location);
	}
	if (oclient->dtstart) {
		if (!strptime(oclient->dtstart, DATE_FORMAT, &tm)) {
			printf("Invalid date format: yyyy-mm-dd hh:mm:ss (e.g.: 2007-09-17 10:00:00)\n");
			return MAPI_E_INVALID_PARAMETER;
		}
		unix_to_nt_time(&nt, mktime(&tm));
		start_date = talloc(mem_ctx, struct FILETIME);
		start_date->dwLowDateTime = (nt << 32) >> 32;
		start_date->dwHighDateTime = (nt >> 32);

		retval = check_conflict_date(oclient, obj_folder, start_date);
		if (oclient->force == false) {
			MAPI_RETVAL_IF(retval, retval, NULL);
		}

		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_START_DATE, (const void *)start_date);
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidCommonStart, (const void *)start_date);
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidAppointmentStartWhole, (const void *) start_date);
	}
	if (oclient->dtend) {
		if (!strptime(oclient->dtend, DATE_FORMAT, &tm)) {
			printf("Invalid date format: yyyy-mm-dd hh:mm:ss (e.g.:2007-09-17 18:30:00)\n");
			return MAPI_E_INVALID_PARAMETER;
		}
		unix_to_nt_time(&nt, mktime(&tm));
		end_date = talloc(mem_ctx, struct FILETIME);
		end_date->dwLowDateTime = (nt << 32) >> 32;
		end_date->dwHighDateTime = (nt >> 32);

		retval = check_conflict_date(oclient, obj_folder, end_date);
		if (oclient->force == false) {
			MAPI_RETVAL_IF(retval, retval, NULL);
		}

		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_END_DATE, (const void *) end_date);
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidCommonEnd, (const void *) end_date);
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidAppointmentEndWhole, (const void *) end_date);
	}

	if (!oclient->update) {
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_MESSAGE_CLASS_UNICODE, (const void *)"IPM.Appointment");

		flag = 1;
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_MESSAGE_FLAGS, (const void *)&flag);

		flag= MEETING_STATUS_NONMEETING;
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidAppointmentStateFlags, (const void *) &flag);

		flag = 30;
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidReminderDelta, (const void *)&flag);
		
		/* WARNING needs to replace private */
		flag2 = oclient->private;
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidPrivate, (const void *)&flag2);

		oclient->label = (oclient->label == -1) ? 0 : oclient->label;
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidAppointmentColor, (const void *) &oclient->label);

		oclient->busystatus = (oclient->busystatus == -1) ? 0 : oclient->busystatus;
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidBusyStatus, (const void *) &oclient->busystatus);
	} else {
		if (oclient->busystatus != -1) {
			lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidBusyStatus, (const void *) &oclient->busystatus);
		}
		if (oclient->label != -1) {
			lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidAppointmentColor, (const void *) &oclient->label);
		}
	}

	flag = (oclient->private == true) ? 2 : 0;
	lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_SENSITIVITY, (const void *)&flag);
	
	retval = SetProps(obj_message, 0, lpProps, cValues);
	MAPIFreeBuffer(lpProps);
	MAPI_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}


static bool openchangeclient_sendappointment(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_calendar;
	mapi_object_t		obj_message;
	mapi_id_t		id_calendar;
	char			*uniq_id;

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

	/* Create calendar message */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_calendar, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Set calendar message properties */
	retval = appointment_SetProps(mem_ctx, &obj_calendar, &obj_message, oclient);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = SaveChangesMessage(&obj_calendar, &obj_message, KeepOpenReadOnly);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Fetch and Display the message unique ID */
	uniq_id = build_uniqueID(mem_ctx, &obj_calendar, &obj_message);
	printf("    %-25s: %s\n", "Unique ID", uniq_id);
	fflush(0);
	talloc_free(uniq_id);

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_calendar);
	
	return true;
}

/**
 * Create a contact
 */
static enum MAPISTATUS contact_SetProps(TALLOC_CTX *mem_ctx,
					mapi_object_t *obj_folder,
					mapi_object_t *obj_message,
					struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	struct SPropValue	*lpProps;
	struct mapi_nameid	*nameid;
	struct SPropTagArray	*SPropTagArray;
	uint32_t		cValues = 0;

	/* Build the list of named properties we want to set */
	nameid = mapi_nameid_new(mem_ctx);
	retval = mapi_nameid_string_add(nameid, "urn:schemas:contacts:fileas", PS_PUBLIC_STRINGS);
	if (retval != MAPI_E_SUCCESS) {
		talloc_free(nameid);
		return retval;
	}

	/* GetIDsFromNames and map property types */
	SPropTagArray = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = GetIDsFromNames(obj_folder, nameid->count,
				 nameid->nameid, 0, &SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		talloc_free(nameid);
		return retval;
	}
	mapi_nameid_SPropTagArray(nameid, SPropTagArray);
	MAPIFreeBuffer(nameid);

	cValues = 0;
	lpProps = talloc_array(mem_ctx, struct SPropValue, 2);

	if (oclient->card_name) {
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_NORMALIZED_SUBJECT_UNICODE, (const void *)oclient->card_name);
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidFileUnder, (const void *)oclient->card_name);
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, SPropTagArray->aulPropTag[0], (const void *)oclient->card_name);
	}
	if (oclient->full_name) {
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_DISPLAY_NAME_UNICODE, (const void *)oclient->full_name);
	}
	if (oclient->email) {
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidEmail1OriginalDisplayName, (const void *)oclient->email);
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidEmail1EmailAddress, (const void *)oclient->email);
	}
	if (!oclient->update) {
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_MESSAGE_CLASS_UNICODE, (const void *)"IPM.Contact");
	}
	retval = SetProps(obj_message, 0, lpProps, cValues);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(lpProps);
	MAPI_RETVAL_IF(retval, retval, NULL);
	
	return MAPI_E_SUCCESS;
}

static bool openchangeclient_sendcontact(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_contact;
	mapi_object_t		obj_message;
	mapi_id_t		id_contact;	
	char			*uniq_id;

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

	/* Create contact message */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_contact, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Set contact message properties */
	retval = contact_SetProps(mem_ctx, &obj_contact, &obj_message, oclient);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = SaveChangesMessage(&obj_contact, &obj_message, KeepOpenReadOnly);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Fetch and Display the message unique ID */
	uniq_id = build_uniqueID(mem_ctx, &obj_contact, &obj_message);
	printf("    %-25s: %s\n", "Unique ID", uniq_id);
	fflush(0);
	talloc_free(uniq_id);

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_contact);

	return true;
}

/**
 * Create task
 */
static enum MAPISTATUS task_SetProps(TALLOC_CTX *mem_ctx,
					mapi_object_t *obj_message,
					struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	struct SPropValue	*lpProps;
	struct FILETIME		*start_date;
	struct FILETIME		*end_date;
	uint32_t		cValues = 0;
	NTTIME			nt;
	struct tm		tm;

	cValues = 0;
	lpProps = talloc_array(mem_ctx, struct SPropValue, 2);

	if (oclient->card_name) {
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_CONVERSATION_TOPIC_UNICODE, (const void *)oclient->card_name);
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_NORMALIZED_SUBJECT_UNICODE, (const void *)oclient->card_name);
	}

	if (oclient->dtstart) {
		if (!strptime(oclient->dtstart, DATE_FORMAT, &tm)) {
			printf("Invalid date format (e.g.: 2007-06-01 22:30:00)\n");
			return MAPI_E_INVALID_PARAMETER;
		}
		unix_to_nt_time(&nt, mktime(&tm));
		start_date = talloc(mem_ctx, struct FILETIME);
		start_date->dwLowDateTime = (nt << 32) >> 32;
		start_date->dwHighDateTime = (nt >> 32);		
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidTaskStartDate, (const void *)start_date);
	}

	if (oclient->dtend) {
		if (!strptime(oclient->dtend, DATE_FORMAT, &tm)) {
			printf("Invalid date format (e.g.: 2007-06-01 22:30:00)\n");
			return MAPI_E_INVALID_PARAMETER;
		}
		unix_to_nt_time(&nt, mktime(&tm));
		end_date = talloc(mem_ctx, struct FILETIME);
		end_date->dwLowDateTime = (nt << 32) >> 32;
		end_date->dwHighDateTime = (nt >> 32);
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidTaskDueDate, (const void *)end_date);
	}

	if (oclient->pr_body) {
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_BODY_UNICODE, (const void *)oclient->pr_body);
	}

	if (!oclient->update) {
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_MESSAGE_CLASS_UNICODE, (const void *)"IPM.Task");
		oclient->importance = (oclient->importance == -1) ? 1 : oclient->importance;
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_IMPORTANCE, (const void *)&oclient->importance);
		oclient->taskstatus = (oclient->taskstatus == -1) ? 0 : oclient->taskstatus;
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidTaskStatus, (const void *)&oclient->taskstatus);
	} else {
		if (oclient->importance != -1) {
			lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_IMPORTANCE, (const void *)&oclient->importance);
		}
		if (oclient->taskstatus != -1) {
			lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidTaskStatus, (const void *)&oclient->taskstatus);
		}
	}

	retval = SetProps(obj_message, 0, lpProps, cValues);
	MAPIFreeBuffer(lpProps);
	MAPI_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}

static bool openchangeclient_sendtask(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_task;
	mapi_object_t		obj_message;
	mapi_id_t		id_task;	
	char			*uniq_id;

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

	/* Create contact message */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_task, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Set contact properties */
	retval = task_SetProps(mem_ctx, &obj_message, oclient);

	retval = SaveChangesMessage(&obj_task, &obj_message, KeepOpenReadOnly);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Fetch and Display the message unique ID */
	uniq_id = build_uniqueID(mem_ctx, &obj_task, &obj_message);
	printf("    %-25s: %s\n", "Unique ID", uniq_id);
	fflush(0);
	talloc_free(uniq_id);

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_task);

	return true;
}


/**
 * Send notes
 */
static enum MAPISTATUS note_SetProps(TALLOC_CTX *mem_ctx,
					mapi_object_t *obj_message,
					struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	struct SPropValue	*lpProps;
	uint32_t		cValues = 0;
	uint32_t		value;

	cValues = 0;
	lpProps = talloc_array(mem_ctx, struct SPropValue, 2);

	if (oclient->card_name) {
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_CONVERSATION_TOPIC_UNICODE, (const void *)oclient->card_name);
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_SUBJECT_UNICODE, (const void *)oclient->card_name);
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_NORMALIZED_SUBJECT_UNICODE, (const void *)oclient->card_name);
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_BODY_UNICODE, (const void *)oclient->card_name);
	}

	if (!oclient->update) {
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_MESSAGE_CLASS_UNICODE, (const void *)"IPM.StickyNote");

		value = 1;
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_MESSAGE_FLAGS, (const void *)&value);

		value = 768;
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PR_ICON_INDEX, (const void *)&value);
		
		value = 272;
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidSideEffects, (const void *)&value);

		oclient->color = (oclient->color == -1) ? olYellow : oclient->color;
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidNoteColor, (const void *)&oclient->color);

		oclient->width = (oclient->width == -1) ? 166 : oclient->width;
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidNoteWidth, (const void *)&oclient->width);

		oclient->height = (oclient->height == -1) ? 200 : oclient->height;
		lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidNoteHeight, (const void *)&oclient->height);

	} else {
		if (oclient->color != -1) {
			lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidNoteColor, (const void *)&oclient->color);
		}
		if (oclient->width != -1) {
			lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidNoteWidth, (const void *)&oclient->width);
		}
		if (oclient->height != -1) {
			lpProps = add_SPropValue(mem_ctx, lpProps, &cValues, PidLidNoteHeight, (const void *)&oclient->height);
		}
	}

	
	retval = SetProps(obj_message, 0, lpProps, cValues);
	MAPIFreeBuffer(lpProps);
	MAPI_RETVAL_IF(retval, retval, NULL);
	
	return MAPI_E_SUCCESS;
}

static bool openchangeclient_sendnote(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_note;
	mapi_object_t		obj_message;
	mapi_id_t		id_note;	
	char			*uniq_id;

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

	/* Create contact message */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_note, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Set Note message properties */
	retval = note_SetProps(mem_ctx, &obj_message, oclient);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = SaveChangesMessage(&obj_note, &obj_message, KeepOpenReadOnly);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Fetch and Display the message unique ID */
	uniq_id = build_uniqueID(mem_ctx, &obj_note, &obj_message);
	printf("    %-25s: %s\n", "Unique ID", uniq_id);
	fflush(0);
	talloc_free(uniq_id);

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
	retval = GetProps(&obj_folder, MAPI_UNICODE, SPropTagArray, &lpProps, &count);
	MAPIFreeBuffer(SPropTagArray);
	mapi_object_release(&obj_folder);
	if ((lpProps[0].ulPropTag != PR_CONTAINER_CLASS) || (retval != MAPI_E_SUCCESS)) {
		errno = 0;
		return IPF_NOTE;
	}
	return (const char *) lpProps[0].value.lpszA;
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
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, NULL);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x6,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_FID,
					  PR_COMMENT_UNICODE,
					  PR_CONTENT_UNREAD,
					  PR_CONTENT_COUNT,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;
	
	while (((retval = QueryRows(&obj_htable, 0x32, TBL_ADVANCE, TBL_FORWARD_READ, &rowset)) != MAPI_E_NOT_FOUND) && rowset.cRows) {
		for (index = 0; index < rowset.cRows; index++) {
			fid = (const uint64_t *)find_SPropValue_data(&rowset.aRow[index], PR_FID);
			name = (const char *)find_SPropValue_data(&rowset.aRow[index], PR_DISPLAY_NAME_UNICODE);
			comment = (const char *)find_SPropValue_data(&rowset.aRow[index], PR_COMMENT_UNICODE);
			total = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_CONTENT_COUNT);
			unread = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_CONTENT_UNREAD);
			child = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_FOLDER_CHILD_COUNT);

			for (i = 0; i < count; i++) {
				printf("|   ");
			}
			printf("|---+ %-15s : %-20s (Total: %u / Unread: %u - Container class: %s) [FID: 0x%016"PRIx64"]\n", 
			       name, comment?comment:"", total?*total:0, unread?*unread:0,
			       get_container_class(mem_ctx, parent, *fid), *fid);
			if (child && *child) {
				ret = get_child_folders(mem_ctx, &obj_folder, *fid, count + 1);
				if (ret == false) return ret;
			}
			
		}
	}
	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_folder);

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
	const uint32_t		*child;
	uint32_t		index;
	const uint64_t		*fid;
	int			i;

	mapi_object_init(&obj_folder);
	retval = OpenFolder(parent, folder_id, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, NULL);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x3,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;
	
	while (((retval = QueryRows(&obj_htable, 0x32, TBL_ADVANCE, TBL_FORWARD_READ, &rowset)) != MAPI_E_NOT_FOUND) && rowset.cRows) {
		for (index = 0; index < rowset.cRows; index++) {
			fid = (const uint64_t *)find_SPropValue_data(&rowset.aRow[index], PR_FID);
			name = (const char *)find_SPropValue_data(&rowset.aRow[index], PR_DISPLAY_NAME_UNICODE);
			child = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_FOLDER_CHILD_COUNT);

			for (i = 0; i < count; i++) {
				printf("|   ");
			}
			printf("|---+ %-15s [FID: 0x%016"PRIx64"]\n", name, *fid);
			if (child && *child) {
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

	/* Retrieve the mailbox folder name */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_DISPLAY_NAME_UNICODE);
	retval = GetProps(obj_store, MAPI_UNICODE, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;

	if (lpProps[0].value.lpszW) {
		mailbox_name = lpProps[0].value.lpszW;
	} else {
		return false;
	}

	/* Prepare the directory listing */
	retval = GetDefaultFolder(obj_store, &id_mailbox, olFolderTopInformationStore);
	if (retval != MAPI_E_SUCCESS) return false;

	printf("+ %s\n", mailbox_name);
	return get_child_folders(mem_ctx, obj_store, id_mailbox, 0);
}

static bool openchangeclient_fetchitems(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, const char *item,
					struct oclient *oclient)
{
	enum MAPISTATUS			retval;
	mapi_object_t			obj_tis;
	mapi_object_t			obj_folder;
	mapi_object_t			obj_table;
	mapi_object_t			obj_message;
	mapi_id_t			fid;
	uint32_t			olFolder = 0;
	struct SRowSet			SRowSet;
	struct SPropTagArray		*SPropTagArray;
	struct mapi_SPropValue_array	properties_array;
	uint32_t			count;
	uint32_t       			i;
	char				*id;
	
	if (!item) return false;

	mapi_object_init(&obj_tis);
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
		if (oclient->folder) {
			retval = GetDefaultFolder(obj_store, &fid, olFolderTopInformationStore);
			MAPI_RETVAL_IF(retval, retval, mem_ctx);

			retval = OpenFolder(obj_store, fid, &obj_tis);
			MAPI_RETVAL_IF(retval, retval, mem_ctx);

			retval = openchangeclient_getdir(mem_ctx, &obj_tis, &obj_folder, oclient->folder);
			MAPI_RETVAL_IF(retval, retval, mem_ctx);
		} else {
			retval = GetDefaultFolder(obj_store, &fid, olFolder);
			if (retval != MAPI_E_SUCCESS) return false;

			/* We now open the folder */
			retval = OpenFolder(obj_store, fid, &obj_folder);
			if (retval != MAPI_E_SUCCESS) return false;
		}
	}

	/* Operations on the  folder */
	mapi_object_init(&obj_table);
	retval = GetContentsTable(&obj_folder, &obj_table, 0, &count);
	if (retval != MAPI_E_SUCCESS) return false;

	printf("MAILBOX (%u messages)\n", count);
	if (!count) {
		mapi_object_release(&obj_table);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_tis);

		return true;
	}

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x8,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT_UNICODE,
					  PR_MESSAGE_CLASS_UNICODE,
					  PR_RULE_MSG_PROVIDER,
					  PR_RULE_MSG_NAME);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;

	while ((retval = QueryRows(&obj_table, count, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet)) != MAPI_E_NOT_FOUND && SRowSet.cRows) {
		count -= SRowSet.cRows;
		for (i = 0; i < SRowSet.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(&obj_folder, 
					     SRowSet.aRow[i].lpProps[0].value.d,
					     SRowSet.aRow[i].lpProps[1].value.d,
					     &obj_message, 0);
			if (retval != MAPI_E_NOT_FOUND) {
				if (oclient->summary) {
					mapidump_message_summary(&obj_message);
				} else {
					retval = GetPropsAll(&obj_message, MAPI_UNICODE, &properties_array);
					if (retval == MAPI_E_SUCCESS) {
						id = talloc_asprintf(mem_ctx, ": %"PRIX64"/%"PRIX64,
								     SRowSet.aRow[i].lpProps[0].value.d,
								     SRowSet.aRow[i].lpProps[1].value.d);
						mapi_SPropValue_array_named(&obj_message, 
									    &properties_array);
						switch (olFolder) {
						case olFolderInbox:
						  mapidump_message(&properties_array, id, NULL);
							break;
						case olFolderCalendar:
							mapidump_appointment(&properties_array, id);
							break;
						case olFolderContacts:
							mapidump_contact(&properties_array, id);
							break;
						case olFolderTasks:
							mapidump_task(&properties_array, id);
							break;
						case olFolderNotes:
							mapidump_note(&properties_array, id);
							break;
						}
						talloc_free(id);
					}
				}
				mapi_object_release(&obj_message);
			}
		}
	}
	
	mapi_object_release(&obj_table);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_tis);

	return true;
}

/**
 * Update Item
 *
 * Edit the existing item specified on command line.
 * The function first loops over the mailbox, looking for the folder
 * ID, then it looks for the particular message ID. It finally opens
 * it, set properties and save the message.
 *
 */
static enum MAPISTATUS folder_lookup(TALLOC_CTX *mem_ctx,
				     uint64_t sfid,
				     mapi_object_t *obj_parent,
				     mapi_id_t folder_id,
				     mapi_object_t *obj_ret)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	uint32_t		i;
	const uint64_t		*fid;

	mapi_object_init(&obj_folder);
	retval = OpenFolder(obj_parent, folder_id, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return retval;

	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, NULL);
	if (retval != MAPI_E_SUCCESS) return retval;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_FID);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return retval;

	while (((retval = QueryRows(&obj_htable, 0x32, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet)) != MAPI_E_NOT_FOUND && SRowSet.cRows)) {
		for (i = 0; i < SRowSet.cRows; i++) {
			fid = (const uint64_t *)find_SPropValue_data(&SRowSet.aRow[i], PR_FID);
			if (fid && *fid == sfid) {
				retval = OpenFolder(&obj_folder, *fid, obj_ret);
				mapi_object_release(&obj_htable);
				mapi_object_release(&obj_folder);
				return MAPI_E_SUCCESS;
			} else if (fid) {
				retval = folder_lookup(mem_ctx, sfid, &obj_folder, *fid, obj_ret);
				if (retval == MAPI_E_SUCCESS) {
					mapi_object_release(&obj_htable);
					mapi_object_release(&obj_folder);
					return MAPI_E_SUCCESS;
				}
			}
		}
	}

	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_folder);

	errno = MAPI_E_NOT_FOUND;
	return MAPI_E_NOT_FOUND;
}

static enum MAPISTATUS message_lookup(TALLOC_CTX *mem_ctx, 
				      uint64_t smid,
				      mapi_object_t *obj_folder, 
				      mapi_object_t *obj_message)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_htable;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	uint32_t		i;
	const uint64_t		*fid;
	const uint64_t		*mid;

	mapi_object_init(&obj_htable);
	retval = GetContentsTable(obj_folder, &obj_htable, 0, NULL);
	if (retval != MAPI_E_SUCCESS) return retval;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2, PR_FID, PR_MID);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return retval;

	while (((retval = QueryRows(&obj_htable, 0x32, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet)) != MAPI_E_NOT_FOUND) && SRowSet.cRows) {
		for (i = 0; i < SRowSet.cRows; i++) {
			fid = (const uint64_t *)find_SPropValue_data(&SRowSet.aRow[i], PR_FID);
			mid = (const uint64_t *)find_SPropValue_data(&SRowSet.aRow[i], PR_MID);
			if (mid && *mid == smid) {
				retval = OpenMessage(obj_folder, *fid, *mid, obj_message, ReadWrite);
				mapi_object_release(&obj_htable);
				return retval;
			}
		}
	}

	mapi_object_release(&obj_htable);

	errno = MAPI_E_NOT_FOUND;
	return MAPI_E_NOT_FOUND;
}

static bool openchangeclient_updateitem(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store,
					struct oclient *oclient, const char *container_class)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_id_t		id_tis;
	char			*fid_str;
	uint64_t		fid;
	uint64_t		mid;
	const char		*item = NULL;

	item = oclient->update;

	if (!item) {
		OC_DEBUG(0, "Missing ID");
		errno = MAPI_E_INVALID_PARAMETER;
		return false;
	}

	if (!container_class) {
		OC_DEBUG(0, "Missing container class");
		errno = MAPI_E_INVALID_PARAMETER;
		return false;
	}

	fid_str = strsep((char **)&item, "/");
	if (!fid_str || !item) {
		OC_DEBUG(0, "    Invalid ID: %s", fid_str ? fid_str : "null");
		errno = MAPI_E_INVALID_PARAMETER;
		return false;
	}

	fid = strtoull(fid_str, NULL, 16);
	mid = strtoull(item, NULL, 16);

	/* Step 1: search the folder from Top Information Store */
	mapi_object_init(&obj_folder);
	retval = GetDefaultFolder(obj_store, &id_tis, olFolderTopInformationStore);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = folder_lookup(mem_ctx, fid, obj_store, id_tis, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Step 2: search the message */
	mapi_object_init(&obj_message);
	retval = message_lookup(mem_ctx, mid, &obj_folder, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Step 3: edit the message */
	if (!strcmp(container_class, IPF_APPOINTMENT)) {
		retval = appointment_SetProps(mem_ctx, &obj_folder, &obj_message, oclient);
	} else if (!strcmp(container_class, IPF_CONTACT)) {
		retval = contact_SetProps(mem_ctx, &obj_folder, &obj_message, oclient);
	} else if (!strcmp(container_class, IPF_TASK)) {
		retval = task_SetProps(mem_ctx, &obj_message, oclient);
	} else if (!strcmp(container_class, IPF_STICKYNOTE)) {
		retval = note_SetProps(mem_ctx, &obj_message, oclient);
	}

	if (retval != MAPI_E_SUCCESS) return false;

	/* Step 4: save the message */
	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);

	return true;
}

/**
 * Delete an item given its unique ID
 */
static bool openchangeclient_deleteitems(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, 
					 struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_id_t		id_tis;
	char			*fid_str;
	uint64_t		fid;
	uint64_t		mid;
	const char		*item = NULL;

	item = oclient->delete;

	if (!item) {
		OC_DEBUG(0, "Missing ID");
		errno = MAPI_E_INVALID_PARAMETER;
		return false;
	}
	
	fid_str = strsep((char **)&item, "/");
	if (!fid_str || !item) {
		OC_DEBUG(0, "    Invalid ID: %s", fid_str ? fid_str : "null");
		errno = MAPI_E_INVALID_PARAMETER;
		return false;
	}

	fid = strtoull(fid_str, NULL, 16);
	mid = strtoull(item, NULL, 16);

	/* Step 1: search the folder from Top Information Store */
	mapi_object_init(&obj_folder);
	retval = GetDefaultFolder(obj_store, &id_tis, olFolderTopInformationStore);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = folder_lookup(mem_ctx, fid, obj_store, id_tis, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return false;
	
	/* Step 2: search the message within returned folder */
	mapi_object_init(&obj_message);
	retval = message_lookup(mem_ctx, mid, &obj_folder, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Step 3: delete the message */
	retval = DeleteMessage(&obj_folder, &mid, 1);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_release(&obj_message);
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
						   mapi_id_t msgid)
{
	enum MAPISTATUS			retval;
	TALLOC_CTX			*mem_ctx;
	struct SRowSet			SRowSet;
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
	char				*id;

	mem_ctx = talloc_named(NULL, 0, "openchangeclient_findmail");

	/* Get Inbox folder */
	retval = GetDefaultFolder(obj_store, &fid, olFolderInbox);
	MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx);

	/* Open Inbox */
	mapi_object_init(&obj_inbox);
	retval = OpenFolder(obj_store, fid, &obj_inbox);
	MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx);

	/* Retrieve contents table */
	mapi_object_init(&obj_table);
	retval = GetContentsTable(&obj_inbox, &obj_table, 0, &count);
	MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx);

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2,
					  PR_FID,
					  PR_MID);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx);

	while ((retval = QueryRows(&obj_table, 0xa, TBL_ADVANCE, TBL_FORWARD_READ, &SRowSet)) != MAPI_E_NOT_FOUND && SRowSet.cRows) {
		for (i = 0; i < SRowSet.cRows; i++) {
			lpProp = get_SPropValue_SRowSet(&SRowSet, PR_MID);
			if (lpProp != NULL) {
				mid = (const uint64_t *)get_SPropValue(lpProp, PR_MID);
				if (*mid == msgid) {
					mapi_object_init(&obj_message);
					retval = OpenMessage(obj_store,
							     SRowSet.aRow[i].lpProps[0].value.d,
							     SRowSet.aRow[i].lpProps[1].value.d,
							     &obj_message, 0);
					if (GetLastError() == MAPI_E_SUCCESS) {
						retval = GetPropsAll(&obj_message, MAPI_UNICODE, &properties_array);
						if (retval != MAPI_E_SUCCESS) return retval;
						id = talloc_asprintf(mem_ctx, ": %"PRIX64"/%"PRIX64,
								     SRowSet.aRow[i].lpProps[0].value.d,
								     SRowSet.aRow[i].lpProps[1].value.d);
						mapidump_message(&properties_array, id, NULL);
						mapi_object_release(&obj_message);
						talloc_free(id);

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


static int callback(uint16_t NotificationType, void *NotificationData, void *private_data)
{
	struct NewMailNotification	*newmail;
	struct HierarchyTableChange    	*htable;
	struct ContentsTableChange     	*ctable;
	struct ContentsTableChange     	*stable;

	switch(NotificationType) {
	case fnevNewMail:
	case fnevNewMail|fnevMbit:
		OC_DEBUG(0, "[+] New mail Received");
		newmail = (struct NewMailNotification *) NotificationData;
		mapidump_newmail(newmail, "\t");
		openchangeclient_findmail((mapi_object_t *)private_data, newmail->MID);
		mapi_errstr("openchangeclient_findmail", GetLastError());
		break;
	case fnevObjectCreated:
		OC_DEBUG(0, "[+] Folder Created");
		break;
	case fnevObjectDeleted:
		OC_DEBUG(0, "[+] Folder Deleted");
		break;
	case fnevObjectModified:
	case fnevTbit|fnevObjectModified:
	case fnevUbit|fnevObjectModified:
	case fnevTbit|fnevUbit|fnevObjectModified:
		OC_DEBUG(0, "[+] Folder Modified");
		break;
	case fnevObjectMoved:
		OC_DEBUG(0, "[+] Folder Moved");
		break;
	case fnevObjectCopied:
		OC_DEBUG(0, "[+] Folder Copied");
		break;
	case fnevSearchComplete:
		OC_DEBUG(0, "[+] Search complete in search folder");
		break;
	case fnevTableModified:
		htable = (struct HierarchyTableChange *) NotificationData;
		switch (htable->TableEvent) {
		case TABLE_CHANGED:
			OC_DEBUG(0, "[+] Hierarchy Table:  changed");
			break;
		case TABLE_ROW_ADDED:
			OC_DEBUG(0, "[+] Hierarchy Table: row added");
			break;
		case TABLE_ROW_DELETED:
			OC_DEBUG(0, "[+] Hierarchy Table: row deleted");
			break;
		case TABLE_ROW_MODIFIED:
			OC_DEBUG(0, "[+] Hierarchy Table: row modified");
			break;
		case TABLE_RESTRICT_DONE:
			OC_DEBUG(0, "[+] Hierarchy Table: restriction done");
			break;
		default:
			OC_DEBUG(0, "[+] Hierarchy Table: ");
			break;
		}
		break;
	case fnevStatusObjectModified:
		OC_DEBUG(0, "[+] ICS Notification");
		break;
	case fnevMbit|fnevObjectCreated:
		OC_DEBUG(0, "[+] Message created");
		break;
	case fnevMbit|fnevObjectDeleted:
		OC_DEBUG(0, "[+] Message deleted");
		break;
	case fnevMbit|fnevObjectModified:
		OC_DEBUG(0, "[+] Message modified");
		break;
	case fnevMbit|fnevObjectMoved:
		OC_DEBUG(0, "[+] Message moved");
		break;
	case fnevMbit|fnevObjectCopied:
		OC_DEBUG(0, "[+] Message copied");
		break;
	case fnevMbit|fnevTableModified:
		ctable = (struct ContentsTableChange *) NotificationData;
		switch (ctable->TableEvent) {
		case TABLE_CHANGED:
			OC_DEBUG(0, "[+] Contents Table:  changed");
			break;
		case TABLE_ROW_ADDED:
			OC_DEBUG(0, "[+] Contents Table: row added");
			break;
		case TABLE_ROW_DELETED:
			OC_DEBUG(0, "[+] Contents Table: row deleted");
			break;
		case TABLE_ROW_MODIFIED:
			OC_DEBUG(0, "[+] Contents Table: row modified");
			break;
		case TABLE_RESTRICT_DONE:
			OC_DEBUG(0, "[+] Contents Table: restriction done");
			break;
		default:
			OC_DEBUG(0, "[+] Contents Table: ");
			break;
		}
		break;
	case fnevMbit|fnevSbit|fnevObjectDeleted:
		OC_DEBUG(0, "[+] A message is no longer part of a search folder");
		break;
	case fnevMbit|fnevSbit|fnevObjectModified:
		OC_DEBUG(0, "[+] A property on a message in a search folder has changed");
		break;
	case fnevMbit|fnevSbit|fnevTableModified:
		stable = (struct ContentsTableChange *) NotificationData;
		switch (stable->TableEvent) {
		case TABLE_CHANGED:
			OC_DEBUG(0, "[+] Search Table:  changed");
			break;
		case TABLE_ROW_ADDED:
			OC_DEBUG(0, "[+] Search Table: row added");
			break;
		case TABLE_ROW_DELETED:
			OC_DEBUG(0, "[+] Search Table: row deleted");
			break;
		case TABLE_ROW_MODIFIED:
			OC_DEBUG(0, "[+] Search Table: row modified");
			break;
		case TABLE_RESTRICT_DONE:
			OC_DEBUG(0, "[+] Search Table: restriction done");
			break;
		default:
			OC_DEBUG(0, "[+] Search Table: ");
			break;
		}
		break;
	default:
		OC_DEBUG(0, "[+] Unsupported notification (0x%x)", NotificationType);
		break;
	}

	return 0;
}


static bool openchangeclient_notifications(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_inbox;
	mapi_id_t		fid;
	uint32_t		ulConnection;
	uint16_t		ulEventMask;
	uint32_t		notification = 0;
	struct mapi_session	*session;

	/* Register notification */
	session = mapi_object_get_session(obj_store);
	retval = RegisterNotification(session);
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
	ulEventMask = fnevNewMail|fnevObjectCreated|fnevObjectDeleted|
		fnevObjectModified|fnevObjectMoved|fnevObjectCopied|
		fnevSearchComplete|fnevTableModified|fnevStatusObjectModified;
	retval = Subscribe(obj_store, &ulConnection, ulEventMask, true, (mapi_notify_callback_t)callback,
		(void*) obj_store);
	retval = Subscribe(&obj_inbox, &ulConnection, ulEventMask, true, (mapi_notify_callback_t)callback,
		(void*) obj_store);
	if (retval != MAPI_E_SUCCESS) return false;

	/* wait for notifications: infinite loop */
	retval = RegisterAsyncNotification(mapi_object_get_session(obj_store), &notification);
	if( retval == MAPI_E_NOT_INITIALIZED ) {
		retval = MonitorNotification(mapi_object_get_session(obj_store), (void *)obj_store, NULL);
	}
	if (retval != MAPI_E_SUCCESS) return false;

	retval = Unsubscribe(mapi_object_get_session(obj_store), ulConnection);
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
	
	printf("obj_child fid = 0x%"PRIx64"\n", mapi_object_get_id(&obj_child));

	retval = DeleteFolder(&obj_folder, mapi_object_get_id(&obj_child),
			      DEL_FOLDERS, NULL);
	if (retval != MAPI_E_SUCCESS) return false;
	
	mapi_object_release(&obj_child);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_tis);
	
	return true;
}

static bool openchangeclient_userlist(TALLOC_CTX *mem_ctx, 
				      struct mapi_session *session)
{
	struct SPropTagArray	*SPropTagArray;
	struct PropertyRowSet_r	*RowSet;
	uint32_t		i;
	uint32_t		count;
	uint8_t			ulFlags;
	uint32_t		rowsFetched = 0;
	uint32_t		totalRecs = 0;

	GetGALTableCount(session, &totalRecs);
	printf("Total Number of entries in GAL: %d\n", totalRecs);

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
		GetGALTable(session, SPropTagArray, &RowSet, count, ulFlags);
		if ((!RowSet) || (!(RowSet->aRow))) {
			return false;
		}
		rowsFetched = RowSet->cRows;
		if (rowsFetched) {
			for (i = 0; i < rowsFetched; i++) {
				mapidump_PAB_entry(&RowSet->aRow[i]);
			}
		}
		ulFlags = TABLE_CUR;
		MAPIFreeBuffer(RowSet);
	} while (rowsFetched == count);
	mapi_errstr("GetPABTable", GetLastError());

	MAPIFreeBuffer(SPropTagArray);

	return true;
}


static bool openchangeclient_ocpf_syntax(struct oclient *oclient)
{
	int			ret;
	struct ocpf_file	*element;
	uint32_t		context_id;

	/* Sanity checks */
	if (!oclient->ocpf_files || !oclient->ocpf_files->next) {
		errno = MAPI_E_INVALID_PARAMETER;
		return false;
	}

	/* Step 1. Initialize OCPF context */
	ret = ocpf_init();
	if (ret == -1) {
		errno = MAPI_E_CALL_FAILED;
		return false;
	}

	/* Step2. Parse OCPF files */
	for (element = oclient->ocpf_files; element->next; element = element->next) {
	  ret = ocpf_new_context(element->filename, &context_id, OCPF_FLAGS_READ);
		if (ret == -1) {
			errno = MAPI_E_INVALID_PARAMETER;
			return false;
		}
		ret = ocpf_parse(context_id);
		if (ret == -1) {
			OC_DEBUG(0, "ocpf_parse failed ...");
			errno = MAPI_E_INVALID_PARAMETER;
			return false;
		}

		/* Dump OCPF contents */
		ocpf_dump(context_id);

		ret = ocpf_del_context(context_id);
	}


	/* Step4. Release OCPF context */
	ret = ocpf_release();
	if (ret == -1) {
		errno = MAPI_E_CALL_FAILED;
		return false;
	}

	return true;
}


static bool openchangeclient_ocpf_sender(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS		retval;
	int			ret;
	struct ocpf_file	*element;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	uint32_t		cValues = 0;
	struct SPropValue	*lpProps;
	uint32_t		context_id;

	/* Sanity Check */
	if (!oclient->ocpf_files || !oclient->ocpf_files->next) {
		errno = MAPI_E_INVALID_PARAMETER;
		return false;
	}

	/* Step1. Initialize OCPF context */
	ret = ocpf_init();
	if (ret == -1) {
		errno = MAPI_E_CALL_FAILED;
		return false;
	}

	/* Step2. Parse OCPF files */
	for (element = oclient->ocpf_files; element->next; element = element->next) {
	  ret = ocpf_new_context(element->filename, &context_id, OCPF_FLAGS_READ);
		ret = ocpf_parse(context_id);
		if (ret == -1) {
			errno = MAPI_E_INVALID_PARAMETER;
			return false;
		}
	}

	/* Step3. Open destination folder using ocpf API */
	mapi_object_init(&obj_folder);
	retval = ocpf_OpenFolder(context_id, obj_store, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Step4. Create the message */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_folder, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Step5, Set message recipients */
	retval = ocpf_set_Recipients(mem_ctx, context_id, &obj_message);
	if (retval != MAPI_E_SUCCESS && GetLastError() != MAPI_E_NOT_FOUND) return false;
	errno = MAPI_E_SUCCESS;

	/* Step6. Set message properties */
	retval = ocpf_set_SPropValue(mem_ctx, context_id, &obj_folder, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Step7. Set message properties */
	lpProps = ocpf_get_SPropValue(context_id, &cValues);

	retval = SetProps(&obj_message, 0, lpProps, cValues);
	MAPIFreeBuffer(lpProps);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Step8. Save message */
	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);

	ocpf_del_context(context_id);

	return true;
}


static bool openchangeclient_ocpf_dump(TALLOC_CTX *mem_ctx, mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS			retval;
	int				ret;
	mapi_object_t			obj_folder;
	mapi_object_t			obj_message;
	mapi_id_t			id_tis;
	const char			*fid_str;
	uint64_t			fid;
	uint64_t			mid;
	const char			*item = NULL;
	char				*filename = NULL;
	struct mapi_SPropValue_array	lpProps;
	uint32_t			context_id;


	/* retrieve the FID/MID for ocpf_dump parameter */
	item = oclient->ocpf_dump;

	fid_str = strsep((char **)&item, "/");
	if (!fid_str || !item) {
		OC_DEBUG(0, "    Invalid ID: %s", fid_str ? fid_str : "null");
		errno = MAPI_E_INVALID_PARAMETER;
		return false;
	}

	fid = strtoull(fid_str, NULL, 16);
	mid = strtoull(item, NULL, 16);

	/* Step 1. search the folder from Top Information Store */
	mapi_object_init(&obj_folder);
	retval = GetDefaultFolder(obj_store, &id_tis, olFolderTopInformationStore);
	if (retval != MAPI_E_SUCCESS) return false;

	retval = folder_lookup(mem_ctx, fid, obj_store, id_tis, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Step 2. search the message */
	mapi_object_init(&obj_message);
	retval = message_lookup(mem_ctx, mid, &obj_folder, &obj_message);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Step 3. retrieve all message properties */
	retval = GetPropsAll(&obj_message, MAPI_UNICODE, &lpProps);

	/* Step 4. save the message */
	ret = ocpf_init();

	filename = talloc_asprintf(mem_ctx, "%"PRIx64".ocpf", mid);
	OC_DEBUG(0, "OCPF output file: %s", filename);

	ret = ocpf_new_context(filename, &context_id, OCPF_FLAGS_CREATE);
	talloc_free(filename);
	ret = ocpf_write_init(context_id, fid);

	ret = ocpf_write_auto(context_id, &obj_message, &lpProps);
	if (ret == OCPF_SUCCESS) {
		ret = ocpf_write_commit(context_id);
	} 

	ret = ocpf_del_context(context_id);

	ret = ocpf_release();

	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);

	return true;
}


static bool openchangeclient_freebusy(mapi_object_t *obj_store, struct oclient *oclient)
{
	enum MAPISTATUS			retval;
	struct SRow			aRow;
	const char     			*message_name;
	uint32_t			i;
	const uint32_t			*publish_start;
	const uint32_t			*publish_end;
	const struct LongArray_r       	*busy_months;
	const struct BinaryArray_r	*busy_events;
	const struct LongArray_r       	*tentative_months;
	const struct BinaryArray_r	*tentative_events;
	const struct LongArray_r       	*oof_months;
	const struct BinaryArray_r	*oof_events;
	uint32_t			year;
	uint32_t			event_year;

	/* Step 1. Retrieve FreeBusy data for the given user */
	retval = GetUserFreeBusyData(obj_store, oclient->freebusy, &aRow);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Step 2. Dump properties */
	message_name = (const char *) find_SPropValue_data(&aRow, PR_NORMALIZED_SUBJECT);
	publish_start = (const uint32_t *) find_SPropValue_data(&aRow, PR_FREEBUSY_PUBLISH_START);
	publish_end = (const uint32_t *) find_SPropValue_data(&aRow, PR_FREEBUSY_PUBLISH_END);
	busy_months = (const struct LongArray_r *) find_SPropValue_data(&aRow, PR_SCHDINFO_MONTHS_BUSY);
	busy_events = (const struct BinaryArray_r *) find_SPropValue_data(&aRow, PR_SCHDINFO_FREEBUSY_BUSY);
	tentative_months = (const struct LongArray_r *) find_SPropValue_data(&aRow, PR_SCHDINFO_MONTHS_TENTATIVE);
	tentative_events = (const struct BinaryArray_r *) find_SPropValue_data(&aRow, PR_SCHDINFO_FREEBUSY_TENTATIVE);
	oof_months = (const struct LongArray_r *) find_SPropValue_data(&aRow, PR_SCHDINFO_MONTHS_OOF);
	oof_events = (const struct BinaryArray_r *) find_SPropValue_data(&aRow, PR_SCHDINFO_FREEBUSY_OOF);

	year = GetFreeBusyYear(publish_start);

	OC_DEBUG(0, "FreeBusy (%s):", message_name);
	mapidump_date_SPropValue(aRow.lpProps[1], "* FreeBusy Last Modification Time", "\t");
	mapidump_freebusy_date(*publish_start, "\t* FreeBusy Publishing Start:");
	mapidump_freebusy_date(*publish_end, "\t *FreeBusy Publishing End:  ");

	if (busy_months && ((*(const uint32_t *) busy_months) != MAPI_E_NOT_FOUND) &&
	    busy_events && ((*(const uint32_t *) busy_events) != MAPI_E_NOT_FOUND)) {
		OC_DEBUG(0, "\t* Busy Events:");
		for (i = 0; i < busy_months->cValues; i++) {
			event_year = mapidump_freebusy_year(busy_months->lpl[i], year);
			OC_DEBUG(0, "\t\t* %s %u:", mapidump_freebusy_month(busy_months->lpl[i], event_year), event_year);
			mapidump_freebusy_event(&busy_events->lpbin[i], busy_months->lpl[i], event_year, "\t\t\t* ");
		}
	}

	if (tentative_months && ((*(const uint32_t *) tentative_months) != MAPI_E_NOT_FOUND) &&
	    tentative_events && ((*(const uint32_t *) tentative_events) != MAPI_E_NOT_FOUND)) {
		OC_DEBUG(0, "\t* Tentative Events:");
		for (i = 0; i < tentative_months->cValues; i++) {
			event_year = mapidump_freebusy_year(tentative_months->lpl[i], year);
			OC_DEBUG(0, "\t\t* %s %u:", mapidump_freebusy_month(tentative_months->lpl[i], event_year), event_year);
			mapidump_freebusy_event(&tentative_events->lpbin[i], tentative_months->lpl[i], event_year, "\t\t\t* ");
		}
	}

	if (oof_months && ((*(const uint32_t *) oof_months) != MAPI_E_NOT_FOUND) &&
	    oof_events && ((*(const uint32_t *) oof_events) != MAPI_E_NOT_FOUND)) {
		OC_DEBUG(0, "\t* Out Of Office Events:");
		for (i = 0; i < oof_months->cValues; i++) {
			event_year = mapidump_freebusy_year(oof_months->lpl[i], year);
			OC_DEBUG(0, "\t\t* %s %u:", mapidump_freebusy_month(oof_months->lpl[i], event_year), event_year);
			mapidump_freebusy_event(&oof_events->lpbin[i], oof_months->lpl[i], event_year, "\t\t\t* ");
		}
	}

	MAPIFreeBuffer(aRow.lpProps);

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
	bool			opt_ocpf_syntax = false;
	bool			opt_ocpf_sender = false;
	const char		*opt_profdb = NULL;
	char			*opt_profname = NULL;
	const char		*opt_username = NULL;
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
	      OPT_MAPI_EMAIL, OPT_MAPI_FULLNAME, OPT_MAPI_CARDNAME,
	      OPT_MAPI_TASKSTATUS, OPT_MAPI_IMPORTANCE, OPT_MAPI_LABEL, OPT_PF, 
	      OPT_FOLDER, OPT_MAPI_COLOR, OPT_SENDNOTE, OPT_MKDIR, OPT_RMDIR,
	      OPT_FOLDER_NAME, OPT_FOLDER_COMMENT, OPT_USERLIST, OPT_MAPI_PRIVATE,
	      OPT_UPDATE, OPT_DELETEITEMS, OPT_OCPF_FILE, OPT_OCPF_SYNTAX,
	      OPT_OCPF_SENDER, OPT_OCPF_DUMP, OPT_FREEBUSY, OPT_FORCE, OPT_FETCHSUMMARY,
	      OPT_USERNAME };

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path", NULL },
		{"pf", 0, POPT_ARG_NONE, NULL, OPT_PF, "access public folders instead of mailbox", NULL },
		{"profile", 'p', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name", NULL },
		{"password", 'P', POPT_ARG_STRING, NULL, OPT_PASSWORD, "set the profile password", NULL },
		{"username", 0, POPT_ARG_STRING, NULL, OPT_USERNAME, "set the username of the mailbox to use", NULL },
		{"sendmail", 'S', POPT_ARG_NONE, NULL, OPT_SENDMAIL, "send a mail", NULL },
		{"sendappointment", 0, POPT_ARG_NONE, NULL, OPT_SENDAPPOINTMENT, "send an appointment", NULL },
		{"sendcontact", 0, POPT_ARG_NONE, NULL, OPT_SENDCONTACT, "send a contact", NULL },
		{"sendtask", 0, POPT_ARG_NONE, NULL, OPT_SENDTASK, "send a task", NULL },
		{"sendnote", 0, POPT_ARG_NONE, NULL, OPT_SENDNOTE, "send a note", NULL },
		{"fetchmail", 'F', POPT_ARG_NONE, NULL, OPT_FETCHMAIL, "fetch user INBOX mails", NULL },
		{"fetchsummary", 0, POPT_ARG_NONE, NULL, OPT_FETCHSUMMARY, "fetch message summaries only", NULL },
		{"storemail", 'G', POPT_ARG_STRING, NULL, OPT_STOREMAIL, "retrieve a mail on the filesystem", NULL },
		{"fetch-items", 'i', POPT_ARG_STRING, NULL, OPT_FETCHITEMS, "fetch specified user INBOX items", NULL },
		{"freebusy", 0, POPT_ARG_STRING, NULL, OPT_FREEBUSY, "display free / busy information for the specified user", NULL },
		{"force", 0, POPT_ARG_NONE, NULL, OPT_FORCE, "force openchangeclient behavior in some circumstances", NULL },
		{"delete", 0, POPT_ARG_STRING, NULL, OPT_DELETEITEMS, "delete a message given its unique ID", NULL },
		{"update", 'u', POPT_ARG_STRING, NULL, OPT_UPDATE, "update the specified item", NULL },
		{"mailbox", 'm', POPT_ARG_NONE, NULL, OPT_MAILBOX, "list mailbox folder summary", NULL },
		{"deletemail", 'D', POPT_ARG_NONE, NULL, OPT_DELETEMAIL, "delete a mail from user INBOX", NULL },
		{"attachments", 'A', POPT_ARG_STRING, NULL, OPT_ATTACH, "send a list of attachments", NULL },
		{"html-inline", 'I', POPT_ARG_STRING, NULL, OPT_HTML_INLINE, "send PR_HTML content", NULL },
		{"html-file", 'W', POPT_ARG_STRING, NULL, OPT_HTML_FILE, "use HTML file as content", NULL },
		{"to", 't', POPT_ARG_STRING, NULL, OPT_MAPI_TO, "set the To recipients", NULL },
		{"cc", 'c', POPT_ARG_STRING, NULL, OPT_MAPI_CC, "set the Cc recipients", NULL },
		{"bcc", 'b', POPT_ARG_STRING, NULL, OPT_MAPI_BCC, "set the Bcc recipients", NULL },
		{"subject", 's', POPT_ARG_STRING, NULL, OPT_MAPI_SUBJECT, "set the mail subject", NULL },
		{"body", 'B', POPT_ARG_STRING, NULL, OPT_MAPI_BODY, "set the mail body", NULL },
		{"location", 0, POPT_ARG_STRING, NULL, OPT_MAPI_LOCATION, "set the item location", NULL },
		{"label", 0, POPT_ARG_STRING, NULL, OPT_MAPI_LABEL, "set the event label", NULL },
		{"dtstart", 0, POPT_ARG_STRING, NULL, OPT_MAPI_STARTDATE, "set the event start date", NULL },
		{"dtend", 0, POPT_ARG_STRING, NULL, OPT_MAPI_ENDDATE, "set the event end date", NULL },
		{"busystatus", 0, POPT_ARG_STRING, NULL, OPT_MAPI_BUSYSTATUS, "set the item busy status", NULL },
		{"taskstatus", 0, POPT_ARG_STRING, NULL, OPT_MAPI_TASKSTATUS, "set the task status", NULL },
		{"importance", 0, POPT_ARG_STRING, NULL, OPT_MAPI_IMPORTANCE, "Set the item importance", NULL },
		{"email", 0, POPT_ARG_STRING, NULL, OPT_MAPI_EMAIL, "set the email address", NULL },
		{"fullname", 0, POPT_ARG_STRING, NULL, OPT_MAPI_FULLNAME, "set the full name", NULL },
		{"cardname", 0, POPT_ARG_STRING, NULL, OPT_MAPI_CARDNAME, "set a contact card name", NULL },
		{"color", 0, POPT_ARG_STRING, NULL, OPT_MAPI_COLOR, "set the note color", NULL },
		{"notifications", 0, POPT_ARG_NONE, NULL, OPT_NOTIFICATIONS, "monitor INBOX newmail notifications", NULL },
		{"folder", 0, POPT_ARG_STRING, NULL, OPT_FOLDER, "set the folder to use instead of inbox", NULL },
		{"mkdir", 0, POPT_ARG_NONE, NULL, OPT_MKDIR, "create a folder", NULL },
		{"rmdir", 0, POPT_ARG_NONE, NULL, OPT_RMDIR, "delete a folder", NULL },
		{"userlist", 0, POPT_ARG_NONE, NULL, OPT_USERLIST, "list Address Book entries", NULL },
		{"folder-name", 0, POPT_ARG_STRING, NULL, OPT_FOLDER_NAME, "set the folder name", NULL },
		{"folder-comment", 0, POPT_ARG_STRING, NULL, OPT_FOLDER_COMMENT, "set the folder comment", NULL },
		{"debuglevel", 'd', POPT_ARG_STRING, NULL, OPT_DEBUG, "set Debug Level", NULL },
		{"dump-data", 0, POPT_ARG_NONE, NULL, OPT_DUMPDATA, "dump the hex data", NULL },
		{"private", 0, POPT_ARG_NONE, NULL, OPT_MAPI_PRIVATE, "set the private flag on messages", NULL },
		{"ocpf-file", 0, POPT_ARG_STRING, NULL, OPT_OCPF_FILE, "set OCPF file", NULL },
		{"ocpf-dump", 0, POPT_ARG_STRING, NULL, OPT_OCPF_DUMP, "dump message into OCPF file", NULL },
		{"ocpf-syntax", 0, POPT_ARG_NONE, NULL, OPT_OCPF_SYNTAX, "check OCPF files syntax", NULL },
		{"ocpf-sender", 0, POPT_ARG_NONE, NULL, OPT_OCPF_SENDER, "send message using OCPF files contents", NULL },
		POPT_OPENCHANGE_VERSION
		{NULL, 0, 0, NULL, 0, NULL, NULL}
	};

	mem_ctx = talloc_named(NULL, 0, "openchangeclient");

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
			opt_profname = talloc_strdup(mem_ctx, poptGetOptArg(pc));
			break;
		case OPT_PASSWORD:
			opt_password = poptGetOptArg(pc);
			break;
		case OPT_USERNAME:
			opt_username = talloc_strdup(mem_ctx, poptGetOptArg(pc));
			break;
		case OPT_MAILBOX:
			opt_mailbox = true;
			break;
		case OPT_FETCHITEMS:
			opt_fetchitems = poptGetOptArg(pc);
			break;
		case OPT_FETCHSUMMARY:
			oclient.summary = true;
			break;
		case OPT_DELETEITEMS:
			oclient.delete = poptGetOptArg(pc);
			break;
		case OPT_FREEBUSY:
			oclient.freebusy = poptGetOptArg(pc);
			break;
		case OPT_UPDATE:
			oclient.update = poptGetOptArg(pc);
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
		case OPT_OCPF_FILE:
		{
			struct ocpf_file	*element;
			
			if (!oclient.ocpf_files) {
				oclient.ocpf_files = talloc_zero(mem_ctx, struct ocpf_file);
			}
			
			element = talloc_zero(mem_ctx, struct ocpf_file);
			element->filename = talloc_strdup(mem_ctx, poptGetOptArg(pc));
			DLIST_ADD(oclient.ocpf_files, element);
			break;
		}
		case OPT_OCPF_SYNTAX:
			opt_ocpf_syntax = true;
			break;
		case OPT_OCPF_SENDER:
			opt_ocpf_sender = true;
			break;
		case OPT_OCPF_DUMP:
			oclient.ocpf_dump = poptGetOptArg(pc);
			break;
		case OPT_FORCE:
			oclient.force = true;
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
	    (oclient.pf == true && !oclient.folder && !opt_mailbox && !oclient.freebusy)) {
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

	/* One of the rare options which doesn't require MAPI to get
	 *   initialized 
	 */
	if (opt_ocpf_syntax) {
		bool ret = openchangeclient_ocpf_syntax(&oclient);
		mapi_errstr("OCPF Syntax", GetLastError());
		if (ret != true) {
			exit(1);
		}
		exit (0);
	}
	
	/**
	 * Initialize MAPI subsystem
	 */

	retval = MAPIInitialize(&oclient.mapi_ctx, opt_profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	/* debug options */
	SetMAPIDumpData(oclient.mapi_ctx, opt_dumpdata);

	if (opt_debug) {
		SetMAPIDebugLevel(oclient.mapi_ctx, atoi(opt_debug));
	}
	
	/* If no profile is specified try to load the default one from
	 * the database 
	 */

	if (!opt_profname) {
		retval = GetDefaultProfile(oclient.mapi_ctx, &opt_profname);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", GetLastError());
			exit (1);
		}
	}

	if (opt_userlist) {
		retval = MapiLogonProvider(oclient.mapi_ctx, &session, 
					   opt_profname, opt_password,
					   PROVIDER_ID_NSPI);
		if (false == openchangeclient_userlist(mem_ctx, session)) {
			exit(1);
		} else {
			exit(0);
		}
	}

	retval = MapiLogonEx(oclient.mapi_ctx, &session, opt_profname, opt_password);
	talloc_free(opt_profname);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", GetLastError());
		exit (1);
	}

	/**
	 * Open Default Message Store
	 */

	mapi_object_init(&obj_store);
	if (oclient.pf == true) {
		retval = OpenPublicFolder(session, &obj_store);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("OpenPublicFolder", GetLastError());
			exit (1);
		}
	} else if (opt_username) {
		retval = OpenUserMailbox(session, opt_username, &obj_store);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("OpenUserMailbox", GetLastError());
			exit (1);
		}
	} else {
		retval = OpenMsgStore(session, &obj_store);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("OpenMsgStore", GetLastError());
			exit (1);
		}
	}

	/**
	 * OCPF sending command
	 */
	if (opt_ocpf_sender) {
		bool ret = openchangeclient_ocpf_sender(mem_ctx, &obj_store, &oclient);
		mapi_errstr("OCPF Sender", GetLastError());
		if (ret != true) {
			goto end;
		}
	}

	if (oclient.ocpf_dump) {
		bool ret = openchangeclient_ocpf_dump(mem_ctx, &obj_store, &oclient);
		mapi_errstr("OCPF Dump", GetLastError());
		if (ret != true) {
			goto end;
		}
	}

	if (opt_fetchitems) {
		bool ret = openchangeclient_fetchitems(mem_ctx, &obj_store, opt_fetchitems, &oclient);
		mapi_errstr("fetchitems", GetLastError());
		if (ret != true) {
			goto end;
		}
	}

	if (oclient.delete) {
		bool ret = openchangeclient_deleteitems(mem_ctx, &obj_store, &oclient);
		mapi_errstr("deleteitems", GetLastError());
		if (ret != true) {
			goto end;
		}
	}

	if (opt_mailbox) {
		if (oclient.pf == true) {
			bool ret = openchangeclient_pf(mem_ctx, &obj_store);
			mapi_errstr("public folder", GetLastError());
			if (ret != true) {
				goto end;
			}
		} else {
			bool ret = openchangeclient_mailbox(mem_ctx, &obj_store);
			mapi_errstr("mailbox", GetLastError());
			if (ret != true) {
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
		bool ret = openchangeclient_deletemail(mem_ctx, &obj_store, &oclient);
		mapi_errstr("deletemail", GetLastError());
		if (ret != true) {
			goto end;
		}
	}

	/* MAPI calendar operations */
	if (opt_sendappointment) {
		bool ret;
		if (!oclient.dtstart && !oclient.update) {
			printf("You need to specify a start date (e.g: 2007-06-01 22:30:00)\n");
			goto end;
		}
		
		if (!oclient.dtend && !oclient.update) {
			printf("Setting default end date\n");
			oclient.dtend = oclient.dtstart;
		}

		if (!oclient.update) {
			ret = openchangeclient_sendappointment(mem_ctx, &obj_store, &oclient);
			mapi_errstr("sendappointment", GetLastError());
		} else {
			ret = openchangeclient_updateitem(mem_ctx, &obj_store, &oclient, IPF_APPOINTMENT);
			mapi_errstr("update appointment", GetLastError());
		}
		if (ret != true) {
			goto end;
		}
	}

	if (oclient.freebusy) {
		bool ret = openchangeclient_freebusy(&obj_store, &oclient);
		mapi_errstr("freebusy", GetLastError());

		if (ret != true) {
			goto end;
		}
	}

	/* MAPI contact operations */
	if (opt_sendcontact) {
		bool ret;
		if (!oclient.update) {
			ret = openchangeclient_sendcontact(mem_ctx, &obj_store, &oclient);
			mapi_errstr("sendcontact", GetLastError());
		} else {
			ret = openchangeclient_updateitem(mem_ctx, &obj_store, &oclient, IPF_CONTACT);
			mapi_errstr("update contact", GetLastError());
		}
		if (ret != true) {
			goto end;
		}
	}

	/* MAPI task operations */
	if (opt_sendtask) {
		bool ret;
		if (!oclient.dtstart && !oclient.update) {
			printf("You need to specify a start date (e.g: 2007-06-01 22:30:00)\n");
			goto end;
		}
		
		if (!oclient.dtend && !oclient.update) {
			printf("Setting default end date\n");
			oclient.dtend = oclient.dtstart;
		}

		if (!oclient.update) {
			ret = openchangeclient_sendtask(mem_ctx, &obj_store, &oclient);
			mapi_errstr("sendtask", GetLastError());
		} else {
			ret = openchangeclient_updateitem(mem_ctx, &obj_store, &oclient, IPF_TASK);
			mapi_errstr("update task", GetLastError());
		}
		if (ret != true) {
			goto end;
		}
	}

	/* MAPI note operations */
	if (opt_sendnote) {
		bool ret;
		if (!oclient.update) {
			ret = openchangeclient_sendnote(mem_ctx, &obj_store, &oclient);
			mapi_errstr("sendnote", GetLastError());
		} else {
			ret = openchangeclient_updateitem(mem_ctx, &obj_store, &oclient, IPF_STICKYNOTE);
			mapi_errstr("update note", GetLastError());
		}
		if (ret != true) {
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

	poptFreeContext(pc);

	mapi_object_release(&obj_store);

	MAPIUninitialize(oclient.mapi_ctx);

	talloc_free(mem_ctx);

	return 0;
}
