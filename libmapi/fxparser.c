/*
   OpenChange MAPI implementation.

   Copyright (C) Brad Hards <bradh@frogmouth.net> 2010.

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
#include "libmapi/fxparser.h"
#include "libmapi/fxics.h"

#ifdef ENABLE_ASSERTS
#include <assert.h>
#define OC_ASSERT(x) assert(x)
#else
#define OC_ASSERT(x)
#endif

/**
   \file fxparser.c

   \brief Fast Transfer stream parser
 */

static bool pull_uint8_t(struct fx_parser_context *parser, uint8_t *val)
{
	if ((parser->idx) + 1 > parser->data.length) {
		*val = 0;
		return false;
	}
	*val = parser->data.data[parser->idx];
	(parser->idx)++;
	return true;
}

static bool pull_uint16_t(struct fx_parser_context *parser, uint16_t *val)
{
	if ((parser->idx) + 2 > parser->data.length) {
		*val = 0;
		return false;
	}
	*val = parser->data.data[parser->idx];
	(parser->idx)++;
	*val += parser->data.data[parser->idx] << 8;
	(parser->idx)++;
	return true;
}

static bool pull_uint32_t(struct fx_parser_context *parser, uint32_t *val)
{
	if ((parser->idx) + 4 > parser->data.length) {
		*val = 0;
		return false;
	}
	*val = parser->data.data[parser->idx];
	(parser->idx)++;
	*val += parser->data.data[parser->idx] << 8;
	(parser->idx)++;
	*val += parser->data.data[parser->idx] << 16;
	(parser->idx)++;
	*val += parser->data.data[parser->idx] << 24;
	(parser->idx)++;
	return true;
}

static bool pull_tag(struct fx_parser_context *parser)
{
	return pull_uint32_t(parser, &(parser->tag));
}

static bool pull_uint8_data(struct fx_parser_context *parser, uint32_t read_len, uint8_t **data_read)
{
	uint32_t i;
	for (i = 0; i < read_len; i++) {
		if (!pull_uint8_t(parser, (uint8_t*)&((*data_read)[i]))) {
			return false;
		}
	}
	return true;
}

static bool pull_int64_t(struct fx_parser_context *parser, int64_t *val)
{
	int64_t tmp;
	if ((parser->idx) + 8 > parser->data.length) {
		*val = 0;
		return false;
	}
	*val = parser->data.data[parser->idx];
	(parser->idx)++;

	tmp = parser->data.data[parser->idx];
	*val += (tmp << 8);
	(parser->idx)++;

	tmp = parser->data.data[parser->idx];
	*val += (tmp << 16);
	(parser->idx)++;

	tmp = parser->data.data[parser->idx];
	*val += (tmp << 24);
	(parser->idx)++;

	tmp = parser->data.data[parser->idx];
	*val += (tmp << 32);
	(parser->idx)++;

	tmp = parser->data.data[parser->idx];
	*val += (tmp << 40);
	(parser->idx)++;

	tmp = parser->data.data[parser->idx];
	*val += (tmp << 48);
	(parser->idx)++;

	tmp = parser->data.data[parser->idx];
	*val += (tmp << 56);
	(parser->idx)++;

	return true;
}

static bool pull_double(struct fx_parser_context *parser, double *val)
{
	return pull_int64_t(parser, (int64_t *)val);
}

static bool pull_guid(struct fx_parser_context *parser, struct GUID *guid)
{
	int i;

	if ((parser->idx) + 16 > parser->data.length) {
		GUID_all_zero(guid);
		return false;
	}
	if (!pull_uint32_t(parser, &(guid->time_low)))
		return false;
	if (!pull_uint16_t(parser, &(guid->time_mid)))
		return false;
	if (!pull_uint16_t(parser, &(guid->time_hi_and_version)))
		return false;
	if (!pull_uint8_t(parser, &(guid->clock_seq[0])))
		return false;
	if (!pull_uint8_t(parser, &(guid->clock_seq[1])))
		return false;
	for (i = 0; i < 6; ++i) {
		if (!pull_uint8_t(parser, &(guid->node[i])))
			return false;
	}
	return true;
}

