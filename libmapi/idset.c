/*
   OpenChange MAPI implementation.

   Copyright (C) Wolfgang Sourdeau 2011

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

/**
   \file idset.c

   \brief Parsing of GLOBSET structures
 */

#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"

/* TODO: we only handle one GUID per idset */

struct uint32_array {
	uint32_t	*data;
	size_t		length;
};

struct GLOBSET_parser {
	DATA_BLOB			buffer;
	size_t				buffer_position;
	DATA_BLOB			stack[6];
	uint8_t				next_stack_item;
	uint8_t				total_stack_size;
	bool				error;
	uint32_t			range_count;
	struct globset_range	*ranges;
};

static inline void GLOBSET_parser_do_push(struct GLOBSET_parser *parser, uint8_t count);
static inline void GLOBSET_parser_do_bitmask(struct GLOBSET_parser *parser);
static void GLOBSET_parser_do_pop(struct GLOBSET_parser *parser);
static void GLOBSET_parser_do_range(struct GLOBSET_parser *parser);

static inline void GLOBSET_parser_do_push(struct GLOBSET_parser *parser, uint8_t count)
{
	DATA_BLOB *push_buffer;

	if (count == 0 || count > 6 || parser->next_stack_item > 5 || parser->total_stack_size > 6) {
		abort();
	}

	push_buffer = parser->stack + parser->next_stack_item;
	push_buffer->data = parser->buffer.data + parser->buffer_position;
	push_buffer->length = count;
	parser->next_stack_item += 1;
	parser->buffer_position += count;
	parser->total_stack_size += count;
	if (parser->total_stack_size == 6) {
		GLOBSET_parser_do_range(parser);
		GLOBSET_parser_do_pop(parser);
	}
}

static void GLOBSET_parser_do_pop(struct GLOBSET_parser *parser)
{
	DATA_BLOB *push_buffer;

	parser->next_stack_item -= 1;

	push_buffer = parser->stack + parser->next_stack_item;
	parser->total_stack_size -= push_buffer->length;
}

static DATA_BLOB *GLOBSET_parser_stack_combine(TALLOC_CTX *mem_ctx, struct GLOBSET_parser *parser, DATA_BLOB *additional)
{
	DATA_BLOB *combined, *current;
	uint8_t i;
	uint8_t *current_ptr;

	combined = talloc_zero(mem_ctx, DATA_BLOB);
	if (additional != NULL) {
		combined->length = additional->length;
	}

	for (i = 0; i < parser->next_stack_item; i++) {
		current = parser->stack + i;
		combined->length += current->length;
	}
	combined->data = talloc_array(combined, uint8_t, combined->length);

	current_ptr = combined->data;
	for (i = 0; i < parser->next_stack_item; i++) {
		current = parser->stack + i;
		memcpy(current_ptr, current->data, current->length);
		current_ptr += current->length;
	}

	if (additional && additional->length > 0) {
		memcpy(current_ptr, additional->data, additional->length);
	}

	return combined;
}

static uint64_t GLOBSET_parser_range_value(DATA_BLOB *combined)
{
	uint64_t value = 0, base = 1;
	uint8_t i;

	for (i = 0; i < combined->length; i++) {
		value += combined->data[i] * base;
		base = base * 256;
	}

	return value;
}

static void GLOBSET_parser_do_range(struct GLOBSET_parser *parser)
{
	uint8_t count;
	struct globset_range *range;
	DATA_BLOB *combined, *additional;
	void *mem_ctx;

	mem_ctx = talloc_zero(NULL, void);

	range = talloc_zero(parser, struct globset_range);
	count = 6 - parser->total_stack_size;

	if (count > 0) {
		additional = talloc_zero(mem_ctx, DATA_BLOB);
		additional->length = count;
		additional->data = talloc_array(additional, uint8_t, count);
		memcpy(additional->data, parser->buffer.data + parser->buffer_position, count);
	}
	else {
		additional = NULL;
	}
	parser->buffer_position += count;
	combined = GLOBSET_parser_stack_combine(mem_ctx, parser, additional);
	range->low = GLOBSET_parser_range_value(combined);

	if (count == 0) {
		range->high = range->low;
	}
	else if (count > 0) {
		memcpy(additional->data, parser->buffer.data + parser->buffer_position, count);
		parser->buffer_position += count;
		combined = GLOBSET_parser_stack_combine(mem_ctx, parser, additional);
		range->high = GLOBSET_parser_range_value(combined);
	}

	DLIST_ADD_END(parser->ranges, range, void);
	/* DEBUG(5, ("  added range: [%.12Lx:%.12Lx] %p  %p %p\n", range->low, range->high, range, range->prev, range->next)); */
	parser->range_count++;

	if (!parser->ranges) {
		abort();
	}

	talloc_free(mem_ctx);
}

