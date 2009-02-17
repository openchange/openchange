/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2008.

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

%{

#include "libocpf/ocpf_private.h"
#include <libocpf/ocpf.h>
#include <libocpf/ocpf_api.h>
#include <libocpf/lex.h>

void yyerror(char *);

union SPropValue_CTR	lpProp;
struct ocpf_nprop      	nprop;
int		       	typeset;
uint16_t       	       	type;
int			folderset;
uint8_t			recip_type;

%}

%union {
	uint8_t				i;
	uint8_t				b;
	uint16_t			s;
	uint32_t			l;
	uint64_t			d;
	char				*name;
	char				*nameW;
	char				*date;
	char				*var;
	struct StringArray_r		MVszA;
}

%token <i> UINT8
%token <b> BOOLEAN
%token <s> SHORT
%token <l> INTEGER
%token <d> DOUBLE
%token <name> IDENTIFIER
%token <name> STRING
%token <nameW> UNICODE
%token <MVszA> MVSTRING
%token <date> SYSTIME
%token <var> VAR

%token kw_TYPE
%token kw_FOLDER
%token kw_OLEGUID
%token kw_SET
%token kw_PROPERTY
%token kw_NPROPERTY
%token kw_RECIPIENT
%token kw_TO
%token kw_CC
%token kw_BCC
%token kw_OOM
%token kw_MNID_ID
%token kw_MNID_STRING

%token kw_PT_BOOLEAN
%token kw_PT_STRING8
%token kw_PT_UNICODE
%token kw_PT_SHORT
%token kw_PT_LONG
%token kw_PT_SYSTIME
%token kw_PT_MV_STRING8
%token kw_PT_BINARY

%token OBRACE
%token EBRACE
%token COMMA
%token SEMICOLON
%token COLON
%token LOWER
%token GREATER
%token EQUAL

%start keywords

%%

keywords	: | keywords kvalues
		{
			memset(&lpProp, 0, sizeof (union SPropValue_CTR));
		}
		;

kvalues		: Type
		| Folder
		| OLEGUID
		| Set
		| Property
		| NProperty
		| Recipient
		;

Type		: 
		kw_TYPE STRING
		{
			if (!typeset) {
				ocpf_type_add($2);
				typeset++;
			} else {
				error_message("%s", "duplicated TYPE\n");
				return -1;
			}
		}
		;

Folder		:
		kw_FOLDER STRING
		{
			if (folderset == false) {
				ocpf_folder_add($2, 0, NULL);
				folderset = true;
			} else {
				error_message("%s", "duplicated FOLDER\n");
			}
		}
		| kw_FOLDER DOUBLE
		{
			if (folderset == false) {
				ocpf_folder_add(NULL, $2, NULL);
				folderset = true;
			} else {
				error_message("%s", "duplicated FOLDER\n");
			}
		}
		| kw_FOLDER VAR
		{
			if (folderset == false) {
				ocpf_folder_add(NULL, 0, $2);
				folderset = true;
			} else {
				error_message("%s", "duplicated FOLDER\n");
			}
		}
		;

OLEGUID		: 
		kw_OLEGUID IDENTIFIER STRING
		{ 
			char *name;
			char *guid;
			
			name = talloc_strdup(ocpf->mem_ctx, $2);
			guid = talloc_strdup(ocpf->mem_ctx, $3);

			ocpf_oleguid_add(name, guid);
		}
		;

Set		:
		kw_SET VAR EQUAL propvalue
		{
			ocpf_variable_add($2, lpProp, type, true);
			memset(&lpProp, 0, sizeof (union SPropValue_CTR));
		}
		;

Property	:
		kw_PROPERTY OBRACE pcontent EBRACE SEMICOLON
		{
		}

pcontent       	: | pcontent content
		{
			memset(&lpProp, 0, sizeof (union SPropValue_CTR));
		}
		;

content		:
		IDENTIFIER EQUAL propvalue
		{
		  ocpf_propvalue_s($1, lpProp, type, true);
			ocpf_propvalue_free(lpProp, type);
		}
		| INTEGER EQUAL propvalue
		{
			ocpf_propvalue($1, lpProp, type, true);
			ocpf_propvalue_free(lpProp, type);
		}
		| IDENTIFIER EQUAL VAR
		{
			ocpf_propvalue_var($1, 0x0, $3, true);
		}
		| INTEGER EQUAL VAR
		{
			ocpf_propvalue_var(NULL, $1, $3, true);
		}
		;

