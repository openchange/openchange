/*
  OpenChange implementation.

  libndr mapi support

  Copyright (C) Julien Kerihuel 2015

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
#include <ndr.h>
#include "gen_ndr/ndr_exchange.h"
#include "gen_ndr/ndr_property.h"
#include "gen_ndr/ndr_exchange_debug.h"

#if (!defined(NDR_COLOR))
#define NDR_COLOR
#define COLOR_BLACK
#define COLOR_RED
#define COLOR_GREEN
#define COLOR_YELLOW
#define COLOR_BLUE
#define COLOR_MAGENTA
#define COLOR_CYAN
#define COLOR_WHITE
#define COLOR_BOLD
#define COLOR_INVERSE
#define COLOR_BOLD_OFF
#define COLOR_END
#else
#define COLOR_BLACK    "\x1b[30m"
#define COLOR_RED      "\x1b[31m"
#define COLOR_GREEN    "\x1b[32m"
#define COLOR_YELLOW   "\x1b[33m"
#define COLOR_BLUE     "\x1b[34m"
#define COLOR_MAGENTA  "\x1b[35m"
#define COLOR_CYAN     "\x1b[36m"
#define COLOR_WHITE    "\x1b[37m"
#define COLOR_BOLD     "\x1b[1m"
#define COLOR_INVERSE  "\x1b[7m"
#define COLOR_BOLD_OFF "\x1b[22m"
#define COLOR_END      "\x1b[0m"
#endif

#define NDR_RED(s)     COLOR_RED     #s COLOR_END
#define NDR_GREEN(s)   COLOR_GREEN   #s COLOR_END
#define NDR_YELLOW(s)  COLOR_YELLOW  #s COLOR_END
#define NDR_BLUE(s)    COLOR_BLUE    #s COLOR_END
#define NDR_MAGENTA(s) COLOR_MAGENTA #s COLOR_END
#define NDR_CYAN(s)    COLOR_CYAN    #s COLOR_END
#define NDR_WHITE(s)   COLOR_WHITE   #s COLOR_END

/**
 */
static int ndr_print_marker(struct ndr_print *ndr, uint32_t marker)
{
	const char *val = NULL;
	static bool noend = false;
	static uint32_t _saved_depth = 0;

	if (_saved_depth == 0) {
		_saved_depth = ndr->depth;
	}

	ndr_set_flags(&ndr->flags, LIBNDR_PRINT_ARRAY_HEX);

	/* Adjust ndr depth */
	switch (marker) {
	case EndFolder:
	case EndMessage:
	case EndEmbed:
	case EndToRecip:
	case EndAttach:
	case IncrSyncStateEnd:
		if (_saved_depth != ndr->depth) {
			ndr->depth--;
		}
		break;
	case IncrSyncChg:
	case IncrSyncChgPartial:
	case IncrSyncStateBegin:
		if (noend == true) ndr->depth--;
		break;
	}

	switch (marker) {
		/* Folders */
	case StartTopFld: val = NDR_YELLOW("StartTopFld"); break;
	case StartSubFld: val = NDR_YELLOW("StartSubFld"); break;
	case EndFolder: val = NDR_YELLOW("EndFolder"); break;
		/* Messages and their parts */
	case StartMessage: val = NDR_YELLOW("StartMessage"); break;
	case StartFAIMsg: val = NDR_YELLOW("StartFAIMsg"); break;
	case EndMessage: val = NDR_YELLOW("EndMessage"); break;
	case StartEmbed: val = NDR_YELLOW("StartEmbed"); break;
	case EndEmbed: val = NDR_YELLOW("EndEmbed"); break;
	case StartRecip: val = NDR_YELLOW("StartRecip"); break;
	case EndToRecip: val = NDR_YELLOW("EndToRecip"); break;
	case NewAttach: val = NDR_YELLOW("NewAttach"); break;
	case EndAttach: val = NDR_YELLOW("EndAttach"); break;
		/* Synchronization download */
	case IncrSyncChg: val = NDR_YELLOW("IncrSyncChg"); noend = true; break;
	case IncrSyncChgPartial: val = NDR_YELLOW("IncrSyncChgPartial"); noend = true; break;
	case IncrSyncDel: val = NDR_YELLOW("IncrSyncDel"); break;
	case IncrSyncEnd: val = NDR_YELLOW("IncrSyncEnd"); break;
	case IncrSyncRead: val = NDR_YELLOW("IncrSyncRead"); break;
	case IncrSyncStateBegin: val = NDR_YELLOW("IncrSyncStateBegin"); noend = true; break;
	case IncrSyncStateEnd: val = NDR_YELLOW("IncrSyncStateEnd"); break;
	case IncrSyncProgressMode: val = NDR_YELLOW("IncrSyncProgressMode"); break;
	case IncrSyncProgressPerMsg: val = NDR_YELLOW("IncrSyncProgressPerMsg"); break;
	case IncrSyncMessage: val = NDR_YELLOW("IncrSyncMessage"); break;
	case IncrSyncGroupInfo: val = NDR_YELLOW("IncrSyncGroupInfo"); break;
		/* Special */
	case FXErrorInfo: val = "FXErrorInfo"; break;
	}
	if (val == NULL) {
		return -1;
	}

	ndr_print_enum(ndr, NDR_YELLOW(Marker), "ENUM", val, marker);

	/* Adjust ndr depth */
	switch (marker) {
	case EndFolder:
	case EndMessage:
	case EndEmbed:
	case EndToRecip:
	case EndAttach:
	case IncrSyncStateEnd:
		if (_saved_depth != ndr->depth) {
			ndr->depth--;
		}
		break;
	default:
		ndr->depth++;
	}

	return 0;
}

