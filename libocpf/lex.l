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

#include <stdarg.h>
#include <ctype.h>

#include "libocpf/ocpf_private.h"
#include <libocpf/ocpf_api.h>
#include <libocpf/ocpf.tab.h>
#include <libocpf/lex.h>

struct ocpf	*ocpf;
unsigned lineno = 1;

#define	YY_DECL int ocpf_yylex(void)

/*
 * Prototypes
 */

int	yyget_lineno(void);
FILE	*yyget_in(void);
FILE	*yyget_out(void);
int	yyget_leng(void);
char	*yyget_text(void);
void	yyset_lineno(int);
void	yyset_in (FILE *);
void	yyset_out (FILE *);
int	yyget_debug(void);
void	yyset_debug(int);
int	yylex_destroy(void);


static void unterminated(const char *, unsigned);

%}

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
PT_SYSTIME	{ return kw_PT_SYSTIME; }
PT_BOOLEAN	{ return kw_PT_BOOLEAN; }
PT_MV_STRING8	{ return kw_PT_MV_STRING8; }
PT_BINARY	{ return kw_PT_BINARY; }
\{		{ return OBRACE; }
\}		{ return EBRACE; }
,		{ return COMMA; }
;		{ return SEMICOLON; }
:		{ return COLON; }
\<		{ return LOWER; }
\>		{ return GREATER; }
=		{ return EQUAL;}
\/\*			{ 
			    int c, start_lineno = lineno;
			    int level = 1;
			    int seen_star = 0;
			    int seen_slash = 0;
			    while((c = input()) != EOF) {
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
				    lineno++;
				    continue;
				}
			    }
			    if(c == EOF)
				unterminated("comment", start_lineno);
			}
"\""			{ 
			    int start_lineno = lineno;
			    int c, c2;
			    char buf[0x4000];
			    char *p = buf;
			    int f = 0;
			    int skip_ws = 0;
			    
			    while((c = input()) != EOF) {
				if(isspace(c) && skip_ws) {
				    if(c == '\n')
					lineno++;
				    continue;
				}
				skip_ws = 0;

				if (c == '\\') {
					c2 = c;
					c = input();
					if (c == '"') { 
						*p++ = c;
						c = input();
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
				    lineno++;
				    while(p > buf && isspace((unsigned char)p[-1]))
					p--;
				    skip_ws = 1;
				    continue;
				}
				*p++ = c;
			    }
			    if(c == EOF)
				unterminated("string", start_lineno);
			    *p++ = '\0';
			    ocpf_yylval.name = buf;
			    return STRING; 
			}
W"\""			{ 
			    int start_lineno = lineno;
			    int c, c2;
			    char buf[0x4000];
			    char *p = buf;
			    int f = 0;
			    int skip_ws = 0;
			    
			    while((c = input()) != EOF) {
				if(isspace(c) && skip_ws) {
				    if(c == '\n')
					lineno++;
				    continue;
				}
				skip_ws = 0;

				if (c == '\\') {
					c2 = c;
					c = input();
					if (c == '"') { 
						*p++ = c;
						c = input();
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
				    lineno++;
				    while(p > buf && isspace((unsigned char)p[-1]))
					p--;
				    skip_ws = 1;
				    continue;
				}
				*p++ = c;
			    }
			    if(c == EOF)
				unterminated("string", start_lineno);
			    *p++ = '\0';
			    ocpf_yylval.name = buf;
			    return UNICODE; 
			}
\$[-A-Za-z0-9_]+      	{ char *y = yytext + 1;
			  ocpf_yylval.var = strdup((const char *)y);
			  return VAR;
			}
B\"true\"|-?B\"false\" { char *y = yytext + 1;
			   if (y && !strcmp(y, "\"true\"")) {
				ocpf_yylval.b = true;
			   } else {
			       	ocpf_yylval.b = false;
			   }
			   return BOOLEAN;
			}
T[0-9]{4}-[0-9]{2}-[0-9]{2}[ ][0-9]{2}\:[0-9]{2}\:[0-9]{2} {
			  ocpf_yylval.date = strdup((const char *)yytext + 1);
			  return SYSTIME;
			}
0x[0-9A-Fa-f]+		{ char *e, *y = yytext;
			ocpf_yylval.l = strtoul((const char *)y, &e, 0);
			if (e == y)
				error_message("malformed constant (%s)", yytext);
			else
				return INTEGER;
			
}
S0x[0-9A-Fa-f]+|-S[0-9]+ {
			   char *e, *y;
			   y = (yytext[0] == 'S') ? yytext + 1 : yytext;
			   ocpf_yylval.s = strtoul((const char *)y, &e, 0);
			   if (e == y)
				error_message("malformed constant (%s)", yytext);
			   else
				return SHORT;
			 }

L0x[0-9A-Fa-f]+|-?[0-9]+ { char *e, *y;
			  y = (yytext[0] == 'L') ? yytext + 1 : yytext;
			  ocpf_yylval.l = strtoul((const char *)y, &e, 0);
			  if(e == y) 
			    error_message("malformed constant (%s)", yytext); 
			  else
			    return INTEGER;
			}
D0x[0-9A-Fa-f]+	{char *e, *y = yytext + 1;
			  ocpf_yylval.d = strtoull((const char *)y,
						   &e, 0);
			  if(e == y) 
			    error_message("malformed constant (%s)", yytext); 
			  else
			    return DOUBLE;

			}
[A-Za-z][-A-Za-z0-9_]*	{
			  ocpf_yylval.name = strdup((const char *)yytext);
			  return IDENTIFIER;
			}
[ \t]+			;
\n		{ ++lineno; }
.      		{ error_message("Ignoring char(%c)\n", *yytext); }
%%

#ifndef yywrap
int
yywrap(void)
{
	return 1;
}
#endif

void error_message(const char *format, ...)
{
	va_list	args;

	va_start(args, format);
	fprintf(stderr, "ERROR: %s:%d: ", ocpf_get_filename(), lineno);
	vfprintf(stderr, format, args);
	va_end(args);
	error_flag++;
}

static void
unterminated(const char *type, unsigned start_lineno)
{
    error_message("unterminated %s, possibly started on line %d\n", type, start_lineno);
}