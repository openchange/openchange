/*
   Convert Exchange mails to mbox

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

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <param.h>
#include <magic.h>

#include "openchange-tools.h"

/* Ugly and lazy but working ... */
#define BOUNDARY	"DocE+STaALJfprDB"
#define	MAX_READ_SIZE	0x4000
#define	MESSAGEID	"Message-ID: "
#define	MESSAGEID_LEN	11

/**
 * delete a message on the exchange server
 */
static bool delete_message(TALLOC_CTX *mem_ctx, char *msgid, 
			   const char *profname, const char *password)
{
	enum MAPISTATUS		retval;
	struct mapi_session	*session;
	mapi_object_t		obj_store;
	mapi_object_t		obj_inbox;
	mapi_object_t		obj_table;
	mapi_id_t		id_inbox;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	uint32_t		i;
	uint64_t		id_message;

	if (!msgid) {
		return false;
	}

	retval = MapiLogonEx(&session, profname, password);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Open the default message store */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(session, &obj_store);
	if (retval != MAPI_E_SUCCESS) return false;

	/* Open Inbox */
	retval = GetReceiveFolder(&obj_store, &id_inbox, NULL);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_init(&obj_inbox);
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_init(&obj_table);
	retval = GetContentsTable(&obj_inbox, &obj_table, 0, NULL);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x3,
					  PR_FID,
					  PR_MID,
					  PR_INTERNET_MESSAGE_ID);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;

	while ((retval = QueryRows(&obj_table, 0xa, TBL_ADVANCE, &SRowSet)) == MAPI_E_SUCCESS) {
		if (!SRowSet.cRows) break;
		for (i = 0; i < SRowSet.cRows; i++) {
			const char     	*message_id;

			message_id = (const char *)find_SPropValue_data(&(SRowSet.aRow[i]), PR_INTERNET_MESSAGE_ID);

			if (message_id && !strncmp(message_id, msgid, strlen(msgid))) {
				id_message = SRowSet.aRow[i].lpProps[1].value.d;
				retval = DeleteMessage(&obj_inbox, &id_message, 1);
				if (retval != MAPI_E_SUCCESS) return false;
				break;
			}
		}
	}
	
	mapi_object_release(&obj_table);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);

	return true;
}

/**
 * Fetch message ids from the existing mbox
 */
