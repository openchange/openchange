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
#include "gen_ndr/ndr_exchange.h"

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

/**
  \details Inverts the bytes of a globcnt, such as for the ids returned by Exchange
*/
_PUBLIC_ uint64_t exchange_globcnt(uint64_t globcnt)
{
	return ((globcnt & 0x00000000000000ffLL)	<< 40
		| (globcnt & 0x000000000000ff00LL)	<< 24
		| (globcnt & 0x0000000000ff0000LL)	<< 8
		| (globcnt & 0x00000000ff000000LL)	>> 8
		| (globcnt & 0x000000ff00000000LL)	>> 24
		| (globcnt & 0x0000ff0000000000LL)	>> 40);
}

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
	/* OC_DEBUG(5, "  added range: [%.16"PRIx64":%.16"PRIx64"] %p  %p %p", range->low, range->high, range, range->prev, range->next); */
	parser->range_count++;

	if (!parser->ranges) {
		abort();
	}

	talloc_free(mem_ctx);
}

static inline void GLOBSET_parser_do_bitmask(struct GLOBSET_parser *parser)
{
	uint8_t mask, bit, i;
	DATA_BLOB *combined, additional;
	uint64_t baseValue, lowValue, highValue;
	struct globset_range *range;
	bool blank = false;

	mask = parser->buffer.data[parser->buffer_position+1];
	parser->buffer_position += 2;

	additional.length = 1;
	additional.data = parser->buffer.data + parser->buffer_position;

	combined = GLOBSET_parser_stack_combine(NULL, parser, &additional);
	baseValue = GLOBSET_parser_range_value(combined);
	talloc_free(combined);

	lowValue = baseValue;
	highValue = baseValue;

	for (i = 0; i < 8; i++) {
		bit = 1 << i;
		if (blank) {
			if ((mask & bit)) {
				blank = false;
				lowValue = baseValue + ((uint64_t) (bit + 1) << 40);
				highValue = lowValue;
			}
		}
		else {
			if ((mask & bit) == 0) {
				range = talloc_zero(parser, struct globset_range);
				range->low = lowValue;
				range->high = highValue;
				DLIST_ADD_END(parser->ranges, range, void);
				parser->range_count++;
				blank = true;
			}
			else {
				highValue = baseValue + ((uint64_t) (bit + 1) << 40);
			}
		}
	}

	if (!blank) {
		range = talloc_zero(parser, struct globset_range);
		range->low = lowValue;
		range->high = highValue;
		DLIST_ADD_END(parser->ranges, range, void);
		parser->range_count++;
	}
}