static bool pull_systime(struct fx_parser_context *parser, struct FILETIME *ft)
{
	struct FILETIME filetime = {0,0};

	if (parser->idx + 8 > parser->data.length ||
	    !pull_uint32_t(parser, &(filetime.dwLowDateTime)) ||
	    !pull_uint32_t(parser, &(filetime.dwHighDateTime)))
		return false;

	*ft = filetime;

	return true;
}

static bool pull_clsid(struct fx_parser_context *parser, struct FlatUID_r **pclsid)
{
	struct FlatUID_r *clsid;
	int i = 0;

	if (parser->idx + 16 > parser->data.length)
		return false;

	clsid = talloc_zero(parser->mem_ctx, struct FlatUID_r);
	for (i = 0; i < 16; ++i) {
		if (!pull_uint8_t(parser, &(clsid->ab[i])))
			return false;
	}

	*pclsid = clsid;

	return true;
}

static bool pull_string8(struct fx_parser_context *parser, char **pstr)
{
	char *str;
	uint32_t i, length;

	if (!pull_uint32_t(parser, &length) ||
	    parser->idx + length > parser->data.length)
		return false;

	str = talloc_array(parser->mem_ctx, char, length + 1);
	for (i = 0; i < length; i++) {
		if (!pull_uint8_t(parser, (uint8_t*)&(str[i]))) {
			return false;
		}
	}
	str[length] = '\0';

	*pstr = str;

	return true;
}

static bool fetch_ucs2_data(struct fx_parser_context *parser, uint32_t numbytes, smb_ucs2_t **data_read)
{
	if ((parser->idx) + numbytes > parser->data.length) {
		// printf("insufficient data in fetch_ucs2_data (%i requested, %zi available)\n", numbytes, (parser->data.length - parser->idx));
		return false;
	}

	*data_read = talloc_zero_array(parser->mem_ctx, smb_ucs2_t, (numbytes/2) + 1);
	memcpy(*data_read, &(parser->data.data[parser->idx]), numbytes);
	parser->idx += numbytes;
	return true;
}

static bool fetch_ucs2_nullterminated(struct fx_parser_context *parser, smb_ucs2_t **data_read)
{
	uint32_t idx_local = parser->idx;
	bool found = false;
	while (idx_local < parser->data.length -1) {
		smb_ucs2_t val = 0x0000;
		val += parser->data.data[idx_local];
		idx_local++;
		val += parser->data.data[idx_local] << 8;
		idx_local++;
		if (val == 0x0000) {
			found = true;
			break;
		}
	}
	if (!found)
		return false;
	return fetch_ucs2_data(parser, idx_local-(parser->idx), data_read); 
}

static bool pull_unicode(struct fx_parser_context *parser, char **pstr)
{
	smb_ucs2_t *ucs2_data = NULL;
	char *utf8_data = NULL;
	size_t utf8_len;
	uint32_t length;

	if (!pull_uint32_t(parser, &length) ||
	    parser->idx + length > parser->data.length)
		return false;

	ucs2_data = talloc_zero_array(parser->mem_ctx, smb_ucs2_t, (length/2) + 1);

	if (!fetch_ucs2_data(parser, length, &ucs2_data)) {
		return false;
	}
	pull_ucs2_talloc(parser->mem_ctx, &utf8_data, ucs2_data, &utf8_len);

	*pstr = utf8_data;

	return true;
}

static bool pull_binary(struct fx_parser_context *parser, struct Binary_r *bin)
{
	if (!pull_uint32_t(parser, &(bin->cb)) ||
	    parser->idx + bin->cb > parser->data.length)
		return false;

	bin->lpb = talloc_array(parser->mem_ctx, uint8_t, bin->cb + 1);

	return pull_uint8_data(parser, bin->cb, &(bin->lpb));
}