static void ndr_print_IDSET(struct ndr_print *ndr, const struct idset *idset,
                            const char *label)
{
	struct globset_range *range;
	uint32_t i;
	char *guid_str;

	ndr->print(ndr, "%s:", label);
	ndr->depth++;
	while (idset) {
		if (idset->idbased) {
			ndr->print(ndr, COLOR_CYAN "%.4x (%d elements):" COLOR_END, idset->repl.id, idset->range_count);
		}
		else {
			guid_str = GUID_string(NULL, &idset->repl.guid);
			ndr->print(ndr, COLOR_CYAN "{%s} (%d elements):" COLOR_END, guid_str, idset->range_count);
			talloc_free(guid_str);
		}

		ndr->depth++;
		range = idset->ranges;
		for (i = 0; i < idset->range_count; i++) {
			if (exchange_globcnt(range->low) > exchange_globcnt(range->high)) {
				ndr->print(ndr, COLOR_BOLD COLOR_RED "Incorrect GLOBCNT range as high value is larger than low value" COLOR_END COLOR_END);
			}
			ndr->print(ndr, COLOR_CYAN "0x%.12" PRIx64 ":0x%.12" PRIx64 COLOR_END, range->low, range->high);
			range = range->next;
		}
		ndr->depth--;

		idset = idset->next;
	}
	ndr->depth--;

}

static int ndr_parse_ics_state(TALLOC_CTX *mem_ctx, struct ndr_print *ndr,
                               struct ndr_pull *ndr_pull, uint32_t element)
{
	struct idset *idset;
	struct SBinary PtypBinary;
	DATA_BLOB buffer;

	switch (element) {
	case MetaTagIdsetGiven:
		NDR_CHECK(ndr_pull_SBinary(ndr_pull, NDR_SCALARS, &PtypBinary));
		buffer.length = PtypBinary.cb;
		buffer.data = PtypBinary.lpb;
		idset = IDSET_parse(mem_ctx, buffer, false);
		ndr_print_IDSET(ndr, idset, COLOR_BOLD NDR_CYAN(MetaTagIdsetGiven) COLOR_BOLD_OFF);
		return 0;
	default:
		return -1;
	}
	return -1;
}

static int ndr_parse_property(TALLOC_CTX *mem_ctx, struct ndr_print *ndr,
			      struct ndr_pull *ndr_pull, uint32_t element)
{
	struct XID 	xid;
	struct SizedXid	SizedXid;
	struct ndr_pull	*_ndr_buffer;
	uint32_t	len;
	char		*propValue;