static inline void GLOBSET_parser_do_bitmask(struct GLOBSET_parser *parser)
{
	uint8_t startValue, mask, bit, i;
	DATA_BLOB *combined;
	uint64_t baseValue, lowValue, highValue;
	struct globset_range *range;
	bool blank = false;

	startValue = parser->buffer.data[parser->buffer_position];
	mask = parser->buffer.data[parser->buffer_position+1];
	parser->buffer_position += 2;

	combined = GLOBSET_parser_stack_combine(NULL, parser, NULL);
	baseValue = GLOBSET_parser_range_value(combined) << 8;
	talloc_free(combined);

        lowValue = startValue;
	highValue = startValue;
	for (i = 0; i < 8; i++) {
		bit = 1 << i;
		if (blank) {
			if ((mask & bit)) {
				blank = false;
				lowValue = startValue + 1 + i;
				highValue = lowValue;
			}
		}
		else {
			if ((mask & bit)) {
				highValue = startValue + 1 + i;
				if (i == 7) {
					range = talloc_zero(parser, struct globset_range);
					range->low = baseValue | lowValue;
					range->high = baseValue | highValue;
					DLIST_ADD_END(parser->ranges, range, void);
				}
			}
			else {
				range = talloc_zero(parser, struct globset_range);
				range->low = baseValue | lowValue;
				range->high = baseValue | highValue;
				DLIST_ADD_END(parser->ranges, range, void);
				parser->range_count++;
				blank = true;
			}
		}
	}
}

/**
  \details deserialize a GLOBSET following the format described in [OXCFXICS - 2.2.2.5]
*/
_PUBLIC_ struct globset_range *GLOBSET_parse(TALLOC_CTX *mem_ctx, DATA_BLOB buffer, uint32_t *countP)
{
	struct GLOBSET_parser *parser;
	struct globset_range *ranges, *range;
	bool end = false;
	uint8_t command;

	parser = talloc_zero(NULL, struct GLOBSET_parser);
	parser->buffer = buffer;

	while (!end && !parser->error) {
		if (parser->buffer_position >= parser->buffer.length) {
			DEBUG(4, ("%s: end of buffer reached unexpectedly at position %Ld\n", __FUNCTION__,
				  (unsigned long long) parser->buffer_position));
			parser->error = true;
		}
		else {
			command = parser->buffer.data[parser->buffer_position];
			parser->buffer_position++;
			switch (command) {
			case 0x00: /* end */
				end = true;
				break;
			case 0x01: /* push 1 */
			case 0x02: /* push 2 */
			case 0x03: /* push 3 */
			case 0x04: /* push 4 */
			case 0x05: /* push 5 */
			case 0x06: /* push 6 */
				GLOBSET_parser_do_push(parser, command);
				break;
			case 0x42: /* bitmask */
				GLOBSET_parser_do_bitmask(parser);
				break;
			case 0x50: /* pop */
				GLOBSET_parser_do_pop(parser);
				break;
			case 0x52: /* range */
				GLOBSET_parser_do_range(parser);
				break;
			default:
				parser->error = true;
				DEBUG(4, ("%s: invalid command in blockset: %.2x\n", __FUNCTION__, command));
				abort();
			}
		}
	}

	if (parser->error) {
		ranges = NULL;
		/* abort(); */
	}
	else {
		ranges = parser->ranges;
		if (countP) {
			*countP = parser->range_count;
		}
		if (ranges) {
			range = ranges;
			while (range) {
				(void) talloc_reference(mem_ctx, range);
				range = range->next;
			}
		}
	}
	talloc_free(parser);

	return ranges;
}