/*
 pull a property value from the blob, starting at position idx
*/
static bool fetch_property_value(struct fx_parser_context *parser, DATA_BLOB *buf, struct SPropValue *prop)
{
	switch(prop->ulPropTag & 0xFFFF) {
	case PT_NULL:
	{
		if (!pull_uint32_t(parser, &(prop->value.null)))
			return false;
		break; 
	}
	case PT_SHORT:
	{
		if (!pull_uint16_t(parser, &(prop->value.i)))
			return false;
		break;
	}
	case PT_LONG:
	{
		if (!pull_uint32_t(parser, &(prop->value.l)))
			return false;
		break;
	}
	case PT_DOUBLE:
	{
		if (!pull_double(parser, (double *)&(prop->value.dbl)))
			return false;
		break;
	}
	case PT_BOOLEAN:
	{
		if (parser->idx + 2 > parser->data.length ||
		    !pull_uint8_t(parser, &(prop->value.b)))
			return false;

		/* special case for fast transfer, 2 bytes instead of one */
		(parser->idx)++;
		break;
	}
	case PT_I8:
	{
		int64_t val;
		if (!pull_int64_t(parser, &(val)))
			return false;
		prop->value.d = val;
		break;
	}
	case PT_STRING8:
	{
		char *str = NULL;
		if (!pull_string8(parser, &str))
			return false;
		prop->value.lpszA = (uint8_t *) str;
		break;
	}
	case PT_UNICODE:
	{
		char *str = NULL;
		if (!pull_unicode (parser, &str))
			return false;
		prop->value.lpszW = str;
		break;
	}
	case PT_SYSTIME:
	{
		if (!pull_systime(parser, &prop->value.ft))
			return false;
		break;
	}
	case PT_CLSID:
	{
		if (!pull_clsid(parser, &prop->value.lpguid))
			return false;
		break;
	}
	case PT_SVREID:
	case PT_BINARY:
	{
		if (!pull_binary(parser, &prop->value.bin))
			return false;
		break;
	}
	case PT_OBJECT:
	{
		/* the object itself is sent too, thus download it as a binary,
		   not as a meaningless number, which is length of the object here */
		if (!pull_binary(parser, &prop->value.bin))
			return false;
		break;
	}
	case PT_ERROR:
	{
		uint32_t num;
		if (!pull_uint32_t(parser, &num))
			return false;
		prop->value.err = num;
		break;
	}
	case PT_MV_BINARY:
	{
		uint32_t i;
		if (!pull_uint32_t(parser, &(prop->value.MVbin.cValues)) ||
		    parser->idx + prop->value.MVbin.cValues * 4 > parser->data.length)
			return false;
		prop->value.MVbin.lpbin = talloc_array(parser->mem_ctx, struct Binary_r, prop->value.MVbin.cValues);
		for (i = 0; i < prop->value.MVbin.cValues; i++) {
			if (!pull_binary(parser, &(prop->value.MVbin.lpbin[i])))
				return false;
		}
		break;
	}
	case PT_MV_SHORT:
	{
		uint32_t i;
		if (!pull_uint32_t(parser, &(prop->value.MVi.cValues)) ||
		    parser->idx + prop->value.MVi.cValues * 2 > parser->data.length)
			return false;
		prop->value.MVi.lpi = talloc_array(parser->mem_ctx, uint16_t, prop->value.MVi.cValues);
		for (i = 0; i < prop->value.MVi.cValues; i++) {
			if (!pull_uint16_t(parser, &(prop->value.MVi.lpi[i])))
				return false;
		}
		break;
	}
	case PT_MV_LONG:
	{
		uint32_t i;
		if (!pull_uint32_t(parser, &(prop->value.MVl.cValues)) ||
		    parser->idx + prop->value.MVl.cValues * 4 > parser->data.length)
			return false;
		prop->value.MVl.lpl = talloc_array(parser->mem_ctx, uint32_t, prop->value.MVl.cValues);
		for (i = 0; i < prop->value.MVl.cValues; i++) {
			if (!pull_uint32_t(parser, &(prop->value.MVl.lpl[i])))
				return false;
		}
		break;
	}
	case PT_MV_STRING8:
	{
		uint32_t i;
		char *str;
		if (!pull_uint32_t(parser, &(prop->value.MVszA.cValues)) ||
		    parser->idx + prop->value.MVszA.cValues * 4 > parser->data.length)
			return false;
		prop->value.MVszA.lppszA = (uint8_t **) talloc_array(parser->mem_ctx, uint8_t*, prop->value.MVszA.cValues);
		for (i = 0; i < prop->value.MVszA.cValues; i++) {
			str = NULL;
			if (!pull_string8(parser, &str))
				return false;
			prop->value.MVszA.lppszA[i] = (uint8_t *) str;
		}
		break;
	}
	case PT_MV_CLSID:
	{
		uint32_t i;
		if (!pull_uint32_t(parser, &(prop->value.MVguid.cValues)) ||
		    parser->idx + prop->value.MVguid.cValues * 16 > parser->data.length)
			return false;
		prop->value.MVguid.lpguid = talloc_array(parser->mem_ctx, struct FlatUID_r *, prop->value.MVguid.cValues);
		for (i = 0; i < prop->value.MVguid.cValues; i++) {
			if (!pull_clsid(parser, &(prop->value.MVguid.lpguid[i])))
				return false;
		}
		break;
	}
	case PT_MV_UNICODE:
	{
		uint32_t i;
		char *str;

		if (!pull_uint32_t(parser, &(prop->value.MVszW.cValues)) ||
		    parser->idx + prop->value.MVszW.cValues * 4 > parser->data.length)
			return false;
		prop->value.MVszW.lppszW = (const char **)  talloc_array(parser->mem_ctx, char *, prop->value.MVszW.cValues);
		for (i = 0; i < prop->value.MVszW.cValues; i++) {
			str = NULL;
			if (!pull_unicode(parser, &str))
				return false;
			prop->value.MVszW.lppszW[i] = str;
		}
		break;
	}
	case PT_MV_SYSTIME:
	{
		uint32_t i;
		if (!pull_uint32_t(parser, &(prop->value.MVft.cValues)) ||
		    parser->idx + prop->value.MVft.cValues * 8 > parser->data.length)
			return false;
		prop->value.MVft.lpft = talloc_array(parser->mem_ctx, struct FILETIME, prop->value.MVft.cValues);
		for (i = 0; i < prop->value.MVft.cValues; i++) {
			if (!pull_systime(parser, &(prop->value.MVft.lpft[i])))
				return false;
		}
		break;
	}
	default:
		printf("unhandled conversion case in fetch_property_value(): 0x%x\n", prop->ulPropTag);
		OPENCHANGE_ASSERT();
	}
	return true;
}