	switch (element) {
	case PidTagChangeKey:
	case PidTagSourceKey:
		propValue = talloc_asprintf(mem_ctx, COLOR_BOLD NDR_MAGENTA(%s) COLOR_BOLD_OFF, get_proptag_name(element));
		NDR_CHECK(ndr_pull_uint32(ndr_pull, NDR_SCALARS, &len));
		NDR_CHECK((ndr_pull_subcontext_start(ndr_pull, &_ndr_buffer, 0, len)));
		NDR_CHECK(ndr_pull_XID(_ndr_buffer, NDR_SCALARS, &xid));
		ndr_print_XID(ndr, propValue, &xid);
		NDR_CHECK(ndr_pull_subcontext_end(ndr_pull, _ndr_buffer, 4, -1));
		return 0;
	case PidTagPredecessorChangeList:
		propValue = talloc_asprintf(mem_ctx, COLOR_BOLD NDR_MAGENTA(%s) COLOR_BOLD_OFF, get_proptag_name(element));
		NDR_CHECK(ndr_pull_uint32(ndr_pull, NDR_SCALARS, &len));
		NDR_CHECK((ndr_pull_subcontext_start(ndr_pull, &_ndr_buffer, 0, len)));
		ndr->print(ndr, "%s:", propValue);
		ndr->depth++;
		while (_ndr_buffer->offset < _ndr_buffer->data_size) {
			NDR_CHECK(ndr_pull_SizedXid(_ndr_buffer, NDR_SCALARS, &SizedXid));
			ndr_print_XID(ndr, NULL, &SizedXid.XID);
		}
		ndr->depth--;
		NDR_CHECK(ndr_pull_subcontext_end(ndr_pull, _ndr_buffer, 4, -1));
		return 0;
	default:
		return -1;
	}
	return -1;
}

static int ndr_parse_propValue(TALLOC_CTX *mem_ctx, struct ndr_print *ndr,
                               struct ndr_pull *ndr_pull, uint32_t element)
{
	uint16_t        PtypInteger16;
	uint32_t        PtypInteger32;
	double          PtypFloating64;
	uint16_t         PtypBoolean;
	int64_t         PtypInteger64;
	struct FILETIME PtypTime;
	struct GUID     PtypGuid;
	const char      *PtypString;
	const char      *PtypString8;
	struct Binary_r PtypServerId;
	struct SBinary PtypBinary;
	const char      *_propValue;
	char            *propValue;
	DATA_BLOB				datablob;

	_propValue = get_proptag_name(element);
	if (_propValue == NULL) {
		propValue = talloc_asprintf(mem_ctx, COLOR_BOLD NDR_MAGENTA(0x%X) COLOR_BOLD_OFF, element);
	} else {
		propValue = talloc_asprintf(mem_ctx, COLOR_BOLD NDR_MAGENTA(%s) COLOR_BOLD_OFF, _propValue);
	}

	/* named property with propid > 0x8000 or known property */
	if ((element >> 16) & 0x8000) {
		//ret = ndr_parse_namedproperty(ndr_print, ndr_pull);
		ndr_pull->offset += 1;
		talloc_free(propValue);
		return 0;
	}