propvalue	: STRING	
		{ 
			lpProp.lpszA = talloc_strdup(ocpf->mem_ctx, $1); 
			type = PT_STRING8; 
		}
		| UNICODE
		{
			lpProp.lpszW = talloc_strdup(ocpf->mem_ctx, $1);
			type = PT_UNICODE;
		}
		| SHORT		{ lpProp.i = $1; type = PT_SHORT; }
		| INTEGER	{ lpProp.l = $1; type = PT_LONG; }
		| BOOLEAN	{ lpProp.b = $1; type = PT_BOOLEAN; }
		| DOUBLE	{ lpProp.d = $1; type = PT_DOUBLE; }
		| SYSTIME
		{
			ocpf_add_filetime($1, &lpProp.ft);
			type = PT_SYSTIME;
		}
		| OBRACE mvstring_contents STRING EBRACE
		{
			TALLOC_CTX *mem_ctx;

			if (!lpProp.MVszA.cValues) {
				lpProp.MVszA.cValues = 0;
				lpProp.MVszA.lppszA = talloc_array(ocpf->mem_ctx, const char *, 2);
			} else {
				lpProp.MVszA.lppszA = talloc_realloc(NULL, lpProp.MVszA.lppszA, const char *,
								     lpProp.MVszA.cValues + 2);
			}
			mem_ctx = (TALLOC_CTX *) lpProp.MVszA.lppszA;
			lpProp.MVszA.lppszA[lpProp.MVszA.cValues] = talloc_strdup(mem_ctx, $3);
			lpProp.MVszA.cValues += 1;

			type = PT_MV_STRING8;
		}
		| OBRACE binary_contents EBRACE
		{
			type = PT_BINARY;
		}
		| LOWER STRING GREATER
		{
			int	ret;

			ret = ocpf_binary_add($2, &lpProp.bin);
			type = (ret == OCPF_SUCCESS) ? PT_BINARY : PT_ERROR;
		}
		;

mvstring_contents: | mvstring_contents mvstring_content


mvstring_content  : STRING COMMA
		  {
			TALLOC_CTX *mem_ctx;

			if (!lpProp.MVszA.cValues) {
				lpProp.MVszA.cValues = 0;
				lpProp.MVszA.lppszA = talloc_array(ocpf->mem_ctx, const char *, 2);
			} else {
				lpProp.MVszA.lppszA = talloc_realloc(NULL, lpProp.MVszA.lppszA, const char *,
								     lpProp.MVszA.cValues + 2);
			}
			mem_ctx = (TALLOC_CTX *) lpProp.MVszA.lppszA;
			lpProp.MVszA.lppszA[lpProp.MVszA.cValues] = talloc_strdup(mem_ctx, $1);
			lpProp.MVszA.cValues += 1;
		  }
		  ;

binary_contents: | binary_contents binary_content

binary_content	: INTEGER
		{
			TALLOC_CTX *mem_ctx;

			if ($1 > 0xFF) {
				error_message("Invalid Binary constant: 0x%x > 0xFF\n", $1);
			}

			if (!lpProp.bin.cb) {
				lpProp.bin.cb = 0;
				lpProp.bin.lpb = talloc_array(ocpf->mem_ctx, uint8_t, 2);
			} else {
				lpProp.bin.lpb = talloc_realloc(NULL, lpProp.bin.lpb, uint8_t,
								lpProp.bin.cb + 2);
			}
			mem_ctx = (TALLOC_CTX *) lpProp.bin.lpb;
			lpProp.bin.lpb[lpProp.bin.cb] = $1;
			lpProp.bin.cb += 1;
		}
		;

NProperty	:
		kw_NPROPERTY OBRACE npcontent EBRACE SEMICOLON
		{
		}

npcontent	: | npcontent ncontent
		{
			memset(&lpProp, 0, sizeof (union SPropValue_CTR));
		}
		;