static bool pull_named_property(struct fx_parser_context *parser, enum MAPISTATUS *ms)
{
	uint8_t type = 0;
	if (!pull_guid(parser, &(parser->namedprop.lpguid)))
		return false;
	/* printf("guid       : %s\n", GUID_string(parser->mem_ctx, &(parser->namedprop.lpguid))); */
	if (!pull_uint8_t(parser, &type))
		return false;
	if (type == 0) {
		parser->namedprop.ulKind = MNID_ID;
		if (!pull_uint32_t(parser, &(parser->namedprop.kind.lid)))
			return false;
		/* printf("LID dispid: 0x%08x\n", parser->namedprop.kind.lid); */
	} else if (type == 1) {
		smb_ucs2_t *ucs2_data = NULL;
		size_t utf8_len;
		parser->namedprop.ulKind = MNID_STRING;
		if (!fetch_ucs2_nullterminated(parser, &ucs2_data))
			return false;
		pull_ucs2_talloc(parser->mem_ctx, (char**)&(parser->namedprop.kind.lpwstr.Name), ucs2_data, &(utf8_len));
		parser->namedprop.kind.lpwstr.NameSize = utf8_len;
		/* printf("named: %s\n", parser->namedprop.kind.lpwstr.Name); */
	} else {
		printf("unknown named property kind: 0x%02x\n", parser->namedprop.ulKind);
		OPENCHANGE_ASSERT();
	}
	if (parser->op_namedprop) {
		*ms = parser->op_namedprop(parser->lpProp.ulPropTag, parser->namedprop, parser->priv);
	}

	return true;
}

/**
  \details set a callback function for marker output
*/
_PUBLIC_ void fxparser_set_marker_callback(struct fx_parser_context *parser, fxparser_marker_callback_t marker_callback)
{
	parser->op_marker = marker_callback;
}

/**
  \details set a callback function for delete properties output
*/
_PUBLIC_ void fxparser_set_delprop_callback(struct fx_parser_context *parser, fxparser_delprop_callback_t delprop_callback)
{
	parser->op_delprop = delprop_callback;
}

/**
  \details set a callback function for named properties output
*/
_PUBLIC_ void fxparser_set_namedprop_callback(struct fx_parser_context *parser, fxparser_namedprop_callback_t namedprop_callback)
{
	parser->op_namedprop = namedprop_callback;
}

