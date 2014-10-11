/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2008-2011.

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

#include "libocpf/ocpf.h"
#include "libocpf/ocpf_api.h"
#include "libocpf/lex.h"

int ocpf_yylex(void *, void *);
void yyerror(struct ocpf_context *, void *, char *);

%}

%pure-parser
%parse-param {struct ocpf_context *ctx}
%parse-param {void *scanner}
%lex-param {yyscan_t *scanner}
%name-prefix "ocpf_yy"

%union {
	uint8_t				i;
	uint8_t				b;
	uint16_t			s;
	uint32_t			l;
	int64_t				d;
	double				dbl;
	char				*name;
	char				*nameW;
	char				*date;
	char				*var;
	struct LongArray_r		MVl;
	struct StringArray_r		MVszA;
	struct StringArrayW_r		MVszW;
	struct BinaryArray_r		MVbin;
}

%token <i> UINT8
%token <b> BOOLEAN
%token <s> SHORT
%token <l> INTEGER
%token <d> I8
%token <dbl> DOUBLE
%token <name> IDENTIFIER
%token <name> STRING
%token <nameW> UNICODE
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
%token kw_PT_I8
%token kw_PT_DOUBLE
%token kw_PT_SYSTIME
%token kw_PT_MV_LONG
%token kw_PT_MV_BINARY
%token kw_PT_MV_STRING8
%token kw_PT_MV_UNICODE
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
			memset(&ctx->lpProp, 0, sizeof (union SPropValue_CTR));
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
			if (!ctx->typeset) {
			  ocpf_type_add(ctx,$2);
				ctx->typeset++;
			} else {
				ocpf_error_message(ctx, "%s", "duplicated TYPE\n");
				return -1;
			}
		}
		;

Folder		:
		kw_FOLDER STRING
		{
			if (ctx->folderset == false) {
				ocpf_folder_add(ctx, $2, 0, NULL);
				ctx->folderset = true;
			} else {
				ocpf_error_message(ctx, "%s", "duplicated FOLDER\n");
			}
		}
		| kw_FOLDER I8
		{
			if (ctx->folderset == false) {
				ocpf_folder_add(ctx, NULL, $2, NULL);
				ctx->folderset = true;
			} else {
				ocpf_error_message(ctx,"%s", "duplicated FOLDER\n");
			}
		}
		| kw_FOLDER VAR
		{
			if (ctx->folderset == false) {
				ocpf_folder_add(ctx, NULL, 0, $2);
				ctx->folderset = true;
			} else {
				ocpf_error_message(ctx,"%s", "duplicated FOLDER\n");
			}
		}
		;

OLEGUID		: 
		kw_OLEGUID IDENTIFIER STRING
		{ 
			char *name;
			char *guid;
			
			name = talloc_strdup(ctx, $2);
			guid = talloc_strdup(ctx, $3);

			ocpf_oleguid_add(ctx, name, guid);
		}
		;

Set		:
		kw_SET VAR EQUAL propvalue
		{
			ocpf_variable_add(ctx, $2, ctx->lpProp, ctx->ltype, true);
			memset(&ctx->lpProp, 0, sizeof (union SPropValue_CTR));
		}
		;

Recipient	:
		kw_RECIPIENT OBRACE recipients EBRACE SEMICOLON
		{
		}

recipients	: | recipients recipient

recipient	:
		kw_TO OBRACE rpcontent EBRACE SEMICOLON
		{
			ocpf_recipient_set_class(ctx, MAPI_TO);
			ocpf_new_recipient(ctx);
		}
		| kw_CC OBRACE rpcontent EBRACE SEMICOLON
		{
			ocpf_recipient_set_class(ctx, MAPI_CC);
			ocpf_new_recipient(ctx);
		}
		| kw_BCC OBRACE rpcontent EBRACE SEMICOLON
		{
			ocpf_recipient_set_class(ctx, MAPI_BCC);
			ocpf_new_recipient(ctx);
		}
		;

