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

#include <libocpf/ocpf.h>
#include <libocpf/ocpf_api.h>
#include <libocpf/lex.h>

void yyerror(char *);

union SPropValue_CTR	lpProp;
struct ocpf_nprop      	nprop;
int		       	typeset;
uint16_t       	       	type;
int			folderset;

%}

%union {
	uint8_t				b;
	uint32_t			l;
	uint64_t			d;
	char				*name;
	char				*date;
	char				*var;
	struct SLPSTRArray		MVszA;
}

%token <b> BOOLEAN
%token <l> INTEGER
%token <d> DOUBLE
%token <name> IDENTIFIER
%token <name> STRING
%token <MVszA> MVSTRING
%token <date> SYSTIME
%token <var> VAR

%token kw_TYPE
%token kw_FOLDER
%token kw_OLEGUID
%token kw_SET
%token kw_PROPERTY
%token kw_NPROPERTY
%token kw_OOM
%token kw_MNID_ID
%token kw_MNID_STRING

%token kw_PT_BOOLEAN
%token kw_PT_STRING8
%token kw_PT_LONG
%token kw_PT_SYSTIME

%token OBRACE
%token EBRACE
%token SEMICOLON
%token COLON
%token EQUAL

%start keywords

%%

keywords	: | keywords kvalues
		;

kvalues		: Type
		| Folder
		| OLEGUID
		| Set
		| Property
		| NProperty
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
			ocpf_oleguid_add($2, $3);
		}
		;

Set		:
		kw_SET VAR EQUAL propvalue
		{
			ocpf_variable_add($2, lpProp, type);
		}
		;

Property	:
		kw_PROPERTY OBRACE pcontent EBRACE SEMICOLON
		{
		}

pcontent       	: | pcontent content
		;

content		:
		IDENTIFIER EQUAL propvalue
		{
			ocpf_propvalue_s($1, lpProp);
		}
		| INTEGER EQUAL propvalue
		{
			ocpf_propvalue($1, "UNNAMED", lpProp);
		}
		| IDENTIFIER EQUAL VAR
		{
			ocpf_propvalue_var($1, 0x0, $3);
		}
		| INTEGER EQUAL VAR
		{
			ocpf_propvalue_var(NULL, $1, $3);
		}
		;

propvalue	: STRING	
		{ 
			lpProp.lpszA = talloc_strdup(ocpf->mem_ctx, $1); 
			type = PT_STRING8; 
		}
		| INTEGER	{ lpProp.l = $1; type = PT_LONG; }
		| BOOLEAN	{ lpProp.b = $1; type = PT_BOOLEAN; };
		| DOUBLE	{ lpProp.d = $1; type = PT_DOUBLE; }
		| SYSTIME
		{
			ocpf_add_filetime($1, &lpProp.ft);
			type = PT_SYSTIME;
		}
		| MVSTRING	
		{
			type = PT_MV_STRING8;
			lpProp.MVszA = $1;
		}
		;

NProperty	:
		kw_NPROPERTY OBRACE npcontent EBRACE SEMICOLON
		{
		}

npcontent	: | npcontent ncontent
		;

ncontent	: kind EQUAL propvalue
		{
			ocpf_nproperty_add(&nprop, lpProp, NULL, 0);
		}
		| known_kind EQUAL propvalue
		{
			ocpf_nproperty_add(&nprop, lpProp, NULL, 0);
		}
		| kind EQUAL VAR
		{
			ocpf_nproperty_add(&nprop, lpProp, $3, type);
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

%%

void yyerror(char *s)
{
	printf("%s", s);
}