/**
  \details set a callback function for property output
*/
_PUBLIC_ void fxparser_set_property_callback(struct fx_parser_context *parser, fxparser_property_callback_t property_callback)
{
	parser->op_property = property_callback;
}

/**
  \details initialise a fast transfer parser
*/
_PUBLIC_ struct fx_parser_context* fxparser_init(TALLOC_CTX *mem_ctx, void *priv)
{
	struct fx_parser_context *parser = talloc_zero(mem_ctx, struct fx_parser_context);

	parser->mem_ctx = mem_ctx;
	parser->data = data_blob_talloc_named(parser->mem_ctx, NULL, 0, "fast transfer parser");
	parser->state = ParserState_Entry;
	parser->idx = 0;
	parser->lpProp.ulPropTag = (enum MAPITAGS) 0;
	parser->lpProp.dwAlignPad = 0;
	parser->lpProp.value.l = 0;
	parser->priv = priv;

	return parser;
}

/**
  \details parse a fast transfer buffer
*/
_PUBLIC_ enum MAPISTATUS fxparser_parse(struct fx_parser_context *parser, DATA_BLOB *fxbuf)
{
	enum MAPISTATUS ms = MAPI_E_SUCCESS;

	data_blob_append(parser->mem_ctx, &(parser->data), fxbuf->data, fxbuf->length);
	parser->enough_data = true;
	while(ms == MAPI_E_SUCCESS && (parser->idx < parser->data.length) && parser->enough_data) {
		uint32_t idx = parser->idx;

		switch(parser->state) {
			case ParserState_Entry:
			{
				if (pull_tag(parser)) {
					/* printf("tag: 0x%08x\n", parser->tag); */
					parser->state = ParserState_HaveTag;
				} else {
					parser->enough_data = false;
					parser->idx = idx;
				}
				break;
			}
			case ParserState_HaveTag:
			{
				switch (parser->tag) {
					case StartTopFld:
					case StartSubFld:
					case EndFolder:
					case StartMessage:
					case StartFAIMsg:
					case EndMessage:
					case StartRecip:
					case EndToRecip:
					case NewAttach:
					case EndAttach:
					case StartEmbed:
					case EndEmbed:
						if (parser->op_marker) {
							ms = parser->op_marker(parser->tag, parser->priv);
						}
						parser->state = ParserState_Entry;
						break;
					case MetaTagFXDelProp:
					{
						uint32_t tag;
						if (pull_uint32_t(parser, &tag)) {
							if (parser->op_delprop) {
								ms = parser->op_delprop(tag, parser->priv);
							}
							parser->state = ParserState_Entry;
						} else {
							parser->enough_data = false;
							parser->idx = idx;
						}
						break;
					}
					default:
					{
						/* standard property thing */
						parser->lpProp.ulPropTag = (enum MAPITAGS) parser->tag;
						parser->lpProp.dwAlignPad = 0;
						if ((parser->lpProp.ulPropTag >> 16) & 0x8000) {
							/* this is a named property */
							// printf("tag: 0x%08x\n", parser->tag);
							// TODO: this should probably be a separate parser state
							// TODO: this needs to return the named property
							if (pull_named_property(parser, &ms)) {
								parser->state = ParserState_HavePropTag;
							} else {
								parser->enough_data = false;
								parser->idx = idx;
							}
						} else {
							parser->state = ParserState_HavePropTag;
						}
					}
				}
				break;
			}
			case ParserState_HavePropTag:
			{
				if (fetch_property_value(parser, &(parser->data), &(parser->lpProp))) {
					// printf("position %i of %zi\n", parser->idx, parser->data.length);
					if (parser->op_property) {
						ms = parser->op_property(parser->lpProp, parser->priv);
					}
					parser->state = ParserState_Entry;
				} else {
					parser->enough_data = false;
					parser->idx = idx;
				}
				break;
			}
		}
	}
	{
		// Remove the part of the buffer that we've used
		uint32_t remainder_len = parser->data.length - parser->idx;
		DATA_BLOB remainder = data_blob_talloc_named(parser->mem_ctx, &(parser->data.data[parser->idx]), remainder_len, "fast transfer parser");
		data_blob_free(&(parser->data));
		parser->data = remainder;
		parser->idx = 0;
	}

	return ms;
}
