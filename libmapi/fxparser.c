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
		val = 0;
		return false;
	}
	*val = parser->data.data[parser->idx];
	(parser->idx)++;
	return true;
}

static bool pull_uint16_t(struct fx_parser_context *parser, uint16_t *val)
{
	if ((parser->idx) + 2 > parser->data.length) {
		val = 0;
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
		val = 0;
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
	bool result;
	result = pull_uint32_t(parser, &(parser->tag));
	if (!result) {
		printf("bad pull of tag\n");
	}
	return result;
}

static bool pull_uint8_data(struct fx_parser_context *parser, uint8_t **data_read)
{
	for (; parser->offset < parser->length; ++(parser->offset)) {
		/* printf("parser %i of %i\n", parser->offset, parser->length); */
		if (!pull_uint8_t(parser, (uint8_t*)&((*data_read)[parser->offset]))) {
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
	pull_uint32_t(parser, &(guid->time_low));
	pull_uint16_t(parser, &(guid->time_mid));
	pull_uint16_t(parser, &(guid->time_hi_and_version));
	pull_uint8_t(parser, &(guid->clock_seq[0]));
	pull_uint8_t(parser, &(guid->clock_seq[1]));
	for (i = 0; i < 6; ++i) {
		pull_uint8_t(parser, &(guid->node[i]));
	}
	return true;
}

static bool fetch_ucs2_data(struct fx_parser_context *parser, uint32_t numbytes, smb_ucs2_t **data_read)
{
	if ((parser->idx) + numbytes > parser->data.length) {
		// printf("insufficient data in fetch_ucs2_data (%i requested, %zi available)\n", numbytes, (parser->data.length - parser->idx));
		return false;
	}

	*data_read = talloc_array(parser->mem_ctx, smb_ucs2_t, numbytes/2);
	memcpy(*data_read, &(parser->data.data[parser->idx]), numbytes);
	parser->idx += numbytes;
	return true;
}

static bool fetch_ucs2_nullterminated(struct fx_parser_context *parser, smb_ucs2_t **data_read)
{
	uint32_t idx_local = parser->idx;
	while (idx_local < parser->data.length -1) {
		smb_ucs2_t val = 0x0000;
		val += parser->data.data[idx_local];
		idx_local++;
		val += parser->data.data[idx_local] << 8;
		idx_local++;
		if (val == 0x0000) {
			break;
		}
	}
	return fetch_ucs2_data(parser, idx_local-(parser->idx), data_read); 
}

/*
 pull a property value from the blob, starting at position idx
*/
static bool fetch_property_value(struct fx_parser_context *parser, DATA_BLOB *buf, struct SPropValue *prop, uint32_t *len)
{
	switch(prop->ulPropTag & 0xFFFF) {
	case PT_SHORT:
	{
		pull_uint16_t(parser, &(prop->value.i));
		break;
	}
	case PT_LONG:
	{
		pull_uint32_t(parser, &(prop->value.l));
		break;
	}
	case PT_DOUBLE:
	{
		pull_double(parser, (double *)&(prop->value.dbl));
		break;
	}
	case PT_BOOLEAN:
	{
		pull_uint8_t(parser, &(prop->value.b));
		/* special case for fast transfer, 2 bytes instead of one */
		(parser->idx)++;
		break;
	}
	case PT_I8:
	{
		int64_t val;
		pull_int64_t(parser, &(val));
		prop->value.d = val;
		break;
	}
	case PT_STRING8:
	{
		char *ptr = 0;
		if (parser->length == 0) {
			pull_uint32_t(parser, &(parser->length));
			parser->offset = 0;
			prop->value.lpszA = talloc_array(parser->mem_ctx, char, parser->length + 1);
		}
		for (; parser->offset < parser->length; ++(parser->offset)) {
			if (!pull_uint8_t(parser, (uint8_t*)&(prop->value.lpszA[parser->offset]))) {
				return false;
			}
		}
		ptr = (char*)prop->value.lpszA;
		ptr += parser->length;
		*ptr = '\0';
		break;
	}
	case PT_UNICODE:
	{
		/* TODO: rethink this to handle split buffers */
		smb_ucs2_t *ucs2_data = NULL;
		if (parser->length == 0) {
			pull_uint32_t(parser, &(parser->length));
			ucs2_data = talloc_array(parser->mem_ctx, smb_ucs2_t, parser->length/2);
			parser->offset = 0;
		}
		char *utf8_data = NULL;
		size_t utf8_len;
		if (!fetch_ucs2_data(parser, parser->length, &ucs2_data)) {
			return false;
		}
		pull_ucs2_talloc(parser->mem_ctx, &utf8_data, ucs2_data, &utf8_len);
		prop->value.lpszW = utf8_data;
		break;
	}
	case PT_SYSTIME:
	{
		struct FILETIME filetime = {0,0};
		pull_uint32_t(parser, &(filetime.dwLowDateTime));
		pull_uint32_t(parser, &(filetime.dwHighDateTime));
		prop->value.ft = filetime;
		break;
	}
	case PT_CLSID:
	{
		int i = 0;
		prop->value.lpguid = talloc_zero(parser->mem_ctx, struct FlatUID_r);
		for (i = 0; i < 16; ++i) {
		  	pull_uint8_t(parser, &(prop->value.lpguid->ab[i]));
		}
		break;
	}
	case PT_SVREID:
	case PT_BINARY:
	{
		if (parser->length == 0) {
			  pull_uint32_t(parser, &(prop->value.bin.cb));
			  parser->length = prop->value.bin.cb;
			  prop->value.bin.lpb = talloc_array(parser->mem_ctx, uint8_t, parser->length + 1);
			  parser->offset = 0;
		}
		if (!pull_uint8_data(parser, &(prop->value.bin.lpb))) {
			return false;
		}
		break;
	}
	case PT_OBJECT:
	{
		pull_uint32_t(parser, &(prop->value.object));
		break;
	}
	case PT_MV_BINARY:
	{
		/* TODO: handle partial count / length */
		uint32_t        i;
		pull_uint32_t(parser, &(prop->value.MVbin.cValues));
		prop->value.MVbin.lpbin = talloc_array(parser->mem_ctx, struct Binary_r, prop->value.MVbin.cValues);
		for (i = 0; i < prop->value.MVbin.cValues; i++) {
			pull_uint32_t(parser, &(prop->value.MVbin.lpbin[i].cb));
			parser->length = prop->value.MVbin.lpbin[i].cb;
			prop->value.MVbin.lpbin[i].lpb = talloc_array(parser->mem_ctx, uint8_t, parser->length + 1);
			parser->offset = 0;
			if (!pull_uint8_data(parser, &(prop->value.MVbin.lpbin[i].lpb))) {
				return false;
			}
		}
		break;
	}
	default:
		printf("unhandled conversion case in fetch_property_value(): 0x%x\n", (prop->ulPropTag & 0xFFFF));
		OPENCHANGE_ASSERT();
	}
	return true;
}

static void pull_named_property(struct fx_parser_context *parser)
{
	uint8_t type = 0;
	pull_guid(parser, &(parser->namedprop.lpguid));
	/* printf("guid       : %s\n", GUID_string(parser->mem_ctx, &(parser->namedprop.lpguid))); */
	pull_uint8_t(parser, &type);
	if (type == 0) {
		parser->namedprop.ulKind = MNID_ID;
		pull_uint32_t(parser, &(parser->namedprop.kind.lid));
		/* printf("LID dispid: 0x%08x\n", parser->namedprop.kind.lid); */
	} else if (type == 1) {
		smb_ucs2_t *ucs2_data = NULL;
		size_t utf8_len;
		parser->namedprop.ulKind = MNID_STRING;
		fetch_ucs2_nullterminated(parser, &ucs2_data);
		pull_ucs2_talloc(parser->mem_ctx, (char**)&(parser->namedprop.kind.lpwstr.Name), ucs2_data, &(utf8_len));
		parser->namedprop.kind.lpwstr.NameSize = utf8_len;
		/* printf("named: %s\n", parser->namedprop.kind.lpwstr.Name); */
	} else {
		printf("unknown named property kind: 0x%02x\n", parser->namedprop.ulKind);
		OPENCHANGE_ASSERT();
	}
	if (parser->op_namedprop) {
		parser->op_namedprop(parser->lpProp.ulPropTag, parser->namedprop, parser->priv);
	}
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
	parser->length = 0;
	parser->priv = priv;

	return parser;
}

/**
  \details parse a fast transfer buffer
*/
_PUBLIC_ void fxparser_parse(struct fx_parser_context *parser, DATA_BLOB *fxbuf)
{
	data_blob_append(parser->mem_ctx, &(parser->data), fxbuf->data, fxbuf->length);
	parser->enough_data = true;
	while((parser->idx < parser->data.length) && parser->enough_data) {
		switch(parser->state) {
			case ParserState_Entry:
			{
				pull_tag(parser);
				/* printf("tag: 0x%08x\n", parser->tag); */
				parser->state = ParserState_HaveTag;
				break;
			}
			case ParserState_HaveTag:
			{
				switch (parser->tag) {
					case PidTagStartTopFld:
					case PidTagStartSubFld:
					case PidTagEndFolder:
					case PidTagStartMessage:
					case PidTagStartFAIMsg:
					case PidTagEndMessage:
					case PidTagStartRecip:
					case PidTagEndToRecip:
					case PidTagNewAttach:
					case PidTagEndAttach:
					case PidTagStartEmbed:
					case PidTagEndEmbed:
						if (parser->op_marker) {
							parser->op_marker(parser->tag, parser->priv);
						}
						parser->state = ParserState_Entry;
						break;
					case PidTagFXDelProp:
					{
						uint32_t tag;
						if (pull_uint32_t(parser, &tag)) {
							if (parser->op_delprop) {
								parser->op_delprop(tag, parser->priv);
							}
							parser->state = ParserState_Entry;
						} else {
							parser->enough_data = false;
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
							pull_named_property(parser);
						}
						parser->state = ParserState_HavePropTag;
					}
				}
				break;
			}
			case ParserState_HavePropTag:
			{
				if (fetch_property_value(parser, &(parser->data), &(parser->lpProp), &(parser->length))) {
					// printf("position %i of %zi\n", parser->idx, parser->data.length);
					if (parser->op_property) {
						parser->op_property(parser->lpProp, parser->priv);
					}
					parser->state = ParserState_Entry;
					parser->length = 0;
				} else {
					parser->enough_data = false;
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
}