	switch (element & 0xFFFF) {
		/* fixedSizeValue */
	case PT_SHORT:
		NDR_CHECK(ndr_pull_uint16(ndr_pull, NDR_SCALARS, &PtypInteger16));
		ndr_print_uint16(ndr, propValue, PtypInteger16);
		break;
	case PT_LONG:
		NDR_CHECK(ndr_pull_uint32(ndr_pull, NDR_SCALARS, &PtypInteger32));
		ndr_print_uint32(ndr, propValue, PtypInteger32);
		break;
		/* no support for PtypFloating32 0x0004 PT_FLOAT */
	case PT_DOUBLE:
		NDR_CHECK(ndr_pull_double(ndr_pull, NDR_SCALARS, &PtypFloating64));
		ndr_print_double(ndr, propValue, PtypFloating64);
		break;
		/* no support for PtypCurrency, 0x0006 PT_CURRENCY */
		/* no support for PtypFloatingTime, 0x0007 PT_APPTIME */
	case PT_BOOLEAN:
		NDR_CHECK(ndr_pull_uint16(ndr_pull, NDR_SCALARS, &PtypBoolean));
		ndr_print_string(ndr, propValue, (const char *)((PtypBoolean == true) ? "True" : "False"));
		//ndr_print_uint16(ndr, propValue, PtypBoolean);
		break;
	case PT_I8:
		NDR_CHECK(ndr_pull_dlong(ndr_pull, NDR_SCALARS, &PtypInteger64));
		ndr_print_dlong(ndr, propValue, PtypInteger64);
		break;
	case PT_SYSTIME:
		NDR_CHECK(ndr_pull_FILETIME(ndr_pull, NDR_SCALARS, &PtypTime));
		ndr_print_FILETIME(ndr, propValue, &PtypTime);
		break;
	case PT_CLSID:
		NDR_CHECK(ndr_pull_GUID(ndr_pull, NDR_SCALARS, &PtypGuid));
		ndr_print_GUID(ndr, propValue, &PtypGuid);
		break;
		/* varSizeValue */
	case PT_UNICODE:
	{
		uint32_t  _flags_save_string = ndr_pull->flags;
		NDR_CHECK(ndr_pull_uint32(ndr_pull, NDR_SCALARS, &PtypInteger32));
		ndr_set_flags(&ndr_pull->flags, LIBNDR_FLAG_STR_NULLTERM);
		NDR_CHECK(ndr_pull_string(ndr_pull, NDR_SCALARS, &PtypString));
		ndr_pull->flags = _flags_save_string;
		ndr_print_string(ndr, propValue, PtypString);
	}
	break;
	case PT_STRING8:
	{
		/* FIXME: probably need to pull uint32_t first */
		uint32_t _flags_save_string = ndr_pull->flags;
		ndr_set_flags(&ndr_pull->flags, LIBNDR_FLAG_STR_RAW8|LIBNDR_FLAG_STR_NULLTERM|LIBNDR_FLAG_STR_SIZE4);
		NDR_CHECK(ndr_pull_string(ndr_pull, NDR_SCALARS, &PtypString8));
		ndr_pull->flags = _flags_save_string;
		ndr_print_string(ndr, propValue, PtypString8);
	}
	break;
	case PT_SVREID:
		NDR_CHECK(ndr_pull_Binary_r(ndr_pull, NDR_SCALARS, &PtypServerId));
		ndr_print_Binary_r(ndr, propValue, &PtypServerId);
		break;
	case PT_BINARY:
		NDR_CHECK(ndr_pull_SBinary(ndr_pull, NDR_SCALARS, &PtypBinary));
		ndr_print_SBinary(ndr, propValue, &PtypBinary);
		break;
	default:
	{
		uint32_t _flags_save_default = ndr_pull->flags;

		ndr->print(ndr, COLOR_RED "Not supported: %s" COLOR_END, propValue);
		ndr_set_flags(&ndr_pull->flags, LIBNDR_FLAG_REMAINING);
		NDR_CHECK(ndr_pull_DATA_BLOB(ndr_pull, NDR_SCALARS, &datablob));
		ndr_pull->flags = _flags_save_default;
		ndr_dump_data(ndr, datablob.data, datablob.length);
	}
	}
	talloc_free(propValue);
	return 0;
}

/**
   \details Parse and Display Fast Transfer element. An element is either
   a marker or a propvalue.

   \param mem_ctx pointer to the memory context
   \param ndr pointer to the ndr_print structure
   \param ndr_pull pointer to the ndr_pull structure representing the fxbuffer

   \return value >= 0 on success, otherwise -1
*/
static int ndr_parse_FastTransferElement(TALLOC_CTX *mem_ctx, struct ndr_print *ndr, struct ndr_pull *ndr_pull)
{
	uint32_t  element;
	int       ret;

	NDR_CHECK(ndr_pull_uint32(ndr_pull, NDR_SCALARS, &element));

	ret = ndr_print_marker(ndr, element);
	if (ret == 0) return 0;

	ret = ndr_parse_ics_state(mem_ctx, ndr, ndr_pull, element);
	if (ret == 0) return 0;

	ret = ndr_parse_property(mem_ctx, ndr, ndr_pull, element);
	if (ret == 0) return 0;

	ret = ndr_parse_propValue(mem_ctx, ndr, ndr_pull, element);
	return ret;
}