static uint32_t update(TALLOC_CTX *mem_ctx, FILE *fp, 
		       const char *profdb, char *profname,
		       const char *password)
{
	enum MAPISTATUS		retval;
	struct mapi_profile	profile;
	size_t			read_size;
	char			*line = NULL;
#if !defined(__FreeBSD__)
	ssize_t			size;
#endif
	const char		*msgid;
	char     		*id;
	char			**mbox_msgids;
	char			**prof_msgids;
	unsigned int		mbox_count = 0;
	unsigned int		count;
	unsigned int		i, j;
	bool			found = false;

	retval = MAPIInitialize(profdb);
	MAPI_RETVAL_IF(retval, retval, NULL);

	if (!profname) {
		retval = GetDefaultProfile(&profname);
		MAPI_RETVAL_IF(retval, retval, NULL);
	}

	retval = OpenProfile(&profile, profname, password);
	MAPI_RETVAL_IF(retval, retval, profname);

	mbox_msgids = talloc_zero(mem_ctx, char *);
	/* Add Message-ID attribute to the profile if it is missing */
#if defined(__FreeBSD__)
	while ((line = fgetln(fp, &read_size)) != NULL) {
#else
	while ((size = getline(&line, &read_size, fp)) != -1) {
#endif
		if (line && !strncmp(line, MESSAGEID, strlen(MESSAGEID))) {
			msgid = strstr(line, MESSAGEID);
			id = talloc_strdup(mem_ctx, msgid + strlen(MESSAGEID));
			id[strlen(id) - 1] = 0;

			mbox_msgids = talloc_realloc(mem_ctx, mbox_msgids, char *, mbox_count + 2);
			mbox_msgids[mbox_count] = talloc_strdup(mem_ctx, id);
			mbox_count++;

			retval = FindProfileAttr(&profile, "Message-ID", id);
			if (GetLastError() == MAPI_E_NOT_FOUND) {
				errno = 0;
				printf("[+] Adding %s to %s\n", id, profname);
				retval = mapi_profile_add_string_attr(profname, "Message-ID", id);
				if (retval != MAPI_E_SUCCESS) {
					mapi_errstr("mapi_profile_add_string_attr", GetLastError());
					talloc_free(profname);
					MAPIUninitialize();
					return -1;
				}
			}
			talloc_free(id);
		}
		
	}
	if (line)
		free(line);

	/* Remove Message-ID and update Exchange mailbox if a
	 * Message-ID is missing in mbox 
	 */
	retval = GetProfileAttr(&profile, "Message-ID", &count, &prof_msgids);
	MAPI_RETVAL_IF(retval, retval, profname);

	if (count != mbox_count) {
		printf("{+] Synchonizing mbox with Exchange mailbox\n");
		for (i = 0; i < count; i++) {
			found = false;
			for (j = 0; j < mbox_count; j++) {
				if (!strcmp(prof_msgids[i], mbox_msgids[j])) {
					found = true;
				}
			}
			if (found == false) {
				if (delete_message(mem_ctx, prof_msgids[i], profname, password) == true) {
					printf("%s deleted from the Exchange server\n", prof_msgids[i]);
					mapi_profile_delete_string_attr(profname, "Message-ID", prof_msgids[i]);
				}
			}
		}
	} else {
		printf("[+] mbox already synchronized with Exchange Mailbox\n");
	}

	talloc_free(prof_msgids);
	talloc_free(mbox_msgids);
	talloc_free(profname);
	MAPIUninitialize();

	return MAPI_E_SUCCESS;
}

static const char *get_filename(const char *filename)
{
	const char *substr;

	if (!filename) return NULL;

	substr = rindex(filename, '/');
	if (substr) return substr;

	return filename;
}

/*
  encode as base64
  Samba4 code
  caller frees
*/
static char *ldb_base64_encode(void *mem_ctx, const char *buf, int len)
{
	const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int bit_offset, byte_offset, idx, i;
	const uint8_t *d = (const uint8_t *)buf;
	int bytes = (len*8 + 5)/6, pad_bytes = (bytes % 4) ? 4 - (bytes % 4) : 0;
	char *out;

	out = talloc_array(mem_ctx, char, bytes+pad_bytes+1);
	if (!out) return NULL;

	for (i=0;i<bytes;i++) {
		byte_offset = (i*6)/8;
		bit_offset = (i*6)%8;
		if (bit_offset < 3) {
			idx = (d[byte_offset] >> (2-bit_offset)) & 0x3F;
		} else {
			idx = (d[byte_offset] << (bit_offset-2)) & 0x3F;
			if (byte_offset+1 < len) {
				idx |= (d[byte_offset+1] >> (8-(bit_offset-2)));
			}
		}
		out[i] = b64[idx];
	}

	for (;i<bytes+pad_bytes;i++)
		out[i] = '=';
	out[i] = 0;

	return out;
}


static char *get_base64_attachment(TALLOC_CTX *mem_ctx, mapi_object_t obj_attach, const uint32_t size, char **magic)
{
	enum MAPISTATUS	retval;
	const char     	*tmp;
	mapi_object_t	obj_stream;
	uint32_t	stream_size;
	uint16_t	read_size;
	unsigned char  	buf[MAX_READ_SIZE];
	uint32_t	max_read_size = MAX_READ_SIZE;
	DATA_BLOB	data;
	magic_t		cookie = NULL;

	data.length = 0;
	data.data = talloc_size(mem_ctx, size);

	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 0, &obj_stream);
	if (retval != MAPI_E_SUCCESS) return false;

	if (size < MAX_READ_SIZE) {
		retval = ReadStream(&obj_stream, buf, size, &read_size);
		if (retval != MAPI_E_SUCCESS) return NULL;
		memcpy(data.data, buf, read_size);
	}

	for (stream_size = 0; stream_size < size; stream_size += 0x4000) {
		retval = ReadStream(&obj_stream, buf, max_read_size, &read_size);
		if (retval != MAPI_E_SUCCESS) return NULL;
		memcpy(data.data + stream_size, buf, read_size);
	}

	data.length = size;

	cookie = magic_open(MAGIC_MIME);
	if (cookie == NULL) {
		printf("%s\n", magic_error(cookie));
		return NULL;
	}
	if (magic_load(cookie, NULL) == -1) {
		printf("%s\n", magic_error(cookie));
		return NULL;
	}
	tmp = magic_buffer(cookie, (void *)data.data, data.length);
	*magic = talloc_strdup(mem_ctx, tmp);
	magic_close(cookie);

	/* convert attachment to base64 */
	return (ldb_base64_encode(mem_ctx, (const char *)data.data, data.length));
}

