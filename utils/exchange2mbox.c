/*
   Convert Exchange mails to mbox

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
#include <tdb.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <magic.h>

struct message_ids {
	const char *msgid;
	
	struct message_ids *next;
	struct message_ids *prev;
};

/* Ugly and lazy but working ... */
#define BOUNDARY	"DocE+STaALJfprDB"
#define	MAX_READ_SIZE	0x4000
#define	MESSAGEID	"Message-ID: "

/**
 * Fetch message ids from the existing mbox
 */
static BOOL get_mbox_msgids(TALLOC_CTX *mem_ctx, const char *mbox, struct tdb_context *db)
{
	FILE		*fp;
	uint32_t	read_size;
	char		*line = NULL;
	size_t		size;
	const char	*msgid;
	const char	*id;
	TDB_DATA	key;

	if ((fp = fopen(mbox, "a+")) == NULL) {
		perror("fopen");
		return False;
	}
	
	while ((size = getline(&line, &read_size, fp)) != -1) {
		if (line && !strncmp(line, MESSAGEID, strlen(MESSAGEID))) {
			msgid = strstr(line, MESSAGEID);
			id = msgid + strlen(MESSAGEID);
			key.dptr = (unsigned char *)id;
			key.dsize = strlen(id) - 1;
			if (!tdb_exists(db, key)) {
				if (tdb_store(db, key, key, TDB_INSERT) != 0) {
					printf("Error while inserting %s in the index database\n", msgid);
					return False;
				}
			}
		}

	}
	if (line)
		free(line);
	fclose(fp);

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


static char *get_base64_attachment(TALLOC_CTX *mem_ctx, mapi_object_t obj_attach, uint32_t *size, char **magic)
{
	enum MAPISTATUS	retval;
	const char     	*tmp;
	mapi_object_t	obj_stream;
	uint32_t	stream_size;
	uint32_t	read_size;
	unsigned char  	buf[MAX_READ_SIZE];
	uint32_t	max_read_size = MAX_READ_SIZE;
	DATA_BLOB	data;
	magic_t		cookie = NULL;

	data.length = 0;
	data.data = talloc_size(mem_ctx, *size);

	retval = OpenStream(&obj_attach, PR_ATTACH_DATA_BIN, 0, &obj_stream);
	if (retval != MAPI_E_SUCCESS) return False;

	if (*size < MAX_READ_SIZE) {
		retval = ReadStream(&obj_stream, buf, *size, &read_size);
		if (retval != MAPI_E_SUCCESS) return NULL;
		memcpy(data.data, buf, read_size);
	}

	for (stream_size = 0; stream_size < *size; stream_size += 0x4000) {
		retval = ReadStream(&obj_stream, buf, max_read_size, &read_size);
		if (retval != MAPI_E_SUCCESS) return NULL;
		memcpy(data.data + stream_size, buf, read_size);
	}

	data.length = *size;

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

static BOOL message2mbox(TALLOC_CTX *mem_ctx, int fd, 
			 struct mapi_SPropValue_array *properties, mapi_object_t *obj_message)
{
	enum MAPISTATUS	retval;
	mapi_object_t	obj_tb_attach;
	mapi_object_t	obj_attach;
	uint64_t       	*delivery_date;
	const char	*date = NULL;
	const char	*from = NULL;
	const char	*to = NULL;
	const char	*cc = NULL;
	const char	*bcc = NULL;
	const char	*subject = NULL;
	const char	*msgid;
	const char	*body = NULL;
	const char	*attach_filename;
	uint32_t	*attach_size;
	char		*attachment_data;
	uint32_t	*has_attach = NULL;
	uint32_t	*attach_num = NULL;
	char	*magic;
	char	*line = NULL;
	struct SBinary_short	*html = NULL;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct SRowSet		rowset_attach;
	struct mapi_SPropValue_array properties_array;
	int			i;

	has_attach = (uint32_t *) find_mapi_SPropValue_data(properties, PR_HASATTACH);

	from = (char *) find_mapi_SPropValue_data(properties, PR_SENT_REPRESENTING_NAME);
	to = (char *) find_mapi_SPropValue_data(properties, PR_DISPLAY_TO);
	cc = (char *) find_mapi_SPropValue_data(properties, PR_DISPLAY_CC);
	bcc = (char *) find_mapi_SPropValue_data(properties, PR_DISPLAY_BCC);

	if (!to && !cc && !bcc) {
		return False;
	}

	delivery_date = (uint64_t *)find_mapi_SPropValue_data(properties, PR_MESSAGE_DELIVERY_TIME);
	date = nt_time_string(mem_ctx, *delivery_date);

	subject = (char *) find_mapi_SPropValue_data(properties, PR_CONVERSATION_TOPIC);
	msgid = (char *) find_mapi_SPropValue_data(properties, PR_INTERNET_MESSAGE_ID);
	body = (char *) find_mapi_SPropValue_data(properties, PR_BODY);
	if (!body) {
		body = (char *) find_mapi_SPropValue_data(properties, PR_BODY_UNICODE);
		if (!body) {
			html = (struct SBinary_short *) find_mapi_SPropValue_data(properties, PR_HTML);
		}
	}

	/* First line From */
	line = talloc_asprintf(mem_ctx, "From \"%s\" %s\n", from, date);
	if (line) write(fd, line, strlen(line));
	talloc_free(line);

	/* Second line: Date */
	line = talloc_asprintf(mem_ctx, "Date: %s\n", date);
	if (line) write(fd, line, strlen(line));
	talloc_free(line);

	/* Third line From */
	line = talloc_asprintf(mem_ctx, "From: %s\n", from);
	if (line) write(fd, line, strlen(line));
	talloc_free(line);

	/* To, Cc, Bcc */
	if (to) {
		line = talloc_asprintf(mem_ctx, "To: %s\n", to);
		if (line) write(fd, line, strlen(line));
		talloc_free(line);
	}

	if (cc) {
		line = talloc_asprintf(mem_ctx, "Cc: %s\n", cc);
		if (line) write(fd, line, strlen(line));
		talloc_free(line);
	}

	if (bcc) {
		line = talloc_asprintf(mem_ctx, "Bcc: %s\n", bcc);
		if (line) write(fd, line, strlen(line));
		talloc_free(line);
	}

	/* Subject */
	if (subject) {
		line = talloc_asprintf(mem_ctx, "Subject: %s\n", subject);
		if (line) write(fd, line, strlen(line));
		talloc_free(line);
	}

	if (msgid) {
		line = talloc_asprintf(mem_ctx, "Message-ID: %s\n", msgid);
		if (line) write(fd, line, strlen(line));
		talloc_free(line);
	}

	/* Set multi-type if we have attachment */
	if (has_attach && *has_attach) {
		line = talloc_asprintf(mem_ctx, "Content-Type: multipart/mixed; boundary=\"%s\"\n", BOUNDARY);
		if (line) write(fd, line, strlen(line));
		talloc_free(line);
	}

	/* body */
	if (body) {
		if (has_attach && *has_attach) {
			line = talloc_asprintf(mem_ctx, "--%s\n", BOUNDARY);
			if (line) write(fd, line, strlen(line));
			talloc_free(line);

			line = talloc_asprintf(mem_ctx, "Content-Type: text/plain; charset=us-ascii\n");
			if (line) write(fd, line, strlen(line));
			talloc_free(line);
			
			/* Just display UTF8 content inline */
			line = talloc_asprintf(mem_ctx, "Content-Disposition: inline\n");
			if (line) write(fd, line, strlen(line));
			talloc_free(line);
		}

		line = talloc_asprintf(mem_ctx, "\n%s", body);
		if (line) write(fd, line, strlen(line));
		talloc_free(line);
	} else if (html) {
		if (has_attach && *has_attach) {
			line = talloc_asprintf(mem_ctx, "--%s\n", BOUNDARY);
			if (line) write(fd, line, strlen(line));
			talloc_free(line);
		}

		line = talloc_asprintf(mem_ctx, "Content-Type: \"text/html\"\n");
		if (line) write(fd, line, strlen(line));
		talloc_free(line);		

		

		write(fd, html->lpb, html->cb);
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
				attach_num = (uint32_t *)find_SPropValue_data(&(rowset_attach.aRow[i]), PR_ATTACH_NUM);
				retval = OpenAttach(obj_message, *attach_num, &obj_attach);
				if (retval == MAPI_E_SUCCESS) {
					retval = GetPropsAll(&obj_attach, &properties_array);
					attach_filename = get_filename((char *)(find_mapi_SPropValue_data(&properties_array, PR_ATTACH_FILENAME)));
					attach_size = (uint32_t *) find_mapi_SPropValue_data(&properties_array, PR_ATTACH_SIZE);
					attachment_data = get_base64_attachment(mem_ctx, obj_attach, attach_size, &magic);
					if (attachment_data) {
						line = talloc_asprintf(mem_ctx, "\n\n--%s\n", BOUNDARY);
						if (line) write(fd, line, strlen(line));
						talloc_free(line);

						line = talloc_asprintf(mem_ctx, "Content-Disposition: attachment; filename=\"%s\"\n", attach_filename);
						if (line) write(fd, line, strlen(line));
						talloc_free(line);

						line = talloc_asprintf(mem_ctx, "Content-Type: \"%s\"\n", magic);
						if (line) write(fd, line, strlen(line));
						talloc_free(line);

						line = talloc_asprintf(mem_ctx, "Content-Transfer-Encoding: base64\n\n");
						if (line) write(fd, line, strlen(line));
						talloc_free(line);
						
						write(fd, attachment_data, strlen(attachment_data));
						talloc_free(attachment_data);

						line = talloc_asprintf(mem_ctx, "\n\n--%s--\n\n\n", BOUNDARY);
						if (line) write(fd, line, strlen(line));
						talloc_free(line);
						
					}
				}
			}
		}
		
	}
	
	write(fd, "\n\n\n", 3);

	return True;
}

