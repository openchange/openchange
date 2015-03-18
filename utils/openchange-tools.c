/*
   Convenient functions for openchange tools

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
#include "openchange-tools.h"

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


/*
 * Retrieve the property value for a given SRow and property tag.  
 *
 * If the property type is a string: fetch PT_UNICODE then PT_STRING8
 * in case the desired property is not available in first choice.
 *
 * Fetch property normally for any others properties
 */
_PUBLIC_ void *octool_get_propval(struct SRow *aRow, uint32_t proptag)
{
	const char	*str;

	if (((proptag & 0xFFFF) == PT_STRING8) ||
	    ((proptag & 0xFFFF) == PT_UNICODE)) {
		proptag = (proptag & 0xFFFF0000) | PT_UNICODE;
		str = (const char *) find_SPropValue_data(aRow, proptag);
		if (str) return (void *)str;

		proptag = (proptag & 0xFFFF0000) | PT_STRING8;
		str = (const char *) find_SPropValue_data(aRow, proptag);
		return (void *)str;
	} 

	return (void *)find_SPropValue_data(aRow, proptag);
}


/*
 * Read a stream and store it in a DATA_BLOB
 */
_PUBLIC_ enum MAPISTATUS octool_get_stream(TALLOC_CTX *mem_ctx,
					 mapi_object_t *obj_stream, 
					 DATA_BLOB *body)
{
	enum MAPISTATUS	retval;
	uint16_t	read_size;
	uint8_t		buf[0x1000];

	body->length = 0;
	body->data = talloc_zero(mem_ctx, uint8_t);

	do {
		retval = ReadStream(obj_stream, buf, 0x1000, &read_size);
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


/*
 * Fetch the body given PR_MSG_EDITOR_FORMAT property value
 */
_PUBLIC_ enum MAPISTATUS octool_get_body(TALLOC_CTX *mem_ctx,
				       mapi_object_t *obj_message,
				       struct SRow *aRow,
				       DATA_BLOB *body)
{
	enum MAPISTATUS			retval;
	const struct SBinary_short	*bin;
	mapi_object_t			obj_stream;
	char				*data;
	uint8_t				format;

	/* Sanity checks */
	MAPI_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);

	/* initialize body DATA_BLOB */
	body->data = NULL;
	body->length = 0;

	retval = GetBestBody(obj_message, &format);
	MAPI_RETVAL_IF(retval, retval, NULL);

	switch (format) {
	case olEditorText:
		data = octool_get_propval(aRow, PR_BODY_UNICODE);
		if (data) {
			body->data = talloc_memdup(mem_ctx, data, strlen(data));
			body->length = strlen(data);
		} else {
			mapi_object_init(&obj_stream);
			retval = OpenStream(obj_message, PR_BODY_UNICODE, 0, &obj_stream);
			MAPI_RETVAL_IF(retval, GetLastError(), NULL);
			
			retval = octool_get_stream(mem_ctx, &obj_stream, body);
			MAPI_RETVAL_IF(retval, GetLastError(), NULL);
			
			mapi_object_release(&obj_stream);
		}
		break;
	case olEditorHTML:
		bin = (const struct SBinary_short *) octool_get_propval(aRow, PR_HTML);
		if (bin) {
			body->data = talloc_memdup(mem_ctx, bin->lpb, bin->cb);
			body->length = bin->cb;
		} else {
			mapi_object_init(&obj_stream);
			retval = OpenStream(obj_message, PR_HTML, 0, &obj_stream);
			MAPI_RETVAL_IF(retval, GetLastError(), NULL);

			retval = octool_get_stream(mem_ctx, &obj_stream, body);
			MAPI_RETVAL_IF(retval, GetLastError(), NULL);

			mapi_object_release(&obj_stream);
		}			
		break;
	case olEditorRTF:
		mapi_object_init(&obj_stream);

		retval = OpenStream(obj_message, PR_RTF_COMPRESSED, 0, &obj_stream);
		MAPI_RETVAL_IF(retval, GetLastError(), NULL);

		retval = WrapCompressedRTFStream(&obj_stream, body);
		MAPI_RETVAL_IF(retval, GetLastError(), NULL);

		mapi_object_release(&obj_stream);
		break;
	default:
		OC_DEBUG(0, "Undefined Body");
		break;
	}

	return MAPI_E_SUCCESS;
}


/*
 * Optimized dump message routine (use GetProps rather than GetPropsAll)
 */
_PUBLIC_ enum MAPISTATUS octool_message(TALLOC_CTX *mem_ctx,
					mapi_object_t *obj_message)
{
	enum MAPISTATUS			retval;
	struct SPropTagArray		*SPropTagArray;
	struct SPropValue		*lpProps;
	struct SRow			aRow;
	uint32_t			count;
	/* common email fields */
	const char			*msgid;
	const char			*from, *to, *cc, *bcc;
	const char			*subject;
	DATA_BLOB			body;
	const uint8_t			*has_attach;
	const uint32_t			*cp;
	const char			*codepage;

	/* Build the array of properties we want to fetch */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x13,
					  PR_INTERNET_MESSAGE_ID,
					  PR_INTERNET_MESSAGE_ID_UNICODE,
					  PR_CONVERSATION_TOPIC,
					  PR_CONVERSATION_TOPIC_UNICODE,
					  PR_MSG_EDITOR_FORMAT,
					  PR_BODY_UNICODE,
					  PR_HTML,
					  PR_RTF_IN_SYNC,
					  PR_RTF_COMPRESSED,
					  PR_SENT_REPRESENTING_NAME,
					  PR_SENT_REPRESENTING_NAME_UNICODE,
					  PR_DISPLAY_TO,
					  PR_DISPLAY_TO_UNICODE,
					  PR_DISPLAY_CC,
					  PR_DISPLAY_CC_UNICODE,
					  PR_DISPLAY_BCC,
					  PR_DISPLAY_BCC_UNICODE,
					  PR_HASATTACH,
					  PR_MESSAGE_CODEPAGE);
	lpProps = NULL;
	retval = GetProps(obj_message, MAPI_UNICODE, SPropTagArray, &lpProps, &count);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, retval, NULL);

	/* Build a SRow structure */
	aRow.ulAdrEntryPad = 0;
	aRow.cValues = count;
	aRow.lpProps = lpProps;

	msgid =	(const char *) octool_get_propval(&aRow, PR_INTERNET_MESSAGE_ID);
	subject = (const char *) octool_get_propval(&aRow, PR_CONVERSATION_TOPIC);

	retval = octool_get_body(mem_ctx, obj_message, &aRow, &body);

	if (retval != MAPI_E_SUCCESS) {
		printf("Invalid Message: %s\n", msgid ? msgid : "");
		return GetLastError();
	}
	
	from = (const char *) octool_get_propval(&aRow, PR_SENT_REPRESENTING_NAME);
	to = (const char *) octool_get_propval(&aRow, PR_DISPLAY_TO_UNICODE);
	cc = (const char *) octool_get_propval(&aRow, PR_DISPLAY_CC_UNICODE);
	bcc = (const char *) octool_get_propval(&aRow, PR_DISPLAY_BCC_UNICODE);

	has_attach = (const uint8_t *) octool_get_propval(&aRow, PR_HASATTACH);
	cp = (const uint32_t *) octool_get_propval(&aRow, PR_MESSAGE_CODEPAGE);
	switch (cp ? *cp : 0) {
	case CP_USASCII:
		codepage = "CP_USASCII";
		break;
	case CP_UNICODE:
		codepage = "CP_UNICODE";
		break;
	case CP_JAUTODETECT:
		codepage = "CP_JAUTODETECT";
		break;
	case CP_KAUTODETECT:
		codepage = "CP_KAUTODETECT";
		break;
	case CP_ISO2022JPESC:
		codepage = "CP_ISO2022JPESC";
		break;
	case CP_ISO2022JPSIO:
		codepage = "CP_ISO2022JPSIO";
		break;
	default:
		codepage = "";
		break;
	}

	printf("+-------------------------------------+\n");
	printf("message id: %s\n", msgid ? msgid : "");
	printf("subject: %s\n", subject ? subject : "");
	printf("From: %s\n", from ? from : "");
	printf("To:  %s\n", to ? to : "");
	printf("Cc:  %s\n", cc ? cc : "");
	printf("Bcc: %s\n", bcc ? bcc : "");
	if (has_attach) {
		printf("Attachment: %s\n", *has_attach ? "True" : "False");
	}
	printf("Codepage: %s\n", codepage);
	printf("Body:\n");
	fflush(0);
	if (body.length) {
		write(1, body.data, body.length);
		write(1, "\n", 1);
		fflush(0);
		talloc_free(body.data);
	} 
	return MAPI_E_SUCCESS;
}


/*
 * OpenChange MAPI programs initialization routine
 */
_PUBLIC_ struct mapi_session *octool_init_mapi(struct mapi_context *mapi_ctx,
					       const char *opt_profname,
					       const char *opt_password,
					       uint32_t provider)
{
	enum MAPISTATUS		retval;
	char			*profname = NULL;
	struct mapi_session	*session = NULL;
	TALLOC_CTX		*mem_ctx = NULL;

	mem_ctx = talloc_named(NULL, 0, "octool_init_mapi");
	if (opt_profname) {
		profname = talloc_strdup(mem_ctx, (char *)opt_profname);
	} else {
		retval = GetDefaultProfile(mapi_ctx, &profname);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", GetLastError());
			talloc_free(mem_ctx);
			return NULL;
		}
	}

	if (!provider) {
		retval = MapiLogonEx(mapi_ctx, &session, profname, opt_password);
	} else {
		retval = MapiLogonProvider(mapi_ctx, &session, profname, opt_password, provider);
	}
	MAPIFreeBuffer((char *)profname);

	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", GetLastError());
		talloc_free(mem_ctx);
		return NULL;
	}

	talloc_free(mem_ctx);
	return session;
}