/**
   \details Parse and display Fast Transfer Stream

   \param ndr pointer to the ndr_print structure
   \param buf the FastTransfer Stream buffer to parse

   \return 0 on success, otherwise -1
*/
static int ndr_parse_FastTransferStream(struct ndr_print *ndr, DATA_BLOB buf)
{
	TALLOC_CTX      *mem_ctx;
	struct ndr_pull *ndr_pull;
	int             ret = 0;
	int             offset;

	mem_ctx = talloc_named(NULL, 0, "FastTransferStream");
	if (!mem_ctx) return -1;
	if (buf.length == 0) return 0;

	/* Map DATA_BLOB to ndr pull context */
	ndr_pull = talloc_zero(mem_ctx, struct ndr_pull);
	NDR_ERR_HAVE_NO_MEMORY(ndr_pull);
	ndr_pull->flags |= LIBNDR_FLAG_NOALIGN;
	ndr_pull->current_mem_ctx = mem_ctx;
	ndr_pull->data = buf.data;
	ndr_pull->data_size = buf.length;
	ndr_pull->offset = 0;

	ndr_set_flags(&ndr_pull->flags, LIBNDR_FLAG_NOALIGN);
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);

	while (ndr_pull->offset < ndr_pull->data_size -1) {
		offset = ndr_parse_FastTransferElement(mem_ctx, ndr, ndr_pull);
		if (offset < 0) {
			ret = -1;
			goto end;
		}
	}

end:
	talloc_free(mem_ctx);
	return ret;
}

_PUBLIC_ void ndr_print_FastTransferSourceGetBuffer_repl(struct ndr_print *ndr, const char *name, const struct FastTransferSourceGetBuffer_repl *r)
{
	int res;

	ndr_print_struct(ndr, name, "FastTransferSourceGetBuffer_repl");
	if (r == NULL) { ndr_print_null(ndr); return; }
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		ndr->depth++;
		ndr_print_TransferStatus(ndr, "TransferStatus", r->TransferStatus);
		ndr_print_uint16(ndr, "InProgressCount", r->InProgressCount);
		ndr_print_uint16(ndr, "TotalStepCount", r->TotalStepCount);
		ndr_print_uint8(ndr, "Reserved", r->Reserved);
		ndr_print_uint16(ndr, "TransferBufferSize", r->TransferBufferSize);
		ndr_print_DATA_BLOB(ndr, "TransferBuffer", r->TransferBuffer);
		res = ndr_parse_FastTransferStream(ndr, r->TransferBuffer);
		if (res != 0) {
			ndr_print_DATA_BLOB(ndr, "TransferBuffer", r->TransferBuffer);
		}
		ndr->depth--;
		ndr->flags = _flags_save_STRUCT;
	}
}


/* Debug interface */
_PUBLIC_ void ndr_print_FastTransferSourceGetBuffer(struct ndr_print *ndr, const char *name, int flags, const struct FastTransferSourceGetBuffer *r)
{
	ndr_print_struct(ndr, name, "FastTransferSourceGetBuffer");
	if (r == NULL) { ndr_print_null(ndr); return; }
	ndr->depth++;
	if (flags & NDR_SET_VALUES) {
		ndr->flags |= LIBNDR_PRINT_SET_VALUES;
	}
	if (flags & NDR_IN) {
		ndr_print_struct(ndr, "in", "FastTransferSourceGetBuffer");
		ndr->depth++;
		ndr_print_DATA_BLOB(ndr, "data", r->in.data);
		ndr_parse_FastTransferStream(ndr, r->in.data);
		ndr->depth--;
	}
	if (flags & NDR_OUT) {
		ndr_print_struct(ndr, "out", "FastTransferSourceGetBuffer");
		ndr->depth++;
		ndr_print_DATA_BLOB(ndr, "data", r->out.data);
		ndr_parse_FastTransferStream(ndr, r->out.data);
		ndr->depth--;
	}
	ndr->depth--;
}