int main(int argc, const char *argv[])
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct mapi_session		*session = NULL;
	mapi_object_t			obj_store;
	mapi_object_t			obj_inbox;
	mapi_object_t			obj_table;
	mapi_object_t			obj_message;
	mapi_id_t			id_inbox;
	uint32_t			count;
	struct mapi_SPropValue_array	properties_array;
	struct SPropTagArray		*SPropTagArray = NULL;
	struct SRowSet			rowset;
	poptContext			pc;
	int				opt;
	int				fd;
	int				i;
	const char			*opt_profdb = NULL;
	const char			*opt_profname = NULL;
	const char			*opt_mbox = NULL;
	const char			*opt_tdb = NULL;
	const char			*msgid;
	BOOL				opt_populate = False;
	TDB_DATA			key;
	struct tdb_context		*db;


	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_MBOX, OPT_TDB, OPT_POPULATE};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path"},
		{"profile", 'p', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name"},
		{"mbox", 'm', POPT_ARG_STRING, NULL, OPT_MBOX, "set the mbox file"},
		{"index", 'i', POPT_ARG_STRING, NULL, OPT_TDB, "set the message-id index database path"},
		{"populate", 'n', POPT_ARG_NONE, 0, OPT_POPULATE, "populate the index database"},
		{ NULL }
	};

	mem_ctx = talloc_init("exchange2mbox");

	pc = poptGetContext("exchange2mbox", argc, argv, long_options, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_PROFILE_DB:
			opt_profdb = poptGetOptArg(pc);
			break;
		case OPT_PROFILE:
			opt_profname = poptGetOptArg(pc);
			break;
		case OPT_MBOX:
			opt_mbox = poptGetOptArg(pc);
			break;
		case OPT_TDB:
			opt_tdb = poptGetOptArg(pc);
			break;
		case OPT_POPULATE:
			opt_populate = True;
			break;
		}
	}

	/**
	 * Sanity checks
	 */

	if (!opt_profdb) {
		poptPrintUsage(pc, stderr, 0);
		exit (1);
	}

	if (!opt_mbox) {
		poptPrintUsage(pc, stderr, 0);
		exit (1);
	}

	if (!opt_tdb) {
		poptPrintUsage(pc, stderr, 0);
		exit (1);
	}

	/**
	 * open/create the message-id database if it doesn't already exist
	 * store an index for message-ids of the current mbox
	 */
	db = tdb_open(opt_tdb, 0, 0, O_RDWR|O_CREAT, 0600);
	if (!db) {
		printf("Failed to open the database\n");
		exit (1);
	}

	if (opt_populate == True) {
		get_mbox_msgids(mem_ctx, opt_mbox, db);
	}

	/**
	 * Open the MBOX
	 */

	if ((fd = open(opt_mbox, O_CREAT|O_APPEND|O_WRONLY, S_IRWXU)) == -1) {
		perror("open");
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

	/* if no profile is supplied use the default one */
	if (!opt_profname) {
		retval = GetDefaultProfile(&opt_profname, 0);
		if (retval != MAPI_E_SUCCESS) {
			printf("No profile specified and no default profile found\n");
			exit (1);
		}
	}
	
	retval = MapiLogonEx(&session, opt_profname);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", GetLastError());
		exit (1);
	}

	/* Open the default message store */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore", GetLastError());
		exit (1);
	}

	/* Open Inbox */
	retval = GetReceiveFolder(&obj_store, &id_inbox);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	mapi_object_init(&obj_inbox);
	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	mapi_object_init(&obj_table);
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
	
	while ((retval = QueryRows(&obj_table, 0xa, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		for (i = 0; i < rowset.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(&obj_store, rowset.aRow[i].lpProps[0].value.d, rowset.aRow[i].lpProps[1].value.d, 
					     &obj_message);
			if (GetLastError() == MAPI_E_SUCCESS) {
				retval = GetPropsAll(&obj_message, &properties_array);
				MAPI_RETVAL_IF(retval, retval, mem_ctx);
				
				msgid = (char *) find_mapi_SPropValue_data(&properties_array, PR_INTERNET_MESSAGE_ID);
				if (msgid) {
					key.dptr = (unsigned char *)msgid;
					key.dsize = strlen(msgid);
					if (!tdb_exists(db, key)) {
						message2mbox(mem_ctx, fd, &properties_array, &obj_message);
						if (tdb_store(db, key, key, TDB_INSERT) != 0) {
							printf("Error while inserting %s in the index database\n", msgid);
							return False;
						} 
					} 
				}
			}
			mapi_object_release(&obj_message);
		}
	}

	tdb_close(db);
	close(fd);
	mapi_object_release(&obj_table);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);
	MAPIUninitialize();

	talloc_free(mem_ctx);

	return 0;
}
