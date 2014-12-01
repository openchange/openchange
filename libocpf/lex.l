/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2008-2010

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

#include <stdarg.h>
#include <ctype.h>

#include "libocpf/ocpf.h"
#include "libocpf/ocpf_api.h"
#include "libocpf/ocpf.tab.h"
#include "libocpf/lex.h"

#define	YY_EXTRA_TYPE	struct ocpf_context *
#define	YY_NO_UNPUT

static void unterminated(struct ocpf_context *, const char *, unsigned int);

%}

%option reentrant 
%option bison-bridge
%option prefix="ocpf_yy"
%option noyywrap

%%
TYPE		{ return kw_TYPE; }
FOLDER		{ return kw_FOLDER; }
OLEGUID		{ return kw_OLEGUID; }
SET		{ return kw_SET; }
PROPERTY	{ return kw_PROPERTY; }
NPROPERTY	{ return kw_NPROPERTY; }
TO		{ return kw_TO; }
CC		{ return kw_CC; }
BCC		{ return kw_BCC; }
RECIPIENT	{ return kw_RECIPIENT; }
OOM		{ return kw_OOM; }
MNID_ID		{ return kw_MNID_ID; }
MNID_STRING	{ return kw_MNID_STRING; }
PT_STRING8	{ return kw_PT_STRING8; }
PT_UNICODE	{ return kw_PT_UNICODE; }
PT_SHORT	{ return kw_PT_SHORT; }
PT_LONG		{ return kw_PT_LONG; }
PT_DOUBLE	{ return kw_PT_DOUBLE; }
PT_I8		{ return kw_PT_I8; }
PT_SYSTIME	{ return kw_PT_SYSTIME; }
PT_BOOLEAN	{ return kw_PT_BOOLEAN; }
PT_MV_LONG	{ return kw_PT_MV_LONG; }
PT_MV_STRING8	{ return kw_PT_MV_STRING8; }
PT_MV_UNICODE	{ return kw_PT_MV_UNICODE; }
PT_BINARY	{ return kw_PT_BINARY; }
PT_MV_BINARY	{ return kw_PT_MV_BINARY; }
\{		{ return OBRACE; }
\}		{ return EBRACE; }
,		{ return COMMA; }
;		{ return SEMICOLON; }
:		{ return COLON; }
\<		{ return LOWER; }
\>		{ return GREATER; }
=		{ return EQUAL;}
\/\*			{ 
			    struct ocpf_context *ctx = yyextra;
			    int c, start_lineno = ctx->lineno;
			    int level = 1;
			    int seen_star = 0;
			    int seen_slash = 0;
			    while((c = input(yyscanner)) != EOF) {
				if(c == '/') {
				    if(seen_star) {
					if(--level == 0)
					    break;
					seen_star = 0;
					continue;
				    }
				    seen_slash = 1;
				    continue;
				}
				if(seen_star && c == '/') {
				    if(--level == 0)
					break;
				    seen_star = 0;
				    continue;
				}
				if(c == '*') {
				    if(seen_slash) {
					level++;
					seen_star = seen_slash = 0;
					continue;
				    } 
				    seen_star = 1;
				    continue;
				}
				seen_star = seen_slash = 0;
				if(c == '\n') {
				    ctx->lineno++;
				    continue;
				}
			    }
			    if(c == EOF)
				unterminated(ctx, "comment", start_lineno);
			}
"\""			{ 
			    struct ocpf_context *ctx = yyextra;
			    int start_lineno = ctx->lineno;
			    int c, c2;
			    char buf[0x4000];
			    char *p = buf;
			    int f = 0;
			    int skip_ws = 0;
			    
			    while((c = input(yyscanner)) != EOF) {
				if(isspace(c) && skip_ws) {
				    if(c == '\n')
					ctx->lineno++;
				    continue;
				}
				skip_ws = 0;

				if (c == '\\') {
					c2 = c;
					c = input(yyscanner);
					if (c == '"') { 
						*p++ = c;
						c = input(yyscanner);
					} else {
						*p++ = c2;
					}
				}

				if(c == '"') {
					if(f) {
						*p++ = '"';
						f = 0;
					} else {
						f = 1;
					}
					continue;
				}
				if(f == 1) {
					unput(c);
					break;
				}
				if(c == '\n') {
				    ctx->lineno++;
				    while(p > buf && isspace((unsigned char)p[-1]))
					p--;
				    skip_ws = 1;
				    continue;
				}
				*p++ = c;
			    }
			    if(c == EOF)
				unterminated(ctx, "string", start_lineno);
			    *p++ = '\0';
			    yylval->name = buf;
			    return STRING; 
			}