#if 0 /* IDSET debugging */
static void check_idset(struct idset *idset)
{
	uint32_t i = 0;
	struct globset_range *range, *last_range;

	if (GUID_all_zero(&idset->guid)) {
		DEBUG(5, ("idset: invalid guid\n"));
		abort();
	}

	if (idset->ranges) {
		range = idset->ranges;
		while (range) {
			/* DEBUG(5, ("range %d: [%.16Lx:%.16Lx] prev: %p; next: %p\n", i, range->low, range->high, range->prev, range->next)); */
			if (range->prev == NULL) {
				DEBUG(5, ("range %d has a NULL prev\n", i));
				abort();
			}
			i++;
			last_range = range;
			range = range->next;
		}

		if (idset->ranges->prev != last_range) {
			DEBUG(5, ("idset: last element of linked list is not the expected one\n"));
			abort();
		}
	}

	if (i != idset->range_count) {
		DEBUG(5, ("idset: elements count does not match the reported value (%d and %d)\n", i, idset->range_count));
		abort();
	}
}
#else /* IDSET debugging */
#define check_idset(x) {}
#endif

/**
  \details deserialize an IDSET following the format described in [OXCFXICS - 2.2.2.4]
*/
_PUBLIC_ struct idset *IDSET_parse(TALLOC_CTX *mem_ctx, DATA_BLOB buffer)
{
	struct idset		*idset;
        DATA_BLOB			guid_blob, globset;

	if (buffer.length < 17) return NULL;

	idset = talloc_zero(mem_ctx, struct idset);

	guid_blob.data = buffer.data;
	guid_blob.length = 16;
	GUID_from_data_blob(&guid_blob, &idset->guid);

	globset.length = buffer.length - 16;
	globset.data = (uint8_t *) buffer.data + 16;
	idset->ranges = GLOBSET_parse(idset, globset, &idset->range_count);

	IDSET_dump(idset, "freshly parsed");

	check_idset(idset);

	return idset;
}

static int IDSET_uint64_compar(const void *vap, const void *vbp)
{
	int retval;
	const uint64_t *ap, *bp;

	ap = vap;
	bp = vbp;

	if ((*ap) < (*bp)) {
		retval = -1;
	}
	else if ((*ap) == (*bp)) {
		retval = 0;
	}
	else {
		retval = 1;
	}

	return retval;
}

/**
  \details creates an idset structure based on an array of ids
*/
_PUBLIC_ struct idset *IDSET_make(TALLOC_CTX *mem_ctx, struct GUID *base_guid, const uint64_t *array, uint32_t length, bool can_skip)
{
	struct idset *idset;
	struct globset_range *current_globset;
	uint64_t last_consequent;
	uint64_t *work_array;
	uint32_t i;

	if (!array) {
		return NULL;
	}

	idset = talloc_zero(mem_ctx, struct idset);
	idset->guid = *base_guid;

	current_globset = talloc_zero(idset, struct globset_range);
	DLIST_ADD_END(idset->ranges, current_globset, end);
	idset->range_count = 1;

	if (length == 0) {
		return idset;
	}

	work_array = talloc_memdup(NULL, array, sizeof(uint64_t) * length);
	qsort(work_array, length, sizeof(uint64_t), IDSET_uint64_compar);

	if (length == 2) {
		DEBUG(5, ("work_array[0]: %.16Lx, %.16Lx\n", (unsigned long long) work_array[0], (unsigned long long) work_array[1]));
		if (work_array[0] != array[0]) {
			DEBUG(5, ("elements were reordered\n"));
		}
	}

	current_globset->low = work_array[0];

	if (can_skip || length < 3) {
		current_globset->high = work_array[length-1];
	}
	else {
		last_consequent = current_globset->low;
		for (i = 1; i < length; i++) {
			if ((work_array[i] != last_consequent) && (work_array[i] != (last_consequent + 1))) {
				current_globset->high = last_consequent;
				current_globset = talloc_zero(idset, struct globset_range);
				DLIST_ADD_END(idset->ranges, current_globset, void);
				idset->range_count++;
				current_globset->low = work_array[i];
			}
			last_consequent = work_array[i];
		}
		current_globset->high = last_consequent;
	}

	talloc_free(work_array);

	check_idset(idset);

	return idset;
}

