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
	case StartTopFld: val = "\x1b[33mStartTopFld\x1b[0m"; break;
	case StartSubFld: val = "\x1b[33mStartSubFld\x1b[0m"; break;
	case EndFolder: val = "\x1b[33mEndFolder\x1b[0m"; break;
		/* Messages and their parts */
	case StartMessage: val = "\x1b[33mStartMessage\x1b[0m"; break;
	case StartFAIMsg: val = "\x1b[33mStartFAIMsg\x1b[0m"; break;
	case EndMessage: val = "\x1b[33mEndMessage\x1b[0m"; break;
	case StartEmbed: val = "\x1b[33mStartEmbed\x1b[0m"; break;
	case EndEmbed: val = "\x1b[33mEndEmbed\x1b[0m"; break;
	case StartRecip: val = "\x1b[33mStartRecip\x1b[0m"; break;
	case EndToRecip: val = "\x1b[33mEndToRecip\x1b[0m"; break;
	case NewAttach: val = "\x1b[33mNewAttach\x1b[0m"; break;
	case EndAttach: val = "\x1b[33mEndAttach\x1b[0m"; break;
		/* Synchronization download */
	case IncrSyncChg: val = "\x1b[33mIncrSyncChg\x1b[0m"; noend = true; break;
	case IncrSyncChgPartial: val = "\x1b[33mIncrSyncChgPartial\x1b[0m"; noend = true; break;
	case IncrSyncDel: val = "\x1b[33mIncrSyncDel\x1b[0m"; break;
	case IncrSyncEnd: val = "\x1b[33mIncrSyncEnd\x1b[0m"; break;
	case IncrSyncRead: val = "\x1b[33mIncrSyncRead\x1b[0m"; break;
	case IncrSyncStateBegin: val = "\x1b[33mIncrSyncStateBegin\x1b[0m"; noend = true; break;
	case IncrSyncStateEnd: val = "\x1b[33mIncrSyncStateEnd\x1b[0m"; break;
	case IncrSyncProgressMode: val = "\x1b[33mIncrSyncProgressMode\x1b[0m"; break;
	case IncrSyncProgressPerMsg: val = "\x1b[33mIncrSyncProgressPerMsg\x1b[0m"; break;
	case IncrSyncMessage: val = "\x1b[33mIncrSyncMessage\x1b[0m"; break;
	case IncrSyncGroupInfo: val = "\x1b[33mIncrSyncGroupInfo\x1b[0m"; break;
		/* Special */
	case FXErrorInfo: val = "FXErrorInfo"; break;
	}
	if (val == NULL) {
		return -1;
	}

	ndr_print_enum(ndr, "\x1b[33mMarker\x1b[0m", "ENUM", val, marker);

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
			ndr->print(ndr, "\x1b[32m%.4x: %d elements\x1b[0m", idset->repl.id, idset->range_count);
		}
		else {
			guid_str = GUID_string(NULL, &idset->repl.guid);
			ndr->print(ndr, "\x1b[32m%s: %d elements\x1b[0m", guid_str, idset->range_count);
			talloc_free(guid_str);
		}

		ndr->depth++;
		range = idset->ranges;
		for (i = 0; i < idset->range_count; i++) {
			if (exchange_globcnt(range->low) > exchange_globcnt(range->high)) {
				ndr->print(ndr, "\x1b[31mIncorrect GLOBCNT range as high value is larger than low value\x1b[0m");
			}
			ndr->print(ndr, "\x1b[32m[0x%.12" PRIx64 ":0x%.12" PRIx64 "]\x1b[0m", range->low, range->high);
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
		ndr_print_IDSET(ndr, idset, "\x1b[32mMetaTagIdsetGiven\x1b[0m");
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
		propValue = talloc_asprintf(mem_ctx, "0x%X", element);
	} else {
		//propValue = talloc_strdup(mem_ctx, _propValue);
		propValue = talloc_asprintf(mem_ctx, "\x1b[1m\x1b[35m%s\x1b[0m\x1b[0m", _propValue);
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

			ndr->print(ndr, "Not supported: %s", propValue);
			ndr_set_flags(&ndr_pull->flags, LIBNDR_FLAG_REMAINING);
			NDR_CHECK(ndr_pull_DATA_BLOB(ndr_pull, NDR_SCALARS, &datablob));
			ndr_pull->flags = _flags_save_default;
			ndr_print_DATA_BLOB(ndr, propValue, datablob);
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