/**
   Sample mbox mail:

   From Administrator Mon Apr 23 14:43:01 2007
   Date: Mon Apr 23 14:43:01 2007
   From: Administrator 
   To: Julien Kerihuel
   Subject: This is the subject

   This is a sample mail

**/

static bool message2mbox(TALLOC_CTX *mem_ctx, FILE *fp, 
			 struct SRow *aRow, mapi_object_t *obj_message)
{
	enum MAPISTATUS			retval;
	mapi_object_t			obj_tb_attach;
	mapi_object_t			obj_attach;
	const uint64_t			*delivery_date;
	const char			*date = NULL;
	const char			*from = NULL;
	const char			*to = NULL;
	const char			*cc = NULL;
	const char			*bcc = NULL;
	const char			*subject = NULL;
	const char			*msgid;
	DATA_BLOB			body;
	const char			*attach_filename;
	const uint32_t			*attach_size;
	char				*attachment_data;
	const uint32_t			*has_attach = NULL;
	const uint32_t			*attach_num = NULL;
	char				*magic;
	char				*line = NULL;
	struct SPropTagArray		*SPropTagArray = NULL;
	struct SPropValue		*lpProps;
	struct SRow			aRow2;
	struct SRowSet			rowset_attach;
	uint32_t			count;
	uint8_t				format;
	unsigned int			i;
	ssize_t				len;

	has_attach = (const uint32_t *) octool_get_propval(aRow, PR_HASATTACH);
	from = (const char *) octool_get_propval(aRow, PR_SENT_REPRESENTING_NAME);
	to = (const char *) octool_get_propval(aRow, PR_DISPLAY_TO);
	cc = (const char *) octool_get_propval(aRow, PR_DISPLAY_CC);
	bcc = (const char *) octool_get_propval(aRow, PR_DISPLAY_BCC);

	if (!to && !cc && !bcc) {
		return false;
	}

	delivery_date = (const uint64_t *)octool_get_propval(aRow, PR_MESSAGE_DELIVERY_TIME);
	if (delivery_date) {
		date = nt_time_string(mem_ctx, *delivery_date);
	} else {
		date = "None";
	}

	subject = (const char *) octool_get_propval(aRow, PR_CONVERSATION_TOPIC);
	msgid = (const char *) octool_get_propval(aRow, PR_INTERNET_MESSAGE_ID);

	retval = octool_get_body(mem_ctx, obj_message, aRow, &body);

	/* First line From */
	line = talloc_asprintf(mem_ctx, "From \"%s\" %s\n", from, date);
	if (line) {
		len = fwrite(line, strlen(line), 1, fp);
	}
	talloc_free(line);

	/* Second line: Date */
	line = talloc_asprintf(mem_ctx, "Date: %s\n", date);
	if (line) {
		len = fwrite(line, strlen(line), 1, fp);
	}
	talloc_free(line);

	/* Third line From */
	line = talloc_asprintf(mem_ctx, "From: %s\n", from);
	if (line) {
		len = fwrite(line, strlen(line), 1, fp);
	}
	talloc_free(line);

	/* To, Cc, Bcc */
	if (to) {
		line = talloc_asprintf(mem_ctx, "To: %s\n", to);
		if (line) {
			len = fwrite(line, strlen(line), 1, fp);
		}
		talloc_free(line);
	}

	if (cc) {
		line = talloc_asprintf(mem_ctx, "Cc: %s\n", cc);
		if (line) {
			len = fwrite(line, strlen(line), 1, fp);
		}
		talloc_free(line);
	}

	if (bcc) {
		line = talloc_asprintf(mem_ctx, "Bcc: %s\n", bcc);
		if (line) {
			len = fwrite(line, strlen(line), 1, fp);
		}
		talloc_free(line);
	}