ncontent	: kind EQUAL propvalue
		{
			ocpf_nproperty_add(&nprop, lpProp, NULL, type, true);
		}
		| known_kind EQUAL propvalue
		{
			ocpf_nproperty_add(&nprop, lpProp, NULL, type, true);
		}
		| kind EQUAL VAR
		{
			ocpf_nproperty_add(&nprop, lpProp, $3, type, true);
		}
		| known_kind EQUAL VAR
		{
			ocpf_nproperty_add(&nprop, lpProp, $3, type, true);
		}
		;

kind		: kw_OOM COLON IDENTIFIER COLON IDENTIFIER
		{
			memset(&nprop, 0, sizeof (struct ocpf_nprop));
			nprop.OOM = talloc_strdup(ocpf->mem_ctx, $3);
			nprop.guid = $5;
		}
		| kw_MNID_ID COLON INTEGER COLON proptype COLON IDENTIFIER
		{
			nprop.registered = false;
			nprop.mnid_id = $3;
			nprop.guid = $7;
		}
		| kw_MNID_STRING COLON STRING COLON proptype COLON IDENTIFIER
		{
			nprop.registered = false;
			nprop.mnid_string = talloc_strdup(ocpf->mem_ctx, $3);
			nprop.guid = $7;
		}
		;

proptype	: kw_PT_STRING8	
		{
 			memset(&nprop, 0, sizeof (struct ocpf_nprop));
			nprop.propType = PT_STRING8; 
		}
		| kw_PT_UNICODE
		{
			memset(&nprop, 0, sizeof (struct ocpf_nprop));
			nprop.propType = PT_UNICODE; 
		}
		| kw_PT_SHORT
		{
			memset(&nprop, 0, sizeof (struct ocpf_nprop));
			nprop.propType = PT_SHORT;
		}
		| kw_PT_LONG 
		{
			memset(&nprop, 0, sizeof (struct ocpf_nprop));
			nprop.propType = PT_LONG; 
		}
		| kw_PT_BOOLEAN
		{
			memset(&nprop, 0, sizeof (struct ocpf_nprop));
			nprop.propType = PT_BOOLEAN;
		}
		| kw_PT_SYSTIME
		{
			memset(&nprop, 0, sizeof (struct ocpf_nprop));
			nprop.propType = PT_SYSTIME; 
		}
		| kw_PT_MV_STRING8
		{
			memset(&nprop, 0, sizeof (struct ocpf_nprop));
			nprop.propType = PT_MV_STRING8;
		}
		| kw_PT_BINARY
		{
			memset(&nprop, 0, sizeof (struct ocpf_nprop));
			nprop.propType = PT_BINARY;
		}
		;

known_kind	: kw_MNID_ID COLON INTEGER COLON IDENTIFIER
		{
			memset(&nprop, 0, sizeof (struct ocpf_nprop));
			nprop.registered = true;
			nprop.mnid_id = $3;
			nprop.guid = $5;
		}
		| kw_MNID_STRING COLON STRING COLON IDENTIFIER
		{
			memset(&nprop, 0, sizeof (struct ocpf_nprop));
			nprop.registered = true;
			nprop.mnid_string = talloc_strdup(ocpf->mem_ctx, $3);
			nprop.guid = $5;
		}
		;

Recipient	: 
		kw_RECIPIENT recipClass recipients STRING
		{
			char	*recipient = NULL;

			recipient = talloc_strdup(ocpf->mem_ctx, $4);
			ocpf_recipient_add(recip_type, recipient);
			talloc_free(recipient);

			recip_type = 0;
		}
		;

recipClass	: kw_TO
		{
			recip_type = MAPI_TO;
		}
		| kw_CC
		{
			recip_type = MAPI_CC;
		}
		| kw_BCC
		{
			recip_type = MAPI_BCC;
		}
		;

recipients	: | recipients recipient

recipient	: STRING SEMICOLON
		{
			char	*recipient = NULL;

			recipient = talloc_strdup(ocpf->mem_ctx, $1);
			ocpf_recipient_add(recip_type, recipient);
			talloc_free(recipient);
		}

%%

void yyerror(char *s)
{
	printf("%s: %d", s, lineno);
}
