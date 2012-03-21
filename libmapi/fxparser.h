/*
   OpenChange MAPI implementation.

   Copyright (C) Brad Hards <bradh@openchange.org> 2010

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

#ifndef __LIBMAPI_FXPARSER_H__
#define __LIBMAPI_FXPARSER_H__

/* This header is private to the parser. If you use this directly, you may suffer API or ABI breakage.
   
   We mean it.
*/

enum fx_parser_state { ParserState_Entry, ParserState_HaveTag, ParserState_HavePropTag };

struct fx_parser_context {
	TALLOC_CTX		*mem_ctx;
	DATA_BLOB		data;	/* the data we have (so far) to parse */
	uint32_t		idx;	/* where we are up to in the data blob */
	enum fx_parser_state	state;
	struct SPropValue	lpProp;		/* the current property tag and value we are parsing */
	struct MAPINAMEID	namedprop;	/* the current named property we are parsing */
	bool 			enough_data;
	uint32_t		tag;
	void			*priv;
	
	/* callbacks for parser actions */
	enum MAPISTATUS (*op_marker)(uint32_t, void *);
	enum MAPISTATUS (*op_delprop)(uint32_t, void *);
	enum MAPISTATUS (*op_namedprop)(uint32_t, struct MAPINAMEID, void *);
	enum MAPISTATUS (*op_property)(struct SPropValue, void *);
};

#endif