rpcontent	: | rpcontent rcontent
		{
			memset(&ctx->lpProp, 0, sizeof (union SPropValue_CTR));
		}

rcontent	:
		IDENTIFIER EQUAL propvalue
		{
			ocpf_propvalue_s(ctx, $1, ctx->lpProp, ctx->ltype, true, kw_RECIPIENT);
			ocpf_propvalue_free(ctx->lpProp, ctx->ltype);
		}
		| INTEGER EQUAL propvalue
		{
			ocpf_propvalue(ctx, $1, ctx->lpProp, ctx->ltype, true, kw_RECIPIENT);
			ocpf_propvalue_free(ctx->lpProp, ctx->ltype);
		}
		| IDENTIFIER EQUAL VAR
		{
			ocpf_propvalue_var(ctx, $1, 0x0, $3, true, kw_RECIPIENT);
		}
		| INTEGER EQUAL VAR
		{
			ocpf_propvalue_var(ctx, NULL, $1, $3, true, kw_RECIPIENT);
		};

Property	:
		kw_PROPERTY OBRACE pcontent EBRACE SEMICOLON
		{
		}

pcontent       	: | pcontent content
		{
			memset(&ctx->lpProp, 0, sizeof (union SPropValue_CTR));
		}
		;

content		:
		IDENTIFIER EQUAL propvalue
		{
			ocpf_propvalue_s(ctx, $1, ctx->lpProp, ctx->ltype, true, kw_PROPERTY);
			ocpf_propvalue_free(ctx->lpProp, ctx->ltype);
		}
		| INTEGER EQUAL propvalue
		{
			ocpf_propvalue(ctx, $1, ctx->lpProp, ctx->ltype, true, kw_PROPERTY);
			ocpf_propvalue_free(ctx->lpProp, ctx->ltype);
		}
		| IDENTIFIER EQUAL VAR
		{
			ocpf_propvalue_var(ctx, $1, 0x0, $3, true, kw_PROPERTY);
		}
		| INTEGER EQUAL VAR
		{
			ocpf_propvalue_var(ctx, NULL, $1, $3, true, kw_PROPERTY);
		}
		;