/**
  \details deserialize a GLOBSET following the format described in [OXCFXICS - 2.2.2.5]
*/
_PUBLIC_ struct globset_range *GLOBSET_parse(TALLOC_CTX *mem_ctx, DATA_BLOB buffer, uint32_t *countP, uint32_t *byte_countP)
{
	struct GLOBSET_parser *parser;
	struct globset_range *ranges, *range;
	bool end = false;
	uint8_t command;

	parser = talloc_zero(NULL, struct GLOBSET_parser);
	parser->buffer = buffer;

	while (!end && !parser->error) {
		if (parser->buffer_position >= parser->buffer.length) {
			OC_DEBUG(4, "end of buffer reached unexpectedly at position %Ld",
				  (unsigned long long) parser->buffer_position);
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
				OC_DEBUG(4, "invalid command in blockset: %.2x", command);
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
		if (byte_countP) {
			*byte_countP = parser->buffer_position;
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
static void check_idset(const struct idset *idset)
{
	uint32_t i;
	struct globset_range *range, *last_range;

	while (idset) {
		if (!idset->idbased && GUID_all_zero(&idset->repl.guid)) {
			OC_DEBUG(5, "idset: invalid guid");
			abort();
		}

		i = 0;
		if (idset->ranges) {
			range = idset->ranges;
			while (range) {
				/* OC_DEBUG(5, ("range %d: [%.16Lx:%.16Lx] prev: %p; next: %p", i, range->low, range->high, range->prev, range->next)); */
				if (range->prev == NULL) {
					OC_DEBUG(5, "range %d has a NULL prev", i);
					abort();
				}
				i++;
				last_range = range;
				range = range->next;
			}
			
			if (idset->ranges->prev != last_range) {
				OC_DEBUG(5, "idset: last element of linked list is not the expected one");
				abort();
			}
		}

		if (i != idset->range_count) {
			OC_DEBUG(5, "idset: elements count does not match the reported value (%d and %d)", i, idset->range_count);
			abort();
		}
		idset = idset->next;
	}
}
#else /* IDSET debugging */
#define check_idset(x) {}
#endif

/**
  \details deserialize an IDSET following the format described in [OXCFXICS - 2.2.2.4]
*/
_PUBLIC_ struct idset *IDSET_parse(TALLOC_CTX *mem_ctx, DATA_BLOB buffer, bool idbased)
{
	struct idset		*idset, *prev_idset = NULL;
        DATA_BLOB		guid_blob, globset;
	uint32_t		total_bytes, byte_count;

	if (buffer.length < 17) return NULL;

	total_bytes = 0;
	while (total_bytes < buffer.length) {
		idset = talloc_zero(mem_ctx, struct idset);
		if (prev_idset) {
			prev_idset->next = idset;
		}

		if (idbased) {
			idset->repl.id = (buffer.data[total_bytes] | (buffer.data[total_bytes+1] << 8));
			total_bytes += 2;
		}
		else {
			guid_blob.data = buffer.data;
			guid_blob.length = 16;
			GUID_from_data_blob(&guid_blob, &idset->repl.guid);
			total_bytes += 16;
		}

		globset.length = buffer.length - 16;
		globset.data = (uint8_t *) buffer.data + 16;
		idset->ranges = GLOBSET_parse(idset, globset, &idset->range_count, &byte_count);
		
		total_bytes += byte_count;

		check_idset(idset);

		prev_idset = idset;
	}

	IDSET_dump(idset, "freshly parsed");

	return idset;
}

static int IDSET_ID_compar(const void *vap, const void *vbp)
{
	const struct idset *ap, *bp;
	int rc;

	ap = *(const struct idset **) vap;
	bp = *(const struct idset **) vbp;

	if (ap->repl.id == bp ->repl.id) {
		rc = 0;
	}
	else if (ap->repl.id < bp ->repl.id) {
		rc = 1;
	}
	else {
		rc = -1;
	}

	return rc;
}

static int IDSET_GUID_compar(const void *vap, const void *vbp)
{
	const struct idset *ap, *bp;

	ap = *(const struct idset **) vap;
	bp = *(const struct idset **) vbp;
	
	return GUID_compare(&ap->repl.guid, &bp->repl.guid);
}

static int IDSET_globcnt_compar(const void *vap, const void *vbp)
{
	int retval;
	uint64_t a, b;

	a = exchange_globcnt(*(uint64_t *) vap);
	b = exchange_globcnt(*(uint64_t *) vbp);

	if (a < b) {
		retval = -1;
	}
	else if (a == b) {
		retval = 0;
	}
	else {
		retval = 1;
	}

	return retval;
}

static int IDSET_range_compar(const void *vap, const void *vbp)
{
	const struct globset_range *ap, *bp;

	ap = *(const struct globset_range **) vap;
	bp = *(const struct globset_range **) vbp;

	return IDSET_globcnt_compar(&ap->low, &bp->low);
}

static struct idset *IDSET_make(TALLOC_CTX *mem_ctx, bool idbased, uint16_t base_id, const struct GUID *base_guid, const uint64_t *array, uint32_t length, bool single)
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
	idset->idbased = idbased;
	if (idbased) {
		idset->repl.id = base_id;
	} else {
		idset->repl.guid = *base_guid;
	}
	idset->single = single;

	current_globset = talloc_zero(idset, struct globset_range);
	DLIST_ADD_END(idset->ranges, current_globset, end);
	idset->range_count = 1;

	if (length == 0) {
		return idset;
	}

	work_array = talloc_memdup(NULL, array, sizeof(uint64_t) * length);
	qsort(work_array, length, sizeof(uint64_t), IDSET_globcnt_compar);

	if (length == 2) {
		OC_DEBUG(5, "work_array[0]: %.16Lx, %.16Lx", (unsigned long long) work_array[0], (unsigned long long) work_array[1]);
		if (work_array[0] != array[0]) {
			OC_DEBUG(5, "elements were reordered");
		}
	}

	current_globset->low = work_array[0];

	if (single || length < 3) {
		current_globset->high = work_array[length-1];
	}
	else {
		last_consequent = exchange_globcnt(current_globset->low);
		for (i = 1; i < length; i++) {
			if ((exchange_globcnt(work_array[i]) != last_consequent) && (exchange_globcnt(work_array[i]) != (last_consequent + 1))) {
				current_globset->high = exchange_globcnt(last_consequent);
				current_globset = talloc_zero(idset, struct globset_range);
				DLIST_ADD_END(idset->ranges, current_globset, void);
				idset->range_count++;
				current_globset->low = work_array[i];
			}
			last_consequent = exchange_globcnt(work_array[i]);
		}
		current_globset->high = exchange_globcnt(last_consequent);
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

static int IDSET_count(const struct idset *idset)
{
	int	max = 0;

	while (idset) {
		max++;
		idset = idset->next;
	}

	return max;
}

static void IDSET_reorder_idset(struct idset **idsetP)
{
	int		i, max = 0;
	struct idset	**idsets;
	struct idset	*idset, *current_idset, *last_idset;

	if (!idsetP) return;
	idset = *idsetP;
	if (!idset || !idset->next) return;

	check_idset(idset);

	max = IDSET_count(idset);
	idsets = talloc_array(NULL, struct idset *, max);

	current_idset = idset;
	for (i = 0; i < max; i++) {
		idsets[i] = current_idset;
		current_idset = current_idset->next;
	}

	qsort(idsets, max, sizeof(struct idset *), idsets[0]->idbased ? IDSET_ID_compar : IDSET_GUID_compar);

	current_idset = idsets[0];
	for (i = 1; i < max; i++) {
		last_idset = current_idset;
		current_idset = idsets[i];
		last_idset->next = current_idset;
	}
	current_idset->next = NULL;

	*idsetP = idsets[0];

	talloc_free(idsets);
}

static void IDSET_reorder_ranges(struct idset *idset)
{
	struct globset_range *range;
	struct globset_range **ranges;
	uint32_t i;

	if (!idset || idset->range_count < 2) return;

	ranges = talloc_array(NULL, struct globset_range *, idset->range_count);
	range = idset->ranges;
	for (i = 0; i < idset->range_count; i++) {
		ranges[i] = range;
		range = range->next;
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

static void IDSET_compact_ranges(struct idset *idset)
{
	struct globset_range *range, *next_range, *prev_range;

	if (!idset || idset->range_count < 2) return;

	if (idset->single) {
		range = idset->ranges;
		next_range = range->next;
		while (next_range) {
			if (exchange_globcnt(next_range->low) < exchange_globcnt(range->low)) {
				range->low = next_range->low;
			}
			if (exchange_globcnt(next_range->high) > exchange_globcnt(range->high)) {
				range->high = next_range->high;
			}
			prev_range = next_range;
			next_range = next_range->next;
			talloc_free(prev_range);
		}
		range->next = NULL;
		range->prev = range;
		idset->range_count = 1;
	}
	else {
		range = idset->ranges;
		while (range) {
			next_range = range->next;
			while (next_range) {
				if (exchange_globcnt(next_range->low) >= exchange_globcnt(range->low)
				    && exchange_globcnt(next_range->low) <= exchange_globcnt(range->high)) {		/* A[  B[...  ]A */
					if (exchange_globcnt(next_range->high) > exchange_globcnt(range->high)) {	/* A[  B[  ]A  ]B -> A[  B[  ]AB */
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
static struct idset *IDSET_clone(TALLOC_CTX *mem_ctx, const struct idset *source_idset)
{
	struct globset_range *range, *clone;
	struct idset *idset = NULL, *head_idset = NULL, *tail_idset;

	if (!source_idset) return NULL;

	check_idset(source_idset);

	while (source_idset) {
		tail_idset = idset;
		idset = talloc_zero(mem_ctx, struct idset);
		idset->idbased = source_idset->idbased;
		if (idset->idbased) {
			idset->repl.id = source_idset->repl.id;
		}
		else {
			idset->repl.guid = source_idset->repl.guid;
		}
		idset->single = source_idset->single;
		idset->range_count = source_idset->range_count;

		range = source_idset->ranges;
		while (range) {
			clone = talloc_zero(idset, struct globset_range);
			clone->low = range->low;
			clone->high = range->high;
			DLIST_ADD_END(idset->ranges, clone, void);
			range = range->next;
		}

		if (!head_idset) {
			head_idset = idset;
		}
		if (tail_idset) {
			tail_idset->next = idset;
		}
		source_idset = source_idset->next;
	}

	check_idset(head_idset);

	return head_idset;
}

/**
  \details merge two idsets structures into a third one
*/
_PUBLIC_ struct idset *IDSET_merge_idsets(TALLOC_CTX *mem_ctx, const struct idset *left, const struct idset *right)
{
	struct idset *merged_idset, *clone_right, *current, *next;
	uint16_t current_id = 0, next_id;
	struct GUID *current_guid = NULL, *next_guid;
	bool added_ranges = false, same_id, idbased;
	struct globset_range *range;

	if (!left || left->range_count == 0) return IDSET_clone(mem_ctx, right);
	if (!right || right->range_count == 0) return IDSET_clone(mem_ctx, left);

	merged_idset = IDSET_clone(mem_ctx, left);
	clone_right = IDSET_clone(mem_ctx, right);

	current = merged_idset;
	while (current->next) {
		current = current->next;
	}
	current->next = clone_right;

	IDSET_reorder_idset(&merged_idset);

	current = merged_idset;
	idbased = current->idbased;
	if (idbased) {
		current_id = current->repl.id;
	} else {
		current_guid = &current->repl.guid;
	}
	while (current->next) {
		next = current->next;

		if (idbased) {
			next_id = next->repl.id;
			same_id = (current_id == next_id);
		} else {
			next_guid = &next->repl.guid;
			same_id = GUID_equal(current_guid, next_guid);
		}
		
		if (same_id) {
			added_ranges = true;
			current->range_count += next->range_count;
			range = next->ranges;
			current->ranges->prev->next = range;
			while (range) {
				(void) talloc_reference(current, range);
				range = range->next;
			}
			current->next = next->next;
			talloc_free(next);
		}
		else {
			current = next;
		}
	}

	if (added_ranges) {
		current = merged_idset;
		while (current) {
			IDSET_reorder_ranges(current);
			IDSET_compact_ranges(current);
			current = current->next;
		}
	}

	return merged_idset;
}

/**
  \details serialize an idset structure in a struct SBinary_r
*/
_PUBLIC_ struct Binary_r *IDSET_serialize(TALLOC_CTX *mem_ctx, const struct idset *idset)
{
	struct ndr_push	*ndr;
	struct globset_range *current_range;
	struct Binary_r *data;

	check_idset(idset);

	ndr = ndr_push_init_ctx(NULL);
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	ndr->offset = 0;

	while (idset) {
		if (idset->idbased) {
			ndr_push_uint16(ndr, NDR_SCALARS, idset->repl.id);
		} else {
			ndr_push_GUID(ndr, NDR_SCALARS, &idset->repl.guid);
		}

		current_range = idset->ranges;
		while (current_range) {
			GLOBSET_ndr_push_globset_range(ndr, current_range);
			current_range = current_range->next;
		}
		ndr_push_uint8(ndr, NDR_SCALARS, 0x00); /* end */
		idset = idset->next;
	}

	data = talloc_zero(mem_ctx, struct Binary_r);
	data->cb = ndr->offset;
	data->lpb = ndr->data;
	(void) talloc_reference(data, data->lpb);
	talloc_free(ndr);

	return data;
}

/**
  \details tests the presence of a specific id in the ranges of a ReplID-based idset structure
*/
_PUBLIC_ bool IDSET_includes_eid(const struct idset *idset, uint64_t eid)
{
	struct globset_range *range;
	uint16_t eid_id;
	uint64_t eid_globcnt;

	if (!idset || !idset->idbased) {
		return false;
	}

	eid_id = eid & 0xffff;
	eid_globcnt = eid >> 16;

	while (idset) {
		if (idset->repl.id == eid_id) {
			range = idset->ranges;
			while (range) {
				if (exchange_globcnt(range->low) <= exchange_globcnt(eid_globcnt)
				    && exchange_globcnt(range->high) >= exchange_globcnt(eid_globcnt)) {
					return true;
				}
				range = range->next;
			}
		}
		idset = idset->next;
	}

	return false;
}

/**
  \details tests the presence of a specific id in the ranges of a ReplGUID-based idset structure
*/
_PUBLIC_ bool IDSET_includes_guid_glob(const struct idset *idset, struct GUID *replica_guid, uint64_t id)
{
	struct globset_range *range;

	if (!idset || idset->idbased) {
		return false;
	}
	if (!replica_guid) {
		return false;
	}

	while (idset) {
		if (GUID_equal(&idset->repl.guid, replica_guid)) {
			range = idset->ranges;
			while (range) {
				if (exchange_globcnt(range->low) <= exchange_globcnt(id) && exchange_globcnt(range->high) >= exchange_globcnt(id)) {
					return true;
				}
				range = range->next;
			}
		}
		idset = idset->next;
	}

	return false;
}

static void IDSET_ranges_remove_globcnt(struct idset *idset, uint64_t eid) {
	struct globset_range *range, *new_range;
	bool done = false;
	uint64_t work_eid;

	work_eid = exchange_globcnt(eid);

	range = idset->ranges;
	while (!done && range) {
		if (range->low == eid) {
			if (range->high == eid) {
				DLIST_REMOVE(idset->ranges, range);
				idset->range_count--;
				if (range->next == NULL && idset->ranges != NULL) {
					idset->ranges->prev = range->prev;
				}
				talloc_free(range);
			}
			else {
				range->low = exchange_globcnt(work_eid + 1);
			}
			done = true;
		}
		else if (range->high == eid) {
			range->high = exchange_globcnt(work_eid + 1);
			done = true;
		}
		else if ((exchange_globcnt(range->low) < work_eid) && (exchange_globcnt(range->high) > work_eid)) {
			new_range = talloc_zero(idset, struct globset_range);
			new_range->low = exchange_globcnt(work_eid + 1);
			new_range->high = range->high;
			range->high = exchange_globcnt(work_eid - 1);
			DLIST_ADD_AFTER(idset->ranges, new_range, range);
			idset->range_count++;
			done = true;
		}
		else {
			range = range->next;
		}
	}
}

_PUBLIC_ void IDSET_remove_rawidset(struct idset *idset, const struct rawidset *rawidset)
{
	struct idset *current_idset;
	int i;

	if (!idset || !rawidset) {
		return;
	}

	if (idset->single) {
		return;
	}

	if (idset->idbased != rawidset->idbased) {
		return;
	}

	if (rawidset->next) {
		OC_DEBUG(5, "warning, only first rawidset will be taken into account");
	}

	current_idset = idset;
	if (idset->idbased) {
		while (current_idset && (current_idset->repl.id != rawidset->repl.id)) {
			current_idset = current_idset->next;
		}
	}
	else {
		while (current_idset && !GUID_equal(&current_idset->repl.guid, &rawidset->repl.guid)) {
			current_idset = current_idset->next;
		}
	}

	if (current_idset) {
		for (i = 0; i < rawidset->count; i++) {
			IDSET_ranges_remove_globcnt(current_idset, rawidset->globcnts[i]);
		}
	}

	check_idset(idset);
}

/**
  \details dump an idset structure

  \param idset pointer to the idset structure to dump
  \param label string to define the dump in log
*/
_PUBLIC_ void IDSET_dump(const struct idset *idset, const char *label)
{
	struct globset_range *range;
	uint32_t i;
	char *guid_str;

	OC_DEBUG(0, "[%s] Dump of idset", label);
	while (idset) {
		if (idset->idbased) {
			OC_DEBUG(0, "  %.4x: %d elements", idset->repl.id, idset->range_count);
		}
		else {
			guid_str = GUID_string(NULL, &idset->repl.guid);
			OC_DEBUG(0, "  %s: %d elements", guid_str, idset->range_count);
			talloc_free(guid_str);
		}

		range = idset->ranges;
		for (i = 0; i < idset->range_count; i++) {
			if (exchange_globcnt(range->low) > exchange_globcnt(range->high)) {
				oc_log(OC_LOG_ERROR, "Incorrect GLOBCNT range as high value is larger than low value");
			}
			OC_DEBUG(0, "  [0x%.12" PRIx64 ":0x%.12" PRIx64 "]", range->low, range->high);
			range = range->next;
		}

		idset = idset->next;
	}
}

/**
  \details check GLOBCNT ranges from an idset structure. [MS-OXCFXICS] Section 3.1.5.4.3.2.4

  \param idset pointer to the idset structure to check

  \return MAPI_E_SUCCESS on success, ecRpcFormat if any range is incorrect (low > high)
          or any range is NULL.
*/
_PUBLIC_ enum MAPISTATUS IDSET_check_ranges(const struct idset *idset)
{
	struct globset_range *range;
	uint32_t	     i;

	while (idset) {
		range = idset->ranges;
		for (i = 0; i < idset->range_count; i++) {
			OPENCHANGE_RETVAL_IF(!range, ecRpcFormat, NULL);
			if (exchange_globcnt(range->low) > exchange_globcnt(range->high)) {
				return ecRpcFormat;
			}
			range = range->next;
		}
		idset = idset->next;
	}

	return MAPI_E_SUCCESS;
}


/**
  \details push an idset on an ndr stream
*/

_PUBLIC_ void ndr_push_idset(struct ndr_push *ndr, struct idset *idset)
{
	struct Binary_r *bin_data;

	bin_data = IDSET_serialize(NULL, idset);
	ndr_push_Binary_r(ndr, NDR_BUFFERS, bin_data);
	talloc_free(bin_data);
}

_PUBLIC_ struct rawidset *RAWIDSET_make(TALLOC_CTX *mem_ctx, bool idbased, bool single)
{
	struct rawidset *rawidset;

	rawidset = talloc_zero(mem_ctx, struct rawidset);
	rawidset->mem_ctx = mem_ctx;
	rawidset->idbased = idbased;
	rawidset->single = single;
	rawidset->globcnts = talloc_zero(rawidset, uint64_t);
	rawidset->count = 0;
	rawidset->max_count = 0;
	rawidset->next = NULL;

	return rawidset;
}

static struct rawidset *RAWIDSET_find_by_ID(struct rawidset *rawidset, uint16_t replid, struct rawidset **rawidset_last)
{
	static struct rawidset *current;

	if (!rawidset->idbased) {
		return NULL;
	}

	current = rawidset;
	if (rawidset_last)
		*rawidset_last = current;
	while (current) {
		if (current->repl.id == replid) {
			return current;
		}
		else {
			current = current->next;
			if (rawidset_last && current)
				*rawidset_last = current;
		}
	}

	return NULL;
}

static struct rawidset *RAWIDSET_find_by_GUID(struct rawidset *rawidset, const struct GUID *guid, struct rawidset **rawidset_last)
{
	static struct rawidset *current;

	if (rawidset->idbased) {
		return NULL;
	}

	current = rawidset;
	if (rawidset_last)
		*rawidset_last = current;
	while (current) {
		if (GUID_equal(&current->repl.guid, guid)) {
			return current;
		}
		else {
			current = current->next;
			if (rawidset_last && current)
				*rawidset_last = current;
		}
	}

	return NULL;
}

_PUBLIC_ void RAWIDSET_push_eid(struct rawidset *rawidset, uint64_t eid)
{
	struct rawidset *glob_idset, *last_glob_idset = NULL;
	uint16_t eid_id;
	uint64_t eid_globcnt;

	if (!rawidset) return;

	eid_id = eid & 0xffff;
	eid_globcnt = eid >> 16;

	glob_idset = RAWIDSET_find_by_ID(rawidset, eid_id, &last_glob_idset);
	if (!glob_idset) {
		if (last_glob_idset == rawidset && rawidset->repl.id == 0) {
			glob_idset = rawidset;
		}
		else {
			glob_idset = RAWIDSET_make(rawidset->mem_ctx, true, rawidset->single);
			last_glob_idset->next = glob_idset;
		}
		glob_idset->repl.id = eid_id;
	}

	if (glob_idset->count + 1 > glob_idset->max_count) {
		glob_idset->max_count += 256;
		glob_idset->globcnts = talloc_realloc(glob_idset, glob_idset->globcnts, uint64_t, glob_idset->max_count);
	}
	glob_idset->globcnts[glob_idset->count] = eid_globcnt;
	glob_idset->count++;
}

_PUBLIC_ void RAWIDSET_push_guid_glob(struct rawidset *rawidset, const struct GUID *guid, uint64_t globcnt)
{
	struct rawidset		*glob_idset;
	struct rawidset		*last_glob_idset = NULL;
	static struct GUID	*zero_guid = NULL;

	if (!rawidset) return;

	/* OC_DEBUG(0, "pushing %.16"PRIx64" into idset...", globcnt); */

	if (globcnt == 0) {
		oc_log(OC_LOG_FATAL, "attempting to push a null globcnt");
		abort();
	}

	if ((globcnt & 0xffff000000000000)) {
		oc_log(OC_LOG_FATAL, "attempting to push a globcnt that has not been shifted by 16 bits beforehand");
		abort();
	}

	glob_idset = RAWIDSET_find_by_GUID(rawidset, guid, &last_glob_idset);
	if (!glob_idset) {
		if (!zero_guid) {
			zero_guid = talloc_zero(NULL, struct GUID);
		}

		glob_idset = RAWIDSET_find_by_GUID(rawidset, zero_guid, NULL);
		if (!glob_idset) {
			glob_idset = RAWIDSET_make(rawidset->mem_ctx, false, rawidset->single);
			if (last_glob_idset) {
				last_glob_idset->next = glob_idset;
			}
		}
		glob_idset->repl.guid = *guid;
	}

	if (glob_idset->count + 1 > glob_idset->max_count) {
		glob_idset->max_count += 256;
		glob_idset->globcnts = talloc_realloc(glob_idset, glob_idset->globcnts, uint64_t, glob_idset->max_count);
	}
	glob_idset->globcnts[glob_idset->count] = globcnt;
	glob_idset->count++;
}

_PUBLIC_ struct idset *RAWIDSET_convert_to_idset(TALLOC_CTX *mem_ctx, const struct rawidset *rawidset)
{
	struct idset *head_idset = NULL, *last_idset = NULL, *new_idset;
	const struct rawidset *current;
	bool valid_repl;

	current = rawidset;
	while (current) {
		if (current->idbased) {
			valid_repl = (current->repl.id != 0);
		}
		else {
			valid_repl = !GUID_all_zero(&current->repl.guid);
		}
		if (current->count > 0 && valid_repl) {
			new_idset = IDSET_make(mem_ctx, current->idbased, current->repl.id, &current->repl.guid,
					       current->globcnts, current->count, current->single);
			if (!head_idset) {
				head_idset = new_idset;
			}
			if (last_idset) {
				last_idset->next = new_idset;
			}
			last_idset = new_idset;
		}
		current = current->next;
	}

	check_idset(head_idset);

	return head_idset;
}