	/* Subject */
	if (subject) {
		line = talloc_asprintf(mem_ctx, "Subject: %s\n", subject);
		if (line) {
			len = fwrite(line, strlen(line), 1, fp);
		}
		talloc_free(line);
	}

	if (msgid) {
		line = talloc_asprintf(mem_ctx, "Message-ID: %s\n", msgid);
		if (line) {
			len = fwrite(line, strlen(line), 1, fp);
		}
		talloc_free(line);
	}

	/* Set multi-type if we have attachment */
	if (has_attach && *has_attach) {
		line = talloc_asprintf(mem_ctx, "Content-Type: multipart/mixed; boundary=\"%s\"\n", BOUNDARY);
		if (line) {
			len = fwrite(line, strlen(line), 1, fp);
		}
		talloc_free(line);
	}

	/* body */
	if (body.length) {
		if (has_attach && *has_attach) {
			line = talloc_asprintf(mem_ctx, "--%s\n", BOUNDARY);
			if (line) {
				len = fwrite(line, strlen(line), 1, fp);
			}
			talloc_free(line);
		}
		retval = GetBestBody(obj_message, &format);
		switch (format) {
		case olEditorText:
			line = talloc_asprintf(mem_ctx, "Content-Type: text/plain; charset=us-ascii\n");
			if (line) {
				len = fwrite(line, strlen(line), 1, fp);
			}
			talloc_free(line);
			
			/* Just display UTF8 content inline */
			line = talloc_asprintf(mem_ctx, "Content-Disposition: inline\n");
			if (line) {
				len = fwrite(line, strlen(line), 1, fp);
			}
			talloc_free(line);
			break;
		case olEditorHTML:
			line = talloc_asprintf(mem_ctx, "Content-Type: text/html\n");
			if (line) {
				len = fwrite(line, strlen(line), 1, fp);
			}
			talloc_free(line);		
			break;
		case olEditorRTF:
			line = talloc_asprintf(mem_ctx, "Content-Type: text/rtf\n");
			if (line) {
				len = fwrite(line, strlen(line), 1, fp);
			}
			talloc_free(line);					

			line = talloc_asprintf(mem_ctx, "--%s\n", BOUNDARY);
			if (line) {
				len = fwrite(line, strlen(line), 1, fp);
			}
			talloc_free(line);
			break;
		}

		len = fwrite(body.data, body.length, 1, fp);
		talloc_free(body.data);
	} 

	/* We are now fetching attachments */
	if (has_attach && *has_attach) {
		mapi_object_init(&obj_tb_attach);
		retval = GetAttachmentTable(obj_message, &obj_tb_attach);
		if (retval == MAPI_E_SUCCESS) {
			SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_ATTACH_NUM);
			retval = SetColumns(&obj_tb_attach, SPropTagArray);
			MAPIFreeBuffer(SPropTagArray);
			MAPI_RETVAL_IF(retval, retval, NULL);
			
			retval = QueryRows(&obj_tb_attach, 0xa, TBL_ADVANCE, &rowset_attach);
			MAPI_RETVAL_IF(retval, retval, NULL);
			
			for (i = 0; i < rowset_attach.cRows; i++) {
				attach_num = (const uint32_t *)find_SPropValue_data(&(rowset_attach.aRow[i]), PR_ATTACH_NUM);
				retval = OpenAttach(obj_message, *attach_num, &obj_attach);
				if (retval == MAPI_E_SUCCESS) {
					SPropTagArray = set_SPropTagArray(mem_ctx, 0x3,
									  PR_ATTACH_FILENAME,
									  PR_ATTACH_LONG_FILENAME,
									  PR_ATTACH_SIZE);
					lpProps = talloc_zero(mem_ctx, struct SPropValue);
					retval = GetProps(&obj_attach, SPropTagArray, &lpProps, &count);
					MAPIFreeBuffer(SPropTagArray);
					if (retval == MAPI_E_SUCCESS) {
						aRow2.ulAdrEntryPad = 0;
						aRow2.cValues = count;
						aRow2.lpProps = lpProps;

						attach_filename = get_filename(octool_get_propval(&aRow2, PR_ATTACH_LONG_FILENAME));
						if (!attach_filename || (attach_filename && !strcmp(attach_filename, ""))) {
							attach_filename = get_filename(octool_get_propval(&aRow2, PR_ATTACH_FILENAME));
						}
						attach_size = (const uint32_t *) octool_get_propval(&aRow2, PR_ATTACH_SIZE);						
						attachment_data = get_base64_attachment(mem_ctx, obj_attach, *attach_size, &magic);
						if (attachment_data) {
							line = talloc_asprintf(mem_ctx, "\n\n--%s\n", BOUNDARY);
							if (line) {
								len = fwrite(line, strlen(line), 1, fp);
							}
							talloc_free(line);

							line = talloc_asprintf(mem_ctx, "Content-Disposition: attachment; filename=\"%s\"\n", attach_filename);
							if (line) {
								len = fwrite(line, strlen(line), 1, fp);
							}
							talloc_free(line);
							
							line = talloc_asprintf(mem_ctx, "Content-Type: \"%s\"\n", magic);
							if (line) {
								len = fwrite(line, strlen(line), 1, fp);
							}
							talloc_free(line);
							
							line = talloc_asprintf(mem_ctx, "Content-Transfer-Encoding: base64\n\n");
							if (line) {
								len = fwrite(line, strlen(line), 1, fp);
							}
							talloc_free(line);
							
							len = fwrite(attachment_data, strlen(attachment_data), 1, fp);
							talloc_free(attachment_data);
						}
					}
					MAPIFreeBuffer(lpProps);
				}
			}
			if (has_attach && *has_attach) {
				line = talloc_asprintf(mem_ctx, "\n\n--%s--\n\n\n", BOUNDARY);
				if (line) {
					len = fwrite(line, strlen(line), 1, fp);
				}
				talloc_free(line);
			}
		}
		
	}
	
	len = fwrite("\n\n\n", 3, 1, fp);

	return true;
}