propvalue	: STRING	
		{ 
			ctx->lpProp.lpszA = talloc_strdup(ctx, $1); 
			ctx->ltype = PT_STRING8; 
		}
		| UNICODE
		{
			ctx->lpProp.lpszW = talloc_strdup(ctx, $1);
			ctx->ltype = PT_UNICODE;
		}
		| SHORT		{ ctx->lpProp.i = $1; ctx->ltype = PT_SHORT; }
		| INTEGER	{ ctx->lpProp.l = $1; ctx->ltype = PT_LONG; }
		| BOOLEAN	{ ctx->lpProp.b = $1; ctx->ltype = PT_BOOLEAN; }
		| I8		{ ctx->lpProp.d = $1; ctx->ltype = PT_I8; }
		| DOUBLE	{ ctx->lpProp.dbl = $1, ctx->ltype = PT_DOUBLE; }
		| SYSTIME
		{
			ocpf_add_filetime($1, &ctx->lpProp.ft);
			ctx->ltype = PT_SYSTIME;
		}
		| OBRACE mvlong_contents INTEGER EBRACE
		{
			if (!ctx->lpProp.MVl.cValues) {
				ctx->lpProp.MVl.cValues = 0;
				ctx->lpProp.MVl.lpl = talloc_array(ctx, uint32_t, 2);
			} else {
				ctx->lpProp.MVl.lpl = talloc_realloc(NULL, ctx->lpProp.MVl.lpl,
								     uint32_t,
								     ctx->lpProp.MVl.cValues + 2);
			}
			ctx->lpProp.MVl.lpl[ctx->lpProp.MVl.cValues] = $3;
			ctx->lpProp.MVl.cValues += 1;

			ctx->ltype = PT_MV_LONG;
		}
		| OBRACE mvstring_contents STRING EBRACE
		{
			TALLOC_CTX	*mem_ctx;

			if (!ctx->lpProp.MVszA.cValues) {
				ctx->lpProp.MVszA.cValues = 0;
				ctx->lpProp.MVszA.lppszA = talloc_array(ctx, const char *, 2);
			} else {
				ctx->lpProp.MVszA.lppszA = talloc_realloc(NULL, ctx->lpProp.MVszA.lppszA, 
									  const char *,
									  ctx->lpProp.MVszA.cValues + 2);
			}
			mem_ctx = (TALLOC_CTX *) ctx->lpProp.MVszA.lppszA;
			ctx->lpProp.MVszA.lppszA[ctx->lpProp.MVszA.cValues] = talloc_strdup(mem_ctx, $3);
			ctx->lpProp.MVszA.cValues += 1;

			ctx->ltype = PT_MV_STRING8;
		}
		| OBRACE mvunicode_contents UNICODE EBRACE
		{
			TALLOC_CTX	*mem_ctx;
			
			if (!ctx->lpProp.MVszW.cValues) {
				ctx->lpProp.MVszW.cValues = 0;
				ctx->lpProp.MVszW.lppszW = talloc_array(ctx, const char *, 2);
			} else {
				ctx->lpProp.MVszW.lppszW = talloc_realloc(NULL, ctx->lpProp.MVszW.lppszW,
									  const char *,
									  ctx->lpProp.MVszW.cValues + 2);
			}
			mem_ctx = (TALLOC_CTX *) ctx->lpProp.MVszW.lppszW;
			ctx->lpProp.MVszW.lppszW[ctx->lpProp.MVszW.cValues] = talloc_strdup(mem_ctx, $3);
			ctx->lpProp.MVszW.cValues += 1;

			ctx->ltype = PT_MV_UNICODE;
		}
		| OBRACE binary_contents EBRACE
		{
			ctx->lpProp.bin.cb = ctx->bin.cb;
			ctx->lpProp.bin.lpb = talloc_memdup(ctx, ctx->bin.lpb, ctx->bin.cb);

			talloc_free(ctx->bin.lpb);
			ctx->bin.cb = 0;

			ctx->ltype = PT_BINARY;
		}
		| OBRACE mvbin_contents OBRACE binary_contents EBRACE EBRACE
		{
			TALLOC_CTX	*mem_ctx;

			if (!ctx->lpProp.MVbin.cValues) {
				ctx->lpProp.MVbin.cValues = 0;
				ctx->lpProp.MVbin.lpbin = talloc_array(ctx, struct Binary_r, 2);
			} else {
				ctx->lpProp.MVbin.lpbin = talloc_realloc(NULL, ctx->lpProp.MVbin.lpbin,
									 struct Binary_r,
									 ctx->lpProp.MVbin.cValues + 2);
			}
			mem_ctx = (TALLOC_CTX *) ctx->lpProp.MVbin.lpbin;
			ctx->lpProp.MVbin.lpbin[ctx->lpProp.MVbin.cValues].cb = ctx->bin.cb;
			ctx->lpProp.MVbin.lpbin[ctx->lpProp.MVbin.cValues].lpb = talloc_memdup(mem_ctx,
											       ctx->bin.lpb, 
											       ctx->bin.cb);
			ctx->lpProp.MVbin.cValues += 1;
			talloc_free(ctx->bin.lpb);
			ctx->bin.cb = 0;

			ctx->ltype = PT_MV_BINARY;
		}
		| LOWER STRING GREATER
		{
			int	ret;

			ret = ocpf_binary_add(ctx, $2, &ctx->lpProp.bin);
			ctx->ltype = (ret == OCPF_SUCCESS) ? PT_BINARY : PT_ERROR;
		}
		;

mvlong_contents: | mvlong_contents mvlong_content