/* start: [0|1|2|3|4|5] -- count --> */
static void GLOBSET_ndr_push_shifted_id(struct ndr_push *ndr, uint64_t range_id, uint8_t start, uint8_t count)
{
	uint8_t i, steps, byte;

	for (i = 0; i < count; i++) {
		steps = start + i;
		byte = (range_id >> (8 * steps)) & 0xff;
		ndr_push_uint8(ndr, NDR_SCALARS, byte);
	}
}

static void GLOBSET_ndr_push_globset_range(struct ndr_push *ndr, struct globset_range *range)
{
	uint8_t i;
	uint64_t mask;
	bool done;

	if (range->low == range->high) {
		ndr_push_uint8(ndr, NDR_SCALARS, 0x06); /* push 6 */
		GLOBSET_ndr_push_shifted_id(ndr, range->low, 0, 6);
	}
	else {
		i = 0;
		mask = 0xff;
		done = false;
		while (!done && i < 6) {
			if ((range->low & mask) == (range->high & mask)) {
				mask <<= 8;
				i++;
			}
			else {
				done = true;
			}
		}
		if (i > 0 && i < 6) {
			/* push i */
			ndr_push_uint8(ndr, NDR_SCALARS, i);
			GLOBSET_ndr_push_shifted_id(ndr, range->low, 0, i);
		}

		ndr_push_uint8(ndr, NDR_SCALARS, 0x52); /* range */
		GLOBSET_ndr_push_shifted_id(ndr, range->low, i, 6 - i);
		GLOBSET_ndr_push_shifted_id(ndr, range->high, i, 6 - i);

		if (i > 0 && i < 6) {
			/* pop */
			ndr_push_uint8(ndr, NDR_SCALARS, 0x50);
		}
	}
}

static int IDSET_range_compar(const void *vap, const void *vbp)
{
	int retval;
	const struct globset_range *ap, *bp;

	ap = *(const struct globset_range **) vap;
	bp = *(const struct globset_range **) vbp;

	if (ap->low == bp->low) {
		retval = 0;
	}
	else if (ap->low < bp->low) {
		retval = -1;
	}
	else {
		retval = 1;
	}

	return retval;
}

/**
  \details reorder the ranges of an idset structure in ascending order
*/
_PUBLIC_ void IDSET_reorder(struct idset *idset)
{
	struct globset_range *range;
	struct globset_range **ranges;
	uint32_t i;

	if (!idset || idset->range_count < 2) return;

	ranges = talloc_array(NULL, struct globset_range *, idset->range_count);
	for (range = idset->ranges, i = 0; i < idset->range_count; i++, range = range->next) {
		ranges[i] = range;
	}

	qsort(ranges, idset->range_count, sizeof(struct globset_range *), IDSET_range_compar);

	idset->ranges = ranges[0];
	ranges[0]->prev = ranges[idset->range_count-1];
	for (i = 0; i < idset->range_count - 1; i++) {
		ranges[i]->next = ranges[i+1];
		ranges[i+1]->prev = ranges[i];
	}
	ranges[idset->range_count-1]->next = NULL;

	check_idset(idset);

	talloc_free(ranges);
}

/**
  \details compact the ranges of an idset structure (note: the idset must be pre-ordered, otherwise information will be lost)
*/
_PUBLIC_ void IDSET_compact(struct idset *idset, bool can_skip)
{
	struct globset_range *range, *next_range;

	if (!idset || idset->range_count < 2) return;

	if (can_skip) {
		range = idset->ranges;
		next_range = range->prev;
		if (next_range->low < range->low) {
			range->low = next_range->low;
		}
		if (next_range->high > range->high) {
			range->high = next_range->high;
		}

		while ((range = range->next)) {
			talloc_free(range);
		}

		range = idset->ranges;
		range->next = NULL;
		range->prev = range;
		idset->range_count = 1;
	}
	else {
		range = idset->ranges;
		while (range) {
			next_range = range->next;
			while (next_range) {
				if (next_range->low >= range->low
				    && next_range->low <= range->high) {	/* A[  B[...  ]A */
					if (next_range->high > range->high) {		/* A[  B[  ]A  ]B -> A[  B[  ]AB */
						range->high = next_range->high;
					}
					range->next = next_range->next;
					if (range->next) {
						range->next->prev = range;
					}
					if (idset->ranges->prev == next_range) {
						idset->ranges->prev = range;
					}
					idset->range_count--;
					talloc_free(next_range);
					next_range = range->next;
				}
				else {
					next_range = NULL;
				}
			}
			range = range->next;
		}
	}

	check_idset(idset);
}