int main(int argc, const char *argv[])
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct mapi_session		*session = NULL;
	struct mapi_profile		*profile;
	mapi_object_t			obj_store;
	mapi_object_t			obj_inbox;
	mapi_object_t			obj_table;
	mapi_object_t			obj_message;
	mapi_id_t			id_inbox;
	uint32_t			count;
	struct SPropTagArray		*SPropTagArray = NULL;
	struct SPropValue		*lpProps;
	struct SRow			aRow;
	struct SRowSet			rowset;
	poptContext			pc;
	int				opt;
	FILE				*fp;
	unsigned int			i;
	const char			*opt_profdb = NULL;
	char				*opt_profname = NULL;
	const char			*opt_password = NULL;
	const char			*opt_mbox = NULL;
	bool				opt_update = false;
	bool				opt_dumpdata = false;
	const char			*opt_debug = NULL;
	const char			*msgid;

	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_PASSWORD, OPT_MBOX, OPT_UPDATE,
	      OPT_DEBUG, OPT_DUMPDATA};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path", "PATH"},
		{"profile", 'p', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name", "PROFILE"},
		{"password", 'P', POPT_ARG_STRING, NULL, OPT_PASSWORD, "set the profile password", "PASSWORD"},
		{"mbox", 'm', POPT_ARG_STRING, NULL, OPT_MBOX, "set the mbox file", "FILENAME"},
		{"update", 'u', POPT_ARG_NONE, 0, OPT_UPDATE, "mirror mbox changes back to the Exchange server", NULL},
		{"debuglevel", 'd', POPT_ARG_STRING, NULL, OPT_DEBUG, "set the debug level", "LEVEL"},
		{"dump-data", 0, POPT_ARG_NONE, NULL, OPT_DUMPDATA, "dump the hex data", NULL},
		POPT_OPENCHANGE_VERSION
		{ NULL, 0, POPT_ARG_NONE, NULL, 0, NULL, NULL }
	};

	mem_ctx = talloc_named(NULL, 0, "exchange2mbox");

	pc = poptGetContext("exchange2mbox", argc, argv, long_options, 0);

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
		case OPT_MBOX:
			opt_mbox = poptGetOptArg(pc);
			break;
		case OPT_UPDATE:
			opt_update = true;
			break;
		case OPT_DEBUG:
			opt_debug = poptGetOptArg(pc);
			break;
		case OPT_DUMPDATA:
			opt_dumpdata = true;
			break;
		}
	}

	/**
	 * Sanity checks
	 */

	if (!opt_profdb) {
		opt_profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
	}

	if (!opt_mbox) {
		opt_mbox = talloc_asprintf(mem_ctx, DEFAULT_MBOX, getenv("HOME"));
	}

	/**
	 * Open the MBOX
	 */

	if ((fp = fopen(opt_mbox, "a+")) == NULL) {
		perror("fopen");
		exit (1);
	}

	if (opt_update == true) {
		retval = update(mem_ctx, fp, opt_profdb, opt_profname, opt_password);
		if (GetLastError() != MAPI_E_SUCCESS) {
			printf("Problem encountered during update\n");
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

	/* debug options */
	SetMAPIDumpData(opt_dumpdata);

	if (opt_debug) {
		SetMAPIDebugLevel(atoi(opt_debug));
	}

	/* if no profile is supplied use the default one */
	if (!opt_profname) {
		retval = GetDefaultProfile(&opt_profname);
		if (retval != MAPI_E_SUCCESS) {
			printf("No profile specified and no default profile found\n");
			exit (1);
		}
	}
	
	retval = MapiLogonEx(&session, opt_profname, opt_password);
	talloc_free(opt_profname);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", GetLastError());
		exit (1);
	}
	profile = session->profile;

	/* Open the default message store */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(session, &obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore", GetLastError());
		exit (1);
	}

	/* Open Inbox */
	retval = GetReceiveFolder(&obj_store, &id_inbox, NULL);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	mapi_object_init(&obj_inbox);
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	mapi_object_init(&obj_table);
	retval = GetContentsTable(&obj_inbox, &obj_table, 0, &count);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_INTERNET_MESSAGE_ID);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	while ((retval = QueryRows(&obj_table, 0xa, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		for (i = 0; i < rowset.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(&obj_store, 
					     rowset.aRow[i].lpProps[0].value.d, 
					     rowset.aRow[i].lpProps[1].value.d, 
					     &obj_message, 0);
			if (GetLastError() == MAPI_E_SUCCESS) {
				SPropTagArray = set_SPropTagArray(mem_ctx, 0x13,
								  PR_INTERNET_MESSAGE_ID,
								  PR_INTERNET_MESSAGE_ID_UNICODE,
								  PR_CONVERSATION_TOPIC,
								  PR_CONVERSATION_TOPIC_UNICODE,
								  PR_MESSAGE_DELIVERY_TIME,
								  PR_MSG_EDITOR_FORMAT,
								  PR_BODY,
								  PR_BODY_UNICODE,
								  PR_HTML,
								  PR_RTF_COMPRESSED,
								  PR_SENT_REPRESENTING_NAME,
								  PR_SENT_REPRESENTING_NAME_UNICODE,
								  PR_DISPLAY_TO,
								  PR_DISPLAY_TO_UNICODE,
								  PR_DISPLAY_CC,
								  PR_DISPLAY_CC_UNICODE,
								  PR_DISPLAY_BCC,
								  PR_DISPLAY_BCC_UNICODE,
								  PR_HASATTACH);
				retval = GetProps(&obj_message, SPropTagArray, &lpProps, &count);
				MAPIFreeBuffer(SPropTagArray);
				if (retval != MAPI_E_SUCCESS) {
					exit (1);
				}

				/* Build a SRow structure */
				aRow.ulAdrEntryPad = 0;
				aRow.cValues = count;
				aRow.lpProps = lpProps;

				msgid = (const char *) octool_get_propval(&aRow, PR_INTERNET_MESSAGE_ID);
				if (msgid) {
					retval = FindProfileAttr(profile, "Message-ID", msgid);
					if (GetLastError() == MAPI_E_NOT_FOUND) {
						message2mbox(mem_ctx, fp, &aRow, &obj_message);
						if (mapi_profile_add_string_attr(profile->profname, "Message-ID", msgid) != MAPI_E_SUCCESS) {
							mapi_errstr("mapi_profile_add_string_attr", GetLastError());
						} else {
							printf("Message-ID: %s added to profile %s\n", msgid, profile->profname);
						}
					} 
				}
				talloc_free(lpProps);
			} 
			mapi_object_release(&obj_message);
			errno = 0;
		}
	}

	fclose(fp);
	mapi_object_release(&obj_table);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);
	MAPIUninitialize();

	talloc_free(mem_ctx);

	return 0;
}