mvlong_content :  INTEGER COMMA
		{
			if (!ctx->lpProp.MVl.cValues) {
				ctx->lpProp.MVl.cValues = 0;
				ctx->lpProp.MVl.lpl = talloc_array(ctx, uint32_t, 2);
			} else {
				ctx->lpProp.MVl.lpl = talloc_realloc(NULL, ctx->lpProp.MVl.lpl, uint32_t,
								     ctx->lpProp.MVl.cValues + 2);
			}
			ctx->lpProp.MVl.lpl[ctx->lpProp.MVl.cValues] = $1;
			ctx->lpProp.MVl.cValues += 1;
		}
		;


mvstring_contents: | mvstring_contents mvstring_content


mvstring_content  : STRING COMMA
		  {
			TALLOC_CTX	*mem_ctx;

			if (!ctx->lpProp.MVszA.cValues) {
				ctx->lpProp.MVszA.cValues = 0;
				ctx->lpProp.MVszA.lppszA = talloc_array(ctx, const char *, 2);
			} else {
				ctx->lpProp.MVszA.lppszA = talloc_realloc(NULL, ctx->lpProp.MVszA.lppszA, 
									  const char *,
									  ctx->lpProp.MVszA.cValues + 2);
			}
			mem_ctx = (TALLOC_CTX *) ctx->lpProp.MVszA.lppszA;
			ctx->lpProp.MVszA.lppszA[ctx->lpProp.MVszA.cValues] = talloc_strdup(mem_ctx, $1);
			ctx->lpProp.MVszA.cValues += 1;
		  }
		  ;

mvunicode_contents: | mvunicode_contents mvunicode_content

mvunicode_content: UNICODE COMMA
		{
			TALLOC_CTX *mem_ctx;

			if (!ctx->lpProp.MVszW.cValues) {
				ctx->lpProp.MVszW.cValues = 0;
				ctx->lpProp.MVszW.lppszW = talloc_array(ctx, const char *, 2);
			} else {
				ctx->lpProp.MVszW.lppszW = talloc_realloc(NULL, ctx->lpProp.MVszW.lppszW,
									  const char *,
									  ctx->lpProp.MVszW.cValues + 2);
			}
			mem_ctx = (TALLOC_CTX *) ctx->lpProp.MVszW.lppszW;
			ctx->lpProp.MVszW.lppszW[ctx->lpProp.MVszW.cValues] = talloc_strdup(mem_ctx, $1);
			ctx->lpProp.MVszW.cValues += 1;
		}
		;

binary_contents: | binary_contents binary_content

binary_content	: UINT8
		{
			if ($1 > 0xFF) {
				ocpf_error_message(ctx,"Invalid Binary constant: 0x%x > 0xFF\n", $1);
			}

			if (!ctx->bin.cb) {
				ctx->bin.cb = 0;
				ctx->bin.lpb = talloc_array(ctx, uint8_t, 2);
			} else {
				ctx->bin.lpb = talloc_realloc(NULL, ctx->bin.lpb, uint8_t,
								     ctx->bin.cb + 2);
			}
			ctx->bin.lpb[ctx->bin.cb] = $1;
			ctx->bin.cb += 1;
		}
		;

mvbin_contents: | mvbin_contents mvbin_content

mvbin_content	: OBRACE binary_contents EBRACE COMMA
		{
			TALLOC_CTX	*mem_ctx;

			if (!ctx->lpProp.MVbin.cValues) {
				ctx->lpProp.MVbin.cValues = 0;
				ctx->lpProp.MVbin.lpbin = talloc_array(ctx, struct Binary_r, 2);
			} else {
				ctx->lpProp.MVbin.lpbin = talloc_realloc(NULL, ctx->lpProp.MVbin.lpbin,
									 struct Binary_r,
									 ctx->lpProp.MVbin.cValues + 2);
			}
			mem_ctx = (TALLOC_CTX *) ctx->lpProp.MVbin.lpbin;
			ctx->lpProp.MVbin.lpbin[ctx->lpProp.MVbin.cValues].cb = ctx->bin.cb;
			ctx->lpProp.MVbin.lpbin[ctx->lpProp.MVbin.cValues].lpb = talloc_memdup(mem_ctx,
											       ctx->bin.lpb,
											       ctx->bin.cb);
			ctx->lpProp.MVbin.cValues += 1;

			ctx->bin.cb = 0;
		}
		;

