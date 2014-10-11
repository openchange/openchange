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

#include "libmapi/libmapi.h"
#include <popt.h>
#include <ldb.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <param.h>
#include <magic.h>

#include <string.h>
#include <ctype.h>

#include "openchange-tools.h"

/* Ugly and lazy but working ... */
#define DEFAULT_BOUNDARY_BASE	"DocE+STaALJfprDB"
#define	MESSAGEID	"Message-ID: "
#define	MESSAGEID_LEN	11

/*
 * how much to request at a time,  and it's complex :-(
 * This was 4096 - was getting NT_STATUS_BUFFER_TOO_SMALL loading large
 *                 attachments
 * It must be less than 16K (Windows API is a signed short)
 * If you ask for more than windows allows you get nothing.
 * Asking for too little just dogs the performance
 *
 * found the NTSTATUS error by adding debug to the ReadStream code.
 */
#define	MAX_READ_SIZE	12000

static int message_error = 0;	/* did we get an error processing message */

static bool opt_test = false;

static char boundary_base[128] = DEFAULT_BOUNDARY_BASE;

static time_t start_time;

static char *boundary(int index)
{
	snprintf(boundary_base, sizeof(boundary_base), "%s_%lu_%d",
			DEFAULT_BOUNDARY_BASE, (unsigned long) start_time, index);
	return boundary_base;
}

static void fix_froms(unsigned char *cp, int len)
{
	unsigned char *ep = cp + len;
	for (; cp + 6 < ep; cp++) {
		if (*cp != '\n')
			continue;
		if (strncmp((char *)cp, "\nFrom ", 6) == 0) {
			cp[1] = 'f';
			cp += 5;
		}
	}
}

/*
 * find a Header at the start of a line (or the start of the text)
 */

static char *find_header(char *cp, char *hdr)
{
	char *ep;
	while ((ep = strcasestr(cp, hdr))) {
		if (ep == cp || ep[-1] == '\n')
			return ep;
		cp = ep + 1;
	}
	return NULL;
}


/**
 * delete a message on the exchange server
 */