W"\""			{ 
			    struct ocpf_context *ctx = yyextra;
			    int start_lineno = ctx->lineno;
			    int c, c2;
			    char buf[0x4000];
			    char *p = buf;
			    int f = 0;
			    int skip_ws = 0;
			    
			    while((c = input(yyscanner)) != EOF) {
				if(isspace(c) && skip_ws) {
				    if(c == '\n')
					ctx->lineno++;
				    continue;
				}
				skip_ws = 0;

				if (c == '\\') {
					c2 = c;
					c = input(yyscanner);
					if (c == '"') { 
						*p++ = c;
						c = input(yyscanner);
					} else {
						*p++ = c2;
					}
				}

				if(c == '"') {
					if(f) {
						*p++ = '"';
						f = 0;
					} else {
						f = 1;
					}
					continue;
				}
				if(f == 1) {
					unput(c);
					break;
				}
				if(c == '\n') {
				    ctx->lineno++;
				    while(p > buf && isspace((unsigned char)p[-1]))
					p--;
				    skip_ws = 1;
				    continue;
				}
				*p++ = c;
			    }
			    if(c == EOF)
				unterminated(ctx, "string", start_lineno);
			    *p++ = '\0';
			    yylval->name = buf;
			    return UNICODE; 
			}
\$[-A-Za-z0-9_]+      	{ char *y = yytext + 1;
			  yylval->var = strdup((const char *)y);
			  return VAR;
			}
B\"true\"|-?B\"false\" { char *y = yytext + 1;
			   if (y && !strcmp(y, "\"true\"")) {
				yylval->b = true;
			   } else {
			       	yylval->b = false;
			   }
			   return BOOLEAN;
			}
F-?[0-9]"."[0-9]+e[-+]?[0-9]+ {
			struct ocpf_context *ctx = yyextra;
			char *y = yytext + 1;
			char *e;
			yylval->dbl = strtod((const char *)y, &e);
			if (e == y) {
				ocpf_error_message(ctx, "malformed constant (%s)", yytext);
			} else {
				return DOUBLE;
			}
		     }
T[0-9]{4}-[0-9]{2}-[0-9]{2}[ ][0-9]{2}\:[0-9]{2}\:[0-9]{2} {
			  yylval->date = strdup((const char *)yytext + 1);
			  return SYSTIME;
			}
0x[0-9A-Fa-f]{2}	{
			struct ocpf_context *ctx = yyextra;
			char *e, *y = yytext;
			yylval->i = strtoul((const char *)y, &e, 0);
			if (e == y) 
				ocpf_error_message(ctx, "malformed constant (%s)", yytext);
			else
				return UINT8;
			}
0x[0-9A-Fa-f]+		{
			 struct ocpf_context *ctx = yyextra;
			char *e, *y = yytext;
			yylval->l = strtoul((const char *)y, &e, 0);
			if (e == y)
				ocpf_error_message(ctx, "malformed constant (%s)", yytext);
			else
				return INTEGER;
			
			}
S0x[0-9A-Fa-f]+|-?S[0-9]+ {
			   struct ocpf_context *ctx = yyextra;
			   char *e, *y;
			   y = (yytext[0] == 'S') ? yytext + 1 : yytext;
			   yylval->s = strtoul((const char *)y, &e, 0);
			   if (e == y)
				ocpf_error_message(ctx, "malformed constant (%s)", yytext);
			   else
				return SHORT;
			 }

L0x[0-9A-Fa-f]+|-?[0-9]+ {
			  struct ocpf_context *ctx = yyextra;
			  char *e, *y;
			  y = (yytext[0] == 'L') ? yytext + 1 : yytext;
			  yylval->l = strtoul((const char *)y, &e, 0);
			  if(e == y) 
			    ocpf_error_message(ctx, "malformed constant (%s)", yytext); 
			  else
			    return INTEGER;
			}
D0x[0-9A-Fa-f]+		{ 
			  struct ocpf_context *ctx = yyextra;
			  char *e, *y = yytext + 1;
			  yylval->d = strtoull((const char *)y, &e, 0);
			  if(e == y) 
			    ocpf_error_message(ctx, "malformed constant (%s)", yytext); 
			  else
			    return I8;

			}
[A-Za-z][-A-Za-z0-9_]*	{
			  yylval->name = strdup((const char *)yytext);
			  return IDENTIFIER;
			}
[ \t]+			;
\n		{
		struct ocpf_context *ctx = yyextra;
		 ++ctx->lineno; 
		}
.      		{ 
		struct ocpf_context *ctx = yyextra;
		ocpf_error_message(ctx, "Ignoring char(%c)\n", *yytext); 
		}
%%

void ocpf_error_message(struct ocpf_context *ctx, const char *format, ...)
{
	va_list	args;

	va_start(args, format);
	fprintf(stderr, "ERROR: %s:%d: ", ctx->filename, ctx->lineno);
	vfprintf(stderr, format, args);
	va_end(args);
	error_flag++;
	fflush(0);
}

static void
unterminated(struct ocpf_context *ctx, const char *type, unsigned int start_lineno)
{
    ocpf_error_message(ctx, "unterminated %s, possibly started on line %d\n", type, start_lineno);
    fflush(0);
}