NProperty	:
		kw_NPROPERTY OBRACE npcontent EBRACE SEMICOLON
		{
		}

npcontent	: | npcontent ncontent
		{
			memset(&ctx->lpProp, 0, sizeof (union SPropValue_CTR));
		}
		;

ncontent	: kind EQUAL propvalue
		{
			ocpf_nproperty_add(ctx, &ctx->nprop, ctx->lpProp, NULL, ctx->ltype, true);
		}
		| known_kind EQUAL propvalue
		{
			ocpf_nproperty_add(ctx, &ctx->nprop, ctx->lpProp, NULL, ctx->ltype, true);
		}
		| kind EQUAL VAR
		{
			ocpf_nproperty_add(ctx, &ctx->nprop, ctx->lpProp, $3, ctx->ltype, true);
		}
		| known_kind EQUAL VAR
		{
			ocpf_nproperty_add(ctx, &ctx->nprop, ctx->lpProp, $3, ctx->ltype, true);
		}
		;

kind		: kw_OOM COLON IDENTIFIER COLON IDENTIFIER
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.OOM = talloc_strdup(ctx, $3);
			ctx->nprop.guid = $5;
		}
		| kw_MNID_ID COLON INTEGER COLON proptype COLON IDENTIFIER
		{
			ctx->nprop.registered = false;
			ctx->nprop.mnid_id = $3;
			ctx->nprop.guid = $7;
		}
		| kw_MNID_STRING COLON STRING COLON proptype COLON IDENTIFIER
		{
			ctx->nprop.registered = false;
			ctx->nprop.mnid_string = talloc_strdup(ctx, $3);
			ctx->nprop.guid = $7;
		}
		;

proptype	: kw_PT_STRING8	
		{
 			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.propType = PT_STRING8; 
		}
		| kw_PT_UNICODE
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.propType = PT_UNICODE; 
		}
		| kw_PT_SHORT
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.propType = PT_SHORT;
		}
		| kw_PT_LONG 
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.propType = PT_LONG; 
		}
		| kw_PT_DOUBLE
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.propType = PT_DOUBLE;
		}
		| kw_PT_I8
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.propType = PT_I8;
		}
		| kw_PT_BOOLEAN
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.propType = PT_BOOLEAN;
		}
		| kw_PT_SYSTIME
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.propType = PT_SYSTIME; 
		}
		| kw_PT_MV_LONG
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.propType = PT_MV_LONG;
		}
		| kw_PT_MV_STRING8
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.propType = PT_MV_STRING8;
		}
		| kw_PT_MV_UNICODE
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.propType = PT_MV_UNICODE;
		}
		| kw_PT_BINARY
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.propType = PT_BINARY;
		}
		| kw_PT_MV_BINARY
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.propType = PT_MV_BINARY;
		}
		;

known_kind	: kw_MNID_ID COLON INTEGER COLON IDENTIFIER
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.registered = true;
			ctx->nprop.mnid_id = $3;
			ctx->nprop.guid = $5;
		}
		| kw_MNID_STRING COLON STRING COLON IDENTIFIER
		{
			memset(&ctx->nprop, 0, sizeof (struct ocpf_nprop));
			ctx->nprop.registered = true;
			ctx->nprop.mnid_string = talloc_strdup(ctx, $3);
			ctx->nprop.guid = $5;
		}
		;

%%

void yyerror(struct ocpf_context *ctx, void *scanner, char *s)
{
	printf("%s: %d\n", s, ctx->lineno);
	fflush(0);
}