static bool delete_messages(
	TALLOC_CTX *mem_ctx,
	struct mapi_session	*session,
	char **del_msgid, 
	int del_count)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_inbox;
	mapi_object_t		obj_table;
	mapi_id_t		id_inbox;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	uint32_t		i, j;
	int64_t			id_message;
	struct mapi_profile	*profile;

	if (!del_count || !del_msgid) {
		return false;
	}

	profile = session->profile;

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

	if (retval != MAPI_E_SUCCESS)
		return false;

	while ((retval = QueryRows(&obj_table, 0xa, TBL_ADVANCE, &SRowSet)) == MAPI_E_SUCCESS) {
		if (!SRowSet.cRows)
			break;

		for (i = 0; i < SRowSet.cRows; i++) {
			const char     	*message_id;

			message_id = (const char *)find_SPropValue_data(&(SRowSet.aRow[i]), PR_INTERNET_MESSAGE_ID);

			if (!message_id)
				continue;

			for (j = 0; j < del_count; j++) {
				if (strcmp(message_id, del_msgid[j]) == 0) {
					id_message = SRowSet.aRow[i].lpProps[1].value.d;
					retval = DeleteMessage(&obj_inbox, &id_message, 1);
					if (retval == MAPI_E_SUCCESS) {
						printf("%s deleted from the Exchange server\n",
								del_msgid[j]);
						retval =
						mapi_profile_delete_string_attr(profile->mapi_ctx,
								profile->profname, "Message-ID", del_msgid[j]);
						if (retval)
							printf("%s not deleted from profile %s: err=0x%x\n",
									del_msgid[j], profile->profname, retval);
					} else {
						printf("%s NOT deleted from the Exchange server\n",
								del_msgid[j]);
					}
				}
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
static uint32_t update(
	TALLOC_CTX *mem_ctx, FILE *fp, 
	struct mapi_session	*session)
{
	enum MAPISTATUS		retval;
	struct mapi_profile	*profile;
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
	char			**del_msgids;
	unsigned int		del_count = 0;

	profile = session->profile;

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

			retval = FindProfileAttr(profile, "Message-ID", id);
			if (GetLastError() == MAPI_E_NOT_FOUND) {
				errno = 0;
				printf("[+] Adding %s to %s\n", id, profile->profname);
				retval = mapi_profile_add_string_attr(profile->mapi_ctx, profile->profname, "Message-ID", id);
				if (retval != MAPI_E_SUCCESS) {
					mapi_errstr("mapi_profile_add_string_attr", GetLastError());
#if 0
					talloc_free(profname);
					MAPIUninitialize(profile->mapi_ctx);
#endif
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
	retval = GetProfileAttr(profile, "Message-ID", &count, &prof_msgids);
	if (retval == MAPI_E_NOT_FOUND) {
		printf("No Synchonizing is needed at all\n");
		return MAPI_E_SUCCESS;
	} else if (retval) {
		fprintf(stderr, "GetProfileAttr failed - %x\n", retval);
		return retval;
	}

	if (count != mbox_count) {
		printf("{+] Synchonizing mbox with Exchange mailbox\n");

		del_msgids = talloc_zero(mem_ctx, char *);
		del_count = 0;

		for (i = 0; i < count; i++) {
			found = false;
			for (j = 0; j < mbox_count; j++) {
				if (!strcmp(prof_msgids[i], mbox_msgids[j])) {
					found = true;
				}
			}
			if (found == false) {
				del_msgids = talloc_realloc(mem_ctx, del_msgids, char *, del_count + 2);
				del_msgids[del_count] = talloc_strdup(mem_ctx, prof_msgids[i]);
				del_count++;
			}
		}

		delete_messages(mem_ctx, session, del_msgids, del_count);

		talloc_free(del_msgids);
		del_count = 0;
	} else {
		printf("[+] mbox already synchronized with Exchange Mailbox\n");
	}

	talloc_free(prof_msgids);
	talloc_free(mbox_msgids);

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


static char *get_base64_attachment(TALLOC_CTX *mem_ctx, mapi_object_t *obj_attach, const uint32_t size, char **magic)
{
	enum MAPISTATUS	retval;
	const char     	*tmp;
	char            *ret;
	mapi_object_t	obj_stream;
	uint32_t	stream_size;
	uint16_t	read_size;
	DATA_BLOB	data;
	magic_t		cookie = NULL;

	mapi_object_init(&obj_stream);
	retval = OpenStream(obj_attach, PR_ATTACH_DATA_BIN, 0, &obj_stream);
	if (retval != MAPI_E_SUCCESS) {
		fprintf(stderr, "OpenStream failed %x\n", retval);
		return NULL;
	}

	data.length = 0;
	data.data = talloc_zero_size(mem_ctx, size);

	for (stream_size = 0; stream_size < size; ) {
		/*
		 * exchange can only handle about 4K chunks at a time,  and don't
		 * ask for more or you get none
		 */
		retval = ReadStream(&obj_stream, data.data + stream_size,
				(stream_size + MAX_READ_SIZE < size) ? MAX_READ_SIZE :
					(size - stream_size), &read_size);
		if ((retval != MAPI_E_SUCCESS) || read_size == 0)
			break;
		stream_size += read_size;
	}
	if (retval != MAPI_E_SUCCESS) {
		fprintf(stderr, "ReadStream failed retval=%x read_size=%d "
		                "stream_size=%d size=%d\n",
						retval, read_size, stream_size, size);
		talloc_free(data.data);
		mapi_object_release(&obj_stream);
		return NULL;
	}

	data.length = stream_size;

	if (magic) {
		/* if they want a mime magic string try and autodetect one */
		cookie = magic_open(MAGIC_MIME);
		if (cookie == NULL) {
			fprintf(stderr, "%s,%d - NULL\n", __FILE__, __LINE__);
			printf("%s\n", magic_error(cookie));
			talloc_free(data.data);
			mapi_object_release(&obj_stream);
			return NULL;
		}
		if (magic_load(cookie, NULL) == -1) {
			fprintf(stderr, "%s,%d - NULL\n", __FILE__, __LINE__);
			printf("%s\n", magic_error(cookie));
			talloc_free(data.data);
			mapi_object_release(&obj_stream);
			return NULL;
		}
		tmp = magic_buffer(cookie, (void *)data.data, data.length);
		*magic = talloc_strdup(mem_ctx, tmp);
		magic_close(cookie);
	}

	/* convert attachment to base64 */
	ret = ldb_base64_encode(mem_ctx, (const char *)data.data, data.length);

	talloc_free(data.data);
	mapi_object_release(&obj_stream);

	return ret;
}


#define WRAP_LINES_AT	76

static void write_base64_data(FILE *fp, const char *buf)
{
	int len = strlen(buf);
	size_t n;

	while (len > 0) {
		int chunk = len > WRAP_LINES_AT ? WRAP_LINES_AT : len;

		n = fwrite(buf, len > WRAP_LINES_AT ? WRAP_LINES_AT : len, 1, fp);
		if (n != 1)
			fprintf(stderr, "Error writing %d bytes of base64 attachment: %d\n",
				chunk, ferror(fp));
		len -= chunk;
		buf += chunk;
		fwrite("\n", 1, 1, fp);
	}
}


/*
 * Read a stream and store it in a DATA_BLOB
 */
static enum MAPISTATUS get_stream(TALLOC_CTX *mem_ctx,
					 mapi_object_t *obj_stream, 
					 DATA_BLOB *body)
{
	enum MAPISTATUS	retval;
	uint16_t	read_size;
	uint8_t		buf[MAX_READ_SIZE];

	body->length = 0;
	body->data = talloc_zero(mem_ctx, uint8_t);

	do {
		retval = ReadStream(obj_stream, buf, MAX_READ_SIZE, &read_size);
		MAPI_RETVAL_IF(retval, GetLastError(), body->data);
		if (read_size) {
			body->data = talloc_realloc(mem_ctx, body->data, uint8_t,
						    body->length + read_size);
			memcpy(&(body->data[body->length]), buf, read_size);
			body->length += read_size;
		}
	} while (read_size);

	errno = 0;
	return MAPI_E_SUCCESS;
}

typedef struct {
   DATA_BLOB body;
   char *body_header;
} body_stuff_t;

/* Fetch the body, no fancy junk */
static enum MAPISTATUS get_body(TALLOC_CTX *mem_ctx,
				       mapi_object_t *obj_message,
				       struct SRow *aRow,
					   body_stuff_t body[3],
					   int *body_count)
{
	mapi_object_t		obj_stream;
	enum MAPISTATUS			retval;
	char *data;
	const struct SBinary_short	*bin;

	*body_count = 0;
	memset(body, 0, sizeof(body_stuff_t) * 3);

	data = octool_get_propval(aRow, PR_BODY);
	if (data && strlen(data)) {
		body[*body_count].body.data = talloc_memdup(mem_ctx, data, strlen(data));
		body[*body_count].body.length = strlen(data);
		body[*body_count].body_header = "Content-Type: text/plain; charset=us-ascii\n";
		(*body_count)++;
	}

	bin = (const struct SBinary_short *) octool_get_propval(aRow, PR_HTML);
	if (bin && bin->cb) {
		body[*body_count].body.data = talloc_memdup(mem_ctx, bin->lpb, bin->cb);
		body[*body_count].body.length = bin->cb;
		body[*body_count].body_header = "Content-Type: text/html\n";
		(*body_count)++;
	}
	
#if 0
	bin = (const struct SBinary_short *) octool_get_propval(aRow, PR_RTF_COMPRESSED);
	if (bin && bin->cb) {
		body[*body_count].body.data = talloc_memdup(mem_ctx, bin->lpb, bin->cb);
		body[*body_count].body.length = bin->cb;
		body[*body_count].body_header = "Content-Type: text/rtf\n";
		(*body_count)++;
	}
#endif

	if (*body_count <= 0) {
		printf("No HTML or TEXT, generating TEXT body\n");
		/* generate a body for us ? */
		mapi_object_init(&obj_stream);
		retval = OpenStream(obj_message, PR_BODY, 0, &obj_stream);
		if (retval) {
			fprintf(stderr, "Failed to get a message body, making empty one: %x\n", retval);
			//message_error = 1;
			body[*body_count].body.data = talloc_zero(mem_ctx, uint8_t);
			body[*body_count].body.length = 0;
			body[*body_count].body_header = "Content-Type: text/plain; charset=us-ascii\n";
			(*body_count)++;
			body[*body_count].body.length = 0;
		} else {
			retval = get_stream(mem_ctx, &obj_stream, &body[*body_count].body);
			if (retval) {
				body[*body_count].body.length = 0;
				fprintf(stderr, "HTML ERROR1 %x\n", retval);
			}
		}
		mapi_object_release(&obj_stream);
		if (body[*body_count].body.length) {
			body[*body_count].body_header = "Content-Type: text/plain; charset=us-ascii\n";
			(*body_count)++;
		}
	}

	return MAPI_E_SUCCESS;
}

/**
   Sample mbox mail:

   From Administrator Mon Apr 23 14:43:01 2007
   Archived-At: <outlook:$message_entryID>
   Date: Mon Apr 23 14:43:01 2007
   From: Administrator 
   To: Julien Kerihuel
   Subject: This is the subject

   This is a sample mail

**/

static bool message2mbox(TALLOC_CTX *mem_ctx, FILE *fp, 
			 struct SRow *aRow, mapi_object_t *obj_store,
			 mapi_object_t *obj_folder, mapi_object_t *obj_message,
			 int base_level)
{
	enum MAPISTATUS			retval;
	mapi_object_t			obj_tb_attach;
	mapi_object_t			obj_attach;
	const uint64_t			*delivery_date;
	const char			*date = NULL;
	const char			*to = NULL;
	const char			*cc = NULL;
	const char			*bcc = NULL;
	const char			*from = NULL;
	const char			*subject = NULL;
	const char			*msgid;
	const char                      *msgheaders = NULL;
	const char			*attach_filename;
	const uint32_t			*attach_size;
	char				*attachment_data;
	const uint8_t			*has_attach = NULL;
	const uint32_t			*attach_num = NULL;
	char				*magic;
	char				*line = NULL;
	struct SPropTagArray		*SPropTagArray = NULL;
	struct SPropValue		*lpProps;
	struct SRow			aRow2;
	struct SRowSet			rowset_attach;
	uint32_t			count;
	unsigned int			i;
	int				header_done = 0;
	body_stuff_t			body[3];
	int				body_count = 0;

	has_attach = (const uint8_t *) octool_get_propval(aRow, PR_HASATTACH);
	to = (const char *) octool_get_propval(aRow, PR_DISPLAY_TO);
	cc = (const char *) octool_get_propval(aRow, PR_DISPLAY_CC);
	bcc = (const char *) octool_get_propval(aRow, PR_DISPLAY_BCC);

	delivery_date = (const uint64_t *)octool_get_propval(aRow, PR_MESSAGE_DELIVERY_TIME);
	if (delivery_date) {
		date = nt_time_string(mem_ctx, *delivery_date);
	} else {
		date = "None";
	}

	from = (const char *) octool_get_propval(aRow, PR_SENT_REPRESENTING_NAME);
	if (from == NULL) {
		from = "unknown";
	}

	subject = (const char*) octool_get_propval(aRow, PR_SUBJECT);
	msgid = (const char *) octool_get_propval(aRow, PR_INTERNET_MESSAGE_ID);

	msgheaders = (const char *) octool_get_propval(aRow, PR_TRANSPORT_MESSAGE_HEADERS);

	retval = get_body(mem_ctx, obj_message, aRow, body, &body_count);

	/* First line From - but only if base_level == 0 */
	if (base_level == 0) {
		char				*f, *p;
		const struct SBinary_short	*entry_id;
		struct SBinary_short		entry_id_for_2010;
		uint8_t				*ptr;

		f = talloc_strdup(mem_ctx, from);
		/* strip out all '"'s, ugly but works */
		for (p = f; p && *p; ) {
			if (*p == '"') {
				memmove(p, p+1, strlen(p)); /* gets NUL */
				continue;
			}
			p++;
		}
		fprintf(fp, "From \"%s\" %s\n", f, date);
		entry_id = (const struct SBinary_short *) find_SPropValue_data(aRow, PR_ENTRYID);
		if (!entry_id) {
			EntryIDFromSourceIDForMessage(mem_ctx, obj_store, obj_folder, obj_message, &entry_id_for_2010);
			entry_id = &entry_id_for_2010;
		}
		ptr = entry_id->lpb;
		if (ptr) {
			size_t c = entry_id->cb;
			fprintf(fp, "Archived-At: <outlook:");
			while (c--) {
				fprintf(fp, "%02X", (unsigned char)*ptr++);
			}
			fprintf(fp, ">\n");
		}
  		talloc_free(f);
	}

	if (msgheaders) {
		char *mhalloc, *mhclean, *mhend, *mhp, *mht;
		
		mhalloc = talloc_strdup(mem_ctx, msgheaders);

		mhclean = mhalloc;
		do {
			/* Skip past the comment Exchange adds to the beginning */
			mhclean = strchr(mhclean, '\n');
			if (!mhclean)
				goto old_code;
			mhclean++;
		} while (isspace(*mhclean));
		
		/* Trim off the empty mime parts that Exchange leaves after 
		   the end of the headers */
		mhend = strstr(mhclean, "\n------=");
		if (mhend) {
			mhend++;
			*mhend = '\0';
		}

		/* strip CR/NL */
		while ((mhend = strchr(mhclean, '\r')))
			memmove(mhend, mhend+1, strlen(mhend) /* gets NUL */);

		/*
		 * strip Content-* headers (we make our own), be sure to get
		 * and extended (indented parts) of this header
		 */
		while (1) {
			/*
			 * strip some bad-for-us headers (poor mans lookup table below :-)
			 */
			mhend = find_header(mhclean, "Content-");
			if (!mhend)
				mhend = find_header(mhclean, "X-MS-");
			if (!mhend)
				mhend = find_header(mhclean, "Lines:");
			if (!mhend)
				break;
			mhp = NULL;
			if (mhend) {
				mht = mhend;
				while ((mhp = strchr(mht, '\n'))) {
					mhp++;
					if (!*mhp || !isspace(*mhp))
						break;
					mht = mhp;
				}
			}
			if (mhp) {
				/* match was in the middle of other headers */
				memmove(mhend, mhp, strlen(mhp) + 1);
			} else if (mhend) {
				/* match was at the end of the headers, truncate it */
				*mhend = '\0';
			}
		}

		/* remove any NL's at end of headers */
		mhp = mhclean + strlen(mhclean);
		while (mhp > mhclean && mhp[-1] == '\n')
			*--mhp = '\0';
		
		line = talloc_asprintf(mem_ctx, "%s\n", mhclean);
  		if (line) fwrite(line, strlen(line), 1, fp);
  		talloc_free(line);
		talloc_free(mhalloc);
	}

old_code:
	if (!msgheaders) {
		/* Second line: Date */
		line = talloc_asprintf(mem_ctx, "Date: %s\n", date);
		if (line) {
			fwrite(line, strlen(line), 1, fp);
		}
		talloc_free(line);

		/* Third line From */
		line = talloc_asprintf(mem_ctx, "From: %s\n", from);
		if (line) {
			fwrite(line, strlen(line), 1, fp);
		}
		talloc_free(line);

		/* To, Cc, Bcc */
		if (to) {
			line = talloc_asprintf(mem_ctx, "To: %s\n", to);
			if (line) {
				fwrite(line, strlen(line), 1, fp);
			}
			talloc_free(line);
		}

		if (cc) {
			line = talloc_asprintf(mem_ctx, "Cc: %s\n", cc);
			if (line) {
				fwrite(line, strlen(line), 1, fp);
			}
			talloc_free(line);
		}

		if (bcc) {
			line = talloc_asprintf(mem_ctx, "Bcc: %s\n", bcc);
			if (line) {
				fwrite(line, strlen(line), 1, fp);
			}
			talloc_free(line);
		}

		/* Subject */
		if (subject) {
			line = talloc_asprintf(mem_ctx, "Subject: %s\n", subject);
			if (line) {
				fwrite(line, strlen(line), 1, fp);
			}
			talloc_free(line);
		}

		if (msgid) {
			line = talloc_asprintf(mem_ctx, "Message-ID: %s\n", msgid);
			if (line) {
				fwrite(line, strlen(line), 1, fp);
			}
			talloc_free(line);
		}
	}

	/* Set multi-type if we have attachment mixed for all things/alternative
	 * for just bodies */
	if (has_attach && *has_attach) {
		fprintf(fp, "Content-Type: multipart/mixed; boundary=\"%s\"\n",
				boundary(base_level+0));
	} else if (body_count > 1) {
		fprintf(fp, "Content-Type: multipart/alternative; boundary=\"%s\"\n",
				boundary(base_level+0));
	}

	/* body */
	if (body_count > 0) {
		if ((has_attach && *has_attach) || body_count > 1) {
			/* blank line before content */
			if (!header_done) {
				fwrite("\n", 1, 1, fp);
				header_done = 1;
			}
			fprintf(fp, "--%s\n", boundary(base_level+0));
		}

		/*
		 * if we have more than 1 body we need to do a multipart alternative
		 * within the first attachment
		 */
		if ((has_attach && *has_attach) && body_count > 1) {
			fprintf(fp, "Content-Type: multipart/alternative; boundary=\"%s\"\n", boundary(base_level+1));
			fwrite("\n", 1, 1, fp);
			fprintf(fp, "\n\n--%s\n", boundary(base_level+1));
		}

		/*
		 * output content type
		 */
		fwrite(body[0].body_header, strlen(body[0].body_header), 1, fp);
		fprintf(fp, "Content-Disposition: inline\n");

		/* blank after header */
		fwrite("\n", 1, 1, fp);
		header_done = 1;

		fix_froms(body[0].body.data, body[0].body.length);
		fwrite(body[0].body.data, body[0].body.length, 1, fp);
		talloc_free(body[0].body.data);
	}

	/* blank line before content */
	if (!header_done) {
		fwrite("\n", 1, 1, fp);
		header_done = 1;
	}

	/* do the other bodies before attachments */
	for (i = 1; i < body_count && i < 3; i++) {
		if (has_attach && *has_attach) {
			fprintf(fp, "\n\n--%s\n", boundary(base_level+1));
		} else {
			fprintf(fp, "\n\n--%s\n", boundary(base_level+0));
		}
		fwrite(body[i].body_header, strlen(body[i].body_header), 1, fp);
		fprintf(fp, "Content-Disposition: inline\n");
		fwrite("\n", 1, 1, fp);
		fix_froms(body[i].body.data, body[i].body.length);
		fwrite(body[i].body.data, body[i].body.length, 1, fp);
	}

	if (has_attach && *has_attach) {
		/* close body attachments */
		if (body_count > 1) {
			fprintf(fp, "\n\n--%s--\n", boundary(base_level+1));
		}

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
				//attach_num = (const uint32_t *)find_SPropValue_data(&(rowset_attach.aRow[i]), PR_ATTACH_NUM);
				uint32_t n = rowset_attach.aRow[i].lpProps[0].value.l;
				attach_num = &n;
				retval = OpenAttach(obj_message, *attach_num, &obj_attach);
				if (retval == MAPI_E_SUCCESS) {
					SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
									  PR_ATTACH_FILENAME,
									  PR_ATTACH_LONG_FILENAME,
									  PR_ATTACH_SIZE,
									  PR_ATTACH_MIME_TAG,
									  PR_ATTACH_METHOD);
					lpProps = NULL;
					retval = GetProps(&obj_attach, MAPI_UNICODE, SPropTagArray, &lpProps, &count);
					MAPIFreeBuffer(SPropTagArray);
					if (retval == MAPI_E_SUCCESS) {
						uint32_t *mp, method = -1;

						aRow2.ulAdrEntryPad = 0;
						aRow2.cValues = count;
						aRow2.lpProps = lpProps;

						mp = (uint32_t *) octool_get_propval(&aRow2, PR_ATTACH_METHOD);
						if (mp)
							method = *mp;

						attach_filename = get_filename(octool_get_propval(&aRow2, PR_ATTACH_LONG_FILENAME));
						if (!attach_filename || (attach_filename && !strcmp(attach_filename, ""))) {
							attach_filename = get_filename(octool_get_propval(&aRow2, PR_ATTACH_FILENAME));
						}
						attach_size = (const uint32_t *) octool_get_propval(&aRow2, PR_ATTACH_SIZE);						

						attachment_data = NULL;
						switch (method) {
						case ATTACH_BY_VALUE:
							magic = (char *) octool_get_propval(&aRow2, PR_ATTACH_MIME_TAG);						
							if (magic)
								magic = talloc_strdup(mem_ctx, magic);
							attachment_data = get_base64_attachment(mem_ctx,
									&obj_attach, *attach_size,
									magic ? NULL : &magic);
							if (attachment_data == NULL) {
								message_error = 1;
								fprintf(stderr, "Failed to read attachment for message %s\n", msgid ? msgid : "unknown");
								break;
							}
							fprintf(fp, "\n\n--%s\n", boundary(base_level+0));
							fprintf(fp, "Content-Disposition: attachment; filename=\"%s\"\n", attach_filename);
							fprintf(fp, "Content-Type: %s\n", magic);
							fprintf(fp, "Content-Transfer-Encoding: base64\n\n");
							write_base64_data(fp, attachment_data);
							talloc_free(attachment_data);
							talloc_free(magic);
							break;
						case ATTACH_BY_REFERENCE:
							fprintf(stderr,"ATTACH_BY_REFERENCE unsupported\n");
							message_error = 1;
							break;
						case ATTACH_BY_REF_RESOLVE:
							fprintf(stderr,"ATTACH_BY_REF_RESOLVE unsupported\n");
							message_error = 1;
							break;
						case ATTACH_BY_REF_ONLY:
							fprintf(stderr,"ATTACH_BY_REF_ONLY unsupported\n");
							message_error = 1;
							break;
						case ATTACH_EMBEDDED_MSG: {
							mapi_object_t obj_embeddedmsg;
							struct SPropTagArray	*embTagArray = NULL;
							struct SPropValue		*embProps;
							struct SRow				eRow;
							uint32_t				emb_count = 0;

							mapi_object_init(&obj_embeddedmsg);
							retval = OpenEmbeddedMessage(&obj_attach,
									&obj_embeddedmsg, MAPI_READONLY);
							if (retval != MAPI_E_SUCCESS) {
								fprintf(stderr, "Failed to open Embedded msg: %x\n", retval);
								message_error = 1;
								break;
							}

							embTagArray = set_SPropTagArray(mem_ctx, 0x15,
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
											PR_RTF_IN_SYNC,
											PR_SENT_REPRESENTING_NAME,
											PR_SENT_REPRESENTING_NAME_UNICODE,
											PR_DISPLAY_TO,
											PR_DISPLAY_TO_UNICODE,
											PR_DISPLAY_CC,
											PR_DISPLAY_CC_UNICODE,
											PR_DISPLAY_BCC,
											PR_DISPLAY_BCC_UNICODE,
											PR_HASATTACH,
											PR_TRANSPORT_MESSAGE_HEADERS);
							retval = GetProps(&obj_embeddedmsg, MAPI_UNICODE, embTagArray,
									&embProps, &emb_count);
							MAPIFreeBuffer(embTagArray);

							if (retval != MAPI_E_SUCCESS) {
								fprintf(stderr, "Failed to get Embedded msg props: %x\n", retval);
								message_error = 1;
								break;
							}

							/* Build a SRow structure */
							eRow.ulAdrEntryPad = 0;
							eRow.cValues = emb_count;
							eRow.lpProps = embProps;

							fprintf(fp, "\n\n--%s\n", boundary(base_level+0));
							fprintf(fp, "Content-Type: message/rfc822\n");
							fprintf(fp, "Content-Disposition: inline\n");
							fprintf(fp, "\n");

							message2mbox(mem_ctx, fp, &eRow, NULL, NULL, &obj_embeddedmsg,
									base_level + 2 /* 0 = main, 1 = alt */);
							talloc_free(embProps);
							} break;
						case ATTACH_OLE:
							fprintf(stderr,"ATTACH_OLE unsupported - "
									"allowing message through anyway\n");
							// message_error = 1;
							break;
						default:
							fprintf(stderr, "Unsupported attach method = %d\n", method);
							message_error = 1;
							break;
						}
					}
					MAPIFreeBuffer(lpProps);
				}
			}

			line = talloc_asprintf(mem_ctx, "\n\n--%s--\n\n\n", boundary(base_level+0));
			if (line) {
				fwrite(line, strlen(line), 1, fp);
			}
			talloc_free(line);
		}
	} else if (body_count > 1) {
		/* close body attachments */
		fprintf(fp, "\n\n--%s--\n", boundary(base_level+0));
	}
	
	fwrite("\n\n\n", 3, 1, fp);

	return true;
}



int main(int argc, const char *argv[])
{
	TALLOC_CTX			*mem_ctx = NULL;
	enum MAPISTATUS			retval;
	struct mapi_context		*mapi_ctx = NULL;
	struct mapi_session		*session = NULL;
	struct mapi_profile		*profile = NULL;
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
	      OPT_DEBUG, OPT_DUMPDATA, OPT_TEST};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"test", 't', POPT_ARG_NONE, 0, OPT_TEST, "Do not update server, just download messages to mbox", NULL},
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

	start_time = time(0);

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
		case OPT_TEST:
			opt_test = true;
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

	/**
	 * Initialize MAPI subsystem
	 */

	retval = MAPIInitialize(&mapi_ctx, opt_profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	/* debug options */
	SetMAPIDumpData(mapi_ctx, opt_dumpdata);

	if (opt_debug) {
		SetMAPIDebugLevel(mapi_ctx, atoi(opt_debug));
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
		mapi_errstr("MapiLogonEx", GetLastError());
		exit (1);
	}
	profile = session->profile;

	/* not sure about this,  but it works and it's nice to have it there */
	profile->mapi_ctx = mapi_ctx;

	/* do the updates now */
	if (opt_update == true) {
		retval = update(mem_ctx, fp, session);
		if (retval != MAPI_E_SUCCESS) {
			printf("Problem encountered during update: %d\n", retval);
			exit (1);
		}
	}
	
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
			if (retval == MAPI_E_SUCCESS) {
				SPropTagArray = set_SPropTagArray(mem_ctx, 0x1c,
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
								  PR_RTF_IN_SYNC,
								  PR_SENT_REPRESENTING_NAME,
								  PR_SENT_REPRESENTING_NAME_UNICODE,
								  PR_DISPLAY_TO,
								  PR_DISPLAY_TO_UNICODE,
								  PR_DISPLAY_CC,
								  PR_DISPLAY_CC_UNICODE,
								  PR_DISPLAY_BCC,
								  PR_DISPLAY_BCC_UNICODE,
								  PR_HASATTACH,
								  PR_TRANSPORT_MESSAGE_HEADERS,
								  PR_SUBJECT_PREFIX,
								  PR_SUBJECT_PREFIX_UNICODE,
								  PR_NORMALIZED_SUBJECT,
								  PR_NORMALIZED_SUBJECT_UNICODE,
								  PR_SUBJECT,
								  PR_SUBJECT_UNICODE,
								  PR_ENTRYID);
				retval = GetProps(&obj_message, MAPI_UNICODE, SPropTagArray, &lpProps, &count);
				MAPIFreeBuffer(SPropTagArray);
				if (retval != MAPI_E_SUCCESS) {
					fprintf(stderr, "Badness getting row %d attrs\n", i);
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
						bool ok;
						
						message_error = 0;
						ok = message2mbox(mem_ctx, fp, &aRow, &obj_store, &obj_inbox, &obj_message, 0);
						if (!ok) {
							printf("Message-ID: %s error, not added to %s\n", msgid, profile->profname);
						} else if (message_error) {
							printf("Message-ID: %s error, ignoring\n", msgid);
							fprintf(stderr, "Message-ID: %s error, ignoring message (check with OWA if you can, will retry next time)\n", msgid);
						} else if (opt_test) {
							printf("Message-ID: %s saved but not updated in %s\n", msgid, profile->profname);
						} else if
						(mapi_profile_add_string_attr(profile->mapi_ctx, profile->profname, "Message-ID", msgid) != MAPI_E_SUCCESS) {
							mapi_errstr("mapi_profile_add_string_attr", GetLastError());
						} else {
							printf("Message-ID: %s added to profile %s\n", msgid, profile->profname);
						}
					} else {
						printf("Message-ID: %s already in profile %s\n", msgid, profile->profname);
					}
				} else {
					fprintf(stderr, "%s: message with no msgid cannot be downloaded\n", profile->profname);
				}
				talloc_free(lpProps);
			} else {
				fprintf(stderr, "could not open row %d: retval=%d GetLastError=%d\n", i, retval, GetLastError());
			}
			mapi_object_release(&obj_message);
			errno = 0;
		}
	}

	fclose(fp);
	mapi_object_release(&obj_table);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);
	MAPIUninitialize(mapi_ctx);

	talloc_free(mem_ctx);

	return 0;
}