/**
  \details returns an exact but totally distinct copy of an idset structure
*/
_PUBLIC_ struct idset *IDSET_clone(TALLOC_CTX *mem_ctx, struct idset *source_idset)
{
	struct globset_range *range, *clone;
	struct idset *idset;

	if (!source_idset) return NULL;

	check_idset(source_idset);

	idset = talloc_zero(mem_ctx, struct idset);
	idset->guid = source_idset->guid;
	idset->range_count = source_idset->range_count;

	range = source_idset->ranges;
	while (range) {
		clone = talloc_zero(idset, struct globset_range);
		clone->low = range->low;
		clone->high = range->high;
		DLIST_ADD_END(idset->ranges, clone, void);
		range = range->next;
	}

	check_idset(idset);

	return idset;
}

/**
  \details merge two idsets structures into a third one, by merging intersecting ranges
*/
_PUBLIC_ struct idset *IDSET_merge_idsets(TALLOC_CTX *mem_ctx, struct idset *left, struct idset *right)
{
	struct idset		*merged_idset = NULL, *clone = NULL;
	struct globset_range	*range, *firstLeft, *lastLeft, *firstRight, *lastRight;

	if (!left && !right) return NULL;

	if (left) {
		merged_idset = IDSET_clone(mem_ctx, left);
		if (right && right->range_count) {
			clone = IDSET_clone(NULL, right);
			range = clone->ranges;
			while (range) {
				(void) talloc_reference(merged_idset, range);
				range = range->next;
			}

			firstLeft = merged_idset->ranges;
			lastLeft = firstLeft->prev;
			firstRight = clone->ranges;
			lastRight = firstRight->prev;

			lastLeft->next = firstRight;
			lastRight->prev = lastLeft;
			firstLeft->prev = lastRight;

			merged_idset->range_count += clone->range_count;
			talloc_free(clone);

			check_idset(merged_idset);
		}
	}
	else {
		merged_idset = IDSET_clone(mem_ctx, right);
	}

	return merged_idset;
}

/**
  \details serialize an idset structure in a struct SBinary_r
*/
_PUBLIC_ struct Binary_r *IDSET_serialize(TALLOC_CTX *mem_ctx, struct idset *idset)
{
	struct ndr_push	*ndr;
	struct globset_range *current_range;
	struct Binary_r *data;

	if (!mem_ctx || !idset) return NULL;

	check_idset(idset);

	ndr = ndr_push_init_ctx(NULL);
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	ndr->offset = 0;

	ndr_push_GUID(ndr, NDR_SCALARS, &idset->guid);

	current_range = idset->ranges;
	while (current_range) {
		GLOBSET_ndr_push_globset_range(ndr, current_range);
		current_range = current_range->next;
	}
	ndr_push_uint8(ndr, NDR_SCALARS, 0x00); /* end */

	data = talloc_zero(mem_ctx, struct Binary_r);
	data->cb = ndr->offset;
	data->lpb = ndr->data;
	(void) talloc_reference(data, data->lpb);
	talloc_free(ndr);

	return data;
}

/**
  \details tests the presence of a specific id in the ranges of an idset structure
*/
_PUBLIC_ bool IDSET_includes_id(struct idset *idset, struct GUID *replica_guid, uint64_t id)
{
	struct globset_range *range;

	if (!idset) {
		return false;
	}
	if (!replica_guid) {
		return false;
	}

	if (GUID_equal(&idset->guid, replica_guid)) {
		range = idset->ranges;
		while (range) {
			if (range->low <= id && range->high >= id) {
				return true;
			}
			range = range->next;
		}
	}

	return false;
}

/**
  \details dump an idset structure
*/
_PUBLIC_ void IDSET_dump(struct idset *idset, const char *label)
{
	struct globset_range *range;
	uint32_t i;

	if (!idset) return;

	DEBUG(0, ("[%s] Dump of idset: %d elements\n", label, idset->range_count));
	range = idset->ranges;
	for (i = 0; i < idset->range_count; i++) {
		if (range->low > range->high) {
			abort();
		}
		DEBUG(0, ("  [0x%." PRIx64 ":0x%." PRIx64 "]\n", range->low, range->high));
		range = range->next;
	}
}
